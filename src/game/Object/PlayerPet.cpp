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

#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "SkillDiscovery.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "SQLStorages.h"
#include "Vehicle.h"
#include "Calendar.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#include <cmath>

/**
 * @brief Sends the controlled pet's spell bar and cooldown state to the client.
 */
void Player::PetSpellInitialize()
{
    Pet* pet = GetPet();

    if (!pet)
    {
        return;
    }

    DEBUG_LOG("Pet Spells Groups");

    CharmInfo* charmInfo = pet->GetCharmInfo();

    MopPetPackets::SpellSnapshot snapshot;
    snapshot.guid = pet->GetObjectGuid();
    snapshot.family = uint16(pet->GetCreatureInfo()->Family);
    snapshot.mode = uint32(charmInfo->GetReactState()) |
        (uint32(charmInfo->GetCommandState()) << 8);

    for (uint8 index = 0; index < MAX_UNIT_ACTION_BAR_INDEX; ++index)
        snapshot.actionBar[index] = charmInfo->GetActionBarEntry(index)->packedData;

    if (pet->isControlled())
    {
        // spells loop
        for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
        {
            if (itr->second.state == PETSPELL_REMOVED)
            {
                continue;
            }

            snapshot.spells.push_back(
                uint32(MAKE_UNIT_ACTION_BUTTON(itr->first, itr->second.active)));
        }
    }

    time_t curTime = time(NULL);

    for (CreatureSpellCooldowns::const_iterator itr = pet->m_CreatureSpellCooldowns.begin(); itr != pet->m_CreatureSpellCooldowns.end(); ++itr)
    {
        time_t cooldown = (itr->second > curTime) ? (itr->second - curTime) * IN_MILLISECONDS : 0;

        snapshot.cooldowns.push_back({
            uint32(itr->first), uint16(0), uint32(cooldown), uint32(0)
        });
    }

    for (CreatureSpellCooldowns::const_iterator itr = pet->m_CreatureCategoryCooldowns.begin(); itr != pet->m_CreatureCategoryCooldowns.end(); ++itr)
    {
        time_t cooldown = (itr->second > curTime) ? (itr->second - curTime) * IN_MILLISECONDS : 0;

        snapshot.cooldowns.push_back({
            uint32(itr->first), uint16(0), uint32(0), uint32(cooldown)
        });
    }

    WorldPacket data;
    if (!MopPetPackets::BuildSpellSnapshot(data, snapshot))
    {
        sLog.outError("Player::PetSpellInitialize(): pet spell snapshot exceeds 18414 count limits");
        return;
    }
    GetSession()->SendPacket(&data);
}

void Player::SendPetGUIDs()
{
    if (!GetPetGuid())
    {
        return;
    }

    // Later this function might get modified for multiple guids
    WorldPacket data(SMSG_PET_GUIDS, 12);
    data << uint32(1);                      // count
    data << ObjectGuid(GetPetGuid());
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the possessed unit's action bar state to the client.
 */
void Player::PossessSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
    {
        return;
    }

    CharmInfo* charmInfo = charm->GetCharmInfo();

    if (!charmInfo)
    {
        sLog.outError("Player::PossessSpellInitialize(): charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    MopPetPackets::SpellSnapshot snapshot;
    snapshot.guid = charm->GetObjectGuid();
    for (uint8 index = 0; index < MAX_UNIT_ACTION_BAR_INDEX; ++index)
        snapshot.actionBar[index] = charmInfo->GetActionBarEntry(index)->packedData;

    WorldPacket data;
    MopPetPackets::BuildSpellSnapshot(data, snapshot);
    GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the charmed unit's available actions and spells to the client.
 */
void Player::CharmSpellInitialize()
{
    Unit* charm = GetCharm();

    if (!charm)
    {
        return;
    }

    CharmInfo* charmInfo = charm->GetCharmInfo();
    if (!charmInfo)
    {
        sLog.outError("Player::CharmSpellInitialize(): the player's charm (GUID: %u TypeId: %u) has no charminfo!", charm->GetGUIDLow(), charm->GetTypeId());
        return;
    }

    MopPetPackets::SpellSnapshot snapshot;
    snapshot.guid = charm->GetObjectGuid();

    if (charm->GetTypeId() != TYPEID_PLAYER)
    {
        snapshot.mode = uint32(charmInfo->GetReactState()) |
            (uint32(charmInfo->GetCommandState()) << 8);

        CreatureInfo const* cinfo = ((Creature*)charm)->GetCreatureInfo();
        if (cinfo && cinfo->CreatureType == CREATURE_TYPE_DEMON &&
                getClass() == CLASS_WARLOCK)
        {
            for (uint32 index = 0; index < CREATURE_MAX_SPELLS; ++index)
            {
                CharmSpellEntry* spell = charmInfo->GetCharmSpell(index);
                if (spell->GetAction())
                    snapshot.spells.push_back(spell->packedData);
            }
        }
    }

    for (uint8 index = 0; index < MAX_UNIT_ACTION_BAR_INDEX; ++index)
        snapshot.actionBar[index] = charmInfo->GetActionBarEntry(index)->packedData;

    WorldPacket data;
    MopPetPackets::BuildSpellSnapshot(data, snapshot);
    GetSession()->SendPacket(&data);
}
