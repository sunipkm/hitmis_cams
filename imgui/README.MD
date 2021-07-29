# Dear ImGui base library with GLFW backend (Unix) and DirectX backend (Windows NT)

Upgraded to WIP 1.84.

## Unix-like Systems
To build the library, execute `make -j$(nproc)` in the directory. To enable ImPlot support (adds to compile time significantly),
pass the option `implot=1` to the `make` command.

To compile a program with the library,

1. Install libglfw backend (Ubuntu based linux: `sudo apt install libglfw3-dev`, macOS: `brew install libglfw`)
1. Point include flag to `imgui/include`. To include the headers in your program, use e.g. `imgui/imgui.h`.
To use implot in your program, you need to compile the library with the `implot=1` option.
1. Compile your program with `-limgui_glfw` flag (with linker search path flag, `-L`, pointing to the correct link path)
1. Make sure to pass the following flags as well for complete linkage:
    macOS: `-arch $(ARCH) -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo -L/usr/local/lib -lglfw`,
    assuming `brew` linked the GLFW library files to `/usr/local/lib`. Update this path as necessary.

    Linux: ``-lGL `pkg-config --static --libs glfw3` ``

## Windows NT Systems
Install Visual Studio C++ Compiler 2010 (MSVC 1600) or above, and the DirectX June 2010 SDK.
Open command prompt and load vcvarsall.bat with x86 option to load compiler path settings.
Execute build_dxYY.bat to compile.

### WSL OpenGL3 support and issues
To use OpenGL3 with VcXsrV and WSL, perform the following steps:
1. Disable `LIBGL_ALWAYS_INDIRECT` by editing `/etc/wsl-integration.sh`.
2. Do not use `-wgl` command line argument (or Native opengl option in GUI) in VcXsrV.
With these settings, the program runs on WSL.

