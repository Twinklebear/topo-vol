#pragma once
 
#include "gl_core_4_5.h"
 
namespace glt {
/*
 * Register the debug callback using the context capabilities to select between 4.3+ core debug
 * and ARB debug
 */
void register_debug_callback();
 
/*
 * Callbacks for core debug output and debug_output_arb to be selected
 * depending on the context capabilities
 */
#ifdef _WIN32
void APIENTRY debug_callback(GLenum src, GLenum type, GLuint id, GLenum severity,
	GLsizei len, const GLchar *msg, const GLvoid *user);
#else
void debug_callback(GLenum src, GLenum type, GLuint id, GLenum severity,
	GLsizei len, const GLchar *msg, const GLvoid *user);
#endif
 
/*
 * Debug logging function called by our registered callbacks, simply
 * logs the debug messages to stdout
 */
void log_debug_msg(GLenum src, GLenum type, GLuint id, GLenum severity, GLsizei len, const GLchar *msg);
}
 
