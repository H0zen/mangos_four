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
 * Independent byte fixtures for 5.4.8.18414 guild packets.
 */

#include "Guild.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void Append(WorldPacket& packet, std::vector<uint8> const& bytes)
{
    if (!bytes.empty())
        packet.append(bytes.data(), bytes.size());
}

static void test_empty_motd()
{
    WorldPacket packet(SMSG_GUILD_EVENT_MOTD, 2);
    CHECK(MopGuildPackets::BuildGuildMotd(packet, ""));
    CHECK(Equal(packet, { 0x00, 0x00 }));
}

static void test_short_motd()
{
    WorldPacket packet(SMSG_GUILD_EVENT_MOTD, 5);
    CHECK(MopGuildPackets::BuildGuildMotd(packet, "ABC"));
    CHECK(Equal(packet, { 0x00, 0xC0, 0x41, 0x42, 0x43 }));
}

static void test_length_boundary()
{
    std::string maximum(1023, 'x');
    WorldPacket valid(SMSG_GUILD_EVENT_MOTD, maximum.size() + 2);
    CHECK(MopGuildPackets::BuildGuildMotd(valid, maximum));
    CHECK(valid.size() == maximum.size() + 2);
    CHECK(valid.contents()[0] == 0xFF);
    CHECK(valid.contents()[1] == 0xC0);

    WorldPacket rejected(SMSG_GUILD_EVENT_MOTD, 0);
    CHECK(!MopGuildPackets::BuildGuildMotd(rejected, std::string(1024, 'x')));
    CHECK(rejected.empty());
}

static void test_opcode()
{
    CHECK(uint32(SMSG_GUILD_EVENT_MOTD) == 0x0B68u);
    CHECK(uint32(SMSG_GUILD_EVENT_MOTD) < uint32(OPCODE_TABLE_SIZE));
}

static void test_tabard_vendor_activate_request()
{
    {
        WorldPacket packet(CMSG_TABARD_VENDOR_ACTIVATE, 9);
        Append(packet, { 0xFF, 0x09, 0x06, 0x02, 0x07, 0x00, 0x03, 0x04, 0x05 });
        CHECK(MopGuildPackets::ReadTabardVendorActivate(packet) == UI64LIT(0x0807060504030201));
    }
    {
        WorldPacket packet(CMSG_TABARD_VENDOR_ACTIVATE, 4);
        Append(packet, { 0x1A, 0xC2, 0xA0, 0xB3 });
        CHECK(MopGuildPackets::ReadTabardVendorActivate(packet) == UI64LIT(0x00C30000B20000A1));
    }
}

static void test_tabard_vendor_activate_response()
{
    WorldPacket full;
    MopGuildPackets::BuildTabardVendorActivate(full, UI64LIT(0x0807060504030201));
    CHECK(full.GetOpcode() == SMSG_TABARD_VENDOR_ACTIVATE);
    CHECK(Equal(full, { 0xFF, 0x07, 0x04, 0x02, 0x05, 0x06, 0x00, 0x03, 0x09 }));

    WorldPacket sparse;
    MopGuildPackets::BuildTabardVendorActivate(sparse, UI64LIT(0x00C30000B20000A1));
    CHECK(Equal(sparse, { 0x26, 0xB3, 0xC2, 0xA0 }));
}

static void test_save_guild_emblem_request()
{
    WorldPacket packet(CMSG_SAVE_GUILD_EMBLEM, 29);
    Append(packet, {
        0x34, 0x33, 0x32, 0x31, // border style
        0x54, 0x53, 0x52, 0x51, // background color
        0x44, 0x43, 0x42, 0x41, // border color
        0x24, 0x23, 0x22, 0x21, // emblem color
        0x14, 0x13, 0x12, 0x11, // emblem style
        0x91, 0xC2, 0xA0, 0xB3
    });

    MopGuildPackets::EmblemDesign const design = MopGuildPackets::ReadSaveGuildEmblem(packet);
    CHECK(design.vendorGuid == UI64LIT(0x00C30000B20000A1));
    CHECK(design.emblemStyle == 0x11121314u);
    CHECK(design.emblemColor == 0x21222324u);
    CHECK(design.borderStyle == 0x31323334u);
    CHECK(design.borderColor == 0x41424344u);
    CHECK(design.backgroundColor == 0x51525354u);
}

static void test_save_guild_emblem_result()
{
    for (uint32 result = ERR_GUILDEMBLEM_SUCCESS;
            result <= ERR_GUILDEMBLEM_INVALIDVENDOR; ++result)
    {
        WorldPacket packet;
        MopGuildPackets::BuildSaveGuildEmblemResult(packet, result);
        CHECK(packet.GetOpcode() == SMSG_SAVE_GUILD_EMBLEM);
        CHECK(Equal(packet, {
            uint8(result), 0x00, 0x00, 0x00
        }));
    }
}

static void test_tabard_opcodes()
{
    CHECK(uint32(CMSG_TABARD_VENDOR_ACTIVATE) == 0x11C3u);
    CHECK(uint32(SMSG_TABARD_VENDOR_ACTIVATE) == 0x0A3Eu);
    CHECK(uint32(CMSG_SAVE_GUILD_EMBLEM) == 0x1D60u);
    CHECK(uint32(SMSG_SAVE_GUILD_EMBLEM) == 0x089Fu);
}

static void test_guild_member_joined()
{
    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildMemberJoined(packet,
        UI64LIT(0x0807060504030201), "J", 0x11223344u));
    CHECK(packet.GetOpcode() == SMSG_GUILD_EVENT_PLAYER_JOINED);
    CHECK(Equal(packet, {
        0xE0, 0xFC,
        0x02, 0x04, 0x03, 0x06, 0x07,
        0x44, 0x33, 0x22, 0x11,
        0x05, 0x00, 0x4A, 0x09
    }));
}

static void test_guild_presence_change()
{
    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildPresenceChange(packet,
        UI64LIT(0x00C30000B20000A1), "P", 0x11223344u, true, false));
    CHECK(packet.GetOpcode() == SMSG_GUILD_EVENT_PRESENCE_CHANGE);
    CHECK(Equal(packet, {
        0xC4, 0x11,
        0xB3, 0xA0,
        0x44, 0x33, 0x22, 0x11,
        0xC2, 0x50
    }));
}

static void test_guild_member_rank_update()
{
    WorldPacket packet;
    MopGuildPackets::BuildGuildMemberRankUpdate(packet,
        UI64LIT(0x1817161514131211), UI64LIT(0x0807060504030201),
        3, true);
    CHECK(packet.GetOpcode() == SMSG_GUILD_RANKS_UPDATE);
    CHECK(Equal(packet, {
        0xFF, 0xFF, 0x80,
        0x02, 0x13, 0x06, 0x03, 0x07, 0x10,
        0x03, 0x00, 0x00, 0x00,
        0x15, 0x19, 0x09, 0x12, 0x05, 0x04,
        0x16, 0x17, 0x00, 0x14
    }));
}

static void test_guild_new_leader()
{
    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildNewLeader(packet,
        UI64LIT(0x0807060504030201), "O", 0x11223344u,
        UI64LIT(0x1817161514131211), "N", 0x55667788u, false));
    CHECK(packet.GetOpcode() == SMSG_GUILD_EVENT_NEW_LEADER);
    CHECK(Equal(packet, {
        0xF0, 0x7B, 0xF8, 0x38,
        0x17, 0x16, 0x4F, 0x4E, 0x15, 0x14,
        0x88, 0x77, 0x66, 0x55,
        0x06, 0x10, 0x07, 0x12, 0x19, 0x09, 0x04,
        0x44, 0x33, 0x22, 0x11,
        0x13, 0x02, 0x03, 0x05, 0x00
    }));
}

static void test_guild_disbanded()
{
    WorldPacket packet;
    MopGuildPackets::BuildGuildDisbanded(packet);
    CHECK(packet.GetOpcode() == SMSG_GUILD_EVENT_DISBANDED);
    CHECK(packet.empty());
}

static void test_guild_player_left()
{
    WorldPacket selfLeave;
    CHECK(MopGuildPackets::BuildGuildPlayerLeft(selfLeave,
        UI64LIT(0x0807060504030201), "L", 0x11223344u,
        false, 0, "", 0));
    CHECK(selfLeave.GetOpcode() == SMSG_GUILD_EVENT_PLAYER_LEFT);
    CHECK(Equal(selfLeave, {
        0x83, 0xBE, 0x4C, 0x03,
        0x44, 0x33, 0x22, 0x11,
        0x00, 0x04, 0x02, 0x05, 0x06, 0x07, 0x09
    }));

    WorldPacket removed;
    CHECK(MopGuildPackets::BuildGuildPlayerLeft(removed,
        UI64LIT(0x0807060504030201), "L", 0x11223344u,
        true, UI64LIT(0x1817161514131211), "R", 0x55667788u));
    CHECK(Equal(removed, {
        0x83, 0xC0, 0x7F, 0xDF,
        0x13, 0x15, 0x17, 0x12, 0x10, 0x14, 0x16, 0x19,
        0x52, 0x88, 0x77, 0x66, 0x55,
        0x4C, 0x03, 0x44, 0x33, 0x22, 0x11,
        0x00, 0x04, 0x02, 0x05, 0x06, 0x07, 0x09
    }));
}

static void test_guild_event_name_bounds()
{
    std::string const maximum(63, 'x');
    std::string const oversized(64, 'x');

    WorldPacket joined;
    CHECK(MopGuildPackets::BuildGuildMemberJoined(joined, 1, maximum, 1));
    WorldPacket joinedRejected;
    CHECK(!MopGuildPackets::BuildGuildMemberJoined(joinedRejected, 1, oversized, 1));
    CHECK(joinedRejected.empty());

    WorldPacket presence;
    CHECK(MopGuildPackets::BuildGuildPresenceChange(presence, 1, maximum, 1, true, false));
    WorldPacket presenceRejected;
    CHECK(!MopGuildPackets::BuildGuildPresenceChange(presenceRejected, 1, oversized, 1, true, false));
    CHECK(presenceRejected.empty());

    WorldPacket leader;
    CHECK(MopGuildPackets::BuildGuildNewLeader(leader, 1, maximum, 1,
        2, maximum, 1, false));
    WorldPacket leaderRejected;
    CHECK(!MopGuildPackets::BuildGuildNewLeader(leaderRejected, 1, oversized, 1,
        2, "n", 1, false));
    CHECK(leaderRejected.empty());

    WorldPacket left;
    CHECK(MopGuildPackets::BuildGuildPlayerLeft(left, 1, maximum, 1,
        true, 2, maximum, 1));
    WorldPacket leftRejected;
    CHECK(!MopGuildPackets::BuildGuildPlayerLeft(leftRejected, 1, "l", 1,
        true, 2, oversized, 1));
    CHECK(leftRejected.empty());
}

static void test_guild_event_opcodes()
{
    CHECK(uint32(SMSG_GUILD_RANKS_UPDATE) == 0x0A60u);
    CHECK(uint32(SMSG_GUILD_EVENT_PLAYER_JOINED) == 0x0B69u);
    CHECK(uint32(SMSG_GUILD_EVENT_PRESENCE_CHANGE) == 0x0B70u);
    CHECK(uint32(SMSG_GUILD_EVENT_PLAYER_LEFT) == 0x0BF8u);
    CHECK(uint32(SMSG_GUILD_EVENT_NEW_LEADER) == 0x0E69u);
    CHECK(uint32(SMSG_GUILD_EVENT_DISBANDED) == 0x1E68u);
}

static void test_guild_bank_money_withdrawn()
{
    WorldPacket packet(SMSG_GUILD_BANK_MONEY_WITHDRAWN, 8);
    packet << uint64(UI64LIT(0x0807060504030201));
    CHECK(Equal(packet, { 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08 }));
    CHECK(uint32(CMSG_GUILD_BANK_MONEY_WITHDRAWN) == 0x14DBu);
    CHECK(uint32(SMSG_GUILD_BANK_MONEY_WITHDRAWN) == 0x0B78u);
    CHECK(uint32(CMSG_GUILD_BANK_MONEY_WITHDRAWN) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_GUILD_BANK_MONEY_WITHDRAWN) < uint32(OPCODE_TABLE_SIZE));
}

static void test_guild_bank_text_bounds()
{
    WorldPacket shortText;
    CHECK(MopGuildPackets::BuildGuildBankText(
        shortText, 0x11223344u, "ABC"));
    CHECK(shortText.GetOpcode() == SMSG_GUILD_BANK_TEXT);
    CHECK(Equal(shortText, {
        0x00, 0x0C,
        0x44, 0x33, 0x22, 0x11,
        0x41, 0x42, 0x43
    }));

    WorldPacket maximum;
    CHECK(MopGuildPackets::BuildGuildBankText(
        maximum, 1u, std::string(500, 'x')));
    CHECK(maximum.size() == 506);

    WorldPacket truncated;
    CHECK(MopGuildPackets::BuildGuildBankText(
        truncated, 1u, std::string(501, 'x')));
    CHECK(truncated.size() == 506);
    CHECK(truncated.contents()[0] == 0x07);
    CHECK(truncated.contents()[1] == 0xD0);
    for (size_t index = 6; index < truncated.size(); ++index)
        CHECK(truncated.contents()[index] == uint8('x'));

    std::string const fourByteCharacter("\xF0\x9F\x98\x80", 4);
    std::string multibyte;
    for (size_t index = 0; index < 500; ++index)
        multibyte += fourByteCharacter;
    WorldPacket multibyteMaximum;
    CHECK(MopGuildPackets::BuildGuildBankText(
        multibyteMaximum, 1u, multibyte));
    CHECK(multibyteMaximum.size() == 2006);
}

static void test_guild_command_result()
{
    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildCommandResult(packet,
        0x01020304u, "ABC", 0x05060708u));
    CHECK(packet.GetOpcode() == SMSG_GUILD_COMMAND_RESULT);
    CHECK(Equal(packet, {
        0x04, 0x03, 0x02, 0x01,
        0x08, 0x07, 0x06, 0x05,
        0x03, 0x41, 0x42, 0x43
    }));

    WorldPacket empty;
    CHECK(MopGuildPackets::BuildGuildCommandResult(empty, 1, "", 2));
    CHECK(Equal(empty, {
        0x01, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00,
        0x00
    }));

    WorldPacket maximum;
    CHECK(MopGuildPackets::BuildGuildCommandResult(maximum, 1,
        std::string(255, 'x'), 2));
    CHECK(maximum.size() == 264);

    WorldPacket rejected;
    CHECK(!MopGuildPackets::BuildGuildCommandResult(rejected, 1,
        std::string(256, 'x'), 2));
    CHECK(rejected.empty());

    CHECK(uint32(SMSG_GUILD_COMMAND_RESULT) == 0x0EF1u);
}

static void test_guild_invite_request()
{
    WorldPacket live(CMSG_GUILD_INVITE, 19);
    Append(live, {
        0x08, 0x80,
        0x4E, 0x6F, 0x73, 0x75, 0x63, 0x68, 0x70, 0x6C, 0x61,
        0x79, 0x65, 0x72, 0x31, 0x32, 0x33, 0x34, 0x35
    });
    std::string name;
    CHECK(MopGuildPackets::ReadGuildInvite(live, name));
    CHECK(name == "Nosuchplayer12345");
    CHECK(live.rpos() == live.size());

    WorldPacket truncated(CMSG_GUILD_INVITE, 1);
    Append(truncated, { 0x08 });
    CHECK(!MopGuildPackets::ReadGuildInvite(truncated, name));

    WorldPacket wrongLength(CMSG_GUILD_INVITE, 19);
    Append(wrongLength, {
        0x09, 0x00,
        0x4E, 0x6F, 0x73, 0x75, 0x63, 0x68, 0x70, 0x6C, 0x61,
        0x79, 0x65, 0x72, 0x31, 0x32, 0x33, 0x34, 0x35
    });
    CHECK(!MopGuildPackets::ReadGuildInvite(wrongLength, name));

    WorldPacket trailing(CMSG_GUILD_INVITE, 20);
    Append(trailing, {
        0x08, 0x80,
        0x4E, 0x6F, 0x73, 0x75, 0x63, 0x68, 0x70, 0x6C, 0x61,
        0x79, 0x65, 0x72, 0x31, 0x32, 0x33, 0x34, 0x35, 0x00
    });
    CHECK(!MopGuildPackets::ReadGuildInvite(trailing, name));

    CHECK(uint32(CMSG_GUILD_INVITE) == 0x0869u);
}

static void test_guild_achievement_tracking_request()
{
    // Deployed 18414 smoke capture (2026-07-31): the client sent an empty
    // tracking snapshot as exactly three zero bytes. This proves the empty
    // encoding only; the non-empty shape below comes from the client writer.
    WorldPacket capturedEmpty(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, 3);
    Append(capturedEmpty, { 0x00, 0x00, 0x00 });
    std::vector<uint32> achievementIds = { 0xFFFFFFFFu };
    CHECK(MopGuildPackets::ReadGuildAchievementTracking(capturedEmpty, achievementIds));
    CHECK(achievementIds.empty());
    CHECK(capturedEmpty.rpos() == capturedEmpty.size());

    // Synthetic fixture derived from Wow.exe's 22-bit MSB-first count writer
    // and little-endian uint32 loop. Distinct scalars discriminate width,
    // padding, byte order, and element order; it is not a retail capture.
    WorldPacket nonEmpty(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, 11);
    Append(nonEmpty, {
        0x00, 0x00, 0x08,
        0x44, 0x33, 0x22, 0x11,
        0xD4, 0xC3, 0xB2, 0xA1
    });
    CHECK(MopGuildPackets::ReadGuildAchievementTracking(nonEmpty, achievementIds));
    CHECK(achievementIds.size() == 2);
    CHECK(achievementIds[0] == 0x11223344u);
    CHECK(achievementIds[1] == 0xA1B2C3D4u);
    CHECK(nonEmpty.rpos() == nonEmpty.size());

    WorldPacket maximum(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, 43);
    Append(maximum, { 0x00, 0x00, 0x28 });
    for (uint32 id = 1; id <= 10; ++id)
        maximum << id;
    CHECK(MopGuildPackets::ReadGuildAchievementTracking(maximum, achievementIds));
    CHECK(achievementIds.size() == 10);
    CHECK(achievementIds.front() == 1);
    CHECK(achievementIds.back() == 10);

    // The client enforces ten tracked achievements overall. Reject hostile
    // counts before reserving or reading their claimed scalar array.
    WorldPacket hostile(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, 47);
    Append(hostile, { 0x00, 0x00, 0x2C });
    for (uint32 id = 1; id <= 11; ++id)
        hostile << id;
    CHECK(!MopGuildPackets::ReadGuildAchievementTracking(hostile, achievementIds));
    CHECK(achievementIds.empty());

    WorldPacket shortHeader(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, 2);
    Append(shortHeader, { 0x00, 0x00 });
    achievementIds.push_back(7);
    CHECK(!MopGuildPackets::ReadGuildAchievementTracking(shortHeader, achievementIds));
    CHECK(achievementIds.empty());

    WorldPacket truncated(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, 3);
    Append(truncated, { 0x00, 0x00, 0x04 });
    achievementIds.push_back(7);
    CHECK(!MopGuildPackets::ReadGuildAchievementTracking(truncated, achievementIds));
    CHECK(achievementIds.empty());

    WorldPacket trailing(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, 7);
    Append(trailing, { 0x00, 0x00, 0x00, 0x44, 0x33, 0x22, 0x11 });
    achievementIds.push_back(7);
    CHECK(!MopGuildPackets::ReadGuildAchievementTracking(trailing, achievementIds));
    CHECK(achievementIds.empty());

    CHECK(uint32(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING) == 0x0CF0u);
}

/*
 * The three guild read-only queries. Every fixture below is a byte-for-byte retail
 * capture from the 18414 corpus (catalogue 2BE10C89), not a hand-rolled guess, and
 * the guid orders are the ones the client's own send serializers emit.
 */

// capture-000006 seq 2082, 7 bytes. Two further captures carry the identical body.
static void test_guild_query_ranks_request()
{
    std::vector<uint8> const capture = { 0xCF, 0xF5, 0x88, 0x24, 0x1E, 0x00, 0xFE };

    WorldPacket packet(CMSG_GUILD_QUERY_RANKS, capture.size());
    Append(packet, capture);

    uint64 const guid = MopGuildPackets::ReadGuildQueryRanks(packet);

    // Mask 0xCF leaves guid bytes 4 and 5 absent; the rest arrive XOR 1.
    CHECK(guid == 0x1FF4000001FF2589ULL);
    CHECK(packet.rpos() == capture.size());
}

// capture-000006 seq 2081, 18 bytes: mask 0xFFFF, so all sixteen bytes are present.
static void test_guild_roster_request()
{
    std::vector<uint8> const capture =
    {
        0xFF, 0xFF, 0x00, 0x00, 0x0F, 0xDA, 0x35, 0x00, 0x69,
        0xF7, 0x66, 0x0E, 0xF7, 0x41, 0x38, 0x38, 0x61, 0x4A
    };

    WorldPacket packet(CMSG_GUILD_ROSTER, capture.size());
    Append(packet, capture);

    uint64 guidA = 0;
    uint64 guidB = 0;
    MopGuildPackets::ReadGuildRoster(packet, guidA, guidB);

    CHECK(guidA == 0x0139F6340139F668ULL);
    CHECK(guidB == 0x0F674060014BDB0EULL);
    CHECK(packet.rpos() == capture.size());
}

// capture-000006 seq 1959, 83 bytes. All 2,080 corpus observations are exactly 83.
static void test_guild_permissions_response()
{
    std::vector<uint8> const capture =
    {
        0x05, 0x00, 0x00, 0x00,                             // rank id 5
        0x00, 0x00, 0x00, 0x00,                             // money per day remaining
        0x07, 0x00, 0x00, 0x00,                             // seven purchased tabs
        0x53, 0x60, 0x10, 0x00,                             // rank rights mask
        0x00, 0x00, 0x40,                                   // 21-bit tab count == 8
        0x04, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00      // eighth tab unpurchased
    };

    uint32 const remainingSlots[GUILD_BANK_MAX_TABS] = { 4, 3, 2, 1, 0, 0, 0, 0 };
    uint32 const tabRights[GUILD_BANK_MAX_TABS]      = { 3, 3, 3, 3, 3, 3, 3, 0 };

    WorldPacket packet;
    MopGuildPackets::BuildGuildPermissions(packet, 5, 0, 7, 0x00106053,
        remainingSlots, tabRights);

    CHECK(packet.GetOpcode() == SMSG_GUILD_PERMISSIONS);
    CHECK(packet.size() == 83);
    CHECK(Equal(packet, capture));
}

// capture-000019 seq 185, 447 bytes: a five-rank guild with the stock MoP rank
// names. The 17-bit count is 5 and the five 7-bit name lengths are 12, 7, 7, 6
// and 8, which is exactly Guild Master, Officer, Veteran, Member and Initiate.
static void test_guild_ranks_response()
{
    std::vector<uint8> const capture =
    {
        0x00, 0x02, 0x8C, 0x0E, 0x1C, 0x30, 0x80, 0x00, 0x00, 0x00, 0x00, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x47, 0x75, 0x69, 0x6C, 0x64,
        0x20, 0x4D, 0x61, 0x73, 0x74, 0x65, 0x72, 0x00, 0x00, 0x00, 0x00, 0xBF,
        0xFF, 0xDD, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x4F, 0x66, 0x66, 0x69, 0x63, 0x65, 0x72, 0x01, 0x00,
        0x00, 0x00, 0xFF, 0xF3, 0xD1, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x56, 0x65, 0x74, 0x65, 0x72, 0x61,
        0x6E, 0x02, 0x00, 0x00, 0x00, 0x43, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4D, 0x65, 0x6D,
        0x62, 0x65, 0x72, 0x03, 0x00, 0x00, 0x00, 0x43, 0x00, 0x00, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x49,
        0x6E, 0x69, 0x74, 0x69, 0x61, 0x74, 0x65, 0x04, 0x00, 0x00, 0x00, 0x43,
        0x00, 0x00, 0x00
    };

    auto makeRank = [](uint32 index, uint32 money, uint32 tabFill,
        char const* name, uint32 rights)
    {
        MopGuildPackets::RankEntry rank;
        rank.index = index;
        rank.bankMoneyPerDay = money;
        for (uint8 tab = 0; tab < GUILD_BANK_MAX_TABS; ++tab)
        {
            rank.tabSlots[tab] = tabFill;
            rank.tabRights[tab] = tabFill;
        }
        rank.name = name;
        rank.rankId = index;
        rank.rights = rights;
        return rank;
    };

    std::vector<MopGuildPackets::RankEntry> const ranks =
    {
        makeRank(0, 0xFFFFFFFF, 0xFFFFFFFF, "Guild Master", 0x00DDFFBF),
        makeRank(1, 0, 0, "Officer",  0x00D1F3FF),
        makeRank(2, 0, 0, "Veteran",  0x00000043),
        makeRank(3, 0, 0, "Member",   0x00000043),
        makeRank(4, 0, 0, "Initiate", 0x00000043)
    };

    WorldPacket packet;
    MopGuildPackets::BuildGuildRanks(packet, ranks);

    CHECK(packet.GetOpcode() == SMSG_GUILD_QUERY_RANKS_RESULT);
    CHECK(packet.size() == 447);
    CHECK(Equal(packet, capture));
}

// capture-000019 seq 923, 235 bytes: a two-member guild. Decoding it field by
// field consumes all 235 exactly -- the 17-bit count reads 2 against two names,
// and the 10-bit MOTD length reads 24 against a 24-character MOTD.
static void test_guild_roster_response()
{
    std::vector<uint8> const capture =
    {
        0x00, 0x01, 0x03, 0x00, 0x00, 0x06, 0x15, 0x60, 0x00, 0x06, 0x2D, 0x60,
        0x30, 0x01, 0x65, 0x04, 0x00, 0x00, 0x59, 0x72, 0x72, 0x65, 0x62, 0x02,
        0x03, 0x00, 0x00, 0x00, 0xBA, 0x00, 0x00, 0x00, 0xB4, 0x00, 0x00, 0x00,
        0x02, 0x00, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x00, 0x5A, 0x00, 0x00, 0x00,
        0x5A, 0x01, 0xD0, 0x16, 0x00, 0x00, 0x17, 0x11, 0x00, 0x00, 0x06, 0x60,
        0x3D, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x01, 0x03, 0x07, 0x60, 0x3D, 0x08,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x08, 0x00, 0x00, 0x4E, 0x2C, 0x01,
        0xFF, 0xFF, 0xFF, 0xFF, 0x48, 0x65, 0x62, 0x6E, 0x69, 0x7A, 0x61, 0x64,
        0x6B, 0x65, 0x6D, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x6E, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x43, 0x00, 0x00, 0x00, 0x06, 0x00, 0x0E, 0x00, 0x00, 0x00, 0x17, 0x11,
        0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x69,
        0xA0, 0xF6, 0x42, 0x00, 0x04, 0x00, 0x00, 0x00, 0x19, 0x00, 0x01, 0x03,
        0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
        0xFF, 0x12, 0x23, 0x02, 0x00, 0x00, 0x00, 0x3A, 0x45, 0x44, 0x0E, 0x53,
        0x6D, 0x6F, 0x75, 0x6C, 0x61, 0x20, 0x67, 0x75, 0x69, 0x6C, 0x64, 0x17,
        0x11, 0x00, 0x00, 0x59, 0x6F, 0x75, 0x20, 0x63, 0x61, 0x6E, 0x20, 0x6C,
        0x65, 0x61, 0x76, 0x65, 0x20, 0x61, 0x6E, 0x79, 0x20, 0x74, 0x69, 0x6D,
        0x65, 0x3A, 0x29, 0x00, 0x00, 0x00, 0x00
    };

    MopGuildPackets::RosterMember first;
    first.guid = 0x06000000072D4F03ULL;
    first.cls = 1;
    first.level = 90;
    first.flags = 1;                                        // online
    first.zoneId = 5840;
    first.rankId = 0;                                       // guild master
    first.totalReputation = 1125;
    first.remainingWeekReputation = 4375;
    first.totalActivity = 540000;
    first.weekActivity = 540000;
    first.achievementPoints = 2200;
    first.virtualRealm = 50397209;
    first.lastLogoutDays = 0.0f;
    uint32 const firstProfessions[6] = { 3, 186, 180, 2, 164, 90 };
    for (uint8 i = 0; i < 6; ++i) { first.professions[i] = firstProfessions[i]; }
    first.name = "Yrreb";

    MopGuildPackets::RosterMember second;
    second.guid = 0x0600000007221301ULL;
    second.cls = 1;
    second.level = 6;
    second.flags = 0;                                       // offline
    second.zoneId = 14;
    second.rankId = 4;
    second.totalReputation = 0xFFFFFFFF;
    second.remainingWeekReputation = 4375;
    second.totalActivity = 0;
    second.weekActivity = 0;
    second.achievementPoints = 0xFFFFFFFF;
    second.virtualRealm = 50397209;
    second.lastLogoutDays = 123.31330108642578f;
    uint32 const secondProfessions[6] = { 2, 0, 110, 2, 0, 67 };
    for (uint8 i = 0; i < 6; ++i) { second.professions[i] = secondProfessions[i]; }
    second.name = "Hebnizadkem";

    std::vector<MopGuildPackets::RosterMember> const roster = { first, second };

    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildRoster(packet, roster, "You can leave any time:)",
        "Smoula guild", 2, 0x0E44453A, 4375));

    CHECK(packet.GetOpcode() == SMSG_GUILD_ROSTER);
    CHECK(packet.size() == 235);
    CHECK(Equal(packet, capture));
}

// Each roster length goes out in a bit field narrower than the string it describes,
// so an oversized field must be refused rather than silently truncated.
static void test_guild_roster_length_bounds()
{
    MopGuildPackets::RosterMember member;
    member.guid = 0x0600000007221301ULL;
    member.name = "Yrreb";

    std::vector<MopGuildPackets::RosterMember> roster = { member };
    WorldPacket packet;

    // Baseline accepts.
    CHECK(MopGuildPackets::BuildGuildRoster(packet, roster, "", "", 1, 0, 0));

    // 6-bit name length: 63 fits, 64 does not.
    roster[0].name = std::string(63, 'a');
    CHECK(MopGuildPackets::BuildGuildRoster(packet, roster, "", "", 1, 0, 0));
    roster[0].name = std::string(64, 'a');
    CHECK(!MopGuildPackets::BuildGuildRoster(packet, roster, "", "", 1, 0, 0));
    roster[0].name = "Yrreb";

    // 8-bit note lengths: 255 fits, 256 does not.
    roster[0].publicNote = std::string(255, 'p');
    CHECK(MopGuildPackets::BuildGuildRoster(packet, roster, "", "", 1, 0, 0));
    roster[0].publicNote = std::string(256, 'p');
    CHECK(!MopGuildPackets::BuildGuildRoster(packet, roster, "", "", 1, 0, 0));
    roster[0].publicNote.clear();

    roster[0].officerNote = std::string(255, 'o');
    CHECK(MopGuildPackets::BuildGuildRoster(packet, roster, "", "", 1, 0, 0));
    roster[0].officerNote = std::string(256, 'o');
    CHECK(!MopGuildPackets::BuildGuildRoster(packet, roster, "", "", 1, 0, 0));
    roster[0].officerNote.clear();

    // 10-bit MOTD length: 1023 fits, 1024 does not.
    CHECK(MopGuildPackets::BuildGuildRoster(packet, roster, std::string(1023, 'm'), "", 1, 0, 0));
    CHECK(!MopGuildPackets::BuildGuildRoster(packet, roster, std::string(1024, 'm'), "", 1, 0, 0));

    // 11-bit guild-info length: 2047 fits, 2048 does not.
    CHECK(MopGuildPackets::BuildGuildRoster(packet, roster, "", std::string(2047, 'i'), 1, 0, 0));
    CHECK(!MopGuildPackets::BuildGuildRoster(packet, roster, "", std::string(2048, 'i'), 1, 0, 0));

    // 17-bit member count: 131071 fits, 131072 does not. No real guild is anywhere
    // near this, but the guard is what stops the count silently wrapping.
    MopGuildPackets::RosterMember tiny;
    tiny.name = "a";
    std::vector<MopGuildPackets::RosterMember> const atLimit((size_t(1) << 17) - 1, tiny);
    std::vector<MopGuildPackets::RosterMember> const overLimit(size_t(1) << 17, tiny);
    CHECK(MopGuildPackets::BuildGuildRoster(packet, atLimit, "", "", 1, 0, 0));
    CHECK(!MopGuildPackets::BuildGuildRoster(packet, overLimit, "", "", 1, 0, 0));
}

// CMSG_GUILD_QUERY request decode. Both bodies are retail. The inherited reader took
// two raw uint64 -- a fixed 16 bytes -- against a wire body of 9..17, so it could not
// have parsed either of these.
static void test_guild_query_request()
{
    {
        std::vector<uint8> const capture =
        {
            0xD5, 0x5F, 0x05, 0xC4, 0x1E, 0xA0, 0xF5, 0xEB, 0x00, 0x12, 0x04, 0xF5, 0xCD
        };
        WorldPacket packet(CMSG_GUILD_QUERY, capture.size());
        Append(packet, capture);

        uint64 playerGuid = 0;
        uint64 guildGuid = 0;
        MopGuildPackets::ReadGuildQuery(packet, playerGuid, guildGuid);

        CHECK(playerGuid == 0x040000000513CCA1ULL);
        CHECK(guildGuid == 0x1FF4000001C5F4EAULL);
        CHECK(packet.rpos() == capture.size());
    }
    {
        std::vector<uint8> const capture =
        {
            0x95, 0x5F, 0x05, 0x21, 0x1E, 0xFD, 0xF5, 0x1A, 0x03, 0xC6, 0xC7, 0xC5
        };
        WorldPacket packet(CMSG_GUILD_QUERY, capture.size());
        Append(packet, capture);

        uint64 playerGuid = 0;
        uint64 guildGuid = 0;
        MopGuildPackets::ReadGuildQuery(packet, playerGuid, guildGuid);

        CHECK(playerGuid == 0x0400000000C7C4FCULL);
        CHECK(guildGuid == 0x1FF400000220C61BULL);
        CHECK(packet.rpos() == capture.size());
    }
}

// capture-000004 seq 39473, 133 bytes: a four-rank guild. Decoding it field by field
// consumes all 133 exactly, and the guid appears twice -- once inside the has-data
// block and again at the end in a different byte order, identical both times.
static void test_guild_query_response()
{
    std::vector<uint8> const capture =
    {
        0x40, 0x00, 0x08, 0xA1, 0xC3, 0x10, 0x1A, 0xE3, 0xDE, 0x80, 0x02, 0x00,
        0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x0B, 0x1E, 0x0E, 0x00, 0x00, 0x00,
        0x0D, 0x00, 0x04, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x46, 0xC3, 0xBC, 0x68, 0x72, 0x65, 0x72, 0x03, 0x00, 0x00, 0x00, 0x05,
        0x00, 0x00, 0x00, 0x4D, 0x65, 0x6D, 0x62, 0x65, 0x72, 0x02, 0x00, 0x00,
        0x00, 0x06, 0x00, 0x00, 0x00, 0x4F, 0x62, 0x65, 0x72, 0x67, 0x72, 0x75,
        0x70, 0x70, 0x65, 0x6E, 0x66, 0xC3, 0xBC, 0x68, 0x72, 0x01, 0x00, 0x00,
        0x00, 0x09, 0x00, 0x00, 0x00, 0x52, 0x65, 0x69, 0x63, 0x68, 0x73, 0x66,
        0xC3, 0xBC, 0x68, 0x72, 0x65, 0x72, 0x49, 0x20, 0x4E, 0x20, 0x43, 0x20,
        0x4F, 0x20, 0x4D, 0x20, 0x49, 0x20, 0x4E, 0x20, 0x47, 0x02, 0x00, 0x00,
        0x00, 0x03, 0x00, 0x00, 0x00, 0x78, 0xF5, 0x8D, 0x0B, 0xF5, 0x8D, 0x1E,
        0x78
    };

    // The three names carry a U+00FC. Writing it as a \x escape is not reliable here:
    // MSVC re-encodes the escape into the execution charset and emits four bytes where
    // retail has two, which inflates both the name and its 7-bit length field. Build the
    // byte sequence explicitly instead.
    std::string const uuml = std::string(1, char(0xC3)) + char(0xBC);
    std::vector<MopGuildPackets::QueryRank> const ranks =
    {
        { 0, 0, "F" + uuml + "hrer" },
        { 3, 5, "Member" },
        { 2, 6, "Obergruppenf" + uuml + "hr" },
        { 1, 9, "Reichsf" + uuml + "hrer" }
    };
    CHECK(ranks[0].name.size() == 7);
    CHECK(ranks[2].name.size() == 16);
    CHECK(ranks[3].name.size() == 13);

    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildQueryResponse(packet, 0x1FF40000000A798CULL,
        "I N C O M I N G", ranks,
        /*emblemStyle*/ 24, /*emblemColor*/ 14, /*borderStyle*/ 2,
        /*borderColor*/ 2, /*backgroundColor*/ 3, /*realm*/ 50593805));

    CHECK(packet.GetOpcode() == SMSG_GUILD_QUERY_RESPONSE);
    std::fprintf(stderr, "GQ got: ");
    for (size_t i = 0; i < packet.size(); ++i) std::fprintf(stderr, "%02X ", packet.contents()[i]);
    std::fprintf(stderr, "|");
    for (size_t i = 0; i < packet.size() && i < capture.size(); ++i)
        if (packet.contents()[i] != capture[i]) { std::fprintf(stderr, "diff@%u got %02X want %02X ", unsigned(i), packet.contents()[i], capture[i]); break; }
    std::fprintf(stderr, "|\n");
    CHECK(packet.size() == 133);
    CHECK(Equal(packet, capture));
}

static void test_guild_query_opcodes()
{
    CHECK(CMSG_GUILD_QUERY == 0x1AB6);
    CHECK(CMSG_GUILD_ROSTER == 0x1459);
    CHECK(CMSG_GUILD_QUERY_RANKS == 0x0D50);
    CHECK(CMSG_GUILD_PERMISSIONS == 0x145A);
    CHECK(SMSG_GUILD_ROSTER == 0x0BE0);
    CHECK(SMSG_GUILD_QUERY_RANKS_RESULT == 0x0A79);
    CHECK(SMSG_GUILD_PERMISSIONS == 0x0FF9);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_empty_motd();
    test_short_motd();
    test_length_boundary();
    test_opcode();
    test_tabard_vendor_activate_request();
    test_tabard_vendor_activate_response();
    test_save_guild_emblem_request();
    test_save_guild_emblem_result();
    test_tabard_opcodes();
    test_guild_member_joined();
    test_guild_presence_change();
    test_guild_member_rank_update();
    test_guild_new_leader();
    test_guild_disbanded();
    test_guild_player_left();
    test_guild_event_name_bounds();
    test_guild_event_opcodes();
    test_guild_bank_money_withdrawn();
    test_guild_bank_text_bounds();
    test_guild_command_result();
    test_guild_invite_request();
    test_guild_achievement_tracking_request();
    test_guild_query_ranks_request();
    test_guild_roster_request();
    test_guild_permissions_response();
    test_guild_ranks_response();
    test_guild_roster_response();
    test_guild_roster_length_bounds();
    test_guild_query_request();
    test_guild_query_response();
    test_guild_query_opcodes();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_guild_packets: all checks passed\n");
    return 0;
}
