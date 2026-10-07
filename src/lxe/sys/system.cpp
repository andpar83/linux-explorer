#include "lxe/sys/system.hpp"

#include <unistd.h>

namespace lxe::sys {

std::uint64_t page_size() noexcept
{
    const long size = ::sysconf(_SC_PAGESIZE);
    constexpr std::uint64_t fallback = 4096;
    return size > 0 ? static_cast<std::uint64_t>(size) : fallback;
}

} // namespace lxe::sys
