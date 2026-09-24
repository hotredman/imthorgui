#include <iostream>

#include "imgui.h"
#include "imgui_ext/frame_dedup.h"
#include "imgui_ext/recorder.h"

int main() {
    std::cout << "========================================================\n";
    std::cout << " Stage 2/3: Frame Deduplication Unit Test\n";
    std::cout << "========================================================\n\n";

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;

    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    auto& dedup = ImGuiExt::FrameDeduplicator::Instance();
    dedup.SetEnabled(true);
    dedup.Reset();

    // ----------------------------------------------------
    // Test 1: Vector Interception Mode (Stage 3)
    // ----------------------------------------------------
    std::cout << "[Test 1] Vector Interception Mode...\n";
    ImGuiExt::SetVectorInterception(true);

    // Frame 1: Initial state
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Button("Click Me");
    ImGui::End();
    ImGui::Render();

    ImDrawData* dd1 = ImGui::GetDrawData();
    bool should_render1 = dedup.ShouldRenderFrame(dd1);
    std::cout << "  Frame 1 (Initial): ShouldRender = " << (should_render1 ? "true" : "false") << "\n";
    if (!should_render1) { std::cerr << "Error: Frame 1 should render\n"; return 1; }

    // Frame 2: Second frame (ImGui settles window sizing)
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Button("Click Me");
    ImGui::End();
    ImGui::Render();

    ImDrawData* dd2 = ImGui::GetDrawData();
    bool should_render2 = dedup.ShouldRenderFrame(dd2);
    std::cout << "  Frame 2 (Second): ShouldRender = " << (should_render2 ? "true" : "false") << "\n";

    // Frame 2b: Third identical frame - MUST be deduplicated
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Button("Click Me");
    ImGui::End();
    ImGui::Render();

    ImDrawData* dd2b = ImGui::GetDrawData();
    bool should_render2b = dedup.ShouldRenderFrame(dd2b);
    std::cout << "  Frame 2b (Third identical): ShouldRender = " << (should_render2b ? "true" : "false") << "\n";
    if (should_render2b) { std::cerr << "Error: Frame 2b should be deduplicated\n"; return 1; }

    // Frame 3: Mouse interaction simulation (hover button by moving mouse over button)
    io.AddMousePosEvent(50.0f, 40.0f);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Button("Click Me");
    ImGui::End();
    ImGui::Render();

    ImDrawData* dd3 = ImGui::GetDrawData();
    bool should_render3 = dedup.ShouldRenderFrame(dd3);
    std::cout << "  Frame 3 (Mouse Hover): ShouldRender = " << (should_render3 ? "true" : "false") << "\n";
    if (!should_render3) { std::cerr << "Error: Frame 3 should render on mouse hover\n"; return 1; }

    // Frame 4: Identical hovered state - MUST be deduplicated
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Button("Click Me");
    ImGui::End();
    ImGui::Render();

    ImDrawData* dd4 = ImGui::GetDrawData();
    bool should_render4 = dedup.ShouldRenderFrame(dd4);
    std::cout << "  Frame 4 (Hovered Identical): ShouldRender = " << (should_render4 ? "true" : "false") << "\n";
    if (should_render4) { std::cerr << "Error: Frame 4 should be deduplicated\n"; return 1; }

    // Frame 5: Texture update notification - MUST render
    dedup.NotifyTextureUpdated();
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Button("Click Me");
    ImGui::End();
    ImGui::Render();

    ImDrawData* dd5 = ImGui::GetDrawData();
    bool should_render5 = dedup.ShouldRenderFrame(dd5);
    std::cout << "  Frame 5 (NotifyTextureUpdated): ShouldRender = " << (should_render5 ? "true" : "false") << "\n";
    if (!should_render5) { std::cerr << "Error: Frame 5 should render after texture update\n"; return 1; }

    std::cout << ">>> [PASS] Vector interception deduplication verified!\n\n";

    // ----------------------------------------------------
    // Test 2: Stock ImGui Mesh Mode (Stage 0/1/2)
    // ----------------------------------------------------
    std::cout << "[Test 2] Stock Triangle Mesh Mode...\n";
    ImGuiExt::SetVectorInterception(false);
    dedup.Reset();

    // Frame 6: Initial stock
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Button("Click Me");
    ImGui::End();
    ImGui::Render();

    ImDrawData* dd6 = ImGui::GetDrawData();
    bool should_render6 = dedup.ShouldRenderFrame(dd6);
    std::cout << "  Frame 6 (Stock Initial): ShouldRender = " << (should_render6 ? "true" : "false") << "\n";
    if (!should_render6) { std::cerr << "Error: Frame 6 should render\n"; return 1; }

    // Frame 7: Identical stock
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(300, 200));
    ImGui::Begin("Test Window");
    ImGui::Button("Click Me");
    ImGui::End();
    ImGui::Render();

    ImDrawData* dd7 = ImGui::GetDrawData();
    bool should_render7 = dedup.ShouldRenderFrame(dd7);
    std::cout << "  Frame 7 (Stock Identical): ShouldRender = " << (should_render7 ? "true" : "false") << "\n";
    if (should_render7) { std::cerr << "Error: Frame 7 should be deduplicated\n"; return 1; }

    std::cout << ">>> [PASS] Stock mesh deduplication verified!\n\n";

    ImGui::DestroyContext();
    std::cout << "========================================================\n";
    std::cout << " All deduplication tests passed successfully!\n";
    std::cout << "========================================================\n";
    return 0;
}
