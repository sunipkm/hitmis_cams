@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
@set OUT_DIR=build
@set OUT_EXE=image_display
@set INCLUDES=/I .\ /I .\include /I .\imgui\include /I "%DXSDK_DIR%/Include"
@set SOURCES=guimain.cpp ^
jpge.cpp
@set LIBS=/LIBPATH:"%DXSDK_DIR%/Lib/x86" d3d9.lib imgui\win32_lib\libimgui_win32.lib build\picam.lib lib\pvcam32.lib lib\picam32.lib
mkdir %OUT_DIR%
cl /nologo /Zi /MD %INCLUDES% /D UNICODE /D _UNICODE %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%