#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <string>
#include <unordered_map>
#include <iostream>

struct Glyph {
    GLuint texture = 0;     // glyph alpha texture
    glm::ivec2 size;        // glyph size in pixels
    glm::ivec2 bearing;     // offset from baseline to left/top in pixels
    GLuint advance = 0;     // advance.x in 1/64 pixels
};

class TextRenderer {
public:
    bool init(const char* ttfPath, int pixelHeight) {
        // --- compile shaders ---
        const char* vs = R"GLSL(
            #version 330 core
            layout (location = 0) in vec4 aPosUV; // xy = pos, zw = uv
            out vec2 vUV;
            uniform mat4 uProj;
            void main() {
                vUV = aPosUV.zw;
                gl_Position = uProj * vec4(aPosUV.xy, 0.0, 1.0);
            }
        )GLSL";

        const char* fs = R"GLSL(
            #version 330 core
            in vec2 vUV;
            out vec4 FragColor;
            uniform sampler2D uGlyph;
            uniform vec3 uColor;
            void main() {
                float a = texture(uGlyph, vUV).r;   // single-channel alpha
                FragColor = vec4(uColor, a);
            }
        )GLSL";

        prog = createProgram(vs, fs);
        if (!prog) return false;

        uProj  = glGetUniformLocation(prog, "uProj");
        uColor = glGetUniformLocation(prog, "uColor");

        // --- VAO/VBO for 6 vertices (2 tris) per glyph ---
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        // --- FreeType load ---
        FT_Library ft{};
        if (FT_Init_FreeType(&ft)) {
            std::cerr << "FT_Init_FreeType failed\n";
            return false;
        }

        FT_Face face{};
        if (FT_New_Face(ft, ttfPath, 0, &face)) {
            std::cerr << "FT_New_Face failed for: " << ttfPath << "\n";
            FT_Done_FreeType(ft);
            return false;
        }

        FT_Set_Pixel_Sizes(face, 0, (FT_UInt)pixelHeight);

        // Better alignment for 1-byte textures
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // Load ASCII set (you can extend later)
        for (unsigned char c = 32; c < 127; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
                std::cerr << "FT_Load_Char failed for char " << int(c) << "\n";
                continue;
            }

            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                face->glyph->bitmap.width,
                face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            Glyph g;
            g.texture = tex;
            g.size = { (int)face->glyph->bitmap.width, (int)face->glyph->bitmap.rows };
            g.bearing = { face->glyph->bitmap_left, face->glyph->bitmap_top };
            g.advance = (GLuint)face->glyph->advance.x; // 1/64 pixels

            glyphs.emplace((char)c, g);
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        return true;
    }

    void shutdown() {
        for (auto& [ch, g] : glyphs) {
            if (g.texture) glDeleteTextures(1, &g.texture);
        }
        glyphs.clear();
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        if (prog) glDeleteProgram(prog);
        vbo = vao = prog = 0;
    }

    // Screen-space orthographic: (0,0) bottom-left, (w,h) top-right
    void begin(int fbw, int fbh) {
        glUseProgram(prog);
        glm::mat4 P = glm::ortho(0.0f, (float)fbw, 0.0f, (float)fbh);
        glUniformMatrix4fv(uProj, 1, GL_FALSE, &P[0][0]);

        glActiveTexture(GL_TEXTURE0);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void draw(const std::string& text, float x, float y, float scale, glm::vec3 color) {
        glUseProgram(prog);
        glUniform3f(uColor, color.r, color.g, color.b);

        glBindVertexArray(vao);

        float penX = x;
        // y is baseline in pixels (common convention)
        for (char ch : text) {
            auto it = glyphs.find(ch);
            if (it == glyphs.end()) continue;
            const Glyph& g = it->second;

            float xpos = penX + g.bearing.x * scale;
            float ypos = y - (g.size.y - g.bearing.y) * scale;

            float w = g.size.x * scale;
            float h = g.size.y * scale;

            // 2 triangles, each vertex: x y u v
            float verts[6][4] = {
                { xpos,     ypos + h, 0.f, 0.f },
                { xpos,     ypos,     0.f, 1.f },
                { xpos + w, ypos,     1.f, 1.f },

                { xpos,     ypos + h, 0.f, 0.f },
                { xpos + w, ypos,     1.f, 1.f },
                { xpos + w, ypos + h, 1.f, 0.f }
            };

            glBindTexture(GL_TEXTURE_2D, g.texture);

            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            // advance is 1/64 pixels
            penX += (g.advance >> 6) * scale;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Convenience: center text around (cx, cy) where cy is baseline-centered-ish
    void drawCentered(const std::string& text, float cx, float cy, float scale, glm::vec3 color) {
        float w = measureWidth(text, scale);
        // For vertical centering: use font height as an approximation
        float h = (float)fontPixelHeight * scale;
        float x = cx - w * 0.5f;
        float y = cy - h * 0.25f; // tweak baseline so it looks visually centered
        draw(text, x, y, scale, color);
    }

    float measureWidth(const std::string& text, float scale) {
        float width = 0.0f;

        for (char c : text) {
            auto it = glyphs.find(c);
            if (it == glyphs.end()) continue;

            const Glyph& g = it->second;
            width += (g.advance >> 6) * scale;
        }

        return width;
    }
    
    std::vector<std::string> wrapText(
    const std::string& text,
    float maxWidth,
    float scale
    ) {
        std::vector<std::string> lines;

        std::string word, line;

        for (size_t i = 0; i <= text.size(); ++i) {
            char c = (i < text.size()) ? text[i] : ' ';

            if (c == ' ' || c == '\n' || i == text.size()) {
                std::string test = line;
                if (!test.empty()) test += " ";
                test += word;

                if (measureWidth(test, scale) > maxWidth || c == '\n') {
                    if (!line.empty()) lines.push_back(line);
                    line = word;
                } else {
                    if (!line.empty()) line += " ";
                    line += word;
                }

                word.clear();

                if (c == '\n') {
                    lines.push_back(line);
                    line.clear();
                }
            } else {
                word += c;
            }
        }

        if (!line.empty()) lines.push_back(line);
        return lines;
    }

    void drawCenteredWrapped(
        const std::string& text,
        float cx,
        float cy,
        float maxWidth,
        float scale,
        glm::vec3 color
    ) {
        auto lines = wrapText(text, maxWidth, scale);

        float lineHeight = fontPixelHeight * scale * 1.0f;
        float totalHeight = lineHeight * lines.size();

        float y = cy + totalHeight * 0.5f;

        for (const auto& line : lines) {
            float w = measureWidth(line, scale);
            float x = cx - w * 0.5f;

            draw(line, x, y, scale, color);
            y -= lineHeight;
        }
    }

    int fontPixelHeight = 48;

private:
    std::unordered_map<char, Glyph> glyphs;

    GLuint prog = 0, vao = 0, vbo = 0;
    GLint uProj = -1, uColor = -1;

    static GLuint compile(GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetShaderInfoLog(s, len, &len, log.data());
            std::cerr << "Shader compile error:\n" << log << "\n";
            glDeleteShader(s);
            return 0;
        }
        return s;
    }

    static GLuint createProgram(const char* vs, const char* fs) {
        GLuint v = compile(GL_VERTEX_SHADER, vs);
        GLuint f = compile(GL_FRAGMENT_SHADER, fs);
        if (!v || !f) return 0;

        GLuint p = glCreateProgram();
        glAttachShader(p, v);
        glAttachShader(p, f);
        glLinkProgram(p);

        glDeleteShader(v);
        glDeleteShader(f);

        GLint ok = 0;
        glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            GLint len = 0;
            glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
            std::string log(len, '\0');
            glGetProgramInfoLog(p, len, &len, log.data());
            std::cerr << "Program link error:\n" << log << "\n";
            glDeleteProgram(p);
            return 0;
        }
        return p;
    }
};