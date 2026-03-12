#include "UI.h"
#include "AI.h"
#include "Mesh.h"
#include "Lighting.h"
#include "Shader.h"
#include <cstdio>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

extern float gTimeScale;
extern bool  gShowTitleScreen;
extern bool  gDebugMode;
extern Mesh  gObjectMesh;
extern GLuint gShaderProgram;
extern GLuint gShadowMapTexture;
extern float gObjectScale;

static GLuint gBestBlobPreviewFBO       = 0;
static GLuint gBestBlobPreviewColorTex  = 0;
static GLuint gBestBlobPreviewDepthRBO  = 0;
static const int BEST_BLOB_PREVIEW_SIZE = 512;

// --------------------- Helpers ---------------------

static float UIScale(int sWidth, int sHeight) {
    return std::min(sWidth / 1920.0f, sHeight / 1080.0f);
}

static bool WorldToScreen(glm::vec3 worldPos, glm::vec2& screenPos,
                           const glm::mat4& view, const glm::mat4& proj,
                           int sWidth, int sHeight) {
    glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.0f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    screenPos.x   = (ndc.x + 1.0f) * 0.5f * sWidth;
    screenPos.y   = (1.0f - ndc.y) * 0.5f * sHeight;
    return true;
}

static int CountAlive(const std::vector<BlobInstance>& blobs) {
    int n = 0;
    for (const auto& b : blobs) if (b.alive) n++;
    return n;
}

// --------------------- Best Blob Preview Framebuffer ---------------------

static void InitBestBlobPreviewFramebuffer() {
    if (gBestBlobPreviewFBO != 0) return;

    glGenFramebuffers(1, &gBestBlobPreviewFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, gBestBlobPreviewFBO);

    glGenTextures(1, &gBestBlobPreviewColorTex);
    glBindTexture(GL_TEXTURE_2D, gBestBlobPreviewColorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 BEST_BLOB_PREVIEW_SIZE, BEST_BLOB_PREVIEW_SIZE,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, gBestBlobPreviewColorTex, 0);

    glGenRenderbuffers(1, &gBestBlobPreviewDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, gBestBlobPreviewDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          BEST_BLOB_PREVIEW_SIZE, BEST_BLOB_PREVIEW_SIZE);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, gBestBlobPreviewDepthRBO);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Best blob preview framebuffer is incomplete!" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void ShutdownBestBlobPreviewFramebuffer() {
    if (gBestBlobPreviewDepthRBO) {
        glDeleteRenderbuffers(1, &gBestBlobPreviewDepthRBO);
        gBestBlobPreviewDepthRBO = 0;
    }
    if (gBestBlobPreviewColorTex) {
        glDeleteTextures(1, &gBestBlobPreviewColorTex);
        gBestBlobPreviewColorTex = 0;
    }
    if (gBestBlobPreviewFBO) {
        glDeleteFramebuffers(1, &gBestBlobPreviewFBO);
        gBestBlobPreviewFBO = 0;
    }
}

static void RenderBestBlobPreview(const IndividualRecord& bestBlob) {
    InitBestBlobPreviewFramebuffer();

    GLint previousFBO        = 0;
    GLint previousViewport[4] = {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);
    glGetIntegerv(GL_VIEWPORT, previousViewport);

    glBindFramebuffer(GL_FRAMEBUFFER, gBestBlobPreviewFBO);
    glViewport(0, 0, BEST_BLOB_PREVIEW_SIZE, BEST_BLOB_PREVIEW_SIZE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.16f, 0.18f, 0.24f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    Genome g { bestBlob.gSize, bestBlob.gSpeed, bestBlob.gPredation, bestBlob.gAggression };
    float  worldRadius = gObjectMesh.GetBoundingRadius() * gObjectScale;
    Phenotype p        = MakePhenotype(g, gObjectScale, worldRadius);

    glm::mat4 previewView = glm::lookAt(
        glm::vec3(0.0f, 0.95f, 2.15f),
        glm::vec3(0.0f, 0.25f, 0.0f),
        glm::vec3(0.0f, 1.0f,  0.0f)
    );
    glm::mat4 previewProj = glm::perspective(glm::radians(30.0f), 1.0f, 0.1f, 100.0f);

    glm::mat4 model = glm::rotate(glm::scale(glm::mat4(1.0f), glm::vec3(p.worldScale)),
                                  glm::radians(-25.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 lightSpace = glm::ortho(-25.0f, 25.0f, -25.0f, 25.0f, 0.1f, 200.0f)
                         * glm::lookAt(gLightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glUseProgram(gShaderProgram);
    SetLightUniforms(gShaderProgram);

    glUniformMatrix4fv(glGetUniformLocation(gShaderProgram, "uModel"),            1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(gShaderProgram, "uView"),             1, GL_FALSE, glm::value_ptr(previewView));
    glUniformMatrix4fv(glGetUniformLocation(gShaderProgram, "uProjection"),       1, GL_FALSE, glm::value_ptr(previewProj));
    glUniformMatrix4fv(glGetUniformLocation(gShaderProgram, "uLightSpaceMatrix"), 1, GL_FALSE, glm::value_ptr(lightSpace));

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gShadowMapTexture);
    glUniform1i(glGetUniformLocation(gShaderProgram, "uShadowMap"), 1);
    glUniform3f(glGetUniformLocation(gShaderProgram, "uObjectColor"), p.color.r, p.color.g, p.color.b);

    glBindVertexArray(gObjectMesh.VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)gObjectMesh.indices.size(), GL_UNSIGNED_INT, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
    glViewport(previousViewport[0], previousViewport[1],
               previousViewport[2], previousViewport[3]);
}

// --------------------- Debug Overlay ---------------------

static void DrawDebugOverlay(std::vector<BlobInstance>& blobs, int selectedBlob,
                              const glm::mat4& view, const glm::mat4& proj,
                              int sWidth, int sHeight) {
    ImDrawList* drawList      = ImGui::GetBackgroundDrawList();
    const float PICK_RADIUS   = 0.5f;

    for (int i = 0; i < (int)blobs.size(); i++) {
        const auto& blob = blobs[i];
        if (!blob.alive) continue;

        glm::vec3 centerPos = blob.body.position;
        glm::vec2 screenBase;
        if (!WorldToScreen(centerPos, screenBase, view, proj, sWidth, sHeight)) continue;

        // Forward arrow
        glm::vec3 hVel    = glm::vec3(blob.body.velocity.x, 0.0f, blob.body.velocity.z);
        glm::vec3 forward = glm::length(hVel) > 0.1f
                          ? glm::normalize(hVel)
                          : glm::vec3(0.0f, 0.0f, 1.0f);

        glm::vec2 screenTip;
        if (!WorldToScreen(centerPos + forward * blob.phenotype.worldScale * 3.0f,
                           screenTip, view, proj, sWidth, sHeight)) continue;

        const ImU32 arrowColor = IM_COL32(255, 255, 0, 180);
        drawList->AddLine(ImVec2(screenBase.x, screenBase.y),
                          ImVec2(screenTip.x,  screenTip.y),
                          arrowColor, 1.5f);

        glm::vec2 dir  = glm::normalize(screenTip - screenBase);
        glm::vec2 perp = glm::vec2(-dir.y, dir.x) * 4.0f;
        glm::vec2 back = screenTip - dir * 8.0f;
        drawList->AddTriangleFilled(
            ImVec2(screenTip.x,          screenTip.y),
            ImVec2(back.x + perp.x, back.y + perp.y),
            ImVec2(back.x - perp.x, back.y - perp.y),
            arrowColor
        );

        // World scale circle
        glm::vec3 camRight = glm::vec3(view[0][0], view[1][0], view[2][0]);
        glm::vec2 screenEdge;
        if (!WorldToScreen(centerPos + camRight * blob.phenotype.worldScale,
                           screenEdge, view, proj, sWidth, sHeight)) continue;

        drawList->AddCircle(
            ImVec2(screenBase.x, screenBase.y),
            glm::length(screenEdge - screenBase),
            (i == selectedBlob) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 80),
            32, 1.0f
        );

        // Pick radius circle
        glm::vec2 screenPickEdge;
        if (!WorldToScreen(centerPos + camRight * PICK_RADIUS,
                           screenPickEdge, view, proj, sWidth, sHeight)) continue;

        drawList->AddCircle(
            ImVec2(screenBase.x, screenBase.y),
            glm::length(screenPickEdge - screenBase),
            IM_COL32(0, 255, 0, 60), 32, 1.0f
        );
    }
}

// --------------------- Windows ---------------------

static void DrawSelectedBlobWindow(const std::vector<BlobInstance>& blobs,
                                   int selectedBlob, int sWidth, int sHeight) {
    float ui           = UIScale(sWidth, sHeight);
    float windowWidth  = 400.0f * ui;
    float windowHeight = 540.0f * ui;

    ImGui::SetNextWindowPos(ImVec2(12.0f * ui, 10.0f * ui), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    ImGui::Begin("Selected Blob", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (selectedBlob == -1 || !blobs[selectedBlob].alive) {
        ImGui::SetWindowFontScale(1.1f * ui);
        ImGui::Text("Click a blob to inspect it");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
        return;
    }

    const BlobInstance& blob = blobs[selectedBlob];
    const Genome&       g    = blob.genome;
    const Phenotype&    p    = blob.phenotype;

    ImGui::BeginChild("SelectedBlobScroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::SetWindowFontScale(1.05f * ui);

    ImGui::Text("Blob #%d", selectedBlob);
    ImGui::ColorButton("##color",
        ImVec4(p.color.r, p.color.g, p.color.b, 1.0f), 0, ImVec2(400.0f * ui, 20.0f * ui));
    ImGui::Separator();

    ImGui::Text("--- Genome ---");
    ImGui::SliderFloat("Size",       (float*)&g.size,       0.0f, 1.0f);
    ImGui::SliderFloat("Speed",      (float*)&g.speed,      0.0f, 1.0f);
    ImGui::SliderFloat("Predation",  (float*)&g.predation,  0.0f, 1.0f);
    ImGui::SliderFloat("Aggression", (float*)&g.aggression, 0.0f, 1.0f);

    ImGui::Separator();
    ImGui::Text("--- Phenotype ---");
    ImGui::Text("World Scale:    %.3f", p.worldScale);
    ImGui::Text("Mass:           %.3f", p.mass);
    ImGui::Text("Move Force:     %.3f", p.moveForce);
    ImGui::Text("Max Speed:      %.3f", p.maxSpeed);
    ImGui::Text("Max Energy:     %.3f", p.maxEnergy);
    ImGui::Text("Energy Cost:    %.3f", p.energyCost);
    ImGui::Text("Attack Damage:  %.3f", p.attackDamage);
    ImGui::Text("Attack Range:   %.3f", p.attackRange);
    ImGui::Text("Sight Dist:     %.3f", p.sightDist);
    ImGui::Text("Predation Bias: %.3f", p.predationBias);
    ImGui::Text("Max Health:     %.3f", p.maxHealth);
    ImGui::Text("Starv. Rate:    %.3f", p.starvationRate);

    ImGui::Separator();
    ImGui::Text("--- Runtime ---");

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
    ImGui::ProgressBar(blob.health / p.maxHealth, ImVec2(-1, 0), "Health");
    ImGui::PopStyleColor();
    ImGui::Text("%.1f / %.1f HP", blob.health, p.maxHealth);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
    ImGui::ProgressBar(blob.energy / p.maxEnergy, ImVec2(-1, 0), "Energy");
    ImGui::PopStyleColor();
    ImGui::Text("%.1f / %.1f EP", blob.energy, p.maxEnergy);

    ImGui::Text("Grounded:  %s", blob.body.Grounded ? "Yes" : "No");
    ImGui::Text("Velocity:  (%.2f, %.2f, %.2f)",
                blob.body.velocity.x, blob.body.velocity.y, blob.body.velocity.z);

    ImGui::Separator();
    ImGui::Text("--- Stats ---");
    ImGui::Text("Time Alive:    %.1f s", blob.stats.timeAlive);
    ImGui::Text("Energy Gained: %.1f",   blob.stats.energyGained);
    ImGui::Text("Health Lost:   %.1f",   blob.stats.healthLost);
    ImGui::Text("Damage Dealt:  %.1f",   blob.stats.damageDealt);
    ImGui::Text("Food Eaten:    %d",     blob.stats.foodEaten);
    ImGui::Text("Kills:         %d",     blob.stats.killCount);

    ImGui::SetWindowFontScale(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

static void DrawGenerationWindow(const std::vector<BlobInstance>& blobs,
                                 int blobCount, int sWidth, int sHeight) {
    float  ui   = UIScale(sWidth, sHeight);
    ImVec2 size(380.0f * ui, 210.0f * ui);

    ImGui::SetNextWindowPos(ImVec2(sWidth - size.x - 12.0f * ui, 12.0f * ui), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("Generation", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    ImGui::SetWindowFontScale(1.1f * ui);
    ImGui::Text("Generation: %d",    gGeneration.number);
    ImGui::Text("Time: %.1f / %.1f", gGeneration.timer, gGeneration.duration);
    ImGui::ProgressBar(gGeneration.timer / gGeneration.duration, ImVec2(-1, 0));
    ImGui::Text("Alive: %d / %d",    CountAlive(blobs), blobCount);
    ImGui::Text("Time Scale: %.1fx", gTimeScale);
    ImGui::Text("(Keys 1-8 to change)");
    ImGui::Separator();
    if (ImGui::SliderInt("Food Count", &gFoodCount, 0, 1000))
        SpawnFood(100.0f);

    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
}

static void DrawBestBlobWindow(int sWidth, int sHeight) {
    float ui           = UIScale(sWidth, sHeight);
    float windowWidth  = 400.0f * ui;
    float windowHeight = 540.0f * ui;
    float topOffset    = 10.0f * ui + windowHeight + 4.0f * ui;

    ImGui::SetNextWindowPos(ImVec2(12.0f * ui, topOffset), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    ImGui::Begin("Best Blob", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

    if (gGenerationLog.empty()) {
        ImGui::Text("No generations yet.");
        ImGui::End();
        return;
    }

    const GenerationRecord& gr = gGenerationLog.back();
    const IndividualRecord& b  = gr.bestIndividual;

    ImGui::BeginChild("BestBlobScroll", ImVec2(0, 0), false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    RenderBestBlobPreview(b);

    float previewSize  = 220.0f * ui;
    float contentWidth = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX((contentWidth - previewSize) * 0.5f);
    ImGui::Image((ImTextureID)(intptr_t)gBestBlobPreviewColorTex,
                 ImVec2(previewSize, previewSize),
                 ImVec2(0, 1), ImVec2(1, 0));

    ImGui::Separator();
    ImGui::Text("Generation: %d",  gr.number);
    ImGui::Text("Fitness:    %.1f", b.fitness);
    ImGui::Text("Survived:   %s",  b.survived ? "Yes" : "No");

    ImGui::Separator();
    ImGui::Text("Genome");
    ImGui::Text("Size:       %.2f", b.gSize);
    ImGui::Text("Speed:      %.2f", b.gSpeed);
    ImGui::Text("Predation:  %.2f", b.gPredation);
    ImGui::Text("Aggression: %.2f", b.gAggression);

    ImGui::Separator();
    ImGui::Text("Phenotype");
    ImGui::Text("World Scale:   %.3f", b.worldScale);
    ImGui::Text("Move Force:    %.3f", b.moveForce);
    ImGui::Text("Max Energy:    %.3f", b.maxEnergy);
    ImGui::Text("Energy Cost:   %.3f", b.energyCost);
    ImGui::Text("Attack Damage: %.3f", b.attackDamage);
    ImGui::Text("Max Health:    %.3f", b.maxHealth);

    ImGui::Separator();
    ImGui::Text("Stats");
    ImGui::Text("Kills:         %d",   b.killCount);
    ImGui::Text("Food Eaten:    %d",   b.foodEaten);
    ImGui::Text("Time Alive:    %.1f s", b.timeAlive);
    ImGui::Text("Energy Gained: %.1f", b.energyGained);
    ImGui::Text("Health Lost:   %.1f", b.healthLost);
    ImGui::Text("Damage Dealt:  %.1f", b.damageDealt);

    ImGui::EndChild();
    ImGui::End();
}

static void DrawEvolutionLogWindow(int sWidth, int sHeight) {
    float  ui   = UIScale(sWidth, sHeight);
    ImVec2 size(380.0f * ui, 700.0f * ui);

    ImGui::SetNextWindowPos(ImVec2(sWidth - size.x - 12.0f * ui, 290.0f * ui), ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::Begin("Evolution Log", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowFontScale(1.0f * ui);

    if (gGenerationLog.empty()) {
        ImGui::Text("No generations yet.");
        ImGui::SetWindowFontScale(1.0f);
        ImGui::End();
        return;
    }

    static std::vector<float> bestFitnessValues;
    static std::vector<float> avgFitnessValues;
    bestFitnessValues.clear();
    avgFitnessValues.clear();

    float maxFitness = 1.0f;
    for (const auto& gr : gGenerationLog) {
        bestFitnessValues.push_back(gr.bestFitness);
        avgFitnessValues.push_back(gr.avgFitness);
        maxFitness = std::max(maxFitness, std::max(gr.bestFitness, gr.avgFitness));
    }

    const GenerationRecord& latest = gGenerationLog.back();
    ImGui::Text("Fitness Over Generations");
    ImGui::Text("Latest Best: %.1f", latest.bestFitness);
    ImGui::Text("Latest Avg:  %.1f", latest.avgFitness);

    ImGui::Separator();
    ImGui::Text("Best Fitness");
    ImGui::PlotLines("##BestFitness",
                     bestFitnessValues.data(), (int)bestFitnessValues.size(),
                     0, nullptr, 0.0f, maxFitness * 1.1f, ImVec2(-1, 90.0f * ui));

    ImGui::Text("Average Fitness");
    ImGui::PlotLines("##AvgFitness",
                     avgFitnessValues.data(), (int)avgFitnessValues.size(),
                     0, nullptr, 0.0f, maxFitness * 1.1f, ImVec2(-1, 90.0f * ui));

    ImGui::Separator();
    ImGui::Text("Generation Details");

    for (int i = (int)gGenerationLog.size() - 1; i >= 0; i--) {
        const GenerationRecord& gr = gGenerationLog[i];
        char label[32];
        snprintf(label, sizeof(label), "Gen %d##%d", gr.number, i);

        if (ImGui::CollapsingHeader(label)) {
            ImGui::Text("Survivors:  %d / %d", gr.survivors, gr.totalBlobs);
            ImGui::Text("Best fit:   %.1f",    gr.bestFitness);
            ImGui::Text("Avg fit:    %.1f",    gr.avgFitness);
            ImGui::Text("Avg size:   %.2f",    gr.avgSize);
            ImGui::Text("Avg speed:  %.2f",    gr.avgSpeed);
            ImGui::Text("Avg pred:   %.2f",    gr.avgPredation);
            ImGui::Text("Avg aggr:   %.2f",    gr.avgAggression);
            ImGui::Text("Avg nrg+:   %.1f",    gr.avgEnergyGained);
            ImGui::Text("Kills:      %d",      gr.totalKills);
            ImGui::Text("Food eaten: %d",      gr.totalFoodEaten);
            ImGui::Separator();
            ImGui::Text("Best blob:");
            ImGui::Text("  Fitness:  %.1f",   gr.bestIndividual.fitness);
            ImGui::Text("  Survived: %s",     gr.bestIndividual.survived ? "Yes" : "No");
            ImGui::Text("  Size:     %.2f",   gr.bestIndividual.gSize);
            ImGui::Text("  Speed:    %.2f",   gr.bestIndividual.gSpeed);
            ImGui::Text("  Pred:     %.2f",   gr.bestIndividual.gPredation);
            ImGui::Text("  Kills:    %d",     gr.bestIndividual.killCount);
            ImGui::Text("  Food:     %d",     gr.bestIndividual.foodEaten);
        }
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::End();
}

static void DrawTitleScreen(int sWidth, int sHeight) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::Begin("TitleScreen", nullptr,
                 ImGuiWindowFlags_NoDecoration  |
                 ImGuiWindowFlags_NoMove        |
                 ImGuiWindowFlags_NoResize      |
                 ImGuiWindowFlags_NoSavedSettings);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
        IM_COL32(20, 24, 36, 255)
    );

    ImVec2 windowSize = ImGui::GetWindowSize();
    ImVec2 center(windowSize.x * 0.5f, windowSize.y * 0.5f);

    float buttonWidth  = sWidth  * 0.3f;
    float buttonHeight = sHeight * 0.2f;

    ImVec2 panelMin(center.x - sWidth * 0.4f, center.y - sHeight * 0.4f);
    ImVec2 panelMax(center.x + sWidth * 0.4f, center.y + sHeight * 0.4f);
    drawList->AddRectFilled(panelMin, panelMax, IM_COL32(45, 50, 75, 235), 16.0f);
    drawList->AddRect(panelMin, panelMax, IM_COL32(160, 180, 230, 120), 16.0f, 0, 2.0f);

    const char* title = "Evolution Simulator";
    ImGui::SetWindowFontScale(sHeight / 140.0f);
    ImVec2 titleSize = ImGui::CalcTextSize(title);
    ImGui::SetCursorPos(ImVec2(center.x - titleSize.x * 0.5f, center.y - sHeight * 0.3f));
    ImGui::Text("%s", title);

    ImGui::SetCursorPos(ImVec2(center.x - buttonWidth * 0.5f, center.y + sHeight * 0.05f));
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.30f, 0.45f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.55f, 0.95f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.20f, 0.35f, 0.75f, 1.0f));
    ImGui::SetWindowFontScale(3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
    if (ImGui::Button("Play Simulation", ImVec2(buttonWidth, buttonHeight)))
        gShowTitleScreen = false;
    ImGui::PopStyleVar();
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor(3);

    ImGui::End();
}

// --------------------- Entry Point ---------------------

void DrawBlobUI(std::vector<BlobInstance>& blobs, int selectedBlob,
                int blobCount, int sWidth, int sHeight,
                const glm::mat4& view, const glm::mat4& proj,
                float objectBottomOffset) {

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    if (gShowTitleScreen) {
        DrawTitleScreen(sWidth, sHeight);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        return;
    }

    if (gDebugMode)
        DrawDebugOverlay(blobs, selectedBlob, view, proj, sWidth, sHeight);

    DrawSelectedBlobWindow(blobs, selectedBlob, sWidth, sHeight);
    DrawBestBlobWindow(sWidth, sHeight);
    DrawGenerationWindow(blobs, blobCount, sWidth, sHeight);
    DrawEvolutionLogWindow(sWidth, sHeight);

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void ShutdownUIPreviewResources() {
    ShutdownBestBlobPreviewFramebuffer();
}