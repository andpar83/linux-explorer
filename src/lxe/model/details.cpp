#include "lxe/model/details.hpp"

#include <numeric>
#include <utility>

namespace lxe::model {

std::uint64_t ProcessDetails::mapped_bytes() const noexcept
{
    return std::accumulate(maps.begin(), maps.end(), std::uint64_t{0}, [](std::uint64_t sum, const auto& mapping) {
        return sum + mapping.size();
    });
}

ProcessDetails load_details(const proc::ProcFs& fs, std::optional<ProcessId> pid)
{
    ProcessDetails details{.pid = pid, .maps = {}, .error = {}};
    if (!pid) {
        return details;
    }
    if (auto maps = fs.read_maps(*pid)) {
        details.maps = std::move(*maps);
    }
    else {
        details.error = maps.error();
    }
    return details;
}

} // namespace lxe::model
