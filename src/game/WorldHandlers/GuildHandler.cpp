/**
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

/**
 * @file GuildHandler.cpp
 * @brief Guild opcode handlers
 *
 * This file handles guild-related opcodes including:
 * - CMSG_GUILD_QUERY: Query guild information
 * - CMSG_GUILD_CREATE: Create new guild
 * - CMSG_GUILD_INVITE: Invite player to guild
 * - CMSG_GUILD_ACCEPT: Accept guild invitation
 * - CMSG_GUILD_DECLINE: Decline guild invitation
 * - CMSG_GUILD_ROSTER: Request guild roster
 * - CMSG_GUILD_LEAVE: Leave guild
 * - CMSG_GUILD_DISBAND: Disband guild
 * - CMSG_GUILD_LEADER: Transfer guild leadership
 * - CMSG_GUILD_MOTD: Set guild message of the day
 * - CMSG_GUILD_ADD_RANK: Add guild rank
 * - CMSG_GUILD_DELETE_RANK: Delete guild rank
 * - CMSG_GUILD_DEMOTE: Demote guild member
 * - CMSG_GUILD_PROMOTE: Promote guild member
 * - CMSG_GUILD_REMOVE: Remove guild member
 * - CMSG_GUILD_CHAT: Send guild chat message
 * - CMSG_GUILD_BANK: Guild bank operations
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Opcodes.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "GossipDef.h"
#include "SocialMgr.h"
#include "Calendar.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

void WorldSession::HandleGuildQueryOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_QUERY");

    // The inherited reader took two raw uint64, i.e. a fixed 16 bytes, against a wire
    // body that is 9..17 and bit-packed. Orders now come from the client's own send
    // serializer sub_665EE4; see MopGuildPackets::ReadGuildQuery. Note the wire order
    // is player first, then guild -- the reverse of how this was named.
    uint64 rawPlayerGuid = 0;
    uint64 rawGuildGuid = 0;
    MopGuildPackets::ReadGuildQuery(recvPacket, rawPlayerGuid, rawGuildGuid);

    ObjectGuid const guildGuid(rawGuildGuid);
    ObjectGuid const playerGuid(rawPlayerGuid);

    if (Guild* guild = sGuildMgr.GetGuildByGuid(guildGuid))
    {
        if (guild->GetMemberSlot(playerGuid))
        {
            guild->Query(this);
            return;
        }
    }

    SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

/**
 * @brief Creates a new guild for the current player.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildCreateOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_CREATE");

    std::string gname;
    recvPacket >> gname;

    if (GetPlayer()->GetGuildId())                          // already in guild
    {
        return;
    }

    Guild* guild = new Guild;
    if (!guild->Create(GetPlayer(), gname))
    {
        delete guild;
        return;
    }

    sGuildMgr.AddGuild(guild);
}

/**
 * @brief Sends a guild invitation packet to another player.
 *
 * @param player The invited player.
 * @param alreadyInGuild Unused legacy flag for prior guild membership checks.
 */
void WorldSession::SendGuildInvite(Player* player, bool alreadyInGuild /*= false*/)
{
    // PARKED alongside CMSG_GUILD_INVITE. This is the second way into the same
    // broken reply and it is NOT dead code: Eluna exposes it to Lua as
    // Player:SendGuildInvite, and SCRIPT_LIB_ELUNA defaults ON, so unregistering
    // the opcode did not close this path.
    //
    // What it used to do was worse than the handler's version -- two raw
    // cstrings, a body pre-dating even the six-uint32 one -- and it set
    // SetGuildIdInvited before sending. Since SMSG_GUILD_INVITE is not admitted
    // to IsEnterWorldConverted the packet was dropped every time, so the only
    // observable effect was leaving the target flagged as invited to a guild
    // whose invitation they could never see or act on.
    //
    // Refusing outright is better than half-working: a script author gets a log
    // line instead of a target stuck in an invited state. Restore this together
    // with the opcode, once the reply is rebuilt from reader sub_69E959 -- the
    // layout is recorded at the registration site in Opcodes.cpp.
    sLog.outError("WorldSession::SendGuildInvite: refused for %s -- SMSG_GUILD_INVITE "
                  "is not converted for 5.4.8 and is dropped by the send gate. "
                  "Guild invitations are unavailable until it is rebuilt.",
                  player ? player->GetGuidStr().c_str() : "<null>");
}

/**
 * @brief Invites another player to the current guild.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildInviteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_INVITE");

    std::string Invitedname, plname;
    Player* player = NULL;

    if (!MopGuildPackets::ReadGuildInvite(recvPacket, Invitedname))
    {
        sLog.outError("WORLD: Malformed CMSG_GUILD_INVITE from player %s",
            GetPlayer()->GetName());
        return;
    }

    // The 18414 roster and who-list hand GuildInvite a realm-qualified name, so
    // the same home-realm suffix that broke whisper replies has to come off here
    // before the lookup.
    StripHomeRealmSuffix(Invitedname);

    if (normalizePlayerName(Invitedname))
    {
        player = sObjectAccessor.FindPlayerByName(Invitedname.c_str());
    }

    if (!player)
    {
        SendGuildCommandResult(GUILD_INVITE_S, Invitedname, ERR_GUILD_PLAYER_NOT_FOUND_S);
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    // OK result but not send invite
    if (player->GetSocial()->HasIgnore(GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    // not let enemies sign guild charter
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != GetPlayer()->GetTeam())
    {
        SendGuildCommandResult(GUILD_INVITE_S, Invitedname, ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        plname = player->GetName();
        SendGuildCommandResult(GUILD_INVITE_S, plname, ERR_ALREADY_IN_GUILD_S);
        return;
    }

    if (player->GetGuildIdInvited())
    {
        plname = player->GetName();
        SendGuildCommandResult(GUILD_INVITE_S, plname, ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_INVITE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    DEBUG_LOG("Player %s Invited %s to Join his Guild", GetPlayer()->GetName(), Invitedname.c_str());

    player->SetGuildIdInvited(GetPlayer()->GetGuildId());
    // Put record into guildlog
    guild->LogGuildEvent(GUILD_EVENT_LOG_INVITE_PLAYER, GetPlayer()->GetObjectGuid(), player->GetObjectGuid());

    ObjectGuid oldGuild = player->GetGuildGuid();
    ObjectGuid newGuild = guild->GetObjectGuid();
    std::string oldGuildName = player->GetGuildName();
    std::string newGuildName = guild->GetName();

    WorldPacket data(SMSG_GUILD_INVITE, 4 * 6 + 10);          // guess size
    data << uint32(guild->GetLevel());
    data << uint32(guild->GetBorderStyle());
    data << uint32(guild->GetBorderColor());
    data << uint32(guild->GetEmblemStyle());
    data << uint32(guild->GetBackgroundColor());
    data << uint32(guild->GetEmblemColor());

    data.WriteGuidMask<3, 2>(newGuild);
    data.WriteBits(oldGuildName.length(), 8);
    data.WriteGuidMask<1>(newGuild);
    data.WriteGuidMask<6, 4, 1, 5, 7, 2>(oldGuild);
    data.WriteGuidMask<7, 0, 6>(newGuild);
    data.WriteBits(newGuildName.length(), 8);
    data.WriteGuidMask<3, 0>(oldGuild);
    data.WriteGuidMask<5>(newGuild);
    data.WriteBits(strlen(_player->GetName()), 7);
    data.WriteGuidMask<4>(newGuild);

    data.WriteGuidBytes<1>(newGuild);
    data.WriteGuidBytes<3>(oldGuild);
    data.WriteGuidBytes<6>(newGuild);
    data.WriteGuidBytes<2, 1>(oldGuild);
    data.WriteGuidBytes<0>(newGuild);

    data.WriteStringData(oldGuildName);

    data.WriteGuidBytes<7, 2>(newGuild);

    data.WriteStringData(_player->GetName());

    data.WriteGuidBytes<7, 6, 5, 0>(oldGuild);
    data.WriteGuidBytes<4>(newGuild);

    data.WriteStringData(newGuildName);

    data.WriteGuidBytes<5, 3>(newGuild);
    data.WriteGuidBytes<4>(oldGuild);

    player->GetSession()->SendPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_INVITE)");
}

/**
 * @brief Removes a member from the current guild.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildRemoveOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_REMOVE");

    std::string plName;
    ObjectGuid targetGuid;
    // Writer sub_C868E0 (thunk sub_C84F18).
    recvPacket.ReadGuidMask<7, 3, 4, 2, 5, 6, 1, 0>(targetGuid);
    recvPacket.ReadGuidBytes<0, 2, 5, 6, 7, 1, 4, 3>(targetGuid);

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_REMOVE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (!sObjectMgr.GetPlayerNameByGUID(targetGuid, plName))
    {
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(targetGuid);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->RankId == GR_GUILDMASTER)
    {
        SendGuildCommandResult(GUILD_QUIT_S, "", ERR_GUILD_LEADER_LEAVE);
        return;
    }

    // do not allow to kick player with same or higher rights
    if (GetPlayer()->GetRank() >= slot->RankId)
    {
        SendGuildCommandResult(GUILD_QUIT_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    // do not delete guilds that level are higher than undeletable
    if (guild->GetMemberSize() == 1 && guild->GetLevel() >= sWorld.getConfig(CONGIG_UINT32_GUILD_UNDELETABLE_LEVEL))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_UNDELETABLE_DUE_TO_LEVEL);
        return;
    }

    ObjectGuid const removedGuid = slot->guid;
    std::string const removedName = slot->Name;

    // possible last member removed, do cleanup, and no need events
    if (guild->DelMember(removedGuid))
    {
        guild->Disband();
        delete guild;
        return;
    }

    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_UNINVITE_PLAYER,
        GetPlayer()->GetObjectGuid(), removedGuid);

    guild->BroadcastMemberRemoved(removedGuid, removedName,
        _player->GetObjectGuid(), _player->GetName());
}

/**
 * @brief Accepts a pending guild invitation.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildAcceptOpcode(WorldPacket& /*recvPacket*/)
{
    Guild* guild;
    Player* player = GetPlayer();

    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ACCEPT");

    guild = sGuildMgr.GetGuildById(player->GetGuildIdInvited());
    if (!guild || player->GetGuildId())
    {
        return;
    }

    // not let enemies sign guild charter
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && player->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(guild->GetLeaderGuid()))
    {
        return;
    }

    if (!guild->AddMember(GetPlayer()->GetObjectGuid(), guild->GetLowestRank()))
    {
        return;
    }
    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_JOIN_GUILD, GetPlayer()->GetObjectGuid());

    guild->BroadcastMemberJoined(player->GetObjectGuid(), player->GetName());
}

/**
 * @brief Declines a pending guild invitation.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildDeclineOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode %s", LookupOpcodeName(DIR_CLIENT, recvPacket.GetOpcode()));
    if (GetPlayer()->GetGuildId())
    {
        return;
    }

    if (Player* inviter = sObjectMgr.GetPlayer(GetPlayer()->GetGuildInviterGuid()))
    {
        inviter->SendGuildDeclined(GetPlayer()->GetName(), recvPacket.GetOpcode() == CMSG_GUILD_AUTO_DECLINE);
    }

    GetPlayer()->SetGuildIdInvited(0);
    GetPlayer()->SetGuildLevel(0);
}

/**
 * @brief Sends the guild roster to the current player.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildRosterOpcode(WorldPacket& recvPacket)
{
    ObjectGuid guid1, guid2;

    // Order is taken from the 18414 client's own send serializer sub_C85E7C. It holds
    // two guids at object offsets +16..23 (guid1) and +24..31 (guid2) and interleaves
    // them; the order this handler inherited from MaNGOS Three was a different
    // permutation entirely.
    uint64 rawGuid1 = 0;
    uint64 rawGuid2 = 0;
    MopGuildPackets::ReadGuildRoster(recvPacket, rawGuid1, rawGuid2);
    guid1.Set(rawGuid1);
    guid2.Set(rawGuid2);

    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ROSTER, guid1: %s raw: " UI64FMTD ", guid2: %s raw: " UI64FMTD "",
        guid1.GetString().c_str(), guid1.GetRawValue(), guid2.GetString().c_str(), guid2.GetRawValue());

    if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId()))
    {
        guild->Roster(this);
    }
}

/**
 * @brief Promotes a guild member to a higher rank.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildPromoteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_PROMOTE");

    std::string plName;
    ObjectGuid targetGuid;
    // Writer sub_C85476 (thunk sub_C849CD). The permutations below were wrong in
    // both runs and are re-derived from it; IDA and Binary Ninja agree exactly.
    recvPacket.ReadGuidMask<6, 0, 4, 3, 1, 7, 2, 5>(targetGuid);
    recvPacket.ReadGuidBytes<1, 7, 2, 5, 3, 4, 0, 6>(targetGuid);

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_PROMOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (!sObjectMgr.GetPlayerNameByGUID(targetGuid, plName))
    {
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(targetGuid);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == GetPlayer()->GetObjectGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_NAME_INVALID);
        return;
    }

    // allow to promote only to lower rank than member's rank
    // guildmaster's rank = 0
    // GetPlayer()->GetRank() + 1 is highest rank that current player can promote to
    if (GetPlayer()->GetRank() + 1 >= slot->RankId)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    uint32 newRankId = slot->RankId - 1;                    // when promoting player, rank is decreased

    slot->ChangeRank(newRankId);
    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_PROMOTE_PLAYER, GetPlayer()->GetObjectGuid(), slot->guid, newRankId);

    guild->BroadcastMemberRankUpdate(_player->GetObjectGuid(), slot->guid,
        newRankId, true);
}

/**
 * @brief Demotes a guild member to a lower rank.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildDemoteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DEMOTE");

    std::string plName;
    ObjectGuid targetGuid;
    // Writer sub_C86553 (thunk sub_C84E1B). Note this permutation differs from
    // GUILD_PROMOTE's and GUILD_REMOVE's -- MoP randomises it per opcode, so no
    // sibling's order may be carried across.
    recvPacket.ReadGuidMask<3, 6, 0, 2, 7, 5, 4, 1>(targetGuid);
    recvPacket.ReadGuidBytes<7, 4, 2, 5, 1, 3, 0, 6>(targetGuid);

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_DEMOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (!sObjectMgr.GetPlayerNameByGUID(targetGuid, plName))
    {
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(targetGuid);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == GetPlayer()->GetObjectGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_NAME_INVALID);
        return;
    }

    // do not allow to demote same or higher rank
    if (GetPlayer()->GetRank() >= slot->RankId)
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_HIGH_S);
        return;
    }

    // do not allow to demote lowest rank
    if (slot->RankId >= guild->GetLowestRank())
    {
        SendGuildCommandResult(GUILD_INVITE_S, plName, ERR_GUILD_RANK_TOO_LOW_S);
        return;
    }

    uint32 newRankId = slot->RankId + 1;                    // when demoting player, rank is increased

    slot->ChangeRank(newRankId);
    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_DEMOTE_PLAYER, GetPlayer()->GetObjectGuid(), slot->guid, newRankId);

    guild->BroadcastMemberRankUpdate(_player->GetObjectGuid(), slot->guid,
        newRankId, false);
}

void WorldSession::HandleGuildSetRankOpcode(WorldPacket& recvPacket)
{
    // Rebuilt from writer sub_C866DC (thunk sub_C84EA8). The previous reader took
    // a rank id and two bit-packed GUIDs -- a member rank ASSIGNMENT -- which is
    // a different packet entirely. This one DEFINES a rank.
    //
    // No capture of this opcode exists in any build, so every field is named by
    // the builder route, from sub_9679B8 which fills the object the writer
    // serialises:
    //   the eight pairs are (tab RIGHTS, tab SLOTS-PER-DAY) -- the Lua setter
    //     SetGuildBankTabItemWithdraw writes the second array, clamped to 100000;
    //   rights is the field GuildControlGetRankFlags reads (client record +0),
    //     and the writer emits it TWICE, at +20 and +100;
    //   money-per-day is the field SetGuildBankWithdrawGoldLimit writes
    //     (record +12), emitted at +96.
    uint32 rankId, rights, moneyPerDay;
    uint32 tabRights[GUILD_BANK_MAX_TABS] = {};
    uint32 tabSlots[GUILD_BANK_MAX_TABS] = {};

    recvPacket >> rankId;                                   // client rank identifier
    for (uint8 tab = 0; tab < GUILD_BANK_MAX_TABS; ++tab)
    {
        recvPacket >> tabRights[tab];
        recvPacket >> tabSlots[tab];
    }
    recvPacket >> moneyPerDay;
    recvPacket >> rights;
    recvPacket.read_skip<uint32>();                         // rights again, +100
    uint32 rankIndex;
    recvPacket >> rankIndex;

    uint32 const nameLen = recvPacket.ReadBits(7);
    if (recvPacket.rpos() + nameLen > recvPacket.size())
    {
        sLog.outError("CMSG_GUILD_SET_RANK: %s sent a %u byte rank name with %zu remaining, refusing",
                      GetPlayer()->GetGuidStr().c_str(), nameLen, recvPacket.size() - recvPacket.rpos());
        recvPacket.rfinish();
        return;
    }
    std::string rankName = recvPacket.ReadString(nameLen);

    DEBUG_LOG("WORLD: Received CMSG_GUILD_SET_RANK rank %u (index %u) rights %u money %u name %s",
              rankId, rankIndex, rights, moneyPerDay, rankName.c_str());

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    // Only the leader may redefine ranks. HasRankRight is not enough here: this
    // packet can grant any right to any rank, so anyone able to send it could
    // grant themselves everything.
    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (rankIndex >= guild->GetRanksSize())
    {
        return;
    }

    // The guildmaster rank IS editable -- the retail rank UI renames it, and
    // GuildDefaultRanks says so explicitly -- so it cannot simply be rejected.
    // But SetRankRights, unlike SetBankMoneyPerDay and SetBankRightsAndSlots,
    // does not force the guildmaster back to full rights, so a packet naming
    // rank 0 with rights=0 would lock the guild out of its own management.
    // Force the invariant instead of refusing the packet: the rename still
    // lands, the rights cannot be dropped.
    if (rankIndex == GR_GUILDMASTER)
    {
        // OR rather than assign: the load path does the same (Guild.cpp), and
        // assigning would strip any stored extension bits on what may be nothing
        // more than a rename.
        rights |= GR_RIGHT_ALL;
    }

    // Match the client's own ceiling. It clamps both of these to 100000 before
    // sending, so a larger value did not come from the stock UI; storing one
    // would grant effectively unlimited withdrawal.
    if (moneyPerDay > GUILD_WITHDRAW_MONEY_CLIENT_MAX)
    {
        moneyPerDay = GUILD_WITHDRAW_MONEY_CLIENT_MAX;
    }
    for (uint8 tab = 0; tab < GUILD_BANK_MAX_TABS; ++tab)
    {
        if (tabSlots[tab] > GUILD_WITHDRAW_SLOTS_CLIENT_MAX)
        {
            tabSlots[tab] = GUILD_WITHDRAW_SLOTS_CLIENT_MAX;
        }
    }

    guild->SetRankName(rankIndex, rankName);
    guild->SetRankRights(rankIndex, rights);
    guild->SetBankMoneyPerDay(rankIndex, moneyPerDay);
    for (uint8 tab = 0; tab < GUILD_BANK_MAX_TABS; ++tab)
    {
        guild->SetBankRightsAndSlots(rankIndex, tab, tabRights[tab], tabSlots[tab], true);
    }

    // Rank definitions are guild-wide state, so every member needs the update --
    // not just the leader who sent it. Roster(this) alone left everyone else
    // showing the old rank names and rights until they relogged.
    guild->BroadcastRankDefinitions();
}

void WorldSession::HandleGuildSwitchRankOpcode(WorldPacket& recvPacket)
{
    uint32 rankId;
    bool up;

    recvPacket >> rankId;
    up = recvPacket.ReadBit();

    DEBUG_LOG("WORLD: Received CMSG_GUILD_SWITCH_RANK rank %u up %u", rankId, up);

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (!guild->SwitchRank(rankId, up))
    {
        return;
    }

    // Reordering ranks changes two definitions, so every member needs them --
    // Roster alone carries only rank IDs. Same gap as CMSG_GUILD_SET_RANK had.
    guild->BroadcastRankDefinitions();
}

/**
 * @brief Removes the current player from the guild.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildLeaveOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_LEAVE");

    Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (_player->GetObjectGuid() == guild->GetLeaderGuid() && guild->GetMemberSize() > 1)
    {
        SendGuildCommandResult(GUILD_QUIT_S, "", ERR_GUILD_LEADER_LEAVE);
        return;
    }

    // do not delete guilds that level are higher than undeletable
    if (guild->GetMemberSize() == 1 && guild->GetLevel() >= sWorld.getConfig(CONGIG_UINT32_GUILD_UNDELETABLE_LEVEL))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_UNDELETABLE_DUE_TO_LEVEL);
        return;
    }

    sCalendarMgr.RemoveGuildCalendar(_player->GetObjectGuid(), guild->GetId());

    if (_player->GetObjectGuid() == guild->GetLeaderGuid())
    {
        guild->Disband();
        delete guild;
        return;
    }

    SendGuildCommandResult(GUILD_QUIT_S, guild->GetName(), ERR_PLAYER_NO_MORE_IN_GUILD);

    if (guild->DelMember(_player->GetObjectGuid()))
    {
        guild->Disband();
        delete guild;
        return;
    }

    // Put record into guild log
    guild->LogGuildEvent(GUILD_EVENT_LOG_LEAVE_GUILD, _player->GetObjectGuid());

    guild->BroadcastMemberLeft(_player->GetObjectGuid(), _player->GetName());
}

/**
 * @brief Disbands the current guild if the player is its leader.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildDisbandOpcode(WorldPacket& /*recvPacket*/)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DISBAND");

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    // do not delete guilds that level are higher than undeletable
    if (guild->GetMemberSize() == 1 && guild->GetLevel() >= sWorld.getConfig(CONGIG_UINT32_GUILD_UNDELETABLE_LEVEL))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_UNDELETABLE_DUE_TO_LEVEL);
        return;
    }

    guild->Disband();
    delete guild;

    DEBUG_LOG("WORLD: Guild Successfully Disbanded");
}

/**
 * @brief Transfers guild leadership to another member.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildLeaderOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_LEADER");

    std::string name = recvPacket.ReadString(recvPacket.ReadBits(7));

    Player* oldLeader = GetPlayer();

    // Promoting from the guild roster hands us "Name-Realm"; same strip the
    // guild INVITE path above already performs.
    StripHomeRealmSuffix(name);

    if (!normalizePlayerName(name))
    {
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(oldLeader->GetGuildId());

    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (oldLeader->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* oldSlot = guild->GetMemberSlot(oldLeader->GetObjectGuid());
    if (!oldSlot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(name);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, name, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (slot->guid == oldLeader->GetObjectGuid())
    {
        return;
    }

    guild->SetLeader(slot->guid);
    // NOTE: GR_OFFICER might not actually be officer rank
    oldSlot->ChangeRank(GR_OFFICER);

    guild->BroadcastNewLeader(oldLeader->GetObjectGuid(), oldLeader->GetName(),
        slot->guid, slot->Name);
}

/**
 * @brief Updates the guild message of the day.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildMOTDOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_MOTD");

    // Writer sub_C872E6 -> sub_66B79F: sub_665185 writes the top EIGHT bits and
    // sub_664CD1 the low TWO, so the length field is 10 bits, not 11. Reading one
    // bit too many shifts the length and desynchronises the string after it.
    std::string MOTD = recvPacket.ReadString(recvPacket.ReadBits(10));

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }
    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_SETMOTD))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->SetMOTD(MOTD);

    guild->BroadcastMotd(MOTD);
}

/**
 * @brief Updates a guild member's public note.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildSetNoteOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_SET_NOTE");

    std::string name, note;
    ObjectGuid targetGuid;
    bool isPublic = false;

    // Parsed by MopGuildPackets::ParseGuildSetNote, which carries the derivation
    // and is driven by real captured bodies in mop_guild_packets. A set flag is
    // the PUBLIC note, so officer is its inverse.
    MopGuildPackets::ParseGuildSetNote(recvPacket, targetGuid, isPublic, note);
    bool const officer = !isPublic;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), officer ? GR_RIGHT_EOFFNOTE : GR_RIGHT_EPNOTE))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (!sObjectMgr.GetPlayerNameByGUID(targetGuid, name))
    {
        return;
    }

    MemberSlot* slot = guild->GetMemberSlot(targetGuid);
    if (!slot)
    {
        SendGuildCommandResult(GUILD_INVITE_S, name, ERR_GUILD_PLAYER_NOT_IN_GUILD_S);
        return;
    }

    if (officer)
    {
        slot->SetOFFNOTE(note);
    }
    else
    {
        slot->SetPNOTE(note);
    }

    guild->Roster(this);
}


/**
 * @brief Adds a new guild rank.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildAddRankOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_ADD_RANK");

    recvPacket >> Unused<uint32>(); // rank id
    std::string rankname = recvPacket.ReadString(recvPacket.ReadBits(7));

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    if (guild->GetRanksSize() >= GUILD_RANKS_MAX_COUNT)     // client not let create more 10 than ranks
    {
        return;
    }

    guild->CreateRank(rankname, GR_RIGHT_GCHATLISTEN | GR_RIGHT_GCHATSPEAK);

    guild->BroadcastRankDefinitions();
}

/**
 * @brief Deletes the lowest removable guild rank.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildDelRankOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_DEL_RANK");

    uint32 rankId;
    recvPacket >> rankId;

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (GetPlayer()->GetObjectGuid() != guild->GetLeaderGuid())
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    // do not allow delete rank if there are still members using it
    if (guild->HasMembersWithRank(rankId))
    {
        SendGuildCommandResult(GUILD_INVITE_S, "", ERR_GUILD_RANK_IN_USE);
        return;
    }

    if (!guild->DelRank(rankId))
    {
        return;
    }

    guild->BroadcastRankDefinitions();
}

/**
 * @brief Sends the result of a guild command back to the client.
 *
 * @param typecmd The guild command type.
 * @param str The related player or guild name.
 * @param cmdresult The result code.
 */
void WorldSession::SendGuildCommandResult(uint32 typecmd, const std::string& str, uint32 cmdresult)
{
    WorldPacket data;
    if (!MopGuildPackets::BuildGuildCommandResult(data, typecmd, str, cmdresult))
    {
        sLog.outError("WORLD: Guild command result name is too long (%u bytes)", uint32(str.size()));
        return;
    }
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent (SMSG_GUILD_COMMAND_RESULT)");
}

/**
 * @brief Updates the guild information text.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildChangeInfoTextOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_GUILD_INFO_TEXT");

    // Writer sub_C87297 -> sub_66B7DA: eight bits via sub_665185 plus three via
    // sub_664D4F, so 11 bits and not 12.
    std::string GINFO = recvPacket.ReadString(recvPacket.ReadBits(11));

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
        return;
    }

    if (!guild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_MODIFY_GUILD_INFO))
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_PERMISSIONS);
        return;
    }

    guild->SetGINFO(GINFO);
}

/**
 * @brief Saves a new guild emblem design.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleSaveGuildEmblemOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SAVE_GUILD_EMBLEM");

    MopGuildPackets::EmblemDesign const design = MopGuildPackets::ReadSaveGuildEmblem(recvPacket);
    ObjectGuid const vendorGuid(design.vendorGuid);

    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(vendorGuid, UNIT_NPC_FLAG_TABARDDESIGNER);
    if (!pCreature)
    {
        //"That's not an emblem vendor!"
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_INVALIDVENDOR);
        DEBUG_LOG("WORLD: HandleSaveGuildEmblemOpcode - %s not found or you can't interact with him.", vendorGuid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    if (design.emblemStyle >= 196 || design.emblemColor >= 17 ||
        design.borderStyle >= 6 || design.borderColor >= 17 ||
        design.backgroundColor >= 51)
    {
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_INVALID_TABARD_COLORS);
        return;
    }

    Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId());
    if (!guild)
    {
        //"You are not part of a guild!";
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOGUILD);
        return;
    }

    if (guild->GetLeaderGuid() != GetPlayer()->GetObjectGuid())
    {
        //"Only guild leaders can create emblems."
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTGUILDMASTER);
        return;
    }

    if (GetPlayer()->GetMoney() < 10 * GOLD)
    {
        //"You can't afford to do that."
        SendSaveGuildEmblem(ERR_GUILDEMBLEM_NOTENOUGHMONEY);
        return;
    }

    GetPlayer()->ModifyMoney(-10 * GOLD);
    guild->SetEmblem(design.emblemStyle, design.emblemColor, design.borderStyle,
        design.borderColor, design.backgroundColor);

    //"Guild Emblem saved."
    SendSaveGuildEmblem(ERR_GUILDEMBLEM_SUCCESS);

    guild->Query(this);
}

/**
 * @brief Sends the guild event log to the current player.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::HandleGuildEventLogQueryOpcode(WorldPacket& /* recvPacket */)
{
    // empty
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_EVENT_LOG_QUERY)");

    if (uint32 GuildId = GetPlayer()->GetGuildId())
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
        {
            pGuild->DisplayGuildEventLog(this);
        }
}

/******  GUILD BANK  *******/

void WorldSession::HandleGuildBankMoneyWithdrawn(WorldPacket& /* recv_data */)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_MONEY_WITHDRAWN)");

    if (uint32 GuildId = GetPlayer()->GetGuildId())
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
        {
            pGuild->SendMoneyInfo(this, GetPlayer()->GetGUIDLow());
        }
}

void WorldSession::HandleGuildPermissions(WorldPacket& /* recv_data */)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_PERMISSIONS)");

    if (uint32 GuildId = GetPlayer()->GetGuildId())
    {
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
        {
            uint32 rankId = GetPlayer()->GetRank();

            // Layout is fixed by retail capture (capture-000006 seq 1959, 83 bytes,
            // and 2,080 packets all exactly 83). The inherited body had three separate
            // faults, each visible in those bytes:
            //
            //   field order   bytes 0..15 are 05 00000000 07 00106053. Read as
            //                 rank/money/tabs/rights that is rank 5, no money left,
            //                 7 purchased tabs and a rights mask -- and the tab array
            //                 below indeed carries rights on 7 tabs and 0 on the 8th.
            //                 Read as the old rank/tabs/rights/money it claims 0
            //                 purchased tabs while still describing 7 of them.
            //   count width   bytes 16..18 are 00 00 40. At 21 bits that is 8, which is
            //                 GUILD_BANK_MAX_TABS. At the old 23 bits it is 32.
            //   tab pairing   the pairs run (4,3) (3,3) (2,3) (1,3) (0,3) (0,3) (0,3)
            //                 (0,0), i.e. remaining slots first and rights second, not
            //                 the other way round.
            uint32 remainingSlots[GUILD_BANK_MAX_TABS];
            uint32 tabRights[GUILD_BANK_MAX_TABS];
            for (uint8 tab = 0; tab < GUILD_BANK_MAX_TABS; ++tab)
            {
                remainingSlots[tab] = pGuild->GetMemberSlotWithdrawRem(GetPlayer()->GetGUIDLow(), tab);
                tabRights[tab] = pGuild->GetBankRights(rankId, tab);
            }

            WorldPacket data;
            MopGuildPackets::BuildGuildPermissions(data, rankId,
                pGuild->GetMemberMoneyWithdrawRem(GetPlayer()->GetGUIDLow()),
                pGuild->GetPurchasedTabs(), pGuild->GetRankRights(rankId),
                remainingSlots, tabRights);
            SendPacket(&data);
            DEBUG_LOG("WORLD: Sent (SMSG_GUILD_PERMISSIONS)");
        }
    }
}

/* Called when clicking on Guild bank gameobject */
void WorldSession::HandleGuildBankerActivate(WorldPacket& recv_data)
{
    ObjectGuid goGuid;
    bool fullSlotRefresh = false;
    if (!MopCompactPackets::ReadGuildBankerActivate(
            recv_data, goGuid, fullSlotRefresh))
    {
        DEBUG_LOG("WORLD: Rejected malformed CMSG_GUILD_BANKER_ACTIVATE");
        return;
    }

    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANKER_ACTIVATE) FullSlotRefresh %u",
        uint32(fullSlotRefresh));

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        return;
    }

    if (uint32 GuildId = GetPlayer()->GetGuildId())
    {
        if (Guild* pGuild = sGuildMgr.GetGuildById(GuildId))
        {
            pGuild->DisplayGuildBankTabsInfo(this);         // this also will load guild bank if not yet
            return;
        }
    }

    SendGuildCommandResult(GUILD_UNK1, "", ERR_GUILD_PLAYER_NOT_IN_GUILD);
}

/* Called when opening guild bank tab only (first one) */
void WorldSession::HandleGuildBankQueryTab(WorldPacket& recv_data)
{
    // 18414 leads with the tab id; the "unk1" that followed the raw GUID is a
    // single bit inside the mask, asking for every slot rather than a page.
    uint8 TabId;
    bool sendAllSlots = false;
    ObjectGuid goGuid = MopCompactPackets::ReadGuildBankQueryTab(recv_data, TabId, sendAllSlots);

    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_QUERY_TAB) TabId %u", TabId);

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        return;
    }

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        return;
    }

    if (TabId >= pGuild->GetPurchasedTabs())
    {
        return;
    }

    pGuild->DisplayGuildBankContent(this, TabId, sendAllSlots);
}

void WorldSession::HandleGuildBankDepositMoney(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_DEPOSIT_MONEY)");

    ObjectGuid goGuid;
    uint64 money;
    recv_data >> goGuid >> money;

    if (!money)
    {
        return;
    }

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        return;
    }

    if (GetPlayer()->GetMoney() < money)
    {
        return;
    }

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        return;
    }

    CharacterDatabase.BeginTransaction();

    pGuild->SetBankMoney(pGuild->GetGuildBankMoney() + money);
    GetPlayer()->ModifyMoney(-int64(money));
    GetPlayer()->SaveGoldToDB();

    CharacterDatabase.CommitTransaction();

    // logging money
    if (_player->GetSession()->GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
    {
        sLog.outCommand(_player->GetSession()->GetAccountId(), "GM %s (Account: %u) deposit money (Amount: %u) to guild bank (Guild ID %u)",
                        _player->GetName(), _player->GetSession()->GetAccountId(), money, GuildId);
    }

    // log
    pGuild->LogBankEvent(GUILD_BANK_LOG_DEPOSIT_MONEY, uint8(0), GetPlayer()->GetGUIDLow(), money);

#ifdef ENABLE_ELUNA
    // TODO: ELUNAFIX NEEDED
    //if (Eluna* e = xxx->GetEluna())
    //{
    //    e->OnMemberDepositMoney(pGuild, GetPlayer(), money);
    //}
#endif

    pGuild->DisplayGuildBankTabsInfo(this, 0);
    pGuild->DisplayGuildBankMoneyUpdate();
}

void WorldSession::HandleGuildBankWithdrawMoney(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_WITHDRAW_MONEY)");

    ObjectGuid goGuid;
    uint64 money;
    recv_data >> goGuid >> money;

    if (!money)
    {
        return;
    }

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        return;
    }

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (GuildId == 0)
    {
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        return;
    }

    if (pGuild->GetGuildBankMoney() < money)                // not enough money in bank
    {
        return;
    }

    if (!pGuild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_WITHDRAW_GOLD))
    {
        return;
    }

    CharacterDatabase.BeginTransaction();

    if (!pGuild->MemberMoneyWithdraw(money, GetPlayer()->GetGUIDLow()))
    {
        CharacterDatabase.RollbackTransaction();
        return;
    }

    GetPlayer()->ModifyMoney(money);
    GetPlayer()->SaveGoldToDB();

    CharacterDatabase.CommitTransaction();

    // Log
    pGuild->LogBankEvent(GUILD_BANK_LOG_WITHDRAW_MONEY, uint8(0), GetPlayer()->GetGUIDLow(), money);

    pGuild->SendMoneyInfo(this, GetPlayer()->GetGUIDLow());
    pGuild->DisplayGuildBankTabsInfo(this, 0);
    pGuild->DisplayGuildBankMoneyUpdate();
}

void WorldSession::HandleGuildBankSwapItems(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_SWAP_ITEMS)");

    ObjectGuid goGuid;
    uint8 BankToBank;

    uint8 BankTab, BankTabSlot, AutoStore;
    uint8 PlayerSlot = NULL_SLOT;
    uint8 PlayerBag = NULL_BAG;
    uint8 BankTabDst, BankTabSlotDst, unk2;
    uint8 ToChar = 1;
    uint32 ItemEntry, unk1;
    uint32 AutoStoreCount = 0;
    uint32 SplitedAmount = 0;

    recv_data >> goGuid >> BankToBank;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        recv_data.rfinish();                                // prevent additional spam at rejected packet
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        recv_data.rfinish();                                // prevent additional spam at rejected packet
        return;
    }

    if (BankToBank)
    {
        recv_data >> BankTabDst;
        recv_data >> BankTabSlotDst;
        recv_data >> unk1;                                  // always 0
        recv_data >> BankTab;
        recv_data >> BankTabSlot;
        recv_data >> ItemEntry;
        recv_data >> unk2;                                  // always 0
        recv_data >> SplitedAmount;

        if (BankTabSlotDst >= GUILD_BANK_MAX_SLOTS ||
                (BankTabDst == BankTab && BankTabSlotDst == BankTabSlot) ||
                BankTab >= pGuild->GetPurchasedTabs() ||
                BankTabDst >= pGuild->GetPurchasedTabs())
        {
            recv_data.rfinish();                            // prevent additional spam at rejected packet
            return;
        }
    }
    else
    {
        recv_data >> BankTab;
        recv_data >> BankTabSlot;
        recv_data >> ItemEntry;
        recv_data >> AutoStore;
        if (AutoStore)
        {
            recv_data >> AutoStoreCount;
            recv_data.read_skip<uint8>();                   // ToChar (?), always and expected to be 1 (autostore only triggered in guild->ToChar)
            recv_data.read_skip<uint32>();                  // unknown, always 0
        }
        else
        {
            recv_data >> PlayerBag;
            recv_data >> PlayerSlot;
            recv_data >> ToChar;
            recv_data >> SplitedAmount;
        }

        if ((BankTabSlot >= GUILD_BANK_MAX_SLOTS && BankTabSlot != 0xFF) ||
                BankTab >= pGuild->GetPurchasedTabs())
        {
            recv_data.rfinish();                            // prevent additional spam at rejected packet
            return;
        }
    }

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        return;
    }

    // Bank <-> Bank
    if (BankToBank)
    {
        pGuild->SwapItems(_player, BankTab, BankTabSlot, BankTabDst, BankTabSlotDst, SplitedAmount);
        return;
    }

    // Player <-> Bank

    // allow work with inventory only
    if (!Player::IsInventoryPos(PlayerBag, PlayerSlot) && !(PlayerBag == NULL_BAG && PlayerSlot == NULL_SLOT))
    {
        _player->SendEquipError(EQUIP_ERR_NONE, NULL, NULL);
        return;
    }

    // BankToChar swap or char to bank remaining
    if (ToChar)                                             // Bank -> Char cases
    {
        pGuild->MoveFromBankToChar(_player, BankTab, BankTabSlot, PlayerBag, PlayerSlot, SplitedAmount);
    }
    else                                                    // Char -> Bank cases
    {
        pGuild->MoveFromCharToBank(_player, PlayerBag, PlayerSlot, BankTab, BankTabSlot, SplitedAmount);
    }
}

void WorldSession::HandleGuildBankBuyTab(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_BUY_TAB)");

    ObjectGuid goGuid;
    uint8 TabId;

    recv_data >> goGuid;
    recv_data >> TabId;

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        return;
    }

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        return;
    }

    // m_PurchasedTabs = 0 when buying Tab 0, that is why this check can be made
    // also don't allow buy tabs that are obtained through guild perks
    if (TabId != pGuild->GetPurchasedTabs() || TabId >= GUILD_BANK_MAX_BOUGHT_TABS)
    {
        return;
    }

    uint64 TabCost = GetGuildBankTabPrice(TabId) * GOLD;
    if (!TabCost)
    {
        return;
    }

    if (GetPlayer()->GetMoney() < TabCost)                  // Should not happen, this is checked by client
    {
        return;
    }

    // Go on with creating tab
    pGuild->CreateNewBankTab();
    GetPlayer()->ModifyMoney(-int64(TabCost));
    pGuild->SetBankRightsAndSlots(GetPlayer()->GetRank(), TabId, GUILD_BANK_RIGHT_FULL, WITHDRAW_SLOT_UNLIMITED, true);
    pGuild->Roster();                                       // broadcast for tab rights update
    pGuild->DisplayGuildBankTabsInfo(this, TabId);
}

void WorldSession::HandleGuildBankUpdateTab(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_UPDATE_TAB)");

    ObjectGuid goGuid;
    uint8 TabId;
    std::string Name;
    std::string IconIndex;

    recv_data >> goGuid;
    recv_data >> TabId;
    recv_data >> Name;
    recv_data >> IconIndex;

    if (Name.empty())
    {
        return;
    }

    if (IconIndex.empty())
    {
        return;
    }

    if (!GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK))
    {
        return;
    }

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        return;
    }

    if (TabId >= pGuild->GetPurchasedTabs())
    {
        return;
    }

    if (!pGuild->HasRankRight(GetPlayer()->GetRank(), GR_RIGHT_MODIFY_BANK_TABS))
    {
        return;
    }

    pGuild->SetGuildBankTabInfo(TabId, Name, IconIndex);
    pGuild->DisplayGuildBankTabsInfo(this, TabId);
}

void WorldSession::HandleGuildBankLogQuery(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received (CMSG_GUILD_BANK_LOG_QUERY)");

    uint32 TabId;
    recv_data >> TabId;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        return;
    }

    // GUILD_BANK_MAX_TABS send by client for money log
    if (TabId >= pGuild->GetPurchasedTabs() && TabId != GUILD_BANK_MAX_TABS)
    {
        return;
    }

    pGuild->DisplayGuildBankLogs(this, TabId);
}

void WorldSession::HandleQueryGuildBankTabText(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_QUERY_GUILD_BANK_TEXT");

    uint32 TabId;
    recv_data >> TabId;

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        return;
    }

    if (TabId >= pGuild->GetPurchasedTabs())
    {
        return;
    }

    pGuild->SendGuildBankTabText(this, TabId);
}

void WorldSession::HandleSetGuildBankTabText(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_SET_GUILD_BANK_TEXT");

    uint32 TabId;
    std::string Text;

    recv_data >> TabId;
    Text = recv_data.ReadString(recv_data.ReadBits(14));

    uint32 GuildId = GetPlayer()->GetGuildId();
    if (!GuildId)
    {
        return;
    }

    Guild* pGuild = sGuildMgr.GetGuildById(GuildId);
    if (!pGuild)
    {
        return;
    }

    if (TabId >= pGuild->GetPurchasedTabs())
    {
        return;
    }

    pGuild->SetGuildBankTabText(TabId, Text);
}

/**
 * @brief Sends the result of a guild emblem save request.
 *
 * @param msg The guild emblem result code.
 */
void WorldSession::SendSaveGuildEmblem(uint32 msg)
{
    WorldPacket data;
    MopGuildPackets::BuildSaveGuildEmblemResult(data, msg);
    SendPacket(&data);
}

void WorldSession::HandleGuildQueryRanksOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received CMSG_GUILD_QUERY_RANKS");

    // Order is taken from the 18414 client's own send serializer sub_C860F3, which
    // writes the mask bits from guid bytes 0,2,5,4,3,7,6,1 and then the present bytes
    // in order 6,0,1,7,3,2,5,4 (each XOR 1). The order this handler inherited from
    // MaNGOS Three is a different permutation, so it decoded the wrong guild.
    ObjectGuid guildGuid(MopGuildPackets::ReadGuildQueryRanks(recv_data));
    Guild* guild = sGuildMgr.GetGuildByGuid(guildGuid);
    if (!guild || guild->GetId() != GetPlayer()->GetGuildId() ||
        !guild->GetMemberSlot(GetPlayer()->GetObjectGuid()))
    {
        return;
    }

    guild->QueryRanks(this);
}

void WorldSession::HandleGuildSetAchievementTracking(WorldPacket& recvPacket)
{
    std::vector<uint32> achievementIds;
    if (!MopGuildPackets::ReadGuildAchievementTracking(recvPacket, achievementIds))
        return;

    // The current core has no guild-achievement tracking backend. Parse the
    // client's complete snapshot only as a bounded compatibility sink.
    // Intentionally no persistence or gameplay side effects.
    (void)achievementIds;
}

void WorldSession::HandleGuildAutoDeclineToggleOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received CMSG_GUILD_AUTO_DECLINE_TOGGLE");

    bool apply;
    recv_data >> apply;

    GetPlayer()->ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_AUTO_DECLINE_GUILDS, apply);
}
