// Third Party Libs
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

// C++ Standard Template Library (STL)
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <cstdlib>

// Project Libraries
#include "Mesh.h"
#include "Camera.h"
#include "Shader.h"
#include "Lighting.h"
#include "Physics.h"

// --------------------- Globals ---------------------

// Screen Dimensions
int gScreenHeight = 960;
int gScreenWidth  = 1280;
SDL_Window*   gGraphisApplicationWindow = nullptr;
SDL_GLContext gOpenGLContext = nullptr;

// Main Loop Flag
bool gQuit = false;

GLuint gPlaneVAO, gPlaneVBO, gPlaneEBO;

// Object Object
Mesh      gObjectMesh;
float     gObjectRotation    = 0.0f;
glm::vec3 gObjectCenterOffset = glm::vec3(0.0f);
float gObjectBottomOffset = 0;
const float gObjectScale     = 0.15f;

int gSelectedBlob = -1;

// Shadow Map
GLuint    gShadowMapFBO     = 0;
GLuint    gShadowMapTexture = 0;
const int SHADOW_WIDTH      = 2048;
const int SHADOW_HEIGHT     = 2048;

// Delta Time
float gLastTime  = 0.0f;
float gDeltaTime = 0.0f;

// --------------------- Geometry Data ---------------------

const std::vector<GLfloat> planeVertexData {
//   X       Y      Z      NX    NY    NZ    U     V
    -20.0f, 0.0f, -20.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
     20.0f, 0.0f, -20.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
     20.0f, 0.0f,  20.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    -20.0f, 0.0f,  20.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
};
const std::vector<GLuint> planeIndices { 0,1,2, 2,3,0 };

// --------------------- Genetic Information ---------------------

struct Genome {
    float size;          // 0.0 - 1.0
    float speed;         // 0.0 - 1.0
    float sightRange;    // 0.0 - 1.0
    float energyEff;     // 0.0 - 1.0  (higher = more efficient)
};

struct Phenotype {
    float worldScale;    // actual size in world space
    float moveForce;     // actual force applied when moving
    float sightDist;     // actual sight distance
    float maxEnergy;     // energy pool
    float energyCost;    // energy drained per second while moving
    glm::vec3 color;     // visual color derived from traits
};

Phenotype MakePhenotype(const Genome& g, float baseScale, float baseRadius) {
    Phenotype p;

    // Size and speed are inversely linked — bigger = slower
    p.worldScale  = baseScale * (0.5f + g.size * 1.5f);       // 0.5x to 2x base size
    p.moveForce   = 1.0f + g.speed * 4.0f;                    // 1 to 5
    p.sightDist   = 2.0f + g.sightRange * 18.0f;              // 2 to 20 units
    p.maxEnergy   = 50.0f + g.energyEff * 150.0f;             // 50 to 200
    p.energyCost  = (g.speed * 2.0f) / (g.energyEff + 0.1f);  // fast + inefficient = hungry

    // Color: red channel driven by size, blue by speed, green stays low
    p.color = glm::vec3(
        0.2f + g.size  * 0.8f,   // red   0.2 -> 1.0
        0.1f,                     // green fixed low
        0.2f + g.speed * 0.8f    // blue  0.2 -> 1.0
    );

    return p;
}

// --------------------- Blobs ---------------------

struct BlobInstance {
    Genome    genome;
    Phenotype phenotype;
    RigidBody body;
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::vec3 wanderDir   = glm::vec3(0.0f);
    float     wanderTimer = 0.0f;
    float     energy      = 100.0f;
    bool      alive       = true;

    // Constructor to initialize RigidBody which has no default constructor
    BlobInstance(Genome g, Phenotype p, glm::vec3 startPos, float mass, float radius)
        : genome(g), phenotype(p), body(startPos, mass, radius), energy(p.maxEnergy) {}
};

std::vector<BlobInstance> gBlobs;
const int BLOB_COUNT = 50;

// --------------------- Helper Functions ---------------------

void GetOpenGLVersionInfo() {
    std::cout << "Vendor: "           << glGetString(GL_VENDOR)                   << std::endl;
    std::cout << "Renderer: "         << glGetString(GL_RENDERER)                 << std::endl;
    std::cout << "Version: "          << glGetString(GL_VERSION)                  << std::endl;
    std::cout << "Shading Language: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << std::endl;
}

void SetupMesh(GLuint& vao, GLuint& vbo, GLuint& ebo,
               const std::vector<GLfloat>& verts,
               const std::vector<GLuint>&  indices) {

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(GLfloat),
                 verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint),
                 indices.data(), GL_STATIC_DRAW);

    // Position (location = 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, (GLvoid*)0);

    // Normal (location = 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, (GLvoid*)(sizeof(GLfloat) * 3));

    // UV (location = 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                          sizeof(GLfloat) * 8, (GLvoid*)(sizeof(GLfloat) * 6));

    glBindVertexArray(0);
}

void SetupShadowMap() {
    glGenFramebuffers(1, &gShadowMapFBO);

    glGenTextures(1, &gShadowMapTexture);
    glBindTexture(GL_TEXTURE_2D, gShadowMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
                 SHADOW_WIDTH, SHADOW_HEIGHT, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, gShadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, gShadowMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    // Verify FBO is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cout << "ERROR: Shadow map framebuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Genome RandomGenome() {
    auto rf = []() { return (rand() % 100) / 100.0f; };
    return { rf(), rf(), rf(), rf() };
}

bool RayIntersectsSphere(glm::vec3 rayOrigin, glm::vec3 rayDir,
                          glm::vec3 sphereCenter, float radius) {
    glm::vec3 oc = rayOrigin - sphereCenter;
    float b = glm::dot(oc, rayDir);
    float c = glm::dot(oc, oc) - radius * radius;
    return (b * b - c) >= 0.0f;
}

glm::vec3 ScreenToRay(int mouseX, int mouseY) {
    // Normalize to NDC
    float x = (2.0f * mouseX) / gScreenWidth  - 1.0f;
    float y = 1.0f - (2.0f * mouseY) / gScreenHeight;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                      (float)gScreenWidth / gScreenHeight,
                                      0.1f, 100.0f);
    glm::mat4 view = GetViewMatrix();

    glm::vec4 rayClip  = glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 rayEye   = glm::inverse(proj) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    glm::vec3 rayWorld = glm::vec3(glm::inverse(view) * rayEye);
    return glm::normalize(rayWorld);
}

// --------------------- Core Functions ---------------------

void VertexSpecification() {
    SetupMesh(gPlaneVAO, gPlaneVBO, gPlaneEBO, planeVertexData, planeIndices);

    gObjectMesh.Load("models/Blob.obj");
    gObjectMesh.Setup();

    float     meshRadius  = gObjectMesh.GetBoundingRadius();
    float     worldRadius = meshRadius * gObjectScale;
    glm::vec3 center      = gObjectMesh.GetCenter();

    gObjectCenterOffset = center * gObjectScale;

    float meshBottom = FLT_MAX;
    for (const auto& v : gObjectMesh.vertices) {
        meshBottom = std::min(meshBottom, v.y);
    }
    gObjectBottomOffset = meshBottom; // ← raw, unscaled

    srand(42);
    for (int i = 0; i < BLOB_COUNT; i++) {
        float x = ((rand() % 200) - 100) * 0.1f;
        float z = ((rand() % 200) - 100) * 0.1f;
        float y = 5.0f + (rand() % 100) * 0.1f;

        Genome    g = RandomGenome();
        Phenotype p = MakePhenotype(g, gObjectScale, worldRadius);
        float scaledRadius = worldRadius * (0.5f + g.size * 1.5f);

        gBlobs.emplace_back(g, p, glm::vec3(x, y, z), 1.0f, scaledRadius);
    }

    SetupShadowMap();
}

void InitializeProgram() {

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cout << "SDL3 could not initialize video subsystem" << std::endl;
        exit(1);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    gGraphisApplicationWindow = SDL_CreateWindow("Final Project",
                                                  gScreenWidth, gScreenHeight,
                                                  SDL_WINDOW_OPENGL);
    if (gGraphisApplicationWindow == nullptr) {
        std::cout << "SDL_Window was not able to be created! SDL Error: "
                  << SDL_GetError() << std::endl;
        exit(1);
    }

    gOpenGLContext = SDL_GL_CreateContext(gGraphisApplicationWindow);
    if (gOpenGLContext == nullptr) {
        std::cout << "OpenGL context could not be created! SDL Error: "
                  << SDL_GetError() << std::endl;
        exit(1);
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cout << "GLAD was not initialized" << std::endl;
        exit(1);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(gGraphisApplicationWindow, gOpenGLContext);
    ImGui_ImplOpenGL3_Init("#version 410");

    SDL_SetWindowRelativeMouseMode(gGraphisApplicationWindow, true);
}

void Input() {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {

        ImGui_ImplSDL3_ProcessEvent(&e);

        if (e.type == SDL_EVENT_QUIT) {
            std::cout << "Goodbye!" << std::endl;
            gQuit = true;
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION) {
            MouseLook((float)e.motion.xrel, (float)e.motion.yrel);
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            SDL_SetWindowRelativeMouseMode(gGraphisApplicationWindow, false);

            float mouseX, mouseY;
            SDL_GetMouseState(&mouseX, &mouseY);

            SDL_SetWindowRelativeMouseMode(gGraphisApplicationWindow, true);

            glm::vec3 rayOrigin = gCameraPos;
            glm::vec3 rayDir    = ScreenToRay((int)mouseX, (int)mouseY);

            gSelectedBlob = -1; // deselect
            float closestDist = FLT_MAX;

            for (int i = 0; i < (int)gBlobs.size(); i++) {
                if (!gBlobs[i].alive) continue;

                glm::vec3 blobPos = gBlobs[i].body.position;
                float pickRadius = gBlobs[i].body.radius * 2.5f; // generous pick radius

                glm::vec3 oc = rayOrigin - blobPos;
                float b = glm::dot(oc, rayDir);
                float c = glm::dot(oc, oc) - pickRadius * pickRadius;
                float discriminant = b * b - c;

                if (discriminant >= 0.0f) {
                    float dist = -b - sqrt(discriminant);
                    if (dist < closestDist) {
                        closestDist   = dist;
                        gSelectedBlob = i;
                    }
                }
            }
        }

    }

    const bool* keys = SDL_GetKeyboardState(NULL);
    glm::vec3 right  = glm::normalize(glm::cross(gCameraFront, gCameraUp));

    if (keys[SDL_SCANCODE_W]) gCameraPos += gCameraSpeed * gCameraFront;
    if (keys[SDL_SCANCODE_S]) gCameraPos -= gCameraSpeed * gCameraFront;
    if (keys[SDL_SCANCODE_A]) gCameraPos -= gCameraSpeed * right;
    if (keys[SDL_SCANCODE_D]) gCameraPos += gCameraSpeed * right;

    if (keys[SDL_SCANCODE_SPACE])  gCameraPos += gCameraSpeed * gCameraUp;
    if (keys[SDL_SCANCODE_LSHIFT]) gCameraPos -= gCameraSpeed * gCameraUp;

    if (keys[SDL_SCANCODE_LCTRL]) {
        gCameraSpeed = 0.1f;
    } else {
        gCameraSpeed = 0.01f;
    }

    // Physics interactions
    if (keys[SDL_SCANCODE_F]) {
        for (auto& blob : gBlobs)
            ApplyForce(blob.body, glm::vec3(5.0f, 0.0f, 0.0f));
    }
    if (keys[SDL_SCANCODE_R]) {
        for (auto& blob : gBlobs)
            ApplyForce(blob.body, glm::vec3(-5.0f, 0.0f, -0.0f));
    }
    if (keys[SDL_SCANCODE_G]) {
        for (auto& blob : gBlobs) {
            if (blob.body.Grounded)
                blob.body.velocity.y = 8.0f;
        }
    }

    // Light movement
    if (keys[SDL_SCANCODE_UP])    gLightPos.z -= 0.05f;
    if (keys[SDL_SCANCODE_DOWN])  gLightPos.z += 0.05f;
    if (keys[SDL_SCANCODE_LEFT])  gLightPos.x -= 0.05f;
    if (keys[SDL_SCANCODE_RIGHT]) gLightPos.x += 0.05f;
    if (keys[SDL_SCANCODE_Q])     gLightPos.y += 0.05f;
    if (keys[SDL_SCANCODE_E])     gLightPos.y -= 0.05f;

    if (keys[SDL_SCANCODE_ESCAPE]) gQuit = true;
}

void PreDraw() {
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

}

void DrawMesh(GLuint vao, GLsizei indexCount,
              const glm::mat4& model, const glm::mat4& view, const glm::mat4& proj) {

    glUniformMatrix4fv(glGetUniformLocation(gShaderProgram, "uModel"),
                       1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(gShaderProgram, "uView"),
                       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(gShaderProgram, "uProjection"),
                       1, GL_FALSE, glm::value_ptr(proj));

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
}

void UpdateAI(float dt) {
    for (auto& blob : gBlobs) {
        if (!blob.alive) continue;

        // Drain energy while moving
        glm::vec3 hVel = glm::vec3(blob.body.velocity.x, 0.0f, blob.body.velocity.z);
        if (glm::length(hVel) > 0.1f)
            blob.energy -= blob.phenotype.energyCost * dt;

        // Die if out of energy
        if (blob.energy <= 0.0f) {
            blob.alive = false;
            continue;
        }

        blob.wanderTimer -= dt;
        if (blob.wanderTimer <= 0.0f) {
            float angle      = (rand() % 360) * (3.14159f / 180.0f);
            blob.wanderDir   = glm::vec3(cos(angle), 0.0f, sin(angle));
            blob.wanderTimer = 2.0f + (rand() % 200) * 0.01f;
        }

        if (blob.body.Grounded) {
            // Use phenotype speed for force
            ApplyForce(blob.body, blob.wanderDir * blob.phenotype.moveForce);

            if (rand() % 100 < 2)
                blob.body.velocity.y = 5.0f;
        }

        // Boundary push
        float dist = glm::length(glm::vec2(blob.body.position.x, blob.body.position.z));
        if (dist > 15.0f) {
            glm::vec3 toCenter = glm::normalize(glm::vec3(-blob.body.position.x, 0.0f, -blob.body.position.z));
            ApplyForce(blob.body, toCenter * 3.0f);
            blob.wanderDir = toCenter;
        }
    }
}

void DrawBlobUI() {
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Always-visible population stats in corner
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(220, 80), ImGuiCond_Always);
    ImGui::Begin("Population", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    int alive = 0;
    for (auto& b : gBlobs) if (b.alive) alive++;
    ImGui::Text("Alive: %d / %d", alive, BLOB_COUNT);
    ImGui::Text("Light: (%.1f, %.1f, %.1f)",
                gLightPos.x, gLightPos.y, gLightPos.z);
    ImGui::End();

    // Selected blob stats panel
    if (gSelectedBlob != -1 && gBlobs[gSelectedBlob].alive) {
        const BlobInstance& blob = gBlobs[gSelectedBlob];
        const Genome&       g    = blob.genome;
        const Phenotype&    p    = blob.phenotype;

        ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(280, 280), ImGuiCond_Always);
        ImGui::Begin("Selected Blob", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        ImGui::Text("Blob #%d", gSelectedBlob);
        ImGui::Separator();

        // Color swatch
        ImGui::ColorButton("Color##blob",
            ImVec4(p.color.r, p.color.g, p.color.b, 1.0f),
            0, ImVec2(260, 20));

        ImGui::Separator();
        ImGui::Text("--- Genome ---");
        ImGui::SliderFloat("Size",       (float*)&g.size,       0.0f, 1.0f);
        ImGui::SliderFloat("Speed",      (float*)&g.speed,      0.0f, 1.0f);
        ImGui::SliderFloat("Sight",      (float*)&g.sightRange, 0.0f, 1.0f);
        ImGui::SliderFloat("Energy Eff", (float*)&g.energyEff,  0.0f, 1.0f);

        ImGui::Separator();
        ImGui::Text("--- Phenotype ---");
        ImGui::Text("World Scale:  %.3f", p.worldScale);
        ImGui::Text("Move Force:   %.3f", p.moveForce);
        ImGui::Text("Sight Dist:   %.3f", p.sightDist);
        ImGui::Text("Energy Cost:  %.3f", p.energyCost);

        ImGui::Separator();
        ImGui::Text("--- Runtime ---");
        float energyPct = blob.energy / p.maxEnergy;
        ImGui::ProgressBar(energyPct, ImVec2(-1, 0), "Energy");
        ImGui::Text("%.1f / %.1f", blob.energy, p.maxEnergy);
        ImGui::Text("Grounded: %s", blob.body.Grounded ? "Yes" : "No");
        ImGui::Text("Velocity: (%.2f, %.2f, %.2f)",
                    blob.body.velocity.x,
                    blob.body.velocity.y,
                    blob.body.velocity.z);

        ImGui::End();
    } else {
        ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(280, 40), ImGuiCond_Always);
        ImGui::Begin("Selected Blob", nullptr,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::Text("Click a blob to inspect it");
        ImGui::End();
    }

    // Render
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Draw() {

    UpdateAI(gDeltaTime);

    // Update physics and build matrices for all blobs
    for (auto& blob : gBlobs) {
        if (!blob.alive) continue;

        UpdatePhysics(blob.body, gDeltaTime);

        blob.modelMatrix = glm::mat4(1.0f);
        blob.modelMatrix = glm::translate(blob.modelMatrix,
            glm::vec3(blob.body.position.x,
                    blob.body.position.y - blob.body.radius - (gObjectBottomOffset * blob.phenotype.worldScale),
                    blob.body.position.z));

        // Rotate to face movement direction
        glm::vec3 horizontalVel = glm::vec3(blob.body.velocity.x, 0.0f, blob.body.velocity.z);
        if (glm::length(horizontalVel) > 0.1f) {
            glm::vec3 forward = glm::normalize(horizontalVel);
            float angle = atan2(forward.x, forward.z);
            blob.modelMatrix = glm::rotate(blob.modelMatrix, angle, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        // Use phenotype scale instead of gObjectScale
        blob.modelMatrix = glm::scale(blob.modelMatrix, glm::vec3(blob.phenotype.worldScale));
    }

    // Build view/proj matrices
    glm::mat4 view = GetViewMatrix();
    glm::mat4 proj = glm::perspective(glm::radians(45.0f),
                                      (float)gScreenWidth / gScreenHeight,
                                      0.1f, 100.0f);

    glm::mat4 planeModel = glm::mat4(1.0f);

    // Light space matrix
    glm::mat4 lightProj  = glm::ortho(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 200.0f);
    glm::mat4 lightView  = glm::lookAt(gLightPos,
                                       glm::vec3(0.0f, 0.0f, 0.0f),
                                       glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightSpace = lightProj * lightView;

    // ----- PASS 1: Shadow Map -----
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, gShadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    glUseProgram(gShadowShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(gShadowShaderProgram, "uLightSpaceMatrix"),
                       1, GL_FALSE, glm::value_ptr(lightSpace));

    // Plane shadow
    glUniformMatrix4fv(glGetUniformLocation(gShadowShaderProgram, "uModel"),
                       1, GL_FALSE, glm::value_ptr(planeModel));
    glBindVertexArray(gPlaneVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // Blob shadows
    for (auto& blob : gBlobs) {
        if (!blob.alive) continue;
        glUniformMatrix4fv(glGetUniformLocation(gShadowShaderProgram, "uModel"),
                           1, GL_FALSE, glm::value_ptr(blob.modelMatrix));
        glBindVertexArray(gObjectMesh.VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)gObjectMesh.indices.size(), GL_UNSIGNED_INT, 0);
    }

    glCullFace(GL_BACK);
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ----- PASS 2: Normal Render -----
    glViewport(0, 0, gScreenWidth, gScreenHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(gShaderProgram);
    SetLightUniforms(gShaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(gShaderProgram, "uLightSpaceMatrix"),
                       1, GL_FALSE, glm::value_ptr(lightSpace));

    // Bind shadow map to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gShadowMapTexture);
    glUniform1i(glGetUniformLocation(gShaderProgram, "uShadowMap"), 1);

    // Draw plane
    glUniform3f(glGetUniformLocation(gShaderProgram, "uObjectColor"), 0.4f, 0.7f, 0.4f);
    DrawMesh(gPlaneVAO, 6, planeModel, view, proj);

    // Draw blobs with per-blob phenotype color
    for (auto& blob : gBlobs) {
        if (!blob.alive) continue;
        glUniform3f(glGetUniformLocation(gShaderProgram, "uObjectColor"),
                    blob.phenotype.color.r,
                    blob.phenotype.color.g,
                    blob.phenotype.color.b);
        DrawMesh(gObjectMesh.VAO, (GLsizei)gObjectMesh.indices.size(),
                 blob.modelMatrix, view, proj);
    }

    glUseProgram(0);
}

void MainLoop() {
    while (!gQuit) {
        float currentTime = (float)SDL_GetTicks() / 1000.0f;
        gDeltaTime = currentTime - gLastTime;
        gLastTime  = currentTime;
        if (gDeltaTime > 0.05f) gDeltaTime = 0.05f;

        Input();
        PreDraw();
        Draw();
        DrawBlobUI();
        SDL_GL_SwapWindow(gGraphisApplicationWindow);
    }
}

void CleanUp() {
    glDeleteVertexArrays(1, &gPlaneVAO);
    glDeleteBuffers(1, &gPlaneVBO);
    glDeleteBuffers(1, &gPlaneEBO);

    glDeleteVertexArrays(1, &gObjectMesh.VAO);
    glDeleteBuffers(1, &gObjectMesh.VBO);
    glDeleteBuffers(1, &gObjectMesh.EBO);

    // Clean up shadow map resources
    glDeleteFramebuffers(1, &gShadowMapFBO);      
    glDeleteTextures(1, &gShadowMapTexture);        

    glDeleteProgram(gShaderProgram);
    glDeleteProgram(gShadowShaderProgram);         

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyWindow(gGraphisApplicationWindow);
    SDL_Quit();
}

int main(int argc, char* args[]) {
    InitializeProgram();
    VertexSpecification();
    CreateGraphicsPipeline();
    MainLoop();
    CleanUp();
    return 0;
}