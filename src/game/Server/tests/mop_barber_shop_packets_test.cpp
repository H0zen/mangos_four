/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 barber-shop opener packet fixture.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_barber_shop_open_packet_is_empty()
{
    WorldPacket packet(SMSG_ENABLE_BARBER_SHOP, 0);
    CHECK(uint32(packet.GetOpcode()) == 0x1222u);
    CHECK(packet.empty());
}

int main(int /*argc*/, char** /*argv*/)
{
    test_barber_shop_open_packet_is_empty();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_barber_shop_packets: all checks passed\n");
    return 0;
}
