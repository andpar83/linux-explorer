#include "lxe/sys/file.hpp"

#include "lxe/sys/unique_fd.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <span>

namespace lxe::sys {

std::expected<std::string, std::error_code> read_file(const std::filesystem::path& path, std::size_t max_bytes)
{
    const UniqueFd fd{::open(path.c_str(), O_RDONLY | O_CLOEXEC)};
    if (!fd.valid()) {
        return std::unexpected{std::error_code{errno, std::system_category()}};
    }

    constexpr std::size_t chunk = 4096;
    std::string content;
    while (content.size() < max_bytes) {
        const std::size_t old_size = content.size();
        const std::size_t wanted = std::min(chunk, max_bytes - old_size);
        content.resize(old_size + wanted);
        const ssize_t got = ::read(fd.get(), std::span{content}.subspan(old_size).data(), wanted);
        if (got < 0) {
            if (errno == EINTR) {
                content.resize(old_size);
                continue;
            }
            return std::unexpected{std::error_code{errno, std::system_category()}};
        }
        content.resize(old_size + static_cast<std::size_t>(got));
        if (got == 0) {
            break;
        }
    }
    return content;
}

} // namespace lxe::sys
