
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
 * Byte-exact inbound fixtures for the consolidated live-log opcode worklist.
 */

#include "Channel.h"
#include "GossipDef.h"
#include "Player.h"
#include "WorldSession.h"

#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_hotfix_request()
{
    ObjectGuid first(uint64(0x0800000500000001ull));
    ObjectGuid second(uint64(0x0007060004000200ull));
    WorldPacket packet(CMSG_REQUEST_HOTFIX, 40);
    packet << uint32(0x11223344u);
    packet.WriteBits(2, 21);
    packet.WriteGuidMask<6, 3, 0, 1, 4, 5, 7, 2>(first);
    packet.WriteGuidMask<6, 3, 0, 1, 4, 5, 7, 2>(second);
    packet.FlushBits();
    packet.WriteGuidBytes<1>(first);
    packet << uint32(0xA1B2C3D4u);
    packet.WriteGuidBytes<0, 5, 6, 4, 7, 2, 3>(first);
    packet.WriteGuidBytes<1>(second);
    packet << uint32(0x55667788u);
    packet.WriteGuidBytes<0, 5, 6, 4, 7, 2, 3>(second);

    MopHotfixPackets::HotfixRequest request;
    CHECK(MopHotfixPackets::ReadHotfixRequest(packet, request));
    CHECK(request.type == 0x11223344u);
    CHECK(request.records.size() == 2);
    CHECK(request.records[0].guid == first.GetRawValue());
    CHECK(request.records[0].entry == 0xA1B2C3D4u);
    CHECK(request.records[1].guid == second.GetRawValue());
    CHECK(request.records[1].entry == 0x55667788u);
    CHECK(packet.rpos() == packet.size());

    WorldPacket malformed(CMSG_REQUEST_HOTFIX, 7);
    malformed << uint32(0x11223344u);
    malformed.WriteBits(100, 21);
    malformed.FlushBits();
    MopHotfixPackets::HotfixRequest rejected;
    rejected.records.push_back({ 1, 2 });
    CHECK(!MopHotfixPackets::ReadHotfixRequest(malformed, rejected));
    CHECK(rejected.records.empty());
}

static void test_join_channel_request()
{
    std::string const channel = "General";
    std::string const password = "pw";
    WorldPacket packet(CMSG_JOIN_CHANNEL, 20);
    packet << uint32(0x11223344u);
    packet.WriteBit(true);
    packet.WriteBits(uint32(channel.size()), 7);
    packet.WriteBits(uint32(password.size()), 7);
    packet.WriteBit(false);
    packet.FlushBits();
    packet.WriteStringData(password);
    packet.WriteStringData(channel);

    MopChannelPackets::JoinChannelRequest request;
    CHECK(MopChannelPackets::ReadJoinChannelRequest(packet, request));
    CHECK(request.channelId == 0x11223344u);
    CHECK(request.hasVoice);
    CHECK(!request.zoneUpdate);
    CHECK(request.channelName == channel);
    CHECK(request.password == password);
    CHECK(packet.rpos() == packet.size());

    WorldPacket truncated(CMSG_JOIN_CHANNEL, 7);
    truncated << uint32(0x11223344u);
    truncated.WriteBit(false);
    truncated.WriteBits(5, 7);
    truncated.WriteBits(5, 7);
    truncated.WriteBit(false);
    truncated.FlushBits();
    MopChannelPackets::JoinChannelRequest rejected;
    CHECK(!MopChannelPackets::ReadJoinChannelRequest(truncated, rejected));
}

static void test_load_screen_request()
{
    WorldPacket packet(CMSG_LOAD_SCREEN, 5);
    packet << uint32(0x11223344u);
    packet.WriteBit(true);
    packet.FlushBits();

    MopClientRequestPackets::LoadScreenRequest request =
        MopClientRequestPackets::ReadLoadScreenRequest(packet);
    CHECK(request.mapId == 0x11223344u);
    CHECK(request.loading);
    CHECK(packet.rpos() == packet.size());
}

static void test_player_state_requests()
{
    ObjectGuid const expected(UI64LIT(0x0800060004000201));
    WorldPacket selection(CMSG_SET_SELECTION, 16);
    selection.WriteGuidMask<7, 6, 5, 4, 3, 2, 1, 0>(expected);
    selection.FlushBits();
    selection.WriteGuidBytes<0, 7, 3, 5, 1, 4, 6, 2>(expected);

    ObjectGuid decoded;
    decoded[7] = selection.ReadBit();
    decoded[6] = selection.ReadBit();
    decoded[5] = selection.ReadBit();
    decoded[4] = selection.ReadBit();
    decoded[3] = selection.ReadBit();
    decoded[2] = selection.ReadBit();
    decoded[1] = selection.ReadBit();
    decoded[0] = selection.ReadBit();
    selection.ReadByteSeq(decoded[0]);
    selection.ReadByteSeq(decoded[7]);
    selection.ReadByteSeq(decoded[3]);
    selection.ReadByteSeq(decoded[5]);
    selection.ReadByteSeq(decoded[1]);
    selection.ReadByteSeq(decoded[4]);
    selection.ReadByteSeq(decoded[6]);
    selection.ReadByteSeq(decoded[2]);
    CHECK(decoded == expected);
    CHECK(selection.rpos() == selection.size());

    WorldPacket sheathed(CMSG_SETSHEATHED, 5);
    sheathed << uint32(2);
    sheathed.WriteBit(true);
    sheathed.FlushBits();
    uint32 sheathState = 0;
    sheathed >> sheathState;
    CHECK(sheathState == 2u);
    CHECK(sheathed.ReadBit());
    CHECK(sheathed.rpos() == sheathed.size());

    WorldPacket stand(CMSG_STANDSTATECHANGE, 4);
    stand << uint32(3);
    uint32 standState = 0;
    stand >> standState;
    CHECK(standState == 3u);
    CHECK(stand.rpos() == stand.size());

    WorldPacket standUpdate(SMSG_STANDSTATE_UPDATE, 1);
    standUpdate << uint8(4);
    CHECK(standUpdate.read<uint8>() == 4u);
    CHECK(standUpdate.rpos() == standUpdate.size());
}

static void test_questgiver_status_packets()
{
    ObjectGuid const expected(UI64LIT(0x0800060004000201));
    WorldPacket request(CMSG_QUESTGIVER_STATUS_QUERY, 9);
    request.WriteGuidMask<4, 3, 2, 1, 0, 5, 7, 6>(expected);
    request.FlushBits();
    request.WriteGuidBytes<5, 7, 4, 0, 2, 1, 6, 3>(expected);
    CHECK(MopQuestStatusPackets::ReadStatusQuery(request) == expected);
    CHECK(request.rpos() == request.size());

    WorldPacket response;
    MopQuestStatusPackets::BuildStatus(response, 0x11223344u, expected);
    CHECK(response.GetOpcode() == SMSG_QUESTGIVER_STATUS);
    ObjectGuid decoded;
    response.ReadGuidMask<1, 7, 4, 2, 5, 3, 6, 0>(decoded);
    response.ReadByteSeq(decoded[7]);
    uint32 status = 0;
    response >> status;
    response.ReadGuidBytes<4, 6, 1, 5, 2, 0, 3>(decoded);
    CHECK(decoded == expected);
    CHECK(status == 0x11223344u);
    CHECK(response.rpos() == response.size());
}

int main(int /*argc*/, char** /*argv*/)
{
    test_hotfix_request();
    test_join_channel_request();
    test_load_screen_request();
    test_player_state_requests();
    test_questgiver_status_packets();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("mop_client_opcode_worklist: all checks passed\n");
    return 0;
}
