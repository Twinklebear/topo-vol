add_library(glt gl_core_4_5.c debug.cpp buffer_allocator.cpp util.cpp arcball_camera.cpp)
set_property(TARGET glt PROPERTY CXX_STANDARD 11)

