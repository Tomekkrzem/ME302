#pragma once
#include <glad/glad.h>
#include <string>

extern GLuint gShaderProgram;
extern GLuint gShadowShaderProgram ;
/*
* Loads a Shader File and Returns it as a String
*
*  @param filename : Path to Shader File
*  @return Shader Source Code as String
*/
std::string LoadShaderAsString(const std::string& filename);

/*
* Compile and Validate a Shader
*
*  @param type   : GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
*  @param source : Shader Source Code as String
*  @return ID of Compiled Shader Object
*/
GLuint CompileShader(GLuint type, const std::string& source);

/*
* Links a Vertex and Fragment Shader into a Shader Program
*
*  @param vertexShaderSrc   : Vertex Shader Source Code as String
*  @param fragmentShaderSrc : Fragment Shader Source Code as String
*  @return ID of Linked Program Object
*/
GLuint CreateShaderProgram(const std::string& vertexShaderSrc,
                           const std::string& fragmentShaderSrc);

/*
* Loads Shader Files and Builds the Full Graphics Pipeline
* Sets gShaderProgram
*
*  @return void
*/
void CreateGraphicsPipeline();