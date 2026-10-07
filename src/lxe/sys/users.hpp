#pragma once

#include "lxe/ids.hpp"

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace lxe::sys {

/// Account name for `uid` from the system user database (NSS), if it has one.
[[nodiscard]] std::optional<std::string> lookup_system_user(UserId uid);

/// Effective user id of this process.
[[nodiscard]] UserId current_user() noexcept;

/// Caches uid -> account name lookups. Unknown uids are shown as their number.
class UserNames
{
public:
    using Lookup = std::move_only_function<std::optional<std::string>(UserId) const>;

    /// Uses the system user database.
    UserNames();

    /// Uses `lookup` instead of the system user database (tests inject a fake).
    explicit UserNames(Lookup lookup);

    [[nodiscard]] const std::string& name(UserId uid);

private:
    Lookup lookup_;
    std::unordered_map<UserId, std::string> cache_;
};

} // namespace lxe::sys
