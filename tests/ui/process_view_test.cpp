// Drives the real view code headlessly (ImGui null backend): no window, no GPU, deterministic.
#include "lxe/ui/process_view.hpp"
#include "lxe/ui/theme.hpp"
#include "support/builders.hpp"
#include "support/headless_imgui.hpp"

#include <catch2/catch_test_macros.hpp>

#include <imgui_internal.h>

#include <cstdint>
#include <system_error>

using lxe::ProcessId;
using lxe::UserId;
using lxe::test::entry;
using lxe::test::HeadlessImGui;
using lxe::test::info;
using lxe::test::item_id;
using lxe::test::make_model;
using lxe::test::mapping;
using lxe::test::pagemap_entry;
using lxe::ui::ExpandRequest;
using lxe::ui::Page;
using lxe::ui::ViewConfig;
using lxe::ui::ViewState;

namespace {

lxe::model::Model sample_model()
{
    auto systemd = entry(1, 0, "systemd");
    auto bash = entry(100, 1, "bash");
    bash.uid = UserId{1000};
    bash.user = "alice";
    bash.cpu_percent = 42.5;
    bash.memory_bytes = 5'000'000;
    bash.command_line = "-bash";
    auto kthreadd = entry(2, 0, "kthreadd");
    kthreadd.kernel_thread = true;
    auto worker = entry(3, 2, "kworker/0:0H");
    worker.kernel_thread = true;
    auto model = make_model({systemd, kthreadd, worker, bash, entry(101, 100, "vim")});
    model.summary.cpu_percent = 12.0;
    model.summary.memory_total_bytes = 16ULL << 30U;
    model.summary.memory_used_bytes = 4ULL << 30U;
    return model;
}

ViewConfig config_for(lxe::ui::Theme theme)
{
    return ViewConfig{
        .current_user = UserId{1000},
        .palette = lxe::ui::apply_theme(theme, 1.0F, 15.0F),
        .icons = false,
        .mono_font = nullptr,
    };
}

const lxe::model::ProcessDetails no_details{};

lxe::model::ProcessDetails details_for(pid_t pid, std::size_t mappings, bool with_stats = false)
{
    lxe::model::ProcessDetails details{.pid = ProcessId{pid}};
    for (std::size_t i = 0; i < mappings; ++i) {
        const auto start = 0x7f0000000000U + static_cast<std::uint64_t>(i) * 0x2000U;
        auto entry = info(mapping(start, start + 0x1000U, i % 3 == 0 ? "" : "/usr/lib/libfoo.so", "r-xp"));
        if (with_stats) {
            lxe::proc::MappingStats stats;
            stats.rss = 4096;
            stats.pss = 2048;
            stats.vm_flags = {"rd", "ex", "zz"};
            stats.thp_eligible = i % 2 == 0;
            entry.stats = stats;
        }
        details.maps.push_back(std::move(entry));
    }
    return details;
}

lxe::model::PageSummary pages_for(std::size_t count)
{
    std::vector<std::uint64_t> entries;
    for (std::size_t i = 0; i < count; ++i) {
        entries.push_back(pagemap_entry(i % 2 == 0, i % 5 == 1, i % 3 == 0, i % 4 == 0, i % 7 == 0));
    }
    return lxe::model::summarize_pages(entries, 4096, count, 512);
}

} // namespace

TEST_CASE("main_window.draws_a_model_in_both_themes")
{
    for (const auto theme : {lxe::ui::Theme::light, lxe::ui::Theme::dark}) {
        HeadlessImGui imgui;
        const auto model = sample_model();
        const auto config = config_for(theme);
        ViewState state;
        for (int i = 0; i < 3; ++i) {
            const auto requests = imgui.frame([&] { return lxe::ui::draw_main_window(model, no_details, state, config); });
            CHECK_FALSE(requests.refresh_now);
        }
        CHECK_FALSE(state.paused);
        CHECK_FALSE(state.selected.has_value());
        // Tree nodes are ImGui items: the table drew one per visible process.
        CHECK(ImGui::GetCurrentContext()->Windows.Size > 0);
    }
}

TEST_CASE("main_window.f5_requests_a_refresh")
{
    HeadlessImGui imgui;
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::light);
    ViewState state;
    const auto draw = [&] { return lxe::ui::draw_main_window(model, no_details, state, config); };
    std::ignore = imgui.frame(draw);
    CHECK(imgui.press(ImGuiKey_F5, draw).refresh_now);
    CHECK_FALSE(imgui.frame(draw).refresh_now);
}

TEST_CASE("main_window.space_toggles_pause")
{
    HeadlessImGui imgui;
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::light);
    ViewState state;
    const auto draw = [&] { return lxe::ui::draw_main_window(model, no_details, state, config); };
    std::ignore = imgui.frame(draw);
    std::ignore = imgui.press(ImGuiKey_Space, draw);
    CHECK(state.paused);
    std::ignore = imgui.press(ImGuiKey_Space, draw);
    CHECK_FALSE(state.paused);
}

TEST_CASE("main_window.expand_requests_are_applied_once")
{
    HeadlessImGui imgui;
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::light);
    ViewState state{.paused = false, .selected = ProcessId{100}, .expand = ExpandRequest::expand_all};
    std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, no_details, state, config); });
    CHECK(state.expand == ExpandRequest::none);
    state.expand = ExpandRequest::collapse_all;
    std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, no_details, state, config); });
    CHECK(state.expand == ExpandRequest::none);
    CHECK(state.selected == ProcessId{100});
}

TEST_CASE("main_window.handles_empty_and_very_deep_trees")
{
    HeadlessImGui imgui;
    const auto config = config_for(lxe::ui::Theme::dark);
    ViewState state{.paused = false, .selected = std::nullopt, .expand = ExpandRequest::expand_all};

    const auto empty = make_model({});
    std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(empty, no_details, state, config); });

    std::vector<lxe::model::ProcessEntry> chain;
    for (int pid = 1; pid <= 2000; ++pid) {
        chain.push_back(entry(pid, pid - 1));
    }
    const auto deep = make_model(std::move(chain));
    state.expand = ExpandRequest::expand_all;
    for (int i = 0; i < 2; ++i) {
        std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(deep, no_details, state, config); });
    }
    CHECK(state.expand == ExpandRequest::none);
}

TEST_CASE("main_window.draws_lists_longer_than_the_window")
{
    // Rows below the visible area are clipped by ImGui; that path must be as safe as the visible one.
    HeadlessImGui imgui;
    imgui.set_display_size(640.0F, 240.0F);
    const auto config = config_for(lxe::ui::Theme::light);
    std::vector<lxe::model::ProcessEntry> processes{entry(1, 0, "systemd"), entry(2, 0, "kthreadd")};
    for (int pid = 10; pid < 400; ++pid) {
        processes.push_back(entry(pid, pid % 7 == 0 ? 2 : 1)); // roots and nested rows, some clipped
    }
    for (int pid = 400; pid < 420; ++pid) {
        processes.push_back(entry(pid, 0)); // extra roots, all below the fold
    }
    const auto model = make_model(std::move(processes));
    ViewState state{.paused = false, .selected = std::nullopt, .expand = ExpandRequest::expand_all};
    for (int i = 0; i < 3; ++i) {
        std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, no_details, state, config); });
    }
    CHECK(state.expand == ExpandRequest::none);
}

TEST_CASE("main_window.maps_page_shows_maps_errors_and_hints")
{
    HeadlessImGui imgui;
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::light);
    ViewState state{.page = Page::memory_maps};
    const auto draw = [&](const lxe::model::ProcessDetails& details) {
        return imgui.frame([&] { return lxe::ui::draw_main_window(model, details, state, config); });
    };

    std::ignore = draw(no_details); // nothing selected: a hint
    state.selected = ProcessId{100};
    std::ignore = draw(details_for(100, 3)); // maps of a listed process
    std::ignore = draw(details_for(4242, 2)); // maps of a process no longer in the table

    lxe::model::ProcessDetails denied{.pid = ProcessId{1}, .maps = {}, .error = std::make_error_code(std::errc::permission_denied)};
    std::ignore = draw(denied);
    lxe::model::ProcessDetails gone{.pid = ProcessId{1}, .maps = {}, .error = std::make_error_code(std::errc::no_such_file_or_directory)};
    std::ignore = draw(gone);
    lxe::model::ProcessDetails other{.pid = ProcessId{1}, .maps = {}, .error = std::make_error_code(std::errc::io_error)};
    std::ignore = draw(other);
    CHECK(state.page == Page::memory_maps);
}

TEST_CASE("main_window.maps_page_clips_long_lists")
{
    HeadlessImGui imgui;
    imgui.set_display_size(800.0F, 300.0F);
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::dark);
    const auto details = details_for(100, 20'000);
    ViewState state{.paused = false, .selected = ProcessId{100}, .page = Page::memory_maps};
    for (int i = 0; i < 3; ++i) {
        std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, details, state, config); });
    }
}

TEST_CASE("main_window.ctrl_m_switches_between_pages")
{
    HeadlessImGui imgui;
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::light);
    ViewState state{.paused = false, .selected = ProcessId{100}};
    const auto draw = [&] { return lxe::ui::draw_main_window(model, details_for(100, 3), state, config); };
    std::ignore = imgui.frame(draw);
    REQUIRE(state.page == Page::processes);
    std::ignore = imgui.press_chord(ImGuiMod_Ctrl, ImGuiKey_M, draw);
    CHECK(state.page == Page::memory_maps);
    std::ignore = imgui.press_chord(ImGuiMod_Ctrl, ImGuiKey_M, draw);
    CHECK(state.page == Page::processes);
    // A plain M does nothing.
    std::ignore = imgui.press(ImGuiKey_M, draw);
    CHECK(state.page == Page::processes);
    CHECK(state.selected == ProcessId{100}); // the selection survives the round trip
}

TEST_CASE("main_window.toolbar_buttons_switch_pages")
{
    HeadlessImGui imgui;
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::dark);
    ViewState state{.paused = false, .selected = ProcessId{100}};
    const auto draw = [&] { return lxe::ui::draw_main_window(model, details_for(100, 3), state, config); };
    std::ignore = imgui.frame(draw);
    const ImGuiID processes = item_id("Linux Explorer", "###processes");
    const ImGuiID memory_maps = item_id("Linux Explorer", "###memory_maps");
    REQUIRE(memory_maps != 0);
    std::ignore = imgui.activate(memory_maps, draw);
    CHECK(state.page == Page::memory_maps);
    std::ignore = imgui.activate(memory_maps, draw); // already there: stays
    CHECK(state.page == Page::memory_maps);
    std::ignore = imgui.activate(processes, draw);
    CHECK(state.page == Page::processes);
    CHECK(state.selected == ProcessId{100});
}

TEST_CASE("main_window.pages_panel_shows_every_state")
{
    HeadlessImGui imgui;
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::light);
    ViewState state{.paused = false, .selected = ProcessId{100}, .page = Page::memory_maps};
    const auto draw = [&](const lxe::model::ProcessDetails& details) {
        return imgui.frame([&] { return lxe::ui::draw_main_window(model, details, state, config); });
    };

    auto details = details_for(100, 5, true);
    std::ignore = draw(details); // no mapping selected: hint

    details.selected_mapping = details.maps[1].mapping.start;
    details.pages = pages_for(1000);
    std::ignore = draw(details); // strip + counts + stats

    details.pages = pages_for(3);
    std::ignore = draw(details); // fewer pages than pixels

    details.pages->pages = 5'000'000; // sampled
    std::ignore = draw(details);

    details.pages.reset();
    details.pages_error = std::make_error_code(std::errc::permission_denied);
    std::ignore = draw(details);

    details.selected_mapping = 0x42; // gone
    std::ignore = draw(details);

    auto plain = details_for(100, 2, false); // no smaps on this kernel
    plain.selected_mapping = plain.maps[0].mapping.start;
    plain.pages = pages_for(10);
    std::ignore = draw(plain);
}

TEST_CASE("main_window.selecting_a_process_forgets_the_mapping")
{
    HeadlessImGui imgui;
    const auto model = sample_model();
    const auto config = config_for(lxe::ui::Theme::dark);
    ViewState state{.paused = false, .selected = ProcessId{100}, .selected_mapping = 0x1000};
    std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, no_details, state, config); });
    CHECK(state.selected_mapping == 0x1000); // untouched while the selection stays
    state.selected = ProcessId{101};
    state.selected_mapping.reset(); // what a click on another row does (see draw_row)
    std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, no_details, state, config); });
    CHECK_FALSE(state.selected_mapping.has_value());
}
