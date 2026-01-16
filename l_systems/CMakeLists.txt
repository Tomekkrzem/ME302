cmake_minimum_required(VERSION 3.15)
project(myproj LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DEPS_DIR "C:/Dev/CppDep")
set(GLFW_DIR "${DEPS_DIR}/glfw-3.4.bin.WIN64")
set(GLAD_DIR "${DEPS_DIR}/glad")

set(GLFW_LIB_DIR "${GLFW_DIR}/lib-mingw-w64")

add_library(glad STATIC
    "${GLAD_DIR}/src/glad.c"
)
target_include_directories(glad PUBLIC
    "${GLAD_DIR}/include"
)


add_executable(l_systems
    src/main.cpp
)

target_include_directories(l_systems PRIVATE
    "${GLFW_DIR}/include"
    "${GLAD_DIR}/include"
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/glm
)

target_link_directories(l_systems PRIVATE
    "${GLFW_LIB_DIR}"
)

target_link_libraries(l_systems PRIVATE
    glad
    glfw3
    opengl32
)