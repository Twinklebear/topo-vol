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
execute_command(COMMAND ${CMAKE_COMMAND} -E diff ${TTK_DIR}/ttk.cmake ${CMAKE_CURRENT_LIST_DIR}/ttk.cmake
	RESULT_VARIABLE ttk_diff OUTPUT_QUIET)
if (ttk_diff)
	message(WARNING "Copying the modified ttk.cmake over ttk's default ttk.cmake")
	file(COPY ${CMAKE_CURRENT_LIST_DIR}/ttk.cmake DESTINATION ${TTK_DIR}/ttk.cmake)
endif()

include(${TTK_DIR}/ttk.cmake)

# include the different packages required for the application
ttk_add_vtkWrapper_package(vtkContourForests)

# wrap-up the library
ttk_add_source(ttk-bs.cpp)
ttk_external_lib(ttk-bs)
get_property(PROJECT_INCLUDES DIRECTORY 
	${CMAKE_CURRENT_SOURCE_DIR} PROPERTY INCLUDE_DIRECTORIES)
message(STATUS "TTK BS ${PROJECT_INCLUDES}")
message(STATUS "ttk BS deps: ${PROJECT_DEP}")
message(STATUS "ttk BS flags: ${PROJECT_FLAGS}")
message(STATUS "ttk BS src: ${PROJECT_SRC}")
set(TTK_INCLUDE_DIRS ${PROJECT_INCLUDES} PARENT_SCOPE)

