#include <thread>
#include <chrono>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "imgui-1.49/imgui.h"
#include "imgui-1.49/imgui_impl_sdl_gl3.h"

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

#include "glt/gl_core_4_5.h"
#include "external_gl_renderer.h"
#include "glt/arcball_camera.h"

static int WIN_WIDTH = 1280;
static int WIN_HEIGHT = 720;

SDL_Window *win = nullptr;
SDL_GLContext ctx = nullptr;

static void vtk_make_current(vtkObject *caller, long unsigned int event_id,
		void *client_data, void *call_data)
{
	if (win && ctx) {
		SDL_GL_MakeCurrent(win, ctx);
	}
}

int main(int argc, const char **argv) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
		std::cerr << "Failed to initialize SDL\n";
		return 1;
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	win = SDL_CreateWindow("vtk-test", SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_OPENGL);
	if (!win){
		std::cout << "Failed to open SDL window: " << SDL_GetError() << "\n";
		return 1;
	}
	ctx = SDL_GL_CreateContext(win);
	if (!ctx){
		std::cout << "Failed to get OpenGL context: " << SDL_GetError() << "\n";
		return 1;
	}
	if (ogl_LoadFunctions() == ogl_LOAD_FAILED){
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Failed to Load OpenGL Functions",
				"Could not load OpenGL functions for 4.4, OpenGL 4.4 or higher is required",
				NULL);
		std::cout << "ogl load failed" << std::endl;
		SDL_GL_DeleteContext(ctx);
		SDL_DestroyWindow(win);
		SDL_Quit();
		return 1;
	}
	SDL_GL_SetSwapInterval(1);

	ImGui_ImplSdlGL3_Init(win);
	glEnable(GL_DEPTH_TEST);
	glClearDepth(1.0);

	// Stuff from the external rendering test
	// https://github.com/Kitware/VTK/blob/master/Rendering/External/Testing/Cxx/TestGLUTRenderWindow.cxx
	vtkNew<vtkExternalOpenGLRenderWindow> renderWindow;
	vtkNew<ExternalVTKWidget> externalWidget;

	externalWidget->SetRenderWindow(renderWindow.GetPointer());
	externalWidget->GetRenderWindow()->SetSize(WIN_WIDTH, WIN_HEIGHT);
	vtkNew<vtkCallbackCommand> callback;
	callback->SetCallback(vtk_make_current);
	renderWindow->AddObserver(vtkCommand::WindowMakeCurrentEvent, callback.GetPointer());
	externalWidget->GetRenderWindow()->Initialize();

	vtkNew<vtkPolyDataMapper> mapper;
	vtkNew<vtkActor> actor;
	actor->SetMapper(mapper.GetPointer());
	actor->GetProperty()->SetDiffuse(1);
	vtkNew<ExternalOpenGLRenderer> ren;
	renderWindow->AddRenderer(ren.GetPointer());
	ren->AddActor(actor.GetPointer());
	ren->PreserveColorBufferOff();
	ren->SetBackground(0.1, 0.1, 0.1);
	ren->AutomaticLightCreationOn();

	vtkNew<vtkSphereSource> sphere;
	sphere->SetCenter(0, 0, 0);
	sphere->SetRadius(1.0);
	sphere->Update();
	mapper->SetInputConnection(sphere->GetOutputPort());

	vtkNew<vtkLight> light;
	light->SetDiffuseColor(0.9, 0.3, 0.3);
	light->SetAmbientColor(0.2, 0.2, 0.2);
	light->SetSpecularColor(1, 1, 1);
	light->SwitchOn();
	ren->AddLight(light.GetPointer());

	const glm::mat4 proj_mat = glm::perspective(glm::radians(65.f),
			static_cast<float>(WIN_WIDTH) / WIN_HEIGHT, 0.1f, 500.f);
	const glm::mat4 start_cam = glm::lookAt(glm::vec3{0, 0, 4}, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0});
	ren->set_camera(proj_mat, start_cam);

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
		externalWidget->GetRenderWindow()->Render();

		// TODO: Any additional OpenGL rendering we want to do should
		// be done here, VTK seems to not handle the preserve color/depth
		// buffer flags well.

		// Put our UI on top of the vtk output
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

