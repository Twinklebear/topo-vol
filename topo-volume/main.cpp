#include <thread>
#include <chrono>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "imgui-1.49/imgui.h"
#include "imgui-1.49/imgui_impl_sdl_gl3.h"
#include "glt/gl_core_4_5.h"
#include "glt/arcball_camera.h"
#include "glt/debug.h"

#include "vtkPolyDataMapper.h"
#include "vtkActor.h"
#include "vtkCamera.h"
#include "ExternalVTKWidget.h"
#include "vtkExternalOpenGLRenderer.h"
#include "vtkExternalOpenGLCamera.h"
#include "vtkExternalOpenGLRenderWindow.h"
#include "vtkNew.h"
#include "vtkSphereSource.h"
#include "vtkCallbackCommand.h"
#include "vtkLight.h"
#include "vtkProperty.h"
#include "external_gl_renderer.h"

static int WIN_WIDTH = 1280;
static int WIN_HEIGHT = 720;

void setup_window(SDL_Window *&win, SDL_GLContext &ctx);

int main(int argc, const char **argv) {
	SDL_Window *win = nullptr;
	SDL_GLContext ctx = nullptr;
	setup_window(win, ctx);

	const glm::mat4 proj_mat = glm::perspective(glm::radians(65.f),
			static_cast<float>(WIN_WIDTH) / WIN_HEIGHT, 0.1f, 500.f);
	const glm::mat4 start_cam = glm::lookAt(glm::vec3{0, 0, 4}, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});

	bool quit = false;
	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e)){
			ImGui_ImplSdlGL3_ProcessEvent(&e);

			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
				quit = true;
				break;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glViewport(0, 0, WIN_WIDTH, WIN_HEIGHT);

		// Draw UI
		ImGui_ImplSdlGL3_NewFrame(win);
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
				1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
		ImGui::Render();

		SDL_GL_SwapWindow(win);
	}

	ImGui_ImplSdlGL3_Shutdown();
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();

	return 0;
}
void setup_window(SDL_Window *&win, SDL_GLContext &ctx) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
		throw std::runtime_error("Failed to initialize SDL");
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#ifndef NDEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

	win = SDL_CreateWindow("Topology Guided Volume Exploration", SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_OPENGL);
	if (!win){
		const std::string err = std::string("Failed to open SDL window: ") + SDL_GetError();
		throw std::runtime_error(err);
	}
	ctx = SDL_GL_CreateContext(win);
	if (!ctx){
		std::cout << "Failed to get OpenGL context: " << SDL_GetError() << "\n";
		const std::string err = std::string("Failed to open SDL window: ") + SDL_GetError();
		throw std::runtime_error(err);
	}
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED){
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to Load OpenGL Functions",
				"Could not load OpenGL functions for 4.4, OpenGL 4.4 or higher is required",
				NULL);
		SDL_GL_DeleteContext(ctx);
		SDL_DestroyWindow(win);
		SDL_Quit();
		throw std::runtime_error("OpenGL Load failed");
	}
	SDL_GL_SetSwapInterval(1);

#ifndef NDEBUG
	glt::register_debug_callback();
	glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER,
			0, GL_DEBUG_SEVERITY_NOTIFICATION, 16, "DEBUG LOG START");
#endif

	ImGui_ImplSdlGL3_Init(win);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0);
}

