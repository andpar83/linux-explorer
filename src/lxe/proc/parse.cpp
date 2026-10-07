#include "lxe/proc/parse.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <limits>
#include <memory>
#include <ranges>
#include <span>

namespace lxe::proc {
namespace {

class ParseErrorCategory final : public std::error_category
{
public:
    [[nodiscard]] const char* name() const noexcept override { return "lxe.proc.parse"; }

    [[nodiscard]] std::string message(int value) const override
    {
        switch (static_cast<ParseError>(value)) {
        case ParseError::malformed:
            return "malformed /proc content";
        case ParseError::truncated:
            return "truncated /proc content";
        case ParseError::bad_number:
            return "invalid number in /proc content";
        case ParseError::missing_field:
            return "required field missing from /proc content";
        }
        return "unknown /proc parse error";
    }
};

[[nodiscard]] std::unexpected<std::error_code> fail(ParseError error)
{
    return std::unexpected{make_error_code(error)};
}

constexpr std::string_view whitespace = " \t\n";

/// Splits on runs of spaces, tabs and newlines.
[[nodiscard]] std::vector<std::string_view> split_whitespace(std::string_view text)
{
    std::vector<std::string_view> fields;
    while (true) {
        const auto start = text.find_first_not_of(whitespace);
        if (start == std::string_view::npos) {
            break;
        }
        text.remove_prefix(start);
        const auto end = std::min(text.find_first_of(whitespace), text.size());
        fields.push_back(text.substr(0, end));
        text.remove_prefix(end);
    }
    return fields;
}

/// The whole of `text` as a number; nullopt on empty input, junk, sign mismatch or overflow.
template <std::integral T>
[[nodiscard]] std::optional<T> to_number(std::string_view text, int base = 10) noexcept
{
    T value{};
    const char* const last = std::to_address(text.end());
    const auto [end, error] = std::from_chars(text.data(), last, value, base);
    if (text.empty() || error != std::errc{} || end != last) {
        return std::nullopt;
    }
    return value;
}

template <std::integral T>
[[nodiscard]] bool assign_number(std::string_view text, T& out) noexcept
{
    const auto value = to_number<T>(text);
    if (value) {
        out = *value;
    }
    return value.has_value();
}

[[nodiscard]] std::optional<std::uint64_t> checked_add(std::uint64_t a, std::uint64_t b) noexcept
{
    if (b > std::numeric_limits<std::uint64_t>::max() - a) {
        return std::nullopt;
    }
    return a + b;
}

/// The next whitespace-delimited field of `rest`, consumed from it; nullopt when none is left.
[[nodiscard]] std::optional<std::string_view> take_field(std::string_view& rest) noexcept
{
    const auto start = rest.find_first_not_of(" \t");
    if (start == std::string_view::npos) {
        rest = {};
        return std::nullopt;
    }
    rest.remove_prefix(start);
    const auto end = std::min(rest.find_first_of(" \t"), rest.size());
    const auto field = rest.substr(0, end);
    rest.remove_prefix(end);
    return field;
}

/// true for `on`, false for '-', nullopt for anything else.
[[nodiscard]] std::optional<bool> permission_flag(char c, char on) noexcept
{
    if (c == on) {
        return true;
    }
    if (c == '-') {
        return false;
    }
    return std::nullopt;
}

/// Each line of `text`, without the newline.
[[nodiscard]] auto lines(std::string_view text)
{
    return text | std::views::split('\n') |
           std::views::transform([](auto line) { return std::string_view{line.begin(), line.end()}; });
}

/// Value of a "Key: <number> kB" line in /proc/meminfo, in bytes; nullopt when the key is absent.
[[nodiscard]] std::expected<std::optional<std::uint64_t>, std::error_code> meminfo_value(
    std::string_view text,
    std::string_view key
)
{
    for (const std::string_view line : lines(text)) {
        if (!line.starts_with(key)) {
            continue;
        }
        const auto fields = split_whitespace(line.substr(key.size()));
        if (fields.empty()) {
            return fail(ParseError::truncated);
        }
        const auto kib = to_number<std::uint64_t>(fields.front());
        constexpr std::uint64_t bytes_per_kib = 1024;
        if (!kib || *kib > std::numeric_limits<std::uint64_t>::max() / bytes_per_kib) {
            return fail(ParseError::bad_number);
        }
        return *kib * bytes_per_kib;
    }
    return std::nullopt;
}

} // namespace

const std::error_category& parse_error_category() noexcept
{
    static const ParseErrorCategory category;
    return category;
}

std::error_code make_error_code(ParseError error) noexcept
{
    return {static_cast<int>(error), parse_error_category()};
}

std::optional<ProcessId> parse_pid(std::string_view text) noexcept
{
    const auto value = to_number<pid_t>(text);
    if (!value || *value <= 0) {
        return std::nullopt;
    }
    return ProcessId{*value};
}

std::expected<StatInfo, std::error_code> parse_stat(std::string_view text)
{
    // "pid (comm) state ppid ...": comm may contain anything, so it ends at the LAST ')'.
    const auto open = text.find(" (");
    const auto close = text.rfind(')');
    if (open == std::string_view::npos || close == std::string_view::npos || close < open + 2) {
        return fail(ParseError::malformed);
    }

    StatInfo info;
    const auto pid = to_number<pid_t>(text.substr(0, open));
    if (!pid || *pid <= 0) {
        return fail(ParseError::bad_number);
    }
    info.pid = ProcessId{*pid};
    info.comm = std::string{text.substr(open + 2, close - open - 2)};

    // Fields after comm, numbered from 0 (= field 3, "state", in proc_pid_stat(5)).
    const auto fields = split_whitespace(text.substr(close + 1));
    constexpr std::size_t needed = 22;
    if (fields.size() < needed) {
        return fail(ParseError::truncated);
    }
    if (fields[0].size() != 1) {
        return fail(ParseError::malformed);
    }
    info.state = ProcessState{fields[0].front()};

    pid_t ppid = 0;
    const bool numbers_ok = assign_number(fields[1], ppid) && assign_number(fields[6], info.flags) &&
                            assign_number(fields[11], info.utime_ticks) &&
                            assign_number(fields[12], info.stime_ticks) && assign_number(fields[15], info.priority) &&
                            assign_number(fields[16], info.nice) && assign_number(fields[17], info.threads) &&
                            assign_number(fields[19], info.start_time_ticks) &&
                            assign_number(fields[20], info.virtual_bytes) &&
                            assign_number(fields[21], info.resident_pages);
    if (!numbers_ok) {
        return fail(ParseError::bad_number);
    }
    info.ppid = ProcessId{ppid};
    return info;
}

std::expected<StatusInfo, std::error_code> parse_status(std::string_view text)
{
    constexpr std::string_view key = "Uid:";
    for (const std::string_view line : lines(text)) {
        if (!line.starts_with(key)) {
            continue;
        }
        // "Uid:\treal\teffective\tsaved\tfilesystem"
        const auto fields = split_whitespace(line.substr(key.size()));
        if (fields.size() < 2) {
            return fail(ParseError::truncated);
        }
        const auto real = to_number<uid_t>(fields[0]);
        const auto effective = to_number<uid_t>(fields[1]);
        if (!real || !effective) {
            return fail(ParseError::bad_number);
        }
        return StatusInfo{.real_uid = UserId{*real}, .effective_uid = UserId{*effective}};
    }
    return fail(ParseError::missing_field);
}

std::vector<std::string> parse_cmdline(std::string_view text)
{
    std::vector<std::string> args;
    for (const auto arg : text | std::views::split('\0')) {
        args.emplace_back(arg.begin(), arg.end());
    }
    while (!args.empty() && args.back().empty()) {
        args.pop_back();
    }
    return args;
}

std::expected<CpuTimes, std::error_code> parse_cpu_times(std::string_view text)
{
    for (const std::string_view line : lines(text)) {
        const auto fields = split_whitespace(line);
        if (fields.empty() || fields.front() != "cpu") {
            continue;
        }
        // cpu user nice system idle iowait irq softirq steal guest guest_nice. Guest time is already
        // counted in user/nice, so it is left out of the total. Old kernels have only the first four.
        constexpr std::size_t minimum = 4;
        constexpr std::size_t counted = 8;
        const auto values = std::span{fields}.subspan(1);
        if (values.size() < minimum) {
            return fail(ParseError::truncated);
        }
        std::array<std::uint64_t, counted> ticks{};
        for (std::size_t i = 0; i < std::min(values.size(), counted); ++i) {
            if (!assign_number(values[i], ticks.at(i))) {
                return fail(ParseError::bad_number);
            }
        }
        std::optional<std::uint64_t> total = 0;
        for (const auto value : ticks) {
            total = total.and_then([value](std::uint64_t sum) { return checked_add(sum, value); });
        }
        constexpr std::size_t idle = 3;
        constexpr std::size_t iowait = 4;
        const auto idle_total = checked_add(ticks.at(idle), ticks.at(iowait));
        if (!total || !idle_total) {
            return fail(ParseError::bad_number);
        }
        return CpuTimes{.total_ticks = *total, .idle_ticks = *idle_total};
    }
    return fail(ParseError::missing_field);
}

std::expected<MemoryInfo, std::error_code> parse_meminfo(std::string_view text)
{
    const auto total = meminfo_value(text, "MemTotal:");
    auto available = meminfo_value(text, "MemAvailable:");
    if (available && !*available) {
        available = meminfo_value(text, "MemFree:");
    }
    if (!total) {
        return std::unexpected{total.error()};
    }
    if (!available) {
        return std::unexpected{available.error()};
    }
    if (!*total || !*available) {
        return fail(ParseError::missing_field);
    }
    return MemoryInfo{.total_bytes = **total, .available_bytes = **available};
}

std::expected<std::vector<MemoryMapping>, std::error_code> parse_maps(std::string_view text)
{
    constexpr int hex = 16;
    std::vector<MemoryMapping> mappings;
    for (const std::string_view line : lines(text)) {
        if (line.find_first_not_of(" \t") == std::string_view::npos) {
            continue;
        }
        // "start-end perms offset major:minor inode   path"
        std::string_view rest = line;
        const auto range = take_field(rest);
        const auto perms = take_field(rest);
        const auto offset = take_field(rest);
        const auto device = take_field(rest);
        const auto inode = take_field(rest);
        if (!inode) {
            return fail(ParseError::truncated);
        }
        const auto dash = range->find('-');
        const auto colon = device->find(':');
        if (dash == std::string_view::npos || colon == std::string_view::npos || perms->size() != 4) {
            return fail(ParseError::malformed);
        }
        const auto start = to_number<std::uint64_t>(range->substr(0, dash), hex);
        const auto end = to_number<std::uint64_t>(range->substr(dash + 1), hex);
        const auto file_offset = to_number<std::uint64_t>(*offset, hex);
        const auto major = to_number<std::uint32_t>(device->substr(0, colon), hex);
        const auto minor = to_number<std::uint32_t>(device->substr(colon + 1), hex);
        const auto inode_number = to_number<std::uint64_t>(*inode);
        if (!start || !end || !file_offset || !major || !minor || !inode_number) {
            return fail(ParseError::bad_number);
        }
        const auto readable = permission_flag((*perms)[0], 'r');
        const auto writable = permission_flag((*perms)[1], 'w');
        const auto executable = permission_flag((*perms)[2], 'x');
        const char sharing = (*perms)[3];
        if (*end < *start || !readable || !writable || !executable || (sharing != 'p' && sharing != 's')) {
            return fail(ParseError::malformed);
        }
        MemoryMapping mapping{
            .start = *start,
            .end = *end,
            .readable = *readable,
            .writable = *writable,
            .executable = *executable,
            .shared = sharing == 's',
            .offset = *file_offset,
            .device_major = *major,
            .device_minor = *minor,
            .inode = *inode_number,
            .path = {},
        };
        // The path follows the inode's column padding and runs to the end of the line.
        if (const auto path_start = rest.find_first_not_of(" \t"); path_start != std::string_view::npos) {
            mapping.path = std::string{rest.substr(path_start)};
        }
        mappings.push_back(std::move(mapping));
    }
    return mappings;
}

} // namespace lxe::proc
