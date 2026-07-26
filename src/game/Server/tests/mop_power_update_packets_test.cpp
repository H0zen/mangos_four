/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 power-update packet fixtures.
 */

#include "Unit.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void check_packet(WorldPacket const& packet, uint8 const* expected,
    size_t size)
{
    CHECK(packet.GetOpcode() == SMSG_POWER_UPDATE);
    CHECK(packet.size() == size);
    CHECK(size == 0 || std::memcmp(packet.contents(), expected, size) == 0);
}

static void test_dense_power_update()
{
    WorldPacket packet;
    MopCompactPackets::BuildPowerUpdate(packet,
        ObjectGuid(UINT64_C(0x0807060504030201)), 3, 0x11223344);

    static uint8 const expected[] = {
        0xFF, 0x00, 0x00, 0x08,
        0x09, 0x00, 0x07, 0x05, 0x03, 0x02, 0x04,
        0x03, 0x44, 0x33, 0x22, 0x11,
        0x06,
    };
    check_packet(packet, expected, sizeof(expected));
}

static void test_sparse_power_update()
{
    WorldPacket packet;
    MopCompactPackets::BuildPowerUpdate(packet,
        ObjectGuid(UINT64_C(0x00BB0000000000AA)), 0, 5);

    static uint8 const expected[] = {
        0x42, 0x00, 0x00, 0x08,
        0xAB,
        0x00, 0x05, 0x00, 0x00, 0x00,
        0xBA,
    };
    check_packet(packet, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_dense_power_update();
    test_sparse_power_update();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_power_update_packets: all checks passed\n");
    return 0;
}
