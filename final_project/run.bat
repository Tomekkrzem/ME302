@echo off
setlocal 

cmake --preset g++-debug || exit /b
cmake --build build\debug || exit /b

if exist gen_logs rmdir /s /q gen_logs

build\debug\final_project.exe %*