#pragma once

#include "lxe/proc/parse.hpp"

#include <span>
#include <string>

namespace lxe::model {

/// Memory mappings as a fixed-width text table with a totals line, like `pmap`.
[[nodiscard]] std::string render_maps_text(std::span<const proc::MemoryMapping> maps);

} // namespace lxe::model
