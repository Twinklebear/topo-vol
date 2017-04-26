project(ttk-bs)

option(withVTKCMD "Enable VTK CMD support" true)
option(withVTK "Enable VTK support" true)
set(withVTKCMD true)
set(withVTK true)

# path to the common code base
find_path(TTK_DIR ttk.cmake
  PATHS
  ${TTK_ROOT}/core/
)
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${TTK_DIR}/ttk.cmake ${CMAKE_CURRENT_LIST_DIR}/ttk.cmake
	RESULT_VARIABLE ttk_diff OUTPUT_QUIET)
if (ttk_diff)
	message(WARNING "Copying the modified ttk.cmake over ttk's default ttk.cmake")
	file(COPY ${CMAKE_CURRENT_LIST_DIR}/ttk.cmake DESTINATION ${TTK_DIR})
endif()

include(${TTK_DIR}/ttk.cmake)

add_definitions(-DwithParallelSimplify)

# include the different packages required for the application
ttk_add_vtkWrapper_package(vtkContourForests)
ttk_add_vtkWrapper_package(vtkPersistenceCurve)
ttk_add_vtkWrapper_package(vtkPersistenceDiagram)
ttk_add_vtkWrapper_package(vtkTopologicalSimplification)

# This is basically a dummy wrapper lib to hold the ttk libraries
# because they don't build their code as a proper library and
# instead do this insane injection of their src files and deps
# into the using project and force all these crappy cmake wrapper macros
# on you instead.
ttk_external_lib(ttk-bs)
get_property(PROJECT_INCLUDES DIRECTORY 
	${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)

# Export the includes so the TopoVol app can include TTK
set(TTK_INCLUDE_DIRS ${PROJECT_INCLUDES} PARENT_SCOPE)
set(TTK_CMAKE_CXX_FLAGS ${PROJECT_FLAGS} PARENT_SCOPE)

