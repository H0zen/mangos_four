/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for the directly verified 5.4.8 GameObject interaction
 * requests and the type-dependent GameObject animation/page packets.
 */

#include "GameObject.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"

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

static bool ExpectBytes(WorldPacket const& packet,
    std::initializer_list<uint8_t> expected)
{
    if (packet.size() != expected.size())
    {
        return false;
    }

    size_t index = 0;
    for (uint8_t byte : expected)
    {
        if (packet[index++] != byte)
        {
            return false;
        }
    }
    return true;
}

static void TestUseRequest()
{
    WorldPacket dense = MakePacket(CMSG_GAMEOBJ_USE,
        { 0xFF, 0x10, 0x23, 0x76, 0x32, 0x45, 0x54, 0x67, 0x89 });
    uint64 guid = 0;
    CHECK(MopGameObjectPackets::ParseUseRequest(dense, guid));
    CHECK(guid == UINT64_C(0x8877665544332211));

    WorldPacket sparse = MakePacket(CMSG_GAMEOBJ_USE,
        { 0x0A, 0x10, 0x89 });
    CHECK(MopGameObjectPackets::ParseUseRequest(sparse, guid));
    CHECK(guid == UINT64_C(0x8800000000000011));

    WorldPacket truncated = MakePacket(CMSG_GAMEOBJ_USE, { 0xFF, 0x10 });
    CHECK(!MopGameObjectPackets::ParseUseRequest(truncated, guid));
    CHECK(truncated.rpos() == truncated.size());

    WorldPacket trailing = MakePacket(CMSG_GAMEOBJ_USE, { 0x00, 0xAA });
    CHECK(!MopGameObjectPackets::ParseUseRequest(trailing, guid));
    CHECK(trailing.rpos() == trailing.size());
}

static void TestPageTextQueryResponse()
{
    MopQueryPackets::PageTextQueryResponse response;
    response.hasData = true;
    response.pageId = 0x12345678u;
    response.nextPageId = 0x9ABCDEF0u;
    response.text = "Hi";

    WorldPacket packet;
    CHECK(MopQueryPackets::BuildPageTextQueryResponse(packet, response));
    CHECK(packet.GetOpcode() == SMSG_PAGE_TEXT_QUERY_RESPONSE);
    CHECK(ExpectBytes(packet,
        { 0x80, 0x10, 0xF0, 0xDE, 0xBC, 0x9A,
          0x78, 0x56, 0x34, 0x12, 0x48, 0x69,
          0x78, 0x56, 0x34, 0x12 }));

    response.text.clear();
    response.nextPageId = 0;
    CHECK(MopQueryPackets::BuildPageTextQueryResponse(packet, response));
    CHECK(ExpectBytes(packet,
        { 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x78, 0x56, 0x34, 0x12,
          0x78, 0x56, 0x34, 0x12 }));

    response.hasData = false;
    response.text = "ignored";
    response.nextPageId = 0xFFFFFFFFu;
    CHECK(MopQueryPackets::BuildPageTextQueryResponse(packet, response));
    CHECK(ExpectBytes(packet,
        { 0x00, 0x78, 0x56, 0x34, 0x12 }));

    response.hasData = true;
    response.text.assign(4000, 'A');
    CHECK(MopQueryPackets::BuildPageTextQueryResponse(packet, response));
    CHECK(packet.size() == 4014);

    response.text.push_back('B');
    CHECK(!MopQueryPackets::BuildPageTextQueryResponse(packet, response));
}

int main(int /*argc*/, char** /*argv*/)
{
    TestUseRequest();
    TestPageTextQueryResponse();

    if (g_fail != 0)
    {
        std::fprintf(stderr, "%d test(s) failed\n", g_fail);
        return 1;
    }

    std::puts("mop_gameobject_interaction_packets_test: PASS");
    return 0;
}
