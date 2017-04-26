#pragma once

#include <array>
#include <memory>
#include <vector>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vtkImageData.h>
#include <vtkDataArray.h>
#include <vtkCommand.h>
#include "glt/gl_core_4_5.h"
#include "glt/buffer_allocator.h"

/* Manages loading and rendering a volume with GPU ray casting
 * Volume can be a raw or idx file
 * TODO: Loading the volume from disk (e.g. changing raw dims or data type
 * or picking a new hz-level or field from IDX) should be asynchronous
 */
class Volume : public vtkCommand {
	// If dims are -1 no volume has been loaded, e.g. for raw
	// the user must input the dimensions and data type into the picker
	// TODO: Do I need both dims and render_dims? in gpu_dvr we use
	// render_dims to track the dimensions of the full IDX data
	// while dims tracks the size of the currently loaded data
	vtkImageData *vol_data;
	vtkDataArray *vtk_data, *seg_data;
	std::string data_field_name;
	std::array<int, 3> dims;
	GLenum internal_format, format, pixel_format;
	bool uploaded;

	// GL stuff
	GLuint shader, vao, texture, seg_texture;
	GLuint isovalue_unif, isosurface_unif;
	float isovalue;
	bool show_isosurface;
	std::shared_ptr<glt::BufferAllocator> allocator;
	glt::SubBuffer cube_buf, vol_props, segmentation_buf;
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
	std::vector<unsigned int> segmentation_selections;
	bool segmentation_selection_changed;

	Volume(vtkImageData *vol, const std::string &array_name = "ImageFile");
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
	/* Render the volume data, this will also upload the volume
	 * if this is the first time the data is being rendered.
	 */
	void render(std::shared_ptr<glt::BufferAllocator> &buf_allocator);
	void set_isovalue(float isovalue);
	void toggle_isosurface(bool on);
	void Execute(vtkObject *caller, unsigned long event_id, void *call_data) override;

private:
	// Find the min/max of the data and build the histogram
	void build_histogram();
	// Upload the vtk data passed, dims are assumed to be the same but the
	// data type can differ
	void upload_volume(vtkDataArray *data);
	// Check if the voxel is in the selected segments
	bool voxel_selected(const size_t i) const;
};

