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
 * @file PetitionsHandler.cpp
 * @brief Guild and arena charter opcode handlers
 *
 * This file handles petition-related opcodes for guild and arena charters:
 * - CMSG_PETITION_BUY: Buy guild/arena charter
 * - CMSG_PETITION_SHOW_SIGNATURES: Show charter signatures
 * - CMSG_PETITION_SIGN: Sign charter
 * - CMSG_PETITION_OFFER: Offer charter to player
 * - CMSG_PETITION_TURN_IN: Turn in completed charter
 * - CMSG_QUERY_PETITION: Query charter info
 *
 * Charters require a certain number of signatures before they can be
 * turned in to create a guild or arena team.
 */

#include "Common.h"
#include "Language.h"
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

// Charters ID in item_template
#define GUILD_CHARTER               5863
#define CHARTER_DISPLAY_ID          16161

/**
 * @brief Handles charter purchase and petition creation.
 *
 * @param recv_data The incoming petition-buy packet.
 */
void WorldSession::HandlePetitionBuyOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_BUY");
    recv_data.hexlike();

    // Writer sub_68A12F (thunk sub_686523, vtable slot +4). MoP collapsed this
    // packet to two fields: the charter name and the vendor GUID. Everything the
    // pre-MoP body read after the name -- a second string, eleven scalars and ten
    // more strings -- does not exist on the 18414 wire.
    //
    // The name LENGTH is a 7-bit field written INSIDE the GUID mask run, after
    // three mask bits; sub_664F47 is fixed at seven bits (its `v6 = v4 - 1` and
    // the accumulate branch setting the count to 7), and its third argument is
    // unused. The name bytes then go out immediately after the flush, ahead of
    // every GUID byte. Builder sub_9D904A names both fields: a 128-byte name
    // buffer at object +16, and the vendor GUID at +144 taken from the global the
    // client also range-checks its charter price against.
    ObjectGuid guidNPC;

    recv_data.ReadGuidMask<5, 2, 3>(guidNPC);
    uint32 const nameLength = recv_data.ReadBits(7);
    recv_data.ReadGuidMask<4, 1, 7, 0, 6>(guidNPC);

    // The client's own buffer is 128 bytes and it truncates at 127 plus a NUL, so
    // a 7-bit length can never legitimately exceed that. ReadString stops at the
    // end of the body and returns short rather than throwing, so check the bytes
    // are actually present instead of trusting the declared length.
    if (recv_data.rpos() + nameLength > recv_data.size())
    {
        sLog.outError("CMSG_PETITION_BUY: %s declared a %u byte name with %zu remaining, refusing",
                      GetPlayer()->GetObjectGuid().GetString().c_str(), nameLength, recv_data.size() - recv_data.rpos());
        recv_data.rfinish();
        return;
    }

    std::string name = recv_data.ReadString(nameLength);
    recv_data.ReadGuidBytes<1, 7, 4, 6, 0, 5, 2, 3>(guidNPC);

    DEBUG_LOG("Petitioner %s tried sell petition: name %s", guidNPC.GetString().c_str(), name.c_str());

    // prevent cheating
    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guidNPC, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionBuyOpcode - %s not found or you can't interact with him.", guidNPC.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    if (!pCreature->IsTabardDesigner())
    {
        sLog.outError("WORLD: HandlePetitionBuyOpcode - unsupported npc type, npc: %s", guidNPC.GetString().c_str());
        return;
    }

    if (sGuildMgr.GetGuildByName(name))
    {
        SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_EXISTS_S);
        return;
    }
    if (sObjectMgr.IsReservedName(name) || !ObjectMgr::IsValidCharterName(name))
    {
        SendGuildCommandResult(GUILD_CREATE_S, name, ERR_GUILD_NAME_INVALID);
        return;
    }

    ItemPrototype const* pProto = ObjectMgr::GetItemPrototype(GUILD_CHARTER);
    if (!pProto)
    {
        _player->SendBuyError(BUY_ERR_CANT_FIND_ITEM, NULL, GUILD_CHARTER, 0);
        return;
    }

    if (_player->GetMoney() < sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST))
    {
        // player hasn't got enough money
        _player->SendBuyError(BUY_ERR_NOT_ENOUGHT_MONEY, pCreature, GUILD_CHARTER, 0);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, GUILD_CHARTER, pProto->BuyCount);
    if (msg != EQUIP_ERR_OK)
    {
        _player->SendEquipError(msg, NULL, NULL, GUILD_CHARTER);
        return;
    }

    // Create the charter BEFORE taking the money. StoreNewItem can still fail
    // after CanStoreNewItem passed, and the old order burned the cost with no
    // item to show for it. Nothing between the two can change the balance --
    // same thread, no yield, and the petition transaction is opened further
    // down -- so this ordering is safe here and is not the commit-ambiguity
    // case, which needs a transaction to be ambiguous about.
    Item* charter = _player->StoreNewItem(dest, GUILD_CHARTER, true);
    if (!charter)
    {
        return;
    }
    _player->ModifyMoney(-int64(sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST)));

    charter->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1, charter->GetGUIDLow());
    // ITEM_FIELD_ENCHANTMENT_1_1 stores the guild petition id.
    // ITEM_FIELD_ENCHANTMENT_1_1+1 is current signatures count (showed on item)
    charter->SetState(ITEM_CHANGED, _player);
    _player->SendNewItem(charter, 1, true, false);

    // A second petition owned by this player is invalid data.
    QueryResult* result = CharacterDatabase.PQuery("SELECT `petitionguid` FROM `petition` WHERE `ownerguid` = '%u'", _player->GetGUIDLow());

    std::ostringstream ssInvalidPetitionGUIDs;

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ssInvalidPetitionGUIDs << "'" << fields[0].GetUInt32() << "' , ";
        }
        while (result->NextRow());

        delete result;
    }

    // delete petitions with the same guid as this one
    ssInvalidPetitionGUIDs << "'" << charter->GetGUIDLow() << "'";

    DEBUG_LOG("Invalid petition GUIDs: %s", ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.escape_string(name);
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` IN ( %s )",  ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` IN ( %s )", ssInvalidPetitionGUIDs.str().c_str());
    CharacterDatabase.PExecute("INSERT INTO `petition` (`ownerguid`, `petitionguid`, `name`) VALUES ('%u', '%u', '%s')",
                               _player->GetGUIDLow(), charter->GetGUIDLow(), name.c_str());
    CharacterDatabase.CommitTransaction();
}

/**
 * @brief Sends the current signature list for a petition.
 *
 * @param recv_data The incoming show-signatures packet.
 */
void WorldSession::HandlePetitionShowSignOpcode(WorldPacket& recv_data)
{
    // ok
    DEBUG_LOG("Received opcode CMSG_PETITION_SHOW_SIGNATURES");
    // recv_data.hexlike();

    // Writer sub_6890C5 (thunk sub_686220). A lone bit-packed GUID -- the
    // pre-MoP raw uint64 read eight bytes that are not laid out that way.
    uint8 signs = 0;
    ObjectGuid petitionguid;
    recv_data.ReadGuidMask<3, 7, 2, 4, 5, 6, 0, 1>(petitionguid);
    recv_data.ReadGuidBytes<2, 4, 5, 7, 1, 0, 3, 6>(petitionguid);

    // solve (possible) some strange compile problems with explicit use GUID_LOPART(petitionguid) at some GCC versions (wrong code optimization in compiler?)
    uint32 petitionguid_low = petitionguid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT 1 FROM `petition` WHERE `petitionguid` = '%u'", petitionguid_low);
    if (!result)
    {
        sLog.outError("any petition on server...");
        return;
    }
    delete result;

    // if has guild => error, return;
    if (_player->GetGuildId())
    {
        return;
    }

    result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionguid_low);

    // result==NULL also correct in case no sign yet
    if (result)
    {
        signs = (uint8)result->GetRowCount();
    }

    DEBUG_LOG("CMSG_PETITION_SHOW_SIGNATURES petition: %s", petitionguid.GetString().c_str());

    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + 1 + signs * 12));
    data << petitionguid;                                   // petition guid
    data << _player->GetObjectGuid();                       // owner guid
    data << uint32(petitionguid_low);                       // guild guid (in mangos always same as GUID_LOPART(petitionguid)
    data << uint8(signs);                                   // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << signerGuid;                                 // Player GUID
        data << uint32(0);                                  // there 0 ...

        result->NextRow();
    }
    delete result;
    SendPacket(&data);
}

/**
 * @brief Handles a petition query request.
 *
 * @param recv_data The incoming petition query packet.
 */
void WorldSession::HandlePetitionQueryOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_QUERY");
    // recv_data.hexlike();

    // Writer sub_6944E5 (thunk sub_690DC4). The uint32 keeps its leading position
    // and its width, so it is the one field here the pre-MoP body got right; the
    // GUID after it is bit-packed.
    uint32 guildguid;
    ObjectGuid petitionguid;
    recv_data >> guildguid;                                 // in mangos always same as GUID_LOPART(petitionguid)
    recv_data.ReadGuidMask<2, 3, 1, 0, 4, 7, 6, 5>(petitionguid);
    recv_data.ReadGuidBytes<0, 4, 7, 5, 1, 6, 3, 2>(petitionguid);
    DEBUG_LOG("CMSG_PETITION_QUERY Petition %s Guild GUID %u", petitionguid.GetString().c_str(), guildguid);

    SendPetitionQueryOpcode(petitionguid);
}

/**
 * @brief Sends petition metadata for a specific petition item.
 *
 * @param petitionguid The petition guid.
 */
void WorldSession::SendPetitionQueryOpcode(ObjectGuid petitionguid)
{
    uint32 petitionLowGuid = petitionguid.GetCounter();

    ObjectGuid ownerGuid;
    std::string name = "NO_NAME_FOR_GUID";
    uint8 signs = 0;

    QueryResult* result = CharacterDatabase.PQuery(
                              "SELECT `ownerguid`, `name`, "
                              "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
                              "FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid, petitionLowGuid);

    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name      = fields[1].GetCppString();
        signs     = fields[2].GetUInt8();
        delete result;
    }
    else
    {
        DEBUG_LOG("CMSG_PETITION_QUERY failed for petition (GUID: %u)", petitionLowGuid);
        return;
    }

    WorldPacket data(SMSG_PETITION_QUERY_RESPONSE, (4 + 8 + name.size() + 1 + 1 + 4 * 12 + 2 + 10));
    data << uint32(petitionLowGuid);                        // guild/team guid (in mangos always same as GUID_LOPART(petition guid)
    data << ObjectGuid(ownerGuid);                          // charter owner guid
    data << name;                                           // name (guild/arena team)
    data << uint8(0);                                       // some string
    data << uint32(4);
    data << uint32(4);
    data << uint32(0);                                      // bypass client - side limitation, a different value is needed here for each petition
    data << uint32(0);                                      // 5
    data << uint32(0);                                      // 6
    data << uint32(0);                                      // 7
    data << uint32(0);                                      // 8
    data << uint16(0);                                      // 9 2 bytes field
    data << uint32(0);                                      // 10
    data << uint32(0);                                      // 11
    data << uint32(0);                                      // 13 count of next strings?

    for (int i = 0; i < 10; ++i)
    {
        data << uint8(0);                                   // some string
    }

    data << uint32(0);                                      // 14
    data << uint32(0);                                      // 15 0 - guild, 1 - arena team

    SendPacket(&data);
}

/**
 * @brief Handles signing a guild petition.
 *
 * @param recv_data The incoming petition sign packet.
 */
void WorldSession::HandlePetitionSignOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_PETITION_SIGN");    // ok
    // recv_data.hexlike();

    // Writer sub_688009 (thunk sub_685E7A). The byte comes FIRST here and is
    // written with sub_40F018, not sub_40F075 -- the pre-MoP body had it trailing
    // the GUID, so both fields landed in the wrong place.
    Field* fields;
    ObjectGuid petitionGuid;
    uint8 unk;
    recv_data >> unk;
    recv_data.ReadGuidMask<4, 2, 0, 1, 5, 3, 6, 7>(petitionGuid);
    recv_data.ReadGuidBytes<6, 1, 7, 2, 5, 3, 0, 4>(petitionGuid);

    uint32 petitionLowGuid = petitionGuid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery(
                              "SELECT `ownerguid`, "
                              "  (SELECT COUNT(`playerguid`) FROM `petition_sign` WHERE `petition_sign`.`petitionguid` = '%u') AS `signs` "
                              "FROM `petition` WHERE `petitionguid` = '%u'", petitionLowGuid, petitionLowGuid);

    if (!result)
    {
        sLog.outError("any petition on server...");
        return;
    }

    fields = result->Fetch();
    uint32 ownerLowGuid = fields[0].GetUInt32();
    ObjectGuid ownerGuid = ObjectGuid(HIGHGUID_PLAYER, ownerLowGuid);
    uint8 signs = fields[1].GetUInt8();

    delete result;

    if (ownerGuid == _player->GetObjectGuid())
    {
        return;
    }

    // not let enemies sign guild charter
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) &&
            GetPlayer()->GetTeam() != sObjectMgr.GetPlayerTeamByGUID(ownerGuid))
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (_player->GetGuildId() || _player->GetGuildIdInvited())
    {
        // close at signer side
        _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_ALREADY_IN_GUILD);
        return;
    }

    /* todo: this needs to be handled properly */
    if (++signs > sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS))
    {
        // close at signer side
        _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_PETITION_FULL);
        return;
    }

    // client doesn't allow to sign petition two times by one character, but not check sign by another character from same account
    // not allow sign another player from already sign player account
    result = CharacterDatabase.PQuery("SELECT `playerguid`, `petitionguid` FROM `petition_sign` WHERE `player_account` = '%u'", GetAccountId());

    if (result)
    {
        fields = result->Fetch();
        ObjectGuid playerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        uint32 otherPetition = fields[1].GetUInt32();
        delete result;
        if (otherPetition == petitionGuid.GetCounter())
        {
            // close at signer side
            _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_ALREADY_SIGNED);

            // update for owner if online
            if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
            {
                owner->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_ALREADY_SIGNED);
            }
            return;
        }
        else if (playerGuid == _player->GetObjectGuid())
        {
            // close at signer side
            _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_ALREADY_SIGNED_OTHER);

            // update for owner if online
            if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
            {
                owner->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_ALREADY_SIGNED_OTHER);
            }
            return;
        }
    }

    CharacterDatabase.PExecute("INSERT INTO `petition_sign` (`ownerguid`,`petitionguid`, `playerguid`, `player_account`) VALUES ('%u', '%u', '%u','%u')",
                               ownerLowGuid, petitionLowGuid, _player->GetGUIDLow(), GetAccountId());

    DEBUG_LOG("PETITION SIGN: %s by %s", petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    // close at signer side
    _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_OK);

    // update signs count on charter, required testing...
    // Item *item = _player->GetItemByGuid(petitionguid));
    // if (item)
    //    item->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1+1, signs);

    // update for owner if online
    if (Player* owner = sObjectMgr.GetPlayer(ownerGuid))
    {
        owner->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_OK);
    }
}

/**
 * @brief Offers a petition to another player for signature.
 *
 * @param recv_data The incoming offer-petition packet.
 */
void WorldSession::HandleOfferPetitionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_OFFER_PETITION");   // ok
    // recv_data.hexlike();

    // Writer sub_668AB1 (thunk sub_661BFE). Two GUIDs whose sixteen mask bits and
    // sixteen bytes are fully INTERLEAVED, so neither run can be read as a block.
    //
    // Both are eight bytes, so layout alone cannot say which is which and the
    // sequences below would look correct either way round. Builder sub_96311E
    // settles it: the GUID at object +16 is loaded from the global holding the
    // open petition, while +24 takes the GUID of the unit the builder resolves,
    // level-checks against the charter's min/max, and rejects with error 375 when
    // it equals the local player -- that is the offer TARGET. The permutations
    // themselves were re-derived from Binary Ninja's disassembly independently of
    // the IDA reading, because a hand-copied sixteen-element sequence is exactly
    // what survives a self-check while being wrong.
    ObjectGuid petitionGuid;
    ObjectGuid playerGuid;
    uint32 junk;
    recv_data >> junk;                                      // this is not petition type!

    recv_data.ReadGuidMask<4>(playerGuid);
    recv_data.ReadGuidMask<1>(playerGuid);
    recv_data.ReadGuidMask<2>(petitionGuid);
    recv_data.ReadGuidMask<6>(playerGuid);
    recv_data.ReadGuidMask<1>(petitionGuid);
    recv_data.ReadGuidMask<2>(playerGuid);
    recv_data.ReadGuidMask<4>(petitionGuid);
    recv_data.ReadGuidMask<3>(playerGuid);
    recv_data.ReadGuidMask<7>(playerGuid);
    recv_data.ReadGuidMask<0>(petitionGuid);
    recv_data.ReadGuidMask<6>(petitionGuid);
    recv_data.ReadGuidMask<5>(playerGuid);
    recv_data.ReadGuidMask<0>(playerGuid);
    recv_data.ReadGuidMask<3>(petitionGuid);
    recv_data.ReadGuidMask<5>(petitionGuid);
    recv_data.ReadGuidMask<7>(petitionGuid);

    recv_data.ReadGuidBytes<7>(playerGuid);
    recv_data.ReadGuidBytes<1>(petitionGuid);
    recv_data.ReadGuidBytes<4>(petitionGuid);
    recv_data.ReadGuidBytes<2>(petitionGuid);
    recv_data.ReadGuidBytes<6>(playerGuid);
    recv_data.ReadGuidBytes<3>(petitionGuid);
    recv_data.ReadGuidBytes<0>(petitionGuid);
    recv_data.ReadGuidBytes<5>(petitionGuid);
    recv_data.ReadGuidBytes<0>(playerGuid);
    recv_data.ReadGuidBytes<2>(playerGuid);
    recv_data.ReadGuidBytes<5>(playerGuid);
    recv_data.ReadGuidBytes<3>(playerGuid);
    recv_data.ReadGuidBytes<4>(playerGuid);
    recv_data.ReadGuidBytes<7>(petitionGuid);
    recv_data.ReadGuidBytes<1>(playerGuid);
    recv_data.ReadGuidBytes<6>(petitionGuid);

    Player* player = sObjectAccessor.FindPlayer(playerGuid);
    if (!player)
    {
        return;
    }

    /// Get petition type and check
    QueryResult* result = CharacterDatabase.PQuery("SELECT 1 FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    if (!result)
    {
        return;
    }
    delete result;

    DEBUG_LOG("OFFER PETITION: petition %s to %s", petitionGuid.GetString().c_str(), playerGuid.GetString().c_str());

    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD) && GetPlayer()->GetTeam() != player->GetTeam())
    {
        SendGuildCommandResult(GUILD_CREATE_S, "", ERR_GUILD_NOT_ALLIED);
        return;
    }

    if (player->GetGuildId())
    {
        SendGuildCommandResult(GUILD_INVITE_S, _player->GetName(), ERR_ALREADY_IN_GUILD_S);
        return;
    }

    if (player->GetGuildIdInvited())
    {
        SendGuildCommandResult(GUILD_INVITE_S, _player->GetName(), ERR_ALREADY_INVITED_TO_GUILD_S);
        return;
    }

    /// Get petition signs count
    uint8 signs = 0;
    result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    // result==NULL also correct charter without signs
    if (result)
    {
        signs = (uint8)result->GetRowCount();
    }

    /// Send response
    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, (8 + 8 + 4 + signs + signs * 12));
    data << petitionGuid;                                   // petition guid
    data << _player->GetObjectGuid();                       // owner guid
    data << uint32(petitionGuid.GetCounter());              // guild guid (in mangos always same as low part of petition guid)
    data << uint8(signs);                                   // sign's count

    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        ObjectGuid signerGuid = ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32());

        data << signerGuid;                                 // Player GUID
        data << uint32(0);                                  // there 0 ...

        result->NextRow();
    }

    delete result;
    player->GetSession()->SendPacket(&data);
}

/**
 * @brief Handles turning in a completed petition to create a guild.
 *
 * @param recv_data The incoming turn-in-petition packet.
 */
void WorldSession::HandleTurnInPetitionOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received opcode CMSG_TURN_IN_PETITION"); // ok
    // recv_data.hexlike();

    // Writer sub_689A90 (thunk sub_68640F). A lone bit-packed GUID.
    ObjectGuid petitionGuid;

    recv_data.ReadGuidMask<1, 2, 3, 0, 5, 7, 4, 6>(petitionGuid);
    recv_data.ReadGuidBytes<2, 1, 4, 6, 0, 7, 5, 3>(petitionGuid);

    DEBUG_LOG("Petition %s turned in by %s", petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    /// Collect petition info data
    ObjectGuid ownerGuid;
    std::string name;

    // data
    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid`, `name` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    if (result)
    {
        Field* fields = result->Fetch();
        ownerGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        name = fields[1].GetCppString();
        delete result;
    }
    else
    {
        sLog.outError("CMSG_TURN_IN_PETITION: petition table not have data for guid %u!", petitionGuid.GetCounter());
        return;
    }

    if (_player->GetGuildId())
    {
        _player->SendPetitionTurnInResult(PETITION_TURN_ALREADY_IN_GUILD);  // already in guild
        return;
    }

    if (_player->GetObjectGuid() != ownerGuid)
    {
        return;
    }

    // signs
    result = CharacterDatabase.PQuery("SELECT `playerguid` FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    uint8 signs = result ? (uint8)result->GetRowCount() : 0;

    uint32 count = sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS);
    if (signs < count)
    {
        _player->SendPetitionTurnInResult(PETITION_TURN_NEED_MORE_SIGNATURES);  // need more signatures...
        delete result;
        return;
    }

    if (sGuildMgr.GetGuildByName(name))
    {
        _player->SendPetitionTurnInResult(PETITION_TURN_GUILD_NAME_INVALID);
        delete result;
        return;
    }

    // and at last charter item check
    Item* item = _player->GetItemByGuid(petitionGuid);
    if (!item)
    {
        delete result;
        return;
    }

    // OK!

    // delete charter item
    _player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    Guild* guild = new Guild;
    if (!guild->Create(_player, name))
    {
        delete guild;
        delete result;
        return;
    }

    // register guild and add guildmaster
    sGuildMgr.AddGuild(guild);

    // add members
    for (uint8 i = 0; i < signs; ++i)
    {
        Field* fields = result->Fetch();

        ObjectGuid signGuid = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
        if (!signGuid)
        {
            continue;
        }

        guild->AddMember(signGuid, guild->GetLowestRank());
        result->NextRow();
    }

    delete result;

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    CharacterDatabase.PExecute("DELETE FROM `petition_sign` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    CharacterDatabase.CommitTransaction();

    // created
    DEBUG_LOG("TURN IN PETITION %s", petitionGuid.GetString().c_str());

    _player->SendPetitionTurnInResult(PETITION_TURN_OK);
}

/**
 * @brief Handles a request to show the petitioner vendor list.
 *
 * @param recv_data The incoming show-list packet.
 */
void WorldSession::HandlePetitionShowListOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("Received CMSG_PETITION_SHOWLIST");
    // recv_data.hexlike();

    // Writer sub_689E91 (thunk sub_6864DF). A lone bit-packed GUID -- the vendor
    // this list was requested from.
    ObjectGuid guid;
    recv_data.ReadGuidMask<1, 7, 2, 5, 4, 0, 3, 6>(guid);
    recv_data.ReadGuidBytes<6, 3, 2, 4, 1, 7, 5, 0>(guid);

    SendPetitionShowList(guid);
}

/**
 * @brief Sends the available petition list from a petitioner NPC.
 *
 * @param guid The petitioner NPC guid.
 */
void WorldSession::SendPetitionShowList(ObjectGuid guid)
{
    Creature* pCreature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_PETITIONER);
    if (!pCreature)
    {
        DEBUG_LOG("WORLD: HandlePetitionShowListOpcode - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    WorldPacket data(SMSG_PETITION_SHOWLIST, 8 + 1 + 4 * 6);
    data << guid;                           // npc guid

    if (pCreature->IsTabardDesigner())
    {
        data << uint8(1);                   // count
        data << uint32(1);                  // index
        data << uint32(GUILD_CHARTER);      // charter entry
        data << uint32(CHARTER_DISPLAY_ID); // charter display id
        data << uint32(sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST)); // charter cost
        data << uint32(0);                  // unknown
        data << uint32(sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS));  // required signs
    }

    SendPacket(&data);
    DEBUG_LOG("Sent SMSG_PETITION_SHOWLIST");
}
