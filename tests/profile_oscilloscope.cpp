#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

#include "imgui.h"
#include "imgui_ext/recorder.h"
#include "imgui_ext/renderer.h"
#include "imgui_ext/lttb.h"
#include "imgui_ext/oscilloscope.h"

int main() {
    std::cout << "========================================================\n";
    std::cout << " Profiling Oscilloscope & ThorVG Rendering Pipeline\n";
    std::cout << "========================================================\n\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(2040, 1200);
    io.IniFilename = nullptr;

    const char* font_path = "C:/Windows/Fonts/segoeui.ttf";
    io.Fonts->AddFontFromFileTTF(font_path, 19.0f * 1.5f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* dummy_pixels = nullptr;
    int dummy_w = 0, dummy_h = 0;
    io.Fonts->GetTexDataAsRGBA32(&dummy_pixels, &dummy_w, &dummy_h);

    auto* renderer = ImGuiExt::CreateThorVGRenderer();
    renderer->Init(2040, 1200);
    renderer->LoadFontFile(font_path);

    ImGuiExt::SetVectorInterception(true);

    ImGuiExt::OscilloscopeWidget widget;
    // Let signal generate some data
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Warm-up 3 frames
    for (int i = 0; i < 3; ++i) {
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        widget.RenderUI();
        ImGui::Render();
        ImGuiExt::Recorder::Instance().EndFrame();
        renderer->RenderDrawData(ImGui::GetDrawData());
        ImGuiExt::Recorder::Instance().Reset();
    }

    // Now run 30 frames with individual timers for each phase
    double time_newframe = 0.0;
    double time_demowin = 0.0;
    double time_widget = 0.0;
    double time_render = 0.0;
    double time_thorvg = 0.0;

    const int N = 30;
    for (int i = 0; i < N; ++i) {
        auto t0 = std::chrono::high_resolution_clock::now();
        ImGui::NewFrame();
        auto t1 = std::chrono::high_resolution_clock::now();

        ImGui::ShowDemoWindow();
        auto t2 = std::chrono::high_resolution_clock::now();

        widget.RenderUI();
        auto t3 = std::chrono::high_resolution_clock::now();

        ImGui::Render();
        ImDrawData* dd = ImGui::GetDrawData();
        ImGuiExt::Recorder::Instance().EndFrame();
        auto t4 = std::chrono::high_resolution_clock::now();

        renderer->Resize(2040, 1200);
        renderer->RenderDrawData(dd);
        auto t5 = std::chrono::high_resolution_clock::now();

        ImGuiExt::Recorder::Instance().Reset();

        time_newframe += std::chrono::duration<double, std::milli>(t1 - t0).count();
        time_demowin += std::chrono::duration<double, std::milli>(t2 - t1).count();
        time_widget += std::chrono::duration<double, std::milli>(t3 - t2).count();
        time_render += std::chrono::duration<double, std::milli>(t4 - t3).count();
        time_thorvg += std::chrono::duration<double, std::milli>(t5 - t4).count();
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Average breakdown per frame (" << N << " frames):\n";
    std::cout << "  1. ImGui::NewFrame():                 " << (time_newframe / N) << " ms\n";
    std::cout << "  2. ImGui::ShowDemoWindow():           " << (time_demowin / N) << " ms\n";
    std::cout << "  3. OscilloscopeWidget::RenderUI():    " << (time_widget / N) << " ms\n";
    std::cout << "  4. ImGui::Render() + Recorder:        " << (time_render / N) << " ms\n";
    std::cout << "  5. ThorVGRenderer::RenderDrawData():  " << (time_thorvg / N) << " ms\n";
    std::cout << "--------------------------------------------------------\n";
    double total = (time_newframe + time_demowin + time_widget + time_render + time_thorvg) / N;
    std::cout << "  TOTAL CPU FRAME TIME:                 " << total << " ms (" << (1000.0 / total) << " FPS)\n\n";

    renderer->Shutdown();
    delete renderer;
    ImGui::DestroyContext();
    return 0;
}
