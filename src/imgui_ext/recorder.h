#pragma once

#include "imgui.h"
#include "imgui_ext/my_imgui_config.h"
#include <vector>
#include <cstdint>
#include <cfloat>
#include <cstring>
#include <unordered_map>

namespace ImGuiExt {

enum class CmdType : uint16_t {
    None = 0,
    RectFilled,
    Rect,
    Line,
    Circle,
    CircleFilled,
    Ngon,
    NgonFilled,
    Ellipse,
    EllipseFilled,
    Triangle,
    TriangleFilled,
    Quad,
    QuadFilled,
    Polyline,
    ConvexPolyFilled,
    ConcavePolyFilled,
    BezierCubic,
    BezierQuadratic,
    Text,
    Image,
    ImageQuad,
    ImageRounded,
    RectFilledMultiColor,
    PushClipRect,
    PopClipRect,
    PushTexture,
    PopTexture,
    Callback,
    FallbackMesh,
};

struct DrawCommand {
    CmdType type = CmdType::None;
    uint16_t flags = 0;
    ImVec4 bbox{0, 0, 0, 0};
    ImVec4 clip_rect{0, 0, 0, 0};
    ImU32 col = 0;
    ImU32 col2 = 0;
    ImU32 col3 = 0;
    ImU32 col4 = 0;
    ImVec2 p1{0, 0};
    ImVec2 p2{0, 0};
    ImVec2 p3{0, 0};
    ImVec2 p4{0, 0};
    float radius = 0.0f;
    float rounding = 0.0f;
    float thickness = 1.0f;
    int num_segments = 0;
    ImTextureID texture_id = 0;

    // Buffer data references (points, text characters)
    uint32_t data_offset = 0;
    uint32_t data_size = 0;

    // Fallback mesh vertex/index ranges in ImDrawList
    uint32_t vtx_offset = 0;
    uint32_t vtx_count = 0;
    uint32_t idx_offset = 0;
    uint32_t idx_count = 0;

    // Font info for text
    ImFont* font = nullptr;
    float font_size = 0.0f;
    float wrap_width = 0.0f;

    DrawCommand() {
        std::memset(static_cast<void*>(this), 0, sizeof(*this));
        thickness = 1.0f;
    }
};

struct DrawListStream {
    std::vector<DrawCommand> commands;
    std::vector<ImVec2> points_pool;
    std::vector<char> text_pool;

    // Channel splitting support
    std::vector<std::vector<DrawCommand>> channels;
    int current_channel = 0;

    // Fallback tracking
    size_t last_recorded_vtx_count = 0;
    size_t last_recorded_idx_count = 0;

    // Clip rect tracking
    std::vector<ImVec4> clip_stack;

    // Modal dimming reorder tracking
    bool modal_dim_reordered = false;

    void Reset() {
        commands.clear();
        points_pool.clear();
        text_pool.clear();
        channels.clear();
        current_channel = 0;
        last_recorded_vtx_count = 0;
        last_recorded_idx_count = 0;
        clip_stack.clear();
        modal_dim_reordered = false;
    }

    std::vector<DrawCommand>& GetCurrentCmdList() {
        if (!channels.empty() && current_channel < (int)channels.size()) {
            return channels[current_channel];
        }
        return commands;
    }
};

class Recorder {
public:
    static Recorder& Instance() {
        static Recorder instance;
        return instance;
    }

    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    void Reset();

    DrawListStream& GetStream(ImDrawList* dl);

    // Flushes any unhandled direct vertex writes from ImDrawList into a FallbackMesh command
    void FlushFallbackMesh(ImDrawList* dl);

    // Finalize all streams for the frame
    void EndFrame();

    const std::unordered_map<ImDrawList*, DrawListStream>& GetStreams() const {
        return m_streams;
    }

    // Hook entry points
    void OnReset(ImDrawList* dl);
    void OnPushClipRect(ImDrawList* dl, const ImVec2& cr_min, const ImVec2& cr_max, bool intersect);
    void OnPopClipRect(ImDrawList* dl);
    void OnPushTexture(ImDrawList* dl, ImTextureRef tex_ref);
    void OnPopTexture(ImDrawList* dl);

    void OnSplitterSplit(ImDrawListSplitter* splitter, ImDrawList* dl, int count);
    void OnSplitterSetChannel(ImDrawListSplitter* splitter, ImDrawList* dl, int idx);
    void OnSplitterMerge(ImDrawListSplitter* splitter, ImDrawList* dl);

    bool RecordLine(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness);
    bool RecordRect(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, float thickness, ImDrawFlags flags);
    bool RecordRectFilled(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags);
    bool RecordRectFilledMultiColor(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 c1, ImU32 c2, ImU32 c3, ImU32 c4);
    bool RecordQuad(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness);
    bool RecordQuadFilled(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col);
    bool RecordTriangle(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness);
    bool RecordTriangleFilled(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col);
    bool RecordCircle(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness);
    bool RecordCircleFilled(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments);
    bool RecordNgon(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness);
    bool RecordNgonFilled(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments);
    bool RecordEllipse(ImDrawList* dl, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments, float thickness);
    bool RecordEllipseFilled(ImDrawList* dl, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments);
    bool RecordBezierCubic(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments);
    bool RecordBezierQuadratic(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments);
    bool RecordPolyline(ImDrawList* dl, const ImVec2* points, int count, ImU32 col, float thickness, ImDrawFlags flags);
    bool RecordConvexPolyFilled(ImDrawList* dl, const ImVec2* points, int count, ImU32 col);
    bool RecordConcavePolyFilled(ImDrawList* dl, const ImVec2* points, int count, ImU32 col);
    bool RecordText(ImDrawList* dl, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect);
    bool RecordImage(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col);
    bool RecordImageQuad(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4, ImU32 col);
    bool RecordImageRounded(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float rounding, ImDrawFlags flags);
    bool RecordCallback(ImDrawList* dl, ImDrawCallback callback, void* userdata, size_t userdata_size);

private:
    Recorder() = default;

    bool m_enabled = true;
    std::unordered_map<ImDrawList*, DrawListStream> m_streams;

    ImVec4 GetCurrentClipRect(DrawListStream& stream, ImDrawList* dl);
    ImTextureID GetCurrentTextureId(ImDrawList* dl);
};

inline void SetVectorInterception(bool enabled) { Recorder::Instance().SetEnabled(enabled); }
inline bool IsVectorInterceptionEnabled() { return Recorder::Instance().IsEnabled(); }

} // namespace ImGuiExt
