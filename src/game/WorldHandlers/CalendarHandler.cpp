/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#include "Common.h"
#include "Log.h"
#include "Player.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "DBCStores.h"
#include "MapPersistentStateMgr.h"
#include "Calendar.h"
#include "ObjectMgr.h"
#include "SocialMgr.h"
#include "World.h"
#include "Guild.h"
#include "GuildMgr.h"

void WorldSession::HandleCalendarGetCalendar(WorldPacket& /*recv_data*/)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_GET_CALENDAR [%s]", guid.GetString().c_str());

    time_t currTime = time(NULL);

    CalendarInvitesList invites;
    sCalendarMgr.GetPlayerInvitesList(guid, invites);
    std::vector<MopCalendarPackets::CalendarListInvite> inviteRecords;
    inviteRecords.reserve(invites.size());
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Sending > %zu invites", invites.size());

    for (CalendarInvite const* invite : invites)
    {
        CalendarEvent const* event = invite->GetCalendarEvent();
        MANGOS_ASSERT(event);                           // TODO: be sure no way to have a null event

        MopCalendarPackets::CalendarListInvite record;
        record.senderGuid = invite->SenderGuid.GetRawValue();
        record.inviteId = invite->InviteId;
        record.status = invite->Status;
        record.eventId = event->EventId;
        record.rank = invite->Rank;
        record.guildEvent = event->IsGuildEvent() &&
            event->GuildId == _player->GetGuildId();
        inviteRecords.push_back(record);

        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "invite> EventId[" UI64FMTD "], InviteId[" UI64FMTD "], status[%u], rank[%u]",
                         event->EventId, invite->InviteId, uint32(invite->Status), uint32(invite->Rank));
    }

    CalendarEventsList events;
    sCalendarMgr.GetPlayerEventsList(guid, events);
    std::vector<MopCalendarPackets::CalendarListEvent> eventRecords;
    eventRecords.reserve(events.size());
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Sending > %zu events", events.size());

    for (CalendarEvent const* event : events)
    {
        MopCalendarPackets::CalendarListEvent record;
        record.creatorGuid = event->CreatorGuid.GetRawValue();
        if (Guild* guild = sGuildMgr.GetGuildById(event->GuildId))
            record.guildGuid = guild->GetObjectGuid().GetRawValue();
        record.title = event->Title;
        record.dungeonId = event->DungeonId;
        record.eventTime = secsToTimeBitFields(event->EventTime);
        record.flags = event->Flags;
        record.eventId = event->EventId;
        record.type = event->Type;
        eventRecords.push_back(record);

        std::string timeStr = TimeToTimestampStr(event->EventTime);
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Events> EventId[" UI64FMTD "], Title[%s], Time[%s], Type[%u],  Flag[%u], DungeonId[%d], CreatorGuid[%s]",
                         event->EventId, event->Title.c_str(), timeStr.c_str(), uint32(event->Type),
                         uint32(event->Flags), event->DungeonId, event->CreatorGuid.GetString().c_str());
    }

    std::vector<MopCalendarPackets::CalendarListLockout> lockoutRecords;
    for (uint8 i = 0; i < MAX_DIFFICULTY; ++i)
    {
        Player::BoundInstancesMap boundInstances = _player->GetBoundInstances(Difficulty(i));
        for (auto const& bound : boundInstances)
        {
            if (!bound.second.perm)
                continue;

            DungeonPersistentState const* state = bound.second.state;
            MopCalendarPackets::CalendarListLockout record;
            record.instanceGuid = state->GetInstanceGuid().GetRawValue();
            // RAW client DifficultyID on the wire, map-aware -- see ToClientDifficultyForMap.
            // The bind is stored under the internal mode, and this record names its map.
            //
            // isRaid derived rather than hardcoded true: this loop walks m_boundInstances across
            // every internal mode, so it can carry a five-man bind as well as a raid one. It only
            // matters on the fallback path, where the map has no row for the tier, but a
            // hardcoded true would answer from the raid table for a dungeon.
            MapEntry const* stateMap = state->GetMapEntry();
            record.difficulty = ToClientDifficultyForMap(state->GetMapId(), state->GetDifficulty(),
                                                        stateMap && stateMap->IsRaid());
            record.resetRemaining = state->GetResetTime() > currTime ?
                uint32(state->GetResetTime() - currTime) : 0;
            record.mapId = state->GetMapId();
            lockoutRecords.push_back(record);
        }
    }

    std::vector<MopCalendarPackets::CalendarListReset> resetRecords;
    std::set<uint32> sentMaps;
    for (auto const& mapDifficulty : sMapDifficultyMap)
    {
        uint32 map_diff_pair = mapDifficulty.first;
        uint32 mapId = PAIR32_LOPART(map_diff_pair);
        MapDifficultyEntry const* mapDiff = mapDifficulty.second;

        // skip mapDiff without global reset time
        if (!mapDiff->RaidDuration)
            continue;

        // skip non raid map
        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
        if (!mapEntry || !mapEntry->IsRaid())
            continue;

        // skip already sent map (not same difficulty?)
        if (!sentMaps.insert(mapId).second)
            continue;

        // GetMaxResetTimeFor returns a DURATION - RaidDuration scaled and
        // floored to whole days - not an absolute reset timestamp. It was
        // being compared against currTime, which a duration can never exceed,
        // so every record went out as zero. The client uses this word as the
        // recurrence period: it steps from the calendar epoch by this value,
        // and divides by it. Retail carries 604800 for the weekly raids and
        // 259200 for map 509, which is what the IsRaid filter above admits,
        // and never zero.
        uint32 resetPeriod = sMapPersistentStateMgr.GetScheduler().GetMaxResetTimeFor(mapDiff);
        MopCalendarPackets::CalendarListReset record;
        record.mapId = int32(mapId);
        // Rate.InstanceResetTime is only validated against being negative, so
        // a large enough rate makes the scaled duration exceed INT32_MAX and
        // the cast wrap negative. The client divides by this word, so a
        // negative divisor is worse than an implausibly distant reset.
        uint32 const maxResetPeriod = 0x7FFFFFFF;
        record.resetRemaining =
            int32(resetPeriod > maxResetPeriod ? maxResetPeriod : resetPeriod);
        // Retail sends zero here for every raid but one, which carries a
        // seconds-into-the-day reset offset, so zero is a legal value rather
        // than an absent one. We do not model per-map reset hours yet.
        record.offset = 0;
        resetRecords.push_back(record);
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "MapId [%u] -> Reset period: %d", mapId, record.resetRemaining);
    }
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Map sent [%zu]", resetRecords.size());

    WorldPacket data(SMSG_CALENDAR_SEND_CALENDAR);
    if (!MopCalendarPackets::BuildCalendarList(data, eventRecords, inviteRecords, lockoutRecords, resetRecords,
        secsToTimeBitFields(currTime), uint32(currTime)))
    {
        sLog.outError("Calendar list exceeds the 5.4.8 wire bounds for player %s", guid.GetString().c_str());
        return;
    }
    SendPacket(&data);
}

void WorldSession::HandleCalendarGetEvent(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_GET_EVENT [%s]", guid.GetString().c_str());

    if (recv_data.size() < sizeof(uint64))
    {
        sLog.outError("CMSG_CALENDAR_GET_EVENT from %s is truncated (%zu bytes)",
            guid.GetString().c_str(), recv_data.size());
        return;
    }

    uint64 eventId;
    recv_data >> eventId;

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        sCalendarMgr.SendCalendarEvent(_player, event, CALENDAR_SENDTYPE_GET);
    }
    else
    {
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Calendar event " UI64FMTD " does not exist; legacy command-result body remains gated", eventId);
    }
}

void WorldSession::HandleCalendarGuildFilter(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_GUILD_FILTER [%s]", _player->GetGuidStr().c_str());

    // Three BYTES, not three uint32s. The client's writer sub_668A7F emits
    // sub_40F018 three times from object offsets +16, +17 and +18, so the whole
    // body is three bytes. The inherited reader asked for twelve and would have
    // over-read the buffer on every request.
    //
    // The widths are also right on their own terms: all three are a character
    // level or a guild rank index, and none can exceed 255.
    uint8 minLevel;
    uint8 maxLevel;
    uint8 minRank;

    recv_data >> minLevel >> maxLevel >> minRank;

    if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId()))
    {
        guild->MassInviteToEvent(this, minLevel, maxLevel, minRank);
    }

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Min level [%u], Max level [%u], Min rank [%u]", minLevel, maxLevel, minRank);
}

void WorldSession::HandleCalendarEventSignup(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_SIGNUP [%s]", guid.GetString().c_str());

    uint64 eventId;

    // The tentative flag is a single BIT, not a byte. The client's writer
    // sub_6665D7 emits the uint64 and then one sub_665157 followed by a flush, so
    // the trailing byte is 0x80 for true and 0x00 for false. Reading it as a uint8
    // yielded 128, which is truthy by luck rather than by parse -- any comparison
    // against 1 would have failed silently.
    recv_data >> eventId;
    bool const tentative = recv_data.ReadBit();
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "] Tentative %u", eventId, uint32(tentative));

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        if (event->IsGuildEvent() && event->GuildId != _player->GetGuildId())
        {
            sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_GUILD_PLAYER_NOT_IN_GUILD);
            return;
        }

        CalendarInviteStatus status = tentative ? CALENDAR_STATUS_TENTATIVE : CALENDAR_STATUS_SIGNED_UP;
        sCalendarMgr.AddInvite(event, guid, guid, CalendarInviteStatus(status), CALENDAR_RANK_PLAYER, "", time(NULL));
        sCalendarMgr.SendCalendarClearPendingAction(_player);
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_EVENT_INVALID);
    }
}

void WorldSession::HandleCalendarAddEvent(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_ADD_EVENT [%s]", guid.GetString().c_str());

    // Rebuilt from the client's writer sub_66D4E2. The field NAMES come from
    // CalendarAddEvent's own builder, sub_9E8EEB, which constructs this packet
    // object on the stack and fills it -- so each stack slot names the offset the
    // writer then serialises:
    //
    //     +1200 <- ui+1212   maxInvites  (the field checked against 100 for
    //                                     CALENDAR_ERROR_INVITES_EXCEEDED)
    //     +1180 <- ui+1280   flags
    //     +1172 <- ui+1296   dungeonId   (CalendarEventSetTextureID's target)
    //     +1176 <- packed date/time      eventTime
    //     +1170 <- ui+1204   type
    //     +16                title       (128 cap)
    //     +145               description (1024 cap)
    //     +1184/+1188        invite vector, 16-byte records: guid, status, rank
    //
    // Wire order is the writer's: the four uint32 then the type byte, a 22-bit
    // invite count and an 11-bit description length, ALL the invite GUID masks,
    // an 8-bit title length, then the per-invite bytes, then title, then
    // description. Both strings go LAST and neither is NUL-terminated.
    //
    // The inherited reader took title and description FIRST as cstrings and read
    // nine scalars including `repeatable` and a second packed time, neither of
    // which 5.4.8 sends.
    uint32 maxInvites;
    uint32 flags;
    int32 dungeonId;
    uint32 eventPackedTime;
    uint8 type;

    recv_data >> maxInvites;
    recv_data >> flags;
    recv_data >> dungeonId;
    recv_data >> eventPackedTime;
    recv_data >> type;

    uint32 const inviteCount = recv_data.ReadBits(22);
    uint32 const descriptionLength = recv_data.ReadBits(11);

    if (inviteCount > CALENDAR_MAX_INVITES)
    {
        sLog.outError("CMSG_CALENDAR_ADD_EVENT: %s sent %u invites, refusing",
                      guid.GetString().c_str(), inviteCount);
        recv_data.rfinish();
        return;
    }

    // The wire widths are wider than anything the client can legitimately produce:
    // 11 bits allows a description of 2047 where the client's own builder caps it
    // at 1024, and 8 bits allows a title of 255 against a cap of 128. That gap is
    // not cosmetic. Both strings are echoed straight back out in the invite and
    // update alerts, whose CLIENT readers (sub_6F4D55, sub_708569) copy them into
    // fixed stack-backed packet objects without checking. An oversized value would
    // therefore be reflected by this server into every recipient's client memory.
    if (descriptionLength > CALENDAR_MAX_DESCRIPTION_LEN)
    {
        sLog.outError("CMSG_CALENDAR_ADD_EVENT: %s sent a %u byte description, refusing",
                      guid.GetString().c_str(), descriptionLength);
        recv_data.rfinish();
        return;
    }

    std::vector<ObjectGuid> invitees(inviteCount);
    for (uint32 i = 0; i < inviteCount; ++i)
    {
        recv_data.ReadGuidMask<7, 2, 6, 3, 5, 1, 0, 4>(invitees[i]);
    }

    uint32 const titleLength = recv_data.ReadBits(8);

    if (titleLength > CALENDAR_MAX_TITLE_LEN)
    {
        sLog.outError("CMSG_CALENDAR_ADD_EVENT: %s sent a %u byte title, refusing",
                      guid.GetString().c_str(), titleLength);
        recv_data.rfinish();
        return;
    }

    std::vector<uint8> inviteStatus(inviteCount);
    std::vector<uint8> inviteRank(inviteCount);
    for (uint32 i = 0; i < inviteCount; ++i)
    {
        // The status byte sits between GUID byte 7 and byte 5, and the rank byte
        // after byte 5 -- the scalars are interleaved into the GUID sequence.
        recv_data.ReadGuidBytes<4, 2, 3, 1, 0, 6, 7>(invitees[i]);
        recv_data >> inviteStatus[i];
        recv_data.ReadGuidBytes<5>(invitees[i]);
        recv_data >> inviteRank[i];
    }

    // ReadString(count) returns a SHORT string if the body ends early rather than
    // failing, so prove the bytes are there before trusting either length.
    if (recv_data.rpos() + titleLength + descriptionLength > recv_data.size())
    {
        sLog.outError("CMSG_CALENDAR_ADD_EVENT: %s claimed %u+%u string bytes with %zu left, refusing",
                      guid.GetString().c_str(), titleLength, descriptionLength,
                      recv_data.size() - recv_data.rpos());
        recv_data.rfinish();
        return;
    }

    std::string const title = recv_data.ReadString(titleLength);
    std::string const description = recv_data.ReadString(descriptionLength);

    // 5.4.8 sends neither a repeat option nor a second packed time in this request.
    uint8 const repeatable = 0;
    uint32 const unkPackedTime = 0;

    // DECODE before comparing. The client sends a packed calendar bitfield, not a
    // timestamp: timeBitFieldsToSecs unpacks it, while LocalTimeToUTCTime merely
    // adds a timezone offset in seconds. Applying that offset to packed bits
    // corrupts the date, and comparing the packed value against GameTime compares
    // roughly 2.4e8 against roughly 1.7e9 -- the packed format cannot even reach
    // current Unix time, so EVERY request was rejected as "in the past" before it
    // did anything. The validation and any timezone policy belong on the decoded
    // value.
    time_t const eventTime = timeBitFieldsToSecs(eventPackedTime);

    // prevent events in the past
    if (eventTime < (GameTime::GetGameTime() - time_t(86400L)))
    {
        recv_data.rfinish();
        return;
    }

    // 946684800 is 01/01/2000 00:00:00 - default response time
    CalendarEvent* cal =  sCalendarMgr.AddEvent(_player->GetObjectGuid(), title, description, type, repeatable, maxInvites, dungeonId, eventTime, timeBitFieldsToSecs(unkPackedTime), flags);

    if (cal)
    {
        if (cal->IsGuildAnnouncement())
        {
            sCalendarMgr.AddInvite(cal, guid, ObjectGuid(uint64(0)),  CALENDAR_STATUS_NOT_SIGNED_UP, CALENDAR_RANK_PLAYER, "", time(NULL));
        }
        else
        {
            // The invites were already consumed above -- they are interleaved into
            // the bit stream and the GUID byte sequence, so they cannot be read
            // lazily here as the pre-MoP body allowed.
            for (uint32 i = 0; i < inviteCount; ++i)
            {
                sCalendarMgr.AddInvite(cal, guid, invitees[i],
                                       CalendarInviteStatus(inviteStatus[i]),
                                       CalendarModerationRank(inviteRank[i]), "", time(NULL));
            }
        }
        sCalendarMgr.SendCalendarEvent(_player, cal, CALENDAR_SENDTYPE_ADD);
    }
}

void WorldSession::HandleCalendarUpdateEvent(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_UPDATE_EVENT [%s]", guid.GetString().c_str());

    time_t oldEventTime;
    uint64 eventId;
    uint64 inviteId;
    std::string title;
    std::string description;
    uint8 type;
    uint8 repetitionType;
    uint32 maxInvites;
    int32 dungeonId;
    uint32 eventPackedTime;
    uint32 UnknownPackedTime;
    uint32 flags;

    // Rebuilt from the client's writer sub_66D0A3, with field names from
    // CalendarUpdateEvent's own builder sub_9E78EE, which fills this packet object
    // on its stack: +1200 maxInvites, +1188 dungeonId, +1192 eventTime,
    // +1196 flags, +1186 type, +16 eventId, +24 inviteId, +32 title (128),
    // +161 description (1024).
    //
    // Both GUIDs and both strings are INTERLEAVED. The two lengths sit inside the
    // GUID mask run, and eventId's bit 3 comes after them; the title is emitted
    // partway through the byte run and the description later still. The reads below
    // are in the writer's exact order and must stay that way.
    ObjectGuid eventGuid;
    ObjectGuid inviteGuid;

    recv_data >> maxInvites;
    recv_data >> dungeonId;
    recv_data >> eventPackedTime;
    recv_data >> flags;
    recv_data >> type;

    recv_data.ReadGuidMask<4>(eventGuid);
    recv_data.ReadGuidMask<5>(eventGuid);
    recv_data.ReadGuidMask<2>(eventGuid);
    recv_data.ReadGuidMask<4>(inviteGuid);
    recv_data.ReadGuidMask<7>(eventGuid);
    recv_data.ReadGuidMask<0>(eventGuid);
    recv_data.ReadGuidMask<5>(inviteGuid);
    recv_data.ReadGuidMask<3>(inviteGuid);
    recv_data.ReadGuidMask<6>(eventGuid);
    recv_data.ReadGuidMask<1>(eventGuid);
    recv_data.ReadGuidMask<6>(inviteGuid);
    recv_data.ReadGuidMask<2>(inviteGuid);
    recv_data.ReadGuidMask<7>(inviteGuid);
    recv_data.ReadGuidMask<1>(inviteGuid);
    recv_data.ReadGuidMask<0>(inviteGuid);

    uint32 const descriptionLength = recv_data.ReadBits(11);
    uint32 const titleLength = recv_data.ReadBits(8);

    // Same bound as HandleCalendarAddEvent, and for the same reason: these strings
    // are echoed back out in the update alert, whose client reader copies them into
    // a fixed stack-backed object without checking.
    if (descriptionLength > CALENDAR_MAX_DESCRIPTION_LEN || titleLength > CALENDAR_MAX_TITLE_LEN)
    {
        sLog.outError("CMSG_CALENDAR_UPDATE_EVENT: %s sent title %u / description %u, refusing",
                      guid.GetString().c_str(), titleLength, descriptionLength);
        recv_data.rfinish();
        return;
    }

    recv_data.ReadGuidMask<3>(eventGuid);                   // after the lengths

    recv_data.ReadGuidBytes<6>(inviteGuid);
    recv_data.ReadGuidBytes<0>(eventGuid);
    recv_data.ReadGuidBytes<7>(inviteGuid);
    recv_data.ReadGuidBytes<3>(inviteGuid);
    recv_data.ReadGuidBytes<6>(eventGuid);
    recv_data.ReadGuidBytes<1>(inviteGuid);
    recv_data.ReadGuidBytes<2>(eventGuid);
    title = recv_data.ReadString(titleLength);
    recv_data.ReadGuidBytes<5>(inviteGuid);
    recv_data.ReadGuidBytes<4>(inviteGuid);
    recv_data.ReadGuidBytes<5>(eventGuid);
    recv_data.ReadGuidBytes<3>(eventGuid);
    recv_data.ReadGuidBytes<0>(inviteGuid);
    recv_data.ReadGuidBytes<4>(eventGuid);
    description = recv_data.ReadString(descriptionLength);
    recv_data.ReadGuidBytes<1>(eventGuid);
    recv_data.ReadGuidBytes<2>(inviteGuid);
    recv_data.ReadGuidBytes<7>(eventGuid);

    eventId = eventGuid.GetRawValue();
    inviteId = inviteGuid.GetRawValue();

    // 5.4.8 sends neither a repetition type nor a second packed time here.
    repetitionType = 0;
    UnknownPackedTime = 0;

    // Decode before comparing -- see the note in HandleCalendarAddEvent. The packed
    // bitfield can never exceed current Unix time, so the old comparison rejected
    // every update as "in the past".
    time_t const decodedEventTime = timeBitFieldsToSecs(eventPackedTime);

    // prevent events in the past
    if (decodedEventTime < (GameTime::GetGameTime() - time_t(86400L)))
    {
        recv_data.rfinish();
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "], InviteId [" UI64FMTD "] Title %s, Description %s, type %u "
                     "Repeatable %u, MaxInvites %u, Dungeon ID %d, Flags %u", eventId, inviteId, title.c_str(),
                     description.c_str(), uint32(type), uint32(repetitionType), maxInvites, dungeonId, flags);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        if (guid != event->CreatorGuid)
        {
            CalendarInvite* updaterInvite = event->GetInviteByGuid(guid);
            if (updaterInvite == NULL)
            {
                sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NOT_INVITED);
                return ;
            }

            if (updaterInvite->Rank != CALENDAR_RANK_MODERATOR)
            {
                // remover have not enough right to change invite status
                sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_PERMISSIONS);
                return;
            }
        }

        oldEventTime = event->EventTime;

        event->Type = CalendarEventType(type);
        event->Flags = flags;
        event->EventTime = timeBitFieldsToSecs(eventPackedTime);
        event->UnknownTime = timeBitFieldsToSecs(UnknownPackedTime);
        event->DungeonId = dungeonId;
        event->Title = title;
        event->Description = description;

        sCalendarMgr.SendCalendarEventUpdateAlert(event, oldEventTime);

        // query construction
        CharacterDatabase.escape_string(title);
        CharacterDatabase.escape_string(description);
        CharacterDatabase.PExecute("UPDATE `calendar_events` SET "
                                   "`type`=%hu, `flags`=%u, `dungeonId`=%d, `eventTime`=%lu, `title`='%s', `description`='%s'"
                                   "WHERE `eventid` = " UI64FMTD,
                                   type, flags, dungeonId, event->EventTime, title.c_str(), description.c_str(), eventId);
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_EVENT_INVALID);
    }
}

void WorldSession::HandleCalendarRemoveEvent(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_REMOVE_EVENT [%s]", _player->GetGuidStr().c_str());

    uint64 eventId;
    uint64 inviteId;
    uint32 Flags;

    recv_data >> eventId;
    recv_data >> inviteId;
    recv_data >> Flags;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Remove event (eventId=" UI64FMTD ", remover inviteId =" UI64FMTD ")", eventId, inviteId);
    sCalendarMgr.RemoveEvent(eventId, _player);
}

void WorldSession::HandleCalendarCopyEvent(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_COPY_EVENT [%s]", guid.GetString().c_str());

    uint64 eventId;
    uint64 inviteId;
    uint32 packedTime;

    recv_data >> eventId >> inviteId;
    recv_data >> packedTime;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "] inviteId [" UI64FMTD "]",
                     eventId, inviteId);

    // Decode before comparing -- see the note in HandleCalendarAddEvent. The packed
    // bitfield can never exceed current Unix time, so the old comparison rejected
    // every copy as "in the past".
    time_t const copyTime = timeBitFieldsToSecs(packedTime);

    // prevent events in the past
    if (copyTime < (GameTime::GetGameTime() - time_t(86400L)))
    {
        recv_data.rfinish();
        return;
    }

    sCalendarMgr.CopyEvent(eventId, copyTime, guid);
}

void WorldSession::HandleCalendarEventInvite(WorldPacket& recv_data)
{
    ObjectGuid playerGuid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_INVITE [%s]", playerGuid.GetString().c_str());

    uint64 eventId;

    // TODO it seem its not inviteID but event->CreatorGuid
    uint64 inviteId;
    std::string name;
    bool isPreInvite;
    bool isGuildEvent;

    ObjectGuid inviteeGuid;
    uint32 inviteeTeam = 0;
    uint32 inviteeGuildId = 0;
    bool isIgnored = false;

    // From the client's writer sub_66CA8E: the two ids, then the pre-invite bit, a
    // NINE-bit name length split as eight bits then one, the guild-event bit, and
    // the raw name last. The length split is the same shape SMSG_CALENDAR_COMMAND_
    // RESULT uses. The inherited reader took a NUL-terminated name mid-body and two
    // whole bytes for the flags.
    recv_data >> eventId;
    recv_data >> inviteId;
    isPreInvite = recv_data.ReadBit();
    uint32 nameLength = recv_data.ReadBits(8) << 1;
    nameLength |= recv_data.ReadBit();
    isGuildEvent = recv_data.ReadBit();
    name = recv_data.ReadString(nameLength);

    if (Player* player = sObjectAccessor.FindPlayerByName(name.c_str()))
    {
        // Invitee is online
        inviteeGuid = player->GetObjectGuid();
        inviteeTeam = player->GetTeam();
        inviteeGuildId = player->GetGuildId();
        if (player->GetSocial()->HasIgnore(playerGuid))
        {
            isIgnored = true;
        }
    }
    else
    {
        // Invitee offline, get data from database
        CharacterDatabase.escape_string(name);
        QueryResult* result = CharacterDatabase.PQuery("SELECT `guid`,`race` FROM `characters` WHERE `name` = '%s'", name.c_str());
        if (result)
        {
            Field* fields = result->Fetch();
            inviteeGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            inviteeTeam = Player::TeamForRace(fields[1].GetUInt8());
            inviteeGuildId = Player::GetGuildIdFromDB(inviteeGuid);
            delete result;

            result = CharacterDatabase.PQuery("SELECT `flags` FROM `character_social` WHERE `guid` = %u AND `friend` = %u", inviteeGuid.GetCounter(), playerGuid.GetCounter());
            if (result)
            {
                Field* fields = result->Fetch();
                if (fields[0].GetUInt8() & SOCIAL_FLAG_IGNORED)
                {
                    isIgnored = true;
                }
                delete result;
            }
        }
    }

    if (inviteeGuid.IsEmpty())
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_PLAYER_NOT_FOUND);
        return;
    }

    if (isIgnored)
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_IGNORING_YOU_S, name.c_str());
        return;
    }

    if (_player->GetTeam() != inviteeTeam && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CALENDAR))
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NOT_ALLIED);
        return;
    }

    if (!isPreInvite)
    {
        if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
        {
            if (event->IsGuildEvent() && event->GuildId == inviteeGuildId)
            {
                // we can't invite guild members to guild events
                sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NO_GUILD_INVITES);
                return;
            }

            sCalendarMgr.AddInvite(event, playerGuid, inviteeGuid, CALENDAR_STATUS_INVITED, CALENDAR_RANK_PLAYER, "", time(NULL));
        }
        else
        {
            sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_EVENT_INVALID);
        }
    }
    else
    {
        if (isGuildEvent && inviteeGuildId == _player->GetGuildId())
        {
            sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NO_GUILD_INVITES);
            return;
        }

        // create a temp invite to send it back to client
        CalendarInvite invite;
        invite.SenderGuid = playerGuid;
        invite.InviteeGuid = inviteeGuid;
        invite.Status = CALENDAR_STATUS_INVITED;
        invite.Rank = CALENDAR_RANK_PLAYER;
        invite.LastUpdateTime = time(NULL);

        sCalendarMgr.SendCalendarEventInvite(&invite);
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "PREINVITE> sender[%s], Invitee[%s]", playerGuid.GetString().c_str(), inviteeGuid.GetString().c_str());
    }
}

void WorldSession::HandleCalendarEventRsvp(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_RSVP [%s]", guid.GetString().c_str());

    uint64 eventId;
    uint64 inviteId;
    // One BYTE, not a uint32. The client's writer sub_66919E emits two uint64 and
    // then a single sub_40F018; the inherited reader took four bytes where the
    // client sends one and ran three bytes past the end of the body.
    uint8 status;

    recv_data >> eventId >> inviteId >> status;
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "], InviteId [" UI64FMTD "], status %u",
                     eventId, inviteId, uint32(status));

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        // i think we still should be able to remove self from locked events
        if (status != CALENDAR_STATUS_REMOVED && event->Flags & CALENDAR_FLAG_INVITES_LOCKED)
        {
            sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_EVENT_LOCKED);
            return;
        }

        if (CalendarInvite* invite = event->GetInviteById(inviteId))
        {
            if (invite->InviteeGuid != guid)
            {
                CalendarInvite* updaterInvite = event->GetInviteByGuid(guid);
                if (updaterInvite == NULL)
                {
                    sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NOT_INVITED);
                    return ;
                }

                if (updaterInvite->Rank != CALENDAR_RANK_MODERATOR && updaterInvite->Rank != CALENDAR_RANK_OWNER)
                {
                    // remover have not enough right to change invite status
                    sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_PERMISSIONS);
                    return;
                }
            }
            invite->Status = CalendarInviteStatus(status);
            invite->LastUpdateTime = time(NULL);

            CharacterDatabase.PExecute("UPDATE `calendar_invites` SET `status`=%u, `lastUpdateTime`=%u WHERE `inviteId` = " UI64FMTD , status, uint32(invite->LastUpdateTime), invite->InviteId);
            sCalendarMgr.SendCalendarEventStatus(invite);
            sCalendarMgr.SendCalendarClearPendingAction(_player);
        }
        else
        {
            sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NO_INVITE); // correct?
        }
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_EVENT_INVALID);
    }
}

void WorldSession::HandleCalendarEventRemoveInvite(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_REMOVE_INVITE [%s]", guid.GetString().c_str());

    ObjectGuid invitee;
    uint64 eventId;
    uint64 ownerInviteId; // isn't it sender's inviteId? TODO: need more test to see what we can do with that value
    uint64 inviteId;

    // From the client's writer sub_669371: three ids in the pre-MoP order, then the
    // invitee GUID bit-packed at the END. The leading pre-MoP packed GUID is gone.
    recv_data >> inviteId >> ownerInviteId >> eventId;
    recv_data.ReadGuidMask<6, 3, 2, 4, 5, 7, 0, 1>(invitee);
    recv_data.ReadGuidBytes<0, 4, 7, 3, 5, 1, 2, 6>(invitee);

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "], ownerInviteId [" UI64FMTD "], Invitee ([%s] id: [" UI64FMTD "])",
                     eventId, ownerInviteId, invitee.GetString().c_str(), inviteId);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        sCalendarMgr.RemoveInvite(eventId, inviteId, guid);
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_EVENT_INVALID);
    }
}

void WorldSession::HandleCalendarEventStatus(WorldPacket& recv_data)
{
    ObjectGuid updaterGuid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_STATUS [%s]", updaterGuid.GetString().c_str());
    recv_data.hexlike();

    ObjectGuid invitee;
    uint64 eventId;
    uint64 inviteId;
    uint64 ownerInviteId; // isn't it sender's inviteId?
    uint8 status;

    // From the client's writer sub_667F7B, with the ids NAMED by the client's own
    // packet builder sub_9E858C (CalendarEventSetStatus -> sub_971DC3 -> sub_9716F2
    // -> sub_9E858C), which fills this object:
    //
    //     +16 <- invite->+0     inviteId
    //     +24 <- calendar->+40  ownerInviteId
    //     +32 <- calendar->+0   eventId
    //
    // and the writer emits +24, +32, +16. So the wire order is ownerInviteId,
    // eventId, inviteId -- a three-way rotation away from the pre-MoP order.
    //
    // This does NOT keep the legacy scalar order. An earlier pass assumed it did,
    // on the strength of four sibling opcodes that happen to; RSVP and REMOVE_INVITE
    // really do, and these two do not. The builder is the authority -- offsets alone
    // never named these, and the inference was the wrong tool.
    recv_data >> ownerInviteId >> eventId >> inviteId;
    recv_data >> status;
    recv_data.ReadGuidMask<4, 3, 7, 6, 2, 0, 5, 1>(invitee);
    recv_data.ReadGuidBytes<7, 5, 2, 1, 4, 6, 0, 3>(invitee);
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "] ownerInviteId [" UI64FMTD "], Invitee ([%s] id: [" UI64FMTD "], status %u",
                     eventId, ownerInviteId, invitee.GetString().c_str(), inviteId, status);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        if (CalendarInvite* invite = event->GetInviteById(inviteId))
        {
            if (invite->InviteeGuid != updaterGuid)
            {
                CalendarInvite* updaterInvite = event->GetInviteByGuid(updaterGuid);
                if (updaterInvite == NULL)
                {
                    sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NOT_INVITED);
                    return ;
                }

                if (updaterInvite->Rank != CALENDAR_RANK_MODERATOR && updaterInvite->Rank != CALENDAR_RANK_OWNER)
                {
                    // remover have not enough right to change invite status
                    sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_PERMISSIONS);
                    return;
                }
            }
            invite->Status = (CalendarInviteStatus)status;
            invite->LastUpdateTime = time(NULL);            // not sure if we should set response time when moderator changes invite status

            CharacterDatabase.PExecute("UPDATE `calendar_invites` SET `status`=%u, `lastUpdateTime`=%u WHERE `inviteId`=" UI64FMTD, status, uint32(invite->LastUpdateTime), invite->InviteId);
            sCalendarMgr.SendCalendarEventStatus(invite);
            sCalendarMgr.SendCalendarClearPendingAction(sObjectMgr.GetPlayer(invitee));
        }
        else
        {
            sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NO_INVITE);
        }
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_EVENT_INVALID);
    }
}

void WorldSession::HandleCalendarEventModeratorStatus(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_EVENT_MODERATOR_STATUS [%s]", guid.GetString().c_str());

    ObjectGuid invitee;
    uint64 eventId;
    uint64 inviteId;
    uint64 ownerInviteId; // isn't it sender's inviteId?
    uint8 rank;

    // From the client's writer sub_6657A5, with the ids NAMED by the client's own
    // packet builder sub_9E8626 (CalendarEventSetModerator -> sub_971E24 ->
    // sub_971723 -> sub_9E8626), which fills this object:
    //
    //     +16 <- invite->+0     inviteId
    //     +24 <- calendar->+0   eventId
    //     +32 <- calendar->+40  ownerInviteId
    //
    // and the writer emits the rank byte, then +24, +32, +16. So the wire order is
    // rank, eventId, ownerInviteId, inviteId -- the last two are TRANSPOSED against
    // the pre-MoP order.
    //
    // An earlier pass assumed the legacy scalar order held here. It does not, and
    // neither does it for CMSG_CALENDAR_EVENT_STATUS. Use the builder, not the
    // inference: it is the only thing that names an id the offsets cannot.
    recv_data >> rank;
    recv_data >> eventId >> ownerInviteId >> inviteId;
    recv_data.ReadGuidMask<6, 5, 1, 3, 4, 7, 0, 2>(invitee);
    recv_data.ReadGuidBytes<7, 5, 0, 4, 1, 3, 2, 6>(invitee);
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "] ownerInviteId [" UI64FMTD "], Invitee ([%s] id: [" UI64FMTD "], rank %u",
                     eventId, ownerInviteId, invitee.GetString().c_str(), inviteId, rank);

    if (CalendarEvent* event = sCalendarMgr.GetEventById(eventId))
    {
        if (CalendarInvite* invite = event->GetInviteById(inviteId))
        {
            if (invite->InviteeGuid != guid)
            {
                CalendarInvite* updaterInvite = event->GetInviteByGuid(guid);
                if (updaterInvite == NULL)
                {
                    sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NOT_INVITED);
                    return ;
                }
                if (updaterInvite->Rank != CALENDAR_RANK_MODERATOR && updaterInvite->Rank != CALENDAR_RANK_OWNER)
                {
                    // remover have not enough right to change invite status
                    sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_PERMISSIONS);
                    return;
                }
            }

            if (CalendarModerationRank(rank) == CALENDAR_RANK_OWNER)
            {
                // cannot set owner
                sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_PERMISSIONS);
                return;
            }

            CharacterDatabase.PExecute("UPDATE `calendar_invites` SET `rank` = %u WHERE `inviteId` = " UI64FMTD, rank, invite->InviteId);
            invite->Rank = CalendarModerationRank(rank);
            sCalendarMgr.SendCalendarEventModeratorStatusAlert(invite);
        }
        else
        {
            sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_NO_INVITE);
        }
    }
    else
    {
        sCalendarMgr.SendCalendarCommandResult(_player, CALENDAR_ERROR_EVENT_INVALID);
    }
}

void WorldSession::HandleCalendarComplain(WorldPacket& recv_data)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_COMPLAIN [%s]", guid.GetString().c_str());

    ObjectGuid badGuyGuid;
    uint64 eventId;
    uint64 inviteId;

    // From the client's writer sub_6669F0: the two ids first, in their pre-MoP
    // order, then the reported player's GUID bit-packed at the END. The inherited
    // reader took a raw uint64 GUID first.
    recv_data >> eventId >> inviteId;
    recv_data.ReadGuidMask<4, 6, 2, 7, 1, 5, 3, 0>(badGuyGuid);
    recv_data.ReadGuidBytes<6, 7, 1, 0, 4, 2, 3, 5>(badGuyGuid);
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "EventId [" UI64FMTD "], BadGuyGuid ([%s] inviteId: [" UI64FMTD "])",
                     eventId, badGuyGuid.GetString().c_str(), inviteId);

    // Remove the invite
    if (sCalendarMgr.RemoveInvite(eventId, inviteId, guid))
    {
        // uint32 then uint8, from the client's reader sub_C69B0E (reached via
        // sub_6BE3FD, vtable off_D6A040). The inherited body sent two bytes, so the
        // client read four bytes past the end of it.
        WorldPacket data(SMSG_COMPLAIN_RESULT, 4 + 1);
        data << uint32(0);
        data << uint8(0); // show complain saved. We can send 0x0C to show windows with ok button
        SendPacket(&data);
    }
}

void WorldSession::HandleCalendarGetNumPending(WorldPacket& /*recv_data*/)
{
    ObjectGuid guid = _player->GetObjectGuid();
    DEBUG_LOG("WORLD: Received opcode CMSG_CALENDAR_GET_NUM_PENDING [%s]", guid.GetString().c_str());

    uint32 pending = sCalendarMgr.GetPlayerNumPending(guid);

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Pending: %u", pending);

    WorldPacket data(SMSG_CALENDAR_SEND_NUM_PENDING, 4);
    data << uint32(pending);
    SendPacket(&data);
}

//////////////////////////////////////////////////////////////////////////
// Send function
//////////////////////////////////////////////////////////////////////////

void CalendarMgr::SendCalendarEventInviteAlert(CalendarInvite const* invite)
{
    DEBUG_LOG("WORLD: SMSG_CALENDAR_EVENT_INVITE_ALERT");

    CalendarEvent const* event = invite->GetCalendarEvent();
    if (!event)
    {
        return;
    }

    // 18414 body, from the client's reader sub_6F4D55 and VERIFIED against ALL SIX
    // captured bodies in the corpus (catalogue 2BE10C89) -- capture-000163 seq
    // 229424 and 626761, capture-000261 seq 911254, capture-000389 seq 2024636,
    // capture-000601 seq 1037992, capture-000696 seq 288566, sizes 50 to 66 bytes,
    // every one consumed exactly.
    //
    //     u64 eventId, i32 dungeonId, u8 type, u64 inviteId, u32 flags,
    //     u8 status, u32 eventTime, u8 rank,
    //     then 22 GUID mask bits across THREE guids, an 8-bit title length,
    //     two more mask bits, a flush, six GUID bytes, the title, and the
    //     remaining eighteen GUID bytes.
    //
    // The captures name the fields. `type` reads 0 (RAID) in exactly the bodies
    // whose dungeonId is a real dungeon (531, 714) and 4 (OTHER) in exactly those
    // where it is -1; `status` reads 7, 0 and 3 (NOT_SIGNED_UP, INVITED,
    // CONFIRMED); `rank` reads 2 (OWNER) only in the body where the invitee is the
    // creator. Guild announcements carry the guild GUID with inviteId 0, personal
    // invites carry inviteId with no guild GUID.
    //
    // The first and third GUID slots were RESOLVED after the fact, and the first
    // reading here had them backwards. The captures cannot settle it -- creator
    // equals sender in all six -- so they were assigned in pre-MoP order, creator
    // first. The client's consumers say otherwise: the day-event consumer puts the
    // FIRST guid into its `invitedBy` field for ordinary invitations and the THIRD
    // for guild signup records, and the full-calendar consumer independently uses
    // the sender for ordinary invites and the creator for guild-event records.
    // Blizzard_Calendar.lua exposes that field as `invitedBy`, rendering ordinary
    // events as "invited by" and announcements as "created by".
    //
    // So slot one is the SENDER and slot three the CREATOR, as written below.
    // Backwards, this shows the wrong name on any invite from someone other than
    // the event creator -- and no byte fixture built from these captures could
    // catch it, because in every one of them the two GUIDs are equal.
    //
    // Guild GUID built the same way CalendarHandler.cpp:84 and Player.cpp:1465
    // already do it, so this field stays consistent with the calendar list and the
    // roster rather than inventing a second convention. Empty when the event is not
    // a guild event, which is what the captures show for personal invites.
    ObjectGuid const senderGuid = invite->SenderGuid;   // slot one
    ObjectGuid const guildGuid = event->IsGuildEvent()
        ? ObjectGuid(HIGHGUID_GUILD, event->GuildId) : ObjectGuid();
    ObjectGuid const creatorGuid = event->CreatorGuid;  // slot three

    WorldPacket data(SMSG_CALENDAR_EVENT_INVITE_ALERT, 8 + 4 + 1 + 8 + 4 + 1 + 4 + 1 + 4 + 24 + event->Title.size());
    data << uint64(event->EventId);
    data << int32(event->DungeonId);
    data << uint8(event->Type);
    data << uint64(invite->InviteId);
    data << uint32(event->Flags);
    data << uint8(invite->Status);
    data << secsToTimeBitFields(event->EventTime);
    data << uint8(invite->Rank);

    data.WriteGuidMask<7>(guildGuid);   data.WriteGuidMask<6>(creatorGuid);
    data.WriteGuidMask<4>(guildGuid);   data.WriteGuidMask<0>(guildGuid);
    data.WriteGuidMask<3>(creatorGuid);  data.WriteGuidMask<1>(senderGuid);
    data.WriteGuidMask<5>(guildGuid);   data.WriteGuidMask<2>(creatorGuid);
    data.WriteGuidMask<0>(creatorGuid);  data.WriteGuidMask<1>(guildGuid);
    data.WriteGuidMask<5>(creatorGuid);  data.WriteGuidMask<6>(guildGuid);
    data.WriteGuidMask<3>(guildGuid);   data.WriteGuidMask<6>(senderGuid);
    data.WriteGuidMask<2>(senderGuid); data.WriteGuidMask<4>(creatorGuid);
    data.WriteGuidMask<7>(creatorGuid);  data.WriteGuidMask<4>(senderGuid);
    data.WriteGuidMask<1>(creatorGuid);  data.WriteGuidMask<3>(senderGuid);
    data.WriteGuidMask<2>(guildGuid);   data.WriteGuidMask<0>(senderGuid);
    data.WriteBits(event->Title.size(), 8);
    data.WriteGuidMask<5>(senderGuid); data.WriteGuidMask<7>(senderGuid);
    data.FlushBits();

    data.WriteGuidBytes<5>(creatorGuid); data.WriteGuidBytes<6>(guildGuid);
    data.WriteGuidBytes<0>(guildGuid);  data.WriteGuidBytes<6>(creatorGuid);
    data.WriteGuidBytes<5>(senderGuid); data.WriteGuidBytes<4>(senderGuid);
    data.append(event->Title.data(), event->Title.size());
    data.WriteGuidBytes<0>(creatorGuid); data.WriteGuidBytes<1>(creatorGuid);
    data.WriteGuidBytes<2>(guildGuid);  data.WriteGuidBytes<7>(guildGuid);
    data.WriteGuidBytes<6>(senderGuid); data.WriteGuidBytes<1>(guildGuid);
    data.WriteGuidBytes<2>(creatorGuid); data.WriteGuidBytes<3>(senderGuid);
    data.WriteGuidBytes<7>(senderGuid); data.WriteGuidBytes<0>(senderGuid);
    data.WriteGuidBytes<3>(creatorGuid); data.WriteGuidBytes<2>(senderGuid);
    data.WriteGuidBytes<7>(creatorGuid); data.WriteGuidBytes<5>(guildGuid);
    data.WriteGuidBytes<4>(guildGuid);  data.WriteGuidBytes<3>(guildGuid);
    data.WriteGuidBytes<1>(senderGuid); data.WriteGuidBytes<4>(creatorGuid);

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SendCalendarInviteAlert> creatorGuid[%s], inviteeGuid[%s], EventId[" UI64FMTD "], Status[%u], InviteId[" UI64FMTD "]",
                     invite->SenderGuid.GetString().c_str(), invite->InviteeGuid.GetString().c_str(), event->EventId, uint32(invite->Status), invite->InviteId);

    if (event->IsGuildEvent() || event->IsGuildAnnouncement())
    {
        if (Guild* guild = sGuildMgr.GetGuildById(event->GuildId))
        {
            guild->BroadcastPacket(&data);
        }
    }
    else if (Player* player = sObjectMgr.GetPlayer(invite->InviteeGuid))
    {
        player->SendDirectMessage(&data);
    }
}

void CalendarMgr::SendCalendarEventInvite(CalendarInvite const* invite)
{
    CalendarEvent const* event = invite->GetCalendarEvent();

    time_t statusTime = invite->LastUpdateTime;
    bool preInvite = true;
    uint64 eventId = 0;
    if (event != NULL)
    {
        preInvite = false;
        eventId = event->EventId;
    }

    Player* player = sObjectMgr.GetPlayer(invite->InviteeGuid);

    uint8 level = player ? player->getLevel() : Player::GetLevelFromDB(invite->InviteeGuid);
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_INVITE");
    // 18414 body, from the client's reader sub_6C3312 and VERIFIED byte-exact
    // against two real captures (capture-000444 seq 262179 and capture-000696 seq
    // 290114, catalogue 2BE10C89), both 30 bytes and both consumed exactly:
    //
    //     u8    !preInvite
    //     u8    status
    //     u64   inviteId
    //     u8    level
    //     u64   eventId
    //     guid mask  <6,4,1,3,7,0,2,5>  invitee
    //     1 bit  preInvite  (SET when there is no status time -- inverted)
    //     1 bit  sender != invitee
    //     guid bytes 7, 0, 5
    //     u32   statusTime  -- only when !preInvite, INTERLEAVED here
    //     guid bytes 2, 3, 4, 1, 6
    //
    // The captures name the fields: +24 held 90, a MoP max level; the eventId slot
    // was identical across two packets for different invitees; and the status byte
    // read 6 (SIGNED_UP) and 8 (TENTATIVE) with the sender-differs bit clear, which
    // is exactly a self sign-up and confirms both readings at once.
    //
    // Note the status time sits INSIDE the GUID byte run, between byte 5 and byte 2.
    // The previous body led with a pre-MoP packed GUID and shares no position.
    ObjectGuid const inviteeGuid = invite->InviteeGuid;
    bool const senderDiffers = invite->SenderGuid != invite->InviteeGuid;

    WorldPacket data(SMSG_CALENDAR_EVENT_INVITE, 1 + 1 + 8 + 1 + 8 + 2 + 8 + 4);
    data << uint8(!preInvite);
    data << uint8(invite->Status);
    data << uint64(invite->InviteId);
    data << uint8(level);
    data << uint64(eventId);
    data.WriteGuidMask<6, 4, 1, 3, 7, 0, 2, 5>(inviteeGuid);
    data.WriteBit(preInvite ? 1 : 0);                       // inverted: set = no time
    data.WriteBit(senderDiffers ? 1 : 0);
    data.FlushBits();
    data.WriteGuidBytes<7, 0, 5>(inviteeGuid);
    if (!preInvite)
    {
        data << secsToTimeBitFields(statusTime);
    }
    data.WriteGuidBytes<2, 3, 4, 1, 6>(inviteeGuid);

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SendCalendarInvit> %s senderGuid[%s], inviteeGuid[%s], EventId[" UI64FMTD "], Status[%u], InviteId[" UI64FMTD "]",
                     preInvite ? "is PreInvite," : "", invite->SenderGuid.GetString().c_str(), invite->InviteeGuid.GetString().c_str(), eventId, uint32(invite->Status), invite->InviteId);

    //data.hexlike();
    if (preInvite)
    {
        if (Player* sender = sObjectMgr.GetPlayer(invite->SenderGuid))
        {
            sender->SendDirectMessage(&data);
        }
    }
    else
    {
        SendPacketToAllEventRelatives(data, event);
    }
}

void CalendarMgr::SendCalendarCommandResult(Player* player, CalendarError err, char const* param /*= NULL*/)
{
    if (!player)
    {
        return;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_COMMAND_RESULT (%u)", err);

    // 18414 body, recovered from the client's own parser sub_706B85 (reached from
    // the receive dispatch through sub_708A1C, which installs vtable off_D6AEA0):
    //
    //     u8    nameLen >> 1          high eight bits of a NINE-bit length
    //     u8    (nameLen & 1) << 7    the low bit, in the MSB; seven bits padding
    //     u8    unidentified          parser stores it at record +323
    //     u8    error                 parser stores it at record +322
    //     bytes name                  nameLen raw bytes, NOT NUL-terminated
    //
    // The length really is split: the parser does `2 * readByte()` then ORs a
    // single ReadBit, and then goes back to raw byte reads, so the remaining seven
    // bits of the second byte are discarded padding rather than part of the stream.
    //
    // The record offsets are what tie this together. The client's error-display
    // switch sub_972E78 reads the error at +0x142 (322) and, for the three _S
    // variants, passes the string at +0x10 (16) as the format argument -- exactly
    // the two fields this parser fills. Its 41 cases also map 1:1 onto CalendarError
    // below (1 GUILD_EVENTS_EXCEEDED, 5 PERMISSIONS, 10 ALREADY_INVITED_TO_EVENT_S,
    // 13 IGNORING_YOU_S, 26 ARENA_EVENTS_EXCEEDED, 29 NO_INVITE ...), so the wire
    // byte is the enum value directly, with no bias.
    //
    // The previous body was pre-MoP -- uint32, uint8, a NUL-terminated param or a
    // filler byte, then uint32(err) -- and shares no field with this one.
    //
    // The name is sent whatever the error; the client only formats it for the _S
    // variants. Clamped because the client's destination buffer runs from +0x10 to
    // +0x142, i.e. 306 bytes, and it NUL-terminates at name[len].
    std::string name = param ? param : "";
    if (name.size() > 255)
    {
        name.resize(255);
    }

    WorldPacket data(SMSG_CALENDAR_COMMAND_RESULT, 4 + name.size());
    data << uint8(uint8(name.size() >> 1));
    data << uint8(uint8((name.size() & 1) << 7));
    data << uint8(0);                                       // record +323, purpose not identified
    data << uint8(uint8(err));                              // record +322
    if (!name.empty())
    {
        data.append(name.c_str(), name.size());
    }

    player->SendDirectMessage(&data);
}

void CalendarMgr::SendCalendarEventRemovedAlert(CalendarEvent const* event)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_REMOVED_ALERT");

    // 18414 body, from the client's reader sub_6EC557 (reached via sub_6F6809,
    // which constructs on vtable off_D6AE78):
    //
    //     u64    eventId          sub_660A2A -> sub_40F370
    //     u32    packedTime       sub_40F340
    //     1 bit  clearPendingAction sub_665262, MSB-first, then flushed
    //
    // The flag is a trailing BIT, not a leading byte. The previous body led with
    // uint8(1) and put the two scalars after it, so all three fields landed in the
    // wrong places.
    WorldPacket data(SMSG_CALENDAR_EVENT_REMOVED_ALERT, 8 + 4 + 1);
    data << uint64(event->EventId);
    data << secsToTimeBitFields(event->EventTime);
    data.WriteBit(1);                                       // clearPendingAction: SET means the
                                                        // pending action is RESOLVED -- the client
                                                        // publishes CALENDAR_ACTION_PENDING(false).
    data.FlushBits();

    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendCalendarEvent(Player* player, CalendarEvent const* event, uint32 sendType)
{
    if (!player || !event)
    {
        return;
    }

    std::string timeStr = TimeToTimestampStr(event->EventTime);
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SendCalendarEvent> sendType[%u], CreatorGuid[%s], EventId[" UI64FMTD "], Type[%u], Flags[%u], Title[%s]",
                     sendType, event->CreatorGuid.GetString().c_str(), event->EventId, uint32(event->Type), event->Flags, event->Title.c_str());

    MopCalendarPackets::CalendarEventDetails eventRecord;
    eventRecord.creatorGuid = event->CreatorGuid.GetRawValue();
    if (Guild* guild = sGuildMgr.GetGuildById(event->GuildId))
        eventRecord.guildGuid = guild->GetObjectGuid().GetRawValue();
    eventRecord.title = event->Title;
    eventRecord.description = event->Description;
    eventRecord.flags = event->Flags;
    eventRecord.eventTime = secsToTimeBitFields(event->EventTime);
    eventRecord.dungeonId = event->DungeonId;
    eventRecord.type = event->Type;
    eventRecord.eventId = event->EventId;
    eventRecord.sendType = uint8(sendType);

    CalendarInviteMap const* cInvMap = event->GetInviteMap();
    std::vector<MopCalendarPackets::CalendarEventInvite> inviteRecords;
    inviteRecords.reserve(cInvMap->size());
    for (auto const& inviteEntry : *cInvMap)
    {
        CalendarInvite const* invite = inviteEntry.second;
        ObjectGuid inviteeGuid = invite->InviteeGuid;
        Player* invitee = sObjectMgr.GetPlayer(inviteeGuid);

        uint8 inviteeLevel = invitee ? invitee->getLevel() : Player::GetLevelFromDB(inviteeGuid);
        uint32 inviteeGuildId = invitee ? invitee->GetGuildId() : Player::GetGuildIdFromDB(inviteeGuid);

        MopCalendarPackets::CalendarEventInvite record;
        record.inviteeGuid = inviteeGuid.GetRawValue();
        record.statusTime = secsToTimeBitFields(invite->LastUpdateTime);
        record.guildEvent = event->IsGuildEvent() && event->GuildId == inviteeGuildId;
        record.text = invite->Text;
        record.level = inviteeLevel;
        record.rank = invite->Rank;
        record.inviteId = invite->InviteId;
        record.status = invite->Status;
        inviteRecords.push_back(record);

        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "Invite> InviteId[" UI64FMTD "], InviteLvl[%u], Status[%u], Rank[%u],  GuildEvent[%s], Text[%s]",
                         invite->InviteId, uint32(inviteeLevel), uint32(invite->Status), uint32(invite->Rank),
                         (event->IsGuildEvent() && event->GuildId == inviteeGuildId) ? "true" : "false", invite->Text.c_str());
    }

    WorldPacket data(SMSG_CALENDAR_SEND_EVENT);
    if (!MopCalendarPackets::BuildCalendarEvent(data, eventRecord, inviteRecords))
    {
        sLog.outError("Calendar event " UI64FMTD " exceeds the 5.4.8 wire bounds", event->EventId);
        return;
    }
    player->SendDirectMessage(&data);
}

void CalendarMgr::SendCalendarEventInviteRemove(CalendarInvite const* invite, uint32 flags)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_INVITE_REMOVED");

    CalendarEvent const* event = invite->GetCalendarEvent();

    // 18414 body, from the client's reader sub_6E61CA (via sub_7096B7, vtable
    // off_D6AA90). Field identity comes from the client's post-construction
    // semantic path sub_6FACFA -> 0x977420 -> sub_977008, which is what names them:
    // +24 is used as the 64-bit event lookup key, the GUID at +40 is compared
    // against the player and passed to the invite-removal routine, +16 is tested
    // with 0x400, and a set bit at +32 makes the client publish
    // CALENDAR_ACTION_PENDING(false).
    //
    //     invitee GUID mask <6,7,3,0,2,4,1,5>
    //     1 bit  clearPendingAction
    //     invitee GUID bytes 0, 4, 3, 5
    //     u64    eventId
    //     invitee GUID bytes 7, 1, 2
    //     u32    eventFlags
    //     invitee GUID byte 6
    //
    // The scalars are INTERLEAVED into the GUID byte run. The previous body led
    // with a pre-MoP packed GUID and shares no field position with this.
    //
    // The trailing flag is named for what the consumer does with it: a SET bit
    // clears the pending-action badge. Sending 1 is what this core intended.
    ObjectGuid const inviteeGuid = invite->InviteeGuid;

    WorldPacket data(SMSG_CALENDAR_EVENT_INVITE_REMOVED, 2 + 8 + 8 + 4);
    data.WriteGuidMask<6, 7, 3, 0, 2, 4, 1, 5>(inviteeGuid);
    data.WriteBit(1);                                       // clear pending action
    data.FlushBits();
    data.WriteGuidBytes<0, 4, 3, 5>(inviteeGuid);
    data << uint64(event->EventId);
    data.WriteGuidBytes<7, 1, 2>(inviteeGuid);
    data << uint32(flags);
    data.WriteGuidBytes<6>(inviteeGuid);

    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendCalendarEventInviteRemoveAlert(Player* player, CalendarEvent const* event, CalendarInviteStatus status)
{
    if (player)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_INVITE_REMOVED_ALERT");
        // 18414 body, from the client's reader sub_6B8FBA (via sub_6BC0BD, vtable
        // off_D6AA18): flags, status, eventId, eventTime -- in that order.
        //
        // The two uint32 are told apart by what the client does with them, at
        // 0x9786A6 (reached via sub_6CA13B -> 0x978B69): the field at record +24 is
        // pushed straight into the calendar date unpacker sub_9725B2, and the field
        // at +28 is masked with 0x440. So +24 is the packed eventTime and +28 the
        // flags -- a distinction nothing in the layout itself could have made, and
        // one that no length check would ever have caught.
        //
        // The previous body wrote all four in the wrong order.
        WorldPacket data(SMSG_CALENDAR_EVENT_INVITE_REMOVED_ALERT, 4 + 1 + 8 + 4);
        data << uint32(event->Flags);
        data << uint8(status);
        data << uint64(event->EventId);
        data << secsToTimeBitFields(event->EventTime);

        player->SendDirectMessage(&data);
    }
}

void CalendarMgr::SendCalendarEventStatus(CalendarInvite const* invite)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_INVITE_STATUS");
    CalendarEvent const* event = invite->GetCalendarEvent();

    MopCalendarPackets::InviteStatus record;
    record.inviteeGuid = invite->InviteeGuid.GetRawValue();
    record.eventId = event->EventId;
    record.eventFlags = event->Flags;
    record.lastUpdateTime = secsToTimeBitFields(invite->LastUpdateTime);
    record.eventTime = secsToTimeBitFields(event->EventTime);
    record.status = invite->Status;
    record.displayPendingAction = true;

    WorldPacket data(SMSG_CALENDAR_EVENT_INVITE_STATUS, 31);
    MopCalendarPackets::BuildCalendarInviteStatus(data, record);
    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendCalendarClearPendingAction(Player* player)
{
    if (player)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_CLEAR_PENDING_ACTION TO [%s]", player->GetObjectGuid().GetString().c_str());
        WorldPacket data(SMSG_CALENDAR_CLEAR_PENDING_ACTION, 0);
        player->SendDirectMessage(&data);
    }
}

void CalendarMgr::SendCalendarEventModeratorStatusAlert(CalendarInvite const* invite)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_MODERATOR_STATUS");
    CalendarEvent const* event = invite->GetCalendarEvent();

    MopCalendarPackets::ModeratorStatus record;
    record.inviteeGuid = invite->InviteeGuid.GetRawValue();
    record.eventId = event->EventId;
    record.rank = invite->Rank;
    record.displayPendingAction = true;

    WorldPacket data(SMSG_CALENDAR_EVENT_MODERATOR_STATUS, 19);
    MopCalendarPackets::BuildCalendarModeratorStatus(data, record);
    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendCalendarEventUpdateAlert(CalendarEvent const* event, time_t oldEventTime)
{
    DEBUG_FILTER_LOG(LOG_FILTER_CALENDAR, "SMSG_CALENDAR_EVENT_UPDATED_ALERT");
    // 18414 body, from the client's reader sub_708569 and VERIFIED against two real
    // captures (capture-000389 seq 2030417, 84 bytes; capture-000163 seq 614026,
    // 118 bytes; catalogue 2BE10C89), both consumed exactly:
    //
    //     u32   flags
    //     u8    type
    //     u32   unknownTime
    //     u32   oldEventTime
    //     u64   eventId
    //     u32   eventTime
    //     i32   dungeonId
    //     1 bit showPendingAlert
    //     11 bits descriptionLength, then 8 bits titleLength
    //     title bytes, then description bytes  -- both raw, neither NUL-terminated
    //
    // The captures name the fields outright. dungeonId read -1 in one and 531 in the
    // other; flags read 0x400 in both; and the two time slots were IDENTICAL in the
    // capture whose event time had not moved but differed in the one where it had,
    // which is what fixes them as oldEventTime then eventTime. Their titles and
    // descriptions decode as real text at the recovered lengths.
    //
    // 5.4.8 sends neither `repeatable` nor a max-invite count here, which is why the
    // old body -- which led with the alert byte and appended both -- could never
    // line up. Both strings go LAST.
    //
    // The alert flag's POLARITY is the one thing the captures do not settle: both
    // carried 0 where this core intends 1. It is written as the server's own value.
    // Getting it backwards costs a spurious or missing toast, not a desync.
    WorldPacket data(SMSG_CALENDAR_EVENT_UPDATED_ALERT,
                     4 + 1 + 4 + 4 + 8 + 4 + 4 + 3 +
                     event->Title.size() + event->Description.size());
    data << uint32(event->Flags);
    data << uint8(event->Type);
    data << secsToTimeBitFields(event->UnknownTime);
    data << secsToTimeBitFields(oldEventTime);
    data << uint64(event->EventId);
    data << secsToTimeBitFields(event->EventTime);
    data << int32(event->DungeonId);
    data.WriteBit(1);                                       // clearPendingAction: SET means the
                                                        // pending action is RESOLVED -- the client
                                                        // publishes CALENDAR_ACTION_PENDING(false).
    data.WriteBits(event->Description.size(), 11);
    data.WriteBits(event->Title.size(), 8);
    data.FlushBits();
    data.append(event->Title.data(), event->Title.size());
    data.append(event->Description.data(), event->Description.size());

    SendPacketToAllEventRelatives(data, event);
}

void CalendarMgr::SendPacketToAllEventRelatives(WorldPacket packet, CalendarEvent const* event)
{
    // Send packet to all guild members
    if (event->IsGuildEvent() || event->IsGuildAnnouncement())
        if (Guild* guild = sGuildMgr.GetGuildById(event->GuildId))
        {
            guild->BroadcastPacket(&packet);
        }

    // Send packet to all invitees if event is non-guild, in other case only to non-guild invitees (packet was broadcasted for them)
    CalendarInviteMap const* cInvMap = event->GetInviteMap();
    for (CalendarInviteMap::const_iterator itr = cInvMap->begin(); itr != cInvMap->end(); ++itr)
        if (Player* player = sObjectMgr.GetPlayer(itr->second->InviteeGuid))
            if (!event->IsGuildEvent() || (event->IsGuildEvent() && player->GetGuildId() != event->GuildId))
            {
                player->SendDirectMessage(&packet);
            }
}

void CalendarMgr::SendCalendarRaidLockoutRemove(Player* player, DungeonPersistentState const* save)
{
    if (!save || !player)
    {
        return;
    }

    DEBUG_LOG("SMSG_CALENDAR_RAID_LOCKOUT_REMOVED [%s]", player->GetObjectGuid().GetString().c_str());
    WorldPacket data(SMSG_CALENDAR_RAID_LOCKOUT_REMOVED, 17);
    // Map-aware raw conversion. The retained retail body for map 580 carries difficulty 4,
    // which is Sunwell's own MapDifficulty row -- not the 3 the fixed raid table returns for
    // the internal 0 it is canonicalised to.
    MapEntry const* saveMap = save->GetMapEntry();
    MopCalendarPackets::BuildCalendarRaidLockoutRemoved(data,
        ToClientDifficultyForMap(save->GetMapId(), save->GetDifficulty(),
                                 saveMap && saveMap->IsRaid()),
        save->GetMapId(), save->GetInstanceGuid().GetRawValue());
    player->SendDirectMessage(&data);
}

void CalendarMgr::SendCalendarRaidLockoutAdd(Player* player, DungeonPersistentState const* save)
{
    if (!save || !player)
    {
        return;
    }

    DEBUG_LOG("SMSG_CALENDAR_RAID_LOCKOUT_ADDED [%s]", player->GetObjectGuid().GetString().c_str());
    time_t currTime = time(NULL);

    WorldPacket data(SMSG_CALENDAR_RAID_LOCKOUT_ADDED, 4 + 4 + 4 + 4 + 8);
    data << secsToTimeBitFields(currTime);
    data << uint32(save->GetMapId());
    // RAW client DifficultyID, map-aware -- see ToClientDifficultyForMap. isRaid derived
    // rather than hardcoded, matching the sibling sites; it decides only the fallback.
    MapEntry const* addMap = save->GetMapEntry();
    data << uint32(ToClientDifficultyForMap(save->GetMapId(), save->GetDifficulty(),
                                            addMap && addMap->IsRaid()));
    data << uint32(save->GetResetTime() - currTime);
    data << uint64(save->GetInstanceId());
    //data.hexlike();
    player->SendDirectMessage(&data);
}
