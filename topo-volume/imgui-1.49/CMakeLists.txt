add_library(imgui imgui.cpp imgui_demo.cpp imgui_draw.cpp
	imgui_impl_sdl_gl3.cpp)
set_property(TARGET imgui PROPERTY CXX_STANDARD 11)

