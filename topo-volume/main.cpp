#include <thread>
#include <regex>
#include <vector>
#include <string>
#include <cassert>

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <vtkSmartPointer.h>
#include <vtkXMLImageDataReader.h>
#include <vtkImageReader2.h>
#include <vtkImageData.h>
#include <vtkThreshold.h>

#include <ttkFTMTree.h>
#include <ttkMorseSmaleComplex.h>
#include <ttkPersistenceCurve.h>
#include <ttkPersistenceDiagram.h>
#include <ttkTopologicalSimplification.h>

#include "imgui-1.49/imgui.h"
#include "imgui-1.49/imgui_impl_sdl_gl3.h"
#include "glt/gl_core_4_5.h"
#include "glt/arcball_camera.h"
#include "glt/debug.h"
#include "glt/buffer_allocator.h"

#include "transfer_function.h"
#include "volume.h"
#include "tree_widget.h"
#include "persistence_curve_widget.h"

static size_t WIN_WIDTH = 1280;
static size_t WIN_HEIGHT = 720;
static unsigned int debuglevel = 0;

void run_app(SDL_Window *win, const std::vector<std::string> &args);
void setup_window(SDL_Window *&win, SDL_GLContext &ctx);
void default_commands(int argc, const char **argv) {
	for (int i = 1; i < argc; ++i) {
		std::string str(argv[i]);
		if (str == "-debug") {
			debuglevel = std::stoi(argv[++i]);
		}
	}
}
// Load a VTI file or RAW volume. RAW volume files must follow
// the naming convention used in the Open SciVis Datasets collection
// https://github.com/pavolzetor/open_scivis_datasets where
// the file name is <name>_<X>x<Y>x<Z>_<data type>.raw
vtkSmartPointer<vtkImageData> load_volume(const std::string &file);

int main(int argc, const char **argv) {
	if (argc < 2) {
		std::cerr << "A filename is required!\nUsage: ./topo-vol <volume file>\n";
		return 1;
	}
	default_commands(argc, argv);
	SDL_Window *win = nullptr;
	SDL_GLContext ctx = nullptr;
	setup_window(win, ctx);
	run_app(win, std::vector<std::string>(argv, argv + argc));
	ImGui_ImplSdlGL3_Shutdown();
	SDL_GL_DeleteContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}
void run_app(SDL_Window *win, const std::vector<std::string> &args) {
	vtkSmartPointer<vtkImageData> vol = load_volume(args[1]);

	glm::vec3 vol_render_size;
	for (size_t i = 0; i < 3; ++i) {
		vol_render_size[i] = vol->GetSpacing()[i] * vol->GetDimensions()[i];
	}
	std::shared_ptr<glt::BufferAllocator> allocator = std::make_shared<glt::BufferAllocator>(size_t(64e6));

	glm::mat4 proj_mat = glm::perspective(glm::radians(65.f),
			static_cast<float>(WIN_WIDTH) / WIN_HEIGHT, 0.1f, 2000.f);
	glt::ArcBallCamera camera(glm::lookAt(glm::vec3{0.0, 0.0, vol_render_size.z},
			glm::vec3{0.0, 0.0, 0}, glm::vec3{0, 1, 0}),
			2.0, 75.0, {WIN_WIDTH, WIN_HEIGHT});

	// Note the vec3 is padded to a vec4 size, so we need a bit more room
	auto viewing_buf = allocator->alloc(2 * sizeof(glm::mat4) + sizeof(glm::vec4), glt::BufAlignment::UNIFORM_BUFFER);
	glBindBuffer(GL_UNIFORM_BUFFER, viewing_buf.buffer);
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, viewing_buf.buffer, viewing_buf.offset, viewing_buf.size);
	{
		char *buf = static_cast<char*>(viewing_buf.map(GL_UNIFORM_BUFFER,
					GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_WRITE_BIT));
		glm::mat4 *mats = reinterpret_cast<glm::mat4*>(buf);
		// While it says vec3 in the buffer with std140 layout they'll be padded to vec4's
		glm::vec4 *vecs = reinterpret_cast<glm::vec4*>(buf + 2 * sizeof(glm::mat4));
		mats[0] = proj_mat;
		mats[1] = camera.transform();
		vecs[0] = glm::vec4{camera.eye_pos(), 1};

		glBindBufferRange(GL_UNIFORM_BUFFER, 0, viewing_buf.buffer, viewing_buf.offset, viewing_buf.size);
		viewing_buf.unmap(GL_UNIFORM_BUFFER);
	}

	PersistenceCurveWidget persistence_curve_widget(vol.Get(), debuglevel);

	vtkSmartPointer<ttkFTMTree> contour_forest
		= vtkSmartPointer<ttkFTMTree>::New();
	contour_forest->SetInputConnection(persistence_curve_widget.get_simplification()->GetOutputPort());
    contour_forest->SetForceInputOffsetScalarField(0);
	contour_forest->SetInputOffsetScalarFieldName("OutputOffsetScalarField");
    contour_forest->SetSuperArcSamplingLevel(30);
	contour_forest->SetUseAllCores(true);
	contour_forest->SetThreadNumber(std::thread::hardware_concurrency());
	contour_forest->SetdebugLevel_(debuglevel);
	contour_forest->SetWithSegmentation(true);
	// Setup transfer function and volume
	TransferFunction tfcn;
	contour_forest->AddObserver(vtkCommand::EndEvent, &tfcn);

	TreeWidget tree_widget(contour_forest, persistence_curve_widget.get_simplification());
	Volume volume(vol.Get(), vtkImageData::SafeDownCast(contour_forest->GetOutput(2)));
	tfcn.histogram = &volume.histogram;

	std::vector<unsigned int> prev_seg_selection, prev_seg_palettes;
	bool ui_hovered = false;
	bool quit = false;
	bool camera_updated = false;
	int volume_render_mode = 0;
	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e)){
			ImGui_ImplSdlGL3_ProcessEvent(&e);

			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)){
				quit = true;
				break;
			}
			if (!ui_hovered) {
				camera_updated |= camera.sdl_input(e, 1000.f / ImGui::GetIO().Framerate);
			}
			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
				WIN_WIDTH = e.window.data1;
				WIN_HEIGHT = e.window.data2;
				camera.update_screen(WIN_WIDTH, WIN_HEIGHT);
				camera_updated = true;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		if (camera_updated) {
			char *buf = static_cast<char*>(viewing_buf.map(GL_UNIFORM_BUFFER, GL_MAP_WRITE_BIT));
			glm::mat4 *mats = reinterpret_cast<glm::mat4*>(buf);
			glm::vec3 *eye_pos = reinterpret_cast<glm::vec3*>(buf + 2 * sizeof(glm::mat4));
			mats[1] = camera.transform();
			*eye_pos = camera.eye_pos();

			viewing_buf.unmap(GL_UNIFORM_BUFFER);
			camera_updated = false;
		}

		glViewport(0, 0, WIN_WIDTH, WIN_HEIGHT);
		tfcn.render();
		volume.render(allocator);

		// Draw UI
		ImGui_ImplSdlGL3_NewFrame(win);
		glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);

		if (ImGui::Begin("TopoVol")) {
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
					1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}
		ImGui::End();

		tfcn.draw_ui();
		tree_widget.draw_ui();
		persistence_curve_widget.set_tree_type(tree_widget.get_tree_type());
		persistence_curve_widget.draw_ui();

		const auto &tree_selection = tree_widget.get_selection();
		const auto &seg_palettes = tfcn.get_segmentation_palettes();
		if (prev_seg_selection != tree_selection || seg_palettes != prev_seg_palettes) {
			std::fill(volume.segmentation_selections.begin(), volume.segmentation_selections.end(),
					tree_selection.empty() ? 1 : 0);
			for (const auto &x : tree_selection) {
				volume.segmentation_selections[x] = 1;
			}
			volume.segmentation_palettes = seg_palettes;
			volume.segmentation_selection_changed = true;
			prev_seg_selection = tree_selection;
			prev_seg_palettes = seg_palettes;
		}

		ui_hovered = ImGui::IsMouseHoveringAnyWindow();
		ImGui::Render();

		SDL_GL_SwapWindow(win);
	}
}
void setup_window(SDL_Window *&win, SDL_GLContext &ctx) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0){
		throw std::runtime_error("Failed to initialize SDL");
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
#ifndef NDEBUG
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif

	win = SDL_CreateWindow("Topology Guided Volume Exploration", SDL_WINDOWPOS_CENTERED,
			SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
	glClearColor(0.1, 0.1, 0.1, 1);
	glClearDepth(1.0);
}
vtkSmartPointer<vtkImageData> load_volume(const std::string &file) {
	vtkSmartPointer<vtkImageData> vol = nullptr;
	const std::string file_ext = file.substr(file.size() - 3);
	if (file_ext == "vti") {
		vtkSmartPointer<vtkXMLImageDataReader> reader
			= vtkSmartPointer<vtkXMLImageDataReader>::New();
		reader->SetFileName(file.c_str());
		reader->Update();
		vol = reader->GetOutput();
	} else if (file_ext == "raw") {
		std::cout << "Note: raw files do not have voxel spacing information, if your "
			<< "file does not have square voxels it may appear incorrectly scaled\n";

		const std::regex match_filename("(\\w+)_(\\d+)x(\\d+)x(\\d+)_(.+)\\.raw");
		auto matches = std::sregex_iterator(file.begin(), file.end(), match_filename);
		if (matches == std::sregex_iterator() || matches->size() != 6) {
			std::cerr << "Unrecognized raw volume naming scheme, expected a format like: "
				<< "'<name>_<X>x<Y>x<Z>_<data type>.raw' but '" << file << "' did not match"
				<< std::endl;
			throw std::runtime_error("Invalaid raw file naming scheme");
		}

		std::array<int, 3> dims = {std::stoi((*matches)[2]), std::stoi((*matches)[3]), std::stoi((*matches)[4])};
		std::string data_type = (*matches)[5];
		int vtk_data_type = -1;
		if (data_type == "uint8") {
			vtk_data_type = VTK_UNSIGNED_CHAR;
		} else if (data_type == "int8") {
			vtk_data_type = VTK_CHAR;
		} else if (data_type == "uint16") {
			vtk_data_type = VTK_UNSIGNED_SHORT;
		} else if (data_type == "int16") {
			vtk_data_type = VTK_SHORT;
		} else if (data_type == "float32") {
			vtk_data_type = VTK_FLOAT;
		} else {
			throw std::runtime_error("Unsupported or unrecognized data type: " + data_type);
		}

		vtkSmartPointer<vtkImageReader2> reader = vtkSmartPointer<vtkImageReader2>::New();
		reader->SetFileName(file.c_str());
		reader->SetFileDimensionality(3);
		reader->SetDataExtent(0, dims[0] - 1, 0, dims[1] - 1, 0, dims[2] - 1);
		reader->SetDataScalarType(vtk_data_type);
		reader->Update();
		vol = reader->GetOutput();
	}
	if (vol.Get() == nullptr) {
		throw std::runtime_error("Failed to load volume file " + file);
	}
	return vol;
}


