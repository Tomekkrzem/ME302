#pragma once
#include <glm/glm.hpp>

extern glm::vec3 gCameraTarget;
extern glm::vec3 gCameraUp;
extern float gCameraYaw;
extern float gCameraPitch;
extern float gCameraDistance;

extern glm::vec3 gCameraPos;
extern glm::vec3 gCameraFront;

void UpdateOrbitCamera();
void OrbitCamera(float deltaX, float deltaY);
void PanCamera(float deltaX, float deltaY);
void ZoomCamera(float delta);
glm::mat4 GetViewMatrix();