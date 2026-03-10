#pragma once
#include <vector>
#include <string>
#include <glad/glad.h>
#include <glm/glm.hpp>

struct Vertex {
    float x, y, z;
    float nx, ny, nz;
    float u, v;
};

class Mesh {
public:

    GLuint VAO, VBO, EBO;
    std::vector<Vertex>  vertices;
    std::vector<GLuint>  indices;

    void Load(const std::string& objPath);
    void Setup();
    void Draw();
    float GetBoundingRadius() const;
    glm::vec3 GetCenter() const;

};
