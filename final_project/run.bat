@echo off
setlocal 

cmake --preset g++-debug || exit /b
cmake --build build\debug || exit /b

build\debug\final_project.exe %*