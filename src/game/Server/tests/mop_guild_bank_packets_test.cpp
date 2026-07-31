/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server of World of Warcraft.
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for the 5.4.8 guild-bank-list packet body.
 */

#include "MopGuildBankPackets.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(ByteBuffer const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "size mismatch: got %zu expected %zu\n",
            packet.size(), expected.size());
        for (size_t i = 0; i < packet.size(); ++i)
        {
            std::fprintf(stderr, "%02X%s", packet.contents()[i],
                i + 1 == packet.size() ? "\n" : " ");
        }
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "byte %zu: got %02X expected %02X\n", i,
                packet.contents()[i], expected[i]);
            return false;
        }
    }

    return true;
}

static MopGuildBankPackets::GuildBankList MakeList(uint32 tabId,
    uint64 money, int32 withdrawRemaining)
{
    MopGuildBankPackets::GuildBankList list;
    list.tabId = tabId;
    list.money = money;
    list.withdrawRemaining = withdrawRemaining;
    return list;
}

static MopGuildBankPackets::ItemRecord MakePresent(uint32 slotId,
    uint32 entry, uint32 dynamicFlags, uint32 spellCharges, uint32 stackCount)
{
    MopGuildBankPackets::ItemRecord item;
    item.present = true;
    item.slotId = slotId;
    item.entry = entry;
    item.dynamicFlags = dynamicFlags;
    item.spellCharges = spellCharges;
    item.stackCount = stackCount;
    return item;
}

static void test_captured_empty_body()
{
    MopGuildBankPackets::GuildBankList list = MakeList(0, 357406960, 4);
    ByteBuffer body;
    CHECK(MopGuildBankPackets::BuildListBody(body, list));
    CHECK(ExpectBytes(body, {
        0x00, 0x00, 0x00, 0x00, 0xF0, 0x98, 0x4D, 0x15,
        0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00
    }));
}

static void test_captured_absent_slot_body()
{
    MopGuildBankPackets::GuildBankList list = MakeList(3, 210710453, 0);
    MopGuildBankPackets::ItemRecord item;
    item.slotId = 39;
    list.items.push_back(item);

    ByteBuffer body;
    CHECK(MopGuildBankPackets::BuildListBody(body, list));
    CHECK(ExpectBytes(body, {
        0x03, 0x00, 0x00, 0x00, 0xB5, 0x2F, 0x8F, 0x0C,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }));
}

static void test_captured_present_item_body()
{
    MopGuildBankPackets::GuildBankList list = MakeList(2, 211105617, 2);
    list.items.push_back(MakePresent(97, 76090, 196640, 1, 20));

    ByteBuffer body;
    CHECK(MopGuildBankPackets::BuildListBody(body, list));
    CHECK(ExpectBytes(body, {
        0x02, 0x00, 0x00, 0x00, 0x51, 0x37, 0x95, 0x0C,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x20, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x3A, 0x29, 0x01, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x61, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));
}

static void test_captured_mixed_delta_body()
{
    MopGuildBankPackets::GuildBankList list = MakeList(2, 878571537, 2);
    MopGuildBankPackets::ItemRecord absent;
    absent.slotId = 3;
    list.items.push_back(absent);
    list.items.push_back(MakePresent(92, 74723, 196608, 1, 1));

    ByteBuffer body;
    CHECK(MopGuildBankPackets::BuildListBody(body, list));
    CHECK(ExpectBytes(body, {
        0x02, 0x00, 0x00, 0x00, 0x11, 0xF0, 0x5D, 0x34,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE3,
        0x23, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x5C, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    }));
}

static void test_binary_derived_synthetic_tab_body()
{
    // Independently encoded from the 18414 client reader: this is synthetic
    // grammar coverage, not a captured retail body. The literal distinguishes
    // the 9-bit icon length from the following 7-bit name length and pins the
    // byte section as index, icon, then name. Retail capture-000601 seq 30404
    // corroborates that shape with seven tabs, but its 39 persisted item
    // modifier blobs cannot be reconstructed by the current backend.
    MopGuildBankPackets::GuildBankList list = MakeList(
        3, UINT64_C(0x0807060504030201), INT32_C(0x0C0B0A09));
    list.fullUpdate = true;
    MopGuildBankPackets::TabRecord tab;
    tab.index = 6;
    tab.icon = "IcoN!";
    tab.name = "Tab";
    list.tabs.push_back(tab);

    ByteBuffer body;
    CHECK(MopGuildBankPackets::BuildListBody(body, list));
    CHECK(ExpectBytes(body, {
        0x03, 0x00, 0x00, 0x00,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C,
        0x80, 0x00, 0x04, 0x00, 0x00,
        0x02, 0x83,
        0x06, 0x00, 0x00, 0x00,
        0x49, 0x63, 0x6F, 0x4E, 0x21,
        0x54, 0x61, 0x62
    }));
}

static uint32_t ReadU32(ByteBuffer const& body, size_t offset)
{
    uint32_t value = 0;
    std::memcpy(&value, body.contents() + offset, sizeof(value));
    return value;
}

static void test_socket_pair_order_and_bounds()
{
    MopGuildBankPackets::GuildBankList list = MakeList(0, 0, 0);
    MopGuildBankPackets::ItemRecord item = MakePresent(0, 1, 0, 0, 1);
    item.socketEnchants = { { 0, 101 }, { 1, 202 }, { 2, 303 } };
    list.items.push_back(item);

    ByteBuffer body;
    CHECK(MopGuildBankPackets::BuildListBody(body, list));
    CHECK(ReadU32(body, 32) == 0);
    CHECK(ReadU32(body, 36) == 101);
    CHECK(ReadU32(body, 40) == 1);
    CHECK(ReadU32(body, 44) == 202);
    CHECK(ReadU32(body, 48) == 2);
    CHECK(ReadU32(body, 52) == 303);

    list.items[0].socketEnchants.push_back({ 0, 404 });
    CHECK(!MopGuildBankPackets::BuildListBody(body, list));
    list.items[0].socketEnchants = { { 3, 101 } };
    CHECK(!MopGuildBankPackets::BuildListBody(body, list));
    list.items[0].socketEnchants = { { 1, 101 }, { 1, 202 } };
    CHECK(!MopGuildBankPackets::BuildListBody(body, list));
}

static void test_tab_and_item_bounds()
{
    MopGuildBankPackets::GuildBankList list = MakeList(7, 0, 0);
    list.fullUpdate = true;
    for (uint32 i = 0; i < MopGuildBankPackets::MAX_TAB_COUNT; ++i)
    {
        MopGuildBankPackets::TabRecord tab;
        tab.index = i;
        tab.name = std::string(MopGuildBankPackets::MAX_TAB_NAME_BYTES, 'n');
        tab.icon = std::string(MopGuildBankPackets::MAX_TAB_ICON_BYTES, 'i');
        list.tabs.push_back(tab);
    }
    for (uint32 i = 0; i < MopGuildBankPackets::MAX_ITEM_COUNT; ++i)
    {
        MopGuildBankPackets::ItemRecord item;
        item.slotId = i;
        list.items.push_back(item);
    }

    ByteBuffer body;
    CHECK(MopGuildBankPackets::BuildListBody(body, list));

    MopGuildBankPackets::GuildBankList invalid = list;
    invalid.tabId = 8;
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
    invalid = list;
    invalid.tabs.push_back({});
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
    invalid = list;
    invalid.items.push_back({});
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
    invalid = list;
    invalid.items[0].slotId = 98;
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
    invalid = list;
    invalid.items[1].slotId = 0;
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
    invalid = list;
    invalid.tabs[0].name.push_back('x');
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
    invalid = list;
    invalid.tabs[0].icon.push_back('x');
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
    invalid = list;
    invalid.fullUpdate = false;
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
}

static void test_utf8_truncation_and_atomic_failure()
{
    std::string const value = std::string(63, 'a') + "\xC3\xA9";
    CHECK(MopGuildBankPackets::TruncateUtf8(value, 64) == std::string(63, 'a'));

    MopGuildBankPackets::GuildBankList invalid = MakeList(8, 0, 0);
    ByteBuffer body;
    body << uint8(0xA5);
    CHECK(!MopGuildBankPackets::BuildListBody(body, invalid));
    CHECK(body.size() == 1);
    CHECK(body.contents()[0] == 0xA5);
    CHECK(MopGuildBankPackets::MAX_POST_CRYPT_PAYLOAD_BYTES == 0x7FFFF);
}

int main()
{
    test_captured_empty_body();
    test_captured_absent_slot_body();
    test_captured_present_item_body();
    test_captured_mixed_delta_body();
    test_binary_derived_synthetic_tab_body();
    test_socket_pair_order_and_bounds();
    test_tab_and_item_bounds();
    test_utf8_truncation_and_atomic_failure();

    if (g_fail)
    {
        return 1;
    }
    std::puts("mop_guild_bank_packets: all checks passed");
    return 0;
}
