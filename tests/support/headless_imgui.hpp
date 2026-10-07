#pragma once

#include <imgui.h>
#include <imgui_impl_null.h>

#include <utility>

namespace lxe::test {

/// An ImGui context with the null backend: frames run without a window or GPU.
class HeadlessImGui
{
public:
    HeadlessImGui()
    {
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplNull_Init();
    }

    HeadlessImGui(const HeadlessImGui&) = delete;
    HeadlessImGui& operator=(const HeadlessImGui&) = delete;
    HeadlessImGui(HeadlessImGui&&) = delete;
    HeadlessImGui& operator=(HeadlessImGui&&) = delete;

    ~HeadlessImGui()
    {
        ImGui_ImplNull_Shutdown();
        ImGui::DestroyContext();
    }

    /// Window size for the next frames; small sizes make rows scroll out of view (clipping).
    void set_display_size(float width, float height) { display_size_ = ImVec2{width, height}; }

    /// Runs one frame around `draw` and returns what it returned.
    template <class Draw>
    auto frame(Draw&& draw)
    {
        ImGui_ImplNull_NewFrame();
        ImGui::GetIO().DisplaySize = display_size_;
        ImGui::NewFrame();
        auto result = std::forward<Draw>(draw)();
        ImGui::Render();
        ImGui_ImplNullRender_RenderDrawData(ImGui::GetDrawData());
        return result;
    }

    /// Presses and releases a key over two frames, returning the first frame's result.
    template <class Draw>
    auto press(ImGuiKey key, Draw&& draw)
    {
        ImGui::GetIO().AddKeyEvent(key, true);
        auto result = frame(draw);
        ImGui::GetIO().AddKeyEvent(key, false);
        std::ignore = frame(draw);
        return result;
    }

private:
    ImVec2 display_size_{1920.0F, 1080.0F};
};

} // namespace lxe::test
