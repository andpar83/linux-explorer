#pragma once

#include "lxe/ids.hpp"
#include "lxe/proc/parse.hpp"
#include "lxe/proc/proc_fs.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <system_error>
#include <vector>

namespace lxe::model {

/// A mapping with its page accounting when the kernel provided it (smaps).
struct MappingInfo
{
    proc::MemoryMapping mapping;
    std::optional<proc::MappingStats> stats;

    friend bool operator==(const MappingInfo&, const MappingInfo&) = default;
};

/// Pages of one slice of a mapping's address range, for drawing a presence strip.
struct PageBucket
{
    std::uint32_t pages = 0;
    std::uint32_t present = 0;
    std::uint32_t swapped = 0;

    friend bool operator==(const PageBucket&, const PageBucket&) = default;
};

/// Per-page classification of one mapping, from /proc/<pid>/pagemap.
struct PageSummary
{
    std::uint64_t page_bytes = 0;
    std::uint64_t pages = 0;    ///< Pages in the mapping.
    std::uint64_t examined = 0; ///< pagemap entries read; fewer than `pages` when the mapping was sampled.
    std::uint64_t present = 0;
    std::uint64_t swapped = 0;
    std::uint64_t exclusive = 0;
    std::uint64_t file_or_shared = 0;
    std::uint64_t soft_dirty = 0;
    std::vector<PageBucket> buckets; ///< `examined` pages in order, split into equal buckets.

    [[nodiscard]] std::uint64_t absent() const noexcept { return examined - present - swapped; }

    [[nodiscard]] bool sampled() const noexcept { return examined < pages; }
};

/// A run of consecutive pages to read from pagemap.
struct PageRun
{
    std::uint64_t first_page = 0;
    std::size_t count = 0;

    friend bool operator==(const PageRun&, const PageRun&) = default;
};

/// Which pagemap entries to read for `pages` pages starting at `first_page`: all of them when
/// there are at most `max_entries`, otherwise `chunks` evenly spread runs of about
/// `max_entries` entries in total, so huge mappings cost a bounded amount of reading.
[[nodiscard]] std::vector<PageRun> plan_page_sample(
    std::uint64_t first_page,
    std::uint64_t pages,
    std::size_t max_entries,
    std::size_t chunks
);

/// Classifies pagemap `entries` (in address order) of a mapping of `pages` pages.
[[nodiscard]] PageSummary summarize_pages(
    std::span<const std::uint64_t> entries,
    std::uint64_t page_bytes,
    std::uint64_t pages,
    std::size_t max_buckets
);

/// What the lower pane shows about the selected process.
struct ProcessDetails
{
    std::optional<ProcessId> pid;                 ///< nullopt: nothing selected.
    std::vector<MappingInfo> maps;                ///< Memory mappings, valid when `error` is empty.
    std::error_code error;                        ///< Why `maps` could not be read (permission, exited).
    std::optional<std::uint64_t> selected_mapping; ///< Start address of the mapping whose pages are shown.
    std::optional<PageSummary> pages;             ///< Pages of the selected mapping, when readable.
    std::error_code pages_error;

    [[nodiscard]] std::uint64_t mapped_bytes() const noexcept;

    /// The selected mapping, if it is still listed.
    [[nodiscard]] const MappingInfo* selected() const noexcept;
};

/// Mappings of `pid` with page accounting; without it when this kernel's smaps can't be parsed.
[[nodiscard]] std::expected<std::vector<MappingInfo>, std::error_code> load_mappings(
    const proc::ProcFs& fs,
    ProcessId pid
);

struct DetailsRequest
{
    std::optional<ProcessId> pid;
    std::optional<std::uint64_t> mapping; ///< Start address of the mapping to classify pages of.
};

/// Reads the details for `request`; an empty result when nothing is selected.
[[nodiscard]] ProcessDetails load_details(const proc::ProcFs& fs, const DetailsRequest& request, std::uint64_t page_bytes);

} // namespace lxe::model
