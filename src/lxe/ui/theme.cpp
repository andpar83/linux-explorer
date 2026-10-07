#include "lxe/ui/theme.hpp"

#include <cstdint>

namespace lxe::ui {
namespace {

/// Packed RGBA in ImGui's layout (what IM_COL32 produces, without its C-style casts).
constexpr ImU32 rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) noexcept
{
    constexpr unsigned g_shift = 8;
    constexpr unsigned b_shift = 16;
    constexpr unsigned a_shift = 24;
    return static_cast<ImU32>(r) | (static_cast<ImU32>(g) << g_shift) | (static_cast<ImU32>(b) << b_shift) |
           (static_cast<ImU32>(a) << a_shift);
}

void set_layout(ImGuiStyle& style)
{
    style.WindowPadding = ImVec2{10.0F, 8.0F};
    style.FramePadding = ImVec2{9.0F, 4.0F};
    style.ItemSpacing = ImVec2{8.0F, 6.0F};
    style.ItemInnerSpacing = ImVec2{6.0F, 4.0F};
    style.CellPadding = ImVec2{7.0F, 3.0F};
    style.ScrollbarSize = 13.0F;
    style.WindowRounding = 0.0F;
    style.ChildRounding = 4.0F;
    style.FrameRounding = 5.0F;
    style.PopupRounding = 5.0F;
    style.ScrollbarRounding = 6.0F;
    style.GrabRounding = 5.0F;
    style.WindowBorderSize = 0.0F;
    style.PopupBorderSize = 1.0F;
    style.TreeLinesFlags = ImGuiTreeNodeFlags_DrawLinesToNodes;
    style.TreeLinesSize = 1.0F;
    style.TreeLinesRounding = 3.0F;
}

Palette set_light_colors(ImGuiStyle& style)
{
    ImGui::StyleColorsLight(&style);
    auto& c = style.Colors;
    c[ImGuiCol_Text] = ImVec4{0.11F, 0.11F, 0.13F, 1.00F};
    c[ImGuiCol_TextDisabled] = ImVec4{0.50F, 0.50F, 0.54F, 1.00F};
    c[ImGuiCol_WindowBg] = ImVec4{0.97F, 0.97F, 0.98F, 1.00F};
    c[ImGuiCol_PopupBg] = ImVec4{1.00F, 1.00F, 1.00F, 0.98F};
    c[ImGuiCol_Border] = ImVec4{0.80F, 0.80F, 0.83F, 1.00F};
    c[ImGuiCol_FrameBg] = ImVec4{1.00F, 1.00F, 1.00F, 1.00F};
    c[ImGuiCol_Button] = ImVec4{0.91F, 0.91F, 0.93F, 1.00F};
    c[ImGuiCol_ButtonHovered] = ImVec4{0.85F, 0.88F, 0.95F, 1.00F};
    c[ImGuiCol_ButtonActive] = ImVec4{0.76F, 0.82F, 0.95F, 1.00F};
    c[ImGuiCol_Header] = ImVec4{0.24F, 0.52F, 0.95F, 0.30F};
    c[ImGuiCol_HeaderHovered] = ImVec4{0.24F, 0.52F, 0.95F, 0.12F};
    c[ImGuiCol_HeaderActive] = ImVec4{0.24F, 0.52F, 0.95F, 0.38F};
    c[ImGuiCol_TableHeaderBg] = ImVec4{0.92F, 0.92F, 0.94F, 1.00F};
    c[ImGuiCol_TableBorderStrong] = ImVec4{0.80F, 0.80F, 0.83F, 1.00F};
    c[ImGuiCol_TableBorderLight] = ImVec4{0.88F, 0.88F, 0.90F, 1.00F};
    c[ImGuiCol_TableRowBg] = ImVec4{1.00F, 1.00F, 1.00F, 1.00F};
    c[ImGuiCol_TableRowBgAlt] = ImVec4{0.965F, 0.965F, 0.975F, 1.00F};
    c[ImGuiCol_TreeLines] = ImVec4{0.70F, 0.70F, 0.74F, 1.00F};
    c[ImGuiCol_ScrollbarBg] = ImVec4{0.00F, 0.00F, 0.00F, 0.00F};
    c[ImGuiCol_Separator] = ImVec4{0.84F, 0.84F, 0.87F, 1.00F};
    style.FrameBorderSize = 1.0F;
    return Palette{
        .own_process_row = rgba(96, 140, 255, 46),
        .cpu_heat = rgba(255, 166, 0, 150),
        .kernel_thread_text = ImVec4{0.47F, 0.47F, 0.52F, 1.00F},
        .muted_text = ImVec4{0.33F, 0.33F, 0.38F, 1.00F},
        .accent_text = ImVec4{0.80F, 0.38F, 0.00F, 1.00F},
        .page_present = rgba(66, 133, 244, 255),
        .page_swapped = rgba(240, 150, 30, 255),
        .page_absent = rgba(220, 220, 226, 255),
    };
}

Palette set_dark_colors(ImGuiStyle& style)
{
    ImGui::StyleColorsDark(&style);
    auto& c = style.Colors;
    c[ImGuiCol_Text] = ImVec4{0.92F, 0.92F, 0.94F, 1.00F};
    c[ImGuiCol_TextDisabled] = ImVec4{0.52F, 0.52F, 0.56F, 1.00F};
    c[ImGuiCol_WindowBg] = ImVec4{0.11F, 0.11F, 0.12F, 1.00F};
    c[ImGuiCol_PopupBg] = ImVec4{0.14F, 0.14F, 0.16F, 0.98F};
    c[ImGuiCol_Border] = ImVec4{0.24F, 0.24F, 0.27F, 1.00F};
    c[ImGuiCol_FrameBg] = ImVec4{0.16F, 0.16F, 0.18F, 1.00F};
    c[ImGuiCol_Button] = ImVec4{0.20F, 0.20F, 0.23F, 1.00F};
    c[ImGuiCol_ButtonHovered] = ImVec4{0.26F, 0.27F, 0.32F, 1.00F};
    c[ImGuiCol_ButtonActive] = ImVec4{0.27F, 0.38F, 0.62F, 1.00F};
    c[ImGuiCol_Header] = ImVec4{0.28F, 0.46F, 0.85F, 0.45F};
    c[ImGuiCol_HeaderHovered] = ImVec4{1.00F, 1.00F, 1.00F, 0.06F};
    c[ImGuiCol_HeaderActive] = ImVec4{0.28F, 0.46F, 0.85F, 0.55F};
    c[ImGuiCol_TableHeaderBg] = ImVec4{0.16F, 0.16F, 0.18F, 1.00F};
    c[ImGuiCol_TableBorderStrong] = ImVec4{0.24F, 0.24F, 0.27F, 1.00F};
    c[ImGuiCol_TableBorderLight] = ImVec4{0.19F, 0.19F, 0.21F, 1.00F};
    c[ImGuiCol_TableRowBg] = ImVec4{0.11F, 0.11F, 0.12F, 1.00F};
    c[ImGuiCol_TableRowBgAlt] = ImVec4{0.13F, 0.13F, 0.145F, 1.00F};
    c[ImGuiCol_TreeLines] = ImVec4{0.36F, 0.36F, 0.40F, 1.00F};
    c[ImGuiCol_ScrollbarBg] = ImVec4{0.00F, 0.00F, 0.00F, 0.00F};
    c[ImGuiCol_Separator] = ImVec4{0.22F, 0.22F, 0.25F, 1.00F};
    style.FrameBorderSize = 0.0F;
    return Palette{
        .own_process_row = rgba(80, 120, 230, 52),
        .cpu_heat = rgba(230, 140, 20, 140),
        .kernel_thread_text = ImVec4{0.56F, 0.56F, 0.61F, 1.00F},
        .muted_text = ImVec4{0.66F, 0.66F, 0.71F, 1.00F},
        .accent_text = ImVec4{1.00F, 0.70F, 0.30F, 1.00F},
        .page_present = rgba(90, 150, 240, 255),
        .page_swapped = rgba(240, 160, 50, 255),
        .page_absent = rgba(50, 50, 56, 255),
    };
}

} // namespace

Palette apply_theme(Theme theme, float scale, float font_size)
{
    // Start from a fresh style each time so re-applying (theme switch) never compounds scaling.
    ImGuiStyle style;
    set_layout(style);
    const Palette palette = theme == Theme::dark ? set_dark_colors(style) : set_light_colors(style);
    style.ScaleAllSizes(scale);
    style.FontSizeBase = font_size;
    style.FontScaleDpi = scale;
    ImGui::GetStyle() = style;
    return palette;
}

} // namespace lxe::ui
