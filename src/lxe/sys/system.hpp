#pragma once

#include <cstdint>

namespace lxe::sys {

/// Size of a memory page in bytes (used to convert resident pages to bytes).
[[nodiscard]] std::uint64_t page_size() noexcept;

} // namespace lxe::sys
