/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for the directly verified 5.4.8 quest turn-in packets.
 */

#include "GossipDef.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket MakePacket(OpcodesList opcode,
    std::initializer_list<uint8_t> bytes)
{
    WorldPacket packet(opcode, bytes.size());
    for (uint8_t byte : bytes)
    {
        packet << byte;
    }
    return packet;
}

static uint8_t HexNibble(char value)
{
    return value >= 'a' ? uint8_t(value - 'a' + 10) :
        value >= 'A' ? uint8_t(value - 'A' + 10) :
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
    CHECK(packet.size() == expected.size());
    for (size_t index = 0; index < packet.size() &&
        index < expected.size(); ++index)
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

static void TestCompleteQuestRequest()
{
    MopQuestGiverPackets::CompleteQuestRequest request;
    WorldPacket dense = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFE, 0x80, 0x00, 0x02,
          0x03, 0x04, 0x05, 0x06, 0x09, 0x07 });
    CHECK(MopQuestGiverPackets::ParseCompleteQuest(dense, request));
    CHECK(request.questId == 0x12345678u);
    CHECK(request.questGiverGuid == UINT64_C(0x0807060504030201));
    CHECK(!request.completionContext);

    WorldPacket withContext = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFF, 0x80, 0x00, 0x02,
          0x03, 0x04, 0x05, 0x06, 0x09, 0x07 });
    CHECK(MopQuestGiverPackets::ParseCompleteQuest(withContext, request));
    CHECK(request.completionContext);

    WorldPacket sparse = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x00 });
    CHECK(MopQuestGiverPackets::ParseCompleteQuest(sparse, request));
    CHECK(request.questGiverGuid == 0);

    WorldPacket truncated = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0xFE, 0x80, 0x00 });
    CHECK(!MopQuestGiverPackets::ParseCompleteQuest(truncated, request));
    CHECK(truncated.rpos() == truncated.size());

    WorldPacket padding = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x00, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseCompleteQuest(padding, request));

    WorldPacket noncanonical = MakePacket(CMSG_QUESTGIVER_COMPLETE_QUEST,
        { 0x78, 0x56, 0x34, 0x12, 0x80, 0x00, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseCompleteQuest(noncanonical, request));
}

static void TestChooseRewardRequest()
{
    MopQuestGiverPackets::ChooseRewardRequest request;
    WorldPacket dense = MakePacket(CMSG_QUESTGIVER_CHOOSE_REWARD,
        { 0xD4, 0xC3, 0xB2, 0xA1, 0x78, 0x56, 0x34, 0x12,
          0xFF, 0x03, 0x02, 0x07, 0x09, 0x00, 0x05, 0x06,
          0x04 });
    CHECK(MopQuestGiverPackets::ParseChooseReward(dense, request));
    CHECK(request.rewardItemId == 0xA1B2C3D4u);
    CHECK(request.questId == 0x12345678u);
    CHECK(request.questGiverGuid == UINT64_C(0x0807060504030201));

    WorldPacket sparse = MakePacket(CMSG_QUESTGIVER_CHOOSE_REWARD,
        { 0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12,
          0x00 });
    CHECK(MopQuestGiverPackets::ParseChooseReward(sparse, request));
    CHECK(request.rewardItemId == 0);
    CHECK(request.questGiverGuid == 0);

    WorldPacket truncated = MakePacket(CMSG_QUESTGIVER_CHOOSE_REWARD,
        { 0xD4, 0xC3, 0xB2, 0xA1, 0x78, 0x56, 0x34, 0x12,
          0xFF, 0x03 });
    CHECK(!MopQuestGiverPackets::ParseChooseReward(truncated, request));

    WorldPacket noncanonical = MakePacket(CMSG_QUESTGIVER_CHOOSE_REWARD,
        { 0x00, 0x00, 0x00, 0x00, 0x78, 0x56, 0x34, 0x12,
          0x04, 0x01 });
    CHECK(!MopQuestGiverPackets::ParseChooseReward(noncanonical, request));
}

static void TestQuestgiverOfferRewardVariableFields()
{
    MopQuestGiverPackets::QuestRewardOffer offer;
    offer.questGiverGuid =
        ObjectGuid(UINT64_C(0x0807060504030201));
    offer.questTurnTargetName = "TN";
    offer.questTitle = "Q1";
    offer.offerRewardText = "OR";
    offer.questTurnTextWindow = "TI";
    offer.questGiverTargetName = "GN";
    offer.questGiverTextWindow = "GT";
    offer.autoLaunch = true;
    MopQuestGiverPackets::QuestEmote emote;
    emote.delay = 0x11121314;
    emote.emote = 0x21222324;
    offer.emotes.push_back(emote);

    WorldPacket packet;
    CHECK(MopQuestGiverPackets::BuildQuestOfferReward(packet, offer));
    CHECK(packet.size() == 327);
    for (size_t index = 0; index < 288; ++index)
    {
        CHECK(packet[index] == 0);
    }
    std::vector<uint8_t> const tail = BytesFromHex(
        "0080A00001C050200800BE544E51311413121124232221024F"
        "525449474E070347540009060405");
    for (size_t index = 0; index < tail.size(); ++index)
    {
        if (packet[288 + index] != tail[index])
        {
            std::fprintf(stderr,
                "OFFER TAIL offset=%zu actual=%02X expected=%02X\n",
                index, packet[288 + index], tail[index]);
            ++g_fail;
        }
    }
}

static void TestQuestCompletionNotifications()
{
    MopQuestGiverPackets::QuestRewardSummary summary;
    summary.bonusTalents = 0x01020304;
    summary.money = 0x11121314;
    summary.questId = 0x21222324;
    summary.experience = 0x31323334;

    WorldPacket reward;
    MopQuestGiverPackets::BuildQuestRewardSummary(reward, summary);
    CHECK(reward.GetOpcode() == SMSG_QUESTGIVER_QUEST_COMPLETE);
    CheckBytes(reward, BytesFromHex(
        "80040302011413121124232221000000003433323100000000"));

    WorldPacket update;
    MopQuestGiverPackets::BuildQuestUpdateComplete(
        update, 0xA1B2C3D4);
    CHECK(update.GetOpcode() == SMSG_QUESTUPDATE_COMPLETE);
    CheckBytes(update, BytesFromHex("D4C3B2A1"));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestCompleteQuestRequest();
    TestChooseRewardRequest();
    TestQuestgiverOfferRewardVariableFields();
    TestQuestCompletionNotifications();

    if (g_fail != 0)
    {
        std::fprintf(stderr, "%d test(s) failed\n", g_fail);
        return 1;
    }

    std::puts("mop_questgiver_turnin_packets_test: PASS");
    return 0;
}
