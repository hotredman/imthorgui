#include "imgui_ext/renderer.h"
#include "imgui_ext/recorder.h"
#include "thorvg.h"

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#endif

#include <GL/gl.h>
#include <GLFW/glfw3.h>

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

namespace ImGuiExt {

// Typedefs for core OpenGL 3 functions loaded via glfwGetProcAddress
typedef void (APIENTRY *PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC) (GLsizei n, const GLuint *arrays);
typedef void (APIENTRY *PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRY *PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRY *PFNGLBUFFERDATAPROC) (GLenum target, ptrdiff_t size, const void *data, GLenum usage);
typedef void (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const char* const *string, const GLint *length);
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, char *infoLog);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC) (void);
typedef void (APIENTRY *PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRY *PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, char *infoLog);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRY *PFNGLDELETEPROGRAMPROC) (GLuint program);
typedef void (APIENTRY *PFNGLDELETESHADERPROC) (GLuint shader);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const char *name);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRY *PFNGLACTIVETEXTUREPROC) (GLenum texture);

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER   0x8B31
#define GL_COMPILE_STATUS  0x8B81
#define GL_LINK_STATUS     0x8B82
#define GL_ARRAY_BUFFER    0x8892
#define GL_STATIC_DRAW     0x88E4
#define GL_TEXTURE0        0x84C0

class ThorVGRenderer : public IRenderer {
public:
    ThorVGRenderer() = default;
    ~ThorVGRenderer() override { Shutdown(); }

    bool Init(int width, int height) override;
    void Shutdown() override;
    void Resize(int width, int height) override;

    void BeginFrame() override;
    void EndFrame() override;

    void PushClip(float x, float y, float w, float h) override;
    void PopClip() override;

    void FillRect(float x, float y, float w, float h, Color col, float rounding = 0.0f) override;
    void StrokeRect(float x, float y, float w, float h, Color col, float thickness = 1.0f, float rounding = 0.0f) override;
    void FillRectLinearGradient(float x, float y, float w, float h, float x1, float y1, Color c1, float x2, float y2, Color c2, float rounding = 0.0f) override;

    void FillCircle(float cx, float cy, float radius, Color col) override;
    void StrokeCircle(float cx, float cy, float radius, Color col, float thickness = 1.0f) override;

    void FillNgon(float cx, float cy, float radius, int segments, Color col) override;
    void StrokeNgon(float cx, float cy, float radius, int segments, Color col, float thickness = 1.0f) override;

    void FillEllipse(float cx, float cy, float rx, float ry, float rot, Color col) override;
    void StrokeEllipse(float cx, float cy, float rx, float ry, float rot, Color col, float thickness = 1.0f) override;

    void StrokeLine(float x1, float y1, float x2, float y2, Color col, float thickness = 1.0f) override;
    void StrokePolyline(const ImVec2* points, int count, Color col, float thickness = 1.0f, bool closed = false) override;
    void FillConvexPoly(const ImVec2* points, int count, Color col) override;

    void StrokeBezierCubic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, Color col, float thickness = 1.0f) override;
    void StrokeBezierQuadratic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, Color col, float thickness = 1.0f) override;

    bool LoadFont(const char* name, const void* data, size_t size) override;
    bool LoadFontFile(const char* path) override;
    void DrawText(const char* text_utf8, float x, float y, Color col, const char* font_name = nullptr, float font_size = 0.0f, float wrap_width = 0.0f) override;
    bool MeasureGlyph(const char* utf8_char, const char* font_name, float font_size, GlyphMetricsInfo& out_metrics) override;
    bool MeasureText(const char* text_utf8, const char* font_name, float font_size, float& out_w, float& out_h) override;

    void DrawImage(const uint32_t* pixels, int src_w, int src_h, float dst_x, float dst_y, float dst_w, float dst_h, Color tint = Color(255, 255, 255, 255), float rounding = 0.0f) override;

    void DrawFallbackMesh(const ImDrawList* dl, const DrawCommand& cmd) override;

    void RenderDrawData(ImDrawData* draw_data) override;

    const uint32_t* GetPixelBuffer() const override { return m_pixels.data(); }
    int GetWidth() const override { return m_width; }
    int GetHeight() const override { return m_height; }
    void PresentGL() override;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<uint32_t> m_pixels;
    tvg::SwCanvas* m_canvas = nullptr;
    std::vector<ImVec4> m_clip_stack;
    std::string m_default_font_name = "default";
    float m_font_scale_ratio = 1.7731f; // TrueType EM-to-Pixel height calibration ratio
    bool m_font_loaded = false;
    ImVec2 m_display_scale = ImVec2(1.0f, 1.0f);
    ImVec2 m_display_pos = ImVec2(0.0f, 0.0f);

    // OpenGL presentation
    GLuint m_gl_texture = 0;
    GLuint m_gl_vao = 0;
    GLuint m_gl_vbo = 0;
    GLuint m_gl_shader = 0;
    bool m_gl_initialized = false;

    // GL function pointers
    PFNGLGENVERTEXARRAYSPROC m_glGenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC m_glBindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC m_glDeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC m_glGenBuffers = nullptr;
    PFNGLBINDBUFFERPROC m_glBindBuffer = nullptr;
    PFNGLDELETEBUFFERSPROC m_glDeleteBuffers = nullptr;
    PFNGLBUFFERDATAPROC m_glBufferData = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC m_glEnableVertexAttribArray = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC m_glVertexAttribPointer = nullptr;
    PFNGLCREATESHADERPROC m_glCreateShader = nullptr;
    PFNGLSHADERSOURCEPROC m_glShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC m_glCompileShader = nullptr;
    PFNGLGETSHADERIVPROC m_glGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC m_glGetShaderInfoLog = nullptr;
    PFNGLCREATEPROGRAMPROC m_glCreateProgram = nullptr;
    PFNGLATTACHSHADERPROC m_glAttachShader = nullptr;
    PFNGLLINKPROGRAMPROC m_glLinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC m_glGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC m_glGetProgramInfoLog = nullptr;
    PFNGLUSEPROGRAMPROC m_glUseProgram = nullptr;
    PFNGLDELETEPROGRAMPROC m_glDeleteProgram = nullptr;
    PFNGLDELETESHADERPROC m_glDeleteShader = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC m_glGetUniformLocation = nullptr;
    PFNGLUNIFORM1IPROC m_glUniform1i = nullptr;
    PFNGLACTIVETEXTUREPROC m_glActiveTexture = nullptr;

    void ApplyClip(tvg::Paint* paint);
    void FlushCanvas();
    void InitGLResources();
    void RasterizeFallbackTriangles(const ImDrawList* dl, const DrawCommand& cmd);
};

void ThorVGRenderer::ApplyClip(tvg::Paint* paint) {
    if (!paint || m_clip_stack.empty()) return;
    const ImVec4& cr = m_clip_stack.back();
    float w = (std::max)(0.0f, cr.z - cr.x);
    float h = (std::max)(0.0f, cr.w - cr.y);
    auto clip_shape = tvg::Shape::gen();
    clip_shape->appendRect(cr.x, cr.y, w, h);
    paint->clip(clip_shape);
}

void ThorVGRenderer::FlushCanvas() {
    if (m_canvas) {
        m_canvas->draw(false);
        m_canvas->sync();
        m_canvas->remove();
    }
}

bool ThorVGRenderer::Init(int width, int height) {
    m_width = width;
    m_height = height;

    tvg::Initializer::init(0);

    m_pixels.resize(width * height, 0);
    m_canvas = tvg::SwCanvas::gen(tvg::EngineOption::Default);
    if (!m_canvas) {
        std::cerr << "[ThorVG] Failed to create SwCanvas\n";
        return false;
    }
    m_canvas->target(m_pixels.data(), width, width, height, tvg::ColorSpace::ABGR8888S);

    // Try loading a high quality system font for ThorVG
    const char* font_candidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
        nullptr
    };

    for (int i = 0; font_candidates[i] != nullptr; ++i) {
        if (LoadFontFile(font_candidates[i])) {
            m_default_font_name = font_candidates[i];
            m_font_loaded = true;
            break;
        }
    }

    return true;
}

void ThorVGRenderer::Shutdown() {
    FlushCanvas();
    if (m_canvas) {
        delete m_canvas;
        m_canvas = nullptr;
    }
    tvg::Initializer::term();

    if (m_gl_texture) {
        glDeleteTextures(1, &m_gl_texture);
        m_gl_texture = 0;
    }
    if (m_glDeleteProgram && m_gl_shader) {
        m_glDeleteProgram(m_gl_shader);
        m_gl_shader = 0;
    }
    if (m_glDeleteBuffers && m_gl_vbo) {
        m_glDeleteBuffers(1, &m_gl_vbo);
        m_gl_vbo = 0;
    }
    if (m_glDeleteVertexArrays && m_gl_vao) {
        m_glDeleteVertexArrays(1, &m_gl_vao);
        m_gl_vao = 0;
    }
    m_gl_initialized = false;
}

void ThorVGRenderer::Resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width != m_width || height != m_height) {
        m_width = width;
        m_height = height;
        m_pixels.resize(width * height, 0);
        if (m_canvas) {
            m_canvas->target(m_pixels.data(), width, width, height, tvg::ColorSpace::ABGR8888S);
        }
        if (m_gl_texture) {
            glBindTexture(GL_TEXTURE_2D, m_gl_texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
    }
}

void ThorVGRenderer::BeginFrame() {
    m_clip_stack.clear();
    uint32_t clear_col = 0xFF241F1F; // Matches OpenGL glClearColor(0.12f, 0.12f, 0.14f, 1.0f) rounded to UNORM8
    std::fill(m_pixels.begin(), m_pixels.end(), clear_col);
}

void ThorVGRenderer::EndFrame() {
    FlushCanvas();
}

void ThorVGRenderer::PushClip(float x, float y, float w, float h) {
    ImVec4 cr(x, y, x + w, y + h);
    if (!m_clip_stack.empty()) {
        const ImVec4& prev = m_clip_stack.back();
        cr.x = (std::max)(cr.x, prev.x);
        cr.y = (std::max)(cr.y, prev.y);
        cr.z = (std::min)(cr.z, prev.z);
        cr.w = (std::min)(cr.w, prev.w);
    }
    cr.z = (std::max)(cr.x, cr.z);
    cr.w = (std::max)(cr.y, cr.w);
    m_clip_stack.push_back(cr);
}

void ThorVGRenderer::PopClip() {
    if (!m_clip_stack.empty()) {
        m_clip_stack.pop_back();
    }
}

void ThorVGRenderer::FillRect(float x, float y, float w, float h, Color col, float rounding) {
    if (col.a == 0 || w <= 0 || h <= 0) return;
    auto shape = tvg::Shape::gen();
    shape->appendRect(x, y, w, h, rounding, rounding);
    shape->fill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::StrokeRect(float x, float y, float w, float h, Color col, float thickness, float rounding) {
    if (col.a == 0 || w <= 0 || h <= 0) return;
    auto shape = tvg::Shape::gen();
    shape->appendRect(x, y, w, h, rounding, rounding);
    shape->strokeWidth(thickness);
    shape->strokeFill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::FillRectLinearGradient(float x, float y, float w, float h, float x1, float y1, Color c1, float x2, float y2, Color c2, float rounding) {
    if (w <= 0 || h <= 0) return;
    auto shape = tvg::Shape::gen();
    shape->appendRect(x, y, w, h, rounding, rounding);

    auto grad = tvg::LinearGradient::gen();
    grad->linear(x1, y1, x2, y2);
    tvg::Fill::ColorStop stops[2] = {
        { 0.0f, c1.r, c1.g, c1.b, c1.a },
        { 1.0f, c2.r, c2.g, c2.b, c2.a }
    };
    grad->colorStops(stops, 2);
    shape->fill(grad);

    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::FillCircle(float cx, float cy, float radius, Color col) {
    if (col.a == 0 || radius <= 0) return;
    auto shape = tvg::Shape::gen();
    shape->appendCircle(cx, cy, radius, radius);
    shape->fill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::StrokeCircle(float cx, float cy, float radius, Color col, float thickness) {
    if (col.a == 0 || radius <= 0) return;
    auto shape = tvg::Shape::gen();
    shape->appendCircle(cx, cy, radius, radius);
    shape->strokeWidth(thickness);
    shape->strokeFill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::FillNgon(float cx, float cy, float radius, int segments, Color col) {
    if (col.a == 0 || radius <= 0 || segments < 3) return;
    auto shape = tvg::Shape::gen();
    float step = 2.0f * 3.1415926535f / (float)segments;
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        float px = cx + radius * std::cos(angle);
        float py = cy + radius * std::sin(angle);
        if (i == 0) shape->moveTo(px, py);
        else shape->lineTo(px, py);
    }
    shape->close();
    shape->fill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::StrokeNgon(float cx, float cy, float radius, int segments, Color col, float thickness) {
    if (col.a == 0 || radius <= 0 || segments < 3) return;
    auto shape = tvg::Shape::gen();
    float step = 2.0f * 3.1415926535f / (float)segments;
    for (int i = 0; i < segments; ++i) {
        float angle = i * step;
        float px = cx + radius * std::cos(angle);
        float py = cy + radius * std::sin(angle);
        if (i == 0) shape->moveTo(px, py);
        else shape->lineTo(px, py);
    }
    shape->close();
    shape->strokeWidth(thickness);
    shape->strokeFill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::FillEllipse(float cx, float cy, float rx, float ry, float rot, Color col) {
    if (col.a == 0 || rx <= 0 || ry <= 0) return;
    auto shape = tvg::Shape::gen();
    shape->appendCircle(0.0f, 0.0f, rx, ry);
    if (rot != 0.0f) {
        shape->rotate(rot * 180.0f / 3.1415926535f);
    }
    shape->translate(cx, cy);
    shape->fill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::StrokeEllipse(float cx, float cy, float rx, float ry, float rot, Color col, float thickness) {
    if (col.a == 0 || rx <= 0 || ry <= 0) return;
    auto shape = tvg::Shape::gen();
    shape->appendCircle(0.0f, 0.0f, rx, ry);
    if (rot != 0.0f) {
        shape->rotate(rot * 180.0f / 3.1415926535f);
    }
    shape->translate(cx, cy);
    shape->strokeWidth(thickness);
    shape->strokeFill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::StrokeLine(float x1, float y1, float x2, float y2, Color col, float thickness) {
    if (col.a == 0) return;
    auto shape = tvg::Shape::gen();
    shape->moveTo(x1, y1);
    shape->lineTo(x2, y2);
    shape->strokeWidth(thickness);
    shape->strokeCap(tvg::StrokeCap::Round);
    shape->strokeFill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::StrokePolyline(const ImVec2* points, int count, Color col, float thickness, bool closed) {
    if (col.a == 0 || count < 2) return;
    auto shape = tvg::Shape::gen();
    shape->moveTo(points[0].x, points[0].y);
    for (int i = 1; i < count; ++i) {
        shape->lineTo(points[i].x, points[i].y);
    }
    if (closed) shape->close();
    shape->strokeWidth(thickness);
    shape->strokeCap(tvg::StrokeCap::Round);
    shape->strokeJoin(tvg::StrokeJoin::Round);
    shape->strokeFill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::FillConvexPoly(const ImVec2* points, int count, Color col) {
    if (col.a == 0 || count < 3) return;
    auto shape = tvg::Shape::gen();
    shape->moveTo(points[0].x, points[0].y);
    for (int i = 1; i < count; ++i) {
        shape->lineTo(points[i].x, points[i].y);
    }
    shape->close();
    shape->fill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::StrokeBezierCubic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, Color col, float thickness) {
    if (col.a == 0) return;
    auto shape = tvg::Shape::gen();
    shape->moveTo(p1.x, p1.y);
    shape->cubicTo(p2.x, p2.y, p3.x, p3.y, p4.x, p4.y);
    shape->strokeWidth(thickness);
    shape->strokeCap(tvg::StrokeCap::Round);
    shape->strokeFill(col.r, col.g, col.b, col.a);
    ApplyClip(shape);
    m_canvas->add(shape);
}

void ThorVGRenderer::StrokeBezierQuadratic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, Color col, float thickness) {
    if (col.a == 0) return;
    // Degree elevation from quadratic to cubic
    ImVec2 c1(p1.x + 2.0f / 3.0f * (p2.x - p1.x), p1.y + 2.0f / 3.0f * (p2.y - p1.y));
    ImVec2 c2(p3.x + 2.0f / 3.0f * (p2.x - p3.x), p3.y + 2.0f / 3.0f * (p2.y - p3.y));
    StrokeBezierCubic(p1, c1, c2, p3, col, thickness);
}

bool ThorVGRenderer::LoadFont(const char* name, const void* data, size_t size) {
    if (!name || !data || size == 0) return false;
    tvg::Result res = tvg::Text::load(name, (const char*)data, (uint32_t)size, "ttf", true);
    return res == tvg::Result::Success;
}

bool ThorVGRenderer::LoadFontFile(const char* path) {
    if (!path) return false;
    tvg::Result res = tvg::Text::load(path);
    return res == tvg::Result::Success;
}

void ThorVGRenderer::DrawText(const char* text_utf8, float x, float y, Color col, const char* font_name, float font_size, float wrap_width) {
    if (col.a == 0 || !text_utf8 || text_utf8[0] == '\0') return;

    auto txt = tvg::Text::gen();
    txt->text(text_utf8);

    const char* target_font = (font_name && font_name[0] != '\0') ? font_name : m_default_font_name.c_str();
    if (txt->font(target_font) != tvg::Result::Success) {
        txt->font(nullptr);
    }
    float base_size = font_size <= 0.0f ? 13.0f : font_size;
    txt->size(base_size / m_font_scale_ratio);

    txt->fill(col.r, col.g, col.b);
    txt->opacity(col.a);
    txt->align(0.0f, 0.0f);
    txt->translate(x, y);

    if (wrap_width > 0.0f) {
        txt->layout(wrap_width, 0.0f);
        txt->wrap(tvg::TextWrap::Word);
    }

    ApplyClip(txt);
    m_canvas->add(txt);
}

bool ThorVGRenderer::MeasureGlyph(const char* utf8_char, const char* font_name, float font_size, GlyphMetricsInfo& out_metrics) {
    if (!utf8_char || utf8_char[0] == '\0') return false;
    auto txt = tvg::Text::gen();
    const char* target_font = (font_name && font_name[0] != '\0') ? font_name : m_default_font_name.c_str();
    if (txt->font(target_font) != tvg::Result::Success) {
        txt->font(nullptr);
    }
    float base_size = font_size <= 0.0f ? 13.0f : font_size;
    txt->size(base_size / m_font_scale_ratio);

    tvg::GlyphMetrics gm{};
    tvg::Result res = txt->metrics(utf8_char, gm);
    tvg::Paint::rel(txt);

    if (res == tvg::Result::Success) {
        out_metrics.advance = gm.advance;
        out_metrics.lsb = gm.bearing;
        out_metrics.w = gm.max.x - gm.min.x;
        out_metrics.h = gm.max.y - gm.min.y;
        return true;
    }
    return false;
}

bool ThorVGRenderer::MeasureText(const char* text_utf8, const char* font_name, float font_size, float& out_w, float& out_h) {
    if (!text_utf8 || text_utf8[0] == '\0') {
        out_w = 0.0f; out_h = 0.0f;
        return true;
    }
    auto txt = tvg::Text::gen();
    const char* target_font = (font_name && font_name[0] != '\0') ? font_name : m_default_font_name.c_str();
    if (txt->font(target_font) != tvg::Result::Success) {
        txt->font(nullptr);
    }
    float base_size = font_size <= 0.0f ? 13.0f : font_size;
    txt->size(base_size / m_font_scale_ratio);
    txt->text(text_utf8);

    // Compute width by summing advance of each UTF-8 character
    float total_advance = 0.0f;
    const char* cur = text_utf8;
    while (*cur) {
        tvg::GlyphMetrics gm{};
        const char* next = nullptr;
        if (txt->metrics(cur, gm, &next) == tvg::Result::Success && next) {
            total_advance += gm.advance;
            cur = next;
        } else {
            total_advance += (base_size / m_font_scale_ratio) * 0.6f;
            cur++;
        }
    }

    tvg::TextMetrics tm{};
    txt->metrics(tm);
    out_w = total_advance;
    out_h = tm.ascent + tm.descent;
    tvg::Paint::rel(txt);
    return true;
}

void ThorVGRenderer::DrawImage(const uint32_t* pixels, int src_w, int src_h, float dst_x, float dst_y, float dst_w, float dst_h, Color tint, float rounding) {
    if (!pixels || src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0) return;
    auto pic = tvg::Picture::gen();
    pic->load(pixels, (uint32_t)src_w, (uint32_t)src_h, tvg::ColorSpace::ABGR8888S, true);
    pic->size(dst_w, dst_h);
    pic->translate(dst_x, dst_y);
    if (tint.a < 255) pic->opacity(tint.a);

    if (rounding > 0.0f) {
        auto round_clip = tvg::Shape::gen();
        round_clip->appendRect(dst_x, dst_y, dst_w, dst_h, rounding, rounding);
        pic->clip(round_clip);
    } else {
        ApplyClip(pic);
    }

    m_canvas->add(pic);
}

void ThorVGRenderer::RasterizeFallbackTriangles(const ImDrawList* dl, const DrawCommand& cmd) {
    // Rasterize fallback triangles directly into m_pixels
    unsigned char* atlas_pixels = nullptr;
    int atlas_w = 0, atlas_h = 0;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->GetTexDataAsRGBA32(&atlas_pixels, &atlas_w, &atlas_h);

    const uint32_t* tex_data = (const uint32_t*)atlas_pixels;

    for (uint32_t i = 0; i < cmd.idx_count; i += 3) {
        ImDrawIdx idx0 = dl->IdxBuffer[cmd.idx_offset + i + 0];
        ImDrawIdx idx1 = dl->IdxBuffer[cmd.idx_offset + i + 1];
        ImDrawIdx idx2 = dl->IdxBuffer[cmd.idx_offset + i + 2];

        const ImDrawVert& v0 = dl->VtxBuffer[idx0];
        const ImDrawVert& v1 = dl->VtxBuffer[idx1];
        const ImDrawVert& v2 = dl->VtxBuffer[idx2];

        // Vertex positions scaled to physical framebuffer
        ImVec2 p0((v0.pos.x - m_display_pos.x) * m_display_scale.x, (v0.pos.y - m_display_pos.y) * m_display_scale.y);
        ImVec2 p1((v1.pos.x - m_display_pos.x) * m_display_scale.x, (v1.pos.y - m_display_pos.y) * m_display_scale.y);
        ImVec2 p2((v2.pos.x - m_display_pos.x) * m_display_scale.x, (v2.pos.y - m_display_pos.y) * m_display_scale.y);

        // Scaled clip rect
        float clip_x1 = (cmd.clip_rect.x - m_display_pos.x) * m_display_scale.x;
        float clip_y1 = (cmd.clip_rect.y - m_display_pos.y) * m_display_scale.y;
        float clip_x2 = (cmd.clip_rect.z - m_display_pos.x) * m_display_scale.x;
        float clip_y2 = (cmd.clip_rect.w - m_display_pos.y) * m_display_scale.y;

        // Triangle bounding box
        float min_x = (std::min)({p0.x, p1.x, p2.x});
        float max_x = (std::max)({p0.x, p1.x, p2.x});
        float min_y = (std::min)({p0.y, p1.y, p2.y});
        float max_y = (std::max)({p0.y, p1.y, p2.y});

        // Clip to clip_rect and screen
        int x0 = (std::max)(0, (int)std::floor((std::max)(min_x, clip_x1)));
        int x1 = (std::min)(m_width - 1, (int)std::ceil((std::min)(max_x, clip_x2)));
        int y0 = (std::max)(0, (int)std::floor((std::max)(min_y, clip_y1)));
        int y1 = (std::min)(m_height - 1, (int)std::ceil((std::min)(max_y, clip_y2)));

        if (x0 > x1 || y0 > y1) continue;

        float denom = (p1.y - p2.y) * (p0.x - p2.x) + (p2.x - p1.x) * (p0.y - p2.y);
        if (std::abs(denom) < 1e-5f) continue;
        float inv_denom = 1.0f / denom;

        for (int y = y0; y <= y1; ++y) {
            float py = y + 0.5f;
            for (int x = x0; x <= x1; ++x) {
                float px = x + 0.5f;
                float w0 = ((p1.y - p2.y) * (px - p2.x) + (p2.x - p1.x) * (py - p2.y)) * inv_denom;
                float w1 = ((p2.y - p0.y) * (px - p2.x) + (p0.x - p2.x) * (py - p2.y)) * inv_denom;
                float w2 = 1.0f - w0 - w1;

                if (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) {
                    // Interpolate UV and vertex color
                    float u = w0 * v0.uv.x + w1 * v1.uv.x + w2 * v2.uv.x;
                    float v = w0 * v0.uv.y + w1 * v1.uv.y + w2 * v2.uv.y;

                    uint32_t sampled_color = 0xFFFFFFFF;
                    if (tex_data && atlas_w > 0 && atlas_h > 0) {
                        int tx = (std::max)(0, (std::min)(atlas_w - 1, (int)(u * atlas_w)));
                        int ty = (std::max)(0, (std::min)(atlas_h - 1, (int)(v * atlas_h)));
                        sampled_color = tex_data[ty * atlas_w + tx];
                    }

                    // Vertex color modulation
                    Color vert_c = Color::FromImU32(v0.col);
                    uint8_t sa = (uint8_t)((sampled_color >> 24) & 0xFF);
                    uint8_t sb = (uint8_t)((sampled_color >> 16) & 0xFF);
                    uint8_t sg = (uint8_t)((sampled_color >> 8) & 0xFF);
                    uint8_t sr = (uint8_t)(sampled_color & 0xFF);

                    uint32_t final_r = (sr * vert_c.r) / 255;
                    uint32_t final_g = (sg * vert_c.g) / 255;
                    uint32_t final_b = (sb * vert_c.b) / 255;
                    uint32_t final_a = (sa * vert_c.a) / 255;

                    if (final_a > 0) {
                        // Standard alpha blend over destination
                        uint32_t& dst = m_pixels[y * m_width + x];
                        uint8_t dr = (uint8_t)(dst & 0xFF);
                        uint8_t dg = (uint8_t)((dst >> 8) & 0xFF);
                        uint8_t db = (uint8_t)((dst >> 16) & 0xFF);
                        uint8_t da = (uint8_t)((dst >> 24) & 0xFF);

                        uint32_t out_r = (final_r * final_a + dr * (255 - final_a)) / 255;
                        uint32_t out_g = (final_g * final_a + dg * (255 - final_a)) / 255;
                        uint32_t out_b = (final_b * final_a + db * (255 - final_a)) / 255;
                        uint32_t out_a = final_a + (da * (255 - final_a)) / 255;

                        dst = (out_a << 24) | (out_b << 16) | (out_g << 8) | out_r;
                    }
                }
            }
        }
    }
}

void ThorVGRenderer::DrawFallbackMesh(const ImDrawList* dl, const DrawCommand& cmd) {
    if (!dl || cmd.idx_count == 0) return;

    if (cmd.texture_id == 0) {
        // Flat triangles can be drawn as ThorVG vector shapes
        for (uint32_t i = 0; i < cmd.idx_count; i += 3) {
            ImDrawIdx idx0 = dl->IdxBuffer[cmd.idx_offset + i + 0];
            ImDrawIdx idx1 = dl->IdxBuffer[cmd.idx_offset + i + 1];
            ImDrawIdx idx2 = dl->IdxBuffer[cmd.idx_offset + i + 2];

            const ImDrawVert& v0 = dl->VtxBuffer[idx0];
            const ImDrawVert& v1 = dl->VtxBuffer[idx1];
            const ImDrawVert& v2 = dl->VtxBuffer[idx2];

            auto shape = tvg::Shape::gen();
            shape->moveTo((v0.pos.x - m_display_pos.x) * m_display_scale.x, (v0.pos.y - m_display_pos.y) * m_display_scale.y);
            shape->lineTo((v1.pos.x - m_display_pos.x) * m_display_scale.x, (v1.pos.y - m_display_pos.y) * m_display_scale.y);
            shape->lineTo((v2.pos.x - m_display_pos.x) * m_display_scale.x, (v2.pos.y - m_display_pos.y) * m_display_scale.y);
            shape->close();

            Color col = Color::FromImU32(v0.col);
            shape->fill(col.r, col.g, col.b, col.a);
            ApplyClip(shape);
            m_canvas->add(shape);
        }
    } else {
        // Textured fallback triangles: flush vector primitives so far and rasterize in-order
        FlushCanvas();
        RasterizeFallbackTriangles(dl, cmd);
    }
}

void ThorVGRenderer::RenderDrawData(ImDrawData* draw_data) {
    if (!draw_data) return;

    m_display_scale = draw_data->FramebufferScale;
    m_display_pos = draw_data->DisplayPos;
    if (m_display_scale.x <= 0.0f) m_display_scale.x = 1.0f;
    if (m_display_scale.y <= 0.0f) m_display_scale.y = 1.0f;

    BeginFrame();

    int cmd_lists_count = draw_data->CmdLists.Size;
    for (int n = 0; n < cmd_lists_count; ++n) {
        ImDrawList* dl = draw_data->CmdLists[n];
        DrawListStream& stream = Recorder::Instance().GetStream(dl);
        m_clip_stack.clear();

        for (const auto& cmd : stream.commands) {
            switch (cmd.type) {
            case CmdType::PushClipRect: {
                float cx = (cmd.clip_rect.x - m_display_pos.x) * m_display_scale.x;
                float cy = (cmd.clip_rect.y - m_display_pos.y) * m_display_scale.y;
                float cw = (cmd.clip_rect.z - cmd.clip_rect.x) * m_display_scale.x;
                float ch = (cmd.clip_rect.w - cmd.clip_rect.y) * m_display_scale.y;
                PushClip(cx, cy, cw, ch);
                break;
            }
            case CmdType::PopClipRect:
                PopClip();
                break;
            case CmdType::RectFilled: {
                float x = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float y = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float w = (cmd.p2.x - cmd.p1.x) * m_display_scale.x;
                float h = (cmd.p2.y - cmd.p1.y) * m_display_scale.y;
                float rounding = cmd.rounding * m_display_scale.x;
                FillRect(x, y, w, h, Color::FromImU32(cmd.col), rounding);
                break;
            }
            case CmdType::Rect: {
                float x = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float y = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float w = (cmd.p2.x - cmd.p1.x) * m_display_scale.x;
                float h = (cmd.p2.y - cmd.p1.y) * m_display_scale.y;
                float thickness = cmd.thickness * m_display_scale.x;
                float rounding = cmd.rounding * m_display_scale.x;
                StrokeRect(x, y, w, h, Color::FromImU32(cmd.col), thickness, rounding);
                break;
            }
            case CmdType::RectFilledMultiColor: {
                float x = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float y = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float w = (cmd.p2.x - cmd.p1.x) * m_display_scale.x;
                float h = (cmd.p2.y - cmd.p1.y) * m_display_scale.y;
                float rounding = cmd.rounding * m_display_scale.x;
                if (cmd.col == cmd.col4 && cmd.col2 == cmd.col3) {
                    FillRectLinearGradient(x, y, w, h, x, y, Color::FromImU32(cmd.col), x + w, y, Color::FromImU32(cmd.col2), rounding);
                } else {
                    FillRectLinearGradient(x, y, w, h, x, y, Color::FromImU32(cmd.col), x, y + h, Color::FromImU32(cmd.col4), rounding);
                }
                break;
            }
            case CmdType::Line: {
                float x1 = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float y1 = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float x2 = (cmd.p2.x - m_display_pos.x) * m_display_scale.x;
                float y2 = (cmd.p2.y - m_display_pos.y) * m_display_scale.y;
                float thickness = cmd.thickness * m_display_scale.x;
                StrokeLine(x1, y1, x2, y2, Color::FromImU32(cmd.col), thickness);
                break;
            }
            case CmdType::Circle: {
                float cx = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float cy = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float radius = cmd.radius * m_display_scale.x;
                float thickness = cmd.thickness * m_display_scale.x;
                StrokeCircle(cx, cy, radius, Color::FromImU32(cmd.col), thickness);
                break;
            }
            case CmdType::CircleFilled: {
                float cx = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float cy = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float radius = cmd.radius * m_display_scale.x;
                FillCircle(cx, cy, radius, Color::FromImU32(cmd.col));
                break;
            }
            case CmdType::Ngon: {
                float cx = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float cy = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float radius = cmd.radius * m_display_scale.x;
                float thickness = cmd.thickness * m_display_scale.x;
                StrokeNgon(cx, cy, radius, cmd.num_segments, Color::FromImU32(cmd.col), thickness);
                break;
            }
            case CmdType::NgonFilled: {
                float cx = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float cy = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float radius = cmd.radius * m_display_scale.x;
                FillNgon(cx, cy, radius, cmd.num_segments, Color::FromImU32(cmd.col));
                break;
            }
            case CmdType::Ellipse: {
                float cx = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float cy = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float rx = cmd.p2.x * m_display_scale.x;
                float ry = cmd.p2.y * m_display_scale.y;
                float thickness = cmd.thickness * m_display_scale.x;
                StrokeEllipse(cx, cy, rx, ry, cmd.radius, Color::FromImU32(cmd.col), thickness);
                break;
            }
            case CmdType::EllipseFilled: {
                float cx = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float cy = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float rx = cmd.p2.x * m_display_scale.x;
                float ry = cmd.p2.y * m_display_scale.y;
                FillEllipse(cx, cy, rx, ry, cmd.radius, Color::FromImU32(cmd.col));
                break;
            }
            case CmdType::Triangle: {
                ImVec2 pts[3] = {
                    ImVec2((cmd.p1.x - m_display_pos.x) * m_display_scale.x, (cmd.p1.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p2.x - m_display_pos.x) * m_display_scale.x, (cmd.p2.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p3.x - m_display_pos.x) * m_display_scale.x, (cmd.p3.y - m_display_pos.y) * m_display_scale.y)
                };
                StrokePolyline(pts, 3, Color::FromImU32(cmd.col), cmd.thickness * m_display_scale.x, true);
                break;
            }
            case CmdType::TriangleFilled: {
                ImVec2 pts[3] = {
                    ImVec2((cmd.p1.x - m_display_pos.x) * m_display_scale.x, (cmd.p1.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p2.x - m_display_pos.x) * m_display_scale.x, (cmd.p2.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p3.x - m_display_pos.x) * m_display_scale.x, (cmd.p3.y - m_display_pos.y) * m_display_scale.y)
                };
                FillConvexPoly(pts, 3, Color::FromImU32(cmd.col));
                break;
            }
            case CmdType::Quad: {
                ImVec2 pts[4] = {
                    ImVec2((cmd.p1.x - m_display_pos.x) * m_display_scale.x, (cmd.p1.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p2.x - m_display_pos.x) * m_display_scale.x, (cmd.p2.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p3.x - m_display_pos.x) * m_display_scale.x, (cmd.p3.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p4.x - m_display_pos.x) * m_display_scale.x, (cmd.p4.y - m_display_pos.y) * m_display_scale.y)
                };
                StrokePolyline(pts, 4, Color::FromImU32(cmd.col), cmd.thickness * m_display_scale.x, true);
                break;
            }
            case CmdType::QuadFilled: {
                ImVec2 pts[4] = {
                    ImVec2((cmd.p1.x - m_display_pos.x) * m_display_scale.x, (cmd.p1.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p2.x - m_display_pos.x) * m_display_scale.x, (cmd.p2.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p3.x - m_display_pos.x) * m_display_scale.x, (cmd.p3.y - m_display_pos.y) * m_display_scale.y),
                    ImVec2((cmd.p4.x - m_display_pos.x) * m_display_scale.x, (cmd.p4.y - m_display_pos.y) * m_display_scale.y)
                };
                FillConvexPoly(pts, 4, Color::FromImU32(cmd.col));
                break;
            }
            case CmdType::Polyline: {
                const ImVec2* src_pts = &stream.points_pool[cmd.data_offset];
                bool closed = (cmd.flags & ImDrawFlags_Closed) != 0;
                std::vector<ImVec2> pts(cmd.data_size);
                for (size_t i = 0; i < cmd.data_size; ++i) {
                    pts[i].x = (src_pts[i].x - m_display_pos.x) * m_display_scale.x;
                    pts[i].y = (src_pts[i].y - m_display_pos.y) * m_display_scale.y;
                }
                StrokePolyline(pts.data(), (int)cmd.data_size, Color::FromImU32(cmd.col), cmd.thickness * m_display_scale.x, closed);
                break;
            }
            case CmdType::ConvexPolyFilled:
            case CmdType::ConcavePolyFilled: {
                const ImVec2* src_pts = &stream.points_pool[cmd.data_offset];
                std::vector<ImVec2> pts(cmd.data_size);
                for (size_t i = 0; i < cmd.data_size; ++i) {
                    pts[i].x = (src_pts[i].x - m_display_pos.x) * m_display_scale.x;
                    pts[i].y = (src_pts[i].y - m_display_pos.y) * m_display_scale.y;
                }
                FillConvexPoly(pts.data(), (int)cmd.data_size, Color::FromImU32(cmd.col));
                break;
            }
            case CmdType::BezierCubic: {
                ImVec2 p1((cmd.p1.x - m_display_pos.x) * m_display_scale.x, (cmd.p1.y - m_display_pos.y) * m_display_scale.y);
                ImVec2 p2((cmd.p2.x - m_display_pos.x) * m_display_scale.x, (cmd.p2.y - m_display_pos.y) * m_display_scale.y);
                ImVec2 p3((cmd.p3.x - m_display_pos.x) * m_display_scale.x, (cmd.p3.y - m_display_pos.y) * m_display_scale.y);
                ImVec2 p4((cmd.p4.x - m_display_pos.x) * m_display_scale.x, (cmd.p4.y - m_display_pos.y) * m_display_scale.y);
                StrokeBezierCubic(p1, p2, p3, p4, Color::FromImU32(cmd.col), cmd.thickness * m_display_scale.x);
                break;
            }
            case CmdType::BezierQuadratic: {
                ImVec2 p1((cmd.p1.x - m_display_pos.x) * m_display_scale.x, (cmd.p1.y - m_display_pos.y) * m_display_scale.y);
                ImVec2 p2((cmd.p2.x - m_display_pos.x) * m_display_scale.x, (cmd.p2.y - m_display_pos.y) * m_display_scale.y);
                ImVec2 p3((cmd.p3.x - m_display_pos.x) * m_display_scale.x, (cmd.p3.y - m_display_pos.y) * m_display_scale.y);
                StrokeBezierQuadratic(p1, p2, p3, Color::FromImU32(cmd.col), cmd.thickness * m_display_scale.x);
                break;
            }
            case CmdType::Text: {
                const char* text_str = &stream.text_pool[cmd.data_offset];
                const char* font_name = cmd.font ? cmd.font->GetDebugName() : m_default_font_name.c_str();
                float tx = (cmd.p1.x - m_display_pos.x) * m_display_scale.x;
                float ty = (cmd.p1.y - m_display_pos.y) * m_display_scale.y;
                float font_size = cmd.font_size * m_display_scale.y;
                float wrap_width = cmd.wrap_width > 0.0f ? cmd.wrap_width * m_display_scale.x : 0.0f;
                DrawText(text_str, tx, ty, Color::FromImU32(cmd.col), font_name, font_size, wrap_width);
                break;
            }
            case CmdType::Image:
            case CmdType::ImageQuad:
            case CmdType::ImageRounded:
                // If custom image pixels available, can use DrawImage
                break;
            case CmdType::FallbackMesh:
                DrawFallbackMesh(dl, cmd);
                break;
            default:
                break;
            }
        }
    }

    EndFrame();
}

void ThorVGRenderer::InitGLResources() {
    if (m_gl_initialized) return;

    // Load function pointers via glfwGetProcAddress
    m_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)glfwGetProcAddress("glGenVertexArrays");
    m_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)glfwGetProcAddress("glBindVertexArray");
    m_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)glfwGetProcAddress("glDeleteVertexArrays");
    m_glGenBuffers = (PFNGLGENBUFFERSPROC)glfwGetProcAddress("glGenBuffers");
    m_glBindBuffer = (PFNGLBINDBUFFERPROC)glfwGetProcAddress("glBindBuffer");
    m_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)glfwGetProcAddress("glDeleteBuffers");
    m_glBufferData = (PFNGLBUFFERDATAPROC)glfwGetProcAddress("glBufferData");
    m_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)glfwGetProcAddress("glEnableVertexAttribArray");
    m_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)glfwGetProcAddress("glVertexAttribPointer");
    m_glCreateShader = (PFNGLCREATESHADERPROC)glfwGetProcAddress("glCreateShader");
    m_glShaderSource = (PFNGLSHADERSOURCEPROC)glfwGetProcAddress("glShaderSource");
    m_glCompileShader = (PFNGLCOMPILESHADERPROC)glfwGetProcAddress("glCompileShader");
    m_glGetShaderiv = (PFNGLGETSHADERIVPROC)glfwGetProcAddress("glGetShaderiv");
    m_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)glfwGetProcAddress("glGetShaderInfoLog");
    m_glCreateProgram = (PFNGLCREATEPROGRAMPROC)glfwGetProcAddress("glCreateProgram");
    m_glAttachShader = (PFNGLATTACHSHADERPROC)glfwGetProcAddress("glAttachShader");
    m_glLinkProgram = (PFNGLLINKPROGRAMPROC)glfwGetProcAddress("glLinkProgram");
    m_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)glfwGetProcAddress("glGetProgramiv");
    m_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)glfwGetProcAddress("glGetProgramInfoLog");
    m_glUseProgram = (PFNGLUSEPROGRAMPROC)glfwGetProcAddress("glUseProgram");
    m_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)glfwGetProcAddress("glDeleteProgram");
    m_glDeleteShader = (PFNGLDELETESHADERPROC)glfwGetProcAddress("glDeleteShader");
    m_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)glfwGetProcAddress("glGetUniformLocation");
    m_glUniform1i = (PFNGLUNIFORM1IPROC)glfwGetProcAddress("glUniform1i");
    m_glActiveTexture = (PFNGLACTIVETEXTUREPROC)glfwGetProcAddress("glActiveTexture");

    // Texture creation
    glGenTextures(1, &m_gl_texture);
    glBindTexture(GL_TEXTURE_2D, m_gl_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels.data());

    if (m_glCreateShader && m_glCreateProgram && m_glGenVertexArrays && m_glGenBuffers) {
        // GL3 shader pipeline
        const char* vtx_src =
            "#version 130\n"
            "in vec2 Position;\n"
            "in vec2 UV;\n"
            "out vec2 Frag_UV;\n"
            "void main() {\n"
            "    Frag_UV = UV;\n"
            "    gl_Position = vec4(Position, 0.0, 1.0);\n"
            "}\n";

        const char* frag_src =
            "#version 130\n"
            "uniform sampler2D Texture;\n"
            "in vec2 Frag_UV;\n"
            "out vec4 Out_Color;\n"
            "void main() {\n"
            "    Out_Color = texture(Texture, Frag_UV);\n"
            "}\n";

        GLuint vshader = m_glCreateShader(GL_VERTEX_SHADER);
        m_glShaderSource(vshader, 1, &vtx_src, nullptr);
        m_glCompileShader(vshader);

        GLuint fshader = m_glCreateShader(GL_FRAGMENT_SHADER);
        m_glShaderSource(fshader, 1, &frag_src, nullptr);
        m_glCompileShader(fshader);

        m_gl_shader = m_glCreateProgram();
        m_glAttachShader(m_gl_shader, vshader);
        m_glAttachShader(m_gl_shader, fshader);
        m_glLinkProgram(m_gl_shader);

        m_glDeleteShader(vshader);
        m_glDeleteShader(fshader);

        // Quad geometry: 2 triangles covering [-1, 1] screen with [0, 1] UV
        float vertices[] = {
            // Pos        // UV
            -1.0f,  1.0f, 0.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 1.0f,

            -1.0f,  1.0f, 0.0f, 0.0f,
             1.0f, -1.0f, 1.0f, 1.0f,
             1.0f,  1.0f, 1.0f, 0.0f
        };

        m_glGenVertexArrays(1, &m_gl_vao);
        m_glGenBuffers(1, &m_gl_vbo);

        m_glBindVertexArray(m_gl_vao);
        m_glBindBuffer(GL_ARRAY_BUFFER, m_gl_vbo);
        m_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        m_glEnableVertexAttribArray(0);
        m_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

        m_glEnableVertexAttribArray(1);
        m_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        m_glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_glBindVertexArray(0);
    }

    m_gl_initialized = true;
}

void ThorVGRenderer::PresentGL() {
    if (!m_gl_initialized) {
        InitGLResources();
    }

    // Upload rendered software pixel buffer to GL texture
    glBindTexture(GL_TEXTURE_2D, m_gl_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, m_pixels.data());

    glViewport(0, 0, m_width, m_height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (m_gl_shader && m_gl_vao) {
        // GL3 shader path
        m_glUseProgram(m_gl_shader);
        if (m_glActiveTexture) m_glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_gl_texture);
        if (m_glGetUniformLocation && m_glUniform1i) {
            GLint loc = m_glGetUniformLocation(m_gl_shader, "Texture");
            if (loc >= 0) m_glUniform1i(loc, 0);
        }

        m_glBindVertexArray(m_gl_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        m_glBindVertexArray(0);
        m_glUseProgram(0);
    } else {
        // Immediate mode compatibility path
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, m_gl_texture);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f,  1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f( 1.0f,  1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f( 1.0f, -1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    }
}

IRenderer* CreateThorVGRenderer() {
    return new ThorVGRenderer();
}

} // namespace ImGuiExt
