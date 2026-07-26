/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 fishing outcome packet fixtures.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_fishing_failure_packets_are_empty()
{
    WorldPacket escaped(SMSG_FISH_ESCAPED, 0);
    CHECK(uint32(escaped.GetOpcode()) == 0x0227u);
    CHECK(escaped.empty());

    WorldPacket notHooked(SMSG_FISH_NOT_HOOKED, 0);
    CHECK(uint32(notHooked.GetOpcode()) == 0x10BEu);
    CHECK(notHooked.empty());
}

int main(int /*argc*/, char** /*argv*/)
{
    test_fishing_failure_packets_are_empty();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_fishing_packets: all checks passed\n");
    return 0;
}
