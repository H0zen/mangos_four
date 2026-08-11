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

enum PetitionType
{
    PETITION_TYPE_GUILD = 0,                                // GetPetitionInfo() returns "guild"
    PETITION_TYPE_ARENA = 1,                                // ...and "arena"; anything else "other"
};

/**
 * @brief Serialises SMSG_PETITION_QUERY_RESPONSE for the 18414 client.
 *
 * Reader sub_70BD8D (parser sub_713976, vtable off_D697E4).
 *
 * Field identity came from GetPetitionInfo() (sub_962D86), which reads a CACHED
 * record rather than the packet record -- but the two are the same structure
 * offset by a constant 24 bytes, and that holds across four independent fields:
 * title +16/+40, body text +144/+168, originator GUID +8/+32 and the signature
 * pair +4240,+4244 / +4264,+4268. Under that mapping the client's seven exposed
 * values land as: petition type at +4308 (0 guild, 1 arena, else "other"),
 * title at +40, body text at +168, max signatures at +4268, the originator GUID
 * at +32 (also compared against the local player to decide isOriginator), and
 * min signatures at +4264.
 *
 * The remaining dwords are the same slots the pre-MoP body filled with zeroes
 * and the client does not surface through Lua; they stay zero here.
 *
 * The bit-field widths are not guessable and cost real time to re-derive, so
 * the ones recovered across this wave are recorded here: title 7 (sub_6650D3),
 * body text 12 (sub_69BAFD = sub_66529C's 8 << 4 | sub_6915FD's 4), the ten
 * name slots 6 each (sub_691684), CMSG_PETITION_BUY's name 7 (sub_664F47),
 * SMSG_PETITION_SHOW_SIGNATURES' signer count 21 (sub_6A29A8), and
 * SMSG_TURN_IN_PETITION_RESULTS' whole body 4 (sub_6915FD).
 */
static void BuildPetitionQueryResponse(WorldPacket& data, uint32 petitionId,
                                       ObjectGuid ownerGuid, std::string const& title,
                                       uint32 minSignatures, uint32 maxSignatures,
                                       uint32 petitionType)
{
    std::string const bodyText;                             // MoP leaves this empty

    data << uint32(petitionId);
    data.WriteBit(1);                                       // record present

    for (int i = 0; i < 10; ++i)
    {
        data.WriteBits(0, 6);                               // ten unused name slots
    }

    data.WriteGuidMask<2, 4>(ownerGuid);
    data.WriteBits(bodyText.size(), 12);
    data.WriteGuidMask<0, 7, 3, 6, 5>(ownerGuid);
    data.WriteBits(title.size(), 7);
    data.WriteGuidMask<1>(ownerGuid);
    data.FlushBits();

    data.WriteGuidBytes<5>(ownerGuid);
    data << uint32(0);
    data.WriteStringData(title);
    data << uint32(0);
    data.WriteStringData(bodyText);
    data.WriteGuidBytes<4>(ownerGuid);
    data << uint32(maxSignatures);
    data.WriteGuidBytes<6>(ownerGuid);
    data << uint32(0);
    data << uint32(minSignatures);
    // the ten name slots are all zero-length, so nothing follows for them
    data.WriteGuidBytes<1, 7, 0>(ownerGuid);
    data << uint32(0);
    data << uint32(0);
    data.WriteGuidBytes<2>(ownerGuid);
    data << uint32(0);
    data << uint16(0);
    data << uint32(petitionType);
    data.WriteGuidBytes<3>(ownerGuid);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
    data << uint32(0);
}

/**
 * @brief Serialises SMSG_PETITION_SHOW_SIGNATURES for the 18414 client.
 *
 * Reader sub_72B793 (parser sub_73263F, vtable off_D6B990). Two things here are
 * not guessable from the pre-MoP body and are not negotiable:
 *
 * The signer count is a 21-BIT field, not a uint8 -- sub_6A29A8 assembles it
 * from two 8-bit chunks at shifts 13 and 5 plus a 5-bit tail.
 *
 * One header mask bit -- the petition GUID's byte 2 -- is deferred until AFTER
 * every per-signer mask has been written. A rebuild that emits the two header
 * masks as a block, which is the obvious shape, puts that bit fifteen positions
 * too early and desynchronises every byte that follows.
 *
 * Field identity comes from the consumer, since no capture of this opcode exists
 * in the corpus. The call site at 0x963727 passes record +48 as the petition GUID
 * (into qword_11E9F18, the same global CMSG_OFFER_PETITION's builder reads), and
 * record +40 as a separate owner GUID (into dword_11E9F20/24). The test just
 * above it compares record +40 against the local player to decide isOriginator,
 * which confirms +40 is the owner independently. Record +16 is the key
 * sub_62EB8B uses against the petition-text cache -- the petition id.
 */
static void BuildPetitionShowSignatures(WorldPacket& data, ObjectGuid petitionGuid,
                                        ObjectGuid ownerGuid, uint32 petitionId,
                                        std::vector<ObjectGuid> const& signers)
{
    data.WriteGuidMask<1>(ownerGuid);
    data.WriteGuidMask<3>(petitionGuid);
    data.WriteGuidMask<3>(ownerGuid);
    data.WriteGuidMask<4>(petitionGuid);
    data.WriteGuidMask<0>(petitionGuid);
    data.WriteGuidMask<7>(ownerGuid);
    data.WriteGuidMask<5>(ownerGuid);
    data.WriteGuidMask<1>(petitionGuid);
    data.WriteGuidMask<5>(petitionGuid);
    data.WriteGuidMask<7>(petitionGuid);
    data.WriteGuidMask<0>(ownerGuid);
    data.WriteGuidMask<6>(ownerGuid);
    data.WriteGuidMask<6>(petitionGuid);
    data.WriteGuidMask<2>(ownerGuid);
    data.WriteGuidMask<4>(ownerGuid);

    data.WriteBits(signers.size(), 21);

    for (ObjectGuid const& signer : signers)
    {
        data.WriteGuidMask<2, 0, 4, 7, 5, 1, 6, 3>(signer);
    }

    // The deferred header bit. See the note above -- it belongs here, not with
    // the other fifteen.
    data.WriteGuidMask<2>(petitionGuid);
    data.FlushBits();

    for (ObjectGuid const& signer : signers)
    {
        data.WriteGuidBytes<6, 0, 1, 3, 2, 5, 7, 4>(signer);
        data << uint32(0);                                  // per-signer, always 0
    }

    data.WriteGuidBytes<6, 5, 4>(petitionGuid);
    data.WriteGuidBytes<4>(ownerGuid);
    data.WriteGuidBytes<1>(petitionGuid);
    data << uint32(petitionId);
    data.WriteGuidBytes<2, 3, 7>(petitionGuid);
    data.WriteGuidBytes<5, 6, 3, 7, 1, 0>(ownerGuid);
    data.WriteGuidBytes<0>(petitionGuid);
    data.WriteGuidBytes<2>(ownerGuid);
}

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

    // Direct: queued, this is not ordered against the direct write in
    // HandlePetitionSignOpcode, so a signature could be taken and confirmed
    // against the charter this replaces and then be deleted underneath it -- the
    // signer told it counted when it did not. A failure is ambiguous, so it is
    // reported rather than retried.
    if (!CharacterDatabase.CommitTransactionDirect())
    {
        sLog.outError("CMSG_PETITION_BUY: could not replace the petitions owned by %u", _player->GetGUIDLow());
    }
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

    // Ask for the OWNER, not merely whether the petition exists. The owner GUID
    // is what the client's consumer compares against the local player to decide
    // isOriginator, so sending the requester's own GUID -- as this did -- tells
    // everyone who opens a charter that they created it, and shows them the
    // owner's controls instead of the sign button. That is wrong in the ordinary
    // case of an invitee opening an offered charter, not only under a forged
    // request.
    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '%u'", petitionguid_low);
    if (!result)
    {
        sLog.outError("any petition on server...");
        return;
    }

    ObjectGuid const ownerGuid = ObjectGuid(HIGHGUID_PLAYER, result->Fetch()[0].GetUInt32());
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

    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, 5 + 16 + 4 + signs * 13);
    std::vector<ObjectGuid> signers;
    signers.reserve(signs);
    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        signers.push_back(ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32()));
        result->NextRow();
    }
    delete result;

    BuildPetitionShowSignatures(data, petitionguid, ownerGuid,
                                petitionguid_low, signers);
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

    // The signature requirement the SERVER actually enforces, at the turn-in gate
    // below. The pre-MoP body hardcoded 4 here while that gate defaults to 9, so
    // the client was told a number its own server contradicted -- the charter
    // would read as ready and then be refused. Send what is enforced.
    uint32 const requiredSigns = sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS);

    WorldPacket data(SMSG_PETITION_QUERY_RESPONSE, 4 + 1 + 8 + name.size() + 64);
    BuildPetitionQueryResponse(data, petitionLowGuid, ownerGuid, name,
                               requiredSigns, requiredSigns, PETITION_TYPE_GUILD);
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
        // The client has a message for this (ERR_PETITION_CREATOR); dropping the
        // packet leaves the charter frame waiting instead.
        _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_CANT_SIGN_OWN);
        return;
    }

    // Nothing in this packet binds it to an offer: the petition GUID is a
    // client-supplied item GUID, and item GUIDs are enumerable, so a charter can
    // be named without ever having been shown one. The offer is where ownership
    // and proximity can be established, so signing is admitted only for the
    // charter this player was actually offered.
    //
    // Checking proximity here instead would be weaker AND stricter at once: a
    // forged packet from anyone standing nearby would still pass, while a real
    // offer would stop being signable the moment the owner stepped away.
    if (_player->GetOfferedPetitionGuid() != petitionGuid)
    {
        DEBUG_LOG("CMSG_PETITION_SIGN: %s tried to sign petition %u without being offered it",
                  _player->GetGuidStr().c_str(), petitionLowGuid);
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

    // The client stops a character signing the same charter twice, but not a
    // second character on the same account, so both checks belong here. Each has
    // to ask the database its own question: this used to select every signature
    // the account held and judge from whichever row came back first, which let
    // an alt sign a charter the account had already signed whenever an unrelated
    // signature happened to sort ahead of it -- enough to clear the minimum
    // signature count from one account.
    result = CharacterDatabase.PQuery(
                 "SELECT 1 FROM `petition_sign` WHERE `player_account` = '%u' AND `petitionguid` = '%u' LIMIT 1",
                 GetAccountId(), petitionLowGuid);

    if (result)
    {
        delete result;
        // "You have already signed that charter." -- close at signer side
        _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_ALREADY_SIGNED);
        return;
    }

    result = CharacterDatabase.PQuery(
                 "SELECT 1 FROM `petition_sign` WHERE `playerguid` = '%u' AND `petitionguid` <> '%u' LIMIT 1",
                 _player->GetGUIDLow(), petitionLowGuid);

    if (result)
    {
        delete result;
        // "You've already signed another guild charter." -- close at signer side
        _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_ALREADY_SIGNED_OTHER);
        return;
    }

    // Direct, not queued. Every check above reads committed state through the
    // synchronous pool while PExecute only enqueues, so an async insert stays
    // invisible to the next signature's checks -- two signatures in quick
    // succession could both pass, which the schema permits because its key is
    // (petitionguid, playerguid).
    //
    // The insert also has to prove the charter still exists, rather than trusting
    // the read at the top of this handler. Several paths invalidate a petition
    // inside a QUEUED transaction -- buying a replacement charter, deleting a
    // character, joining another guild -- and none of them can be ordered against
    // this handler. A plain insert could therefore land after the charter had
    // gone, and petition_sign has no foreign key to catch it: that orphan row
    // would bar its signer from every other charter through the check above.
    //
    // That closes one of the two orderings. The other is safe already: an insert
    // that lands BEFORE the invalidation commits is removed by it, because every
    // one of those paths deletes the petition_sign rows as well as the petition.
    // Both together are what make this safe, not this statement alone.
    if (!CharacterDatabase.DirectPExecute(
                "INSERT INTO `petition_sign` (`ownerguid`, `petitionguid`, `playerguid`, `player_account`) "
                "SELECT '%u', '%u', '%u', '%u' FROM DUAL "
                "WHERE EXISTS (SELECT 1 FROM `petition` WHERE `petitionguid` = '%u' AND `ownerguid` = '%u')",
                ownerLowGuid, petitionLowGuid, _player->GetGUIDLow(), GetAccountId(), petitionLowGuid, ownerLowGuid))
    {
        sLog.outError("CMSG_PETITION_SIGN: failed to record %s signing petition %u",
                      _player->GetGuidStr().c_str(), petitionLowGuid);
        return;
    }

    // That statement succeeds whether or not it inserted anything, so confirm
    // before telling anyone the signature counted. The insert was direct, so it
    // is already visible to this read.
    QueryResult* stored = CharacterDatabase.PQuery(
                              "SELECT 1 FROM `petition_sign` WHERE `petitionguid` = '%u' AND `playerguid` = '%u' LIMIT 1",
                              petitionLowGuid, _player->GetGUIDLow());

    if (!stored)
    {
        DEBUG_LOG("CMSG_PETITION_SIGN: petition %u went away while %s was signing it",
                  petitionLowGuid, _player->GetGuidStr().c_str());
        return;
    }

    delete stored;

    DEBUG_LOG("PETITION SIGN: %s by %s", petitionGuid.GetString().c_str(), _player->GetGuidStr().c_str());

    // The offer is spent.
    _player->ClearOfferedPetitionGuid();

    // close at signer side
    _player->SendPetitionSignResult(petitionGuid, _player, PETITION_SIGN_OK);

    // update signs count on charter, required testing...
    // Item *item = _player->GetItemByGuid(petitionguid));
    // if (item)
    //    item->SetUInt32Value(ITEM_FIELD_ENCHANTMENT_1_1+1, signs);

    // Only the successful signature is sent to the owner. The client's consumer
    // never looks at the result when the signer is not itself: it goes straight
    // to sub_9634D4, which appends the signer and announces
    // ERR_PETITION_SIGNED_S. A failure forwarded here would put a signature the
    // database does not have into the owner's charter.
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

    // Both GUIDs in this packet come from the client, and until this opcode was
    // registered nothing could reach the checks below. Two things therefore have
    // to be established here rather than assumed.
    //
    // First, that the sender actually OWNS the petition. The old query asked only
    // whether the petition existed, so any client could name someone else's
    // charter and have the signature list delivered under its own name.
    QueryResult* result = CharacterDatabase.PQuery("SELECT `ownerguid` FROM `petition` WHERE `petitionguid` = '%u'", petitionGuid.GetCounter());
    if (!result)
    {
        return;
    }

    uint32 const ownerLowGuid = result->Fetch()[0].GetUInt32();
    delete result;

    if (ownerLowGuid != _player->GetGUIDLow())
    {
        sLog.outError("CMSG_OFFER_PETITION: %s offered petition %u owned by %u, refusing",
                      _player->GetGuidStr().c_str(), petitionGuid.GetCounter(), ownerLowGuid);
        return;
    }

    // Second, that the target is somewhere this player could actually have
    // offered it. FindPlayer resolves ANY online character on the realm, so
    // without this a forged packet reaches players on other maps and continents.
    // The client's own builder resolves its target as an in-world unit and
    // level-checks it, so requiring proximity matches what it can legitimately
    // produce; TRADE_DISTANCE is the same bound the trade handler uses for the
    // equivalent "hand something to the player in front of me" interaction.
    if (!_player->IsWithinDistInMap(player, TRADE_DISTANCE, false))
    {
        DEBUG_LOG("CMSG_OFFER_PETITION: %s offered petition %u to out-of-range %s",
                  _player->GetGuidStr().c_str(), petitionGuid.GetCounter(), playerGuid.GetString().c_str());
        return;
    }

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
    WorldPacket data(SMSG_PETITION_SHOW_SIGNATURES, 5 + 16 + 4 + signs * 13);
    std::vector<ObjectGuid> signers;
    signers.reserve(signs);
    for (uint8 i = 1; i <= signs; ++i)
    {
        Field* fields2 = result->Fetch();
        signers.push_back(ObjectGuid(HIGHGUID_PLAYER, fields2[0].GetUInt32()));
        result->NextRow();
    }

    delete result;

    BuildPetitionShowSignatures(data, petitionGuid, _player->GetObjectGuid(),
                                petitionGuid.GetCounter(), signers);
    player->GetSession()->SendPacket(&data);

    // Record what was offered. This is the only legitimate route to signing, and
    // the ownership and proximity checks above are the point at which those can
    // be established -- CMSG_PETITION_SIGN itself carries nothing that ties it to
    // an offer.
    player->SetOfferedPetitionGuid(petitionGuid);
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

    std::vector<ObjectGuid> signers;
    signers.reserve(signs);

    if (result)
    {
        do
        {
            if (uint32 signLowGuid = result->Fetch()[0].GetUInt32())
            {
                signers.push_back(ObjectGuid(HIGHGUID_PLAYER, signLowGuid));
            }
        }
        while (result->NextRow());

        delete result;
        result = NULL;
    }

    // The row count is not the membership. Guild::AddMember legitimately refuses
    // a signer who has joined a guild since signing, or whose character no longer
    // exists, and by the time it does so the charter has been destroyed and the
    // guild registered -- leaving a guild standing on fewer members than it was
    // required to collect, with nothing to undo. Establish who can actually be
    // admitted while refusing is still free. At most nine signers, so the cost is
    // a handful of lookups on a rare packet.
    uint32 eligible = 0;

    for (ObjectGuid const& signGuid : signers)
    {
        Player* signer = sObjectMgr.GetPlayer(signGuid);
        uint32 const signerGuildId = signer ? signer->GetGuildId() : Player::GetGuildIdFromDB(signGuid);

        if (signerGuildId == 0)
        {
            ++eligible;
        }
    }

    uint32 count = sWorld.getConfig(CONFIG_UINT32_MIN_PETITION_SIGNS);
    if (eligible < count)
    {
        _player->SendPetitionTurnInResult(PETITION_TURN_NEED_MORE_SIGNATURES);  // need more signatures...
        return;
    }

    if (sGuildMgr.GetGuildByName(name))
    {
        _player->SendPetitionTurnInResult(PETITION_TURN_GUILD_NAME_INVALID);
        return;
    }

    // and at last charter item check
    Item* item = _player->GetItemByGuid(petitionGuid);
    if (!item)
    {
        return;
    }

    // OK!

    // delete charter item
    _player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);

    Guild* guild = new Guild;
    if (!guild->Create(_player, name))
    {
        delete guild;
        return;
    }

    // register guild and add guildmaster
    sGuildMgr.AddGuild(guild);

    // add members
    uint32 admitted = 0;

    for (ObjectGuid const& signGuid : signers)
    {
        if (guild->AddMember(signGuid, guild->GetLowestRank()))
        {
            ++admitted;
        }
    }

    // The preflight above established that enough signers could be admitted, so
    // a shortfall here means one stopped being eligible in between. Nothing can
    // be undone at this point -- the charter is destroyed and the guild
    // registered -- so it is reported.
    if (admitted < count)
    {
        sLog.outError("TURN IN PETITION: guild '%s' created by %s from %u signatures, %u eligible at preflight, but only %u members were admitted",
                      name.c_str(), _player->GetGuidStr().c_str(), uint32(signs), eligible, admitted);
    }

    // DestroyItem above already dropped both of these directly, so this affects
    // no rows. It is kept deliberately: it is the turn-in path's own statement of
    // what it requires, and it does not depend on a charter-flag check in the
    // item code staying where it is.
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

    // Reader sub_6F8626 (parser sub_7047D7, vtable off_D695C8). MoP reduced this
    // to two fields: the vendor GUID and ONE uint32. The count, index, charter
    // entry, display id, unknown and required-signs dwords the pre-MoP body wrote
    // are not on the 18414 wire -- the client sources all of that itself.
    //
    // No capture of this opcode exists in the corpus, so the uint32 is named by
    // the consumer at 0x9D8FF0, which is unambiguous: it takes the GUID from
    // record +16 into the vendor global, and record +24 into dword_1210740 --
    // precisely the global GetGuildCharterCost() returns to the registrar's
    // MoneyFrame_Update, and the one BuyGuildCharter range-checks the player's
    // money against before sending CMSG_PETITION_BUY. It is the charter COST.
    //
    // The scalar is interleaved into the GUID byte run and cannot be moved.
    WorldPacket data(SMSG_PETITION_SHOWLIST, 1 + 8 + 4);
    data.WriteGuidMask<3, 5, 7, 6, 1, 0, 2, 4>(guid);
    data.FlushBits();
    data.WriteGuidBytes<6, 0, 1>(guid);
    data << uint32(pCreature->IsTabardDesigner()
                   ? sWorld.getConfig(CONFIG_UNIT32_GUILD_PETITION_COST) : 0);
    data.WriteGuidBytes<4, 3, 5, 2, 7>(guid);

    SendPacket(&data);
    DEBUG_LOG("Sent SMSG_PETITION_SHOWLIST");
}
