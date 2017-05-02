# topo-vol

topo-vol is a topology guided volume exploration and analysis tool, written for
the final project in
[Bei Wang's Computational Topology Course](http://www.sci.utah.edu/~beiwang/teaching/cs6170-spring-2017/schedule.html).
It is built on top of the [Topology ToolKit](https://topology-tool-kit.github.io/) and
[VTK](http://www.vtk.org/) for computation, and uses
[ImGui](https://github.com/ocornut/imgui) and a custom rendering system for the UI and volume rendering.
By computing relevant topological structures (e.g. the contour tree) and classifying segments of
data corresponding to the branches in this tree we can avoid occlusion issues with global transfer
functions and create more useful, detailed renderings.
See the [report](report.pdf) for more details.

## Building

In addition to VTK and TTK you'll need [GLM](http://glm.g-truc.net/) and [SDL2](https://www.libsdl.org/),
these are likely available through your system's package manager if you're on Linux. We've only tested the
program so far on Linux, however if you can build TTK on Windows or OS X you can likely build and run our program.
OpenGL 4.3 is also required to run the program.

After getting GLM and SDL2 download and build [VTK](http://vtk.org/) and follow the
[TTK installation instructions](https://topology-tool-kit.github.io/installation.html) to build TTK.
Compiling TTK is optional, due to how they configure their build system when using it externally
the required TTK code is actually built by topo-vol when you build the project.
Once these packages are setup topo-vol can be built with CMake. When running CMake you'll
want to specify the location of the VTK cmake file if it's non-standard, and the path to ttk.cmake
in TTK's `core/` directory. For example:

```
cmake -DVTK_DIR=<path to VTKConfig.cmake> -DTTK_DIR=<path to ttk.cmake>
```

Note that due to some issues with how TTK handles compiling packages depending on it
as a library we copy our own modified ttk.cmake over the one in TTK's directory. After
this you should be able to run `make install` to build the program and copy
the shaders where the executable will look for them in the build directory. We recommend
building the Release build for better performance.

## Running

topo-vol currently supports scalar-field VTI files with data type `char`, `unsigned char` or `float`.
To get some data to test on you can load some of the raw files from the
[Open SciVis Datasets](https://github.com/pavolzetor/open_scivis_datasets)
collection in [ParaView](http://www.paraview.org/) and export them as VTI. If you
built TTK with ParaView you can use that build to convert the data. Note that larger datasets
will require much longer computation time.

Here's an example screenshot after performing separate classification of
different segments in the contour tree on the Nucleon dataset.  For an
example of using the system for analysis see the video of a [Tooth analysis session](https://youtu.be/S7Gm2hYsHKU).

![nucleon classification](http://i.imgur.com/0geW8ma.png)

