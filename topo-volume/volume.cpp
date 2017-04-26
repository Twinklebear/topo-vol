#include <iostream>
#include <cstring>
#include <algorithm>
#include <limits>
#include <glm/glm.hpp>
#include <vtkType.h>
#include <vtkDataSetAttributes.h>
#include <vtkDataSet.h>
#include "glt/util.h"
#include "volume.h"

static const std::array<float, 42> CUBE_STRIP = {
	1, 1, 0,
	0, 1, 0,
	1, 1, 1,
	0, 1, 1,
	0, 0, 1,
	0, 1, 0,
	0, 0, 0,
	1, 1, 0,
	1, 0, 0,
	1, 1, 1,
	1, 0, 1,
	0, 0, 1,
	1, 0, 0,
	0, 0, 0
};

static void vtk_type_to_gl(const int vtk, GLenum &gl_internal, GLenum &gl_type, GLenum &pixel_format) {
	pixel_format = GL_RED;
	switch (vtk) {
		case VTK_CHAR:
		case VTK_UNSIGNED_CHAR:
			gl_internal = GL_R8;
			gl_type = GL_UNSIGNED_BYTE;
			break;
		case VTK_FLOAT:
			gl_internal = GL_R32F;
			gl_type = GL_FLOAT;
			break;
		case VTK_INT:
			gl_internal = GL_R32I;
			gl_type = GL_INT;
			pixel_format = GL_RED_INTEGER;
			break;
		default:
			throw std::runtime_error("Unsupported VTK data type '" + std::to_string(vtk) + "'");
	}
}

Volume::Volume(vtkImageData *vol, const std::string &array_name)
	: vol_data(vol), data_field_name(array_name), uploaded(false),
	isovalue(0.f), show_isosurface(false),
	transform_dirty(true), translation(0), scaling(1), segmentation_selection_changed(false)
{
	vol->AddObserver(vtkCommand::ModifiedEvent, this);
	vtkDataSetAttributes *fields = vol->GetAttributes(vtkDataSet::POINT);
	vtk_data = fields->GetArray(array_name.c_str());
	seg_data = fields->GetArray("SegmentationId");
	if (!vtk_data) {
		throw std::runtime_error("Nonexistant field '" + array_name + "'");
	}

	vtk_type_to_gl(vtk_data->GetDataType(), internal_format, format, pixel_format);
	for (size_t i = 0; i < 3; ++i) {
		dims[i] = vol->GetDimensions()[i];
		vol_render_size[i] = vol->GetSpacing()[i] * dims[i];
	}
	std::cout << "dims = { " << dims[0] << ", " << dims[1] << ", " << dims[2] << " }\n";
	// Center the volume in the world
	translate(glm::vec3(vol_render_size[0], vol_render_size[1], vol_render_size[2]) * glm::vec3(-0.5));
	build_histogram();
}
Volume::~Volume(){
	if (allocator){
		allocator->free(cube_buf);
		allocator->free(vol_props);
		glDeleteVertexArrays(1, &vao);
		glDeleteTextures(1, &texture);
		glDeleteTextures(1, &seg_texture);
		glDeleteProgram(shader);
	}
}
void Volume::translate(const glm::vec3 &v){
	translation += v;
	transform_dirty = true;
}
void Volume::scale(const glm::vec3 &v){
	scaling *= v;
	transform_dirty = true;
}
void Volume::rotate(const glm::quat &r){
	rotation = r * rotation;
	transform_dirty = true;
}
void Volume::set_base_matrix(const glm::mat4 &m){
	base_matrix = m;
	transform_dirty = true;
}
void Volume::render(std::shared_ptr<glt::BufferAllocator> &buf_allocator) {
	// We need to apply the inverse volume transform to the eye to get it in the volume's space
	glm::mat4 vol_transform = glm::translate(translation) * glm::mat4_cast(rotation)
		* glm::scale(scaling * vol_render_size) * base_matrix;
	// Setup shaders, vao and volume texture
	if (!allocator){
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);
		allocator = buf_allocator;
		// Setup our cube tri strip to draw the bounds of the volume to raycast against
		cube_buf = buf_allocator->alloc(sizeof(float) * CUBE_STRIP.size());
		{
			float *buf = reinterpret_cast<float*>(cube_buf.map(GL_ARRAY_BUFFER,
						GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_WRITE_BIT));
			for (size_t i = 0; i < CUBE_STRIP.size(); ++i){
				buf[i] = CUBE_STRIP[i];
			}
			cube_buf.unmap(GL_ARRAY_BUFFER);
		}
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)cube_buf.offset);

		vol_props = buf_allocator->alloc(2 * sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(glm::vec2),
			glt::BufAlignment::UNIFORM_BUFFER);
		{
			char *buf = reinterpret_cast<char*>(vol_props.map(GL_UNIFORM_BUFFER,
						GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_WRITE_BIT));
			glm::mat4 *mats = reinterpret_cast<glm::mat4*>(buf);
			glm::vec4 *vecs = reinterpret_cast<glm::vec4*>(buf + 2 * sizeof(glm::mat4));
			glm::vec2 *vec2s = reinterpret_cast<glm::vec2*>(buf + 2 * sizeof(glm::mat4) + sizeof(glm::vec4));
			mats[0] = vol_transform;
			mats[1] = glm::inverse(mats[0]);
			vecs[0] = glm::vec4{static_cast<float>(dims[0]), static_cast<float>(dims[1]),
				static_cast<float>(dims[2]), 0};
			// Set scaling and bias to scale the volume values
			vec2s[0] = glm::vec2{1.f / (vol_max - vol_min), -vol_min};

			// TODO: Again how will this interact with multiple folks doing this?
			glBindBufferRange(GL_UNIFORM_BUFFER, 1, vol_props.buffer, vol_props.offset, vol_props.size);
			vol_props.unmap(GL_UNIFORM_BUFFER);
			transform_dirty = false;
		}

		glGenTextures(1, &texture);
		glGenTextures(1, &seg_texture);

		// TODO: If drawing multiple volumes they can all share the same program
		const std::string resource_path = glt::get_resource_path();
		shader = glt::load_program({std::make_pair(GL_VERTEX_SHADER, resource_path + "vol_vert.glsl"),
				std::make_pair(GL_FRAGMENT_SHADER, resource_path + "vol_frag.glsl")});
		glUseProgram(shader);

		glUniform1i(glGetUniformLocation(shader, "volume"), 1);
		glUniform1i(glGetUniformLocation(shader, "ivolume"), 1);
		glUniform1i(glGetUniformLocation(shader, "segmentation_volume"), 3);
		glUniform1i(glGetUniformLocation(shader, "int_texture"), pixel_format == GL_RED_INTEGER ? 1 : 0);
		glUniform1i(glGetUniformLocation(shader, "palette"), 2);
		isovalue_unif = glGetUniformLocation(shader, "isovalue");
		isosurface_unif = glGetUniformLocation(shader, "isosurface");
	}
	// Upload the volume data, it's changed
	if (!uploaded){
		uploaded = true;
		vtkDataSetAttributes *fields = vol_data->GetAttributes(vtkDataSet::POINT);
		vtk_data = fields->GetArray(data_field_name.c_str());
		seg_data = fields->GetArray("SegmentationId");

		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, texture);
		upload_volume(vtk_data);

		if (seg_data) {
			glActiveTexture(GL_TEXTURE3);
			glBindTexture(GL_TEXTURE_3D, seg_texture);
			upload_volume(seg_data);

			glUseProgram(shader);
			glUniform1i(glGetUniformLocation(shader, "has_segmentation_volume"), 1);
			const int num_segments = seg_data->GetRange()[1] + 1;
			// Re-allocing each time we change leaks some, but re-alloc and free seem to screw up
			// the buffer? Probabaly a todo for me later (will)
			segmentation_buf = allocator->alloc((2 * num_segments + 1) * sizeof(int),
					glt::BufAlignment::SHADER_STORAGE_BUFFER);
			{
				int *buf = reinterpret_cast<int*>(segmentation_buf.map(GL_SHADER_STORAGE_BUFFER, GL_MAP_WRITE_BIT));
				buf[0] = num_segments;
				// The buffer is interleaved [segment, palette, segment, palette, ...]
				segmentation_palettes.clear();
				segmentation_palettes.resize(num_segments, 0);
				segmentation_selections.clear();
				segmentation_selections.resize(num_segments, 1);
				int *s = buf + 1;
				for (const auto &x : segmentation_selections) {
					*s = x;
					++s;
					*s = 0;
					++s;
				}
				segmentation_buf.unmap(GL_SHADER_STORAGE_BUFFER);
			}
			segmentation_selection_changed = true;
		} else {
			glUseProgram(shader);
			glUniform1i(glGetUniformLocation(shader, "has_segmentation_volume"), 0);
		}

		// We're changing the volume so also update the volume properties buffer
		{
			char *buf = reinterpret_cast<char*>(vol_props.map(GL_UNIFORM_BUFFER, GL_MAP_WRITE_BIT));
			glm::mat4 *mats = reinterpret_cast<glm::mat4*>(buf);
			glm::vec4 *vecs = reinterpret_cast<glm::vec4*>(buf + 2 * sizeof(glm::mat4));
			glm::vec2 *vec2s = reinterpret_cast<glm::vec2*>(buf + 2 * sizeof(glm::mat4) + sizeof(glm::vec4));
			mats[0] = vol_transform;
			mats[1] = glm::inverse(mats[0]);
			vecs[0] = glm::vec4{ static_cast<float>(dims[0]), static_cast<float>(dims[1]),
				static_cast<float>(dims[2]), 0 };
			// Set scaling and bias to scale the volume values
			vec2s[0] = glm::vec2{1.f / (vol_max - vol_min), -vol_min};

			vol_props.unmap(GL_UNIFORM_BUFFER);
			transform_dirty = false;
		}
	}
	if (transform_dirty){
		char *buf = reinterpret_cast<char*>(vol_props.map(GL_UNIFORM_BUFFER, GL_MAP_WRITE_BIT));
		glm::mat4 *mats = reinterpret_cast<glm::mat4*>(buf);
		mats[0] = vol_transform;
		mats[1] = glm::inverse(mats[0]);
		vol_props.unmap(GL_UNIFORM_BUFFER);
		transform_dirty = false;
	}
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	glBindBufferRange(GL_UNIFORM_BUFFER, 1, vol_props.buffer, vol_props.offset, vol_props.size);
	if (segmentation_buf.size != 0 && segmentation_selection_changed) {
		segmentation_selection_changed = false;

		build_histogram();
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, segmentation_buf.buffer);
		int *s = reinterpret_cast<int*>(segmentation_buf.map(GL_SHADER_STORAGE_BUFFER, GL_MAP_WRITE_BIT)) + 1;
		for (size_t i = 0; i < segmentation_selections.size(); ++i, s += 2) {
			s[0] = segmentation_selections[i];
			s[1] = segmentation_palettes[i];
		}
		segmentation_buf.unmap(GL_SHADER_STORAGE_BUFFER);
		glBindBufferRange(GL_SHADER_STORAGE_BUFFER, 2, segmentation_buf.buffer,
				segmentation_buf.offset, segmentation_buf.size);
	}
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, texture);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_3D, seg_texture);

	glUseProgram(shader);

	glUniform1f(isovalue_unif, isovalue);
	glUniform1i(isosurface_unif, show_isosurface ? 1 : 0);

	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, CUBE_STRIP.size() / 3);

	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);
}
void Volume::set_isovalue(float i) {
	isovalue = i;
}
void Volume::toggle_isosurface(bool on) {
	show_isosurface = on;
}
void Volume::Execute(vtkObject *caller, unsigned long event_id, void *call_data) {
	uploaded = false;
}
void Volume::build_histogram(){
	// Find scale & bias for the volume data
	// For non f32 or f16 textures GL will normalize for us, given that we've done
	// the proper range correction above if needed (e.g. for R16)
	if (internal_format == GL_R8) {
		vol_min = 0;
		vol_max = 1;
	} else {
		// Find the min/max values in the volume
		vol_min = vtk_data->GetRange()[0];
		vol_max = vtk_data->GetRange()[1];
	}

	// Build the histogram for the data
	histogram.clear();
	histogram.resize(128, 0);
	const size_t num_voxels = dims[0] * dims[1] * dims[2];
	if (format == GL_FLOAT){
		float *data_ptr = reinterpret_cast<float*>(vtk_data->GetVoidPointer(0));
		for (size_t i = 0; i < num_voxels; ++i){
			if (voxel_selected(i)) {
				size_t bin = static_cast<size_t>((data_ptr[i] - vol_min) / (vol_max - vol_min) * histogram.size());
				bin = glm::clamp(bin, size_t{0}, histogram.size() - 1);
				++histogram[bin];
			}
		}
	} else if (format == GL_INT){
		int32_t *data_ptr = reinterpret_cast<int32_t*>(vtk_data->GetVoidPointer(0));
		for (size_t i = 0; i < num_voxels; ++i){
			if (voxel_selected(i)) {
				size_t bin = static_cast<size_t>(static_cast<float>(data_ptr[i] - vol_min)
						/ (vol_max - vol_min) * histogram.size());
				bin = glm::clamp(bin, size_t{0}, histogram.size() - 1);
				++histogram[bin];
			}
		}
	} else if (format == GL_UNSIGNED_BYTE){
		uint8_t *data_ptr = reinterpret_cast<uint8_t*>(vtk_data->GetVoidPointer(0));
		const float data_min = vtk_data->GetRange()[0];
		const float data_max = vtk_data->GetRange()[1];
		for (size_t i = 0; i < num_voxels; ++i){
			if (voxel_selected(i)) {
				size_t bin = static_cast<size_t>((data_ptr[i] - data_min) / (data_max - data_min) * histogram.size());
				bin = glm::clamp(bin, size_t{0}, histogram.size() - 1);
				++histogram[bin];
			}
		}
	}
}
void Volume::upload_volume(vtkDataArray *data) {
	GLenum internal_fmt, data_fmt, px_fmt;
	vtk_type_to_gl(data->GetDataType(), internal_fmt, data_fmt, px_fmt);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage3D(GL_TEXTURE_3D, 0, internal_fmt, dims[0], dims[1], dims[2], 0, px_fmt,
			data_fmt, (char*)data->GetVoidPointer(0));

	if (px_fmt == GL_RED_INTEGER) {
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	} else {
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}
bool Volume::voxel_selected(const size_t i) const {
	if (seg_data && !segmentation_selections.empty()) {
		const int seg = *seg_data->GetTuple(i);
		return segmentation_selections[seg] != 0;
	}
	return true;
}

