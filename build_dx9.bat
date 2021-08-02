@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
@set RC_INCLUDE_PATH="C:\Program Files\Windows Kits\10\Include\10.0.19041.0\um"
@set OUT_DIR=build
@set OUT_EXE=hitmis
@set INCLUDES= /I .\include /I "%DXSDK_DIR%/Include"
@set SOURCES=src/*.cpp
@set LIBS=/LIBPATH:"%DXSDK_DIR%/Lib/x86" d3d9.lib lib\Pvcam32.lib lib\ATMCD32M.LIB lib\cfitsio.lib lib\libimgui_dx9.lib
@set RESOURCES=hitmis.res
mkdir %OUT_DIR%
rc /nologo /i %RC_INCLUDE_PATH% /i .\include\ /fo %OUT_DIR%/%RESOURCES% src/hitmis_resource.rc
cl /nologo /Zi /EHsc /MD %INCLUDES% /D UNICODE /D _UNICODE %SOURCES% /Fe%OUT_DIR%/%OUT_EXE%.exe /Fo%OUT_DIR%/ /link %LIBS% %OUT_DIR%\%RESOURCES%