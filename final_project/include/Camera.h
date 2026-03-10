#pragma once
#include <glm/glm.hpp>

extern glm::vec3 gCameraPos;
extern glm::vec3 gCameraFront;
extern glm::vec3 gCameraUp;
extern float gCameraSpeed;
extern float gYaw;
extern float gPitch;
extern float gLastMouseX;
extern float gLastMouseY;
extern bool  gFirstMouse;

void MouseLook(float xOffset, float yOffset);
glm::mat4 GetViewMatrix();