#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#include <stdexcept>
#include <iostream>
#include <algorithm>
#include <vector>
#include <utility>
#include <string>
#include <fstream>
#include <SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "gl_core_4_5.h"
#include "util.h"

std::string glt::get_resource_path(const std::string &sub_dir) {
	using namespace glt;
	static std::string base_res;
	if (base_res.empty()){
		char *base_path = SDL_GetBasePath();
		if (base_path){
			base_res = base_path;
			SDL_free(base_path);
		}
		else {
			std::cout << "Error getting resource path: " << SDL_GetError() << std::endl;
			return "./";
		}
		base_res = base_res + "res" + PATH_SEP;
	}
	if (sub_dir.empty()){
		return base_res;
	}
	return base_res + sub_dir + PATH_SEP;
}
std::string glt::get_file_content(const std::string &fname){
	std::ifstream file{fname};
	if (!file.is_open()){
		std::cout << "Failed to open file: " << fname << std::endl;
		return "";
	}
	return std::string{std::istreambuf_iterator<char>{file},
		std::istreambuf_iterator<char>{}};
}
std::string glt::load_shader_file(const std::string &fname, std::vector<std::string> &file_names){
	if (std::find(file_names.begin(), file_names.end(), fname) != file_names.end()){
		std::cout << "Multiple includes of file " << fname << " detected, dropping this include\n";
		return "";
	}
	std::string content = get_file_content(fname);
	file_names.push_back(fname);
	// Insert the current file name index and line number for this file to preserve error logs
	// before inserting the included file contents
	size_t inc = content.rfind("#include");
	if (inc != std::string::npos){
		size_t line_no = std::count_if(content.begin(), content.begin() + inc,
				[](const char &c){ return c == '\n'; });
		content.insert(content.find("\n", inc) + 1, "#line " + std::to_string(line_no + 2)
				+ " " + std::to_string(file_names.size() - 1) + "\n");
	}
	else if (file_names.size() > 1){
		content.insert(0, "#line 1 " + std::to_string(file_names.size() - 1) + "\n");
	}
	std::string dir = fname.substr(0, fname.rfind(PATH_SEP) + 1);
	// Insert includes backwards so we don't waste time parsing through the inserted file after inserting
	for (; inc != std::string::npos; inc = content.rfind("#include", inc - 1)){
		// TODO: Handle <> and "" style includes separately to have global/local
		// shader include search paths
		size_t open = content.find("\"", inc + 8);
		size_t close = content.find("\"", open + 1);
		std::string included;
		// Local file includes with ""
		if (open != std::string::npos && close != std::string::npos) {
			included = dir + content.substr(open + 1, close - open - 1);
			std::cout << "Local include of shader " << included << "\n";
		} else {
			// See if it's a global include with <>
			open = content.find("<", inc + 8);
			close = content.find(">", open + 1);
			if (open == std::string::npos || close == std::string::npos) {
				throw std::runtime_error("Failed to parse include string in file " + fname);
			}
			included = glt::get_resource_path() + content.substr(open + 1, close - open - 1);
			std::cout << "Global include of shader " << included << "\n";
		}

		content.erase(inc, close - inc + 2);
		std::string include_content = load_shader_file(included, file_names);
		if (!include_content.empty()){
			content.insert(inc, include_content);
		}
	}
	return content;
}
// Extract the file number the error occured in from the log message
// it's expected that the message begins with file_no(line_no)
// TODO: Is this always the form of the compilation errors?
// TODO it's not always the form of the compilation errors, need to
// handle intel and possible AMD differences properly
int get_file_num(const std::vector<char> &log){
	//auto paren = std::find(log.begin(), log.end(), '(');
	//std::string file_no{log.begin(), paren};
	return 0;
}
GLint glt::load_shader(GLenum type, const std::string &file){
	GLuint shader = glCreateShader(type);
	std::vector<std::string> file_names;
	std::string src = glt::load_shader_file(file, file_names);
	const char *csrc = src.c_str();
	glShaderSource(shader, 1, &csrc, 0);
	glCompileShader(shader);
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE){
		std::cout << "Shader compilation error, ";
		switch (type){
			case GL_VERTEX_SHADER:
				std::cout << "Vertex shader: ";
				break;
			case GL_FRAGMENT_SHADER:
				std::cout << "Fragment shader: ";
				break;
			case GL_GEOMETRY_SHADER:
				std::cout << "Geometry shader: ";
				break;
			case GL_COMPUTE_SHADER:
				std::cout << "Compute shader: ";
				break;
			case GL_TESS_CONTROL_SHADER:
				std::cout << "Tessellation Control shader: ";
				break;
			case GL_TESS_EVALUATION_SHADER:
				std::cout << "Tessellation Evaluation shader: ";
				break;
			default:
				std::cout << "Unknown shader type: ";
		}
		std::cout << file << " failed to compile. Compilation log:\n";
		GLint len;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len, '\0');
		log.resize(len);
		glGetShaderInfoLog(shader, log.size(), 0, log.data());
		std::cout << "In file: " << file_names[get_file_num(log)] << ":\n" << log.data() << "\n";
		glDeleteShader(shader);
		return -1;
	}
	return shader;
}
GLint glt::load_program(const std::vector<std::pair<GLenum, std::string>> &shader_files){
	std::vector<GLuint> shaders;
	for (const auto &s : shader_files){
		GLint h = load_shader(std::get<0>(s), std::get<1>(s));
		if (h == -1){
			std::cout << "Error loading shader program: A required shader failed to compile, aborting\n";
			for (GLuint g : shaders){
				glDeleteShader(g);
			}
			return -1;
		}
		shaders.push_back(h);
	}
	GLuint program = glCreateProgram();
	for (const auto &s : shaders){
		glAttachShader(program, s);
	}
	glLinkProgram(program);
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == GL_FALSE){
		std::cout << "Error loading shader program: Program failed to link, log:\n";
		GLint len;
		glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> log(len, '\0');
		log.resize(len);
		glGetProgramInfoLog(program, log.size(), 0, log.data());
		std::cout << log.data() << "\n";
	}
	for (GLuint s : shaders){
		glDetachShader(program, s);
		glDeleteShader(s);
	}
	if (status == GL_FALSE){
		glDeleteProgram(program);
		return -1;
	}
	return program;
}
/* Swap rows of n bytes pointed to by a with those pointed to by b
 * for use in doing the y-flip for images so OpenGL has them right-side up
 */
static void swap_row(unsigned char *a, unsigned char *b, size_t n){
	for (size_t i = 0; i < n; ++i){
		std::swap(a[i], b[i]);
	}
}
GLuint glt::load_texture(const std::string &file, size_t *width, size_t *height){
	int x, y, n;
	unsigned char *img = stbi_load(file.c_str(), &x, &y, &n, 0);
	if (!img){
		std::cerr << "Failed to load image " << file
			<< stbi_failure_reason() << std::endl;
		return 0;
	}
	if (width){
		*width = x;
	}
	if (height){
		*height = y;
	}
	GLenum format;
	switch (n){
		case 1:
			format = GL_RED;
			break;
		case 2:
			format = GL_RG;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 4:
			format = GL_RGBA;
			break;
	}
	for (int i = 0; i < y / 2; ++i){
		swap_row(&img[i * x * n], &img[(y - i - 1) * x * n], x * n);
	}

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, format, x, y, 0, format, GL_UNSIGNED_BYTE, img);
	glGenerateMipmap(GL_TEXTURE_2D);
	stbi_image_free(img);
	return tex;
}
GLuint glt::load_texture_array(const std::vector<std::string> &files, size_t *w, size_t *h){
	assert(!files.empty());
	int x, y, n;
	std::vector<unsigned char*> images;
	//We need to load the first image to get the dimensions and format we're loading
	images.push_back(stbi_load(files.front().c_str(), &x, &y, &n, 0));
	if (w){
		*w = x;
	}
	if (h){
		*h = y;
	}
	for (auto it = ++files.begin(); it != files.end(); ++it){
		int ix, iy, in;
		images.push_back(stbi_load(it->c_str(), &ix, &iy, &in, 0));
		if (images.back() == nullptr){
			throw std::runtime_error("load_texture_array error: image " + *it + " not found");
		}
		if (x != ix || y != iy || n != in){
			throw std::runtime_error("load_texture_array error: Attempt to create array of incompatible images");
			for (auto i : images){
				stbi_image_free(i);
			}
			return 0;
		}
	}
	//Perform y-swap on each loaded image
	for (auto img : images){
		for (int i = 0; i < y / 2; ++i){
			swap_row(&img[i * x * n], &img[(y - i - 1) * x * n], x * n);
		}
	}
	GLenum format;
	switch (n){
		case 1:
			format = GL_RED;
			break;
		case 2:
			format = GL_RG;
			break;
		case 3:
			format = GL_RGB;
			break;
		case 4:
			format = GL_RGBA;
			break;
	}

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, format, x, y, images.size(), 0, format, GL_UNSIGNED_BYTE, NULL);
	//Upload all the textures in the array
	for (size_t i = 0; i < images.size(); ++i){
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, i, x, y, 1, format, GL_UNSIGNED_BYTE, images.at(i));
	}
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	//Clean up all the image data
	for (auto i : images){
		stbi_image_free(i);
	}
	return tex;
}
#ifdef _WIN32
// Windows stuff from: https://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
// and http://stackoverflow.com/questions/10121560/stdthread-naming-your-thread
const DWORD MS_VC_EXCEPTION = 0x406D1388;

#pragma pack(push,8)
struct WinThreadInfo {
	DWORD dwType; // Must be 0x1000.
	LPCSTR szName; // Pointer to name (in user addr space).
	DWORD dwThreadID; // Thread ID (-1=caller thread).
	DWORD dwFlags; // Reserved for future use, must be zero.
};
#pragma pack(pop)
void glt::set_thread_name(std::thread &thread, const char *name) {
	DWORD thread_id = GetThreadId(static_cast<HANDLE>(thread.native_handle()));
	WinThreadInfo info;
	info.dwType = 0x1000;
	info.szName = name;
	info.dwThreadID = thread_id;
	info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
	__try {
		RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
	}
#pragma warning(pop)
}

#else

void glt::set_thread_name(std::thread &thread, const char *name) {
	pthread_setname_np(thread.native_handle(), name);
}
#endif

