#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <system_error>

namespace lxe::sys {

/// Default cap for read_file(): far more than any /proc text file the application reads.
inline constexpr std::size_t default_read_limit = 1024 * 1024;

/// Reads a whole file into memory, stopping after `max_bytes`.
/// Works for files that report size 0, as everything under /proc does.
[[nodiscard]] std::expected<std::string, std::error_code> read_file(
    const std::filesystem::path& path,
    std::size_t max_bytes = default_read_limit
);

} // namespace lxe::sys
