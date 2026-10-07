// Drives the real view code headlessly (ImGui null backend): no window, no GPU, deterministic.
#include "lxe/ui/process_view.hpp"
#include "lxe/ui/theme.hpp"
#include "support/builders.hpp"
#include "support/headless_imgui.hpp"

#include <catch2/catch_test_macros.hpp>

#include <imgui_internal.h>

using lxe::ProcessId;
using lxe::UserId;
using lxe::test::entry;
using lxe::test::HeadlessImGui;
using lxe::test::make_model;
using lxe::ui::ExpandRequest;
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
    };
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
            const auto requests = imgui.frame([&] { return lxe::ui::draw_main_window(model, state, config); });
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
    const auto draw = [&] { return lxe::ui::draw_main_window(model, state, config); };
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
    const auto draw = [&] { return lxe::ui::draw_main_window(model, state, config); };
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
    std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, state, config); });
    CHECK(state.expand == ExpandRequest::none);
    state.expand = ExpandRequest::collapse_all;
    std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, state, config); });
    CHECK(state.expand == ExpandRequest::none);
    CHECK(state.selected == ProcessId{100});
}

TEST_CASE("main_window.handles_empty_and_very_deep_trees")
{
    HeadlessImGui imgui;
    const auto config = config_for(lxe::ui::Theme::dark);
    ViewState state{.paused = false, .selected = std::nullopt, .expand = ExpandRequest::expand_all};

    const auto empty = make_model({});
    std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(empty, state, config); });

    std::vector<lxe::model::ProcessEntry> chain;
    for (int pid = 1; pid <= 2000; ++pid) {
        chain.push_back(entry(pid, pid - 1));
    }
    const auto deep = make_model(std::move(chain));
    state.expand = ExpandRequest::expand_all;
    for (int i = 0; i < 2; ++i) {
        std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(deep, state, config); });
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
        std::ignore = imgui.frame([&] { return lxe::ui::draw_main_window(model, state, config); });
    }
    CHECK(state.expand == ExpandRequest::none);
}
