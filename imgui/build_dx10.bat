@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
call build_imgui.bat
@set OUT_DIR=Build
@set OUT_EXE=test_dx10
@set INCLUDES=/I .\include /I "%WindowsSdkDir%Include\um" /I "%WindowsSdkDir%Include\shared" /I "%DXSDK_DIR%Include"
@set SOURCES=src\main_dx10.cpp
@set LIBS=/LIBPATH:"%DXSDK_DIR%/Lib/x86" d3d10.lib d3dcompiler.lib win32_lib\libimgui_win32.lib
mkdir %OUT_DIR%
cl /nologo /Zi /MD %INCLUDES% /D UNICODE /D _UNICODE %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS%
