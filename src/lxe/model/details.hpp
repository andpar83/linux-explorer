#pragma once

#include "lxe/ids.hpp"
#include "lxe/proc/parse.hpp"
#include "lxe/proc/proc_fs.hpp"

#include <cstdint>
#include <optional>
#include <system_error>
#include <vector>

namespace lxe::model {

/// What the lower pane shows about the selected process.
struct ProcessDetails
{
    std::optional<ProcessId> pid;          ///< nullopt: nothing selected.
    std::vector<proc::MemoryMapping> maps; ///< Memory mappings, valid when `error` is empty.
    std::error_code error;                 ///< Why `maps` could not be read (permission, exited).

    [[nodiscard]] std::uint64_t mapped_bytes() const noexcept;
};

/// Reads the details of `pid`; an empty result when nothing is selected.
[[nodiscard]] ProcessDetails load_details(const proc::ProcFs& fs, std::optional<ProcessId> pid);

} // namespace lxe::model
