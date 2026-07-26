/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 quest-progress packet fixtures.
 */

#include "Player.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void check_packet(WorldPacket const& packet, uint8 const* expected,
    size_t size)
{
    CHECK(packet.GetOpcode() == SMSG_QUESTUPDATE_ADD_KILL);
    CHECK(packet.size() == size);
    CHECK(size == 0 || std::memcmp(packet.contents(), expected, size) == 0);
}

static void test_creature_kill_full_guid()
{
    MopQuestPackets::QuestProgressCredit credit;
    credit.count = 0x1234;
    credit.type = MopQuestPackets::QuestProgressObjectiveType::CreatureKill;
    credit.questId = 0x55667788;
    credit.requiredCount = 0x9ABC;
    credit.objectId = 0x11223344;
    credit.targetGuid = UINT64_C(0x0807060504030201);

    WorldPacket packet;
    MopQuestPackets::BuildQuestProgressCredit(packet, credit);

    static uint8 const expected[] = {
        0x34, 0x12, 0x00,
        0x88, 0x77, 0x66, 0x55,
        0xBC, 0x9A,
        0x44, 0x33, 0x22, 0x11,
        0xFF,
        0x02, 0x09, 0x05, 0x00, 0x04, 0x07, 0x03, 0x06,
    };
    check_packet(packet, expected, sizeof(expected));
}

static void test_gameobject_uses_plain_id_and_zero_guid()
{
    MopQuestPackets::QuestProgressCredit credit;
    credit.count = 1;
    credit.type = MopQuestPackets::QuestProgressObjectiveType::GameObject;
    credit.questId = 0x01020304;
    credit.requiredCount = 5;
    credit.objectId = 0x00123456;

    WorldPacket packet;
    MopQuestPackets::BuildQuestProgressCredit(packet, credit);

    static uint8 const expected[] = {
        0x01, 0x00, 0x02,
        0x04, 0x03, 0x02, 0x01,
        0x05, 0x00,
        0x56, 0x34, 0x12, 0x00,
        0x00,
    };
    check_packet(packet, expected, sizeof(expected));
}

static void test_creature_interact_sparse_guid()
{
    MopQuestPackets::QuestProgressCredit credit;
    credit.count = 2;
    credit.type = MopQuestPackets::QuestProgressObjectiveType::CreatureInteract;
    credit.questId = 7;
    credit.requiredCount = 3;
    credit.objectId = 9;
    credit.targetGuid = UINT64_C(0x0022000000000010);

    WorldPacket packet;
    MopQuestPackets::BuildQuestProgressCredit(packet, credit);

    static uint8 const expected[] = {
        0x02, 0x00, 0x03,
        0x07, 0x00, 0x00, 0x00,
        0x03, 0x00,
        0x09, 0x00, 0x00, 0x00,
        0x90, 0x11, 0x23,
    };
    check_packet(packet, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_creature_kill_full_guid();
    test_gameobject_uses_plain_id_and_zero_guid();
    test_creature_interact_sparse_guid();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_quest_progress_packets: all checks passed\n");
    return 0;
}
