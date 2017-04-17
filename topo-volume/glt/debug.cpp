#include <iostream>
#include <cassert>
#include <iomanip>
#include <SDL.h>
#include "gl_core_4_5.h"
#include "debug.h"

using namespace glt;

void glt::register_debug_callback(){
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback(debug_callback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
	// Suppress Nvidia's binding info spam
	glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER,
			GL_DEBUG_SEVERITY_NOTIFICATION, 0, NULL, GL_FALSE);
}
#ifdef _WIN32
void APIENTRY glt::debug_callback(GLenum src, GLenum type, GLuint id, GLenum severity,
	GLsizei len, const GLchar *msg, const GLvoid*)
{
	log_debug_msg(src, type, id, severity, len, msg);
}
#else
void glt::debug_callback(GLenum src, GLenum type, GLuint id, GLenum severity,
	GLsizei len, const GLchar *msg, const GLvoid*)
{
	log_debug_msg(src, type, id, severity, len, msg);
}
#endif
void glt::log_debug_msg(GLenum src, GLenum type, GLuint, GLenum severity, GLsizei tag, const GLchar *msg){
	// Disable nvidia mapping spam
	if (tag == 300 || tag == 315){
		return;
	}
	//Print a time stamp for the message
	float sec = SDL_GetTicks() / 1000.f;
	int min = static_cast<int>(sec / 60.f);
	sec -= sec / 60.f;
	std::cout << "---------\n[" << min << ":"
		<< std::setprecision(3) << sec << "] OpenGL Debug Message";

	std::cout << "\n\tSource: ";
	switch (src){
	case GL_DEBUG_SOURCE_API:
		std::cout << "API";
		break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
		std::cout << "Window System";
		break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER:
		std::cout << "Shader Compiler";
		break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:
		std::cout << "Third Party";
		break;
	case GL_DEBUG_SOURCE_APPLICATION:
		std::cout << "Application";
		break;
	default:
		std::cout << "Other";
	}

	std::cout << "\n\tType: ";
	switch (type){
	case GL_DEBUG_TYPE_ERROR:
		std::cout << "Error";
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		std::cout << "Deprecated Behavior";
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		std::cout << "Undefined Behavior";
		break;
	case GL_DEBUG_TYPE_PORTABILITY:
		std::cout << "Portability";
		break;
	case GL_DEBUG_TYPE_PERFORMANCE:
		std::cout << "Performance";
		break;
	case GL_DEBUG_TYPE_MARKER:
		std::cout << "Marker";
		break;
	default:
		std::cout << "Other";
	}

	std::cout << "\n\tSeverity: ";
	switch (severity){
	case GL_DEBUG_SEVERITY_HIGH:
		std::cout << "High";
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		std::cout << "Medium";
		break;
	case GL_DEBUG_SEVERITY_LOW:
		std::cout << "Low";
		break;
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		std::cout << "Notification";
		break;
	default:
		std::cout << "Unknown";
	}

	std::cout << "\n\tTag: " << tag << "\n\tMessage: " << msg
		<< "\n---------\n";
	// Break for a stack trace of sorts
	assert(severity != GL_DEBUG_SEVERITY_HIGH && type != GL_DEBUG_TYPE_ERROR);
}

