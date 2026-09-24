#pragma once

#include "imgui.h"
#include <cstdint>
#include <cstddef>

namespace ImGuiExt {

class FrameDeduplicator {
public:
    static FrameDeduplicator& Instance() {
        static FrameDeduplicator instance;
        return instance;
    }

    void SetEnabled(bool enabled) {
        m_enabled = enabled;
        if (!enabled) {
            Reset();
        }
    }

    bool IsEnabled() const {
        return m_enabled;
    }

    void NotifyTextureUpdated() {
        m_texture_version++;
    }

    uint64_t GetTextureVersion() const {
        return m_texture_version;
    }

    void Reset() {
        m_has_last_hash = false;
        m_last_hash = 0;
    }

    uint64_t GetTotalFramesChecked() const { return m_total_checked; }
    uint64_t GetTotalFramesSkipped() const { return m_total_skipped; }

    // Returns true if the frame has changed and MUST be rendered.
    // Returns false if the frame is identical to the previous frame and render/present can be skipped.
    bool ShouldRenderFrame(const ImDrawData* draw_data) {
        if (!m_enabled || draw_data == nullptr || !draw_data->Valid) {
            return true;
        }

        m_total_checked++;

        uint64_t current_hash = ComputeDrawDataHash(draw_data);

        if (m_has_last_hash && current_hash == m_last_hash) {
            m_total_skipped++;
            return false; // Identical frame, skip rendering!
        }

        m_last_hash = current_hash;
        m_has_last_hash = true;
        return true;
    }

private:
    FrameDeduplicator() = default;

    bool m_enabled = true;
    bool m_has_last_hash = false;
    uint64_t m_last_hash = 0;
    uint64_t m_texture_version = 0;
    uint64_t m_total_checked = 0;
    uint64_t m_total_skipped = 0;

    static inline uint64_t FastHash(const void* data, size_t size, uint64_t seed = 0xcbf29ce484222325ULL) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint64_t hash = seed;

        // Process 8-byte chunks
        size_t words = size / 8;
        const uint64_t* p64 = reinterpret_cast<const uint64_t*>(ptr);
        for (size_t i = 0; i < words; ++i) {
            hash ^= p64[i];
            hash *= 0x100000001b3ULL;
        }

        // Process remaining bytes
        size_t rem = size % 8;
        for (size_t i = 0; i < rem; ++i) {
            hash ^= ptr[words * 8 + i];
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

    uint64_t ComputeDrawDataHash(const ImDrawData* draw_data) {
        uint64_t hash = 0xcbf29ce484222325ULL;

        // Hash header / viewport parameters
        struct HeaderState {
            float display_pos_x, display_pos_y;
            float display_size_x, display_size_y;
            float scale_x, scale_y;
            int cmd_lists_count;
            int total_vtx_count;
            int total_idx_count;
            uint64_t texture_version;
        } header = {
            draw_data->DisplayPos.x, draw_data->DisplayPos.y,
            draw_data->DisplaySize.x, draw_data->DisplaySize.y,
            draw_data->FramebufferScale.x, draw_data->FramebufferScale.y,
            draw_data->CmdListsCount,
            draw_data->TotalVtxCount,
            draw_data->TotalIdxCount,
            m_texture_version
        };

        hash = FastHash(&header, sizeof(header), hash);

        // Hash contents of each ImDrawList
        for (int i = 0; i < draw_data->CmdListsCount; ++i) {
            const ImDrawList* cmd_list = draw_data->CmdLists[i];
            if (!cmd_list) continue;

            // Hash command buffer (ClipRect, TextureId, ElemCount, offsets, callbacks)
            if (!cmd_list->CmdBuffer.empty()) {
                hash = FastHash(cmd_list->CmdBuffer.Data, cmd_list->CmdBuffer.size_in_bytes(), hash);
            }

            // Hash vertex buffer
            if (!cmd_list->VtxBuffer.empty()) {
                hash = FastHash(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.size_in_bytes(), hash);
            }

            // Hash index buffer
            if (!cmd_list->IdxBuffer.empty()) {
                hash = FastHash(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.size_in_bytes(), hash);
            }
        }

        return hash;
    }
};

// Global API
inline void SetFrameDeduplication(bool enabled) { FrameDeduplicator::Instance().SetEnabled(enabled); }
inline bool IsFrameDeduplicationEnabled() { return FrameDeduplicator::Instance().IsEnabled(); }
inline void NotifyTextureUpdated() { FrameDeduplicator::Instance().NotifyTextureUpdated(); }

} // namespace ImGuiExt
