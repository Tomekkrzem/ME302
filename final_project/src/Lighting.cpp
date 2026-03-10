#include "Lighting.h"
#include "Camera.h"
#include <glm/gtc/type_ptr.hpp>

// --------------------- Lighting Global Definitions ---------------------

glm::vec3 gLightPos   = glm::vec3(10.0f, 20.0f, 10.0f);
glm::vec3 gLightColor = glm::vec3(1.0f,  1.0f, 1.0f);  // White light
float gAmbientStrength  = 0.1f;
float gSpecularStrength = 0.5f;

// --------------------- Lighting Functions ---------------------

void SetLightUniforms(GLuint shaderProgram) {
    glUniform3f(glGetUniformLocation(shaderProgram, "uLightPos"),         gLightPos.x,   gLightPos.y,   gLightPos.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "uLightColor"),       gLightColor.x, gLightColor.y, gLightColor.z);
    glUniform3f(glGetUniformLocation(shaderProgram, "uCameraPos"),        gCameraPos.x,  gCameraPos.y,  gCameraPos.z);
    glUniform1f(glGetUniformLocation(shaderProgram, "uAmbientStrength"),  gAmbientStrength);
    glUniform1f(glGetUniformLocation(shaderProgram, "uSpecularStrength"), gSpecularStrength);
}