// Third Party Libs
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
#include "AI.h"
#include "UI.h"

// --------------------- Globals ---------------------

// Screen Dimensions
int gScreenHeight = 1080;
int gScreenWidth  = 1920;
SDL_Window*   gGraphisApplicationWindow = nullptr;
SDL_GLContext gOpenGLContext = nullptr;

// Main Loop Flag
bool gQuit = false;

// Plane Globals
GLuint gPlaneVAO, gPlaneVBO, gPlaneEBO;

// Object
Mesh      gObjectMesh;
float     gObjectRotation     = 0.0f;
glm::vec3 gObjectCenterOffset = glm::vec3(0.0f);
float     gObjectBottomOffset = 0.0f;
float gObjectScale      = 0.15f;

// Food
Mesh gFoodMesh;

// Shadow Map
GLuint    gShadowMapFBO     = 0;
GLuint    gShadowMapTexture = 0;
const int SHADOW_WIDTH      = 2048;
const int SHADOW_HEIGHT     = 2048;

// Delta Time
float gLastTime  = 0.0f;
float gDeltaTime = 0.0f;

// Blob Globals
std::vector<BlobInstance> gBlobs;
const int   BLOB_COUNT         = 400;
int         gSelectedBlob      = -1;
const float SCREEN_PICK_RADIUS = 50.0f; // pixels

// View and Projection Globals
glm::mat4 gView = glm::mat4(1.0f);
glm::mat4 gProj = glm::mat4(1.0f);

// Simulation Time Scale
float gTimeScale = 1.0f;

// Title Screen Condition
bool gShowTitleScreen = true;

// Debug Mode Condition
bool gDebugMode = false;

// Mouse Conditions for Pan and Drag
bool gMiddleMouseHeld = false;
bool gCtrlMiddleMouseHeld = false;

// Follow Blob
bool gFollowSelectedBlob = false;

float gP_Size = 50.0f;

// --------------------- Geometry Data ---------------------

const std::vector<GLfloat> planeVertexData {
//   X       Y      Z      NX    NY    NZ    U     V
    -gP_Size, 0.0f, -gP_Size, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
     gP_Size, 0.0f, -gP_Size, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
     gP_Size, 0.0f,  gP_Size, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    -gP_Size, 0.0f,  gP_Size, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
};
const std::vector<GLuint> planeIndices { 0,1,2, 2,3,0 };

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

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "ERROR: Shadow map framebuffer is not complete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// --------------------- Core Functions ---------------------

void VertexSpecification() {
    SetupMesh(gPlaneVAO, gPlaneVBO, gPlaneEBO, planeVertexData, planeIndices);

    gObjectMesh.Load("models/Blob.obj");
    gObjectMesh.Setup();

    gFoodMesh.Load("models/Food.obj");
    gFoodMesh.Setup();

    float     meshRadius  = gObjectMesh.GetBoundingRadius();
    float     worldRadius = meshRadius * gObjectScale;
    glm::vec3 center      = gObjectMesh.GetCenter();

    gObjectCenterOffset = center * gObjectScale;

    float meshBottom = FLT_MAX;
    for (const auto& v : gObjectMesh.vertices)
        meshBottom = std::min(meshBottom, v.y);
    gObjectBottomOffset = meshBottom; // raw, unscaled

    srand(42);
    for (int i = 0; i < BLOB_COUNT; i++) {
        float x = ((rand() % int(gP_Size * 20)) - gP_Size * 10) * 0.1f;
        float z = ((rand() % int(gP_Size * 20)) - gP_Size * 10) * 0.1f;
        float y = 5.0f + (rand() % 100) * 0.1f;

        Genome    g = RandomGenome();
        Phenotype p = MakePhenotype(g, gObjectScale, worldRadius);
        float scaledRadius = worldRadius * (0.5f + g.size * 1.5f);

        gBlobs.emplace_back(g, p, glm::vec3(x, y, z), 1.0f, scaledRadius);
    }

    SpawnFood(gP_Size);

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

    gGraphisApplicationWindow = SDL_CreateWindow(
        "Evolution Simulator",
        gScreenWidth,
        gScreenHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!gGraphisApplicationWindow) {
        std::cout << "SDL_Window was not able to be created! SDL Error: "
                  << SDL_GetError() << std::endl;
        exit(1);
    }

    gOpenGLContext = SDL_GL_CreateContext(gGraphisApplicationWindow);
    if (!gOpenGLContext) {
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

    SDL_SetWindowRelativeMouseMode(gGraphisApplicationWindow, false);
    SDL_ShowCursor();

    UpdateOrbitCamera();
}

void Input() {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {

        ImGui_ImplSDL3_ProcessEvent(&e);
        
        if (e.type == SDL_EVENT_QUIT) {
            std::cout << "Goodbye!" << std::endl;
            gQuit = true;
        }

        if (e.type == SDL_EVENT_WINDOW_RESIZED) {
            gScreenWidth  = e.window.data1;
            gScreenHeight = e.window.data2;

            glViewport(0, 0, gScreenWidth, gScreenHeight);
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            e.button.button == SDL_BUTTON_MIDDLE) {
            gMiddleMouseHeld = true;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP &&
            e.button.button == SDL_BUTTON_MIDDLE) {
            gMiddleMouseHeld = false;
        }

        if (e.type == SDL_EVENT_MOUSE_MOTION &&
            gMiddleMouseHeld &&
            !gShowTitleScreen &&
            !ImGui::GetIO().WantCaptureMouse) {

            const bool* keys = SDL_GetKeyboardState(NULL);
            bool ctrlHeld = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];

            if (ctrlHeld) {
                // Ctrl + MMB = pan
                PanCamera((float)e.motion.xrel, (float)e.motion.yrel);
            } else {
                // MMB = orbit
                OrbitCamera(-(float)e.motion.xrel, (float)e.motion.yrel);
            }
        }

        if (e.type == SDL_EVENT_MOUSE_WHEEL &&
            !gShowTitleScreen &&
            !ImGui::GetIO().WantCaptureMouse) {
            ZoomCamera((float)e.wheel.y);
        }

        if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_D) {
            gDebugMode = !gDebugMode;
        }

        if (e.type == SDL_EVENT_KEY_DOWN && e.key.scancode == SDL_SCANCODE_F) {
            if (gSelectedBlob != -1 && gBlobs[gSelectedBlob].alive) {
                gFollowSelectedBlob = !gFollowSelectedBlob;
            }
        }

        if (!gShowTitleScreen &&
            e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            e.button.button == SDL_BUTTON_LEFT &&
            !ImGui::GetIO().WantCaptureMouse) {

            int mouseX = e.button.x;
            int mouseY = e.button.y;

            gSelectedBlob = -1;
            float closestScreenDist = FLT_MAX;

            for (int i = 0; i < (int)gBlobs.size(); i++) {
                if (!gBlobs[i].alive) continue;

                glm::vec3 visualPos = gBlobs[i].body.position;

                glm::vec4 clip = gProj * gView * glm::vec4(visualPos, 1.0f);
                if (clip.w <= 0.0f) continue;

                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                float screenX = (ndc.x + 1.0f) * 0.5f * gScreenWidth;
                float screenY = (1.0f - ndc.y) * 0.5f * gScreenHeight;

                float dx = screenX - mouseX;
                float dy = screenY - mouseY;
                float screenDist = sqrt(dx * dx + dy * dy);

                if (screenDist < SCREEN_PICK_RADIUS && screenDist < closestScreenDist) {
                    closestScreenDist = screenDist;
                    gSelectedBlob = i;
                }
            }

            if (gSelectedBlob != -1) {
                gCameraTarget = gBlobs[gSelectedBlob].body.position;
                UpdateOrbitCamera();
            }
        }
    }

    if (gShowTitleScreen) return;

    const bool* keys = SDL_GetKeyboardState(NULL);

    if (keys[SDL_SCANCODE_ESCAPE]) gQuit = true;

    if (keys[SDL_SCANCODE_1]) gTimeScale = 1.0f;
    if (keys[SDL_SCANCODE_2]) gTimeScale = 2.0f;
    if (keys[SDL_SCANCODE_3]) gTimeScale = 3.0f;
    if (keys[SDL_SCANCODE_4]) gTimeScale = 4.0f;
    if (keys[SDL_SCANCODE_5]) gTimeScale = 5.0f;
    if (keys[SDL_SCANCODE_6]) gTimeScale = 6.0f;
    if (keys[SDL_SCANCODE_7]) gTimeScale = 7.0f;
    if (keys[SDL_SCANCODE_8]) gTimeScale = 8.0f;
    if (keys[SDL_SCANCODE_9]) gTimeScale = 9.0f;
    if (keys[SDL_SCANCODE_0]) gTimeScale = 20.0f;
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

void Draw() {

    UpdateAI(gDeltaTime, gBlobs);

    UpdateFood(gDeltaTime);

    for (auto& blob : gBlobs) {
        if (!blob.alive) continue;

        UpdatePhysics(blob.body, gDeltaTime);

        blob.modelMatrix = glm::mat4(1.0f);
        blob.modelMatrix = glm::translate(blob.modelMatrix,
            glm::vec3(blob.body.position.x,
                      blob.body.position.y - blob.body.radius - (gObjectBottomOffset * blob.phenotype.worldScale),
                      blob.body.position.z));

        glm::vec3 horizontalVel = glm::vec3(blob.body.velocity.x, 0.0f, blob.body.velocity.z);
        if (glm::length(horizontalVel) > 0.1f) {
            glm::vec3 forward = glm::normalize(horizontalVel);
            float angle = atan2(forward.x, forward.z) - glm::radians(90.0f);
            blob.modelMatrix = glm::rotate(blob.modelMatrix, angle, glm::vec3(0.0f, 1.0f, 0.0f));
        }

        blob.modelMatrix = glm::scale(blob.modelMatrix, glm::vec3(blob.phenotype.worldScale));
    }

    for (int i = 0; i < (int)gBlobs.size(); i++) {
        if (!gBlobs[i].alive) continue;
        for (int j = i + 1; j < (int)gBlobs.size(); j++) {
            if (!gBlobs[j].alive) continue;
            ResolveSphereCollision(gBlobs[i].body, gBlobs[j].body);
        }
    }

    glm::mat4 planeModel = glm::mat4(1.0f);

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

    glUniformMatrix4fv(glGetUniformLocation(gShadowShaderProgram, "uModel"),
                       1, GL_FALSE, glm::value_ptr(planeModel));
    glBindVertexArray(gPlaneVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    
    for (auto& blob : gBlobs) {
        if (!blob.alive) continue;
        glUniformMatrix4fv(glGetUniformLocation(gShadowShaderProgram, "uModel"),
                           1, GL_FALSE, glm::value_ptr(blob.modelMatrix));
        glBindVertexArray(gObjectMesh.VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)gObjectMesh.indices.size(), GL_UNSIGNED_INT, 0);
    }

    for (auto& food : gFood) {
        if (!food.active) continue;

        glm::mat4 foodModel = glm::mat4(1.0f);
        foodModel = glm::translate(foodModel, food.position);
        foodModel = glm::scale(foodModel, glm::vec3(0.1f)); // adjust scale to taste

        glUniformMatrix4fv(glGetUniformLocation(gShadowShaderProgram, "uModel"),
                        1, GL_FALSE, glm::value_ptr(foodModel));
        glBindVertexArray(gFoodMesh.VAO);
        glDrawElements(GL_TRIANGLES, (GLsizei)gFoodMesh.indices.size(), GL_UNSIGNED_INT, 0);
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

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gShadowMapTexture);
    glUniform1i(glGetUniformLocation(gShaderProgram, "uShadowMap"), 1);

    glUniform3f(glGetUniformLocation(gShaderProgram, "uObjectColor"), 0.4f, 0.7f, 0.4f);
    DrawMesh(gPlaneVAO, 6, planeModel, gView, gProj);

    for (auto& blob : gBlobs) {
        if (!blob.alive) continue;

        // Tick down the flash timer
        if (blob.hitFlashTimer > 0.0f)
            blob.hitFlashTimer -= gDeltaTime;

        // Use flash color if active, otherwise normal phenotype color
        glm::vec3 renderColor = (blob.hitFlashTimer > 0.0f)
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : blob.phenotype.color;

        glUniform3f(glGetUniformLocation(gShaderProgram, "uObjectColor"),
                    renderColor.r, renderColor.g, renderColor.b);

        DrawMesh(gObjectMesh.VAO, (GLsizei)gObjectMesh.indices.size(),
                blob.modelMatrix, gView, gProj);
    }

    glUniform3f(glGetUniformLocation(gShaderProgram, "uObjectColor"), 0.0f, 1.0f, 0.3f);
    for (auto& food : gFood) {
        if (!food.active) continue;

        glm::mat4 foodModel = glm::mat4(1.0f);
        foodModel = glm::translate(foodModel, food.position);
        foodModel = glm::scale(foodModel, glm::vec3(0.05f)); // adjust scale to taste

        DrawMesh(gFoodMesh.VAO, (GLsizei)gFoodMesh.indices.size(),
                foodModel, gView, gProj);
    }

    glUseProgram(0);
}

void MainLoop() {

    while (!gQuit) {
        
        float currentTime = (float)SDL_GetTicks() / 1000.0f;
        gDeltaTime = currentTime - gLastTime;
        gLastTime  = currentTime;
        if (gDeltaTime > 0.05f) gDeltaTime = 0.05f;

        gDeltaTime *= gTimeScale;

        gView = GetViewMatrix();
        gProj = glm::perspective(glm::radians(45.0f),
                                 (float)gScreenWidth / gScreenHeight,
                                 0.1f, 500.0f);

        Input();
        PreDraw();

        if (!gShowTitleScreen) {
            Draw();

            gGeneration.timer += gDeltaTime;
            if (gGeneration.timer >= gGeneration.duration) {
                float meshRadius  = gObjectMesh.GetBoundingRadius();
                float worldRadius = meshRadius * gObjectScale;
                EvolveGeneration(gBlobs, BLOB_COUNT, worldRadius, gObjectScale);
                gSelectedBlob = -1;
            }
        } else {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        if (gFollowSelectedBlob) {
            if (gSelectedBlob != -1 && gSelectedBlob < (int)gBlobs.size() && gBlobs[gSelectedBlob].alive) {
                glm::vec3 desiredTarget = gBlobs[gSelectedBlob].body.position;
                gCameraTarget = glm::mix(gCameraTarget, desiredTarget, 0.08f);
                UpdateOrbitCamera();
            } else {
                gFollowSelectedBlob = false;
            }
        }

        DrawBlobUI(gBlobs, gSelectedBlob, BLOB_COUNT,
                   gScreenWidth, gScreenHeight,
                   gView, gProj, gObjectBottomOffset);
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

    glDeleteFramebuffers(1, &gShadowMapFBO);
    glDeleteTextures(1, &gShadowMapTexture);

    glDeleteProgram(gShaderProgram);
    glDeleteProgram(gShadowShaderProgram);

    ShutdownUIPreviewResources();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyWindow(gGraphisApplicationWindow);
    SDL_Quit();
}

int main(int argc, char* args[]) {
    std::cout << "Starting..." << std::endl;
    InitializeProgram();
    std::cout << "Program initialized" << std::endl;
    VertexSpecification();
    std::cout << "Vertices specified" << std::endl;
    CreateGraphicsPipeline();
    std::cout << "Pipeline created" << std::endl;
    MainLoop();
    CleanUp();
    return 0;
}