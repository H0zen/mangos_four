/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 incremental world-state packet fixtures.
 */

#include "Player.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void check_packet(WorldPacket const& packet, uint8 const* expected,
    size_t size)
{
    CHECK(packet.GetOpcode() == SMSG_UPDATE_WORLD_STATE);
    CHECK(packet.size() == size);
    CHECK(size == 0 || std::memcmp(packet.contents(), expected, size) == 0);
}

static void test_visible_world_state()
{
    WorldPacket packet(SMSG_UPDATE_WORLD_STATE, 9);
    MopWorldEntryPackets::BuildUpdateWorldState(
        packet, 0x11223344u, 0xA1B2C3D4u, false);

    static uint8 const expected[] = {
        0x00,
        0xD4, 0xC3, 0xB2, 0xA1,
        0x44, 0x33, 0x22, 0x11,
    };
    check_packet(packet, expected, sizeof(expected));
}

static void test_hidden_world_state()
{
    WorldPacket packet(SMSG_UPDATE_WORLD_STATE, 9);
    MopWorldEntryPackets::BuildUpdateWorldState(
        packet, 0x01020304u, 0x05060708u, true);

    static uint8 const expected[] = {
        0x80,
        0x08, 0x07, 0x06, 0x05,
        0x04, 0x03, 0x02, 0x01,
    };
    check_packet(packet, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_visible_world_state();
    test_hidden_world_state();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_world_state_packets: all checks passed\n");
    return 0;
}
