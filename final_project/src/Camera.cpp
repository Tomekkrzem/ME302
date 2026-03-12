#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

glm::vec3 gCameraTarget   = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 gCameraUp       = glm::vec3(0.0f, 1.0f, 0.0f);
float gCameraYaw          = -90.0f;
float gCameraPitch        = 30.0f;
float gCameraDistance     = 28.0f;

glm::vec3 gCameraPos      = glm::vec3(0.0f);
glm::vec3 gCameraFront    = glm::vec3(0.0f, 0.0f, -1.0f);

static float ClampPitch(float pitch) {
    return std::clamp(pitch, -89.0f, 89.0f);
}

static float ClampDistance(float dist) {
    return std::clamp(dist, 1.0f, 300.0f);
}

void UpdateOrbitCamera() {
    gCameraPitch = ClampPitch(gCameraPitch);
    gCameraDistance = ClampDistance(gCameraDistance);

    float yawRad   = glm::radians(gCameraYaw);
    float pitchRad = glm::radians(gCameraPitch);

    glm::vec3 offset;
    offset.x = gCameraDistance * cos(pitchRad) * cos(yawRad);
    offset.y = gCameraDistance * sin(pitchRad);
    offset.z = gCameraDistance * cos(pitchRad) * sin(yawRad);

    gCameraPos = gCameraTarget + offset;
    gCameraFront = glm::normalize(gCameraTarget - gCameraPos);
}

void OrbitCamera(float deltaX, float deltaY) {
    float rotateSpeed = 0.25f;
    gCameraYaw   -= deltaX * rotateSpeed;
    gCameraPitch += deltaY * rotateSpeed;
    UpdateOrbitCamera();
}

void PanCamera(float deltaX, float deltaY) {
    glm::vec3 right = glm::normalize(glm::cross(gCameraFront, gCameraUp));
    glm::vec3 up    = glm::normalize(glm::cross(right, gCameraFront));

    float panSpeed = 0.001f * gCameraDistance;

    gCameraTarget += (-right * deltaX + up * deltaY) * panSpeed;
    UpdateOrbitCamera();
}

void ZoomCamera(float delta) {
    float zoomSpeed = 1.5f;
    gCameraDistance -= delta * zoomSpeed;
    UpdateOrbitCamera();
}

glm::mat4 GetViewMatrix() {
    return glm::lookAt(gCameraPos, gCameraTarget, gCameraUp);
}