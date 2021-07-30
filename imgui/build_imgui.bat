@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
@set OUT_DIR=win32_lib
@set INCLUDES=/I .\ /I .\include\ /I .\include\imgui\ /I .\include\backend\ /I "%WindowsSdkDir%Include\um" /I "%WindowsSdkDir%Include\shared" /I "%DXSDK_DIR%Include"
@set SOURCES=src\imgui_impl_win32.cpp ^
src\imgui_impl_dx9.cpp ^
src\imgui_impl_dx10.cpp ^
src\imgui_impl_dx11.cpp ^
src\imgui.cpp ^
src\imgui_demo.cpp ^
src\imgui_draw.cpp ^
src\imgui_tables.cpp ^
src\imgui_widgets.cpp
mkdir %OUT_DIR%
cl /nologo /c /Zi /MD %INCLUDES% /D UNICODE /D _UNICODE %SOURCES% /Fo%OUT_DIR%/
@echo Done generating code
lib /nologo /out:%OUT_DIR%\libimgui_win32.lib %OUT_DIR%/*.obj