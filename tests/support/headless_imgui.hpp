#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <imgui.h>
#include <imgui_impl_null.h>
#include <imgui_internal.h>

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
    void set_display_size(float width, float height)
    {
        display_size_ = ImVec2{width, height};
    }

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

    /// Presses a key while a modifier is held, e.g. Ctrl+M.
    template <class Draw>
    auto press_chord(ImGuiKey modifier, ImGuiKey key, Draw&& draw)
    {
        ImGui::GetIO().AddKeyEvent(modifier, true);
        auto result = press(key, draw);
        ImGui::GetIO().AddKeyEvent(modifier, false);
        std::ignore = frame(draw);
        return result;
    }

    /// Activates the item `id` as a click would, over two frames, returning the first frame's
    /// result. ImGui queues the activation for the next frame that submits the item.
    template <class Draw>
    auto activate(ImGuiID id, Draw&& draw)
    {
        ImGui::ActivateItemByID(id);
        auto result = frame(draw);
        std::ignore = frame(draw);
        return result;
    }

private:
    ImVec2 display_size_{1920.0F, 1080.0F};
};

/// The id of a widget submitted at the top level of window `window`. `label` is the widget's
/// label, or just its "###id" part when the visible caption varies. The window must exist.
[[nodiscard]] inline ImGuiID item_id(std::string_view window, std::string_view label)
{
    const ImGuiWindow* found = ImGui::FindWindowByName(std::string{window}.c_str());
    return found == nullptr ? 0 : ImGui::GetIDWithSeed(label.data(), label.data() + label.size(), found->ID);
}

} // namespace lxe::test
