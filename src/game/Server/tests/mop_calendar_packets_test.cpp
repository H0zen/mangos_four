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
 * Independent byte fixtures for the 5.4.8.18414 calendar updates.
 */

#include "Calendar.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "packet size %zu, expected %zu\n",
            packet.size(), expected.size());
        for (size_t i = 0; i < packet.size(); ++i)
            std::fprintf(stderr, "%s0x%02X", i ? "," : "", packet.contents()[i]);
        std::fprintf(stderr, "\n");
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "packet byte %zu is 0x%02X, expected 0x%02X\n",
                i, packet.contents()[i], expected[i]);
            return false;
        }
    }
    return true;
}






static void test_populated_calendar_list()
{
    MopCalendarPackets::CalendarListEvent event;
    event.creatorGuid = 0x0807060504030201ULL;
    event.guildGuid = 0x1817161514131211ULL;
    event.title = "E";
    event.dungeonId = -2;
    event.eventTime = 0x11223344u;
    event.flags = 0xA1B2C3D4u;
    event.eventId = 0x8877665544332211ULL;
    event.type = 5;

    MopCalendarPackets::CalendarListInvite invite;
    invite.senderGuid = 0x3837363534333231ULL;
    invite.inviteId = 0x0102030405060708ULL;
    invite.status = 6;
    invite.eventId = event.eventId;
    invite.rank = 2;
    invite.guildEvent = true;

    MopCalendarPackets::CalendarListLockout lockout;
    lockout.instanceGuid = 0x2827262524232221ULL;
    lockout.difficulty = 3;
    lockout.resetRemaining = 0x01020304u;
    lockout.mapId = 0x05060708u;

    MopCalendarPackets::CalendarListReset reset;
    reset.mapId = 0x11121314u;
    reset.resetRemaining = 0x21222324u;
    reset.offset = -1;

    WorldPacket packet(SMSG_CALENDAR_SEND_CALENDAR, 128);
    CHECK(MopCalendarPackets::BuildCalendarList(packet, { event }, { invite },
        { lockout }, { reset }, 0x31323334u, 0x41424344u));
    CHECK(Equal(packet, {
        0x00,0x00,0x10,0x00,0x00,0x00,0x01,0xFF,0x00,0x00,0x3F,0xE0,0x00,0x07,0xFE,0x03,
        0xFC,0x07,0x15,0x45,0x19,0xFE,0xFF,0xFF,0xFF,0x00,0x04,0x12,0x09,0x02,0x44,0x33,
        0x22,0x11,0x05,0x03,0x16,0x13,0x06,0xD4,0xC3,0xB2,0xA1,0x14,0x17,0x10,0x11,0x22,
        0x33,0x44,0x55,0x66,0x77,0x88,0x05,0x03,0x00,0x00,0x00,0x25,0x20,0x23,0x27,0x04,
        0x03,0x02,0x01,0x08,0x07,0x06,0x05,0x22,0x29,0x26,0x24,0x32,0x08,0x07,0x06,0x05,
        0x04,0x03,0x02,0x01,0x06,0x36,0x35,0x34,0x33,0x30,0x11,0x22,0x33,0x44,0x55,0x66,
        0x77,0x88,0x39,0x37,0x02,0x01,0x34,0x33,0x32,0x31,0x14,0x13,0x12,0x11,0x24,0x23,
        0x22,0x21,0xFF,0xFF,0xFF,0xFF,0xF0,0x37,0xB2,0x43,0x44,0x43,0x42,0x41
    }));
}



static void test_populated_selected_event()
{
    MopCalendarPackets::CalendarEventInvite invite;
    invite.inviteeGuid = 0x0807060504030201ULL;
    invite.statusTime = 0x11121314u;
    invite.guildEvent = true;
    invite.text = "I";
    invite.level = 0x55;
    invite.rank = 2;
    invite.inviteId = 0x8877665544332211ULL;
    invite.status = 6;

    MopCalendarPackets::CalendarEventDetails event;
    event.creatorGuid = 0x1817161514131211ULL;
    event.guildGuid = 0x2827262524232221ULL;
    event.title = "T";
    event.description = "D";
    event.flags = 0xA1B2C3D4u;
    event.eventTime = 0x21222324u;
    event.dungeonId = -2;
    event.type = 5;
    event.eventId = 0x0102030405060708ULL;
    event.sendType = 2;

    WorldPacket packet(SMSG_CALENDAR_SEND_EVENT, 80);
    CHECK(MopCalendarPackets::BuildCalendarEvent(packet, event, { invite }));
    CHECK(Equal(packet, {
        0x00,0x00,0x1F,0xE0,0x30,0x1F,0xF0,0x03,0xFE,0x14,0x13,0x12,0x11,0x07,0x01,0x03,
        0x02,0x06,0x49,0x55,0x09,0x02,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x00,0x05,
        0x04,0x06,0x10,0x17,0xD4,0xC3,0xB2,0xA1,0x23,0x19,0x15,0x24,0x23,0x22,0x21,0x26,
        0x14,0xFE,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x24,0x05,0x54,0x16,0x25,0x12,0x29,
        0x13,0x44,0x22,0x27,0x20,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x02
    }));
}



static void test_raid_lockout_removed_matches_retail_bodies()
{
    // These retained 18414 bodies cover both observed masks and consume
    // exactly. Their scalar values also rule out the inherited map/difficulty
    // order: 580 and 550 are map IDs, so 4 occupies the difficulty field. It
    // is a retail value, not one this tree's raid Difficulty enum produces.
    WorldPacket dense(SMSG_CALENDAR_RAID_LOCKOUT_REMOVED, 17);
    MopCalendarPackets::BuildCalendarRaidLockoutRemoved(
        dense, 4, 580, UINT64_C(0x1F4400001048C8DD));
    CHECK(dense.GetOpcode() == SMSG_CALENDAR_RAID_LOCKOUT_REMOVED);
    CHECK(Equal(dense, {
        0x04, 0x00, 0x00, 0x00, 0x44, 0x02, 0x00, 0x00,
        0xD7, 0x45, 0xC9, 0x1E, 0x11, 0xDC, 0x49,
    }));

    WorldPacket sparse(SMSG_CALENDAR_RAID_LOCKOUT_REMOVED, 17);
    MopCalendarPackets::BuildCalendarRaidLockoutRemoved(
        sparse, 4, 550, UINT64_C(0x1F440000102C00F6));
    CHECK(sparse.GetOpcode() == SMSG_CALENDAR_RAID_LOCKOUT_REMOVED);
    CHECK(Equal(sparse, {
        0x04, 0x00, 0x00, 0x00, 0x26, 0x02, 0x00, 0x00,
        0xD6, 0x45, 0x1E, 0x11, 0xF7, 0x2D,
    }));

    // This tree encodes HIGHGUID_INSTANCE 0x1F4 in the upper 12 bits, yielding
    // 0x1F40... rather than retail's observed 0x1F44.... Pin the actual shape
    // the live CalendarHandler can emit without pretending it is retail proof.
    WorldPacket local(SMSG_CALENDAR_RAID_LOCKOUT_REMOVED, 17);
    MopCalendarPackets::BuildCalendarRaidLockoutRemoved(
        local, 1, 580, UINT64_C(0x1F4000001048C8DD));
    CHECK(Equal(local, {
        0x01, 0x00, 0x00, 0x00, 0x44, 0x02, 0x00, 0x00,
        0xD7, 0x41, 0xC9, 0x1E, 0x11, 0xDC, 0x49,
    }));
}


/// SMSG_CALENDAR_EVENT_INVITE against two real 18414 bodies -- capture-000444
/// seq 262179 and capture-000696 seq 290114, catalogue 2BE10C89. Both are 30 bytes
/// and both are consumed exactly by the client's reader sub_6C3312.
///
/// These also NAME the fields, which is why they are worth keeping as a fixture
/// rather than a size check: level reads 90, the eventId slot is identical across
/// two packets sent to different invitees, and status reads 6 (SIGNED_UP) and 8
/// (TENTATIVE) with the sender-differs bit clear -- exactly a self sign-up.
///
/// Note the status time sits INSIDE the GUID byte run, between byte 5 and byte 2.
/// A serializer that appends it after the GUID produces the same length and the
/// same bytes for an empty GUID, so length alone cannot catch that mistake.
static void test_event_invite_matches_retail_bodies()
{
    struct Case
    {
        uint64 inviteId;
        uint8 status;
        uint8 level;
        uint64 eventId;
        uint64 invitee;
        uint32 statusTime;
        uint8 const expected[30];
    };

    static Case const cases[] =
    {
        {
            UINT64_C(0x0000000002FC6663), 8, 90, UINT64_C(0x0000000000985C13),
            UINT64_C(0x050000000572B573), 240524531,
            {
                0x01, 0x08, 0x63, 0x66, 0xFC, 0x02, 0x00, 0x00, 0x00, 0x00,
                0x5A, 0x13, 0x5C, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E,
                0x00, 0x04, 0x72, 0xF3, 0x1C, 0x56, 0x0E, 0x73, 0x04, 0xB4,
            }
        },
        {
            UINT64_C(0x0000000002FC60A5), 6, 90, UINT64_C(0x0000000000985C13),
            UINT64_C(0x05000000058AAD38), 240524432,
            {
                0x01, 0x06, 0xA5, 0x60, 0xFC, 0x02, 0x00, 0x00, 0x00, 0x00,
                0x5A, 0x13, 0x5C, 0x98, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E,
                0x00, 0x04, 0x39, 0x90, 0x1C, 0x56, 0x0E, 0x8B, 0x04, 0xAC,
            }
        },
    };

    for (Case const& c : cases)
    {
        ObjectGuid const invitee(c.invitee);
        WorldPacket data(SMSG_CALENDAR_EVENT_INVITE, 30);
        data << uint8(1);                                   // !preInvite
        data << uint8(c.status);
        data << uint64(c.inviteId);
        data << uint8(c.level);
        data << uint64(c.eventId);
        data.WriteGuidMask<6, 4, 1, 3, 7, 0, 2, 5>(invitee);
        data.WriteBit(0);                                   // preInvite: status time present
        data.WriteBit(0);                                   // sender == invitee
        data.FlushBits();
        data.WriteGuidBytes<7, 0, 5>(invitee);
        data << uint32(c.statusTime);
        data.WriteGuidBytes<2, 3, 4, 1, 6>(invitee);

        CHECK(data.size() == sizeof(c.expected));
        if (data.size() == sizeof(c.expected))
        {
            CHECK(std::memcmp(data.contents(), c.expected, sizeof(c.expected)) == 0);
        }
    }
}

/// SMSG_CALENDAR_EVENT_INVITE_ALERT against two of the six real 18414 bodies:
/// capture-000163 seq 229424 (66 bytes, a guild announcement — guild GUID set,
/// inviteId 0, type OTHER) and capture-000601 seq 1037992 (50 bytes, a personal
/// invite — inviteId set, no guild GUID, type RAID with a real dungeon).
///
/// The pair is chosen deliberately: they exercise both arms of the GUID presence
/// mask. All six captures decode exactly under this layout; these two are the ones
/// that would catch a mask/byte transposition, because a body with an absent guild
/// GUID writes eight fewer bytes and would still "look" plausible.
static void test_event_invite_alert_matches_retail_bodies()
{
    struct Case
    {
        uint64 eventId;
        int32 dungeonId;
        uint8 type;
        uint64 inviteId;
        uint32 flags;
        uint8 status;
        uint32 eventTime;
        uint8 rank;
        uint64 creator;
        uint64 guild;
        uint64 sender;
        char const* title;
        size_t len;
        uint8 const expected[70];
    };

    static Case const cases[] =
    {
        {   // capture-000163 seq 229424 — guild announcement
            UINT64_C(0x00000000008751AA), 531, 0, 0, 0x400, 7, 0x0E7654DE, 0,
            UINT64_C(0x040000000017208D), UINT64_C(0x1FF4000001FF2589), UINT64_C(0x040000000017208D),
            "Pro dobrou guildy", 66,
            {
                0xAA,0x51,0x87,0x00,0x00,0x00,0x00,0x00, 0x13,0x02,0x00,0x00, 0x00,
                0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x04,0x00,0x00, 0x07,
                0xDE,0x54,0x76,0x0E, 0x00,
                0x95,0xDA,0xAC,0x45, 0xF5,0x88,
                0x50,0x72,0x6F,0x20,0x64,0x6F,0x62,0x72,0x6F,0x75,0x20,0x67,0x75,0x69,0x6C,0x64,0x79,
                0x8C,0x21,0xFE,0x1E,0x24,0x16,0x05,0x8C,0x16,0x05,0x00,0x21,
            }
        },
        {   // capture-000601 seq 1037992 — personal invite, no guild GUID
            UINT64_C(0x0000000000851300), 714, 0, UINT64_C(0x00000000031E99C1), 0x1, 0, 0x0E7494DE, 0,
            UINT64_C(0x04000000053D19D9), 0, UINT64_C(0x04000000053D19D9),
            "R2 HC", 50,
            {
                0x00,0x13,0x85,0x00,0x00,0x00,0x00,0x00, 0xCA,0x02,0x00,0x00, 0x00,
                0xC1,0x99,0x1E,0x03,0x00,0x00,0x00,0x00, 0x01,0x00,0x00,0x00, 0x00,
                0xDE,0x94,0x74,0x0E, 0x00,
                0x0D,0x82,0xB4,0x15,
                0x52,0x32,0x20,0x48,0x43,
                0xD8,0x18,0x3C,0x04,0x05,0xD8,0x04,0x3C,0x05,0x18,
            }
        },
    };

    for (Case const& c : cases)
    {
        ObjectGuid const creator(c.creator), guild(c.guild), sender(c.sender);
        std::string const title(c.title);

        WorldPacket data(SMSG_CALENDAR_EVENT_INVITE_ALERT, c.len);
        data << uint64(c.eventId);
        data << int32(c.dungeonId);
        data << uint8(c.type);
        data << uint64(c.inviteId);
        data << uint32(c.flags);
        data << uint8(c.status);
        data << uint32(c.eventTime);
        data << uint8(c.rank);

        data.WriteGuidMask<7>(guild);   data.WriteGuidMask<6>(sender);
        data.WriteGuidMask<4>(guild);   data.WriteGuidMask<0>(guild);
        data.WriteGuidMask<3>(sender);  data.WriteGuidMask<1>(creator);
        data.WriteGuidMask<5>(guild);   data.WriteGuidMask<2>(sender);
        data.WriteGuidMask<0>(sender);  data.WriteGuidMask<1>(guild);
        data.WriteGuidMask<5>(sender);  data.WriteGuidMask<6>(guild);
        data.WriteGuidMask<3>(guild);   data.WriteGuidMask<6>(creator);
        data.WriteGuidMask<2>(creator); data.WriteGuidMask<4>(sender);
        data.WriteGuidMask<7>(sender);  data.WriteGuidMask<4>(creator);
        data.WriteGuidMask<1>(sender);  data.WriteGuidMask<3>(creator);
        data.WriteGuidMask<2>(guild);   data.WriteGuidMask<0>(creator);
        data.WriteBits(title.size(), 8);
        data.WriteGuidMask<5>(creator); data.WriteGuidMask<7>(creator);
        data.FlushBits();

        data.WriteGuidBytes<5>(sender); data.WriteGuidBytes<6>(guild);
        data.WriteGuidBytes<0>(guild);  data.WriteGuidBytes<6>(sender);
        data.WriteGuidBytes<5>(creator); data.WriteGuidBytes<4>(creator);
        data.append(title.data(), title.size());
        data.WriteGuidBytes<0>(sender); data.WriteGuidBytes<1>(sender);
        data.WriteGuidBytes<2>(guild);  data.WriteGuidBytes<7>(guild);
        data.WriteGuidBytes<6>(creator); data.WriteGuidBytes<1>(guild);
        data.WriteGuidBytes<2>(sender); data.WriteGuidBytes<3>(creator);
        data.WriteGuidBytes<7>(creator); data.WriteGuidBytes<0>(creator);
        data.WriteGuidBytes<3>(sender); data.WriteGuidBytes<2>(creator);
        data.WriteGuidBytes<7>(sender); data.WriteGuidBytes<5>(guild);
        data.WriteGuidBytes<4>(guild);  data.WriteGuidBytes<3>(guild);
        data.WriteGuidBytes<1>(creator); data.WriteGuidBytes<4>(sender);

        CHECK(data.size() == c.len);
        if (data.size() == c.len)
        {
            CHECK(std::memcmp(data.contents(), c.expected, c.len) == 0);
        }
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    test_populated_calendar_list();
    test_populated_selected_event();
    test_raid_lockout_removed_matches_retail_bodies();
    test_event_invite_matches_retail_bodies();
    test_event_invite_alert_matches_retail_bodies();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_calendar_packets: all checks passed\n");
    return 0;
}
