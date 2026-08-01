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

static uint64_t ReadSingleEmptyPlayerGuid(WorldPacket const& packet)
{
    PacketCursor cursor(packet);
    CHECK(cursor.ReadBits(18) == 1);
    CHECK(cursor.ReadBits(1) == 0);
    CHECK(cursor.ReadBits(8) == 0);
    CHECK(cursor.ReadBits(13) == 0);
    CHECK(cursor.ReadBits(1) == 0);
    CHECK(cursor.ReadBits(1) == 0);
    CHECK(cursor.ReadBits(17) == 0);
    CHECK(cursor.ReadBits(1) == 1);

    std::array<bool, 8> present = {};
    for (uint8_t index : { 2, 6, 7, 0, 5, 3, 1, 4 })
        present[index] = cursor.ReadBits(1) != 0;
    cursor.AlignBytes();
    CHECK(cursor.ReadU32() == 0);

    std::array<uint8_t, 8> bytes = {};
    for (uint8_t index : { 4, 0, 5, 3, 1, 7, 2, 6 })
        if (present[index])
            bytes[index] = cursor.ReadByte() ^ 1;

    uint64_t guid = 0;
    std::memcpy(&guid, bytes.data(), sizeof(guid));
    return guid;
}

static void test_empty_mailbox_captured_body()
{
    WorldPacket packet(SMSG_MAIL_LIST_RESULT, 7);
    CHECK(MopMailPackets::BuildList(packet, 0, {}));
    CHECK(packet.GetOpcode() == SMSG_MAIL_LIST_RESULT);

    // Captured retail body: capture-000059 sequence 188204, build 18414,
    // SMSG 0x1C0B. It proves only the empty header/count representation.
    CHECK(ExpectBytes(packet,
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }));
}

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

static void test_creature_mail_captured_body()
{
    MopMailPackets::MailRecord mail;
    mail.senderIsNotPlayer = true;
    mail.subject = "In The Name Of The Light!";
    mail.body = "To: High General Abbendis\r\nNew Avalon, Scarlet Lands\r\n";
    mail.messageId = 0x4063A14Au;
    mail.mailTemplateId = 0xECu;
    mail.stationery = 41;
    mail.daysLeft = FloatFromBits(0x41EC5A13u);
    mail.checkedFlags = 21;
    mail.senderEntry = 0x7102u;
    mail.messageType = 3;

    WorldPacket packet(SMSG_MAIL_LIST_RESULT, 136);
    CHECK(MopMailPackets::BuildList(packet, 1, { mail }));

    // Captured retail body: capture-000657 sequence 235002, build 18414,
    // SMSG 0x1C0B. It proves this concrete no-GUID/no-item creature form.
    CHECK(ExpectBytes(packet, {
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x63, 0x20,
        0x36, 0x00, 0x00, 0x00, 0x54, 0x6F, 0x3A, 0x20,
        0x48, 0x69, 0x67, 0x68, 0x20, 0x47, 0x65, 0x6E,
        0x65, 0x72, 0x61, 0x6C, 0x20, 0x41, 0x62, 0x62,
        0x65, 0x6E, 0x64, 0x69, 0x73, 0x0D, 0x0A, 0x4E,
        0x65, 0x77, 0x20, 0x41, 0x76, 0x61, 0x6C, 0x6F,
        0x6E, 0x2C, 0x20, 0x53, 0x63, 0x61, 0x72, 0x6C,
        0x65, 0x74, 0x20, 0x4C, 0x61, 0x6E, 0x64, 0x73,
        0x0D, 0x0A, 0x4A, 0xA1, 0x63, 0x40, 0xEC, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x49, 0x6E, 0x20, 0x54, 0x68, 0x65,
        0x20, 0x4E, 0x61, 0x6D, 0x65, 0x20, 0x4F, 0x66,
        0x20, 0x54, 0x68, 0x65, 0x20, 0x4C, 0x69, 0x67,
        0x68, 0x74, 0x21, 0x29, 0x00, 0x00, 0x00, 0x13,
        0x5A, 0xEC, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x02,
        0x71, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00
    }));
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

static void test_guid_mask_and_byte_discrimination()
{
    uint64_t const allBytes = UINT64_C(0x8877665544332211);
    std::array<uint64_t, 9> guids = { allBytes };
    for (size_t zeroByte = 0; zeroByte < 8; ++zeroByte)
        guids[zeroByte + 1] = allBytes & ~(UINT64_C(0xFF) << (zeroByte * 8));

    for (size_t i = 0; i < guids.size(); ++i)
    {
        MopMailPackets::MailRecord mail;
        mail.senderGuid = guids[i];
        WorldPacket packet(SMSG_MAIL_LIST_RESULT, 64);
        CHECK(MopMailPackets::BuildList(packet, 1, { mail }));
        CHECK(ReadSingleEmptyPlayerGuid(packet) == guids[i]);
        CHECK(packet.size() == (i == 0 ? 62 : 61));
    }
}

static void test_optional_gates_are_independent()
{
    for (unsigned combination = 0; combination < 4; ++combination)
    {
        MopMailPackets::MailRecord mail;
        mail.hasOptionalA = (combination & 1) != 0;
        mail.hasOptionalB = (combination & 2) != 0;
        mail.optionalA = 0x11223344u;
        mail.optionalB = 0x55667788u;

        WorldPacket packet(SMSG_MAIL_LIST_RESULT, 64);
        CHECK(MopMailPackets::BuildList(packet, 1, { mail }));

        size_t tail = packet.size();
        if (mail.hasOptionalB)
        {
            tail -= 4;
            uint32_t value = 0;
            std::memcpy(&value, packet.contents() + tail, sizeof(value));
            CHECK(value == mail.optionalB);
        }
        if (mail.hasOptionalA)
        {
            tail -= 4;
            uint32_t value = 0;
            std::memcpy(&value, packet.contents() + tail, sizeof(value));
            CHECK(value == mail.optionalA);
        }
    }
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

static void test_item_layout_and_two_pass_order()
{
    MopMailPackets::ItemRecord item;
    item.guidLow = 0x04030201u;
    item.modifierBlob = {
        0x38, 0x00, 0x00, 0x00, 0x84, 0x04, 0x00, 0x00,
        0x12, 0x00, 0x00, 0x03, 0x01, 0x00, 0x00, 0x00
    };
    item.durability = 0x08070605u;
    item.unknown = 0x0C0B0A09u;
    for (uint32_t i = 0; i < item.enchants.size(); ++i)
    {
        item.enchants[i].fieldAtPlus8 = 0x100u + i;
        item.enchants[i].fieldAtPlus4 = 0x200u + i;
        item.enchants[i].fieldAtPlus0 = 0x300u + i;
    }
    item.randomPropertyId = 0x11121314;
    item.spellCharges = 0x21222324;
    item.maxDurability = 0x31323334u;
    item.stackCount = 0x41424344u;
    item.index = 5;
    item.entry = 0x51525354u;

    MopMailPackets::MailRecord first;
    first.body = "A";
    first.items = { item };
    MopMailPackets::MailRecord second;
    second.body = "B";

    WorldPacket packet(SMSG_MAIL_LIST_RESULT, 256);
    CHECK(MopMailPackets::BuildList(packet, 2, { first, second }));

    PacketCursor cursor(packet);
    CHECK(cursor.ReadBits(18) == 2);
    for (unsigned mail = 0; mail < 2; ++mail)
    {
        CHECK(cursor.ReadBits(1) == 0);
        CHECK(cursor.ReadBits(8) == 0);
        CHECK(cursor.ReadBits(13) == 1);
        CHECK(cursor.ReadBits(1) == 0);
        CHECK(cursor.ReadBits(1) == 0);
        CHECK(cursor.ReadBits(17) == (mail == 0 ? 1 : 0));
        CHECK(cursor.ReadBits(1) == 0);
        if (mail == 0)
            CHECK(cursor.ReadBits(1) == 0);
    }
    cursor.AlignBytes();
    CHECK(cursor.ReadU32() == item.guidLow);
    CHECK(cursor.ReadU32() == item.modifierBlob.size());
    for (uint8_t byte : item.modifierBlob)
        CHECK(cursor.ReadByte() == byte);
    CHECK(cursor.ReadU32() == item.durability);
    CHECK(cursor.ReadU32() == item.unknown);
    for (MopMailPackets::EnchantGroup const& enchant : item.enchants)
    {
        CHECK(cursor.ReadU32() == enchant.fieldAtPlus8);
        CHECK(cursor.ReadU32() == enchant.fieldAtPlus4);
        CHECK(cursor.ReadU32() == enchant.fieldAtPlus0);
    }
    CHECK(cursor.ReadU32() == uint32_t(item.randomPropertyId));
    CHECK(cursor.ReadU32() == uint32_t(item.spellCharges));
    CHECK(cursor.ReadU32() == item.maxDurability);
    CHECK(cursor.ReadU32() == item.stackCount);
    CHECK(cursor.ReadByte() == item.index);
    CHECK(cursor.ReadU32() == item.entry);
    auto readEmptyMail = [&cursor](uint8_t expectedBody)
    {
        CHECK(cursor.ReadByte() == expectedBody);
        CHECK(cursor.ReadU32() == 0); // message ID
        CHECK(cursor.ReadU32() == 0); // template ID
        CHECK(cursor.ReadU32() == 0); // COD low
        CHECK(cursor.ReadU32() == 0); // COD high
        CHECK(cursor.ReadU32() == 0); // stationery
        CHECK(cursor.ReadU32() == 0); // days-left float bits
        CHECK(cursor.ReadU32() == 0); // money low
        CHECK(cursor.ReadU32() == 0); // money high
        CHECK(cursor.ReadU32() == 0); // checked flags
        CHECK(cursor.ReadByte() == 0); // message type
        CHECK(cursor.ReadU32() == 0); // unknown
    };
    readEmptyMail('A');
    readEmptyMail('B');
    CHECK(cursor.bytePosition == packet.size());
}

static void test_bounds_and_large_body()
{
    CHECK(MopMailPackets::MAX_SUBJECT_BYTES == 255);
    CHECK(MopMailPackets::MAX_BODY_BYTES == 7999);
    CHECK(MopMailPackets::MAX_MAIL_COUNT == 50);
    CHECK(MopMailPackets::MAX_POST_CRYPT_PAYLOAD_BYTES == 0x7FFFF);

    MopMailPackets::MailRecord mail;
    mail.subject.assign(MopMailPackets::MAX_SUBJECT_BYTES, 's');
    mail.body.assign(MopMailPackets::MAX_BODY_BYTES, 'b');
    WorldPacket packet;
    CHECK(MopMailPackets::BuildList(packet, 1, { mail }));

    mail.subject.push_back('s');
    CHECK(!MopMailPackets::BuildList(packet, 1, { mail }));
    mail.subject.resize(MopMailPackets::MAX_SUBJECT_BYTES);
    mail.body.push_back('b');
    CHECK(!MopMailPackets::BuildList(packet, 1, { mail }));

    mail.body.clear();
    mail.items.resize(MAX_MAIL_ITEMS);
    CHECK(MopMailPackets::BuildList(packet, 1, { mail }));
    mail.items.push_back({});
    CHECK(!MopMailPackets::BuildList(packet, 1, { mail }));

    std::vector<MopMailPackets::MailRecord> mails(
        MopMailPackets::MAX_MAIL_COUNT);
    CHECK(MopMailPackets::BuildList(packet, uint32_t(mails.size()), mails));
    mails.push_back({});
    CHECK(!MopMailPackets::BuildList(packet, uint32_t(mails.size()), mails));

    MopMailPackets::MailRecord large;
    large.body.assign(MopMailPackets::MAX_BODY_BYTES, 'x');
    mails.assign(5, large);
    CHECK(MopMailPackets::BuildList(packet, 5, mails));
    CHECK(packet.size() > 32767);

    MopMailPackets::MailRecord oversized;
    oversized.items.resize(1);
    oversized.items[0].modifierBlob.resize(
        MopMailPackets::MAX_POST_CRYPT_PAYLOAD_BYTES);
    CHECK(!MopMailPackets::BuildList(packet, 1, { oversized }));
}

static void test_utf8_truncation_keeps_complete_codepoints()
{
    std::string const value = "ab\xE2\x82\xACz";
    CHECK(MopMailPackets::TruncateUtf8(value, 6) == value);
    CHECK(MopMailPackets::TruncateUtf8(value, 5) == "ab\xE2\x82\xAC");
    CHECK(MopMailPackets::TruncateUtf8(value, 4) == "ab");
    CHECK(MopMailPackets::TruncateUtf8(value, 3) == "ab");
    CHECK(MopMailPackets::TruncateUtf8(value, 2) == "ab");
}

int main()
{
    test_empty_mailbox_captured_body();
    test_player_sender_guid_domain();
    test_creature_mail_captured_body();
    test_player_attachment_captured_body();
    test_guid_mask_and_byte_discrimination();
    test_optional_gates_are_independent();
    test_money_and_cod_preserve_high_halves();
    test_item_layout_and_two_pass_order();
    test_bounds_and_large_body();
    test_utf8_truncation_keeps_complete_codepoints();
    return g_fail == 0 ? 0 : 1;
}
