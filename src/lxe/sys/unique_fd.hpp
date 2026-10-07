#pragma once

#include <unistd.h>

#include <utility>

namespace lxe::sys {

/// Owning file descriptor: closes it on destruction. Move-only.
class UniqueFd
{
public:
    UniqueFd() noexcept = default;

    explicit UniqueFd(int fd) noexcept
    : fd_{fd}
    {}

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept
    : fd_{std::exchange(other.fd_, -1)}
    {}

    UniqueFd& operator=(UniqueFd&& other) noexcept
    {
        if (this != &other) {
            reset(std::exchange(other.fd_, -1));
        }
        return *this;
    }

    ~UniqueFd() { reset(); }

    [[nodiscard]] int get() const noexcept { return fd_; }

    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

    void reset(int fd = -1) noexcept
    {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }

private:
    int fd_ = -1;
};

} // namespace lxe::sys
