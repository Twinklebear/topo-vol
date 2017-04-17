#pragma once

#include <array>
#include <memory>
#include <vector>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include "glt/gl_core_4_5.h"
#include "glt/buffer_allocator.h"

/* Manages loading and rendering a volume with GPU ray casting
 * Volume can be a raw or idx file
 * TODO: Loading the volume from disk (e.g. changing raw dims or data type
 * or picking a new hz-level or field from IDX) should be asynchronous
 */
class Volume {
	// If dims are -1 no volume has been loaded, e.g. for raw
	// the user must input the dimensions and data type into the picker
	// TODO: Do I need both dims and render_dims? in gpu_dvr we use
	// render_dims to track the dimensions of the full IDX data
	// while dims tracks the size of the currently loaded data
	std::array<int, 3> dims;
	GLenum internal_format, format;
	// Temporary storage for the volume data, we upload
	// when we're first rendered after being loaded or changed.
	// The volume must be re-uploaded if vol_data isn't empty,
	// since it's changed and we've read some new data
	std::shared_ptr<std::vector<char>> vol_data;

	// GL stuff
	GLuint shader, vao, texture;
	std::shared_ptr<glt::BufferAllocator> allocator;
	glt::SubBuffer cube_buf, vol_props;
	bool transform_dirty;
	// Base transformation matrix, e.g. the IDX logical to physical transform
	glm::mat4 base_matrix;
	glm::vec3 translation, scaling, vol_render_size;
	glm::quat rotation;

public:
	// TODO: make private, public only temporarily Min and max values in the data set
	float vol_min, vol_max;
	// The histogram for the volume data
	// TODO: Should the volume no longer build a histogram and instead the
	// user handles it? Maybe the user could pass the value min/max as well?
	std::vector<size_t> histogram;

	Volume(GLenum data_format, GLenum gl_format, std::shared_ptr<std::vector<char>> &data,
			std::array<int, 3> vol_dims, std::array<float, 3> vol_render_dims = {-1, -1, -1});
	~Volume();
	Volume(const Volume&) = delete;
	Volume& operator=(const Volume&) = delete;
	// Translate the volume along some vector
	void translate(const glm::vec3 &v);
	// Scale the volume by some factor
	void scale(const glm::vec3 &v);
	// Rotate the volume
	void rotate(const glm::quat &r);
	// TODO: Remove, quick hack to demo isosurface & volume rendering at the same time
	inline GLuint shader_handle(){
		return shader;
	}
	void set_base_matrix(const glm::mat4 &m);
	/* Set the volume data being displayed, it's assumed the volume data is single-channel
	 * - format is the format being sent in the byte buffer, e.g. GL_FLOAT, GL_UNSIGNED_BYTE
	 * - gl_format is the format you want on the GPU, e.g. GL_R32F, GL_R8
	 * - dims is the actual dimensions of the data
	 * - render_dims is an optionally different dimension to display the data at
	 */
	void set_volume(GLenum data_format, GLenum gl_format, std::shared_ptr<std::vector<char>> &data,
			std::array<int, 3> vol_dims, std::array<float, 3> vol_render_dims = {-1, -1, -1});
	/* Render the volume data, this will also upload the volume
	 * if this is the first time the data is being rendered,
	 * e.g. after loading from file or changing IDX level/field
	 */
	void render(std::shared_ptr<glt::BufferAllocator> &buf_allocator, const glm::mat4 &view_mat);

private:
	// Find the min/max of the data and build the histogram
	void build_histogram();
};

