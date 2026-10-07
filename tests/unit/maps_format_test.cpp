#include "lxe/model/details.hpp"
#include "lxe/model/format.hpp"
#include "lxe/model/text_maps.hpp"
#include "support/builders.hpp"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using lxe::model::format_permissions;
using lxe::model::render_maps_text;
using lxe::test::mapping;

TEST_CASE("model.format_permissions.matches_proc_notation")
{
    CHECK(format_permissions(mapping(0, 1, {}, "r-xp")) == "r-xp");
    CHECK(format_permissions(mapping(0, 1, {}, "rw-s")) == "rw-s");
    CHECK(format_permissions(mapping(0, 1, {}, "---p")) == "---p");
    CHECK(format_permissions(mapping(0, 1, {}, "--xp")) == "--xp");
}

TEST_CASE("model.text_maps.renders_a_fixed_width_table")
{
    const std::vector maps{
        mapping(0x400000, 0x401000, "/home/alice/my prog", "r-xp"),
        mapping(0x7f0000000000, 0x7f0000021000, "[heap]", "rw-p"),
        mapping(0x7f0000300000, 0x7f0000301000, {}, "---p"),
        mapping(0xffffffffff600000, 0xffffffffff601000, "[vsyscall]", "--xp"),
    };
    CHECK(render_maps_text(maps) == "Address                Size Perms Offset   Path\n"
                                    "0000000000400000    4.0 KiB r-xp  00000000 /home/alice/my prog\n"
                                    "00007f0000000000  132.0 KiB rw-p  00000000 [heap]\n"
                                    "00007f0000300000    4.0 KiB ---p  00000000 \n"
                                    "ffffffffff600000    4.0 KiB --xp  00000000 [vsyscall]\n"
                                    "4 mappings, 144.0 KiB mapped\n");
}

TEST_CASE("model.text_maps.empty_list")
{
    CHECK(render_maps_text({}) == "Address                Size Perms Offset   Path\n0 mappings, 0 B mapped\n");
}

TEST_CASE("model.details.sums_mapped_bytes")
{
    lxe::model::ProcessDetails details;
    CHECK(details.mapped_bytes() == 0);
    details.maps = {mapping(0, 0x1000), mapping(0x1000, 0x3000)};
    CHECK(details.mapped_bytes() == 0x3000U);
}
