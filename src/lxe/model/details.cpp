#include "lxe/model/details.hpp"

#include <algorithm>
#include <numeric>
#include <utility>

namespace lxe::model {
namespace {

/// Up to this many pagemap entries are read per mapping (2 MiB of entries = 1 GiB of 4 KiB pages).
constexpr std::size_t max_page_entries = 262'144;
constexpr std::size_t sample_chunks = 64;
constexpr std::size_t max_page_buckets = 512;

} // namespace

std::vector<PageRun> plan_page_sample(std::uint64_t first_page, std::uint64_t pages, std::size_t max_entries, std::size_t chunks)
{
    std::vector<PageRun> runs;
    if (pages == 0 || max_entries == 0) {
        return runs;
    }
    if (pages <= max_entries) {
        runs.push_back(PageRun{.first_page = first_page, .count = static_cast<std::size_t>(pages)});
        return runs;
    }
    chunks = std::max<std::size_t>(chunks, 1);
    const std::size_t chunk_len = std::max<std::size_t>(max_entries / chunks, 1);
    const std::uint64_t spacing = pages / chunks;
    for (std::size_t i = 0; i < chunks; ++i) {
        const std::uint64_t offset = i * spacing;
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(chunk_len, pages - offset));
        if (count > 0) {
            runs.push_back(PageRun{.first_page = first_page + offset, .count = count});
        }
    }
    return runs;
}

PageSummary summarize_pages(std::span<const std::uint64_t> entries, std::uint64_t page_bytes, std::uint64_t pages, std::size_t max_buckets)
{
    PageSummary summary{.page_bytes = page_bytes, .pages = pages, .examined = entries.size()};
    const std::size_t bucket_count = std::min(entries.size(), std::max<std::size_t>(max_buckets, 1));
    summary.buckets.resize(bucket_count);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const proc::PageFlags flags = proc::decode_pagemap(entries[i]);
        PageBucket& bucket = summary.buckets[i * bucket_count / entries.size()];
        ++bucket.pages;
        if (flags.present) {
            ++summary.present;
            ++bucket.present;
        }
        else if (flags.swapped) {
            ++summary.swapped;
            ++bucket.swapped;
        }
        if (flags.exclusive) {
            ++summary.exclusive;
        }
        if (flags.file_or_shared && (flags.present || flags.swapped)) {
            ++summary.file_or_shared;
        }
        if (flags.soft_dirty) {
            ++summary.soft_dirty;
        }
    }
    return summary;
}

std::uint64_t ProcessDetails::mapped_bytes() const noexcept
{
    return std::accumulate(maps.begin(), maps.end(), std::uint64_t{0}, [](std::uint64_t sum, const MappingInfo& info) {
        return sum + info.mapping.size();
    });
}

const MappingInfo* ProcessDetails::selected() const noexcept
{
    if (!selected_mapping) {
        return nullptr;
    }
    const auto found = std::ranges::find(maps, *selected_mapping, [](const MappingInfo& info) { return info.mapping.start; });
    return found == maps.end() ? nullptr : &*found;
}

std::expected<std::vector<MappingInfo>, std::error_code> load_mappings(const proc::ProcFs& fs, ProcessId pid)
{
    std::vector<MappingInfo> result;
    if (auto smaps = fs.read_smaps(pid)) {
        result.reserve(smaps->size());
        for (auto& entry : *smaps) {
            result.push_back(MappingInfo{.mapping = std::move(entry.mapping), .stats = std::move(entry.stats)});
        }
        return result;
    }
    else if (smaps.error().category() != proc::parse_error_category() && smaps.error() != std::errc::no_such_file_or_directory) {
        return std::unexpected{smaps.error()}; // can't read (e.g. permission): maps would fail the same way
    }
    // No smaps (kernel without CONFIG_PROC_PAGE_MONITOR) or a layout this parser doesn't
    // understand: fall back to the plain mapping list.
    auto maps = fs.read_maps(pid);
    if (!maps) {
        return std::unexpected{maps.error()};
    }
    result.reserve(maps->size());
    for (auto& mapping : *maps) {
        result.push_back(MappingInfo{.mapping = std::move(mapping), .stats = std::nullopt});
    }
    return result;
}

ProcessDetails load_details(const proc::ProcFs& fs, const DetailsRequest& request, std::uint64_t page_bytes)
{
    ProcessDetails details{.pid = request.pid, .selected_mapping = request.mapping};
    if (!request.pid) {
        return details;
    }
    if (auto maps = load_mappings(fs, *request.pid)) {
        details.maps = std::move(*maps);
    }
    else {
        details.error = maps.error();
        return details;
    }
    const MappingInfo* selected = details.selected();
    if (selected == nullptr || page_bytes == 0) {
        return details;
    }
    const std::uint64_t pages = selected->mapping.size() / page_bytes;
    std::vector<std::uint64_t> entries;
    for (const PageRun& run : plan_page_sample(selected->mapping.start / page_bytes, pages, max_page_entries, sample_chunks)) {
        auto part = fs.read_pagemap(*request.pid, run.first_page, run.count);
        if (!part) {
            details.pages_error = part.error();
            return details;
        }
        entries.insert(entries.end(), part->begin(), part->end());
    }
    details.pages = summarize_pages(entries, page_bytes, pages, max_page_buckets);
    return details;
}

} // namespace lxe::model
