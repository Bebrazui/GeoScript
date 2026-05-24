@echo off
call "D:\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd D:\GeoScript
cmake -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake --build build
pause