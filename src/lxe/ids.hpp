#pragma once

#include <sys/types.h>

#include <format>
#include <utility>

namespace lxe {

/// Kernel process id (thread-group id). A distinct type so it can't be mixed up with other integers.
enum class ProcessId : pid_t
{
};

/// Numeric user id.
enum class UserId : uid_t
{
};

} // namespace lxe

/// Formats as the plain number, e.g. std::format("{}", ProcessId{42}) == "42".
template <>
struct std::formatter<lxe::ProcessId> : std::formatter<pid_t>
{
    template <class FormatContext>
    auto format(lxe::ProcessId pid, FormatContext& ctx) const
    {
        return std::formatter<pid_t>::format(std::to_underlying(pid), ctx);
    }
};

template <>
struct std::formatter<lxe::UserId> : std::formatter<uid_t>
{
    template <class FormatContext>
    auto format(lxe::UserId uid, FormatContext& ctx) const
    {
        return std::formatter<uid_t>::format(std::to_underlying(uid), ctx);
    }
};
