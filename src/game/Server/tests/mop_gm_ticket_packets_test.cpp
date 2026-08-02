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
 * Independent byte fixtures for the 5.4.8.18414 GM-ticket update response.
 */

#include "GMTicketMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket make_create_request(std::vector<uint8> const& body)
{
    WorldPacket packet(CMSG_GMTICKET_CREATE, body.size());
    if (!body.empty())
        packet.append(body.data(), body.size());
    return packet;
}

static void test_create_request()
{
    std::vector<uint8> const body = {
        0x44,0x33,0x22,0x11, // map ID
        0x00,0x00,0xA0,0x3F, // Z = 1.25
        0x00,0x00,0x20,0xC0, // Y = -2.5
        0x5A,                // category / unknown
        0x00,0x00,0x70,0x40, // X = 3.75
        0x00,0x00,0x00,0x00, // no chat attachment
        0x80,0x10,           // need response, not follow-up, 11-bit length 2
        'O','K'
    };
    WorldPacket packet = make_create_request(body);
    MopGMTicketPackets::CreateRequest request;

    CHECK(MopGMTicketPackets::ReadCreateRequest(packet, request));
    CHECK(request.mapId == 0x11223344u);
    CHECK(request.x == 3.75f);
    CHECK(request.y == -2.5f);
    CHECK(request.z == 1.25f);
    CHECK(request.category == 0x5Au);
    CHECK(request.attachmentSize == 0u);
    CHECK(request.needResponse);
    CHECK(!request.isFollowup);
    CHECK(request.message == "OK");
    CHECK(packet.rpos() == packet.size());
    CHECK(uint32(CMSG_GMTICKET_CREATE) == 0x1A86u);
    CHECK(uint32(CMSG_GMTICKET_CREATE) < uint32(OPCODE_TABLE_SIZE));
}

static void test_create_request_11_bit_message_length()
{
    std::vector<uint8> body = {
        0x01,0x00,0x00,0x00,
        0x00,0x00,0x80,0x3F,
        0x00,0x00,0x00,0x40,
        0x7F,
        0x00,0x00,0x40,0x40,
        0x00,0x00,0x00,0x00,
        0xA0,0x08 // need response, not follow-up, literal 11-bit length 1025
    };
    body.insert(body.end(), 1025, uint8('Q'));
    WorldPacket packet = make_create_request(body);
    MopGMTicketPackets::CreateRequest request;

    CHECK(MopGMTicketPackets::ReadCreateRequest(packet, request));
    CHECK(request.message == std::string(1025, 'Q'));
    CHECK(packet.rpos() == packet.size());
}

static void check_create_request_rejected(std::vector<uint8> const& body)
{
    WorldPacket packet = make_create_request(body);
    MopGMTicketPackets::CreateRequest request;
    request.message = "stale";
    CHECK(!MopGMTicketPackets::ReadCreateRequest(packet, request));
    CHECK(request.message.empty());
    CHECK(packet.rpos() == packet.size());
}

static void test_create_request_rejects_malformed_bodies()
{
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x00,0x00,0x00 // fixed prefix truncated by one byte
    });
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x04,0x00,0x00,0x00, 0xAA,0xBB,0xCC // attachment overruns tail
    });
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x00,0x00,0x00,0x00, 0x80 // 13-bit header truncated
    });
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x00,0x00,0x00,0x00, 0x80,0x18, 'O','K' // length 3, tail 2
    });
    check_create_request_rejected({
        0x44,0x33,0x22,0x11, 0x00,0x00,0xA0,0x3F,
        0x00,0x00,0x20,0xC0, 0x5A, 0x00,0x00,0x70,0x40,
        0x00,0x00,0x00,0x00, 0x80,0x10, 'O','K','X' // trailing byte
    });
}

int main(int /*argc*/, char** /*argv*/)
{
    test_create_request();
    test_create_request_11_bit_message_length();
    test_create_request_rejects_malformed_bodies();
    return g_fail ? 1 : 0;
}
