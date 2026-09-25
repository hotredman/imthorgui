#pragma once

#include "imgui.h"
#include "imgui_ext/recorder.h"
#include <cstdint>
#include <vector>

namespace ImGuiExt {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;

    Color() = default;
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}

    static Color FromImU32(ImU32 col) {
        return Color(
            (uint8_t)(col & 0xFF),
            (uint8_t)((col >> 8) & 0xFF),
            (uint8_t)((col >> 16) & 0xFF),
            (uint8_t)((col >> 24) & 0xFF)
        );
    }

    uint32_t ToU32() const {
        return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    }
};

struct TextMetricsInfo {
    float ascent = 0.0f;
    float descent = 0.0f;
    float line_gap = 0.0f;
};

struct GlyphMetricsInfo {
    float advance = 0.0f;
    float lsb = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool Init(int width, int height) = 0;
    virtual void Shutdown() = 0;
    virtual void Resize(int width, int height) = 0;

    // Frame lifecycle
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    // Canvas 2D style state
    virtual void PushClip(float x, float y, float w, float h) = 0;
    virtual void PopClip() = 0;

    // Canvas 2D style drawing primitives
    virtual void FillRect(float x, float y, float w, float h, Color col, float rounding = 0.0f) = 0;
    virtual void StrokeRect(float x, float y, float w, float h, Color col, float thickness = 1.0f, float rounding = 0.0f) = 0;
    virtual void FillRectLinearGradient(float x, float y, float w, float h, float x1, float y1, Color c1, float x2, float y2, Color c2, float rounding = 0.0f) = 0;

    virtual void FillCircle(float cx, float cy, float radius, Color col) = 0;
    virtual void StrokeCircle(float cx, float cy, float radius, Color col, float thickness = 1.0f) = 0;

    virtual void FillNgon(float cx, float cy, float radius, int segments, Color col) = 0;
    virtual void StrokeNgon(float cx, float cy, float radius, int segments, Color col, float thickness = 1.0f) = 0;

    virtual void FillEllipse(float cx, float cy, float rx, float ry, float rot, Color col) = 0;
    virtual void StrokeEllipse(float cx, float cy, float rx, float ry, float rot, Color col, float thickness = 1.0f) = 0;

    virtual void StrokeLine(float x1, float y1, float x2, float y2, Color col, float thickness = 1.0f) = 0;
    virtual void StrokePolyline(const ImVec2* points, int count, Color col, float thickness = 1.0f, bool closed = false) = 0;
    virtual void FillConvexPoly(const ImVec2* points, int count, Color col) = 0;

    virtual void StrokeBezierCubic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, Color col, float thickness = 1.0f) = 0;
    virtual void StrokeBezierQuadratic(const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, Color col, float thickness = 1.0f) = 0;

    // Text & font
    virtual bool LoadFont(const char* name, const void* data, size_t size) = 0;
    virtual bool LoadFontFile(const char* path) = 0;
    virtual void DrawText(const char* text_utf8, float x, float y, Color col, const char* font_name = nullptr, float font_size = 0.0f, float wrap_width = 0.0f) = 0;
    virtual bool MeasureGlyph(const char* utf8_char, const char* font_name, float font_size, GlyphMetricsInfo& out_metrics) = 0;
    virtual bool MeasureText(const char* text_utf8, const char* font_name, float font_size, float& out_w, float& out_h) = 0;

    // Image
    virtual void DrawImage(const uint32_t* pixels, int src_w, int src_h, float dst_x, float dst_y, float dst_w, float dst_h, Color tint = Color(255, 255, 255, 255), float rounding = 0.0f) = 0;

    // Fallback mesh
    virtual void DrawFallbackMesh(const ImDrawList* dl, const DrawCommand& cmd) = 0;

    // High-level render of intercepted streams
    virtual void RenderDrawData(ImDrawData* draw_data) = 0;

    // Target buffer & presentation
    virtual const uint32_t* GetPixelBuffer() const = 0;
    virtual int GetWidth() const = 0;
    virtual int GetHeight() const = 0;
};

// Factory for ThorVG renderer
IRenderer* CreateThorVGRenderer();

} // namespace ImGuiExt
