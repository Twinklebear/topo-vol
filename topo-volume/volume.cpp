#include <iostream>
#include <cstring>
#include <algorithm>
#include <limits>
#include <glm/glm.hpp>
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

Volume::Volume(GLenum data_format, GLenum gl_format, std::shared_ptr<std::vector<char>> &data,
		std::array<int, 3> vol_dims, std::array<float, 3> vol_render_dims)
	: dims(vol_dims), internal_format(gl_format), format(data_format),
	vol_data(data), transform_dirty(true), translation(0), scaling(1)
{
	if (vol_render_dims[0] < 0 || vol_render_dims[1] < 0 || vol_render_dims[2] < 0){
		// Find the max side of the volume and set this as 1, then find scaling for the other sides
		float max_axis = static_cast<float>(std::max(dims[0], std::max(dims[1], dims[2])));
		for (size_t i = 0; i < 3; ++i){
			vol_render_size[i] = vol_dims[i] / max_axis;
		}
	} else {
		for (size_t i = 0; i < 3; ++i){
			vol_render_size[i] = vol_render_dims[i];
		}
	}
	build_histogram();
}
Volume::~Volume(){
	if (allocator){
		allocator->free(cube_buf);
		allocator->free(vol_props);
		glDeleteVertexArrays(1, &vao);
		glDeleteTextures(1, &texture);
		// TODO: Why does GL crash on invalid program value, when this is
		// definitely a valid fucking program?
		//glDeleteProgram(shader);
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
void Volume::set_volume(GLenum data_format, GLenum gl_format, std::shared_ptr<std::vector<char>> &data,
	std::array<int, 3> vol_dims, std::array<float, 3> vol_render_dims)
{
	format = data_format;
	internal_format = gl_format;
	vol_data = data;
	dims = vol_dims;
	if (vol_render_dims[0] < 0 || vol_render_dims[1] < 0 || vol_render_dims[2] < 0){
		// Find the max side of the volume and set this as 1, then find scaling for the other sides
		float max_axis = static_cast<float>(std::max(dims[0], std::max(dims[1], dims[2])));
		for (size_t i = 0; i < 3; ++i){
			vol_render_size[i] = vol_dims[i] / max_axis;
		}
	} else {
		for (size_t i = 0; i < 3; ++i){
			vol_render_size[i] = vol_render_dims[i];
		}
	}
	transform_dirty = true;
	build_histogram();
}
void Volume::render(std::shared_ptr<glt::BufferAllocator> &buf_allocator, const glm::mat4 &view_mat){
	// We need to apply the inverse volume transform to the eye to get it in the volume's space
	glm::mat4 vol_transform = glm::translate(translation) * glm::mat4_cast(rotation)
		* glm::scale(scaling * vol_render_size) * base_matrix;
	const glm::vec3 vol_eye_pos = glm::vec3{ glm::inverse(vol_transform) * glm::inverse(view_mat) * glm::vec4{0, 0, 0, 1} };
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
			glm::vec2 *scale_bias = reinterpret_cast<glm::vec2*>(buf + 2 * sizeof(glm::mat4) + sizeof(glm::vec4));
			mats[0] = vol_transform;
			mats[1] = glm::inverse(mats[0]);
			vecs[0] = glm::vec4{static_cast<float>(dims[0]), static_cast<float>(dims[1]),
				static_cast<float>(dims[2]), 0};
			// Set scaling and bias to scale the volume values
			*scale_bias = glm::vec2{1.f / (vol_max - vol_min), -vol_min};

			// TODO: Again how will this interact with multiple folks doing this?
			glBindBufferRange(GL_UNIFORM_BUFFER, 1, vol_props.buffer, vol_props.offset, vol_props.size);
			vol_props.unmap(GL_UNIFORM_BUFFER);
			transform_dirty = false;
		}

		glGenTextures(1, &texture);

		// TODO: If drawing multiple volumes they can all share the same program
		const std::string resource_path = glt::get_resource_path();
		shader = glt::load_program({std::make_pair(GL_VERTEX_SHADER, resource_path + "vol_vert.glsl"),
				std::make_pair(GL_FRAGMENT_SHADER, resource_path + "vol_frag.glsl")});
		glUseProgram(shader);
		// TODO: how does this interact with having multiple volumes? should we just
		// have GL4.5 as a hard requirement for DSA? Can I get 4.5 on my laptop?
		glUniform1i(glGetUniformLocation(shader, "volume"), 1);
		glUniform1i(glGetUniformLocation(shader, "palette"), 2);
	}
	// Upload the volume data, it's changed
	if (vol_data){
		/*
		// TODO: Is there sparse textures for 3d textures? How would it work streaming idx data
		// up instead of re-calling teximage3d and creating/destroying the texture each time?
		// Allocate storage space for the texture
		glTexStorage3D(GL_TEXTURE_3D, 1, internal_format, dims[0], dims[1], dims[2]);
		// Upload our data
		glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, dims[0], dims[1], dims[2], GL_RED,
		format, static_cast<const void*>(data.data()));
		*/
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_3D, texture);
		glTexImage3D(GL_TEXTURE_3D, 0, internal_format, dims[0], dims[1], dims[2], 0, GL_RED,
				format, static_cast<const void*>(vol_data->data()));
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
		// We've uploaded the data and don't need a reference any more
		vol_data = nullptr;
		// We're changing the volume so also update the volume properties buffer
		{
			char *buf = reinterpret_cast<char*>(vol_props.map(GL_UNIFORM_BUFFER, GL_MAP_WRITE_BIT));
			glm::mat4 *mats = reinterpret_cast<glm::mat4*>(buf);
			glm::vec4 *vecs = reinterpret_cast<glm::vec4*>(buf + 2 * sizeof(glm::mat4));
			glm::vec2 *scale_bias = reinterpret_cast<glm::vec2*>(buf + 2 * sizeof(glm::mat4) + sizeof(glm::vec4));
			mats[0] = vol_transform;
			mats[1] = glm::inverse(mats[0]);
			vecs[0] = glm::vec4{ static_cast<float>(dims[0]), static_cast<float>(dims[1]),
				static_cast<float>(dims[2]), 0 };

			// Set scaling and bias to scale the volume values
			*scale_bias = glm::vec2{1.f / (vol_max - vol_min), -vol_min};

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
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_3D, texture);
	glUseProgram(shader);
	glBindVertexArray(vao);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, CUBE_STRIP.size() / 3);

	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);
}
void Volume::build_histogram(){
	// Find scale & bias for the volume data
	if (internal_format == GL_R32F && format == GL_FLOAT){
		// Find the min/max values in the volume
		float *data_ptr = reinterpret_cast<float*>(vol_data->data());
		auto minmax = std::minmax_element(data_ptr, data_ptr + vol_data->size() / 4);
		vol_min = *minmax.first;
		vol_max = *minmax.second;
		std::cout << "Found min max = {" << vol_min << ", " << vol_max << "}\n";
#if 0
	} else if (internal_format == GL_R16F && format == GL_HALF_FLOAT){
		// Find the min/max values in the volume
		unsigned short *data_ptr = reinterpret_cast<unsigned short*>(vol_data->data());
		auto minmax = std::minmax_element(data_ptr, data_ptr + vol_data->size() / 2,
				[](const unsigned short &a, const unsigned short &b){
					return glt::half_to_float(a) < glt::half_to_float(b);
				});
		vol_min = glt::half_to_float(*minmax.first);
		vol_max = glt::half_to_float(*minmax.second);
		std::cout << "Found min max = {" << vol_min << ", " << vol_max << "}\n";
#endif
	} else if (internal_format == GL_R16 && format == GL_UNSIGNED_SHORT){
		// Find the min/max values in the volume
		unsigned short *data_ptr = reinterpret_cast<unsigned short*>(vol_data->data());
		auto minmax = std::minmax_element(data_ptr, data_ptr + vol_data->size() / 2);
		vol_min = *minmax.first;
		vol_max = *minmax.second;
		std::cout << "Found min max = {" << vol_min << ", " << vol_max << "}\n";
		// Check if we need to re-scale so the data occupies the range [0, SHORT_MAX] so
		// OpenGL will re-scale it into [0.0, 1.0] properly when doing texture lookups
		if (*minmax.first != 0 || *minmax.second != std::numeric_limits<unsigned short>::max()){
			float short_max = std::numeric_limits<unsigned short>::max();
			float old_min = *minmax.first;
			float old_max = *minmax.second;
			std::transform(data_ptr, data_ptr + vol_data->size() / 2, data_ptr,
					[&](const unsigned short &s){
						return static_cast<unsigned short>(short_max * (s - old_min) / (old_max - old_min));
					});
		}
	}
	// For non f32 or f16 textures GL will normalize for us, given that we've done
	// the proper range correction above if needed (e.g. for R16)
	if (internal_format == GL_R8 || internal_format == GL_R16){
		std::cout << "Setting gl min max to {0, 1} for R8 or R16 data\n";
		vol_min = 0;
		vol_max = 1;
	}

	// Build the histogram for the data
	histogram.clear();
	histogram.resize(100, 0);
	if (format == GL_FLOAT){
		float *data_ptr = reinterpret_cast<float*>(vol_data->data());
		for (size_t i = 0; i < vol_data->size() / 4; ++i){
			size_t bin = static_cast<size_t>((data_ptr[i] - vol_min) / (vol_max - vol_min) * histogram.size());
			bin = glm::clamp(bin, size_t{0}, histogram.size() - 1);
			++histogram[bin];
		}
	} else if (format == GL_UNSIGNED_SHORT){
		float short_max = std::numeric_limits<unsigned short>::max();
		unsigned short *data_ptr = reinterpret_cast<unsigned short*>(vol_data->data());
		for (size_t i = 0; i < vol_data->size() / 2; ++i){
			size_t bin = static_cast<size_t>(data_ptr[i] / short_max * histogram.size());
			bin = glm::clamp(bin, size_t{0}, histogram.size() - 1);
			++histogram[bin];
		}
	} else if (format == GL_UNSIGNED_BYTE){
		uint8_t *data_ptr = reinterpret_cast<uint8_t*>(vol_data->data());
		for (size_t i = 0; i < vol_data->size() / 4; ++i){
			size_t bin = static_cast<size_t>((data_ptr[i] - vol_min) / (vol_max - vol_min) * histogram.size());
			bin = glm::clamp(bin, size_t{0}, histogram.size() - 1);
			++histogram[bin];
		}
	}
}

