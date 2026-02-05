#ifndef UI_H
#define UI_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

// Simple app state (menu vs game)
enum class Mode { Menu, Game };
static Mode mode = Mode::Menu;

// Basic UI button data
struct Button {
    float x, y, w, h;        // position + size in pixels
    bool hovered = false;    // set true when mouse is inside
};

// Global start button used by the menu
static Button startBtn;

// Check if point p is inside button rectangle
static bool inside(const glm::vec2& p, const Button& b) {
    return p.x >= b.x && p.x <= b.x + b.w &&
           p.y >= b.y && p.y <= b.y + b.h;
}

// Get mouse position in pixel coords (origin at bottom-left)
static glm::vec2 mousePixels(GLFWwindow* win) {
    double mx, my;
    glfwGetCursorPos(win, &mx, &my); // GLFW gives top-left origin

    // framebuffer size (handles HiDPI)
    int fbw = 1, fbh = 1;
    glfwGetFramebufferSize(win, &fbw, &fbh);
    fbw = std::max(1, fbw);
    fbh = std::max(1, fbh);

    // flip y so (0,0) is bottom-left
    return glm::vec2((float)mx, (float)(fbh - my));
}

// Draw a rectangle in pixel space using a unit quad VAO
static void drawRectPx(Shader& sh,
                       const glm::mat4& VP_ortho,
                       float x, float y, float w, float h,
                       const glm::vec4& color,
                       GLuint quadVAO)
{
    glUseProgram(sh.ID); // use UI shader

    // Build model matrix to move + scale the unit quad into place
    glm::mat4 M(1.0f);
    M = glm::translate(M, glm::vec3(x, y, 0.0f));
    M = glm::scale(M, glm::vec3(w, h, 1.0f));

    // Set matrices for UI draw
    glUniformMatrix4fv(glGetUniformLocation(sh.ID, "uViewProj"), 1, GL_FALSE, &VP_ortho[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(sh.ID, "uModel"),    1, GL_FALSE, &M[0][0]);

    // Switch shader into solid-color mode
    glUniform1i(glGetUniformLocation(sh.ID, "uUseSolidColor"), 1);
    glUniform4fv(glGetUniformLocation(sh.ID, "uSolidColor"), 1, &color[0]);

    // Draw the quad (2 triangles)
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Reset solid-color flag for later 3D draws
    glUniform1i(glGetUniformLocation(sh.ID, "uUseSolidColor"), 0);
}

#endif
