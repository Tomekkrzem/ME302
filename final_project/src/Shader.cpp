#include "Shader.h"
#include <iostream>
#include <fstream>
#include <sstream>

GLuint gShaderProgram = 0;
GLuint gShadowShaderProgram = 0;

std::string LoadShaderAsString(const std::string& filename) {
    std::string result = "";
    std::string line   = "";
    std::ifstream myFile(filename.c_str());

    if (myFile.is_open()) {
        while (std::getline(myFile, line)) {
            result += line + '\n';
        }
        myFile.close();
    } else {
        std::cout << "ERROR: Could not open shader file: " << filename << std::endl;
    }

    return result;
}

GLuint CompileShader(GLuint type, const std::string& source) {
    GLuint shaderObject;

    if (type == GL_VERTEX_SHADER) {
        shaderObject = glCreateShader(GL_VERTEX_SHADER);
    } else {
        shaderObject = glCreateShader(GL_FRAGMENT_SHADER);
    }

    const char* src = source.c_str();
    glShaderSource(shaderObject, 1, &src, nullptr);
    glCompileShader(shaderObject);

    int result;
    glGetShaderiv(shaderObject, GL_COMPILE_STATUS, &result);

    if (result == GL_FALSE) {
        int length;
        glGetShaderiv(shaderObject, GL_INFO_LOG_LENGTH, &length);
        char* errorMessage = new char[length];
        glGetShaderInfoLog(shaderObject, length, &length, errorMessage);

        if (type == GL_VERTEX_SHADER) {
            std::cout << "ERROR: GL_VERTEX_SHADER Compilation Failed!\n" << errorMessage << "\n";
        } else if (type == GL_FRAGMENT_SHADER) {
            std::cout << "ERROR: GL_FRAGMENT_SHADER Compilation Failed!\n" << errorMessage << "\n";
        }

        delete[] errorMessage;
        glDeleteShader(shaderObject);
        return 0;
    }

    return shaderObject;
}

GLuint CreateShaderProgram(const std::string& vertexShaderSrc,
                           const std::string& fragmentShaderSrc) {
    GLuint programObject = glCreateProgram();

    GLuint myVS = CompileShader(GL_VERTEX_SHADER,   vertexShaderSrc);
    GLuint myFS = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    glAttachShader(programObject, myVS);
    glAttachShader(programObject, myFS);
    glLinkProgram(programObject);
    glValidateProgram(programObject);

    glDetachShader(programObject, myVS);
    glDetachShader(programObject, myFS);
    glDeleteShader(myVS);
    glDeleteShader(myFS);

    return programObject;
}

void CreateGraphicsPipeline() {
    std::string VSsrc = LoadShaderAsString("shaders/vertex.glsl");
    std::string FSsrc = LoadShaderAsString("shaders/fragment.glsl");
    gShaderProgram = CreateShaderProgram(VSsrc, FSsrc);

    std::string shadowVS = LoadShaderAsString("shaders/shadow_vertex.glsl");
    std::string shadowFS = LoadShaderAsString("shaders/shadow_fragment.glsl");
    gShadowShaderProgram = CreateShaderProgram(shadowVS, shadowFS);
}