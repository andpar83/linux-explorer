#include "lxe/model/text_maps.hpp"

#include "lxe/model/format.hpp"

#include <cstdint>
#include <format>

namespace lxe::model {

std::string render_maps_text(std::span<const MappingInfo> maps)
{
    std::string out =
        std::format("{:<16} {:>10} {:>10} {:<5} {:<8} {}\n", "Address", "Size", "Rss", "Perms", "Offset", "Path");
    std::uint64_t total = 0;
    std::uint64_t resident = 0;
    for (const auto& [mapping, stats] : maps) {
        total += mapping.size();
        resident += stats ? stats->rss : 0;
        out += std::format(
            "{:016x} {:>10} {:>10} {:<5} {:08x} {}\n",
            mapping.start,
            format_bytes(mapping.size()),
            stats ? format_bytes(stats->rss) : std::string{},
            format_permissions(mapping),
            mapping.offset,
            mapping.path
        );
    }
    out += std::format("{} mappings, {} mapped, {} resident\n", maps.size(), format_bytes(total), format_bytes(resident));
    return out;
}

} // namespace lxe::model
