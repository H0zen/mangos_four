/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 AI-reaction packet fixtures.
 */

#include "Unit.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void check_packet(WorldPacket const& packet, uint8 const* expected,
    size_t size)
{
    CHECK(packet.GetOpcode() == SMSG_AI_REACTION);
    CHECK(packet.size() == size);
    CHECK(size == 0 || std::memcmp(packet.contents(), expected, size) == 0);
}

static void test_dense_hostile_reaction()
{
    WorldPacket packet;
    MopCompactPackets::BuildAIReaction(
        packet, ObjectGuid(0x0807060504030201ULL), AI_REACTION_HOSTILE);

    static uint8 const expected[] = {
        0xFF,
        0x04, 0x06, 0x07,
        0x02, 0x00, 0x00, 0x00,
        0x09, 0x03, 0x02, 0x00, 0x05,
    };
    check_packet(packet, expected, sizeof(expected));
}

static void test_sparse_alert_reaction()
{
    WorldPacket packet;
    MopCompactPackets::BuildAIReaction(
        packet, ObjectGuid(0x0000AA00000000BBULL), AI_REACTION_ALERT);

    static uint8 const expected[] = {
        0xA0,
        0xAB,
        0x00, 0x00, 0x00, 0x00,
        0xBA,
    };
    check_packet(packet, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_dense_hostile_reaction();
    test_sparse_alert_reaction();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_ai_reaction_packets: all checks passed\n");
    return 0;
}
