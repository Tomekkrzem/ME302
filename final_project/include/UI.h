#pragma once

#include <vector>
#include <glm/glm.hpp>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "AI.h" // or wherever BlobInstance is defined

void DrawBlobUI(std::vector<BlobInstance>& blobs, int selectedBlob,
                int blobCount, int sWidth, int sHeight,
                const glm::mat4& view, const glm::mat4& proj,
                float objectBottomOffset);

void ShutdownUIPreviewResources();