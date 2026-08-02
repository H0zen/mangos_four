/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for directly verified 5.4.8 quest acquisition packets.
 */

#include "GossipDef.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <initializer_list>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static uint8_t HexNibble(char value)
{
    return value >= 'a' ? uint8_t(value - 'a' + 10) :
        uint8_t(value - '0');
}

static std::vector<uint8_t> BytesFromHex(char const* hex)
{
    std::vector<uint8_t> bytes;
    while (*hex != '\0')
    {
        bytes.push_back(uint8_t((HexNibble(hex[0]) << 4) |
            HexNibble(hex[1])));
        hex += 2;
    }
    return bytes;
}

static void CheckBytes(WorldPacket const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "SIZE actual=%zu expected=%zu\n",
            packet.size(), expected.size());
        ++g_fail;
    }
    for (size_t index = 0; index < expected.size() &&
        index < packet.size(); ++index)
    {
        if (packet[index] != expected[index])
        {
            std::fprintf(stderr,
                "BYTE offset=%zu actual=%02X expected=%02X\n",
                index, packet[index], expected[index]);
            ++g_fail;
        }
    }
}

static void TestQuestgiverQuestDetails()
{
    MopQuestGiverPackets::QuestDetails details;
    details.questGiverGuid =
        ObjectGuid(UINT64_C(0x0102030405060708));
    details.questId = 12345;
    details.suggestedPlayers = 1;
    details.questGiverTextWindow = "NPC";
    details.questTitle = "Test Quest";
    details.questGiverTargetName = "Guide";
    details.questDetails = "Do the thing.";
    details.questObjectives = "Complete it.";

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestDetails(packet, details));
    CHECK(packet.GetOpcode() == SMSG_QUESTGIVER_QUEST_DETAILS);

    std::vector<uint8_t> expected(354, 0);
    expected[88] = 0x39;
    expected[89] = 0x30;
    expected[92] = 0x01;
    std::vector<uint8_t> const tail = BytesFromHex(
        "4020180a00000305a80008068000000000001847756964655465"
        "737420517565737403436f6d706c6574652069742e4e5043446f"
        "20746865207468696e672e00040902060705");
    for (size_t index = 0; index < tail.size(); ++index)
    {
        expected[284 + index] = tail[index];
    }
    CheckBytes(packet, expected);
}

// Reproduces a real retail 18414 response byte-for-byte in size and layout.
//
// Quest 27353 "Blessings of the Elements" was captured from live traffic: a
// 1121-byte response with three item objectives and strings of 58/98/493/25
// bytes. The retail packet places its strings at 235, 301, 451 and 980, and
// repeats the quest id at offset 411. Our builder must land on all of it.
//
// Before the objectives block was populated this produced 1056 bytes with
// every string 65 bytes early, which is what stopped the client auto-watching
// an accepted quest.
static void TestQuestQueryResponseMatchesRetail27353()
{
    MopQuestQueryPackets::Response response;
    response.questId = 27353;
    response.innerQuestId = 27353;
    response.completedText = std::string(58, 'c');
    response.objectivesText = std::string(98, 'o');
    response.details = std::string(493, 'd');
    response.title = std::string(25, 't');

    uint32_t const objectIds[3] = { 60881, 60873, 60875 };
    int32_t const amounts[3] = { 1, 1, 5 };
    for (uint32_t i = 0; i < 3; ++i)
    {
        MopQuestQueryPackets::Objective objective;
        objective.id = 27353 * 16 + i;
        objective.type = MopQuestQueryPackets::OBJECTIVE_ITEM;
        objective.index = uint8_t(i);
        objective.objectId = objectIds[i];
        objective.amount = amounts[i];
        response.objectives.push_back(objective);
    }

    WorldPacket packet;
    CHECK(MopQuestQueryPackets::BuildResponse(packet, response));
    CHECK(packet.size() == 1121);

    // 4 + ceil((109 + 30*3)/8) == 4 + 25, then three 18-byte objectives.
    CHECK(std::memcmp(&packet[29 + 0 * 18], "\x01\x00\x00\x00", 4) == 0);
    CHECK(packet[29 + 0 * 18 + 12] == 0);       // index
    CHECK(packet[29 + 0 * 18 + 13] == 1);       // type: item
    CHECK(packet[29 + 2 * 18 + 12] == 2);
    CHECK(std::memcmp(&packet[29 + 2 * 18 + 14], "\xcb\xed\x00\x00", 4) == 0);

    // String offsets, straight off the retail capture.
    CHECK(std::memcmp(&packet[235], std::string(58, 'c').data(), 58) == 0);
    CHECK(std::memcmp(&packet[301], std::string(98, 'o').data(), 98) == 0);
    CHECK(std::memcmp(&packet[451], std::string(493, 'd').data(), 493) == 0);
    CHECK(std::memcmp(&packet[980], std::string(25, 't').data(), 25) == 0);

    // Retail repeats the quest id here. Feeding this slot from
    // RepObjectiveFaction left it zero, which made the client compute
    // QUEST_ACCEPTED's arguments from a missing cache entry.
    uint32_t inner = 0;
    std::memcpy(&inner, &packet[411], 4);
    CHECK(inner == 27353);
}

// A reputation requirement has to survive as its own objective now that the
// scalar it used to travel in is known to carry the quest id. Retail marks it
// untracked: type 6, index 255, faction in objectId, standing in amount.
int main(int /*argc*/, char** /*argv*/)
{
    TestQuestgiverQuestDetails();
    TestQuestQueryResponseMatchesRetail27353();

    if (g_fail != 0)
    {
        std::fprintf(stderr, "%d test(s) failed\n", g_fail);
        return 1;
    }

    std::puts("mop_questgiver_acquisition_packets_test: PASS");
    return 0;
}
