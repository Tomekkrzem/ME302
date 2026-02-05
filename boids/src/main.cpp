#include "boids.h"
#include "shaders.h"
#include "mesh.h"
#include "camera.h"
#include "ui.h"
#include "text_renderer.h"

#include <random>
#include <chrono>
#include <iostream>
#include <algorithm>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

// --------------------------- Camera callbacks ---------------------------
// Global pointer so GLFW callbacks can control the camera
static CameraController* gCam = nullptr;

static void scroll_callback(GLFWwindow*, double /*xoff*/, double yoff)
{
    if (!gCam) return;               // no camera yet
    if (mode != Mode::Game) return;  // disable zoom while in menu
    gCam->applyZoom((float)yoff);    // zoom in/out
}

static void mouse_button_callback(GLFWwindow* win, int button, int action, int /*mods*/)
{
    // MENU: left click to start
    if (mode == Mode::Menu)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
        {
            glm::vec2 m = mousePixels(win);  // mouse in pixels
            if (inside(m, startBtn))         // clicked the start button
                mode = Mode::Game;           // switch to game
        }
        return; // don't orbit/pan while in menu
    }

    // GAME camera controls
    if (!gCam) return;

    if (action == GLFW_PRESS)
    {
        // record mouse position for drag deltas
        double x, y;
        glfwGetCursorPos(win, &x, &y);
        gCam->lastX = x; gCam->lastY = y;

        // check if shift is held (changes right-drag behavior)
        bool shift = (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
                     (glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);

        // right drag = orbit
        if (button == GLFW_MOUSE_BUTTON_RIGHT && !shift)
            gCam->draggingOrbit = true;

        // middle drag (or shift+right) = pan
        if (button == GLFW_MOUSE_BUTTON_MIDDLE || (button == GLFW_MOUSE_BUTTON_RIGHT && shift))
            gCam->draggingPan = true;
    }
    else if (action == GLFW_RELEASE)
    {
        // stop orbit on right release
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
            gCam->draggingOrbit = false;

        // stop pan on middle/right release
        if (button == GLFW_MOUSE_BUTTON_MIDDLE || button == GLFW_MOUSE_BUTTON_RIGHT)
            gCam->draggingPan = false;
    }
}

static void cursor_pos_callback(GLFWwindow* win, double x, double y)
{
    if (mode != Mode::Game) return; // disable camera motion in menu
    if (!gCam) return;

    // compute mouse movement since last frame
    float dx = (float)(x - gCam->lastX);
    float dy = (float)(y - gCam->lastY);
    gCam->lastX = x; gCam->lastY = y;

    // apply orbit/pan based on which drag is active
    if (gCam->draggingOrbit)
        gCam->applyOrbitDelta(dx, dy);

    if (gCam->draggingPan)
        gCam->applyPanDelta(dx, dy);
}

// ------------------------------------------------
int main()
{
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    // Request OpenGL 3.3 core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window + context
    GLFWwindow* win = glfwCreateWindow(1280, 720, "Boids", nullptr, nullptr);
    if (!win)
    {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1); // vsync

    // ---- GLAD init ----
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to init GLAD\n";
        glfwTerminate();
        return 1;
    }

    // Basic GL state
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Load shader + mesh
    Shader myShader("resources/shader.vs", "resources/shader.fs");
    Mesh pyramid = createPyramid();

    // Simple UI quad geometry (two triangles)
    GLuint uiVAO = 0, uiVBO = 0;
    {
        float verts[] = {
            0,0,0,  1,0,0,  1,1,0,
            0,0,0,  1,1,0,  0,1,0
        };

        glGenVertexArrays(1, &uiVAO);
        glGenBuffers(1, &uiVBO);

        glBindVertexArray(uiVAO);
        glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glBindVertexArray(0);
    }

    // Boid setup + bounds
    const int BOID_COUNT = 500;
    glm::vec3 boundsMin(-25.0f, -25.0f, -25.0f);
    glm::vec3 boundsMax( 25.0f,  25.0f,  25.0f);

    // RNG for initial positions/directions
    std::mt19937 rng(1337u);
    std::uniform_real_distribution<float> px(boundsMin.x, boundsMax.x);
    std::uniform_real_distribution<float> py(boundsMin.y, boundsMax.y);
    std::uniform_real_distribution<float> pz(boundsMin.z, boundsMax.z);

    std::vector<Boid> boids;
    BoidParams params;
    boids.resize(BOID_COUNT);

    // Initialize each boid with random pos + random direction
    for (auto& b : boids)
    {
        b.pos = glm::vec3(px(rng), py(rng), pz(rng));

        // random unit vector using spherical coordinates
        std::uniform_real_distribution<float> u01(0.0f, 1.0f);
        float z = u01(rng) * 2.0f - 1.0f;
        float a = u01(rng) * glm::two_pi<float>();
        float r = std::sqrt(1.0f - z * z);

        glm::vec3 dir(r * std::cos(a), z, r * std::sin(a));

        // boid movement limits
        b.maxSpeed  = 10.0f;
        b.maxForce  = 12.0f;
        b.turnSpeed = 12.0f;

        b.vel = dir * b.maxSpeed;
        updateOrientationVelocity(b); // make model face velocity
    }

    // Camera initialization
    CameraController cam;
    cam.target   = glm::vec3(0.0f, 0.0f, 0.0f);
    cam.distance = 18.0f;
    cam.pitch    = glm::radians(-15.0f);
    cam.yaw      = glm::radians(0.0f);
    cam.rebuild();

    // Hook up GLFW callbacks
    gCam = &cam;
    glfwSetScrollCallback(win, scroll_callback);
    glfwSetMouseButtonCallback(win, mouse_button_callback);
    glfwSetCursorPosCallback(win, cursor_pos_callback);

    // Timing
    float lastT = (float)glfwGetTime();

    // Menu text setup
    TextRenderer menuText;
    menuText.fontPixelHeight = 72;
    menuText.init("assets/PressStart2P-Regular.ttf", 72);

    // Main loop
    while (!glfwWindowShouldClose(win))
    {
        glfwPollEvents();

        // Escape quits
        if (glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(win, GLFW_TRUE);

        // Frame time step (clamped)
        float nowT = (float)glfwGetTime();
        float dt = nowT - lastT;
        lastT = nowT;
        dt = glm::clamp(dt, 0.0f, 1.0f / 60.0f);

        // Resize viewport
        int w=0,h=0;
        glfwGetFramebufferSize(win,&w,&h);
        w = std::max(1,w); h = std::max(1,h);
        glViewport(0,0,w,h);

        // Clear frame
        glClearColor(0.01f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Only move camera in game mode
        if (mode == Mode::Game) {
            cam.updateMotion(win, dt);
        }
        cam.rebuild();

        // Moving target to steer boids toward
        float t = (float)glfwGetTime();
        float a = 50.0f;
        float s = std::sin(t * 0.35f);
        float c = std::cos(t * 0.35f);
        glm::vec3 targetPos( a*std::sin(t*1.5f), a*std::sin(t*1.5f), a*std::sin(t*1.5f));

        // Update flock simulation
        updateBoids(boids, params, dt, boundsMin, boundsMax, targetPos);

        // Build camera matrices
        glm::mat4 P  = glm::perspective(glm::radians(60.0f), (float)w/(float)h, 0.1f, 400.0f);
        glm::mat4 V  = cam.viewMatrix();
        glm::mat4 VP = P * V;

        // Draw boids
        glEnable(GL_DEPTH_TEST);
        glUseProgram(myShader.ID);
        glUniform1i(glGetUniformLocation(myShader.ID, "uUseSolidColor"), 0);
        glUniformMatrix4fv(glGetUniformLocation(myShader.ID, "uViewProj"), 1, GL_FALSE, &VP[0][0]);

        for (const Boid& b : boids) {
            glm::mat4 M = boidModelMatrix(b, 0.35f); // scale + orient boid mesh
            glUniformMatrix4fv(glGetUniformLocation(myShader.ID, "uModel"), 1, GL_FALSE, &M[0][0]);
            pyramid.draw();
        }

        // Draw menu overlay
        if (mode == Mode::Menu)
        {
            // Position/size the start button
            startBtn.w = 260.0f;
            startBtn.h = 80.0f;
            startBtn.x = (w - startBtn.w) * 0.5f;
            startBtn.y = (h * 0.45f) - (startBtn.h * 0.5f);
            startBtn.hovered = inside(mousePixels(win), startBtn);

            // UI projection in pixel space
            glm::mat4 VPui = glm::ortho(0.0f, (float)w, 0.0f, (float)h, -1.0f, 1.0f);

            // UI render state
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Dark background tint
            drawRectPx(myShader, VPui, 0, 0, (float)w, (float)h,
                    glm::vec4(0.02f, 0.05f, 0.08f, 0.8f), uiVAO);

            // Button border
            drawRectPx(myShader, VPui,
                    startBtn.x - 3, startBtn.y - 3,
                    startBtn.w + 6, startBtn.h + 6,
                    glm::vec4(0.35f, 0.70f, 0.90f, 1.0f), uiVAO);

            // Button fill changes on hover
            glm::vec4 fill = startBtn.hovered
                ? glm::vec4(0.10f, 0.45f, 0.60f, 0.90f)
                : glm::vec4(0.08f, 0.30f, 0.40f, 0.90f);

            drawRectPx(myShader, VPui, startBtn.x, startBtn.y, startBtn.w, startBtn.h, fill, uiVAO);

            // Draw text labels
            glBindVertexArray(0);
            glUseProgram(0);
            menuText.begin((float)w, (float)h);

            menuText.drawCentered("BOIDS", (float)w * 0.5f, (float)h * 0.72f, 1.75f,
                                {0.85f, 0.92f, 0.95f});

            menuText.drawCentered("Start",
                startBtn.x + startBtn.w * 0.515f,
                startBtn.y + startBtn.h * 0.5f - 18.0f, 0.72f,
                {0.85f, 0.92f, 0.95f});

            menuText.drawCentered("Click Start to Begin",
                (float)w * 0.5f, (float)h * 0.20f, 0.25f,
                {0.85f, 0.92f, 0.95f});
            
            menuText.drawCentered("Tomasz Krzeminski",
                (float)w * 0.5f, (float)h * 0.10f, 0.25f,
                {0.85f, 0.92f, 0.95f});

            // Restore depth writes
            glDepthMask(GL_TRUE);
        }

        glfwSwapBuffers(win);
    }

    // Cleanup
    pyramid.destroy();
    glDeleteProgram(myShader.ID);
    menuText.shutdown();
    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &uiVBO);

    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
