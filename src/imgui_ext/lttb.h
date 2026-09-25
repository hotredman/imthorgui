#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include "imgui.h"

namespace ImGuiExt {

// Largest-Triangle-Three-Buckets (LTTB) downsampling algorithm
// Preserves visual peaks, valleys, and waveform shape when reducing
// large datasets (e.g. 100,000 points) to screen resolution (e.g. 800 pixels).
class LTTB {
public:
    struct DataPoint {
        float x;
        float y;
    };

    // Downsamples raw time-series data:
    // data_y: array of N values
    // count: number of input values
    // out: pre-allocated output array of ImVec2 (size >= threshold)
    // threshold: target number of points (typically screen width in pixels)
    // screen_pos: top-left coordinate of widget
    // screen_size: width and height of widget
    // y_min, y_max: value range for vertical scaling
    static size_t Downsample(const float* data_y, size_t count,
                             ImVec2* out, size_t threshold,
                             const ImVec2& screen_pos, const ImVec2& screen_size,
                             float y_min, float y_max) {
        if (count == 0 || threshold == 0) return 0;

        // If dataset is smaller than threshold, map points directly
        if (threshold >= count || threshold <= 2) {
            float dx = count > 1 ? (screen_size.x / (float)(count - 1)) : 0.0f;
            float y_range = (y_max - y_min) > 1e-5f ? (y_max - y_min) : 1.0f;

            for (size_t i = 0; i < count; ++i) {
                float norm_y = (data_y[i] - y_min) / y_range;
                norm_y = (std::max)(0.0f, (std::min)(1.0f, norm_y));
                out[i] = ImVec2(screen_pos.x + (float)i * dx,
                                screen_pos.y + screen_size.y * (1.0f - norm_y));
            }
            return count;
        }

        float y_range = (y_max - y_min) > 1e-5f ? (y_max - y_min) : 1.0f;
        auto map_point = [&](size_t idx, float y_val) -> ImVec2 {
            float norm_x = (float)idx / (float)(count - 1);
            float norm_y = (y_val - y_min) / y_range;
            norm_y = (std::max)(0.0f, (std::min)(1.0f, norm_y));
            return ImVec2(screen_pos.x + norm_x * screen_size.x,
                          screen_pos.y + screen_size.y * (1.0f - norm_y));
        };

        // Bucket size for middle buckets (excluding first and last)
        double every = (double)(count - 2) / (double)(threshold - 2);

        size_t a_idx = 0;
        out[0] = map_point(0, data_y[0]);
        size_t out_idx = 1;

        for (size_t i = 0; i < threshold - 2; ++i) {
            // Calculate average point for the next bucket (bucket i + 1)
            size_t avg_range_start = (size_t)floor((double)(i + 1) * every) + 1;
            size_t avg_range_end = (size_t)floor((double)(i + 2) * every) + 1;
            if (avg_range_end > count) avg_range_end = count;

            double avg_x = 0.0;
            double avg_y = 0.0;
            size_t avg_count = avg_range_end - avg_range_start;

            if (avg_count > 0) {
                for (size_t j = avg_range_start; j < avg_range_end; ++j) {
                    avg_x += (double)j;
                    avg_y += (double)data_y[j];
                }
                avg_x /= (double)avg_count;
                avg_y /= (double)avg_count;
            } else {
                avg_x = (double)avg_range_start;
                avg_y = (double)data_y[(std::min)(avg_range_start, count - 1)];
            }

            // Current bucket range (bucket i)
            size_t range_offs = (size_t)floor((double)i * every) + 1;
            size_t range_to = (size_t)floor((double)(i + 1) * every) + 1;
            if (range_to > count) range_to = count;

            // Point A from previous bucket
            double point_a_x = (double)a_idx;
            double point_a_y = (double)data_y[a_idx];

            double max_area = -1.0;
            size_t max_area_idx = range_offs;

            // Find point in current bucket that maximizes triangle area (A, B, Avg)
            for (size_t j = range_offs; j < range_to; ++j) {
                double point_b_x = (double)j;
                double point_b_y = (double)data_y[j];

                // Area = 0.5 * |(Ax - Cx)(By - Ay) - (Ax - Bx)(Cy - Ay)|
                double area = std::abs((point_a_x - avg_x) * (point_b_y - point_a_y) -
                                       (point_a_x - point_b_x) * (avg_y - point_a_y));

                if (area > max_area) {
                    max_area = area;
                    max_area_idx = j;
                }
            }

            out[out_idx++] = map_point(max_area_idx, data_y[max_area_idx]);
            a_idx = max_area_idx; // Next point A
        }

        // Last point is always included
        out[out_idx++] = map_point(count - 1, data_y[count - 1]);
        return out_idx;
    }
};

} // namespace ImGuiExt
