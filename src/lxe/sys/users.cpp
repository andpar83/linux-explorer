#include "lxe/sys/users.hpp"

#include <pwd.h>
#include <unistd.h>

#include <cerrno>
#include <format>
#include <utility>
#include <vector>

namespace lxe::sys {

std::optional<std::string> lookup_system_user(UserId uid)
{
    const long suggested = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    std::size_t size = suggested > 0 ? static_cast<std::size_t>(suggested) : 1024;
    // getpwuid_r reports ERANGE while the buffer is too small; a few doublings are plenty.
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::vector<char> buffer(size);
        passwd entry{};
        passwd* result = nullptr;
        const int rc = ::getpwuid_r(std::to_underlying(uid), &entry, buffer.data(), buffer.size(), &result);
        if (rc == ERANGE) {
            size *= 2;
            continue;
        }
        if (rc != 0 || result == nullptr || entry.pw_name == nullptr) {
            return std::nullopt;
        }
        return std::string{entry.pw_name};
    }
    return std::nullopt;
}

UserId current_user() noexcept
{
    return UserId{::geteuid()};
}

UserNames::UserNames()
: UserNames{Lookup{lookup_system_user}}
{}

UserNames::UserNames(Lookup lookup)
: lookup_{std::move(lookup)}
{}

const std::string& UserNames::name(UserId uid)
{
    if (const auto found = cache_.find(uid); found != cache_.end()) {
        return found->second;
    }
    auto name = lookup_(uid).value_or(std::format("{}", uid));
    return cache_.emplace(uid, std::move(name)).first->second;
}

} // namespace lxe::sys
