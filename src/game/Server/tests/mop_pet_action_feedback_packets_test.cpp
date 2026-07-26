/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 pet-action feedback packet fixtures.
 */

#include "Unit.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void check_packet(WorldPacket const& packet, uint8 const* expected,
    size_t size)
{
    CHECK(packet.GetOpcode() == SMSG_PET_ACTION_FEEDBACK);
    CHECK(packet.size() == size);
    if (size != 0 && std::memcmp(packet.contents(), expected, size) != 0)
    {
        std::fprintf(stderr, "actual:");
        for (size_t index = 0; index < packet.size(); ++index)
            std::fprintf(stderr, " %02X", packet.contents()[index]);
        std::fprintf(stderr, "\n");
        CHECK(false);
    }
}

static void test_pet_dead_without_spell_context()
{
    WorldPacket packet;
    MopCompactPackets::BuildPetActionFeedback(packet, 1u, 0u);

    static uint8 const expected[] = { 0x80, 0x01 };
    check_packet(packet, expected, sizeof(expected));
}

static void test_nothing_to_attack_with_spell_context()
{
    WorldPacket packet;
    MopCompactPackets::BuildPetActionFeedback(packet, 2u, 0x12345678u);

    static uint8 const expected[] = {
        0x00, 0x02,
        0x78, 0x56, 0x34, 0x12,
    };
    check_packet(packet, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_pet_dead_without_spell_context();
    test_nothing_to_attack_with_spell_context();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_pet_action_feedback_packets: all checks passed\n");
    return 0;
}
