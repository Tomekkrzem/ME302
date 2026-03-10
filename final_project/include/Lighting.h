#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>

// --------------------- Lighting Globals ---------------------

extern glm::vec3 gLightPos;
extern glm::vec3 gLightColor;
extern float gAmbientStrength;
extern float gSpecularStrength;

// --------------------- Lighting Functions ---------------------

void SetLightUniforms(GLuint shaderProgram);