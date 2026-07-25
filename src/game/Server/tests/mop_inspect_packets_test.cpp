/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for the 5.4.8 inspect request and result bodies.
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool BytesEqual(WorldPacket const& packet,
    std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;

    return true;
}

static WorldPacket MakePacket(std::vector<uint8> const& body)
{
    WorldPacket packet(CMSG_INSPECT, body.size());
    if (!body.empty())
        packet.append(body.data(), body.size());
    return packet;
}

static void TestInspectRequest()
{
    std::vector<uint8> const denseBody = {
        0xFF, 0x05, 0x07, 0x02, 0x04, 0x03, 0x06, 0x00, 0x09
    };
    WorldPacket dense = MakePacket(denseBody);
    ObjectGuid guid;
    CHECK(MopInspectPackets::ParseRequest(dense, guid));
    CHECK(guid == ObjectGuid(UINT64_C(0x0807060504030201)));
    CHECK(dense.rpos() == dense.size());

    WorldPacket sparse = MakePacket({ 0x93, 0x02, 0x04, 0x06, 0x00 });
    CHECK(MopInspectPackets::ParseRequest(sparse, guid));
    CHECK(guid == ObjectGuid(UINT64_C(0x0007000500030001)));

    WorldPacket empty = MakePacket({ 0x00 });
    CHECK(MopInspectPackets::ParseRequest(empty, guid));
    CHECK(guid.IsEmpty());

    for (size_t length = 0; length < denseBody.size(); ++length)
    {
        std::vector<uint8> truncatedBody(denseBody.begin(),
            denseBody.begin() + length);
        WorldPacket truncated = MakePacket(truncatedBody);
        CHECK(!MopInspectPackets::ParseRequest(truncated, guid));
        CHECK(truncated.rpos() == truncated.size());
    }

    std::vector<uint8> trailingBody = denseBody;
    trailingBody.push_back(0);
    WorldPacket trailing = MakePacket(trailingBody);
    CHECK(!MopInspectPackets::ParseRequest(trailing, guid));
    CHECK(trailing.rpos() == trailing.size());

    std::vector<uint8> noncanonicalBody = denseBody;
    noncanonicalBody[1] = 1; // a present GUID byte must not decode to zero
    WorldPacket noncanonical = MakePacket(noncanonicalBody);
    CHECK(!MopInspectPackets::ParseRequest(noncanonical, guid));
    CHECK(noncanonical.rpos() == noncanonical.size());
}

static void TestEmptyInspectResult()
{
    MopInspectPackets::Response response;
    WorldPacket packet;
    CHECK(MopInspectPackets::BuildResponse(packet, response));
    CHECK(packet.GetOpcode() == SMSG_INSPECT_RESULTS);
    CHECK(BytesEqual(packet, std::vector<uint8>(14, 0)));
}

static MopInspectPackets::Response DenseInspectResponse()
{
    MopInspectPackets::Response response;
    response.targetGuid = ObjectGuid(UINT64_C(0x0807060504030201));
    response.hasGuild = true;
    response.guild.guid = ObjectGuid(UINT64_C(0x1817161514131211));
    response.guild.memberCount = 0x01020304;
    response.guild.experience = UINT64_C(0x0102030405060708);
    response.guild.level = 0x11223344;
    response.glyphs = { 0x1234, 0x0000, 0x5678 };
    response.specializationId = 0x10203040;
    response.talents = { 0x1111, 0x2222 };

    MopInspectPackets::Item item;
    item.creatorGuid = ObjectGuid(UINT64_C(0x2827262524232221));
    item.randomPropertyId = -0x1234;
    item.suffixFactor = 0xA1B2C3D4;
    item.dynamicModifiers = { 0xAA, 0xBB };
    item.enchantments.push_back({ 0x11223344, 2 });
    item.enchantments.push_back({ 0x55667788, 14 });
    item.entry = 0x99AABBCC;
    item.slot = 7;
    response.items.push_back(item);
    return response;
}

static void TestDenseInspectResult()
{
    MopInspectPackets::Response response = DenseInspectResponse();
    WorldPacket packet;
    CHECK(MopInspectPackets::BuildResponse(packet, response));
    CHECK(BytesEqual(packet, {
        0xFF, 0xFC, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x2F,
        0xE0, 0x00, 0x00, 0xC0, 0x00, 0x01, 0x60, 0x03,
        0x04, 0x02, 0xCC, 0xED, 0x25, 0x02, 0x00, 0x00,
        0x00, 0xAA, 0xBB, 0x44, 0x33, 0x22, 0x11, 0x02,
        0x88, 0x77, 0x66, 0x55, 0x0E, 0xCC, 0xBB, 0xAA,
        0x99, 0x26, 0x24, 0x29, 0x22, 0xD4, 0xC3, 0xB2,
        0xA1, 0x27, 0x07, 0x20, 0x23, 0x16, 0x12, 0x17,
        0x10, 0x04, 0x03, 0x02, 0x01, 0x14, 0x19, 0x08,
        0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x13,
        0x44, 0x33, 0x22, 0x11, 0x15, 0x07, 0x34, 0x12,
        0x00, 0x00, 0x78, 0x56, 0x00, 0x40, 0x30, 0x20,
        0x10, 0x11, 0x11, 0x22, 0x22, 0x09, 0x05, 0x06
    }));
}

static void TestInspectResultBounds()
{
    MopInspectPackets::Response response = DenseInspectResponse();

    response.items[0].slot = EQUIPMENT_SLOT_END;
    WorldPacket packet;
    CHECK(!MopInspectPackets::BuildResponse(packet, response));

    response = DenseInspectResponse();
    response.items.push_back(response.items[0]);
    CHECK(!MopInspectPackets::BuildResponse(packet, response));

    response = DenseInspectResponse();
    response.items[0].enchantments[0].slot = MAX_ENCHANTMENT_SLOT;
    CHECK(!MopInspectPackets::BuildResponse(packet, response));

    response = DenseInspectResponse();
    response.glyphs.resize(MAX_GLYPH_SLOT_INDEX + 1);
    CHECK(!MopInspectPackets::BuildResponse(packet, response));
}

static void TestOpcodeValues()
{
    CHECK(uint32(CMSG_INSPECT) == 0x1259u);
    CHECK(uint32(SMSG_INSPECT_RESULTS) == 0x1842u);
    CHECK(uint32(CMSG_INSPECT) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_INSPECT_RESULTS) < uint32(OPCODE_TABLE_SIZE));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestInspectRequest();
    TestEmptyInspectResult();
    TestDenseInspectResult();
    TestInspectResultBounds();
    TestOpcodeValues();
    return g_fail ? 1 : 0;
}
