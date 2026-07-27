/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 5.4.8 client build 18414.
 */

/**
 * Byte-exact tests for the 5.4.8 quest-NPC request and response pair.
 *
 * The response grammar was recovered from client parser sub_6B8B3B ->
 * sub_6B8A06 and then confirmed against real 18414 retail captures: every
 * SMSG_QUEST_NPC_QUERY_RESPONSE in the sniff corpus decodes with this reader
 * and leaves no residual byte.
 */

#include "WorldSession.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "  size %u, wanted %u\n",
            unsigned(packet.size()), unsigned(expected.size()));
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "  byte %u = 0x%02X, wanted 0x%02X\n",
                unsigned(i), packet.contents()[i], expected[i]);
            return false;
        }
    }
    return true;
}

static void test_request_parser()
{
    // The live client always sends a 204-byte body, but only the leading
    // uint32 is initialised. The remainder is client stack memory: on the
    // 64-bit client it contains x64 image pointers that are byte-identical
    // across every capture in one run. It must never be parsed.
    {
        WorldPacket request(CMSG_QUEST_NPC_QUERY, 204);
        request << uint32(28766);
        for (uint32 i = 0; i < 50; ++i)
            request << uint32(0xDEADBEEFu);

        uint32 questId = 0;
        CHECK(MopQueryPackets::ParseQuestNpcQueryRequest(request, questId));
        CHECK(questId == 28766);
    }

    // A body carrying only the quest id is equally valid; the tail is not
    // part of the contract, so its absence must not be treated as malformed.
    {
        WorldPacket minimal(CMSG_QUEST_NPC_QUERY, 4);
        minimal << uint32(29080);

        uint32 questId = 0;
        CHECK(MopQueryPackets::ParseQuestNpcQueryRequest(minimal, questId));
        CHECK(questId == 29080);
    }

    // Short of one quest id there is nothing to answer.
    {
        WorldPacket truncated(CMSG_QUEST_NPC_QUERY, 3);
        truncated << uint8(1) << uint8(2) << uint8(3);

        uint32 questId = 0xFFFFFFFFu;
        CHECK(!MopQueryPackets::ParseQuestNpcQueryRequest(truncated, questId));
    }
}

static void test_response_builder()
{
    // 21-bit quest count then one 22-bit NPC count per quest, flushed, then
    // each quest id followed by its NPC ids. 1 and 2 here occupy 43 bits, so
    // the bit phase is six bytes with five padding bits.
    MopQueryPackets::QuestNpcResponse quest;
    quest.questId = 0xE1E2E3E4u;
    quest.npcIds.push_back(0x11121314u);
    quest.npcIds.push_back(0x21222324u);

    WorldPacket packet;
    CHECK(MopQueryPackets::BuildQuestNpcQueryResponse(packet, { quest }));
    CHECK(packet.GetOpcode() == SMSG_QUEST_NPC_QUERY_RESPONSE);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x08, 0x00, 0x00, 0x40,
        0xE4, 0xE3, 0xE2, 0xE1,
        0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21
    }));

    // A quest whose ender set is empty is still a valid answer: the 22-bit
    // count is simply zero. Our converted world database has such rows.
    MopQueryPackets::QuestNpcResponse barren;
    barren.questId = 33147;

    WorldPacket none;
    CHECK(MopQueryPackets::BuildQuestNpcQueryResponse(none, { barren }));
    CHECK(ExpectBytes(none, {
        0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
        0x7B, 0x81, 0x00, 0x00
    }));

    // No quests at all: 21 bits of zero and nothing else.
    WorldPacket empty;
    CHECK(MopQueryPackets::BuildQuestNpcQueryResponse(empty, {}));
    CHECK(empty.GetOpcode() == SMSG_QUEST_NPC_QUERY_RESPONSE);
    CHECK(ExpectBytes(empty, { 0x00, 0x00, 0x00 }));
}

static void test_retail_corpus_shape()
{
    // Reproduces a real 18414 retail response: seven quests with NPC counts
    // [1,1,1,1,1,1,4]. Retail batches several quests into one reply even
    // though the client requests them one at a time, so the builder must not
    // assume a single-quest response. Bit phase is 21 + 7*22 = 175 bits =
    // 22 bytes, matching the capture exactly.
    std::vector<MopQueryPackets::QuestNpcResponse> response;
    uint32 const quests[] = { 24586, 32374, 33333, 33338, 33374, 31926, 32863 };
    uint32 const singles[] = { 20735, 64616, 72870, 72870, 73303, 66557 };
    for (uint32 i = 0; i < 6; ++i)
    {
        MopQueryPackets::QuestNpcResponse entry;
        entry.questId = quests[i];
        entry.npcIds.push_back(singles[i]);
        response.push_back(entry);
    }
    MopQueryPackets::QuestNpcResponse last;
    last.questId = 32863;
    last.npcIds = { 64582, 64572, 63596, 63626 };
    response.push_back(last);

    WorldPacket packet;
    CHECK(MopQueryPackets::BuildQuestNpcQueryResponse(packet, response));
    CHECK(packet.size() == 90);
    CHECK(packet.contents()[0] == 0x00);
    CHECK(packet.contents()[1] == 0x00);
    CHECK(packet.contents()[2] == 0x38);   // 21-bit count of seven
}

static void test_opcode_values()
{
    CHECK(uint32_t(CMSG_QUEST_NPC_QUERY) == 0x1DAEu);
    CHECK(uint32_t(SMSG_QUEST_NPC_QUERY_RESPONSE) == 0x036Du);
    CHECK(uint32_t(CMSG_QUEST_NPC_QUERY) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_QUEST_NPC_QUERY_RESPONSE) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_request_parser();
    test_response_builder();
    test_retail_corpus_shape();
    test_opcode_values();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("mop_quest_npc_query: all checks passed\n");
    return 0;
}
