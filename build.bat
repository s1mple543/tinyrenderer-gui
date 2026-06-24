@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 exit /b 1
cd /d "%~dp0"
cmake -B build -G Ninja -DCMAKE_CXX_COMPILER=cl -DCMAKE_C_COMPILER=cl
cmake --build build -- -j%NUMBER_OF_PROCESSORS%
echo ---
echo Binary: build\tinyrenderer-gui.exe
echo Run:    build\tinyrenderer-gui.exe
