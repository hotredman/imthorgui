#include <iostream>
#include <iomanip>
#include <cassert>
#include "imgui.h"
#include "imgui_ext/recorder.h"
#include "imgui_ext/renderer.h"

int main() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(800, 600);
    io.DeltaTime = 1.0f / 60.0f;
    io.IniFilename = nullptr;

    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    ImGuiExt::SetVectorInterception(true);
    ImGuiExt::Recorder::Instance().Reset();

    // Frame 1: Request modal popup open
    ImGui::NewFrame();
    ImGui::OpenPopup("Delete?");
    if (ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Modal Content");
        ImGui::EndPopup();
    }
    ImGui::Render();
    ImGuiExt::Recorder::Instance().EndFrame();

    // Frame 2: Modal popup is now active and visible
    ImGui::NewFrame();
    bool modal_active = ImGui::BeginPopupModal("Delete?", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    std::cout << "BeginPopupModal returned: " << (modal_active ? "true" : "false") << "\n";
    if (modal_active) {
        ImGui::Text("All those beautiful files will be deleted.");
        ImGui::Button("OK");
        ImGui::EndPopup();
    }
    ImGui::Render();
    ImGuiExt::Recorder::Instance().EndFrame();

    ImDrawData* dd = ImGui::GetDrawData();
    std::cout << "DrawData CmdLists count: " << dd->CmdLists.Size << "\n";
    if (dd->CmdLists.Size < 1) {
        std::cerr << "Error: Expected at least 1 DrawList for modal window\n";
        return 1;
    }

    ImDrawList* dl = dd->CmdLists[0];
    auto& stream = ImGuiExt::Recorder::Instance().GetStream(dl);

    // Verify stream commands order:
    // Modal dimming background commands (PushClipRect, RectFilled, PopClipRect)
    // MUST be at the front (after initial PushTexture), NOT at the back!
    bool found_dim_rect_at_front = false;
    for (size_t i = 0; i < std::min<size_t>(stream.commands.size(), 4); ++i) {
        const auto& c = stream.commands[i];
        if (c.type == ImGuiExt::CmdType::RectFilled &&
            c.p1.x == 0.0f && c.p1.y == 0.0f &&
            c.p2.x == 800.0f && c.p2.y == 600.0f) {
            found_dim_rect_at_front = true;
            std::cout << ">>> Found fullscreen dimming RectFilled at index " << i << " (FRONT of stream)!\n";
            break;
        }
    }

    if (!found_dim_rect_at_front) {
        std::cerr << "[FAIL] Fullscreen dimming rectangle was not moved to the front of stream.commands!\n";
        return 1;
    }

    // Verify trailing command is NOT the fullscreen dimming rectangle
    const auto& last_cmd = stream.commands.back();
    if (last_cmd.type == ImGuiExt::CmdType::RectFilled &&
        last_cmd.p1.x == 0.0f && last_cmd.p1.y == 0.0f &&
        last_cmd.p2.x == 800.0f && last_cmd.p2.y == 600.0f) {
        std::cerr << "[FAIL] Fullscreen dimming rectangle is still at the tail of stream.commands!\n";
        return 1;
    }

    // Render with ThorVG and verify pixel rasterization
    std::unique_ptr<ImGuiExt::IRenderer> renderer(ImGuiExt::CreateThorVGRenderer());
    renderer->Init(800, 600);
    renderer->RenderDrawData(dd);
    const uint32_t* fb = renderer->GetPixelBuffer();
    if (!fb) {
        std::cerr << "[FAIL] Pixel buffer from ThorVG is null\n";
        return 1;
    }

    // Pixel at (10, 10) is background covered only by modal dimming
    uint32_t bg_pixel = fb[10 * 800 + 10];
    // Pixel at (400, 310) is inside the modal window
    uint32_t modal_pixel = fb[310 * 800 + 400];

    std::cout << "Pixel outside modal (10, 10):   0x" << std::hex << bg_pixel << std::dec << "\n";
    std::cout << "Pixel inside modal  (400, 310): 0x" << std::hex << modal_pixel << std::dec << "\n";

    // Outside pixel must be dimmed (non-zero alpha)
    uint8_t bg_a = (bg_pixel >> 24) & 0xff;
    if (bg_a == 0) {
        std::cerr << "[FAIL] Background pixel outside modal should have dimming color!\n";
        return 1;
    }

    // Inside modal pixel must have full window opacity and not be pure background
    uint8_t modal_a = (modal_pixel >> 24) & 0xff;
    if (modal_a < 0xe0) {
        std::cerr << "[FAIL] Modal window pixel alpha is unexpectedly low!\n";
        return 1;
    }

    std::cout << "\n>>> [PASS] Modal dimming order verified! Dimming is rendered behind modal window.\n";

    ImGui::DestroyContext();
    return 0;
}
