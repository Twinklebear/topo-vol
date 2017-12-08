cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(TopoVol)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${TopoVol_SOURCE_DIR}/cmake")
set(RESOURCE_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/res)

find_package(VTK REQUIRED)
find_package(SDL2 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)
find_package(TTKBase REQUIRED)
find_package(TTKVTK REQUIRED)
find_package(OpenMP REQUIRED)

include(${VTK_USE_FILE})
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
	${OPENGL_LIBRARIES}
	imgui
	glt
	ttk::base::contourForests
	ttk::vtk::ttkContourForests
	ttk::vtk::ttkPersistenceCurve
	ttk::vtk::ttkPersistenceDiagram
	ttk::vtk::ttkTopologicalSimplification)


