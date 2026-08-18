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
        // This guild's two colours are 2 and 3, both inside the 17-row border
        // table, so the capture alone cannot separate them; the assignment comes
        // from retail guilds whose post-name slot holds 44 and 45. Swapping these
        // two inputs against the builder swap leaves the bytes identical.
        /*borderColor*/ 3, /*backgroundColor*/ 2, /*realm*/ 50593805));

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


/// CMSG_GUILD_SET_NOTE against two real 18414 bodies -- capture-001013 seq 397246
/// (15 bytes) and capture-000980 seq 10518 (20 bytes), catalogue 2BE10C89. Four
/// captures of this opcode exist; these two differ in note length and in which
/// GUID bytes are present, so between them they exercise the length field, the
/// interleaved string and a different popcount.
///
/// KNOWN LIMIT, measured rather than assumed. All four captured bodies share the
/// presence mask 11110001 -- bytes 0,1,2,3,7 present, 4,5,6 absent, because a
/// player GUID zeroes the high three. An exhaustive mutation sweep over this
/// fixture gives:
///
///   mask swaps   13 of 28 still pass -- every swap WITHIN {0,1,2,3,7} and
///                every swap within {4,5,6}, since swapping two positions that
///                carry the same presence value is a no-op on the wire
///   byte swaps    7 of 28 still pass -- 1<->5, 1<->6, 3<->4, 4<->5, 4<->6,
///                4<->7, 5<->6, each involving an absent byte
///   length width  0 of 32 pass
///   flag polarity fails correctly
///
/// So this fixture proves the GUID is assembled from the right bytes in the
/// right order for every position the captures can distinguish, and nothing
/// beyond that. More captures cannot help: all four share the mask. Closing the
/// remainder needs a body with a different presence pattern -- a non-player
/// GUID, or a synthetic fixture built from the writer rather than a capture.
///
/// Worth stating plainly because the first version of this fixture asserted only
/// that the GUID was non-empty, and was described as a real detector on the
/// strength of catching one length-changing mutation. Measuring the whole
/// mutation space is what turns that claim into a number.
///
/// This fixture is here because the shipped reader was wrong in THREE ways at
/// once -- bit order, the position of the note length, and the byte order -- and
/// every one of those leaves a plausible-looking packet. A test that restated
/// the corrected derivation would have caught none of them; running the real
/// parse over real bytes catches all three, and the flag polarity besides.
static void test_guild_set_note_parses_retail_bodies()
{
    struct Case
    {
        std::vector<uint8> body;
        char const* note;
        bool isPublic;
        uint64 targetGuid;
    };

    Case const cases[] =
    {
        {
            { 0x83, 0xBA, 0x80, 0xC9, 0x44, 0x50, 0x53, 0x20,
              0x35, 0x37, 0x31, 0xE9, 0x05, 0x04, 0x3D },
            "DPS 571", true, UINT64_C(0x04000000053CC8E8)
        },
        {
            { 0x86, 0x3A, 0x80, 0x1C, 0x52, 0x65, 0x73, 0x74,
              0x6F, 0x20, 0x35, 0x37, 0x30, 0x20, 0x49, 0x4C,
              0xB8, 0x04, 0x04, 0x8A },
            "Resto 570 IL", true, UINT64_C(0x05000000058B1DB9)
        },
    };

    for (Case const& c : cases)
    {
        WorldPacket in(CMSG_GUILD_SET_NOTE, c.body.size());
        Append(in, c.body);

        ObjectGuid targetGuid;
        bool isPublic = false;
        std::string note;
        MopGuildPackets::ParseGuildSetNote(in, targetGuid, isPublic, note);

        CHECK(note == c.note);
        CHECK(isPublic == c.isPublic);
        // The decisive check: a wrong bit order or length position leaves bytes
        // unread or overruns, and neither shows up in the note alone.
        CHECK(in.rpos() == in.size());
        // And the GUID must be the RIGHT one, not merely present. Review showed
        // that transposing the byte order <0,7> to <7,0> still produced the
        // correct note and polarity, still consumed 15 of 15 bytes, and still
        // left a non-empty GUID -- just the wrong player. Asserting non-empty
        // was worthless against exactly the defect class this fixture exists to
        // catch.
        CHECK(targetGuid.GetRawValue() == c.targetGuid);
    }
}


/// SMSG_GUILD_EVENT_LOG entry layout, with DISTINCT EventType and NewRank.
///
/// Those two single bytes were assigned the wrong way round when this packet was
/// first rebuilt -- by position relative to the uint32 -- and only the client's
/// consumer settles them: GetGuildEventInfo switches on the one from record +20
/// to yield invite/join/promote/demote/remove/quit, and passes the one from +21
/// to the rank-name lookup. A fixture using equal values could not have caught
/// that, so this one deliberately uses 3 and 7.
///
/// Every byte is exact: the elapsed time is a parameter of the inline builder
/// rather than time(NULL) - TimeStamp, which no fixture could pin.
static void test_guild_event_log_entry_layout()
{
    WorldPacket data(SMSG_GUILD_EVENT_LOG, 0);
    ByteBuffer buffer;
    data.WriteBits(1, 21);                                  // one entry
    // HIGHGUID_PLAYER is 0, so the raw values are the low parts unchanged.
    MopGuildPackets::BuildGuildEventLogEntry(data, buffer, 3,
        UINT64_C(0x0A0B0C0D), UINT64_C(0x01020304), 7, 0x11223344);
    data.FlushBits();
    data.append(buffer);

    static uint8 const expected[] =
    {
        0x00, 0x00, 0x0A, 0xE3, 0x50,                       // 21-bit count + 16 mask bits
        0x0A, 0x03, 0x05, 0x0B, 0x03, 0x0C,                 // g1[2], EventType, g2[0], g1[3], g2[2], g1[0]
        0x44, 0x33, 0x22, 0x11,                             // elapsed, now a parameter
        0x0D, 0x02, 0x07, 0x00,                             // g1[1], g2[1], NewRank, g2[3]
    };

    CHECK(data.size() == sizeof(expected));
    if (data.size() != sizeof(expected))
    {
        return;
    }

    for (size_t i = 0; i < sizeof(expected); ++i)
    {
        CHECK(data.contents()[i] == expected[i]);
    }
}

// capture-000015 seq 1103, the whole 7-byte body. Six mask bits set, six guid
// bytes, and the reader must land exactly on the end of the packet.
static void test_guild_party_state_request()
{
    {
        std::vector<uint8> const capture = { 0xBE, 0xFE, 0xF5, 0x24, 0x88, 0x1E, 0x00 };
        WorldPacket packet(CMSG_GUILD_REQUEST_PARTY_STATE, capture.size());
        Append(packet, capture);

        uint64 const guildGuid = MopGuildPackets::ReadGuildRequestPartyState(packet);

        CHECK(guildGuid == 0x1FF4000001FF2589ULL);
        CHECK(packet.rpos() == capture.size());
    }
    {
        // capture-000020 seq 2102, the eight-byte form. Seven mask bits, and
        // unlike the body above this one carries guid byte 5.
        std::vector<uint8> const capture = { 0xFE, 0x47, 0x81, 0xF0, 0xC0, 0xE6, 0x1E, 0x03 };
        WorldPacket packet(CMSG_GUILD_REQUEST_PARTY_STATE, capture.size());
        Append(packet, capture);

        uint64 const guildGuid = MopGuildPackets::ReadGuildRequestPartyState(packet);

        CHECK(guildGuid == 0x1FF180000246C1E7ULL);
        CHECK(packet.rpos() == capture.size());
    }
}

// The two decoded retail replies. Both are 13 bytes; the trailing 0x80 is the
// flag written as one MSB-first bit and flushed.
static void test_guild_party_state_response()
{
    {
        // capture-000072 seq 5140: required 12, no multiplier, one guild member
        // present, so not a guild group.
        WorldPacket data;
        MopGuildPackets::BuildGuildPartyState(data, 12, 0.0f, 1, false);

        static uint8 const expected[] =
        {
            0x0C, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x00,
        };

        CHECK(uint32(data.GetOpcode()) == 0x0A78u);
        CHECK(data.size() == sizeof(expected));
        if (data.size() == sizeof(expected))
        {
            for (size_t i = 0; i < sizeof(expected); ++i)
            {
                CHECK(data.contents()[i] == expected[i]);
            }
        }
    }
    {
        // capture-000015 seq 3122: required 2, multiplier 1.0f, two present, is
        // a guild group.
        WorldPacket data;
        MopGuildPackets::BuildGuildPartyState(data, 2, 1.0f, 2, true);

        static uint8 const expected[] =
        {
            0x02, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F,
            0x02, 0x00, 0x00, 0x00,
            0x80,
        };

        CHECK(data.size() == sizeof(expected));
        if (data.size() == sizeof(expected))
        {
            for (size_t i = 0; i < sizeof(expected); ++i)
            {
                CHECK(data.contents()[i] == expected[i]);
            }
        }
    }
}

// Rebuild capture-000009 seq 30430 exactly. The handler ships zeros because
// this core has no challenges, so this is what proves the group ORDER is
// retail's: feed the five decoded groups in and the 120 bytes must come back.
static void test_guild_challenge_update_matches_capture()
{
    uint32 const maxCount[GUILD_CHALLENGE_TYPES]     = { 0, 7, 1, 3, 15, 3 };
    uint32 const gold[GUILD_CHALLENGE_TYPES]         = { 0, 125, 500, 250, 125, 250 };
    uint32 const maxGold[GUILD_CHALLENGE_TYPES]      = { 0, 250, 1000, 500, 250, 500 };
    uint32 const xp[GUILD_CHALLENGE_TYPES]           = { 0, 300000, 3000000, 1500000, 50000, 1000000 };
    uint32 const currentCount[GUILD_CHALLENGE_TYPES] = { 0, 3, 1, 0, 15, 0 };

    WorldPacket data;
    MopGuildPackets::BuildGuildChallengeUpdate(data, maxCount, gold, maxGold, xp, currentCount);

    static uint8 const expected[] =
    {
        0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00, 0xFA, 0x00, 0x00, 0x00, 0x7D, 0x00, 0x00, 0x00, 0xFA, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xFA, 0x00, 0x00, 0x00, 0xE8, 0x03, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00, 0xFA, 0x00, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xE0, 0x93, 0x04, 0x00, 0xC0, 0xC6, 0x2D, 0x00, 0x60, 0xE3, 0x16, 0x00, 0x50, 0xC3, 0x00, 0x00, 0x40, 0x42, 0x0F, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    CHECK(uint32(data.GetOpcode()) == 0x0AE9u);
    CHECK(data.size() == sizeof(expected));
    if (data.size() == sizeof(expected))
    {
        for (size_t i = 0; i < sizeof(expected); ++i)
        {
            CHECK(data.contents()[i] == expected[i]);
        }
    }
}

// SMSG_GUILD_INVITE against capture-000499 seq 777, the whole 65-byte body.
//
// This guild has borderColor and emblemColor both 14, so those two alone could
// be transposed without changing a byte. They are not guessed here: the colour
// assignment comes from the client's consumer sub_9683C3 and the tabard resolver
// sub_831870, and the fixture is written to agree with it. What the fixture
// itself pins is the layout and the three name lengths (7/6/7).
//
// The inviter name is six UTF-8 bytes for five glyphs, which is the case that
// makes a byte length rather than a character count observable.
static void test_guild_invite_matches_capture()
{
    static uint8 const expected[] = {
        0x0C, 0x42, 0x06, 0xD0, 0x10, 0x31, 0x0E, 0x00, 0x00, 0x00, 0x44, 0xC3,
        0xB8, 0x64, 0x67, 0x65, 0x00, 0x00, 0x00, 0x00, 0x8F, 0x13, 0x0E, 0x00,
        0x00, 0x00, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1E, 0x03,
        0x2D, 0x00, 0x00, 0x00, 0x44, 0x69, 0x76, 0x69, 0x6E, 0x65, 0x20, 0x57,
        0x72, 0x61, 0x74, 0x68, 0x18, 0x00, 0x01, 0x03, 0xA4, 0x00, 0x00, 0x00,
        0x18, 0x00, 0x01, 0x03, 0xF5
    };

    WorldPacket packet;
    CHECK(MopGuildPackets::BuildGuildInvite(packet,
        UI64LIT(0x1FF400000212308E),         // new guild guid
        UI64LIT(0),                          // no old guild
        std::string("D\xC3\xB8" "dge"),      // six bytes, five glyphs
        std::string("Divine Wrath"),
        std::string(),                       // empty old guild name
        10u,                                 // guild level
        164u, 14u,                           // emblem style, colour
        0u, 14u,                             // border style, colour
        45u,                                 // background colour
        0x03010018u, 0u, 0x03010018u));      // new realm, old realm, inviter realm
    CHECK(Equal(packet, std::vector<uint8>(expected, expected + sizeof(expected))));

    // A name longer than the 6-bit length field must be refused, not truncated
    // into a body the client would misparse.
    WorldPacket tooLong;
    CHECK(!MopGuildPackets::BuildGuildInvite(tooLong, UI64LIT(1), UI64LIT(0),
        std::string(64, 'A'), std::string("G"), std::string(), 1u,
        0u, 0u, 0u, 0u, 0u, 1u, 0u, 1u));

    CHECK(uint32(SMSG_GUILD_INVITE) == 0x0F71u);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_short_motd();
    test_guild_invite_matches_capture();
    test_guild_invite_request();
    test_guild_achievement_tracking_request();
    test_guild_query_request();
    test_guild_query_response();
    test_guild_set_note_parses_retail_bodies();
    test_guild_event_log_entry_layout();
    test_guild_party_state_request();
    test_guild_party_state_response();
    test_guild_challenge_update_matches_capture();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_guild_packets: all checks passed\n");
    return 0;
}
