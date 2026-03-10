#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

// --------------------- Camera Global Definitions ---------------------
glm::vec3 gCameraPos   = glm::vec3(0.0f, 2.0f, 6.0f);
glm::vec3 gCameraFront = glm::vec3(0.0f, -0.3f, -1.0f);
glm::vec3 gCameraUp    = glm::vec3(0.0f, 1.0f,  0.0f);
float gCameraSpeed     = 0.01f;
float gYaw             = -90.0f;
float gPitch           = 0.0f;
float gLastMouseX      = 640.0f;   // Half of default screen width
float gLastMouseY      = 480.0f;   // Half of default screen height
bool  gFirstMouse      = true;

// --------------------- Camera Functions ---------------------
void MouseLook(float xOffset, float yOffset) {
    float sensitivity = 0.1f;
    xOffset *= sensitivity;
    yOffset *= sensitivity;

    gYaw   += xOffset;
    gPitch -= yOffset;

    // Clamp pitch so camera doesn't flip
    if (gPitch >  89.0f) gPitch =  89.0f;
    if (gPitch < -89.0f) gPitch = -89.0f;

    // Recalculate front vector from yaw and pitch
    glm::vec3 front;
    front.x = cos(glm::radians(gYaw)) * cos(glm::radians(gPitch));
    front.y = sin(glm::radians(gPitch));
    front.z = sin(glm::radians(gYaw)) * cos(glm::radians(gPitch));
    gCameraFront = glm::normalize(front);
}

glm::mat4 GetViewMatrix() {
    return glm::lookAt(gCameraPos, gCameraPos + gCameraFront, gCameraUp);
}