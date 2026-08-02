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

static void test_short_motd()
{
    WorldPacket packet(SMSG_GUILD_EVENT_MOTD, 5);
    CHECK(MopGuildPackets::BuildGuildMotd(packet, "ABC"));
    CHECK(Equal(packet, { 0x00, 0xC0, 0x41, 0x42, 0x43 }));
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
// capture-000006 seq 2081, 18 bytes: mask 0xFFFF, so all sixteen bytes are present.
// capture-000006 seq 1959, 83 bytes. All 2,080 corpus observations are exactly 83.
// capture-000019 seq 185, 447 bytes: a five-rank guild with the stock MoP rank
// names. The 17-bit count is 5 and the five 7-bit name lengths are 12, 7, 7, 6
// and 8, which is exactly Guild Master, Officer, Veteran, Member and Initiate.
// capture-000019 seq 923, 235 bytes: a two-member guild. Decoding it field by
// field consumes all 235 exactly -- the 17-bit count reads 2 against two names,
// and the 10-bit MOTD length reads 24 against a 24-character MOTD.
// Each roster length goes out in a bit field narrower than the string it describes,
// so an oversized field must be refused rather than silently truncated.
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

int main(int /*argc*/, char** /*argv*/)
{
    test_short_motd();
    test_guild_invite_request();
    test_guild_achievement_tracking_request();
    test_guild_query_request();
    test_guild_query_response();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_guild_packets: all checks passed\n");
    return 0;
}
