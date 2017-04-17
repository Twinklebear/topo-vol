#pragma once

#include <thread>
#include <array>
#include <vector>
#include <cassert>
#include <utility>
#include <string>

#include "gl_core_4_5.h"

namespace glt {
#if defined(_WIN32) && !defined(RESOURCE_PATH)
const char PATH_SEP = '\\';
#else
const char PATH_SEP = '/';
#endif

// Find the next power of two, from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
inline uint64_t next_pow2(uint64_t x) {
	x -= 1;
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	x |= (x >> 32);
	return x + 1;
}
// Check if a number is a power of two, http://graphics.stanford.edu/~seander/bithacks.html#DetermineIfPowerOf2
inline bool is_pow2(const uint64_t x) {
	return x && !(x & (x - 1));
}
// Get the resource path for resources located under res/<sub_dir>
// sub_dir defaults to empty to just return res
std::string get_resource_path(const std::string &sub_dir = "");
// Read the contents of a file into the string
std::string get_file_content(const std::string &fname);
// Load a file's content and its includes returning the file with includes inserted
// and #line directives for better GLSL error messages within the included files
// the vector of file names will be filled with the file name for each file name number
// in the #line directive
std::string load_shader_file(const std::string &fname, std::vector<std::string> &file_names);
// Load a GLSL shader from the file. Returns -1 if loading fails and prints
// out the compilation errors
GLint load_shader(GLenum type, const std::string &file);
// Load a GLSL shader program from the shader files specified. The pair
// to specify a shader is { shader type, shader file }
// Returns -1 if program creation fails
GLint load_program(const std::vector<std::pair<GLenum, std::string>> &shader_files);
/*
 * Load an image into a 2D texture, creating a new texture id
 * The texture unit desired for this texture should be set active
 * before loading the texture as it will be bound during the loading process
 * Can also optionally pass width & height variables to return the width
 * and height of the loaded image
 */
GLuint load_texture(const std::string &file, size_t *width = nullptr, size_t *height = nullptr);
/*
 * Load a series of images into a 2D texture array, creating a new texture id
 * The images will appear in the array in the same order they're passed in
 * It is an error if the images don't all have the same dimensions
 * or have different formats
 * The texture unit desired for this texture should be set active
 * before loading the texture as it will be bound during the loading process
 * Can also optionally pass width & height variables to return the width
 * and height of the loaded image
 */
GLuint load_texture_array(const std::vector<std::string> &files, size_t *w = nullptr, size_t *h = nullptr);
void set_thread_name(std::thread &thread, const char *name);
}

