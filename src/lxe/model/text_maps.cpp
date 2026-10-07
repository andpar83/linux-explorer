#include "lxe/model/text_maps.hpp"

#include "lxe/model/format.hpp"

#include <cstdint>
#include <format>

namespace lxe::model {

std::string render_maps_text(std::span<const proc::MemoryMapping> maps)
{
    std::string out = std::format("{:<16} {:>10} {:<5} {:<8} {}\n", "Address", "Size", "Perms", "Offset", "Path");
    std::uint64_t total = 0;
    for (const auto& mapping : maps) {
        total += mapping.size();
        out += std::format(
            "{:016x} {:>10} {:<5} {:08x} {}\n",
            mapping.start,
            format_bytes(mapping.size()),
            format_permissions(mapping),
            mapping.offset,
            mapping.path
        );
    }
    out += std::format("{} mappings, {} mapped\n", maps.size(), format_bytes(total));
    return out;
}

} // namespace lxe::model
