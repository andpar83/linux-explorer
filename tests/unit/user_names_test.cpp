#include "lxe/sys/users.hpp"

#include <catch2/catch_test_macros.hpp>

using lxe::UserId;
using lxe::sys::UserNames;

TEST_CASE("sys.user_names.caches_lookups")
{
    int calls = 0;
    UserNames names{[&calls](UserId) -> std::optional<std::string> {
        ++calls;
        return "alice";
    }};
    CHECK(names.name(UserId{1000}) == "alice");
    CHECK(names.name(UserId{1000}) == "alice");
    CHECK(calls == 1);
}

TEST_CASE("sys.user_names.unknown_uids_show_the_number")
{
    UserNames names{[](UserId) -> std::optional<std::string> { return std::nullopt; }};
    CHECK(names.name(UserId{4242}) == "4242");
}
