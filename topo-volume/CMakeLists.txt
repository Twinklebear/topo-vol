cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(TopoVol)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${TopoVol_SOURCE_DIR}/cmake")
set(RESOURCE_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/res)

find_package(VTK REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLM REQUIRED)
find_package(OpenGL REQUIRED)
include(${VTK_USE_FILE})
include_directories(${SDL2_INCLUDE_DIR} ${OPENGL_INCLUDE_DIR} ${GLM_INCLUDE_DIRS})

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_BINARY_DIR})
add_definitions(-DGLM_FORCE_RADIANS)
add_subdirectory(imgui-1.49)
add_subdirectory(glt)
add_subdirectory(res)
add_subdirectory(ttk-bs)
include_directories(${TTK_INCLUDE_DIRS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TTK_CMAKE_CXX_FLAGS} -fopenmp")

add_executable(topo-volume main.cpp volume.cpp transfer_function.cpp tree_widget.cpp)
set_property(TARGET topo-volume PROPERTY CXX_STANDARD 11)
target_link_libraries(topo-volume ${VTK_LIBRARIES} ${SDL2_LIBRARY}
	${OPENGL_LIBRARIES} imgui glt ttk-bs)

