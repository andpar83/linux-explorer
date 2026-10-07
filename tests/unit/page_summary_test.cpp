#include "lxe/model/details.hpp"
#include "support/builders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using lxe::model::PageRun;
using lxe::model::plan_page_sample;
using lxe::model::summarize_pages;
using lxe::test::pagemap_entry;

TEST_CASE("model.plan_page_sample.reads_small_mappings_whole")
{
    CHECK(plan_page_sample(100, 50, 1000, 8) == std::vector{PageRun{.first_page = 100, .count = 50}});
    CHECK(plan_page_sample(100, 1000, 1000, 8) == std::vector{PageRun{.first_page = 100, .count = 1000}});
    CHECK(plan_page_sample(100, 0, 1000, 8).empty());
    CHECK(plan_page_sample(100, 10, 0, 8).empty());
}

TEST_CASE("model.plan_page_sample.spreads_chunks_over_huge_mappings")
{
    const auto runs = plan_page_sample(1000, 100'000, 1000, 4);
    REQUIRE(runs.size() == 4);
    for (const auto& run : runs) {
        CHECK(run.count == 250);
    }
    CHECK(runs[0].first_page == 1000);
    CHECK(runs[1].first_page == 26'000);
    CHECK(runs[3].first_page == 76'000);
    std::uint64_t total = 0;
    for (const auto& run : runs) {
        total += run.count;
        CHECK(run.first_page + run.count <= 101'000);
    }
    CHECK(total == 1000);
}

TEST_CASE("model.summarize_pages.counts_flags_and_buckets")
{
    const std::vector<std::uint64_t> entries{
        pagemap_entry(true, false, true),               // present, exclusive
        pagemap_entry(true, false, false, true, true),  // present, shared, soft-dirty
        pagemap_entry(false, true, false, true),        // swapped, file/shared
        pagemap_entry(false),                           // absent
        pagemap_entry(false, false, false, true),       // absent with a stray bit 61: not counted as shared
        pagemap_entry(true, false, true),
    };
    const auto summary = summarize_pages(entries, 4096, 6, 3);
    CHECK(summary.page_bytes == 4096U);
    CHECK(summary.pages == 6U);
    CHECK(summary.examined == 6U);
    CHECK_FALSE(summary.sampled());
    CHECK(summary.present == 3U);
    CHECK(summary.swapped == 1U);
    CHECK(summary.absent() == 2U);
    CHECK(summary.exclusive == 2U);
    CHECK(summary.file_or_shared == 2U);
    CHECK(summary.soft_dirty == 1U);
    REQUIRE(summary.buckets.size() == 3);
    CHECK(summary.buckets[0] == lxe::model::PageBucket{.pages = 2, .present = 2, .swapped = 0});
    CHECK(summary.buckets[1] == lxe::model::PageBucket{.pages = 2, .present = 0, .swapped = 1});
    CHECK(summary.buckets[2] == lxe::model::PageBucket{.pages = 2, .present = 1, .swapped = 0});
}

TEST_CASE("model.summarize_pages.sampled_mappings_and_empty_input")
{
    const std::vector<std::uint64_t> entries(10, pagemap_entry(true));
    const auto sampled = summarize_pages(entries, 4096, 1'000'000, 512);
    CHECK(sampled.sampled());
    CHECK(sampled.examined == 10U);
    CHECK(sampled.buckets.size() == 10); // never more buckets than entries

    const auto empty = summarize_pages({}, 4096, 0, 512);
    CHECK(empty.buckets.empty());
    CHECK(empty.absent() == 0U);
}
