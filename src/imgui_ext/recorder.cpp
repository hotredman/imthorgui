#include "recorder.h"
#include <algorithm>
#include <cmath>

namespace ImGuiExt {

void Recorder::Reset() {
    for (auto& kv : m_streams) {
        kv.second.Reset();
    }
}

DrawListStream& Recorder::GetStream(ImDrawList* dl) {
    return m_streams[dl];
}

ImVec4 Recorder::GetCurrentClipRect(DrawListStream& stream, ImDrawList* dl) {
    if (!stream.clip_stack.empty()) {
        return stream.clip_stack.back();
    }
    return dl->_CmdHeader.ClipRect;
}

ImTextureID Recorder::GetCurrentTextureId(ImDrawList* dl) {
    return dl->_CmdHeader.TexRef._TexData ? (ImTextureID)dl->_CmdHeader.TexRef._TexData : (ImTextureID)0;
}

void Recorder::FlushFallbackMesh(ImDrawList* dl) {
    auto& stream = GetStream(dl);
    size_t cur_vtx = (size_t)dl->VtxBuffer.Size;
    size_t cur_idx = (size_t)dl->IdxBuffer.Size;

    if (cur_vtx > stream.last_recorded_vtx_count && cur_idx > stream.last_recorded_idx_count) {
        DrawCommand cmd;
        cmd.type = CmdType::FallbackMesh;
        cmd.clip_rect = GetCurrentClipRect(stream, dl);
        cmd.vtx_offset = (uint32_t)stream.last_recorded_vtx_count;
        cmd.vtx_count = (uint32_t)(cur_vtx - stream.last_recorded_vtx_count);
        cmd.idx_offset = (uint32_t)stream.last_recorded_idx_count;
        cmd.idx_count = (uint32_t)(cur_idx - stream.last_recorded_idx_count);
        cmd.texture_id = GetCurrentTextureId(dl);

        // Calculate bounding box over fallback mesh vertices
        ImVec4 bbox(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (size_t i = cmd.vtx_offset; i < cur_vtx; ++i) {
            const ImVec2& pos = dl->VtxBuffer[(int)i].pos;
            if (pos.x < bbox.x) bbox.x = pos.x;
            if (pos.y < bbox.y) bbox.y = pos.y;
            if (pos.x > bbox.z) bbox.z = pos.x;
            if (pos.y > bbox.w) bbox.w = pos.y;
        }
        cmd.bbox = bbox;

        stream.GetCurrentCmdList().push_back(cmd);
        stream.last_recorded_vtx_count = cur_vtx;
        stream.last_recorded_idx_count = cur_idx;
    }
}

void Recorder::EndFrame() {
    for (auto& kv : m_streams) {
        ImDrawList* dl = kv.first;
        FlushFallbackMesh(dl);
    }
}

void Recorder::OnReset(ImDrawList* dl) {
    m_streams[dl].Reset();
}

void Recorder::OnPushClipRect(ImDrawList* dl, const ImVec2& cr_min, const ImVec2& cr_max, bool intersect) {
    if (!m_enabled) return;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    ImVec4 cr(cr_min.x, cr_min.y, cr_max.x, cr_max.y);
    if (intersect && !stream.clip_stack.empty()) {
        const ImVec4& cur = stream.clip_stack.back();
        if (cr.x < cur.x) cr.x = cur.x;
        if (cr.y < cur.y) cr.y = cur.y;
        if (cr.z > cur.z) cr.z = cur.z;
        if (cr.w > cur.w) cr.w = cur.w;
    }
    cr.z = (std::max)(cr.x, cr.z);
    cr.w = (std::max)(cr.y, cr.w);
    stream.clip_stack.push_back(cr);

    DrawCommand cmd;
    cmd.type = CmdType::PushClipRect;
    cmd.clip_rect = cr;
    stream.GetCurrentCmdList().push_back(cmd);
}

void Recorder::OnPopClipRect(ImDrawList* dl) {
    if (!m_enabled) return;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);
    if (!stream.clip_stack.empty()) {
        stream.clip_stack.pop_back();
    }

    DrawCommand cmd;
    cmd.type = CmdType::PopClipRect;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    stream.GetCurrentCmdList().push_back(cmd);
}

void Recorder::OnPushTexture(ImDrawList* dl, ImTextureRef tex_ref) {
    if (!m_enabled) return;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::PushTexture;
    cmd.texture_id = tex_ref._TexData ? (ImTextureID)tex_ref._TexData : (ImTextureID)0;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    stream.GetCurrentCmdList().push_back(cmd);
}

void Recorder::OnPopTexture(ImDrawList* dl) {
    if (!m_enabled) return;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::PopTexture;
    cmd.texture_id = GetCurrentTextureId(dl);
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    stream.GetCurrentCmdList().push_back(cmd);
}

void Recorder::OnSplitterSplit(ImDrawListSplitter* splitter, ImDrawList* dl, int count) {
    (void)splitter;
    if (!m_enabled) return;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);
    stream.channels.resize(count);
    stream.current_channel = 0;
}

void Recorder::OnSplitterSetChannel(ImDrawListSplitter* splitter, ImDrawList* dl, int idx) {
    (void)splitter;
    if (!m_enabled) return;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);
    if (idx >= 0 && idx < (int)stream.channels.size()) {
        stream.current_channel = idx;
    }
}

void Recorder::OnSplitterMerge(ImDrawListSplitter* splitter, ImDrawList* dl) {
    (void)splitter;
    if (!m_enabled) return;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    // Concatenate all channels sequentially into main commands list
    for (auto& ch : stream.channels) {
        stream.commands.insert(stream.commands.end(), ch.begin(), ch.end());
        ch.clear();
    }
    stream.channels.clear();
    stream.current_channel = 0;
}

bool Recorder::RecordLine(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Line;
    cmd.p1 = p1;
    cmd.p2 = p2;
    cmd.col = col;
    cmd.thickness = thickness;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float half_th = thickness * 0.5f;
    cmd.bbox = ImVec4(
        (std::min)(p1.x, p2.x) - half_th,
        (std::min)(p1.y, p2.y) - half_th,
        (std::max)(p1.x, p2.x) + half_th,
        (std::max)(p1.y, p2.y) + half_th
    );

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordRect(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, float thickness, ImDrawFlags flags) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Rect;
    cmd.p1 = p_min;
    cmd.p2 = p_max;
    cmd.col = col;
    cmd.rounding = rounding;
    cmd.thickness = thickness;
    cmd.flags = (uint16_t)flags;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float half_th = thickness * 0.5f;
    cmd.bbox = ImVec4(p_min.x - half_th, p_min.y - half_th, p_max.x + half_th, p_max.y + half_th);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordRectFilled(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::RectFilled;
    cmd.p1 = p_min;
    cmd.p2 = p_max;
    cmd.col = col;
    cmd.rounding = rounding;
    cmd.flags = (uint16_t)flags;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.bbox = ImVec4(p_min.x, p_min.y, p_max.x, p_max.y);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordRectFilledMultiColor(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 c1, ImU32 c2, ImU32 c3, ImU32 c4) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::RectFilledMultiColor;
    cmd.p1 = p_min;
    cmd.p2 = p_max;
    cmd.col = c1;
    cmd.col2 = c2;
    cmd.col3 = c3;
    cmd.col4 = c4;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.bbox = ImVec4(p_min.x, p_min.y, p_max.x, p_max.y);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordQuad(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Quad;
    cmd.p1 = p1; cmd.p2 = p2; cmd.p3 = p3; cmd.p4 = p4;
    cmd.col = col;
    cmd.thickness = thickness;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float half_th = thickness * 0.5f;
    cmd.bbox = ImVec4(
        (std::min)({p1.x, p2.x, p3.x, p4.x}) - half_th,
        (std::min)({p1.y, p2.y, p3.y, p4.y}) - half_th,
        (std::max)({p1.x, p2.x, p3.x, p4.x}) + half_th,
        (std::max)({p1.y, p2.y, p3.y, p4.y}) + half_th
    );

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordQuadFilled(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::QuadFilled;
    cmd.p1 = p1; cmd.p2 = p2; cmd.p3 = p3; cmd.p4 = p4;
    cmd.col = col;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.bbox = ImVec4(
        (std::min)({p1.x, p2.x, p3.x, p4.x}),
        (std::min)({p1.y, p2.y, p3.y, p4.y}),
        (std::max)({p1.x, p2.x, p3.x, p4.x}),
        (std::max)({p1.y, p2.y, p3.y, p4.y})
    );

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordTriangle(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Triangle;
    cmd.p1 = p1; cmd.p2 = p2; cmd.p3 = p3;
    cmd.col = col;
    cmd.thickness = thickness;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float half_th = thickness * 0.5f;
    cmd.bbox = ImVec4(
        (std::min)({p1.x, p2.x, p3.x}) - half_th,
        (std::min)({p1.y, p2.y, p3.y}) - half_th,
        (std::max)({p1.x, p2.x, p3.x}) + half_th,
        (std::max)({p1.y, p2.y, p3.y}) + half_th
    );

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordTriangleFilled(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::TriangleFilled;
    cmd.p1 = p1; cmd.p2 = p2; cmd.p3 = p3;
    cmd.col = col;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.bbox = ImVec4(
        (std::min)({p1.x, p2.x, p3.x}),
        (std::min)({p1.y, p2.y, p3.y}),
        (std::max)({p1.x, p2.x, p3.x}),
        (std::max)({p1.y, p2.y, p3.y})
    );

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordCircle(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Circle;
    cmd.p1 = center;
    cmd.radius = radius;
    cmd.col = col;
    cmd.num_segments = num_segments;
    cmd.thickness = thickness;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float ext = radius + thickness * 0.5f;
    cmd.bbox = ImVec4(center.x - ext, center.y - ext, center.x + ext, center.y + ext);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordCircleFilled(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::CircleFilled;
    cmd.p1 = center;
    cmd.radius = radius;
    cmd.col = col;
    cmd.num_segments = num_segments;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.bbox = ImVec4(center.x - radius, center.y - radius, center.x + radius, center.y + radius);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordNgon(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) {
    return RecordCircle(dl, center, radius, col, num_segments, thickness);
}

bool Recorder::RecordNgonFilled(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments) {
    return RecordCircleFilled(dl, center, radius, col, num_segments);
}

bool Recorder::RecordEllipse(ImDrawList* dl, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments, float thickness) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Ellipse;
    cmd.p1 = center;
    cmd.p2 = radius;
    cmd.rounding = rot;
    cmd.col = col;
    cmd.num_segments = num_segments;
    cmd.thickness = thickness;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float max_r = (std::max)(radius.x, radius.y) + thickness * 0.5f;
    cmd.bbox = ImVec4(center.x - max_r, center.y - max_r, center.x + max_r, center.y + max_r);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordEllipseFilled(ImDrawList* dl, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::EllipseFilled;
    cmd.p1 = center;
    cmd.p2 = radius;
    cmd.rounding = rot;
    cmd.col = col;
    cmd.num_segments = num_segments;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float max_r = (std::max)(radius.x, radius.y);
    cmd.bbox = ImVec4(center.x - max_r, center.y - max_r, center.x + max_r, center.y + max_r);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordBezierCubic(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::BezierCubic;
    cmd.p1 = p1; cmd.p2 = p2; cmd.p3 = p3; cmd.p4 = p4;
    cmd.col = col;
    cmd.thickness = thickness;
    cmd.num_segments = num_segments;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float half_th = thickness * 0.5f;
    cmd.bbox = ImVec4(
        (std::min)({p1.x, p2.x, p3.x, p4.x}) - half_th,
        (std::min)({p1.y, p2.y, p3.y, p4.y}) - half_th,
        (std::max)({p1.x, p2.x, p3.x, p4.x}) + half_th,
        (std::max)({p1.y, p2.y, p3.y, p4.y}) + half_th
    );

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordBezierQuadratic(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::BezierQuadratic;
    cmd.p1 = p1; cmd.p2 = p2; cmd.p3 = p3;
    cmd.col = col;
    cmd.thickness = thickness;
    cmd.num_segments = num_segments;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    float half_th = thickness * 0.5f;
    cmd.bbox = ImVec4(
        (std::min)({p1.x, p2.x, p3.x}) - half_th,
        (std::min)({p1.y, p2.y, p3.y}) - half_th,
        (std::max)({p1.x, p2.x, p3.x}) + half_th,
        (std::max)({p1.y, p2.y, p3.y}) + half_th
    );

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordPolyline(ImDrawList* dl, const ImVec2* points, int count, ImU32 col, float thickness, ImDrawFlags flags) {
    if (!m_enabled || count < 2) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Polyline;
    cmd.col = col;
    cmd.thickness = thickness;
    cmd.flags = (uint16_t)flags;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    cmd.data_offset = (uint32_t)stream.points_pool.size();
    cmd.data_size = (uint32_t)count;
    stream.points_pool.insert(stream.points_pool.end(), points, points + count);

    float half_th = thickness * 0.5f;
    ImVec4 bbox(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = 0; i < count; ++i) {
        if (points[i].x < bbox.x) bbox.x = points[i].x;
        if (points[i].y < bbox.y) bbox.y = points[i].y;
        if (points[i].x > bbox.z) bbox.z = points[i].x;
        if (points[i].y > bbox.w) bbox.w = points[i].y;
    }
    cmd.bbox = ImVec4(bbox.x - half_th, bbox.y - half_th, bbox.z + half_th, bbox.w + half_th);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordConvexPolyFilled(ImDrawList* dl, const ImVec2* points, int count, ImU32 col) {
    if (!m_enabled || count < 3) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::ConvexPolyFilled;
    cmd.col = col;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);

    cmd.data_offset = (uint32_t)stream.points_pool.size();
    cmd.data_size = (uint32_t)count;
    stream.points_pool.insert(stream.points_pool.end(), points, points + count);

    ImVec4 bbox(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = 0; i < count; ++i) {
        if (points[i].x < bbox.x) bbox.x = points[i].x;
        if (points[i].y < bbox.y) bbox.y = points[i].y;
        if (points[i].x > bbox.z) bbox.z = points[i].x;
        if (points[i].y > bbox.w) bbox.w = points[i].y;
    }
    cmd.bbox = bbox;

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordConcavePolyFilled(ImDrawList* dl, const ImVec2* points, int count, ImU32 col) {
    if (!m_enabled || count < 3) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::ConcavePolyFilled;
    cmd.col = col;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.data_offset = (uint32_t)stream.points_pool.size();
    cmd.data_size = (uint32_t)count;

    ImVec4 bbox(FLT_MAX, FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int i = 0; i < count; ++i) {
        stream.points_pool.push_back(points[i]);
        if (points[i].x < bbox.x) bbox.x = points[i].x;
        if (points[i].y < bbox.y) bbox.y = points[i].y;
        if (points[i].x > bbox.z) bbox.z = points[i].x;
        if (points[i].y > bbox.w) bbox.w = points[i].y;
    }
    cmd.bbox = bbox;

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordText(ImDrawList* dl, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    if (text_end == nullptr) {
        text_end = text_begin + strlen(text_begin);
    }
    size_t len = text_end - text_begin;
    if (len == 0) return true;

    DrawCommand cmd;
    cmd.type = CmdType::Text;
    cmd.font = font;
    cmd.font_size = font_size;
    cmd.p1 = pos;
    cmd.col = col;
    cmd.wrap_width = wrap_width;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    if (cpu_fine_clip_rect) {
        cmd.clip_rect.x = (std::max)(cmd.clip_rect.x, cpu_fine_clip_rect->x);
        cmd.clip_rect.y = (std::max)(cmd.clip_rect.y, cpu_fine_clip_rect->y);
        cmd.clip_rect.z = (std::min)(cmd.clip_rect.z, cpu_fine_clip_rect->z);
        cmd.clip_rect.w = (std::min)(cmd.clip_rect.w, cpu_fine_clip_rect->w);
    }

    cmd.data_offset = (uint32_t)stream.text_pool.size();
    cmd.data_size = (uint32_t)len;
    stream.text_pool.insert(stream.text_pool.end(), text_begin, text_end);
    stream.text_pool.push_back('\0'); // null-terminate in pool for convenience

    // Calculate text size / bbox
    ImVec2 text_size = font ? font->CalcTextSizeA(font_size, FLT_MAX, wrap_width, text_begin, text_end) : ImVec2(100.0f, font_size);
    cmd.bbox = ImVec4(pos.x, pos.y, pos.x + text_size.x, pos.y + text_size.y);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordImage(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Image;
    cmd.texture_id = tex_ref._TexData ? (ImTextureID)tex_ref._TexData : (ImTextureID)0;
    cmd.p1 = p_min;
    cmd.p2 = p_max;
    cmd.p3 = uv_min;
    cmd.p4 = uv_max;
    cmd.col = col;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.bbox = ImVec4(p_min.x, p_min.y, p_max.x, p_max.y);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordImageQuad(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4, ImU32 col) {
    (void)uv1; (void)uv2; (void)uv3; (void)uv4;
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::ImageQuad;
    cmd.texture_id = tex_ref._TexData ? (ImTextureID)tex_ref._TexData : (ImTextureID)0;
    cmd.p1 = p1; cmd.p2 = p2; cmd.p3 = p3; cmd.p4 = p4;
    cmd.col = col;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.bbox = ImVec4(
        (std::min)({p1.x, p2.x, p3.x, p4.x}),
        (std::min)({p1.y, p2.y, p3.y, p4.y}),
        (std::max)({p1.x, p2.x, p3.x, p4.x}),
        (std::max)({p1.y, p2.y, p3.y, p4.y})
    );

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordImageRounded(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float rounding, ImDrawFlags flags) {
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::ImageRounded;
    cmd.texture_id = tex_ref._TexData ? (ImTextureID)tex_ref._TexData : (ImTextureID)0;
    cmd.p1 = p_min;
    cmd.p2 = p_max;
    cmd.p3 = uv_min;
    cmd.p4 = uv_max;
    cmd.col = col;
    cmd.rounding = rounding;
    cmd.flags = (uint16_t)flags;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.bbox = ImVec4(p_min.x, p_min.y, p_max.x, p_max.y);

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

bool Recorder::RecordCallback(ImDrawList* dl, ImDrawCallback callback, void* userdata, size_t userdata_size) {
    (void)userdata; (void)userdata_size;
    if (!m_enabled) return false;
    FlushFallbackMesh(dl);
    auto& stream = GetStream(dl);

    DrawCommand cmd;
    cmd.type = CmdType::Callback;
    cmd.clip_rect = GetCurrentClipRect(stream, dl);
    cmd.texture_id = (ImTextureID)(intptr_t)callback;

    stream.GetCurrentCmdList().push_back(cmd);
    stream.last_recorded_vtx_count = (size_t)dl->VtxBuffer.Size;
    stream.last_recorded_idx_count = (size_t)dl->IdxBuffer.Size;
    return true;
}

void HookReset(ImDrawList* dl) {
    Recorder::Instance().OnReset(dl);
}

void HookPushClipRect(ImDrawList* dl, const ImVec2& cr_min, const ImVec2& cr_max, bool intersect) {
    Recorder::Instance().OnPushClipRect(dl, cr_min, cr_max, intersect);
}

void HookPopClipRect(ImDrawList* dl) {
    Recorder::Instance().OnPopClipRect(dl);
}

void HookPushTexture(ImDrawList* dl, ImTextureRef tex_ref) {
    Recorder::Instance().OnPushTexture(dl, tex_ref);
}

void HookPopTexture(ImDrawList* dl) {
    Recorder::Instance().OnPopTexture(dl);
}

bool HookCallback(ImDrawList* dl, ImDrawCallback callback, void* userdata, size_t userdata_size) {
    return Recorder::Instance().RecordCallback(dl, callback, userdata, userdata_size);
}

void HookSplitterSplit(ImDrawListSplitter* splitter, ImDrawList* dl, int count) {
    Recorder::Instance().OnSplitterSplit(splitter, dl, count);
}

void HookSplitterSetChannel(ImDrawListSplitter* splitter, ImDrawList* dl, int idx) {
    Recorder::Instance().OnSplitterSetChannel(splitter, dl, idx);
}

void HookSplitterMerge(ImDrawListSplitter* splitter, ImDrawList* dl) {
    Recorder::Instance().OnSplitterMerge(splitter, dl);
}

bool HookLine(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness) {
    return Recorder::Instance().RecordLine(dl, p1, p2, col, thickness);
}

bool HookRect(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, float thickness, ImDrawFlags flags) {
    return Recorder::Instance().RecordRect(dl, p_min, p_max, col, rounding, thickness, flags);
}

bool HookRectFilled(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags) {
    return Recorder::Instance().RecordRectFilled(dl, p_min, p_max, col, rounding, flags);
}

bool HookRectFilledMultiColor(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 c1, ImU32 c2, ImU32 c3, ImU32 c4) {
    return Recorder::Instance().RecordRectFilledMultiColor(dl, p_min, p_max, c1, c2, c3, c4);
}

bool HookQuad(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness) {
    return Recorder::Instance().RecordQuad(dl, p1, p2, p3, p4, col, thickness);
}

bool HookQuadFilled(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col) {
    return Recorder::Instance().RecordQuadFilled(dl, p1, p2, p3, p4, col);
}

bool HookTriangle(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness) {
    return Recorder::Instance().RecordTriangle(dl, p1, p2, p3, col, thickness);
}

bool HookTriangleFilled(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col) {
    return Recorder::Instance().RecordTriangleFilled(dl, p1, p2, p3, col);
}

bool HookCircle(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) {
    return Recorder::Instance().RecordCircle(dl, center, radius, col, num_segments, thickness);
}

bool HookCircleFilled(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments) {
    return Recorder::Instance().RecordCircleFilled(dl, center, radius, col, num_segments);
}

bool HookNgon(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) {
    return Recorder::Instance().RecordNgon(dl, center, radius, col, num_segments, thickness);
}

bool HookNgonFilled(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments) {
    return Recorder::Instance().RecordNgonFilled(dl, center, radius, col, num_segments);
}

bool HookEllipse(ImDrawList* dl, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments, float thickness) {
    return Recorder::Instance().RecordEllipse(dl, center, radius, col, rot, num_segments, thickness);
}

bool HookEllipseFilled(ImDrawList* dl, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments) {
    return Recorder::Instance().RecordEllipseFilled(dl, center, radius, col, rot, num_segments);
}

bool HookBezierCubic(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments) {
    return Recorder::Instance().RecordBezierCubic(dl, p1, p2, p3, p4, col, thickness, num_segments);
}

bool HookBezierQuadratic(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments) {
    return Recorder::Instance().RecordBezierQuadratic(dl, p1, p2, p3, col, thickness, num_segments);
}

bool HookPolyline(ImDrawList* dl, const ImVec2* points, int count, ImU32 col, float thickness, ImDrawFlags flags) {
    return Recorder::Instance().RecordPolyline(dl, points, count, col, thickness, flags);
}

bool HookConvexPolyFilled(ImDrawList* dl, const ImVec2* points, int count, ImU32 col) {
    return Recorder::Instance().RecordConvexPolyFilled(dl, points, count, col);
}

bool HookConcavePolyFilled(ImDrawList* dl, const ImVec2* points, int count, ImU32 col) {
    return Recorder::Instance().RecordConcavePolyFilled(dl, points, count, col);
}

bool HookText(ImDrawList* dl, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) {
    return Recorder::Instance().RecordText(dl, font, font_size, pos, col, text_begin, text_end, wrap_width, cpu_fine_clip_rect);
}

bool HookImage(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col) {
    return Recorder::Instance().RecordImage(dl, tex_ref, p_min, p_max, uv_min, uv_max, col);
}

bool HookImageQuad(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4, ImU32 col) {
    return Recorder::Instance().RecordImageQuad(dl, tex_ref, p1, p2, p3, p4, uv1, uv2, uv3, uv4, col);
}

bool HookImageRounded(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float rounding, ImDrawFlags flags) {
    return Recorder::Instance().RecordImageRounded(dl, tex_ref, p_min, p_max, uv_min, uv_max, col, rounding, flags);
}

bool HookHasInterception(ImDrawList* dl) {
    if (!Recorder::Instance().IsEnabled()) return false;
    auto& stream = Recorder::Instance().GetStream(dl);
    return !stream.commands.empty();
}

} // namespace ImGuiExt
