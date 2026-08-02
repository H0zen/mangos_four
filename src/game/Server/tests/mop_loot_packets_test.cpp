/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "Group.h"
#include "LootMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket Packet(OpcodesList opcode, std::initializer_list<uint8> body)
{
    WorldPacket packet(opcode, body.size());
    for (uint8 value : body)
    {
        packet << value;
    }
    return packet;
}

static void CheckPacket(WorldPacket const& packet, OpcodesList opcode,
    std::initializer_list<uint8> expected)
{
    CHECK(packet.GetOpcode() == opcode);
    CHECK(packet.size() == expected.size());
    size_t index = 0;
    for (uint8 value : expected)
    {
        CHECK(index < packet.size());
        if (index < packet.size())
        {
            CHECK(packet[index] == value);
        }
        ++index;
    }
}

static void TestLootRequest()
{
    WorldPacket dense = Packet(CMSG_LOOT,
        { 0xFF, 0x05, 0x07, 0x00, 0x06, 0x04, 0x03, 0x09, 0x02 });
    uint64 guid = 0;
    CHECK(MopLootPackets::ParseLootRequest(dense, guid));
    CHECK(guid == UINT64_C(0x0807060504030201));
    CHECK(dense.rpos() == dense.size());

    WorldPacket sparse = Packet(CMSG_LOOT, { 0x18, 0x10, 0x89 });
    CHECK(MopLootPackets::ParseLootRequest(sparse, guid));
    CHECK(guid == UINT64_C(0x8800000000000011));

    WorldPacket zero = Packet(CMSG_LOOT, { 0x00 });
    CHECK(MopLootPackets::ParseLootRequest(zero, guid));
    CHECK(guid == 0);
}

static MopLootPackets::LootItem DenseGroupLootItem()
{
    MopLootPackets::LootItem item;
    item.itemId = 0x55667788;
    item.displayInfoId = 0xDDEEFF00;
    item.count = 2;
    item.randomPropertyId = int32(0x99AABBCC);
    item.randomSuffix = 0x11223344;
    item.optionalByte = 5;
    item.hasOptionalByte = true;
    item.lootListId = 7;
    item.hasLootListId = true;
    item.unknown = 3;
    item.slotType = 4;
    item.canTradeToTapList = true;
    item.situ = { 0xDD, 0xCC, 0xBB, 0xAA };
    return item;
}

static void TestGroupLootRollWinner()
{
    MopGroupLootPackets::RollWinner winner;
    winner.lootGuid = UINT64_C(0x0807060504030201);
    winner.winnerGuid = UINT64_C(0x1817161514131211);
    winner.rollNumber = 100;
    winner.itemSlot = 7;
    winner.rollType = 1;
    winner.item = DenseGroupLootItem();

    WorldPacket packet;
    CHECK(MopGroupLootPackets::BuildRollWinner(packet, winner));
    CheckPacket(packet, SMSG_LOOT_ROLL_WON,
        {
            0xFF, 0x7F, 0x37, 0x04, 0x00, 0x00, 0x00, 0xDD,
            0xCC, 0xBB, 0xAA, 0x00, 0xFF, 0xEE, 0xDD, 0x12,
            0x09, 0x44, 0x33, 0x22, 0x11, 0x17, 0x05, 0x19,
            0x03, 0x02, 0x00, 0x15, 0x64, 0x00, 0x00, 0x00,
            0xCC, 0xBB, 0xAA, 0x99, 0x06, 0x13, 0x14, 0x01,
            0x16, 0x07, 0x04, 0x88, 0x77, 0x66, 0x55, 0x07,
            0x10, 0x02, 0x00, 0x00, 0x00, 0x05
        });
}

static MopLootPackets::LootResponse BaseLootResponse()
{
    MopLootPackets::LootResponse response;
    response.sourceGuid = UINT64_C(0x0807060504030201);
    response.lootGuid = UINT64_C(0x1817161514131211);
    response.hasLootType = true;
    response.lootType = 1;
    response.success = true;
    return response;
}

static void TestDenseItemLootResponse()
{
    MopLootPackets::LootResponse response = BaseLootResponse();
    MopLootPackets::LootItem item;
    item.slotType = 4;
    item.canTradeToTapList = true;
    item.hasOptionalByte = true;
    item.optionalByte = 5;
    item.hasLootListId = true;
    item.lootListId = 7;
    item.unknown = 3;
    item.randomSuffix = 0x11223344;
    item.count = 2;
    item.itemId = 0x55667788;
    item.situ = { 0xDD, 0xCC, 0xBB, 0xAA };
    item.randomPropertyId = int32(0x99AABBCC);
    item.displayInfoId = 0xDDEEFF00;
    response.items.push_back(item);

    WorldPacket packet;
    CHECK(MopLootPackets::BuildLootResponse(packet, response));
    CheckPacket(packet, SMSG_LOOT_RESPONSE,
        {
            0xA0, 0x00, 0x01, 0xFF, 0x60, 0x00, 0x07, 0x27, 0xFC,
            0x44, 0x33, 0x22, 0x11,
            0x02, 0x00, 0x00, 0x00,
            0x88, 0x77, 0x66, 0x55,
            0x04, 0x00, 0x00, 0x00, 0xDD, 0xCC, 0xBB, 0xAA,
            0x05,
            0xCC, 0xBB, 0xAA, 0x99,
            0x07,
            0x00, 0xFF, 0xEE, 0xDD,
            0x12, 0x19, 0x07, 0x15, 0x14, 0x01, 0x04, 0x17,
            0x02, 0x05, 0x13, 0x00, 0x10, 0x06, 0x09, 0x03, 0x16
        });
}

int main(int /*argc*/, char** /*argv*/)
{
    TestLootRequest();
    TestGroupLootRollWinner();
    TestDenseItemLootResponse();
    return g_fail ? 1 : 0;
}
