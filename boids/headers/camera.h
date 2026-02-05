#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>


struct CameraController
{
    // Orbit target + spherical params
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    float distance = 18.0f;
    float yaw = glm::radians(0.0f);     // around world up
    float pitch = glm::radians(-15.0f); // up/down

    // Interaction state
    bool draggingOrbit = false;
    bool draggingPan = false;
    double lastX = 0.0, lastY = 0.0;

    // Settings / caps
    float orbitSensitivity = 0.0065f;   // radians per pixel
    float panSensitivity   = 0.0020f;   // world units per pixel * distance
    float zoomSensitivity  = 0.9f;      // multiplicative zoom per wheel "tick"

    float minDistance = 2.0f;
    float maxDistance = 200.0f;
    float maxPitch = glm::radians(89.0f);

    // Motion (WASD + QE) state
    glm::vec3 velocity{0.0f};
    float accel = 35.0f;     // units/s^2
    float damping = 10.0f;   // 1/s
    float maxSpeed = 30.0f;  // units/s

    // Outputs
    glm::vec3 position{0.0f, 6.5f, 18.0f};

    glm::vec3 forward() const
    {
        // Forward from yaw/pitch (camera looks toward target)
        glm::vec3 f;
        f.x = std::cos(pitch) * std::sin(yaw);
        f.y = std::sin(pitch);
        f.z = std::cos(pitch) * std::cos(yaw);
        return glm::normalize(f);
    }

    glm::vec3 right() const
    {
        return glm::normalize(glm::cross(forward(), glm::vec3(0,1,0)));
    }

    glm::vec3 up() const
    {
        return glm::normalize(glm::cross(right(), forward()));
    }

    void applyOrbitDelta(float dx, float dy)
    {
        yaw   -= dx * orbitSensitivity;
        pitch -= dy * orbitSensitivity;
        pitch = glm::clamp(pitch, -maxPitch, maxPitch);
    }

    void applyPanDelta(float dx, float dy)
    {
        // Scale pan by distance for consistent feel
        float s = panSensitivity * distance;
        target += (-right() * dx + up() * dy) * s;
    }

    void applyZoom(float scrollY)
    {
        if (scrollY == 0.0f) return;
        // Exponential zoom (smooth, consistent across distances)
        float factor = std::pow(zoomSensitivity, scrollY);
        distance = glm::clamp(distance * factor, minDistance, maxDistance);
    }

    void updateMotion(GLFWwindow* win, float dt)
    {
        // Input direction in camera local frame
        glm::vec3 dir(0.0f);
        if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) dir += forward();
        if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) dir -= forward();
        if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) dir += right();
        if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) dir -= right();
        if (glfwGetKey(win, GLFW_KEY_E) == GLFW_PRESS) dir += glm::vec3(0,1,0);
        if (glfwGetKey(win, GLFW_KEY_Q) == GLFW_PRESS) dir -= glm::vec3(0,1,0);

        bool fast = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
                    (glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
        float accelMul = fast ? 2.5f : 1.0f;
        float maxSpMul = fast ? 2.5f : 1.0f;

        if (glm::dot(dir, dir) > 1e-6f)
            dir = glm::normalize(dir);

        // Accelerate target in direction
        velocity += dir * (accel * accelMul) * dt;

        // Damping (critically important so it doesn't drift forever)
        float damp = std::exp(-damping * dt);
        velocity *= damp;

        // Cap speed
        float sp = glm::length(velocity);
        float maxSp = maxSpeed * maxSpMul;
        if (sp > maxSp && sp > 1e-6f)
            velocity *= (maxSp / sp);

        // Move target (fly-style pan of orbit center)
        target += velocity * dt;
    }

    void rebuild()
    {
        // Orbit camera around target
        glm::vec3 f = forward();
        position = target - f * distance;
    }

    glm::mat4 viewMatrix() const
    {
        return glm::lookAt(position, target, glm::vec3(0,1,0));
    }
};

#endif