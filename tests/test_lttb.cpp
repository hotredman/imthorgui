#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <cassert>
#include <iomanip>

#include "imgui.h"
#include "imgui_ext/lttb.h"

int main() {
    std::cout << "========================================================\n";
    std::cout << " Stage 4: LTTB Downsampling Algorithm Unit Test\n";
    std::cout << "========================================================\n\n";

    // 1. Create a large dataset (100,000 points) with a narrow critical spike (ECG R-peak)
    const size_t N = 100000;
    std::vector<float> signal(N, 0.0f);

    // Baseline sine wave
    for (size_t i = 0; i < N; ++i) {
        signal[i] = 0.5f * sinf((float)i * 0.005f);
    }

    // Place a sharp, narrow impulse spike (3 samples wide) right at index 42,371
    size_t spike_idx = 42371;
    signal[spike_idx - 1] = 1.8f;
    signal[spike_idx]     = 4.5f; // Extreme peak
    signal[spike_idx + 1] = 1.9f;

    std::cout << "[Test 1] Processing " << N << " raw points...\n";
    std::cout << "  Input peak at index " << spike_idx << ": " << signal[spike_idx] << " V\n";

    const size_t target_pixels = 800; // Screen resolution
    std::vector<ImVec2> out_points(target_pixels);

    ImVec2 screen_pos(0.0f, 0.0f);
    ImVec2 screen_size(800.0f, 400.0f);

    // Warm-up
    ImGuiExt::LTTB::Downsample(signal.data(), N, out_points.data(), target_pixels,
                               screen_pos, screen_size, -1.0f, 5.0f);

    // 2. Measure performance (50 runs)
    const int RUNS = 50;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int r = 0; r < RUNS; ++r) {
        ImGuiExt::LTTB::Downsample(signal.data(), N, out_points.data(), target_pixels,
                                   screen_pos, screen_size, -1.0f, 5.0f);
    }
    auto t1 = std::chrono::high_resolution_clock::now();
    double total_us = std::chrono::duration<double, std::micro>(t1 - t0).count();
    double avg_us = total_us / RUNS;
    double avg_ms = avg_us / 1000.0;

    std::cout << "\n[Performance Benchmark]:\n";
    std::cout << "  Dataset: " << N << " points -> " << target_pixels << " points\n";
    std::cout << "  Average execution time: " << std::fixed << std::setprecision(2)
              << avg_us << " us (" << avg_ms << " ms)\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(1)
              << ((double)N / avg_ms / 1000.0) << " Million points / sec\n";

    // 3. Verify Peak Preservation (LTTB MUST preserve the high value of the spike)
    float max_y_screen = 1e9f; // Lowest Y value on screen = highest value
    for (size_t i = 0; i < target_pixels; ++i) {
        if (out_points[i].y < max_y_screen) {
            max_y_screen = out_points[i].y;
        }
    }

    // Normalized Y for 4.5V with range [-1, 5] -> norm = (4.5 - (-1)) / 6 = 5.5 / 6 = ~0.916
    // Screen Y = 400 * (1 - 0.916) = ~33.3 px
    std::cout << "\n[Accuracy & Peak Preservation]:\n";
    std::cout << "  Screen Y coordinate of preserved peak: " << max_y_screen << " px (Expected: ~33.3 px)\n";

    bool peak_preserved = (max_y_screen < 50.0f); // Under 50px means peak >= 4.0V was captured!

    if (peak_preserved) {
        std::cout << ">>> [PASS] Critical peak preserved by LTTB downsampler!\n";
    } else {
        std::cerr << ">>> [FAIL] Critical peak was flattened by downsampler!\n";
        return 1;
    }

    if (avg_ms < 2.0) {
        std::cout << ">>> [PASS] High performance verified (< 2.0 ms for 100k points)!\n";
    } else {
        std::cerr << ">>> [FAIL] Execution time too slow (> 2.0 ms)\n";
        return 1;
    }

    std::cout << "\n========================================================\n";
    std::cout << " LTTB Downsampler Test passed successfully!\n";
    std::cout << "========================================================\n";
    return 0;
}
