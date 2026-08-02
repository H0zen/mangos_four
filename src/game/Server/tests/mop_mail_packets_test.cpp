/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server of World of Warcraft.
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for the 5.4.8 mail-list packet body.
 */

#include "MopMailPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;

    return true;
}

struct PacketCursor
{
    explicit PacketCursor(WorldPacket const& source) : packet(source) {}

    uint32_t ReadBits(unsigned count)
    {
        uint32_t value = 0;
        for (unsigned i = 0; i < count; ++i)
        {
            size_t const byte = bitPosition / 8;
            unsigned const shift = 7 - unsigned(bitPosition % 8);
            value = (value << 1) | ((packet.contents()[byte] >> shift) & 1);
            ++bitPosition;
        }
        return value;
    }

    void AlignBytes()
    {
        bytePosition = (bitPosition + 7) / 8;
    }

    uint8_t ReadByte()
    {
        return packet.contents()[bytePosition++];
    }

    uint32_t ReadU32()
    {
        uint32_t value = 0;
        std::memcpy(&value, packet.contents() + bytePosition, sizeof(value));
        bytePosition += sizeof(value);
        return value;
    }

    WorldPacket const& packet;
    size_t bitPosition = 32;
    size_t bytePosition = 4;
};

static float FloatFromBits(uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static void test_player_sender_guid_domain()
{
    // The mail store retains only the LowGUID. Build 18414 identifies players
    // on the wire with the 0x040 domain used by the captured sender below.
    CHECK(MopMailPackets::BuildPlayerSenderGuid(0x053CC8E8u) ==
        UINT64_C(0x04000000053CC8E8));
    CHECK(MopMailPackets::BuildPlayerSenderGuid(0) == 0);
}

static void test_player_attachment_captured_body()
{
    MopMailPackets::ItemRecord item;
    item.guidLow = 0x347D0F90u;
    item.modifierBlob = { 0x00, 0x00, 0x00, 0x00 };
    item.unknown = 0x17729200u;
    item.stackCount = 4;
    item.entry = 0x1296Cu;

    MopMailPackets::MailRecord mail;
    mail.subject = "Vermilion Onyx (4)";
    mail.hasOptionalA = true;
    mail.hasOptionalB = true;
    mail.optionalA = 0x0304000Du;
    mail.optionalB = 0x03010018u;
    mail.items = { item };
    mail.senderGuid = UINT64_C(0x04000000053CC8E8);
    mail.messageId = 0x52E10DEBu;
    mail.stationery = 41;
    mail.daysLeft = FloatFromBits(0x41F7FEDDu);

    WorldPacket packet(SMSG_MAIL_LIST_RESULT, 222);
    CHECK(MopMailPackets::BuildList(packet, 1, { mail }));

    // Captured retail body: capture-000360 sequence 32325, build 18414,
    // SMSG 0x1C0B. It proves this specific GUID, optional and item form.
    CHECK(ExpectBytes(packet, {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x40,
        0x00, 0xC0, 0x00, 0x3B, 0x60, 0x90, 0x0F, 0x7D,
        0x34, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x92, 0x72,
        0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x00, 0x00, 0x6C, 0x29, 0x01, 0x00, 0xEB, 0x0D,
        0xE1, 0x52, 0xE9, 0x04, 0xC9, 0x05, 0x3D, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x56, 0x65, 0x72, 0x6D, 0x69,
        0x6C, 0x69, 0x6F, 0x6E, 0x20, 0x4F, 0x6E, 0x79,
        0x78, 0x20, 0x28, 0x34, 0x29, 0x29, 0x00, 0x00,
        0x00, 0xDD, 0xFE, 0xF7, 0x41, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00,
        0x04, 0x03, 0x18, 0x00, 0x01, 0x03
    }));
}

static void test_money_and_cod_preserve_high_halves()
{
    MopMailPackets::MailRecord mail;
    mail.cod = UINT64_C(0x1122334455667788);
    mail.money = UINT64_C(0x8877665544332211);

    WorldPacket packet(SMSG_MAIL_LIST_RESULT, 64);
    CHECK(MopMailPackets::BuildList(packet, 1, { mail }));

    PacketCursor cursor(packet);
    CHECK(cursor.ReadBits(18) == 1);
    CHECK(cursor.ReadBits(1) == 0);  // player sender
    CHECK(cursor.ReadBits(8) == 0);  // subject length
    CHECK(cursor.ReadBits(13) == 0); // body length
    CHECK(cursor.ReadBits(1) == 0);  // optional A
    CHECK(cursor.ReadBits(1) == 0);  // optional B
    CHECK(cursor.ReadBits(17) == 0); // items
    CHECK(cursor.ReadBits(1) == 0);  // sender GUID absent
    cursor.AlignBytes();

    CHECK(cursor.ReadU32() == 0);          // message ID
    CHECK(cursor.ReadU32() == 0);          // template ID
    CHECK(cursor.ReadU32() == 0x55667788); // COD low
    CHECK(cursor.ReadU32() == 0x11223344); // COD high
    CHECK(cursor.ReadU32() == 0);          // stationery
    CHECK(cursor.ReadU32() == 0);          // days-left bits
    CHECK(cursor.ReadU32() == 0x44332211); // money low
    CHECK(cursor.ReadU32() == 0x88776655); // money high
    CHECK(cursor.ReadU32() == 0);          // checked
    CHECK(cursor.ReadByte() == 0);         // message type
    CHECK(cursor.ReadU32() == 0);          // unknown
    CHECK(cursor.bytePosition == packet.size());
}

int main()
{
    test_player_sender_guid_domain();
    test_player_attachment_captured_body();
    test_money_and_cod_preserve_high_halves();
    return g_fail == 0 ? 0 : 1;
}
