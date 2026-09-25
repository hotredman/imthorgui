#pragma once

#include <cstddef>

#ifndef SDL_SCALEMODE_INVALID
#define SDL_SCALEMODE_INVALID ((SDL_ScaleMode)-1)
#endif

// Forward declarations of ImGui types so this header has zero dependencies
// and causes no cyclic header resolution when included from imconfig.h
struct ImDrawList;
struct ImDrawListSplitter;
struct ImTextureRef;
struct ImFont;
struct ImVec2;
struct ImVec4;
struct ImDrawCmd;
typedef unsigned int ImU32;
typedef int ImDrawFlags;
typedef void (*ImDrawCallback)(const ImDrawList* parent_list, const ImDrawCmd* cmd);

namespace ImGuiExt {

void HookReset(ImDrawList* dl);
void HookPushClipRect(ImDrawList* dl, const ImVec2& cr_min, const ImVec2& cr_max, bool intersect);
void HookPopClipRect(ImDrawList* dl);
void HookPushTexture(ImDrawList* dl, ImTextureRef tex_ref);
void HookPopTexture(ImDrawList* dl);
bool HookCallback(ImDrawList* dl, ImDrawCallback callback, void* userdata, size_t userdata_size);

void HookSplitterSplit(ImDrawListSplitter* splitter, ImDrawList* dl, int count);
void HookSplitterSetChannel(ImDrawListSplitter* splitter, ImDrawList* dl, int idx);
void HookSplitterMerge(ImDrawListSplitter* splitter, ImDrawList* dl);

bool HookLine(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness);
bool HookRect(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, float thickness, ImDrawFlags flags);
bool HookRectFilled(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags);
bool HookRectFilledMultiColor(ImDrawList* dl, const ImVec2& p_min, const ImVec2& p_max, ImU32 c1, ImU32 c2, ImU32 c3, ImU32 c4);
bool HookQuad(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness);
bool HookQuadFilled(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col);
bool HookTriangle(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness);
bool HookTriangleFilled(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col);
bool HookCircle(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness);
bool HookCircleFilled(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments);
bool HookNgon(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness);
bool HookNgonFilled(ImDrawList* dl, const ImVec2& center, float radius, ImU32 col, int num_segments);
bool HookEllipse(ImDrawList* dl, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments, float thickness);
bool HookEllipseFilled(ImDrawList* dl, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments);
bool HookBezierCubic(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments);
bool HookBezierQuadratic(ImDrawList* dl, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments);
bool HookPolyline(ImDrawList* dl, const ImVec2* points, int count, ImU32 col, float thickness, ImDrawFlags flags);
bool HookConvexPolyFilled(ImDrawList* dl, const ImVec2* points, int count, ImU32 col);
bool HookConcavePolyFilled(ImDrawList* dl, const ImVec2* points, int count, ImU32 col);
bool HookText(ImDrawList* dl, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect);
bool HookImage(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col);
bool HookImageQuad(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4, ImU32 col);
bool HookImageRounded(ImDrawList* dl, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float rounding, ImDrawFlags flags);
bool HookHasInterception(ImDrawList* dl);

} // namespace ImGuiExt

#define IMGUI_DRAWLIST_HOOK_RESET(dl) \
    ::ImGuiExt::HookReset(dl)

#define IMGUI_DRAWLIST_HOOK_PUSH_CLIP_RECT(dl, min, max, intersect) \
    ::ImGuiExt::HookPushClipRect(dl, min, max, intersect)

#define IMGUI_DRAWLIST_HOOK_POP_CLIP_RECT(dl) \
    ::ImGuiExt::HookPopClipRect(dl)

#define IMGUI_DRAWLIST_HOOK_PUSH_TEXTURE(dl, tex) \
    ::ImGuiExt::HookPushTexture(dl, tex)

#define IMGUI_DRAWLIST_HOOK_POP_TEXTURE(dl) \
    ::ImGuiExt::HookPopTexture(dl)

#define IMGUI_DRAWLIST_HOOK_CALLBACK(dl, cb, data, sz) \
    ::ImGuiExt::HookCallback(dl, cb, data, sz)

#define IMGUI_DRAWLIST_HOOK_LINE(dl, p1, p2, col, t) \
    ::ImGuiExt::HookLine(dl, p1, p2, col, t)

#define IMGUI_DRAWLIST_HOOK_LINE_H(dl, min_x, max_x, y, col, t) \
    ::ImGuiExt::HookLine(dl, ImVec2(min_x, y), ImVec2(max_x, y), col, t)

#define IMGUI_DRAWLIST_HOOK_LINE_V(dl, x, min_y, max_y, col, t) \
    ::ImGuiExt::HookLine(dl, ImVec2(x, min_y), ImVec2(x, max_y), col, t)

#define IMGUI_DRAWLIST_HOOK_RECT(dl, min, max, col, r, t, flags) \
    ::ImGuiExt::HookRect(dl, min, max, col, r, t, flags)

#define IMGUI_DRAWLIST_HOOK_RECT_FILLED(dl, min, max, col, r, flags) \
    ::ImGuiExt::HookRectFilled(dl, min, max, col, r, flags)

#define IMGUI_DRAWLIST_HOOK_RECT_FILLED_MULTICOLOR(dl, min, max, c1, c2, c3, c4) \
    ::ImGuiExt::HookRectFilledMultiColor(dl, min, max, c1, c2, c3, c4)

#define IMGUI_DRAWLIST_HOOK_QUAD(dl, p1, p2, p3, p4, col, t) \
    ::ImGuiExt::HookQuad(dl, p1, p2, p3, p4, col, t)

#define IMGUI_DRAWLIST_HOOK_QUAD_FILLED(dl, p1, p2, p3, p4, col) \
    ::ImGuiExt::HookQuadFilled(dl, p1, p2, p3, p4, col)

#define IMGUI_DRAWLIST_HOOK_TRIANGLE(dl, p1, p2, p3, col, t) \
    ::ImGuiExt::HookTriangle(dl, p1, p2, p3, col, t)

#define IMGUI_DRAWLIST_HOOK_TRIANGLE_FILLED(dl, p1, p2, p3, col) \
    ::ImGuiExt::HookTriangleFilled(dl, p1, p2, p3, col)

#define IMGUI_DRAWLIST_HOOK_CIRCLE(dl, c, r, col, seg, t) \
    ::ImGuiExt::HookCircle(dl, c, r, col, seg, t)

#define IMGUI_DRAWLIST_HOOK_CIRCLE_FILLED(dl, c, r, col, seg) \
    ::ImGuiExt::HookCircleFilled(dl, c, r, col, seg)

#define IMGUI_DRAWLIST_HOOK_NGON(dl, c, r, col, seg, t) \
    ::ImGuiExt::HookNgon(dl, c, r, col, seg, t)

#define IMGUI_DRAWLIST_HOOK_NGON_FILLED(dl, c, r, col, seg) \
    ::ImGuiExt::HookNgonFilled(dl, c, r, col, seg)

#define IMGUI_DRAWLIST_HOOK_ELLIPSE(dl, c, r, col, rot, seg, t) \
    ::ImGuiExt::HookEllipse(dl, c, r, col, rot, seg, t)

#define IMGUI_DRAWLIST_HOOK_ELLIPSE_FILLED(dl, c, r, col, rot, seg) \
    ::ImGuiExt::HookEllipseFilled(dl, c, r, col, rot, seg)

#define IMGUI_DRAWLIST_HOOK_BEZIER_CUBIC(dl, p1, p2, p3, p4, col, t, seg) \
    ::ImGuiExt::HookBezierCubic(dl, p1, p2, p3, p4, col, t, seg)

#define IMGUI_DRAWLIST_HOOK_BEZIER_QUADRATIC(dl, p1, p2, p3, col, t, seg) \
    ::ImGuiExt::HookBezierQuadratic(dl, p1, p2, p3, col, t, seg)

#define IMGUI_DRAWLIST_HOOK_POLYLINE(dl, pts, count, col, t, flags) \
    ::ImGuiExt::HookPolyline(dl, pts, count, col, t, flags)

#define IMGUI_DRAWLIST_HOOK_CONVEX_POLY_FILLED(dl, pts, count, col) \
    ::ImGuiExt::HookConvexPolyFilled(dl, pts, count, col)

#define IMGUI_DRAWLIST_HOOK_CONCAVE_POLY_FILLED(dl, pts, count, col) \
    ::ImGuiExt::HookConcavePolyFilled(dl, pts, count, col)

#define IMGUI_DRAWLIST_HOOK_TEXT(dl, font, sz, pos, col, tb, te, wrap, clip) \
    ::ImGuiExt::HookText(dl, font, sz, pos, col, tb, te, wrap, clip)

#define IMGUI_DRAWLIST_HOOK_IMAGE(dl, tex, min, max, uv_min, uv_max, col) \
    ::ImGuiExt::HookImage(dl, tex, min, max, uv_min, uv_max, col)

#define IMGUI_DRAWLIST_HOOK_IMAGE_QUAD(dl, tex, p1, p2, p3, p4, u1, u2, u3, u4, col) \
    ::ImGuiExt::HookImageQuad(dl, tex, p1, p2, p3, p4, u1, u2, u3, u4, col)

#define IMGUI_DRAWLIST_HOOK_IMAGE_ROUNDED(dl, tex, min, max, uv_min, uv_max, col, r, flags) \
    ::ImGuiExt::HookImageRounded(dl, tex, min, max, uv_min, uv_max, col, r, flags)

#define IMGUI_SPLITTER_HOOK_SPLIT(sp, dl, count) \
    ::ImGuiExt::HookSplitterSplit(sp, dl, count)

#define IMGUI_SPLITTER_HOOK_SET_CHANNEL(sp, dl, idx) \
    ::ImGuiExt::HookSplitterSetChannel(sp, dl, idx)

#define IMGUI_SPLITTER_HOOK_MERGE(sp, dl) \
    ::ImGuiExt::HookSplitterMerge(sp, dl)

#define IMGUI_DRAWLIST_HOOK_HAS_INTERCEPTION(dl) \
    ::ImGuiExt::HookHasInterception(dl)
