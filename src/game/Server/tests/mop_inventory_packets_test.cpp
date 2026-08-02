/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "Item.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket Packet(OpcodesList opcode, std::initializer_list<uint8> body)
{
    WorldPacket packet(opcode, body.size());
    for (uint8 value : body)
        packet << value;
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
            if (packet[index] != value)
                std::fprintf(stderr, "packet byte %zu: got %02X expected %02X\n",
                    index, unsigned(packet[index]), unsigned(value));
            CHECK(packet[index] == value);
        }
        ++index;
    }
}

static void TestSwapInvItemDense()
{
    WorldPacket packet = Packet(CMSG_SWAP_INV_ITEM,
        { 0x21, 0x22, 0x80, 0x32, 0x31, 0x42, 0x41 });
    MopItemPackets::SwapInvItemRequest request;

    CHECK(MopItemPackets::ParseSwapInvItem(packet, request));
    CHECK(request.sourceSlot == 0x21);
    CHECK(request.destinationSlot == 0x22);
    CHECK(request.updates[0].bag == 0x31);
    CHECK(request.updates[0].slot == 0x32);
    CHECK(request.updates[1].bag == 0x41);
    CHECK(request.updates[1].slot == 0x42);
    CHECK(packet.rpos() == packet.size());
}

static void TestAutoEquipItemDense()
{
    WorldPacket packet = Packet(CMSG_AUTOEQUIP_ITEM,
        { 0x21, 0x31, 0x40, 0x22, 0x41 });
    MopItemPackets::AutoEquipItemRequest request;

    CHECK(MopItemPackets::ParseAutoEquipItem(packet, request));
    CHECK(request.cursorSlot == 0x21);
    CHECK(request.cursorBag == 0x31);
    CHECK(request.update.bag == 0x41);
    CHECK(request.update.slot == 0x22);
    CHECK(packet.rpos() == packet.size());
}

static void TestSplitItem()
{
    WorldPacket packet = Packet(CMSG_SPLIT_ITEM,
        { 0x31, 0x05, 0x00, 0x00, 0x00, 0x41, 0x21, 0x22, 0x00 });
    MopItemPackets::SplitItemRequest request;

    CHECK(MopItemPackets::ParseSplitItem(packet, request));
    CHECK(request.sourceBag == 0x31);
    CHECK(request.count == 5);
    CHECK(request.destinationBag == 0x41);
    CHECK(request.sourceSlot == 0x21);
    CHECK(request.destinationSlot == 0x22);
    CHECK(packet.rpos() == packet.size());
}

static void TestDestroyItem()
{
    WorldPacket packet = Packet(CMSG_DESTROY_ITEM,
        { 0x00, 0x00, 0x00, 0x00, 0x1A, 0xFF });
    MopItemPackets::DestroyItemRequest request;

    CHECK(MopItemPackets::ParseDestroyItem(packet, request));
    CHECK(request.count == 0);
    CHECK(request.slot == 0x1A);
    CHECK(request.bag == 0xFF);
    CHECK(packet.rpos() == packet.size());
}

static void TestInventoryChangeFailureMinimumBody()
{
    MopItemPackets::InventoryChangeFailure failure;
    WorldPacket packet;

    MopItemPackets::BuildInventoryChangeFailure(packet, failure);
    CheckPacket(packet, SMSG_INVENTORY_CHANGE_FAILURE,
        { 0x00, 0x00, 0x00, 0x00 });
}

int main(int /*argc*/, char** /*argv*/)
{
    TestSwapInvItemDense();
    TestAutoEquipItemDense();
    TestSplitItem();
    TestDestroyItem();
    TestInventoryChangeFailureMinimumBody();
    return g_fail ? 1 : 0;
}
