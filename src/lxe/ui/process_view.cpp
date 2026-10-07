#include "lxe/ui/process_view.hpp"

#include "lxe/model/format.hpp"

#include <imgui.h>

#include <system_error>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lxe::ui {
namespace {

// Font Awesome 4 code points, used only when the icon font is loaded.
constexpr std::string_view icon_pause = "";
constexpr std::string_view icon_play = "";
constexpr std::string_view icon_refresh = "";
constexpr std::string_view icon_expand = "";
constexpr std::string_view icon_collapse = "";

enum class Column : unsigned char
{
    process,
    pid,
    user,
    cpu,
    memory,
    threads,
    state,
    command_line,
    count,
};

[[nodiscard]] constexpr int index(Column column) noexcept
{
    return std::to_underlying(column);
}

void text(std::string_view s)
{
    ImGui::TextUnformatted(s.data(), std::to_address(s.end()));
}

void text_right_aligned(std::string_view s)
{
    const float width = ImGui::CalcTextSize(s.data(), std::to_address(s.end())).x;
    const float available = ImGui::GetContentRegionAvail().x;
    if (width < available) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available - width);
    }
    text(s);
}

[[nodiscard]] std::string button_label(
    const ViewConfig& config,
    std::string_view icon,
    std::string_view caption,
    std::string_view id
)
{
    // "###id" keeps the widget id stable while the visible caption changes (Pause <-> Resume).
    return config.icons ? std::format("{}  {}###{}", icon, caption, id) : std::format("{}###{}", caption, id);
}

void item_tooltip(std::string_view s)
{
    if (ImGui::BeginItemTooltip()) {
        text(s);
        ImGui::EndTooltip();
    }
}

void handle_shortcuts(ViewState& state, FrameRequests& requests)
{
    if (ImGui::GetIO().WantTextInput) {
        return;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        state.paused = !state.paused;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        requests.refresh_now = true;
    }
    if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_M)) {
        state.show_details = !state.show_details;
    }
}

void draw_toolbar(ViewState& state, FrameRequests& requests, const ViewConfig& config)
{
    const auto pause_label = state.paused ? button_label(config, icon_play, "Resume", "pause")
                                          : button_label(config, icon_pause, "Pause", "pause");
    if (ImGui::Button(pause_label.c_str())) {
        state.paused = !state.paused;
    }
    item_tooltip("Pause or resume live updates (Space)");

    ImGui::SameLine();
    if (ImGui::Button(button_label(config, icon_refresh, "Refresh", "refresh").c_str())) {
        requests.refresh_now = true;
    }
    item_tooltip("Refresh now (F5)");

    ImGui::SameLine();
    if (ImGui::Button(button_label(config, icon_expand, "Expand all", "expand").c_str())) {
        state.expand = ExpandRequest::expand_all;
    }
    ImGui::SameLine();
    if (ImGui::Button(button_label(config, icon_collapse, "Collapse all", "collapse").c_str())) {
        state.expand = ExpandRequest::collapse_all;
    }

    ImGui::SameLine(0.0F, ImGui::GetFontSize() * 2.0F);
    ImGui::Checkbox("Memory maps", &state.show_details);
    item_tooltip("Show the memory mappings of the selected process (Ctrl+M)");
}

void setup_columns()
{
    const float em = ImGui::GetFontSize();
    constexpr ImGuiTableColumnFlags fixed = ImGuiTableColumnFlags_WidthFixed;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Process", fixed | ImGuiTableColumnFlags_NoHide, em * 15.0F);
    ImGui::TableSetupColumn("PID", fixed, em * 3.6F);
    ImGui::TableSetupColumn("User", fixed, em * 6.0F);
    ImGui::TableSetupColumn("CPU %", fixed, em * 3.6F);
    ImGui::TableSetupColumn("Memory", fixed, em * 5.2F);
    ImGui::TableSetupColumn("Threads", fixed, em * 3.6F);
    ImGui::TableSetupColumn("State", fixed, em * 5.0F);
    ImGui::TableSetupColumn("Command Line", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
}

/// The CPU cell gets a background as strong as the load, like a heat map.
void cpu_cell_background(std::optional<double> percent, const Palette& palette)
{
    if (!percent || *percent <= 0.0) {
        return;
    }
    // Full colour at 50% of the machine already: on a many-core box single busy processes stay small.
    constexpr double full_at = 50.0;
    ImVec4 color = ImGui::ColorConvertU32ToFloat4(palette.cpu_heat);
    color.w *= static_cast<float>(std::clamp(*percent / full_at, 0.08, 1.0));
    ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, ImGui::ColorConvertFloat4ToU32(color));
}

void row_tooltip(const model::ProcessEntry& process)
{
    if (!ImGui::BeginItemTooltip()) {
        return;
    }
    text(std::format("{}  (PID {})", process.name, process.pid));
    if (!process.command_line.empty()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 40.0F);
        text(process.command_line);
        ImGui::PopTextWrapPos();
    }
    ImGui::EndTooltip();
}

/// Draws one table row; returns true when its tree node is open and has pushed a tree level.
[[nodiscard]] bool draw_row(
    const model::ProcessEntry& process,
    bool has_children,
    ViewState& state,
    const ViewConfig& config
)
{
    ImGui::TableNextRow();
    if (process.uid == config.current_user && !process.kernel_thread) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, config.palette.own_process_row);
    }
    if (process.kernel_thread) {
        ImGui::PushStyleColor(ImGuiCol_Text, config.palette.kernel_thread_text);
    }

    ImGui::TableSetColumnIndex(index(Column::process));
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAllColumns | ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_DrawLinesToNodes;
    if (!has_children) {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }
    else if (!process.kernel_thread) {
        flags |= ImGuiTreeNodeFlags_DefaultOpen; // kthreadd's hundreds of workers start collapsed
    }
    if (state.selected == process.pid) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (has_children && state.expand != ExpandRequest::none) {
        ImGui::SetNextItemOpen(state.expand == ExpandRequest::expand_all);
    }
    // The pid is the node's ImGui id: stable across frames while rows move around.
    const auto node_id = std::bit_cast<const void*>(static_cast<std::uintptr_t>(std::to_underlying(process.pid)));
    const bool open = ImGui::TreeNodeEx(node_id, flags, "%s", process.name.c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        state.selected = process.pid;
    }
    row_tooltip(process);

    ImGui::TableSetColumnIndex(index(Column::pid));
    text_right_aligned(std::format("{}", process.pid));
    ImGui::TableSetColumnIndex(index(Column::user));
    text(process.user);
    ImGui::TableSetColumnIndex(index(Column::cpu));
    cpu_cell_background(process.cpu_percent, config.palette);
    text_right_aligned(model::format_cpu(process.cpu_percent));
    ImGui::TableSetColumnIndex(index(Column::memory));
    text_right_aligned(process.memory_bytes > 0 ? model::format_bytes(process.memory_bytes) : std::string{});
    ImGui::TableSetColumnIndex(index(Column::threads));
    text_right_aligned(std::format("{}", process.threads));
    ImGui::TableSetColumnIndex(index(Column::state));
    text(model::state_name(process.state));
    ImGui::TableSetColumnIndex(index(Column::command_line));
    text(process.command_line);

    if (process.kernel_thread) {
        ImGui::PopStyleColor();
    }
    return open && has_children;
}

/// Deepest tree nesting drawn as nested nodes. ImGui tracks tree depth in a 32-bit mask, so
/// nesting beyond 31 levels is undefined behaviour inside it; and no screen is that wide anyway.
/// Descendants below the cap are drawn as leaves at the cap's depth, right after their parent.
constexpr std::size_t max_nesting = 24;

/// Depth-first over the tree without recursion: process chains can be arbitrarily deep.
void draw_rows(const model::Model& model, ViewState& state, const ViewConfig& config)
{
    struct Level
    {
        std::span<const std::size_t> nodes;
        std::size_t next = 0;
        bool pushed = false; ///< A tree node was pushed for this level and must be popped.
    };
    // Roots are drawn one tree level down, with the indent cancelled out. ImGui 1.92.9 computes
    // `1 << (depth - 1)` for a clipped node at depth 0 when tree lines are on, which is undefined
    // behaviour; at depth 1 the expression is valid and the dummy level stores no line data.
    ImGui::TreePush("roots");
    ImGui::Unindent();
    std::vector<Level> stack{Level{.nodes = model.tree.roots()}};
    std::size_t nesting = 0;
    while (!stack.empty()) {
        Level& level = stack.back();
        if (level.next == level.nodes.size()) {
            if (level.pushed) {
                ImGui::TreePop();
                --nesting;
            }
            stack.pop_back();
            continue;
        }
        const std::size_t node = level.nodes[level.next++];
        const auto children = model.tree.children(node);
        if (children.empty()) {
            std::ignore = draw_row(model.processes.at(node), false, state, config);
        }
        else if (nesting >= max_nesting) {
            std::ignore = draw_row(model.processes.at(node), false, state, config);
            stack.push_back(Level{.nodes = children, .next = 0, .pushed = false});
        }
        else if (draw_row(model.processes.at(node), true, state, config)) {
            stack.push_back(Level{.nodes = children, .next = 0, .pushed = true});
            ++nesting;
        }
    }
    ImGui::Indent();
    ImGui::TreePop();
}

void draw_table(const model::Model& model, ViewState& state, const ViewConfig& config, float height)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                      ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("processes", index(Column::count), flags, ImVec2{0.0F, height})) {
        return;
    }
    setup_columns();
    draw_rows(model, state, config);
    ImGui::EndTable();
}

/// A horizontal drag handle between the two panes; adjusts `height` of the lower pane.
void draw_splitter(float& height, float min_height, float max_height)
{
    const float thickness = ImGui::GetStyle().ItemSpacing.y + 4.0F;
    ImGui::InvisibleButton("splitter", ImVec2{std::max(ImGui::GetContentRegionAvail().x, 1.0F), thickness});
    const bool active = ImGui::IsItemActive();
    if (active) {
        height -= ImGui::GetIO().MouseDelta.y;
    }
    if (active || ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    const ImVec2 top_left = ImGui::GetItemRectMin();
    const ImVec2 bottom_right = ImGui::GetItemRectMax();
    const float y = (top_left.y + bottom_right.y) * 0.5F;
    const ImU32 color = ImGui::GetColorU32(active || ImGui::IsItemHovered() ? ImGuiCol_SeparatorHovered : ImGuiCol_Separator);
    ImGui::GetWindowDrawList()->AddLine(ImVec2{top_left.x, y}, ImVec2{bottom_right.x, y}, color, 1.0F);
    height = std::clamp(height, min_height, std::max(min_height, max_height));
}

/// A readable reason for a failed details read.
[[nodiscard]] std::string describe(const std::error_code& error)
{
    if (error == std::errc::permission_denied) {
        return "permission denied (another user's process; run as root to see it)";
    }
    if (error == std::errc::no_such_file_or_directory) {
        return "the process has exited";
    }
    return error.message();
}

enum class MapsColumn : unsigned char
{
    address,
    end,
    size,
    permissions,
    offset,
    device,
    inode,
    path,
    count,
};

void mono_text(std::string_view s, const ViewConfig& config)
{
    if (config.mono_font != nullptr) {
        ImGui::PushFont(config.mono_font, 0.0F);
    }
    text(s);
    if (config.mono_font != nullptr) {
        ImGui::PopFont();
    }
}

void draw_maps_table(const model::ProcessDetails& details, const ViewConfig& config, float height)
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                      ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                      ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("maps", std::to_underlying(MapsColumn::count), flags, ImVec2{0.0F, height})) {
        return;
    }
    const float em = ImGui::GetFontSize();
    constexpr ImGuiTableColumnFlags fixed = ImGuiTableColumnFlags_WidthFixed;
    constexpr ImGuiTableColumnFlags hidden = fixed | ImGuiTableColumnFlags_DefaultHide;
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Address", fixed | ImGuiTableColumnFlags_NoHide, em * 10.5F);
    ImGui::TableSetupColumn("End", hidden, em * 10.5F);
    ImGui::TableSetupColumn("Size", fixed, em * 5.5F);
    ImGui::TableSetupColumn("Perms", fixed, em * 3.2F);
    ImGui::TableSetupColumn("Offset", fixed, em * 5.5F);
    ImGui::TableSetupColumn("Device", hidden, em * 4.0F);
    ImGui::TableSetupColumn("Inode", hidden, em * 6.0F);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    // Only visible rows are submitted: a browser process has tens of thousands of mappings.
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(details.maps.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const proc::MemoryMapping& mapping = details.maps.at(static_cast<std::size_t>(i));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(std::to_underlying(MapsColumn::address));
            mono_text(std::format("{:016x}", mapping.start), config);
            ImGui::TableSetColumnIndex(std::to_underlying(MapsColumn::end));
            mono_text(std::format("{:016x}", mapping.end), config);
            ImGui::TableSetColumnIndex(std::to_underlying(MapsColumn::size));
            text_right_aligned(model::format_bytes(mapping.size()));
            ImGui::TableSetColumnIndex(std::to_underlying(MapsColumn::permissions));
            mono_text(model::format_permissions(mapping), config);
            ImGui::TableSetColumnIndex(std::to_underlying(MapsColumn::offset));
            mono_text(std::format("{:08x}", mapping.offset), config);
            ImGui::TableSetColumnIndex(std::to_underlying(MapsColumn::device));
            mono_text(std::format("{:02x}:{:02x}", mapping.device_major, mapping.device_minor), config);
            ImGui::TableSetColumnIndex(std::to_underlying(MapsColumn::inode));
            text_right_aligned(mapping.inode == 0 ? std::string{} : std::format("{}", mapping.inode));
            ImGui::TableSetColumnIndex(std::to_underlying(MapsColumn::path));
            if (mapping.path.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, config.palette.kernel_thread_text);
                text("(anonymous)");
                ImGui::PopStyleColor();
            }
            else {
                text(mapping.path);
            }
        }
    }
    ImGui::EndTable();
}

void draw_details_content(const model::Model& model, const model::ProcessDetails& details, const ViewConfig& config)
{
    if (!details.pid) {
        ImGui::PushStyleColor(ImGuiCol_Text, config.palette.muted_text);
        text("Select a process to see its memory maps.");
        ImGui::PopStyleColor();
        return;
    }
    const auto process = std::ranges::find(model.processes, *details.pid, &model::ProcessEntry::pid);
    const std::string_view name = process != model.processes.end() ? std::string_view{process->name} : "?";
    if (details.error) {
        text(std::format("Memory maps of {} ({}): {}", name, *details.pid, describe(details.error)));
        return;
    }
    text(std::format(
        "Memory maps of {} ({}): {} mappings, {} mapped",
        name,
        *details.pid,
        details.maps.size(),
        model::format_bytes(details.mapped_bytes())
    ));
    draw_maps_table(details, config, 0.0F); // fills the rest of the pane
}

/// Lower pane: the memory mappings of the selected process. Occupies `height` whatever it
/// shows, so the status bar below it stays put.
void draw_details_pane(
    const model::Model& model,
    const model::ProcessDetails& details,
    const ViewConfig& config,
    float height
)
{
    if (ImGui::BeginChild("details", ImVec2{0.0F, height}, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar)) {
        draw_details_content(model, details, config);
    }
    ImGui::EndChild();
}

void draw_status_bar(const model::SystemSummary& summary, const ViewState& state, const ViewConfig& config)
{
    constexpr double hundred = 100.0;
    const double memory_percent =
        summary.memory_total_bytes == 0
            ? 0.0
            : hundred * static_cast<double>(summary.memory_used_bytes) / static_cast<double>(summary.memory_total_bytes);
    const std::string cpu = summary.cpu_percent ? std::format("{:.1f}%", *summary.cpu_percent) : std::string{"-"};

    ImGui::PushStyleColor(ImGuiCol_Text, config.palette.muted_text);
    text(std::format(
        "CPU Usage: {}     Memory: {:.1f}% ({} of {})     Processes: {}     Threads: {}",
        cpu,
        memory_percent,
        model::format_bytes(summary.memory_used_bytes),
        model::format_bytes(summary.memory_total_bytes),
        summary.processes,
        summary.threads
    ));
    ImGui::PopStyleColor();
    if (state.paused) {
        ImGui::SameLine(0.0F, ImGui::GetFontSize() * 2.0F);
        ImGui::PushStyleColor(ImGuiCol_Text, config.palette.accent_text);
        text("Paused");
        ImGui::PopStyleColor();
    }
}

} // namespace

FrameRequests draw_main_window(
    const model::Model& model,
    const model::ProcessDetails& details,
    ViewState& state,
    const ViewConfig& config
)
{
    FrameRequests requests;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    constexpr ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("Linux Explorer", nullptr, window_flags)) {
        handle_shortcuts(state, requests);
        draw_toolbar(state, requests, config);
        const float line = ImGui::GetTextLineHeight();
        const float spacing = ImGui::GetStyle().ItemSpacing.y;
        const float status_bar_height = line + spacing;
        if (state.show_details) {
            // Both panes share the space above the status bar; the lower one keeps its height.
            const float available = ImGui::GetContentRegionAvail().y - status_bar_height;
            const float min_pane = line * 4.0F;
            if (state.details_height <= 0.0F) {
                state.details_height = available / 3.0F;
            }
            const float splitter = spacing + 4.0F;
            state.details_height = std::clamp(state.details_height, min_pane, std::max(min_pane, available - min_pane - splitter));
            draw_table(model, state, config, std::max(available - state.details_height - splitter - spacing, min_pane));
            draw_splitter(state.details_height, min_pane, available - min_pane - splitter);
            draw_details_pane(model, details, config, -status_bar_height);
        }
        else {
            draw_table(model, state, config, -status_bar_height);
        }
        state.expand = ExpandRequest::none;
        draw_status_bar(model.summary, state, config);
    }
    ImGui::End();
    return requests;
}

} // namespace lxe::ui
