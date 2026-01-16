// OpenGL loader + window/context creation (GLFW)
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "shader.h" // Minimal shader wrapper (compile/link/use)

#include <iostream>
#include <string>
#include <array>
#include <string_view>

#include <limits>
#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include <glm/glm.hpp>                  // Math types
#include <glm/gtc/matrix_transform.hpp> // Transform helpers
#include <glm/gtc/type_ptr.hpp>         // Pointer access for uniforms

int winW = 1200;
int winH = 900;

// ---------------------------------   RULES   ---------------------------------

// Simple RGB color container
struct RGB
{
    float r{1.0f};
    float g{1.0f};
    float b{1.0f};
};

// L-system rewrite table indexed by ASCII value
using Rules = std::array<std::string_view, 256>;

// Bundle of parameters for a particular L-system + turtle behavior
struct RuleSet 
{
    const Rules* rule{};                            // Pointer to rewrite rules
    std::string_view axiom;                         // Starting string
    float turn_angle;                               // Default turn angle (radians)
    RGB interpol_color1 {0.980f, 0.984f, 0.741f};   // Base color
    RGB interpol_color2 {0.980f, 0.984f, 0.741f};   // Target color
    float c_scale {0.0f};                           // Color interpolation step
    float line_scale {1.0f};                        // Step length multiplier
    float thickness {2.0f};                         // Starting thickness
    float t_scale {0.0f};                           // Thickness scaling factor
};

// Rule definition helpers (constexpr so they can be compile-time initialized)
static constexpr Rules RuleA() 
{
    Rules rule{};
    rule['F'] = ">!F?<";
    rule['a'] = "F[+x]Fb";
    rule['b'] = "F[-y]Fa";
    rule['x'] = "a";
    rule['y'] = "b";

    return rule;
}

static constexpr Rules RuleB() 
{
    Rules rule{};
    rule['X'] = "F-[[X]+X]+F[+FX]-X";
    rule['F'] = "!FF?";

    return rule;
}

static constexpr Rules RuleC() 
{
    Rules rule{};
    rule['F'] = "F+F-F-FF+F+F-F";

    return rule;
}

static constexpr Rules RuleD() 
{
    Rules rule{};
    rule['F'] = "F+F-F-FF+F+F-F";

    return rule;
}

static constexpr Rules RuleE() 
{
    Rules rule{};
    rule['X'] = "X+YF++YF-FX--FXFX-YF+X";
    rule['Y'] = "-FX+YFYF++YF+FX--FX-YF";

    return rule;
}

// Concrete rule tables
constexpr Rules RULE_A = RuleA();
constexpr Rules RULE_B = RuleB();
constexpr Rules RULE_C = RuleC();
constexpr Rules RULE_D = RuleD();
constexpr Rules RULE_E = RuleE();

// Presets pairing rules with turtle parameters
constexpr RuleSet RULESETA {&RULE_A, "a", M_PI/4.0, {0.55f, 0.27f, 0.07f}, {0.1f, 0.7f, 0.1f}, 0.05f, 1.36f, 0.5f, 1.2f};
constexpr RuleSet RULESETB {&RULE_B, "X", M_PI/8.0, {0.980f, 0.984f, 0.741f}, {0.980f, 0.984f, 0.741f}, 0.0f, 1.0f, 0.2f, 1.7f};
constexpr RuleSet RULESETC {&RULE_C, "F+F+F+F", M_PI/2.0, {0.980f, 0.984f, 0.741f}, {0.980f, 0.984f, 0.741f}, 0.0f, 1.0f, 0.1f, 0.0f};
constexpr RuleSet RULESETD {&RULE_D, "F+F+F", (2.0 * M_PI)/3.0, {0.980f, 0.984f, 0.741f}, {0.980f, 0.984f, 0.741f}, 0.0f, 1.0f, 0.1f, 0.0f};
constexpr RuleSet RULESETE {&RULE_E, "X+X+X+X+X+X+X+X", M_PI/4.0, {0.980f, 0.984f, 0.741f}, {0.980f, 0.984f, 0.741f}, 0.0f, 1.0f, 0.1f, 1.1f};

// Currently selected ruleset
const RuleSet* activeRule = &RULESETA;


// ---------------------------------  TURTLE  ---------------------------------

// 2D position vector struct
struct Vec2 
{
    float x{};
    float y{};
};

// Line segment with per-segment color and thickness
struct Line_Segment 
{
    Vec2 a;
    Vec2 b;
    RGB color;
    float thickness;
};

// Saved turtle state for stack operations
struct TurtleState 
{
    Vec2 pos;
    float angle;
    float thickness;
    RGB color;
};

// Precomputed capacities for reserving vectors efficiently
struct TurtleReserves 
{
    size_t num_lines{0};
    size_t max_Depth{0};
};

// Axis-aligned bounds of the generated drawing
struct ImageBounds
{
    Vec2 upper;
    Vec2 lower;
    bool outofbounds = true; // true until first point arrives
};

// Turtle for interpreting L-system and storing generated segments
struct Turtle 
{
    Vec2 pos{0.0f, 0.0f};               // Current position
    float angle = M_PI / 2.0f;          // Heading (radians)
    float step_length = 1.0f;           // Current step length
    float turn_ang;                     // Turn angle per '+'/'-'
    float line_scale;                   // Scale factor for step length

    float thickness;                    // Current stroke thickness
    float thick_scale;                  // Thickness scale factor
    
    RGB color;                          // Current stroke color
    float color_step;                   // Color interpolation step

    std::vector<TurtleState> stack;     // State stack for '[' and ']'
    std::vector<Line_Segment> lines;    // Output segments for rendering

    ImageBounds bounds;                 // Drawing bounds for camera fit
    int genNum{0};                      // Generation number (for bookkeeping)
};

// Apply a preset ruleset to turtle rendering parameters
static void applyRuleSetToTurtle(Turtle& t, const RuleSet& rs)
{
    t.turn_ang    = rs.turn_angle;
    t.line_scale  = rs.line_scale;

    t.thickness   = rs.thickness;
    t.thick_scale = rs.t_scale;

    t.color       = rs.interpol_color1;
    t.color_step  = rs.c_scale;

    // Reset step length for consistent scaling per ruleset
    t.step_length = 1.0f;
}

// Reset turtle state and clear generated geometry
void resetTurtle(Turtle& t) 
{
    t.pos = {0.0f, 0.0f};
    t.angle = M_PI / 2.0f;
    t.stack.clear();
    t.lines.clear();
}

// Estimate how many line segments and stack depth this generation will need
TurtleReserves estimateGeneartion(const std::string& generation)
{
    TurtleReserves res{};
    size_t depth{0};

    for (unsigned char uc : generation) {
        char c = static_cast<char>(uc);

        switch(c) {
            case 'F': res.num_lines++; break;                          // 'F' draws a segment
            case '[': depth++; if (depth > res.max_Depth) res.max_Depth = depth; break; // stack push
            case ']': if(depth > 0) depth--;                           // stack pop
        }
    }   

    return res;
}

// Expand bounds to include a point
static void computeBounds(ImageBounds& b, const Vec2& pos)
{
    if (b.outofbounds) {
        b.lower = b.upper = pos;
        b.outofbounds = false;
        return;
    }

    b.lower.x = std::min({ b.lower.x, pos.x});
    b.lower.y = std::min({ b.lower.y, pos.y});
    b.upper.x = std::max({ b.upper.x, pos.x});
    b.upper.y = std::max({ b.upper.y, pos.y});

}

// Symbol interpreter for converting characters into turtle operations
class SymbolInterpreter 
{
public:
    // Dispatch a single symbol into a turtle action
    void interpret(const char c, Turtle& t) const 
    {
        switch(c) {
            case 'F': forward_draw(t); break;       // draw forward
            case 'f': forward_move(t); break;       // move forward (no draw)
            case '+': rot_left(t); break;           // turn left
            case '-': rot_right(t); break;          // turn right
            case '[': push(t); break;               // push state
            case ']': pop(t); break;                // pop state
            case '>': multiply(t); break;           // scale step up
            case '<': divide(t); break;             // scale step down
            case '!': thicker(t); break;            // increase thickness
            case '?': thinner(t); break;            // decrease thickness
            case '^': saturate_color(t); break;     // lerp toward color2
            case '_': desaturate_color(t); break;   // lerp toward color1
            default: break;
        }
    }

private:
    // Draw a segment forward and record it
    static void forward_draw(Turtle& t) {
        Vec2 start = t.pos;
        t.pos.x += std::cos(t.angle) * t.step_length;
        t.pos.y += std::sin(t.angle) * t.step_length;

        Line_Segment seg;
        seg.a = start;
        seg.b = t.pos;
        seg.thickness = t.thickness;
        seg.color = t.color;
        t.lines.push_back(seg);

        computeBounds(t.bounds, start);
        computeBounds(t.bounds, t.pos);
    }
    
    // Move forward without drawing
    static void forward_move(Turtle& t) {
        t.pos.x += std::cos(t.angle) * t.step_length;
        t.pos.y += std::sin(t.angle) * t.step_length;
    }

    // Rotate left by the current turn angle
    static void rot_left(Turtle& t) {
        t.angle += t.turn_ang;
    }

    // Rotate right by the current turn angle
    static void rot_right(Turtle& t) {
        t.angle -= t.turn_ang;
    }

    // Push current turtle state onto stack
    static void push(Turtle& t) {
    t.stack.push_back({t.pos, t.angle, t.thickness, t.color});
    }

    // Pop turtle state; reset thickness/color to active ruleset baseline
    static void pop(Turtle& t) {
        if (!t.stack.empty()) {
            TurtleState s = t.stack.back();
            t.stack.pop_back();
            t.pos = s.pos;
            t.angle = s.angle;
            t.thickness = activeRule->thickness;
            t.color = activeRule->interpol_color1;
        }
    }

    // Multiply step length by ruleset scale
    static void multiply(Turtle& t) {
        t.step_length *= t.line_scale;
    }

    // Divide step length by ruleset scale
    static void divide(Turtle& t) {
        t.step_length /= t.line_scale;
    }

    // Scale thickness up
    static void thicker(Turtle& t) 
    { 
        t.thickness *= t.thick_scale; 
    }

    // Scale thickness down
    static void thinner(Turtle& t) 
    {
         t.thickness /= t.thick_scale; 

    }

    // Lerp current color toward color2
    static void saturate_color(Turtle& t)
    {
        RGB color2 = activeRule->interpol_color2;
        float s = t.color_step;

        t.color.r += (color2.r - t.color.r) * s;
        t.color.g += (color2.g - t.color.g) * s;
        t.color.b += (color2.b - t.color.b) * s;
    }

    // Lerp current color back toward color1
    static void desaturate_color(Turtle& t)
    {
        RGB color1 = activeRule->interpol_color1;
        float s = t.color_step;

        t.color.r += (color1.r - t.color.r) * s;
        t.color.g += (color1.g - t.color.g) * s;
        t.color.b += (color1.b - t.color.b) * s;
    }

};

// Append one vertex worth of position + color into the interleaved buffer
static inline void pushVertex(std::vector<float>& verts, float x, float y, float z, float r, float g, float b)
{
    verts.push_back(x); verts.push_back(y); verts.push_back(z);
    verts.push_back(r); verts.push_back(g); verts.push_back(b);
}

// Convert thick line segments into a quad mesh (two triangles per segment)
void lineSegmentsToVerts(const std::vector<Line_Segment>& segments, std::vector<float>& verts, std::vector<uint32_t>& indices, int generation)
{
    verts.clear();
    indices.clear();
    if (segments.empty()) return;

    // 4 vertices/segment, 6 indices/segment
    verts.reserve(segments.size() * 4 * 6);      // 4 verts * 6 floats
    indices.reserve(segments.size() * 6);

    uint32_t base = 0;

    for (const Line_Segment& seg : segments)
    {
        const RGB c = seg.color;

        float hw = seg.thickness;   // half-width used for quad extrusion

        const float ax = seg.a.x, ay = seg.a.y;
        const float bx = seg.b.x, by = seg.b.y;

        float dx = bx - ax;
        float dy = by - ay;
        float len2 = dx*dx + dy*dy;

        // Skip degenerate segments
        if (len2 < 1e-12f) continue;

        // Normalize direction and compute perpendicular
        float invLen = 1.0f / std::sqrt(len2);
        dx *= invLen;
        dy *= invLen;

        float nx = -dy;
        float ny =  dx;

        // Build quad corners around the segment centerline
        float v0x = ax + nx * hw;  float v0y = ay + ny * hw;
        float v1x = ax - nx * hw;  float v1y = ay - ny * hw;
        float v2x = bx - nx * hw;  float v2y = by - ny * hw;
        float v3x = bx + nx * hw;  float v3y = by + ny * hw;

        pushVertex(verts, v0x, v0y, 0.0f, c.r, c.g, c.b);
        pushVertex(verts, v1x, v1y, 0.0f, c.r, c.g, c.b);
        pushVertex(verts, v2x, v2y, 0.0f, c.r, c.g, c.b);
        pushVertex(verts, v3x, v3y, 0.0f, c.r, c.g, c.b);

        // Two triangles per segment quad
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base + 2);
        indices.push_back(base + 3);
        indices.push_back(base + 0);

        base += 4;
    }
}

// ---------------------------------  CAMERA  ---------------------------------

// Simple 2D camera for ortho view + rotation
struct Camera2D {
    glm::vec2 center{0.0f, 0.0f}; 
    float halfHeight = 10.0f;     
    float rotation = 0.0f;
};

// Build orthographic MVP matrix for current window aspect ratio
glm::mat4 makeCameraMVP(const Camera2D& cam)
{
     float aspect = float(winW) / float(winH);
    float halfW = cam.halfHeight * aspect;
    float halfH = cam.halfHeight;

    glm::mat4 proj = glm::ortho(-halfW, halfW,
                                -halfH, halfH,
                                -1.0f, 1.0f);

    glm::mat4 view(1.0f);
    view = glm::rotate(view, cam.rotation, glm::vec3(0, 0, 1));
    view = glm::translate(view, glm::vec3(-cam.center, 0.0f));

    return proj * view;
}

// Fit camera center/zoom to encompass drawing bounds with padding
void fitCameraToBounds(Camera2D& cam, const ImageBounds& b)
{
    float minX = b.lower.x, maxX = b.upper.x;
    float minY = b.lower.y, maxY = b.upper.y;

    float w = maxX - minX;
    float h = maxY - minY;
    if (w <= 0.0f) w = 1.0f;
    if (h <= 0.0f) h = 1.0f;

    float pad = 0.10f;
    minX -= w * pad; maxX += w * pad;
    minY -= h * pad; maxY += h * pad;

    cam.center = glm::vec2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);

    float aspect = float(winW) / float(winH);
    float boxW = (maxX - minX);
    float boxH = (maxY - minY);

    float halfHeightFromHeight = boxH * 0.5f;
    float halfHeightFromWidth  = (boxW * 0.5f) / aspect;
    cam.halfHeight = std::max(halfHeightFromHeight, halfHeightFromWidth);
}

// --------------------------------- RENDERING ---------------------------------

// Advance one L-system generation: interpret drawing ops + rewrite symbols
std::string advanceLsystem(std::string generation, const Rules& rules, Turtle& t)
{
    SymbolInterpreter interpreter;

    auto genSize = estimateGeneartion(generation);

    // Clear per-generation output
    t.lines.clear();
    t.stack.clear();
    t.bounds.outofbounds = true;

    // Reserve to reduce reallocations
    size_t needed = t.lines.size() + genSize.num_lines;
    if (t.lines.capacity() < needed) t.lines.reserve(needed);
    if (t.stack.capacity() < genSize.max_Depth) t.stack.reserve(genSize.max_Depth);

    std::string next_generation;
    next_generation.reserve(generation.size() * 2);

    for (unsigned char uc : generation) {
        char c = (char)uc;
        
        // Interpret only supported turtle commands
        switch (c) {
            case 'F': case 'f': case '+': case '-':
            case '[': case ']': case '>': case '<':
            case '!': case '?': case '^': case '_':
                interpreter.interpret(c, t);
                break;
            default:
                break;
        }

        // Rewrite symbol using rules table (if any)
        std::string_view rhs = rules[uc];

        if (!rhs.empty()) next_generation.append(rhs);
        else next_generation.push_back(c);
    }
    return next_generation;
}

// GPU-side mesh for thick line quads (dynamic buffers)
struct LineMesh 
{
    unsigned int VAO = 0;
    unsigned int VBO = 0;
    unsigned int EBO = 0;

    size_t vboCapacity = 0;
    size_t eboCapacity = 0;

    int vertCount = 0;
    int indexCount = 0;
};

// High-level L-system container (mesh + CPU geometry + turtle state)
struct LSys 
{
    LineMesh mesh;
    std::vector<float> verts;
    std::vector<uint32_t> indices;
    Turtle t;
    std::string lsystem;
    int generation{0};
};

// Create VAO/VBO/EBO and configure vertex layout (pos + color)
LineMesh createLineMesh(size_t vboCapacityBytes, size_t eboCapacityBytes)
{
    LineMesh mesh;
    mesh.vboCapacity = vboCapacityBytes;
    mesh.eboCapacity = eboCapacityBytes;

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);
    glGenBuffers(1, &mesh.EBO);

    glBindVertexArray(mesh.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, mesh.vboCapacity, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.eboCapacity, nullptr, GL_DYNAMIC_DRAW);

    // layout(location=0): vec3 position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // layout(location=1): vec3 color
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    return mesh;
}

// Upload CPU vertex/index data; grow buffers if needed
void uploadLineMesh(LineMesh& mesh, const std::vector<float>& verts, const std::vector<uint32_t>& indices)
{
    const size_t vboBytes = verts.size() * sizeof(float);
    const size_t eboBytes = indices.size() * sizeof(uint32_t);

    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    if (mesh.vboCapacity < vboBytes) {
        mesh.vboCapacity = vboBytes * 2;
        glBufferData(GL_ARRAY_BUFFER, mesh.vboCapacity, nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, vboBytes, verts.data());
    
    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.EBO);
    if (mesh.eboCapacity < eboBytes) {
        mesh.eboCapacity = eboBytes * 2;
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.eboCapacity, nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, eboBytes, indices.data());

    mesh.vertCount  = (int)(verts.size() / 6);
    mesh.indexCount = (int)(indices.size());
}

// Rebuild mesh for current generation (segments -> verts/indices -> GPU upload)
void rebuildForGeneration(LineMesh& mesh,
                          const std::vector<Line_Segment>& segments,
                          std::vector<float>& verts,
                          std::vector<uint32_t>& indices,
                          int generation)
{
    size_t lineCount = segments.size();

    verts.clear();

    // Conservative reserve for thick quad geometry
    verts.reserve(lineCount * 36);

    lineSegmentsToVerts(segments, verts, indices, generation);

    uploadLineMesh(mesh, verts, indices);

}

// GLFW resize callback: update viewport + globals
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
    {
        winW = width;
        winH = height;
        glViewport(0, 0, width, height);
    }  


// GLFW scroll callback: zoom camera in/out
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    auto* cam = static_cast<Camera2D*>(glfwGetWindowUserPointer(window));
    if (!cam) return;

    float zoomFactor = 0.9f;
    if (yoffset > 0) cam->halfHeight *= zoomFactor;
    if (yoffset < 0) cam->halfHeight /= zoomFactor;

    cam->halfHeight = std::clamp(cam->halfHeight, 0.0001f, 1e6f);
}

// Handle keyboard/mouse input and update camera + L-system state
void processInput(GLFWwindow *window, LSys& LSystem, Shader& shader, glm::mat4& uMVP, Camera2D& cam)
{
    // Exit on ESC
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, true);
    }
    
    // Right-mouse drag panning
    static bool panning = false;
    static double lastX = 0.0, lastY = 0.0;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        if (!panning) {
            panning = true;
            lastX = x; lastY = y;
        } else {
            double dx = x - lastX;
            double dy = y - lastY;
            lastX = x; lastY = y;

            // Convert pixel drag to world-space translation
            float aspect = float(winW) / float(winH);
            float halfW = cam.halfHeight * aspect;
            float halfH = cam.halfHeight;

            float worldPerPixelX = (2.0f * halfW) / float(winW);
            float worldPerPixelY = (2.0f * halfH) / float(winH);

            // Respect camera rotation when panning
            float c = std::cos(cam.rotation);
            float s = std::sin(cam.rotation);

            float wx = -(dx * worldPerPixelX) * c + ( dy * worldPerPixelY) * s;
            float wy = (dx * worldPerPixelX) * s + ( dy * worldPerPixelY) * c;

            cam.center.x += wx;
            cam.center.y += wy;
        }
    }
    else {
        panning = false;
    }

    // Continuous rotation (Q/E) using frame time delta
    float rotSpeed = 3.0f; // radians per second

    static double lastTime = glfwGetTime();
    double now = glfwGetTime();
    float dt = float(now - lastTime);
    lastTime = now;

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cam.rotation += rotSpeed * dt;

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cam.rotation -= rotSpeed * dt;

    // Edge-trigger generation advance on 'A'
    static bool aWasDown = false;
    bool aIsDown = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;

    // Switch ruleset on 'N' and reset L-system to axiom
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
    {
        char selection;

        std::cout << "Select a New Rule : {A | B | C | D | E} : ";
        std::cin >> selection;

        switch (selection)
        {
            case 'A': activeRule = &RULESETA; break;
            case 'B': activeRule = &RULESETB; break;
            case 'C': activeRule = &RULESETC; break;
            case 'D': activeRule = &RULESETD; break;
            case 'E': activeRule = &RULESETE; break;

            default: break;
        }

        const std::string_view axiom = activeRule->axiom;
        LSystem.lsystem = std::string{axiom};
        LSystem.generation = 0;
        
        applyRuleSetToTurtle(LSystem.t, *activeRule);
    }

    // Advance one generation and rebuild mesh
    if (aIsDown && !aWasDown)
    {
        LSystem.t.genNum = LSystem.generation;

        resetTurtle(LSystem.t);

        LSystem.lsystem = advanceLsystem(LSystem.lsystem, *activeRule->rule, LSystem.t);

        rebuildForGeneration(LSystem.mesh, LSystem.t.lines, LSystem.verts, LSystem.indices, LSystem.generation);

        fitCameraToBounds(cam, LSystem.t.bounds);
        
        LSystem.generation++;

        std::cout << "Generation: " << LSystem.generation << '\n';
        std::cout << "Lines: " << LSystem.t.lines.size() << '\n';

    } 

    aWasDown = aIsDown;

    // Update MVP for current camera state
    uMVP = makeCameraMVP(cam);
}

int main()
{
    // GLFW init + OpenGL context config (3.3 core)
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window and context
    GLFWwindow* window = glfwCreateWindow(winW, winH, "L Systems", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Load OpenGL function pointers via GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }    

    glViewport(0, 0, winW, winH);

    // Register resize callback
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);  

    // Load/compile shaders and grab uniform location
    Shader myShader("src/shader.vs", "src/shader.fs");
    const GLint uMVP_loc = glGetUniformLocation(myShader.ID, "uMVP");

    // Create dynamic line mesh buffers
    LineMesh mesh = createLineMesh(100000 * sizeof(float), 100000 * sizeof(uint32_t));
    std::vector<float> verts;
    glm::mat4 uMVP(1.0f);

    // Initialize turtle and L-system state from current ruleset axiom
    Turtle t;

    applyRuleSetToTurtle(t, *activeRule);

    const std::string_view axiom = activeRule->axiom;
    LSys LSystem{mesh, {}, {}, t, std::string{axiom}};

    // Setup camera controls via GLFW user pointer
    Camera2D cam;
    glfwSetWindowUserPointer(window, &cam);
    glfwSetScrollCallback(window, scroll_callback);

    // Main loop: input -> render
    while(!glfwWindowShouldClose(window))
    {
        processInput(window, LSystem, myShader, uMVP, cam);

        glClearColor(0.122f, 0.227f, 0.294f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        myShader.use();
        glUniformMatrix4fv(uMVP_loc, 1, GL_FALSE, glm::value_ptr(uMVP));

        glBindVertexArray(LSystem.mesh.VAO);
        glDrawElements(GL_TRIANGLES, LSystem.mesh.indexCount, GL_UNSIGNED_INT, (void*)0);

        glfwSwapBuffers(window);
        glfwPollEvents();   
    }

    // Cleanup GPU resources
    glDeleteVertexArrays(1, &LSystem.mesh.VAO);
    glDeleteBuffers(1, &LSystem.mesh.VBO);
    glDeleteBuffers(1, &LSystem.mesh.EBO);

    glfwTerminate();
    return 0;
}
