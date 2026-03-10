#include "Mesh.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>   

void Mesh::Load(const std::string& objPath) {
    std::vector<std::array<float, 3>> positions;
    std::vector<std::array<float, 3>> normals;
    std::vector<std::array<float, 2>> uvs;

    std::map<std::tuple<int,int,int>, GLuint> vertexCache;

    std::ifstream file(objPath);
    if (!file.is_open()) {
        std::cout << "ERROR: Could not open OBJ: " << objPath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string token;
        ss >> token;

        // Skip all non-geometry Blender tokens
        if (token == "mtllib" ||   // Material library file
            token == "usemtl" ||   // Material name
            token == "o"      ||   // Object name
            token == "g"      ||   // Group name
            token == "s"      ||   // Smooth shading
            token == "l"      ||   // Line element
            token == "#"      ||   // Comment
            token == "") {         // Empty line
            continue;
        }

        if (token == "v") {
            float x, y, z;
            ss >> x >> y >> z;
            positions.push_back({x, y, z});

        } else if (token == "vn") {
            float nx, ny, nz;
            ss >> nx >> ny >> nz;
            normals.push_back({nx, ny, nz});

        } else if (token == "vt") {
            float u, v;
            ss >> u >> v;
            uvs.push_back({u, 1.0f - v});

        } else if (token == "f") {
        std::string vertToken;
        std::vector<std::tuple<int,int,int>> faceVerts;

        while (ss >> vertToken) {
            int vIdx = 0, vtIdx = 0, vnIdx = 0;

            // Count slashes to determine format
            int slashCount = std::count(vertToken.begin(), vertToken.end(), '/');
            bool hasDoubleSlash = vertToken.find("//") != std::string::npos;

            if (slashCount == 0) {
                // Format: f v
                vIdx = std::stoi(vertToken);

            } else if (slashCount == 1) {
                // Format: f v/vt
                size_t slash = vertToken.find('/');
                vIdx  = std::stoi(vertToken.substr(0, slash));
                vtIdx = std::stoi(vertToken.substr(slash + 1));

            } else if (slashCount == 2 && hasDoubleSlash) {
                // Format: f v//vn
                size_t slash1 = vertToken.find('/');
                size_t slash2 = vertToken.find('/', slash1 + 1);
                vIdx  = std::stoi(vertToken.substr(0, slash1));
                vnIdx = std::stoi(vertToken.substr(slash2 + 1));

            } else if (slashCount == 2 && !hasDoubleSlash) {
                // Format: f v/vt/vn
                size_t slash1 = vertToken.find('/');
                size_t slash2 = vertToken.find('/', slash1 + 1);
                vIdx  = std::stoi(vertToken.substr(0, slash1));
                vtIdx = std::stoi(vertToken.substr(slash1 + 1, slash2 - slash1 - 1));
                vnIdx = std::stoi(vertToken.substr(slash2 + 1));
            }

            auto resolve = [](int i, int size) {
                return i < 0 ? size + i : i - 1;
            };
            vIdx  = resolve(vIdx,  (int)positions.size());
            vtIdx = resolve(vtIdx, (int)uvs.size());
            vnIdx = resolve(vnIdx, (int)normals.size());

            // Guard against out-of-range indices
            if (vIdx  < 0 || vIdx  >= (int)positions.size()) continue;
            if (!normals.empty() && (vnIdx < 0 || vnIdx >= (int)normals.size())) continue;
            if (!uvs.empty()     && (vtIdx < 0 || vtIdx >= (int)uvs.size()))     continue;

            faceVerts.push_back({vIdx, vtIdx, vnIdx});
        }

            for (int i = 1; i + 1 < (int)faceVerts.size(); i++) {
                std::tuple<int,int,int> tri[3] = {
                    faceVerts[0],
                    faceVerts[i],
                    faceVerts[i + 1]
                };

                for (auto& [vi, vti, vni] : tri) {
                    auto key = std::make_tuple(vi, vti, vni);

                    auto it = vertexCache.find(key);
                    if (it != vertexCache.end()) {
                        indices.push_back(it->second);
                    } else {
                        Vertex vert{};
                        vert.x  = positions[vi][0];
                        vert.y  = positions[vi][1];
                        vert.z  = positions[vi][2];

                        if (!normals.empty()) {
                            vert.nx = normals[vni][0];
                            vert.ny = normals[vni][1];
                            vert.nz = normals[vni][2];
                        }

                        if (!uvs.empty()) {
                            vert.u = uvs[vti][0];
                            vert.v = uvs[vti][1];
                        }

                        GLuint newIdx = (GLuint)vertices.size();
                        vertices.push_back(vert);
                        indices.push_back(newIdx);
                        vertexCache[key] = newIdx;
                    }
                }
            }
        }
    }

    std::cout << "Loaded: " << objPath
              << " | Verts: "   << vertices.size()
              << " | Indices: " << indices.size()
              << std::endl;
}                                                     

void Mesh::Setup() {
    glGenVertexArrays(1, &VAO);     
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);         
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 vertices.size() * sizeof(Vertex), 
                 vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &EBO);      
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(GLuint),
                 indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (GLvoid*)offsetof(Vertex, x));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (GLvoid*)offsetof(Vertex, nx));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                          sizeof(Vertex), (GLvoid*)offsetof(Vertex, u));

    glBindVertexArray(0);
}

void Mesh::Draw() {
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
}

glm::vec3 Mesh::GetCenter() const {
    glm::vec3 min( 999999.0f);
    glm::vec3 max(-999999.0f);

    for (const auto& v : vertices) {
        min.x = std::min(min.x, v.x);
        min.y = std::min(min.y, v.y);
        min.z = std::min(min.z, v.z);
        max.x = std::max(max.x, v.x);
        max.y = std::max(max.y, v.y);
        max.z = std::max(max.z, v.z);
    }

    return (min + max) * 0.5f;
}

float Mesh::GetBoundingRadius() const {
    glm::vec3 center = GetCenter();
    float maxDist = 0.0f;

    for (const auto& v : vertices) {
        glm::vec3 pos(v.x, v.y, v.z);
        float dist = glm::length(pos - center);
        maxDist = std::max(maxDist, dist);
    }

    return maxDist;
}