#pragma once

#include "lxe/model/details.hpp"

#include <span>
#include <string>

namespace lxe::model {

/// Memory mappings as a fixed-width text table with a totals line, like `pmap -x`.
/// The Rss column is blank for mappings without page accounting.
[[nodiscard]] std::string render_maps_text(std::span<const MappingInfo> maps);

} // namespace lxe::model
