
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
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool BytesEqual(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;

    return true;
}

static void TestVendorList()
{
    MopItemPackets::VendorList list;
    list.vendorGuid = ObjectGuid(UINT64_C(0x0007060004030001));
    list.reason = 2;

    MopItemPackets::VendorItemRecord item;
    item.leftInStock = -1;
    item.price = 0x01020304;
    item.type = 1;
    item.maxDurability = 0x11121314;
    item.displayId = 0x21222324;
    item.buyCount = 0x31323334;
    item.itemId = 0x41424344;
    item.hasExtendedCost = true;
    item.extendedCost = 0x51525354;
    item.upgradeId = 0x61626364;
    item.hasCondition = true;
    item.condition = 0x71727374;
    item.slot = 0x81828384;
    item.doNotFilter = true;
    list.items.push_back(item);

    WorldPacket packet;
    MopItemPackets::BuildVendorList(packet, list);

    CHECK(packet.GetOpcode() == SMSG_LIST_INVENTORY);
    CHECK(BytesEqual(packet, {
        0x98, 0x00, 0x03, 0x18, 0x02,
        0xFF, 0xFF, 0xFF, 0xFF,
        0x04, 0x03, 0x02, 0x01,
        0x01, 0x00, 0x00, 0x00,
        0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21,
        0x34, 0x33, 0x32, 0x31,
        0x44, 0x43, 0x42, 0x41,
        0x54, 0x53, 0x52, 0x51,
        0x64, 0x63, 0x62, 0x61,
        0x74, 0x73, 0x72, 0x71,
        0x84, 0x83, 0x82, 0x81,
        0x05, 0x00, 0x06, 0x02, 0x07
    }));
}

static void TestSellItemRequest()
{
    WorldPacket packet(CMSG_SELL_ITEM, 14);
    uint8 const body[] = {
        0x00, 0x00, 0x00, 0x00, 0x34, 0x5D, 0x31,
        0xB7, 0x03, 0x46, 0xF0, 0xE1, 0x99, 0x1C
    };
    packet.append(body, sizeof(body));

    MopItemPackets::SellItemRequest request;
    CHECK(MopItemPackets::ParseSellItem(packet, request));
    CHECK(request.count == 0);
    CHECK(request.itemGuid == ObjectGuid(UINT64_C(0x470000000000001D)));
    CHECK(request.vendorGuid == ObjectGuid(UINT64_C(0xF13000980002B6E0)));
}

static void TestBuyItemRequest()
{
    WorldPacket packet(CMSG_BUY_ITEM, 35);
    uint8 const body[] = {
        0x1A, 0x00, 0x00, 0x00,
        0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21,
        0x34, 0x33, 0x32, 0x31,
        0xFB, 0xFF, 0xC0,
        0x07, 0x00, 0x15, 0x13, 0x16, 0x02, 0x09, 0x06,
        0x10, 0x17, 0x04, 0x12, 0x05, 0x19, 0x03, 0x14
    };
    packet.append(body, sizeof(body));

    MopItemPackets::BuyItemRequest request;
    CHECK(MopItemPackets::ParseBuyItem(packet, request));
    CHECK(request.destinationBagSlot == 0x1A);
    CHECK(request.count == 0x11121314);
    CHECK(request.itemId == 0x21222324);
    CHECK(request.vendorSlot == 0x31323334);
    CHECK(request.type == 2);
    CHECK(request.vendorGuid == ObjectGuid(UINT64_C(0x0807060504030201)));
    CHECK(request.destinationBagGuid == ObjectGuid(UINT64_C(0x1817161514131211)));
    CHECK(packet.rpos() == packet.size());
}

static void TestBuyItemResult()
{
    WorldPacket packet;
    MopItemPackets::BuildBuyItemResult(packet,
        ObjectGuid(UINT64_C(0x0807060504030201)),
        0x11121314, 0x21222324, 0x31323334);

    CHECK(packet.GetOpcode() == SMSG_BUY_ITEM);
    CHECK(BytesEqual(packet, {
        0xFF, 0x06, 0x09,
        0x14, 0x13, 0x12, 0x11,
        0x03, 0x05, 0x07, 0x02,
        0x24, 0x23, 0x22, 0x21,
        0x00, 0x04,
        0x34, 0x33, 0x32, 0x31
    }));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestVendorList();
    TestSellItemRequest();
    TestBuyItemRequest();
    TestBuyItemResult();
    return g_fail ? 1 : 0;
}
