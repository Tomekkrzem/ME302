#ifndef MESH_H
#define MESH_H

#include <iostream>
#include <vector>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Simple mesh wrapper (VAO/VBO/EBO + index count)
struct Mesh
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    GLsizei indexCount = 0; // how many indices to draw

    void destroy()
    {
        // free GPU buffers if they exist
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);

        VAO = VBO = EBO = 0;
        indexCount = 0;
    }

    void draw() const
    {
        // bind VAO and draw indexed triangles
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    }
};

// Build a pyramid mesh with per-face normals
static Mesh createPyramid()
{
    // vertex = position + normal
    struct V
    {
        glm::vec3 p;
        glm::vec3 n;
    };

    // define pyramid points
    const float half = 0.5f;
    const glm::vec3 tip(0.0f, 0.7f, 0.0f);
    const glm::vec3 base0(-half, -0.7f, -half);
    const glm::vec3 base1(half, -0.7f, -half);
    const glm::vec3 base2(half, -0.7f, half);
    const glm::vec3 base3(-half, -0.7f, half);

    // helper to compute triangle normal
    auto faceNorm = [](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        return glm::normalize(glm::cross(b - a, c - a));
    };

    // side normals + base normal
    glm::vec3 norm0 = faceNorm(tip, base0, base1);
    glm::vec3 norm1 = faceNorm(tip, base1, base2);
    glm::vec3 norm2 = faceNorm(tip, base2, base3);
    glm::vec3 norm3 = faceNorm(tip, base3, base0);
    glm::vec3 norm_base(0.0f, -1.0f, 0.0f);

    // build triangles (duplicated verts so each face has a flat normal)
    std::vector<V> verts = {
        {tip, norm0}, {base0, norm0}, {base1, norm0},
        {tip, norm1}, {base1, norm1}, {base2, norm1},
        {tip, norm2}, {base2, norm2}, {base3, norm2},
        {tip, norm3}, {base3, norm3}, {base0, norm3},

        {base0, norm_base}, {base2, norm_base}, {base1, norm_base},
        {base0, norm_base}, {base3, norm_base}, {base2, norm_base}
    };

    // trivial indices 0..N-1
    std::vector<unsigned int> idx(verts.size());
    for (unsigned int i = 0; i < (unsigned)idx.size(); i++)
    {
        idx[i] = i;
    }

    Mesh mesh;
    mesh.indexCount = (GLsizei)idx.size();

    // create VAO + buffers
    glGenVertexArrays(1, &mesh.VAO);
    glBindVertexArray(mesh.VAO);

    glGenBuffers(1, &mesh.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &mesh.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);

    // layout: location 0 = position, location 1 = normal
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,p));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V,n));

    glBindVertexArray(0);
    return mesh;
}

#endif
