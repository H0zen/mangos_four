/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * Independent byte fixtures for the 5.4.8.18414 death-release location.
 */

#include "Player.h"
#include "GridMap.h"
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
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;
    return true;
}

static WorldPacket MakePacket(OpcodesList opcode,
    std::vector<uint8_t> const& body)
{
    WorldPacket packet(opcode, body.size());
    if (!body.empty())
    {
        packet.append(body.data(), body.size());
    }
    return packet;
}

static void test_reclaim_corpse_request()
{
    // Live 2026-07-31 body from the deployed 18414 client.
    WorldPacket live = MakePacket(CMSG_RECLAIM_CORPSE, { 0x22, 0x00, 0xF4 });
    ObjectGuid guid;
    CHECK(MopDeathPackets::ParseReclaimCorpseRequest(live, guid));
    CHECK(guid.GetRawValue() == UINT64_C(0xF500000000000001));
    CHECK(live.rpos() == live.size());

    // Captured retail body capture-000019/112547.
    WorldPacket retail = MakePacket(CMSG_RECLAIM_CORPSE,
        { 0xAA, 0xC1, 0x44, 0xEA, 0xF0 });
    CHECK(MopDeathPackets::ParseReclaimCorpseRequest(retail, guid));
    CHECK(guid.GetRawValue() == UINT64_C(0xF1C00000000045EB));

    // Retail also sends the canonical zero-GUID body; it is not an
    // authorization field and must remain accepted.
    WorldPacket zero = MakePacket(CMSG_RECLAIM_CORPSE, { 0x00 });
    CHECK(MopDeathPackets::ParseReclaimCorpseRequest(zero, guid));
    CHECK(guid.IsEmpty());

    // Synthetic all-present body pins the full byte order. The real GUID is
    // 0x0807060504030201; each present byte is XORed with one on the wire.
    WorldPacket full = MakePacket(CMSG_RECLAIM_CORPSE,
        { 0xFF, 0x02, 0x07, 0x04, 0x06, 0x03, 0x00, 0x09, 0x05 });
    CHECK(MopDeathPackets::ParseReclaimCorpseRequest(full, guid));
    CHECK(guid.GetRawValue() == UINT64_C(0x0807060504030201));

    // Synthetic single-zero cases independently discriminate every mask bit.
    std::vector<std::vector<uint8_t>> const bodies = {
        { 0xFD, 0x02, 0x07, 0x04, 0x06, 0x03, 0x09, 0x05 },
        { 0x7F, 0x02, 0x07, 0x04, 0x06, 0x00, 0x09, 0x05 },
        { 0xEF, 0x07, 0x04, 0x06, 0x03, 0x00, 0x09, 0x05 },
        { 0xFB, 0x02, 0x07, 0x04, 0x06, 0x03, 0x00, 0x09 },
        { 0xFE, 0x02, 0x07, 0x06, 0x03, 0x00, 0x09, 0x05 },
        { 0xBF, 0x02, 0x04, 0x06, 0x03, 0x00, 0x09, 0x05 },
        { 0xF7, 0x02, 0x07, 0x04, 0x03, 0x00, 0x09, 0x05 },
        { 0xDF, 0x02, 0x07, 0x04, 0x06, 0x03, 0x00, 0x05 }
    };
    for (uint8_t zeroByte = 0; zeroByte < 8; ++zeroByte)
    {
        WorldPacket packet = MakePacket(CMSG_RECLAIM_CORPSE, bodies[zeroByte]);
        CHECK(MopDeathPackets::ParseReclaimCorpseRequest(packet, guid));
        uint64_t const expected = UINT64_C(0x0807060504030201) &
            ~(UINT64_C(0xFF) << (zeroByte * 8));
        CHECK(guid.GetRawValue() == expected);
    }
}

static void test_resurrect_request()
{
    WorldPacket player;
    CHECK(MopDeathPackets::BuildResurrectRequest(player,
        ObjectGuid(UINT64_C(0x05000000058B1DB9)), 50769, "", false, false,
        0, 0x03020006));
    CHECK(ExpectBytes(player, {
        0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x02, 0x03,
        0x51, 0xC6, 0x00, 0x00, 0x95, 0x40, 0x04, 0x04,
        0x8A, 0x1C, 0xB8
    }));

    WorldPacket npc;
    CHECK(MopDeathPackets::BuildResurrectRequest(npc,
        ObjectGuid(UINT64_C(0xF13096B30001DFB7)), 72423,
        "Terenas Menethil", false, true, 0, 0));
    CHECK(ExpectBytes(npc, {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xE7, 0x1A, 0x01, 0x00, 0x3F, 0xD0, 0xF0, 0x97,
        0x54, 0x65, 0x72, 0x65, 0x6E, 0x61, 0x73, 0x20,
        0x4D, 0x65, 0x6E, 0x65, 0x74, 0x68, 0x69, 0x6C,
        0x00, 0xB2, 0xDE, 0x31, 0xB6
    }));

    for (size_t length : { size_t(0), size_t(16), size_t(48) })
    {
        WorldPacket boundary;
        CHECK(MopDeathPackets::BuildResurrectRequest(boundary, ObjectGuid(),
            1, std::string(length, 'x'), false, false, 0, 0));
    }

    WorldPacket tooLong;
    CHECK(MopDeathPackets::BuildResurrectRequest(tooLong, ObjectGuid(), 1,
        std::string(49, 'x'), false, false, 0, 0));
    CHECK(tooLong.size() == 62);
    for (size_t i = 14; i < tooLong.size(); ++i)
    {
        CHECK(tooLong[i] == uint8_t('x'));
    }

    WorldPacket splitCodePoint;
    CHECK(MopDeathPackets::BuildResurrectRequest(splitCodePoint, ObjectGuid(), 1,
        std::string(46, 'x') + "\xE2\x82\xAC", false, false, 0, 0));
    CHECK(splitCodePoint.size() == 60);
    for (size_t i = 14; i < splitCodePoint.size(); ++i)
    {
        CHECK(splitCodePoint[i] == uint8_t('x'));
    }
}

static void test_spirit_healer_activate()
{
    struct RetailFixture
    {
        std::vector<uint8_t> body;
        uint64_t guid;
    };
    std::vector<RetailFixture> const retailFixtures = {
        { { 0x7E, 0x26, 0x18, 0x31, 0x9B, 0xF0, 0x5A }, UINT64_C(0xF130195B0000279A) },
        { { 0x7E, 0x25, 0x18, 0x31, 0x14, 0xF0, 0x5A }, UINT64_C(0xF130195B00002415) },
        { { 0x7E, 0x37, 0x18, 0x31, 0xDE, 0xF0, 0x5A }, UINT64_C(0xF130195B000036DF) },
        { { 0x7E, 0x37, 0x18, 0x31, 0xD5, 0xF0, 0x5A }, UINT64_C(0xF130195B000036D4) },
        { { 0x7E, 0x37, 0x18, 0x31, 0x2C, 0xF0, 0x5A }, UINT64_C(0xF130195B0000362D) }
    };
    ObjectGuid guid;
    for (RetailFixture const& fixture : retailFixtures)
    {
        WorldPacket packet = MakePacket(CMSG_SPIRIT_HEALER_ACTIVATE, fixture.body);
        CHECK(MopDeathPackets::ParseSpiritHealerActivate(packet, guid));
        CHECK(guid.GetRawValue() == fixture.guid);
        CHECK(packet.rpos() == packet.size());
    }

    WorldPacket full = MakePacket(CMSG_SPIRIT_HEALER_ACTIVATE,
        { 0xFF, 0x03, 0x07, 0x06, 0x05, 0x02, 0x00, 0x09, 0x04 });
    CHECK(MopDeathPackets::ParseSpiritHealerActivate(full, guid));
    CHECK(guid.GetRawValue() == UINT64_C(0x0807060504030201));

    WorldPacket zero = MakePacket(CMSG_SPIRIT_HEALER_ACTIVATE, { 0x00 });
    CHECK(MopDeathPackets::ParseSpiritHealerActivate(zero, guid));
    CHECK(guid.IsEmpty());

    std::vector<std::vector<uint8_t>> const bodies = {
        { 0xEF, 0x03, 0x07, 0x06, 0x05, 0x02, 0x09, 0x04 },
        { 0xFD, 0x07, 0x06, 0x05, 0x02, 0x00, 0x09, 0x04 },
        { 0x7F, 0x03, 0x07, 0x06, 0x05, 0x00, 0x09, 0x04 },
        { 0xFE, 0x03, 0x07, 0x06, 0x02, 0x00, 0x09, 0x04 },
        { 0xFB, 0x03, 0x07, 0x06, 0x05, 0x02, 0x00, 0x09 },
        { 0xF7, 0x03, 0x06, 0x05, 0x02, 0x00, 0x09, 0x04 },
        { 0xDF, 0x03, 0x07, 0x05, 0x02, 0x00, 0x09, 0x04 },
        { 0xBF, 0x03, 0x07, 0x06, 0x05, 0x02, 0x00, 0x04 }
    };
    for (uint8_t zeroByte = 0; zeroByte < 8; ++zeroByte)
    {
        WorldPacket packet = MakePacket(CMSG_SPIRIT_HEALER_ACTIVATE, bodies[zeroByte]);
        CHECK(MopDeathPackets::ParseSpiritHealerActivate(packet, guid));
        uint64_t const expected = UINT64_C(0x0807060504030201) &
            ~(UINT64_C(0xFF) << (zeroByte * 8));
        CHECK(guid.GetRawValue() == expected);
    }
}

static void test_spirit_healer_activate_rejects_malformed_bodies()
{
    std::vector<uint8_t> const full =
        { 0xFF, 0x03, 0x07, 0x06, 0x05, 0x02, 0x00, 0x09, 0x04 };
    std::vector<std::vector<uint8_t>> bodies;
    for (size_t size = 0; size < full.size(); ++size)
    {
        bodies.push_back(std::vector<uint8_t>(full.begin(), full.begin() + size));
    }
    bodies.push_back({ 0x00, 0x00 }); // trailing byte after a valid zero GUID
    bodies.push_back({ 0x80, 0x01 }); // present byte decodes to zero

    for (std::vector<uint8_t> const& body : bodies)
    {
        WorldPacket packet = MakePacket(CMSG_SPIRIT_HEALER_ACTIVATE, body);
        ObjectGuid guid(UINT64_C(0xFFFFFFFFFFFFFFFF));
        CHECK(!MopDeathPackets::ParseSpiritHealerActivate(packet, guid));
        CHECK(packet.rpos() == packet.size());
        CHECK(guid.GetRawValue() == UINT64_C(0xFFFFFFFFFFFFFFFF));
    }
}

static void test_cemetery_list_response()
{
    std::vector<uint32> const cemeteryIds = { 0x11223344u, 0xA1B2C3D4u };

    WorldPacket scheduled;
    MopDeathPackets::BuildCemeteryListResponse(scheduled, cemeteryIds, false);
    CHECK(ExpectBytes(scheduled, {
        0x00, 0x00, 0x08,
        0x44, 0x33, 0x22, 0x11,
        0xD4, 0xC3, 0xB2, 0xA1
    }));

    WorldPacket gossip;
    MopDeathPackets::BuildCemeteryListResponse(gossip, cemeteryIds, true);
    CHECK(ExpectBytes(gossip, {
        0x00, 0x00, 0x0A,
        0x44, 0x33, 0x22, 0x11,
        0xD4, 0xC3, 0xB2, 0xA1
    }));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_reclaim_corpse_request();
    test_resurrect_request();
    test_spirit_healer_activate();
    test_spirit_healer_activate_rejects_malformed_bodies();
    test_cemetery_list_response();
    return g_fail ? 1 : 0;
}
