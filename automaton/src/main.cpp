#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <optional>
#include <algorithm>
#include <random>
#include <chrono>

#include "shader.h" 
#include "text_renderer.h"

// -------------------------------- Config --------------------------------
// Grid resolution
static constexpr int W_GRID = 120;
static constexpr int H_GRID = 80;

// Window resolution
static constexpr int W_WIN = 1400;
static constexpr int H_WIN = 1000;

// Visual scale constants
static constexpr float TILE_SIZE = 1.0f;
static constexpr float LINE_THICKNESS = 0.1f;

// -------------------------------- GRID WORLD --------------------------------

// Represents one cell state
enum class Cell : uint8_t { DEAD = 0, ALIVE = 1 };

// Grid container storing all cells
struct World {
    int w = W_GRID;
    int h = H_GRID;
    std::vector<Cell> cells;

    // Initialize all cells as DEAD
    World() : cells(size_t(W_GRID * H_GRID), Cell::DEAD) {}

    // Check if coordinates are inside the grid
    bool inWorldBounds(int x, int y) const {
        return x >= 0 && x < w && y >= 0 && y < h;
    }

    // Safe getter with bounds handling
    Cell get(int x, int y) const {
        if (!inWorldBounds(x, y)) return Cell::DEAD;
        return cells[size_t(y * w + x)];
    }

    // Safe setter
    void set(int x, int y, Cell c) {
        if (!inWorldBounds(x, y)) return;
        cells[size_t(y * w + x)] = c;
    }

    // Convenience check
    bool isDead(int x, int y) const {
        return get(x, y) == Cell::DEAD;
    }

    // Flip cell state on click
    void toggleCell(int x, int y) {
        if (!inWorldBounds(x, y)) return;
        set(x, y, isDead(x, y) ? Cell::ALIVE : Cell::DEAD);
    }

    // Clears all Tiles
    void clearAll() {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                set(x, y, Cell::DEAD);
            }
        }
    }

};

// -------------------------------- GAME OF LIFE --------------------------------

// Computes next generation using Conway's rules
World findNextGen(World world) {
    // 8-neighborhood directions
    std::vector<std::vector<int>> directions = {
        {0,1},{1,0},{0,-1},{-1,0},{1,1},{-1,-1},{1,-1},{-1,1}
    };

    World nextGen;

    // Evaluate each cell
    for (int y = 0; y < world.h; ++y) {
        for (int x = 0; x < world.w; ++x) {
            int live = 0;

            // Count live neighbors
            for (auto dir : directions) {
                int X = x + dir[0];
                int Y = y + dir[1];
                if (world.inWorldBounds(X, Y) && world.get(X, Y) == Cell::ALIVE)
                    live++;
            }

            // Apply Conway's rules
            if (world.get(x, y) == Cell::ALIVE && (live < 2 || live > 3))
                nextGen.set(x, y, Cell::DEAD);
            else if (world.get(x, y) == Cell::DEAD && live == 3)
                nextGen.set(x, y, Cell::ALIVE);
            else
                nextGen.set(x, y, world.get(x, y));
        }
    }

    return nextGen;
}

// -------------------------------- CAMERA --------------------------------

// 2D orthographic camera controller
struct Camera2D {
    float zoom = 30.0f;
    glm::vec2 center = glm::vec2(W_GRID / 2.0f, H_GRID / 2.0f);

    // Builds projection matrix
    glm::mat4 proj(int fbw, int fbh) const {
        float aspect = (fbh == 0) ? 1.0f : float(fbw) / float(fbh);

        float halfH = zoom * 0.5f;
        float halfW = halfH * aspect;

        return glm::ortho(
            center.x - halfW, center.x + halfW,
            center.y - halfH, center.y + halfH,
            -1.0f, 1.0f
        );
    }

    // Converts screen-space to world-space coordinates
    glm::vec2 invProj(double sx, double sy, int fbw, int fbh) const {
        float x = float((sx / double(fbw)) * 2.0 - 1.0);
        float y = float((1.0 - (sy / double(fbh))) * 2.0 - 1.0);

        glm::mat4 invP = glm::inverse(proj(fbw, fbh));
        glm::vec4 world = invP * glm::vec4(x, y, 0.0f, 1.0f);
        return glm::vec2(world);
    }
};

// -------------------------------- RENDERER --------------------------------

// Simple quad renderer using instanced uniforms
struct Renderer {
    Shader myShader;
    GLuint prog = 0, vao = 0, vbo = 0;

    // Uniform handles
    GLint uMVP = -1, uPos = -1, uSize = -1, uColor = -1;

    Renderer() : myShader("src/shader.vs", "src/shader.fs") {}

    // Setup shader + quad geometry
    void init() {
        prog = myShader.ID;

        uMVP = glGetUniformLocation(prog, "uMVP");
        uPos = glGetUniformLocation(prog, "uPos");
        uSize = glGetUniformLocation(prog, "uSize");
        uColor = glGetUniformLocation(prog, "uColor");

        // Unit quad mesh
        float verts[] = {
            0,0, 1,0, 1,1,
            0,0, 1,1, 0,1
        };

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
        glBindVertexArray(0);
    }

    // Free GPU resources
    void clear() {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
    }

    // Draws a transformed quad
    void drawQuad(const glm::mat4& mvp, glm::vec2 pos, glm::vec2 size, glm::vec4 color) {
        glUseProgram(prog);
        glUniformMatrix4fv(uMVP, 1, GL_FALSE, &mvp[0][0]);
        glUniform2f(uPos, pos.x, pos.y);
        glUniform2f(uSize, size.x, size.y);
        glUniform4f(uColor, color.r, color.g, color.b, color.a);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }
};

// -------------------------------- APP --------------------------------

// Main application controller
struct App {
    GLFWwindow* win = nullptr;

    World world;
    Camera2D cam;
    Renderer ren;
    
    glm::vec2 homeCenter = glm::vec2(W_GRID / 2.0f, H_GRID / 2.0f);

    float maxZoom = 80.0f;
    float snapStart = 0.95f * maxZoom;

    int fbw = 1, fbh = 1;

    std::optional<glm::ivec2> hover;
    bool leftDown = false;

    TextRenderer menuText, gameText;

    bool paused = true;

    // Fixed timestep simulation variables
    double tick_hz = 10.0;
    double dt = 1.0 / tick_hz;
    double accumulator = 0.0;
    double lastTime = 0.0;

    enum class Mode { Menu, Game };
    Mode mode = Mode::Menu;

    // UI button definition
    struct Button {
        float x, y, w, h;
        bool hovered = false;
    };

    Button startBtn;

    // Convert window mouse coords to framebuffer pixel coords
    glm::vec2 mousePixels() {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);

        int ww, wh;
        glfwGetWindowSize(win, &ww, &wh);
        ww = std::max(1, ww);
        wh = std::max(1, wh);

        return {
            float(mx) * (float(fbw) / float(ww)),
            float(fbh) - float(my) * (float(fbh) / float(wh))
        };
    }

    // Simple AABB hit-test
    static bool inside(const glm::vec2& p, const Button& b) {
        return p.x >= b.x && p.x <= b.x + b.w &&
               p.y >= b.y && p.y <= b.y + b.h;
    }

    // Layout menu button dynamically
    void layoutMenu() {
        startBtn.w = 400.0f;
        startBtn.h = 100.0f;
        startBtn.x = (fbw - startBtn.w) * 0.5f;
        startBtn.y = (fbh * 0.45f) - (startBtn.h * 0.5f);
    }

    // Update viewport & UI layout
    void updateFrameBufferSize() {
        glfwGetFramebufferSize(win, &fbw, &fbh);
        fbw = std::max(1, fbw);
        fbh = std::max(1, fbh);
        glViewport(0, 0, fbw, fbh);
        layoutMenu();
    }

    // Convert mouse to grid coordinates
    std::optional<glm::ivec2> underMouse() {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);
        glm::vec2 wpos = cam.invProj(mx, my, fbw, fbh);

        int cx = int(std::floor(wpos.x));
        int cy = int(std::floor(wpos.y));

        if (!world.inWorldBounds(cx, cy)) return std::nullopt;
        return glm::ivec2(cx, cy);
    }

    // Keyboard input handler
    void onKey(int key, int action) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

        if (key == GLFW_KEY_ESCAPE)
            glfwSetWindowShouldClose(win, GLFW_TRUE);

        if (key == GLFW_KEY_SPACE)
            togglePause();

        if (key == GLFW_KEY_C) {
            world.clearAll();
        }

        if (key == GLFW_KEY_R) {
            for (int y = 0; y < world.h; ++y) {
                for (int x = 0; x < world.w; ++x) {
                    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
                    std::mt19937 generator(seed); 
                    std::uniform_int_distribution<int> distribution(0, 10); 
                    int random_number = distribution(generator);

                    if (random_number < 2) {
                        world.set(x, y, Cell::ALIVE);
                    } else {
                        world.set(x, y, Cell::DEAD);
                    }
                }
            }
        }

    }

    // Mouse input handler
    void onMouseButton(int button, int action) {
        if (button != GLFW_MOUSE_BUTTON_LEFT) return;

        leftDown = (action == GLFW_PRESS);

        if (mode == Mode::Menu && action == GLFW_PRESS) {
            if (inside(mousePixels(), startBtn)) {
                mode = Mode::Game;
                paused = false;
                initTiming();
            }
            return;
        }

        if (action == GLFW_PRESS) {
            hover = underMouse();
            if (hover)
                world.toggleCell(hover->x, hover->y);
        }
    }

    // Zoom camera at mouse position
    void onScroll(double yoffset) {
        double mx, my;
        glfwGetCursorPos(win, &mx, &my);

        glm::vec2 before = cam.invProj(mx, my, fbw, fbh);

        cam.zoom *= (yoffset > 0.0) ? 0.90f : 1.10f;
        cam.zoom = std::clamp(cam.zoom, 6.0f, 80.0f);

        glm::vec2 after = cam.invProj(mx, my, fbw, fbh);
        cam.center += (before - after);
    }

    // Toggle simulation pause
    void togglePause() {
        paused = !paused;
        initTiming();
    }

    // Reset timing accumulator
    void initTiming() {
        lastTime = glfwGetTime();
        accumulator = 0.0;
    }

    // Advance simulation one tick
    void tick() {
        world = findNextGen(world);
    }

    // Per-frame updates
    void frameUpdate() {
        updateFrameBufferSize();

        if (mode == Mode::Menu) {
            startBtn.hovered = inside(mousePixels(), startBtn);
            hover.reset();
            return;
        }

        hover = underMouse();

        // Smooth snap-back camera behavior
        if (cam.zoom > snapStart) {
            float t = (cam.zoom - snapStart) / (maxZoom - snapStart);
            t = std::clamp(t, 0.0f, 1.0f);
            cam.center = glm::mix(cam.center, homeCenter, t * 0.15f);
        }
    }

    // Render game scene
    void renderGame() {
        glClearColor(0.094f, 0.122f, 0.090f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glm::mat4 mvp = cam.proj(fbw, fbh);

        // Color palette
        glm::vec4 cAlive(0.377f,0.486f,0.361f,1);
        glm::vec4 cDead (0.094f,0.122f,0.090f,1);
        glm::vec4 cGrid (0.047f,0.058f,0.043f,1);
        glm::vec4 cHover(0.188f,0.243f,0.180f,0.75f);

        gameText.begin(fbw, fbh);

        // Draw cells
        for (int y = 0; y < world.h; ++y)
            for (int x = 0; x < world.w; ++x)
                ren.drawQuad(mvp, {x,y}, {1,1},
                    world.get(x,y)==Cell::ALIVE?cAlive:cDead);

        // Draw grid lines
        for (int x = 0; x <= world.w; ++x)
            ren.drawQuad(mvp,{float(x)-LINE_THICKNESS*0.5f,0},
                         {LINE_THICKNESS,float(world.h)},cGrid);

        for (int y = 0; y <= world.h; ++y)
            ren.drawQuad(mvp,{0,float(y)-LINE_THICKNESS*0.5f},
                         {float(world.w),LINE_THICKNESS},cGrid);

        // Draw hover highlight
        if (hover)
            ren.drawQuad(mvp,{hover->x,hover->y},{1,1},cHover);

        // UI overlay
        if (paused)
            gameText.draw("PAUSED",10,fbh-48,1,{0.477f,0.586f,0.461});

        gameText.drawCentered(
            "Click : Toggle   |   Scroll : Zoom   |   Space : Pause   |   R : Random Seed   |   C : Clear",
            fbw*0.5f,24,0.5,{0.477f,0.586f,0.461f});
    }

    // Render menu screen
    void renderMenu() {
        glm::mat4 mvp = glm::ortho(0.f,float(fbw),0.f,float(fbh),-1.f,1.f);

        menuText.begin(fbw, fbh);

        menuText.drawCentered("Automatons", fbw*0.5f, fbh*0.72f, 1.2f,
                              {0.377f,0.486f,0.361f});

        glm::vec4 btnCol = startBtn.hovered
            ? glm::vec4(0.188f,0.243f,0.180f,0.5f)
            : glm::vec4(0.188f,0.243f,0.180f,1);

        glm::vec4 outline(0.377f,0.486f,0.361f,1);

        ren.drawQuad(mvp,{startBtn.x-2,startBtn.y-2},
                     {startBtn.w+4,startBtn.h+4},outline);

        ren.drawQuad(mvp,{startBtn.x,startBtn.y},
                     {startBtn.w,startBtn.h},btnCol);

        menuText.drawCentered("Start",
            startBtn.x+startBtn.w*0.5f,
            startBtn.y+startBtn.h*0.5f-18,0.9f,
            {0.377f,0.486f,0.361f});

        menuText.drawCentered("Click Start to Begin",
            fbw*0.5f, fbh*0.20f, 0.6f,
            {0.377f,0.486f,0.361f});
    }

    // Master render dispatcher
    void render() {
        glClearColor(0.094f, 0.122f, 0.090f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (mode == Mode::Menu) renderMenu();
        else renderGame();
    }
};

// -------------------------------- GLFW CALLBACKS --------------------------------

// Forward GLFW input into App instance
static void keyCallBack(GLFWwindow* w, int key, int sc, int action, int mods) {
    if (auto* app = (App*)glfwGetWindowUserPointer(w))
        app->onKey(key, action);
}

static void mouseCallBack(GLFWwindow* w, int button, int action, int mods) {
    if (auto* app = (App*)glfwGetWindowUserPointer(w))
        app->onMouseButton(button, action);
}

static void scrollCallBack(GLFWwindow* w, double xoff, double yoff) {
    if (auto* app = (App*)glfwGetWindowUserPointer(w))
        app->onScroll(yoff);
}

// -------------------------------- MAIN --------------------------------

int main() {
    glfwInit();

    // Setup OpenGL 3.3 core context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(W_WIN, H_WIN, "Automatons", nullptr, nullptr);
    if (!window) return -1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    App app;
    app.win = window;
    glfwSetWindowUserPointer(window, &app);

    // Register callbacks
    glfwSetKeyCallback(window, keyCallBack);
    glfwSetMouseButtonCallback(window, mouseCallBack);
    glfwSetScrollCallback(window, scrollCallBack);

    // Init systems
    app.updateFrameBufferSize();
    app.ren.init();

    app.menuText.fontPixelHeight = 72;
    app.menuText.init("assets/PressStart2P-Regular.ttf", 72);

    app.gameText.fontPixelHeight = 24;
    app.gameText.init("assets/PressStart2P-Regular.ttf", 24);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    app.initTiming();

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        app.frameUpdate();

        double now = glfwGetTime();
        double frameTime = std::min(now - app.lastTime, 0.25);
        app.lastTime = now;

        if (!app.paused) {
            app.accumulator += frameTime;
            while (app.accumulator >= app.dt) {
                app.tick();
                app.accumulator -= app.dt;
            }
        } else {
            app.accumulator = 0;
        }

        app.render();
        glfwSwapBuffers(window);
    }

    // Cleanup
    app.ren.clear();
    app.menuText.shutdown();
    app.gameText.shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
