@REM Build for Visual Studio compiler. Run your copy of vcvars32.bat or vcvarsall.bat to setup command-line compiler.
@set OUT_DIR=build
@set OUT_LIB=picam.lib
@set INCLUDES=/I .\
@set SOURCES=CriticalSection.cpp ^
CameraUnit_PI.cpp ^
ImageData.cpp
mkdir %OUT_DIR%
cl /nologo /c /Zi /MD /EHsc %INCLUDES% /D UNICODE /D _UNICODE %SOURCES% /Fo%OUT_DIR%/
@echo Done generating code
lib /nologo /out:%OUT_DIR%\%OUT_LIB% %OUT_DIR%/*.obj