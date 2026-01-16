@echo off
setlocal

cmake --preset ucrt64-debug || exit /b

cmake --build build\debug || exit /b

build\debug\l_systems.exe %*