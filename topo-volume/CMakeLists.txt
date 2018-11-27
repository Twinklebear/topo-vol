cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(TopoVol)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${TopoVol_SOURCE_DIR}/cmake")
set(RESOURCE_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/res)

find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)
find_package(TTKVTK REQUIRED)

include_directories(${SDL2_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${GLM_INCLUDE_DIRS})

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DGLM_FORCE_RADIANS)
add_subdirectory(imgui-1.49)
add_subdirectory(glt)
add_subdirectory(res)

add_executable(topo-volume main.cpp volume.cpp transfer_function.cpp tree_widget.cpp persistence_curve_widget.cpp)
set_target_properties(topo-volume PROPERTIES CXX_STANDARD 14)

target_link_libraries(topo-volume PUBLIC
	${VTK_LIBRARIES}
	${SDL2_LIBRARY}
	imgui
	glt
	${OPENGL_LIBRARIES}
	ttk::vtk::ttkFTMTree
	ttk::vtk::ttkPersistenceCurve
	ttk::vtk::ttkPersistenceDiagram
	ttk::vtk::ttkTopologicalSimplification)

if (TARGET OpenGL::GLX)
	target_link_libraries(topo-volume PUBLIC OpenGL::GLX)
endif()

