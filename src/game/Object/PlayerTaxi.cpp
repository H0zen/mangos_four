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

#include "PlayerTaxi.h"
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
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Spell.h"
#include "SpellAuras.h"
#include "AchievementMgr.h"
#include "DBCStores.h"
#include "MapManager.h"

#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>

namespace
{
    bool ParseTaxiPersistenceValue(std::string const& token, uint32& value)
    {
        if (token.empty())
        {
            return false;
        }

        auto const result = std::from_chars(token.data(), token.data() + token.size(), value);
        return result.ec == std::errc() && result.ptr == token.data() + token.size();
    }

    void ResetTaxiPersistenceState(PlayerTaxi& taxi)
    {
        taxi.ClearTaxiDestinations();
        taxi.SetFlightMasterFactionTemplateId(0);
    }

    class ObjectMgrTaxiPersistenceValidator final : public TaxiPersistence::Validator
    {
        public:
            bool HasTaxiPath(uint32 source, uint32 destination) override
            {
                uint32 path = 0;
                uint32 cost = 0;
                sObjectMgr.GetTaxiPath(source, destination, path, cost);
                return path != 0;
            }

            bool HasTaxiMount(uint32 source, Team team) override
            {
                return sObjectMgr.GetTaxiMountDisplayId(source, team, true) != 0;
            }
    };
}

bool TaxiPersistence::LoadTaxiDestinations(PlayerTaxi& taxi, std::string const& values,
    Team team, Validator& validator)
{
    ResetTaxiPersistenceState(taxi);

    try
    {
        std::istringstream input(values);
        std::vector<std::string> tokens;
        for (std::string token; input >> token;)
        {
            tokens.push_back(token);
        }

        if (tokens.empty())
        {
            return true;
        }

        auto token = tokens.begin();
        uint32 factionId = 0;
        if (!ParseTaxiPersistenceValue(*token, factionId))
        {
            return false;
        }

        ++token;
        std::vector<uint32> nodes;
        for (; token != tokens.end(); ++token)
        {
            uint32 node = 0;
            if (!ParseTaxiPersistenceValue(*token, node))
            {
                return false;
            }

            if (!taxi.IsValidNodeId(node))
            {
                return false;
            }

            nodes.push_back(node);
        }

        if (nodes.size() < 2)
        {
            return false;
        }

        for (size_t i = 1; i < nodes.size(); ++i)
        {
            if (!validator.HasTaxiPath(nodes[i - 1], nodes[i]))
            {
                return false;
            }
        }

        if (!validator.HasTaxiMount(nodes.front(), team))
        {
            return false;
        }

        taxi.SetFlightMasterFactionTemplateId(factionId);
        for (uint32 node : nodes)
        {
            taxi.AddTaxiDestination(node);
        }

        return true;
    }
    catch (...)
    {
        ResetTaxiPersistenceState(taxi);
        return false;
    }
}

void PlayerTaxi::InitTaxiNodes(uint32 race, uint32 chrClass, uint8 level)
{
    InitTaxiNodesForClass(chrClass);
    InitTaxiNodesForRace(race);
    InitTaxiNodesForFaction(Player::TeamForRace(race));
    InitTaxiNodesForLvl(level);
}

void PlayerTaxi::InitTaxiNodesForClass(uint32 chrClass)
{
    // class specific initial known nodes
    switch (chrClass)
    {
        case CLASS_DEATH_KNIGHT:
        {
            for (size_t i = 0; i < TaxiMaskSize; ++i)
            {
                m_taximask[i] |= sOldContinentsNodesMask[i];
            }
            break;
        }
    }
}

void PlayerTaxi::InitTaxiNodesForRace(uint32 race)
{
    // race specific initial known nodes: capital and taxi hub masks
    switch (race)
    {
        case RACE_HUMAN:
        case RACE_DWARF:
        case RACE_NIGHTELF:
        case RACE_GNOME:
        case RACE_DRAENEI:
        case RACE_WORGEN:
            SetTaximaskNode(2);     // Stormwind, Elwynn
            SetTaximaskNode(6);     // Ironforge, Dun Morogh
            SetTaximaskNode(26);    // Lor'danel, Darkshore
            SetTaximaskNode(27);    // Rut'theran Village, Teldrassil
            SetTaximaskNode(49);    // Moonglade (Alliance)
            SetTaximaskNode(94);    // The Exodar
            SetTaximaskNode(456);   // Dolanaar, Teldrassil
            SetTaximaskNode(457);   // Darnassus, Teldrassil
            SetTaximaskNode(582);   // Goldshire, Elwynn
            SetTaximaskNode(589);   // Eastvale Logging Camp, Elwynn
            SetTaximaskNode(619);   // Kharanos, Dun Morogh
            SetTaximaskNode(620);   // Gol'Bolar Quarry, Dun Morogh
            SetTaximaskNode(624);   // Azure Watch, Azuremyst Isle
            break;

        case RACE_ORC:
        case RACE_UNDEAD:
        case RACE_TAUREN:
        case RACE_TROLL:
        case RACE_BLOODELF:
        case RACE_GOBLIN:
            SetTaximaskNode(11);    // Undercity, Tirisfal
            SetTaximaskNode(22);    // Thunder Bluff, Mulgore
            SetTaximaskNode(23);    // Orgrimmar, Durotar
            SetTaximaskNode(69);    // Moonglade (Horde)
            SetTaximaskNode(82);    // Silvermoon City
            SetTaximaskNode(384);   // The Bulwark, Tirisfal
            SetTaximaskNode(402);   // Bloodhoof Village, Mulgore
            SetTaximaskNode(460);   // Brill, Tirisfal Glades
            SetTaximaskNode(536);   // Sen'jin Village, Durotar
            SetTaximaskNode(537);   // Razor Hill, Durotar
            SetTaximaskNode(625);   // Fairbreeze Village, Eversong Woods
            SetTaximaskNode(631);   // Falconwing Square, Eversong Woods
            break;
    }
}

void PlayerTaxi::InitTaxiNodesForFaction(uint32 faction)
{
    // new continent starting masks (It will be accessible only at new map)
    switch (faction)
    {
        case ALLIANCE:
            SetTaximaskNode(100); // Honor Hold
            SetTaximaskNode(245); // Valiance Keep
            break;

        case HORDE:
            SetTaximaskNode(99);  // Thrallmar
            SetTaximaskNode(257); // Warsong Hold
            break;

        default:
            break;
    }
}

void PlayerTaxi::InitTaxiNodesForLvl(uint8 level)
{
    // level dependent taxi hubs
    if (level >= 68) // Shattered Sun Staging Area
    {
        SetTaximaskNode(213);
    }

    if (level >= 78) // Dalaran
    {
        SetTaximaskNode(310);
    }
}

void PlayerTaxi::LoadTaxiMask(const char* data)
{
    memset(m_taximask, 0, sizeof(m_taximask));
    Tokens tokens = StrSplit(data, " ");

    size_t index;
    Tokens::iterator iter;
    for (iter = tokens.begin(), index = 0; (index < TaxiMaskSize) && (iter != tokens.end()); ++iter, ++index)
    {
        // load and set bits only for existing taxi nodes
        m_taximask[index] = sTaxiNodesMask[index] & uint8(std::stoul((*iter).c_str()));
    }
}

void PlayerTaxi::AppendTaximaskTo(ByteBuffer& data, bool all)
{
    data << uint32(TaxiMaskSize);
    if (all)
    {
        for (size_t i = 0; i < TaxiMaskSize; ++i)
        {
            data << uint8(sTaxiNodesMask[i]);               // all existing nodes
        }
    }
    else
    {
        for (size_t i = 0; i < TaxiMaskSize; ++i)
        {
            data << uint8(m_taximask[i]);                   // known nodes
        }
    }
}

uint8 const* PlayerTaxi::GetTaxiMask(bool all) const
{
    return all ? sTaxiNodesMask : m_taximask;
}

size_t PlayerTaxi::GetTaxiMaskSize() const
{
    return TaxiMaskSize;
}

bool PlayerTaxi::LoadTaxiDestinationsFromString(const std::string& values, Team team)
{
    ObjectMgrTaxiPersistenceValidator validator;
    return TaxiPersistence::LoadTaxiDestinations(*this, values, team, validator);
}

std::string PlayerTaxi::SaveTaxiDestinationsToString()
{
    if (m_TaxiDestinations.empty())
    {
        return "";
    }

    MANGOS_ASSERT(m_TaxiDestinations.size() >= 2);

    std::ostringstream ss;
    ss << m_flightMasterFactionId << ' ';

    for (size_t i = 0; i < m_TaxiDestinations.size(); ++i)
    {
        ss << m_TaxiDestinations[i] << " ";
    }

    return ss.str();
}

uint32 PlayerTaxi::GetCurrentTaxiPath() const
{
    if (m_TaxiDestinations.size() < 2)
    {
        return 0;
    }

    uint32 path;
    uint32 cost;

    sObjectMgr.GetTaxiPath(m_TaxiDestinations[0], m_TaxiDestinations[1], path, cost);

    return path;
}

bool PlayerTaxi::BuildSameMapTaxiPath(TaxiPathNodeList& route, uint32 mapId) const
{
    route.clear();
    if (m_TaxiDestinations.size() < 3)
    {
        return false;
    }

    for (size_t leg = 1; leg < m_TaxiDestinations.size(); ++leg)
    {
        uint32 pathId = 0;
        uint32 cost = 0;
        sObjectMgr.GetTaxiPath(m_TaxiDestinations[leg - 1],
            m_TaxiDestinations[leg], pathId, cost);
        if (!pathId || pathId >= sTaxiPathNodesByPath.size())
        {
            route.clear();
            return false;
        }

        TaxiPathNodeList const& legPath = sTaxiPathNodesByPath[pathId];
        if (legPath.size() < 2)
        {
            route.clear();
            return false;
        }

        // Retail multi-leg splines do not visit the shared flight-master
        // endpoint. Keep the first leg's authored start and the last leg's
        // authored finish, but collapse the duplicated join between them.
        size_t const nodeBegin = leg > 1 ? 1 : 0;
        size_t const nodeEnd = leg + 1 < m_TaxiDestinations.size()
            ? legPath.size() - 1
            : legPath.size();
        if (nodeBegin >= nodeEnd)
        {
            route.clear();
            return false;
        }

        for (size_t node = nodeBegin; node < nodeEnd; ++node)
        {
            if (legPath[node].ContinentID != mapId)
            {
                route.clear();
                return false;
            }
            route.push_back(TaxiPathNodePtr(&legPath[node]));
        }
    }

    return route.size() >= 2;
}

std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi)
{
    for (size_t i = 0; i < TaxiMaskSize; ++i)
    {
        ss << uint32(taxi.m_taximask[i]) << " ";    // cast to prevent conversion to char
    }

    return ss;
}

FactionTemplateEntry const* PlayerTaxi::GetFlightMasterFactionTemplate() const
{
    return sFactionTemplateStore.LookupEntry(m_flightMasterFactionId);
}

/**
 * @brief Starts a taxi flight across a sequence of taxi nodes.
 *
 * @param nodes The ordered taxi node path to travel.
 * @param npc The taxi master providing the route, or NULL for spell/scripted travel.
 * @param spellid The spell initiating the taxi flight, if any.
 * @return True if the flight started successfully; otherwise, false.
 */
bool Player::ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc /*= NULL*/, uint32 spellid /*= 0*/)
{
    if (nodes.size() < 2)
    {
        return false;
    }

    // not let cheating with start flight in time of logout process || if casting not finished || while in combat || if not use Spell's with EffectSendTaxi
    if (GetSession()->isLogingOut() || IsInCombat())
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
        return false;
    }

    if (HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE))
    {
        return false;
    }

    // taximaster case
    if (npc)
    {
        // not let cheating with start flight mounted
        if (IsMounted())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERALREADYMOUNTED);
            return false;
        }

        if (IsInDisallowedMountForm())
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERSHAPESHIFTED);
            return false;
        }

        // not let cheating with start flight in time of logout process || if casting not finished || while in combat || if not use Spell's with EffectSendTaxi
        if (IsNonMeleeSpellCasted(false))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXIPLAYERBUSY);
            return false;
        }
    }
    // cast case or scripted call case
    else
    {
        RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

        if (IsInDisallowedMountForm())
        {
            RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);
        }

        if (Spell* spell = GetCurrentSpell(CURRENT_GENERIC_SPELL))
            if (spell->m_spellInfo->ID != spellid)
            {
                InterruptSpell(CURRENT_GENERIC_SPELL, false);
            }

        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, false);

        if (Spell* spell = GetCurrentSpell(CURRENT_CHANNELED_SPELL))
            if (spell->m_spellInfo->ID != spellid)
            {
                InterruptSpell(CURRENT_CHANNELED_SPELL, true);
            }
    }

    uint32 sourcenode = nodes[0];

    // starting node too far away (cheat?)
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(sourcenode);
    if (!node)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
        return false;
    }

    // check node starting pos data set case if provided
    if (node->x != 0.0f || node->y != 0.0f || node->z != 0.0f)
    {
        if (node->ContinentID != GetMapId() ||
                (node->x - Where().X()) * (node->x - Where().X()) +
                (node->y - Where().Y()) * (node->y - Where().Y()) +
                (node->z - Where().Z()) * (node->z - Where().Z()) >
                (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE) * (2 * INTERACTION_DISTANCE))
        {
            GetSession()->SendActivateTaxiReply(ERR_TAXITOOFARAWAY);
            return false;
        }
    }
    // node must have pos if taxi master case (npc != NULL)
    else if (npc)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);
        return false;
    }

    // Prepare to flight start now

    // stop combat at start taxi flight if any
    CombatStop();

    // stop trade (client cancel trade at taxi map open but cheating tools can be used for reopen it)
    TradeCancel(true);

    // clean not finished taxi path if any
    m_taxi.ClearTaxiDestinations();

    // 0 element current node
    m_taxi.AddTaxiDestination(sourcenode);

    // fill destinations path tail
    uint32 sourcepath = 0;
    uint32 totalcost = 0;
    uint32 firstcost = 0;

    uint32 prevnode = sourcenode;
    uint32 lastnode = 0;

    for (uint32 i = 1; i < nodes.size(); ++i)
    {
        uint32 path, cost;

        lastnode = nodes[i];
        sObjectMgr.GetTaxiPath(prevnode, lastnode, path, cost);

        if (!path)
        {
            m_taxi.ClearTaxiDestinations();
            return false;
        }

        totalcost += cost;

        if (i == 1)
        {
            firstcost = cost;
        }

        if (prevnode == sourcenode)
        {
            sourcepath = path;
        }

        m_taxi.AddTaxiDestination(lastnode);

        prevnode = lastnode;
    }

    // get mount model (in case non taximaster (npc==NULL) allow more wide lookup)
    uint32 mount_display_id = sObjectMgr.GetTaxiMountDisplayId(sourcenode, GetTeam(), npc == NULL);

    // in spell case allow 0 model
    if ((mount_display_id == 0 && spellid == 0) || sourcepath == 0)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);

        m_taxi.ClearTaxiDestinations();
        return false;
    }

    uint64 money = GetMoney();

    if (npc)
    {
        float discount = GetReputationPriceDiscount(npc);

        totalcost = uint32(ceil(totalcost * discount));
        firstcost = uint32(ceil(firstcost * discount));

        m_taxi.SetFlightMasterFactionTemplateId(npc->getFaction());
    }
    else
    {
        m_taxi.SetFlightMasterFactionTemplateId(0);
    }

    if (money < totalcost)
    {
        GetSession()->SendActivateTaxiReply(ERR_TAXINOTENOUGHMONEY);

        m_taxi.ClearTaxiDestinations();
        return false;
    }

    return StartTaxiFlight(mount_display_id, sourcepath, totalcost);
}

/**
 * @brief Debits and launches a taxi route after route and fare validation.
 * @param mountDisplayId Taxi mount display identifier.
 * @param path Taxi path identifier.
 * @param totalCost Full fare to debit and refund if launch fails.
 * @return True only after the movement spline launches successfully.
 */
bool Player::StartTaxiFlight(uint32 mountDisplayId, uint32 path, uint32 totalCost)
{
    ModifyMoney(-(int64)totalCost);
    RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);

    if (!GetSession()->SendDoFlight(mountDisplayId, path))
    {
        ModifyMoney(totalCost);
        m_taxi.ClearTaxiDestinations();
        GetSession()->SendActivateTaxiReply(ERR_TAXIUNSPECIFIEDSERVERERROR);
        return false;
    }

    GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TRAVELLING, totalCost);
    GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_FLIGHT_PATHS_TAKEN, 1);
    GetSession()->SendActivateTaxiReply(ERR_TAXIOK);

    return true;
}

/**
 * @brief Starts a taxi flight using a direct taxi path identifier.
 *
 * @param taxi_path_id The taxi path identifier to use.
 * @param spellid The spell initiating the taxi flight, if any.
 * @return True if the flight started successfully; otherwise, false.
 */
bool Player::ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid /*= 0*/)
{
    TaxiPathEntry const* entry = sTaxiPathStore.LookupEntry(taxi_path_id);
    if (!entry)
    {
        return false;
    }

    std::vector<uint32> nodes;

    nodes.resize(2);
    nodes[0] = entry->FromTaxiNode;
    nodes[1] = entry->ToTaxiNode;

    return ActivateTaxiPathTo(nodes, NULL, spellid);
}

/**
 * @brief Resumes an interrupted taxi flight from the nearest path node.
 */
void Player::ContinueTaxiFlight()
{
    uint32 sourceNode = m_taxi.GetTaxiSource();
    if (!sourceNode)
    {
        return;
    }

    DEBUG_LOG("WORLD: Restart character %u taxi flight", GetGUIDLow());

    // Retail resumes an interrupted multi-hop flight only as far as the next
    // taxi node. The player may choose a new route after landing there.
    m_taxi.TruncateTaxiDestinationsAfterCurrentLeg();

    uint32 mountDisplayId = sObjectMgr.GetTaxiMountDisplayId(sourceNode, GetTeam(), true);
    uint32 path = m_taxi.GetCurrentTaxiPath();

    // search appropriate start path node
    uint32 startNode = 0;

    TaxiPathNodeList const& nodeList = sTaxiPathNodesByPath[path];

    float distPrev = MAP_SIZE * MAP_SIZE;
    float distNext =
        (nodeList[0].x - Where().X()) * (nodeList[0].x - Where().X()) +
        (nodeList[0].y - Where().Y()) * (nodeList[0].y - Where().Y()) +
        (nodeList[0].z - Where().Z()) * (nodeList[0].z - Where().Z());

    for (uint32 i = 1; i < nodeList.size(); ++i)
    {
        TaxiPathNodeEntry const& node = nodeList[i];
        TaxiPathNodeEntry const& prevNode = nodeList[i - 1];

        // skip nodes at another map
        if (node.ContinentID != GetMapId())
        {
            continue;
        }

        distPrev = distNext;

        distNext =
            (node.x - Where().X()) * (node.x - Where().X()) +
            (node.y - Where().Y()) * (node.y - Where().Y()) +
            (node.z - Where().Z()) * (node.z - Where().Z());

        float distNodes =
            (node.x - prevNode.x) * (node.x - prevNode.x) +
            (node.y - prevNode.y) * (node.y - prevNode.y) +
            (node.z - prevNode.z) * (node.z - prevNode.z);

        if (distNext + distPrev < distNodes)
        {
            startNode = i;
            break;
        }
    }

    GetSession()->SendDoFlight(mountDisplayId, path, startNode);
}
