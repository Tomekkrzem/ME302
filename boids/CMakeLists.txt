cmake_minimum_required(VERSION 3.15)
project(boids LANGUAGES C CXX)

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

add_executable(boids
    src/main.cpp
)

find_package(Freetype REQUIRED)
target_link_libraries(boids PRIVATE Freetype::Freetype)

target_include_directories(boids PRIVATE
    "${GLFW_DIR}/include"
    "${GLAD_DIR}/include"
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/glm
    ${CMAKE_SOURCE_DIR}/resources
    ${CMAKE_SOURCE_DIR}/headers
)

target_link_directories(boids PRIVATE
    "${GLFW_LIB_DIR}"
)

target_link_libraries(boids PRIVATE
    glad
    glfw3
    opengl32
)