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

#include "Common.h"

#include "Transports.h"
#include "TransportMap.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "Creature.h"
#include "Player.h"
#include "Path.h"
#include "GameTime.h"

#include "WorldPacket.h"
#include "DBCStores.h"
#include "ProgressBar.h"
#include "ScriptMgr.h"
#include "terrain/TileSerializer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

/**
 * @brief Loads and initializes all configured global transports.
 */
void MapManager::LoadTransports()
{
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `name`, `period` FROM `transports`");

    uint32 count = 0;
    uint32 mapped = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u transports", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Transport* t = new Transport;

        Field* fields = result->Fetch();

        uint32 entry = fields[0].GetUInt32();
        std::string name = fields[1].GetCppString();
        t->m_period = fields[2].GetUInt32();

        const GameObjectInfo* goinfo = ObjectMgr::GetGameObjectInfo(entry);

        if (!goinfo)
        {
            sLog.outErrorDb("Transport ID:%u, Name: %s, will not be loaded, gameobject_template missing", entry, name.c_str());
            delete t;
            continue;
        }

        if (goinfo->type != GAMEOBJECT_TYPE_MO_TRANSPORT)
        {
            sLog.outErrorDb("Transport ID:%u, Name: %s, will not be loaded, gameobject_template type wrong", entry, name.c_str());
            delete t;
            continue;
        }

        // sLog.outString("Loading transport %d between %s, %s", entry, name.c_str(), goinfo->name);

        std::set<uint32> mapsUsed;

        if (!t->GenerateWaypoints(goinfo->moTransport.taxiPathId, mapsUsed))
            // skip transports with empty waypoints list
        {
            sLog.outErrorDb("Transport (path id %u) path size = 0. Transport ignored, check DBC files or transport GO data0 field.", goinfo->moTransport.taxiPathId);
            delete t;
            continue;
        }

        float x, y, z, o;
        uint32 mapid;
        x = t->m_WayPoints[0].x; y = t->m_WayPoints[0].y; z = t->m_WayPoints[0].z; mapid = t->m_WayPoints[0].mapid; o = 1;

        // current code does not support transports in dungeon!
        const MapEntry* pMapInfo = sMapStore.LookupEntry(mapid);
        if (!pMapInfo || pMapInfo->Instanceable())
        {
            delete t;
            continue;
        }

        // A map for this vessel, resolved from the client or minted, before Create asks for it.
        Transport::RegisterVesselMap(entry, name.c_str());

        // creates the Gameobject
        if (!t->Create(entry, mapid, x, y, z, o, GO_ANIMPROGRESS_DEFAULT, 0))
        {
            delete t;
            continue;
        }

        m_Transports.insert(t);

        for (std::set<uint32>::const_iterator i = mapsUsed.begin(); i != mapsUsed.end(); ++i)
        {
            m_TransportsByMap[*i].insert(t);
        }

        t->SetMap(sMapMgr.CreateMap(mapid, t));

        // INTO THE WORLD'S GRID, as an ordinary object of the map she sails. That is what
        // makes IsInWorld() true -- without which every searcher refuses to see the vessel at
        // all -- and active, so the water she crosses stays awake with no player near it. Not
        // filed in a cell: nothing in this core relocates a game object's cell, so the tick
        // she needs comes from the map's own update instead.
        t->AddToWorld();
        t->SetActiveObjectState(true);
        t->GetMap()->AddToActive(t);

        t->PinRouteGrids();

        if (t->AsMap())
        {
            ++mapped;
            DETAIL_LOG("Transport %u '%s' is map %u", entry, name.c_str(), t->VesselMapId());
        }

        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u transports, %u with a map of their own", count, mapped);

    // check transport data DB integrity
    result = WorldDatabase.Query("SELECT `gameobject`.`guid`,`gameobject`.`id`,`transports`.`name` FROM `gameobject`,`transports` WHERE `gameobject`.`id` = `transports`.`entry`");
    if (result)                                             // wrong data found
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 guid  = fields[0].GetUInt32();
            uint32 entry = fields[1].GetUInt32();
            std::string name = fields[2].GetCppString();
            sLog.outErrorDb("Transport %u '%s' have record (GUID: %u) in `gameobject`. Transports DON'T must have any records in `gameobject` or its behavior will be unpredictable/bugged.", entry, name.c_str(), guid);
        }
        while (result->NextRow());

        delete result;
    }
}

Transport::Transport() : GameObject(), m_pathTime(0), m_timer(0), m_nodeSlack(0.0f),
    m_withdrawn(false), m_crossingTo(0), m_crossingX(0.0f), m_crossingY(0.0f), m_crossingZ(0.0f),
    m_crossing(false), m_map(NULL), m_nextNodeTime(0), m_period(0)
{
    m_updateFlag = UPDATEFLAG_TRANSPORT | UPDATEFLAG_HAS_POSITION | UPDATEFLAG_ROTATION;
}

bool Transport::Create(uint32 guidlow, uint32 mapid, float x, float y, float z, float ang, uint8 animprogress, uint16 dynamicHighValue)
{
    Relocate(x, y, z, ang);
    // instance id and phaseMask isn't set to values different from std.

    if (!IsPositionValid())
    {
        sLog.outError("Transport (GUID: %u) not created. Suggested coordinates isn't valid (X: %f Y: %f)",
                      guidlow, x, y);
        return false;
    }

    Object::_Create(guidlow, 0, HIGHGUID_MO_TRANSPORT);

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(guidlow);

    if (!goinfo)
    {
        sLog.outErrorDb("Transport not created: entry in `gameobject_template` not found, guidlow: %u map: %u  (X: %f Y: %f Z: %f) ang: %f", guidlow, mapid, x, y, z, ang);
        return false;
    }

    m_goInfo = goinfo;

    SetObjectScale(goinfo->size);

    SetUInt32Value(GAMEOBJECT_FACTION, goinfo->faction);
    // SetUInt32Value(GAMEOBJECT_FLAGS, goinfo->flags);
    SetUInt32Value(GAMEOBJECT_FLAGS, (GO_FLAG_TRANSPORT | GO_FLAG_NODESPAWN));
    SetUInt32Value(GAMEOBJECT_LEVEL, m_period);
    SetEntry(goinfo->id);

    //SetDisplayId(goinfo->displayId);
    // Use SetDisplayId only if we have the GO assigned to a proper map!
    SetUInt32Value(GAMEOBJECT_DISPLAYID, goinfo->displayId);
    m_displayInfo = sGameObjectDisplayInfoStore.LookupEntry(goinfo->displayId);

    SetGoState(GO_STATE_READY);
    SetGoType(GameobjectTypes(goinfo->type));
    SetGoArtKit(0);
    SetGoAnimProgress(animprogress);

    // low part always 0, dynamicHighValue is some kind of progression (not implemented)
    SetUInt16Value(GAMEOBJECT_DYNAMIC, 0, 0);
    SetUInt16Value(GAMEOBJECT_DYNAMIC, 1, dynamicHighValue);

    SetName(goinfo->name);
    SetWorldRotationAngles(ang, 0.0f, 0.0f);
    SetTransportPathRotation(QuaternionData(0.0f, 0.0f, 0.0f, 1.0f));

    // THE VESSEL IS A MAP. The client has a row for her hull and no terrain for it; the baker
    // fills that in from the hull's own model, so from here she answers height, collision and
    // routing through the ordinary engines. Model space is that map's space, which is why
    // nothing in the chain applies a transform.
    if (const uint32 mapId = VesselMapIdOf(goinfo->id))
    {
        Map* hull = sMapMgr.CreateMap(mapId, this);
        m_map = hull ? hull->AsTransport() : NULL;

        // A map that could not be commissioned is kept all the same: it is still the relay,
        // and it is what refuses to take anyone aboard.
        if (m_map)
        {
            m_map->Commission();
        }
    }
    else
    {
        sLog.outErrorDb("Transport %u (%s, display %u) has no map of its own.",
                        goinfo->id, goinfo->name, goinfo->displayId);
    }

    return true;
}

namespace
{
    /// Resolved once per vessel entry and then authoritative: the store it is derived from is
    /// MUTATED below (minted rows are injected into it), so re-deriving would see a different
    /// world each time.
    std::unordered_map<uint32, uint32> s_vesselMapByEntry;
    std::unordered_set<uint32> s_vesselMapIds;

    /// A DBCString is a MAX_LOCALE array, and Map.dbc's Directory is not a localized column --
    /// the loader fills the slot for the locale it read and leaves the rest empty. So the first
    /// non-empty entry is the string, whatever locale the realm was built from.
    char const* FirstNonEmpty(DBCString field)
    {
        if (!field)
        {
            return NULL;
        }

        for (uint32 i = 0; i < MAX_LOCALE; ++i)
        {
            if (field[i] && *field[i])
            {
                return field[i];
            }
        }

        return NULL;
    }

    /// Map.dbc rows keyed by Directory, built once before the first minting touches the store.
    std::unordered_map<std::string, uint32> const& VesselMapsByDirectory()
    {
        static std::unordered_map<std::string, uint32> index;
        static bool built = false;
        if (!built)
        {
            built = true;
            for (uint32 i = 0; i < sMapStore.GetNumRows(); ++i)
            {
                MapEntry const* row = sMapStore.LookupEntry(i);
                char const* directory = row ? FirstNonEmpty(row->Directory) : NULL;
                if (directory)
                {
                    index[directory] = row->ID;
                }
            }
        }
        return index;
    }
}

void Transport::RegisterVesselMap(uint32 goEntry, char const* vesselName)
{
    if (s_vesselMapByEntry.find(goEntry) != s_vesselMapByEntry.end())
    {
        return;
    }

    // 5.4.8's MapEntryfmt loads Directory, so a hull that Blizzard gave a row is MATCHED, not
    // guessed. (1.12's format string drops the field, which is why mangos_zero mints every
    // vessel map; do not copy that here -- a minted id for a hull the client already names
    // would make the baker and the server disagree about which map the mesh belongs to.)
    char directory[32];
    snprintf(directory, sizeof(directory), "Transport%u", goEntry);

    auto const& byDirectory = VesselMapsByDirectory();
    auto const found = byDirectory.find(directory);
    if (found != byDirectory.end())
    {
        s_vesselMapByEntry[goEntry] = found->second;
        s_vesselMapIds.insert(found->second);
        return;
    }

    // No row at all. Mint one, and inject a Map.dbc entry to carry it -- the baker agrees
    // through tools/extractor/vessels.txt, which uses the same arithmetic.
    const uint32 minted = world::terrain::MintedVesselMapId(goEntry);

    MapEntry* row = new MapEntry();
    row->ID = minted;
    row->InstanceType = MAP_COMMON;
    row->AreaTableID = 0;
    row->LoadingScreenID = 0;

    // A DBCString is a MAX_LOCALE array of pointers, not one string: the name has to be laid
    // out the way the loader would have, or GetMapName() indexes past the end of a char array.
    static std::list<std::string> s_names;
    static std::list<std::vector<char const*> > s_localeArrays;

    s_names.push_back(vesselName ? vesselName : "Vessel");
    s_localeArrays.push_back(std::vector<char const*>(MAX_LOCALE, s_names.back().c_str()));

    row->Directory = s_localeArrays.back().data();
    row->MapName_lang = s_localeArrays.back().data();

    sMapStore.SetEntry(minted, row);

    s_vesselMapByEntry[goEntry] = minted;
    s_vesselMapIds.insert(minted);

    DETAIL_LOG("Transport %u '%s' has no Map.dbc row; map %u minted.", goEntry,
               vesselName ? vesselName : "", minted);
}

uint32 Transport::VesselMapIdOf(uint32 goEntry)
{
    const auto found = s_vesselMapByEntry.find(goEntry);
    return found != s_vesselMapByEntry.end() ? found->second : 0;
}

bool Transport::IsVesselMapId(uint32 mapId)
{
    return s_vesselMapIds.find(mapId) != s_vesselMapIds.end();
}

Transport* Transport::VesselOf(WorldObject const& obj)
{
    // DERIVED, NEVER STORED. Being aboard is not a fact anyone records: it is what having this
    // map means. A creature summoned at sea, a crew member read from `creature`, a player who
    // walked up the gangplank -- all the same answer, with nothing to keep in step.
    //
    // A lift passenger and a vehicle rider are NOT here: their map is the world's, and the
    // seat transform is the vehicle system's business, not this one's.
    Map* map = obj.FindMap();
    TransportMap* hull = map ? map->AsTransport() : NULL;
    return hull ? hull->Vessel() : NULL;
}

Transport* Transport::GetTransport(Map const* map, ObjectGuid guid)
{
    if (!map || !guid)
    {
        return NULL;
    }

    MapManager::TransportsByMapType::const_iterator vessels =
        sMapMgr.m_TransportsByMap.find(map->GetId());
    if (vessels == sMapMgr.m_TransportsByMap.end())
    {
        return NULL;
    }

    for (Transport* vessel : vessels->second)
    {
        if (vessel->GetObjectGuid() == guid)
        {
            return vessel;
        }
    }

    return NULL;
}

void Transport::PinRouteGrids()
{
    // THE ROUTE'S GRIDS, LOADED AT START-UP AND NEVER LET GO. The vessel is an active object
    // in them, so what the relay finds ashore is whatever those cells hold -- and a cell that
    // had expired holds nothing, silently, and only once she was already sailing past it.
    uint32 pinned = 0;
    for (WayPointMap::value_type const& node : m_WayPoints)
    {
        if (Map* sailed = sMapMgr.CreateMap(node.second.mapid, this))
        {
            sailed->ForceLoadGrid(node.second.x, node.second.y);
            ++pinned;
        }
    }

    DETAIL_LOG("Transport %u '%s': %u route node(s) pinned.", GetEntry(), GetName(), pinned);
}

void Transport::WithdrawFromWorld()
{
    // Guarded because both teardown paths call it, and the second runs after the maps have
    // been deleted -- GetMap() would then point at freed memory.
    if (m_withdrawn)
    {
        return;
    }
    m_withdrawn = true;
    m_crossing = false;

    // Nothing else does this. A vessel is in no cell, so no grid unload reaches it, and the
    // map that owns it is deleted before the vessel is -- ~Object then asserts on an object
    // still flagged in-world, against a map that no longer exists.
    if (m_map)
    {
        m_map->ReleaseCrew();
    }

    if (Map* sailed = GetMap())
    {
        sailed->RemoveFromActive(this);
    }

    if (IsInWorld())
    {
        RemoveFromWorld();
    }

    m_map = NULL;
}

struct keyFrame
{
    explicit keyFrame(TaxiPathNodeEntry const& _node) : node(&_node),
        distSinceStop(-1.0f), distUntilStop(-1.0f), distFromPrev(-1.0f), tFrom(0.0f), tTo(0.0f)
    {
    }

    TaxiPathNodeEntry const* node;

    float distSinceStop;
    float distUntilStop;
    float distFromPrev;
    float tFrom, tTo;
};

/**
 * @brief Builds the waypoint timeline used by a global transport route.
 *
 * @return true if waypoint generation succeeded.
 */
bool Transport::GenerateWaypoints(uint32 pathid, std::set<uint32>& mapids)
{
    if (pathid >= sTaxiPathNodesByPath.size())
    {
        return false;
    }

    TaxiPathNodeList const& path = sTaxiPathNodesByPath[pathid];

    std::vector<keyFrame> keyFrames;
    int mapChange = 0;
    mapids.clear();
    for (size_t i = 1; i < path.size() - 1; ++i)
    {
        if (mapChange == 0)
        {
            TaxiPathNodeEntry const& node_i = path[i];
            if (node_i.ContinentID == path[i + 1].ContinentID)
            {
                keyFrame k(node_i);
                keyFrames.push_back(k);
                mapids.insert(k.node->ContinentID);
            }
            else
            {
                mapChange = 1;
            }
        }
        else
        {
            --mapChange;
        }
    }

    int lastStop = -1;
    int firstStop = -1;

    if (keyFrames.empty())
    {
        return false;
    }

    // first cell is arrived at by teleportation :S
    keyFrames[0].distFromPrev = 0;
    if (keyFrames[0].node->Flags == 2)
    {
        lastStop = 0;
    }

    // find the rest of the distances between key points
    for (size_t i = 1; i < keyFrames.size(); ++i)
    {
        if ((keyFrames[i].node->Flags == 1) || (keyFrames[i].node->ContinentID != keyFrames[i - 1].node->ContinentID))
        {
            keyFrames[i].distFromPrev = 0;
        }
        else
        {
            keyFrames[i].distFromPrev =
                sqrt(pow(keyFrames[i].node->x - keyFrames[i - 1].node->x, 2) +
                     pow(keyFrames[i].node->y - keyFrames[i - 1].node->y, 2) +
                     pow(keyFrames[i].node->z - keyFrames[i - 1].node->z, 2));
        }
        if (keyFrames[i].node->Flags == 2)
        {
            // remember first stop frame
            if (firstStop == -1)
            {
                firstStop = i;
            }
            lastStop = i;
        }
    }

    float tmpDist = 0;
    for (size_t i = 0; i < keyFrames.size(); ++i)
    {
        int j = (i + lastStop) % keyFrames.size();
        if (keyFrames[j].node->Flags == 2)
        {
            tmpDist = 0;
        }
        else
        {
            tmpDist += keyFrames[j].distFromPrev;
        }
        keyFrames[j].distSinceStop = tmpDist;
    }

    for (int i = int(keyFrames.size()) - 1; i >= 0; --i)
    {
        int j = (i + (firstStop + 1)) % keyFrames.size();
        tmpDist += keyFrames[(j + 1) % keyFrames.size()].distFromPrev;
        keyFrames[j].distUntilStop = tmpDist;
        if (keyFrames[j].node->Flags == 2)
        {
            tmpDist = 0;
        }
    }

    for (size_t i = 0; i < keyFrames.size(); ++i)
    {
        if (keyFrames[i].distSinceStop < (30 * 30 * 0.5f))
        {
            keyFrames[i].tFrom = sqrt(2 * keyFrames[i].distSinceStop);
        }
        else
        {
            keyFrames[i].tFrom = ((keyFrames[i].distSinceStop - (30 * 30 * 0.5f)) / 30) + 30;
        }

        if (keyFrames[i].distUntilStop < (30 * 30 * 0.5f))
        {
            keyFrames[i].tTo = sqrt(2 * keyFrames[i].distUntilStop);
        }
        else
        {
            keyFrames[i].tTo = ((keyFrames[i].distUntilStop - (30 * 30 * 0.5f)) / 30) + 30;
        }

        keyFrames[i].tFrom *= 1000;
        keyFrames[i].tTo *= 1000;
    }

    //    for (int i = 0; i < keyFrames.size(); ++i) {
    //        sLog.outString("%f, %f, %f, %f, %f, %f, %f", keyFrames[i].x, keyFrames[i].y, keyFrames[i].distUntilStop, keyFrames[i].distSinceStop, keyFrames[i].distFromPrev, keyFrames[i].tFrom, keyFrames[i].tTo);
    //    }

    // Now we're completely set up; we can move along the length of each waypoint at 100 ms intervals
    // speed = max(30, t) (remember x = 0.5s^2, and when accelerating, a = 1 unit/s^2
    int t = 0;
    bool teleport = false;
    if (keyFrames[keyFrames.size() - 1].node->ContinentID != keyFrames[0].node->ContinentID)
    {
        teleport = true;
    }

    WayPoint pos(keyFrames[0].node->ContinentID, keyFrames[0].node->x, keyFrames[0].node->y, keyFrames[0].node->z, teleport,
                 keyFrames[0].node->ArrivalEventID, keyFrames[0].node->DepartureEventID);
    m_WayPoints[0] = pos;
    t += keyFrames[0].node->Delay * 1000;

    uint32 cM = keyFrames[0].node->ContinentID;
    for (size_t i = 0; i < keyFrames.size() - 1; ++i)
    {
        float d = 0;
        float tFrom = keyFrames[i].tFrom;
        float tTo = keyFrames[i].tTo;

        // keep the generation of all these points; we use only a few now, but may need the others later
        if (((d < keyFrames[i + 1].distFromPrev) && (tTo > 0)))
        {
            while ((d < keyFrames[i + 1].distFromPrev) && (tTo > 0))
            {
                tFrom += 100;
                tTo -= 100;

                if (d > 0)
                {
                    float newX, newY, newZ;
                    newX = keyFrames[i].node->x + (keyFrames[i + 1].node->x - keyFrames[i].node->x) * d / keyFrames[i + 1].distFromPrev;
                    newY = keyFrames[i].node->y + (keyFrames[i + 1].node->y - keyFrames[i].node->y) * d / keyFrames[i + 1].distFromPrev;
                    newZ = keyFrames[i].node->z + (keyFrames[i + 1].node->z - keyFrames[i].node->z) * d / keyFrames[i + 1].distFromPrev;

                    teleport = false;
                    if (keyFrames[i].node->ContinentID != cM)
                    {
                        teleport = true;
                        cM = keyFrames[i].node->ContinentID;
                    }

                    //                    sLog.outString("T: %d, D: %f, x: %f, y: %f, z: %f", t, d, newX, newY, newZ);
                    pos = WayPoint(keyFrames[i].node->ContinentID, newX, newY, newZ, teleport);
                    if (teleport)
                    {
                        m_WayPoints[t] = pos;
                    }
                }

                if (tFrom < tTo)                            // caught in tFrom dock's "gravitational pull"
                {
                    if (tFrom <= 30000)
                    {
                        d = 0.5f * (tFrom / 1000) * (tFrom / 1000);
                    }
                    else
                    {
                        d = 0.5f * 30 * 30 + 30 * ((tFrom - 30000) / 1000);
                    }
                    d = d - keyFrames[i].distSinceStop;
                }
                else
                {
                    if (tTo <= 30000)
                    {
                        d = 0.5f * (tTo / 1000) * (tTo / 1000);
                    }
                    else
                    {
                        d = 0.5f * 30 * 30 + 30 * ((tTo - 30000) / 1000);
                    }
                    d = keyFrames[i].distUntilStop - d;
                }
                t += 100;
            }
            t -= 100;
        }

        if (keyFrames[i + 1].tFrom > keyFrames[i + 1].tTo)
        {
            t += 100 - ((long)keyFrames[i + 1].tTo % 100);
        }
        else
        {
            t += (long)keyFrames[i + 1].tTo % 100;
        }

        bool teleport = false;
        if ((keyFrames[i + 1].node->Flags == 1) || (keyFrames[i + 1].node->ContinentID != keyFrames[i].node->ContinentID))
        {
            teleport = true;
            cM = keyFrames[i + 1].node->ContinentID;
        }

        pos = WayPoint(keyFrames[i + 1].node->ContinentID, keyFrames[i + 1].node->x, keyFrames[i + 1].node->y, keyFrames[i + 1].node->z, teleport,
                     keyFrames[i + 1].node->ArrivalEventID, keyFrames[i + 1].node->DepartureEventID);

        //        sLog.outString("T: %d, x: %f, y: %f, z: %f, t:%d", t, pos.x, pos.y, pos.z, teleport);

        // if (teleport)
        m_WayPoints[t] = pos;

        t += keyFrames[i + 1].node->Delay * 1000;
        //        sLog.outString("------");
    }

    uint32 timer = t;

    //    sLog.outDetail("    Generated %lu waypoints, total time %u.", (unsigned long)m_WayPoints.size(), timer);

    m_next = m_WayPoints.begin();                           // will used in MoveToNextWayPoint for init m_curr
    MoveToNextWayPoint();                                   // m_curr -> first point
    MoveToNextWayPoint();                                   // skip first point

    m_pathTime = timer;

    m_nextNodeTime = m_curr->first;

    // How wrong the estimate is allowed to be. The pose snaps from node to node and is never
    // interpolated, so at worst it sits half a segment away from where the client draws the
    // hull. Every proximity question about this vessel is widened by that.
    m_nodeSlack = 0.0f;
    for (WayPointMap::const_iterator it = m_WayPoints.begin(); it != m_WayPoints.end(); ++it)
    {
        WayPointMap::const_iterator nxt = it;
        if (++nxt == m_WayPoints.end() || nxt->second.mapid != it->second.mapid)
        {
            continue;
        }

        const float dx = nxt->second.x - it->second.x;
        const float dy = nxt->second.y - it->second.y;
        m_nodeSlack = std::max(m_nodeSlack, std::sqrt(dx * dx + dy * dy) * 0.5f);
    }

    return true;
}

/**
 * @brief Advances the current and next transport waypoint pointers.
 */
void Transport::MoveToNextWayPoint()
{
    m_curr = m_next;

    ++m_next;
    if (m_next == m_WayPoints.end())
    {
        m_next = m_WayPoints.begin();
    }
}

/**
 * @brief Records that the route wants a change of world map. Nothing acts on it here.
 *
 * @param newMapid The destination map id.
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 */
void Transport::TeleportTransport(uint32 newMapid, float x, float y, float z)
{
    Map* oldMap = GetMap();

    // The route decided WHEN; what crossing means for anyone standing on her is not this
    // object's business. Her own map is told and owns every consequence.
    Relocate(x, y, z);

    // A node flagged for teleport that does not leave this map: nothing changes for anyone.
    // Her passengers' coordinates are her own map's and do not move, and the client draws the
    // jump itself out of the path progress.
    if (!oldMap || oldMap->GetId() == newMapid)
    {
        return;
    }

    // AND NOT ONE STEP FURTHER ON THIS THREAD. Crossing writes into another map's active list,
    // object store and player list, and that map may be running right now on another core.
    // Worse, half a crossing is a vessel that reports a map she is no longer on. So the route
    // only says GO; all of it happens at once, at MapManager's barrier.
    m_crossingTo = newMapid;
    m_crossingX = x;
    m_crossingY = y;
    m_crossingZ = z;
    m_crossing = true;
}

void Transport::CompleteCrossing()
{
    if (!m_crossing)
    {
        return;
    }

    m_crossing = false;

    const uint32 newMapid = m_crossingTo;
    m_crossingTo = 0;

    Map* oldMap = GetMap();
    Map* newMap = sMapMgr.CreateMap(newMapid, this);
    if (!oldMap || !newMap || oldMap == newMap)
    {
        return;
    }

    // THIS SIDE FIRST, while she is still on it: the shore loses her, and her passengers are
    // started on their way. The transfer packet they get names the map they are leaving, which
    // one line later would already be the map they are going to.
    if (m_map)
    {
        // The node's own coordinates, not the pose. Same number the client's path is built
        // from, so nobody is put down anywhere the ship is not.
        m_map->VesselLeavingWorld(oldMap, newMapid, m_crossingX, m_crossingY, m_crossingZ,
                                  GetOrientation());
    }

    oldMap->RemoveFromActive(this);
    RemoveFromWorld();

    SetMap(newMap);

    AddToWorld();
    newMap->AddToActive(this);

    if (m_map)
    {
        m_map->VesselEnteredWorld(newMap);
    }
}

/**
 * @brief Runs the route clock, then the vessel's own map.
 *
 * @param update_diff The elapsed update time.
 * @param p_time The current path time parameter.
 */
void Transport::Update(uint32 update_diff, uint32 /*p_time*/)
{
    // Between two world maps: she belongs to neither until the barrier hands her over, and her
    // own map does not tick without her.
    if (m_crossing)
    {
        return;
    }

    if (m_WayPoints.size() > 1 && m_period)
    {
        // Absolute wall-clock, NOT milliseconds since this process started. The phase of a
        // route has to survive a restart: keyed off uptime, every vessel sails from the
        // beginning of its path each time we come up, while the client -- which interpolates
        // the hull from the value we hand it -- draws her somewhere else entirely.
        const uint32 mapBefore = GetMapId();

        m_timer = uint32(GameTime::GetAbsoluteTimeMS() % m_period);
        while (((m_timer - m_curr->first) % m_pathTime) > ((m_next->first - m_curr->first) % m_pathTime))
        {
            DoEventIfAny(*m_curr, true);

            MoveToNextWayPoint();

            DoEventIfAny(*m_curr, false);

            // THE TIME OF THE TRANSFER. The one thing the route has to decide: this node
            // belongs to another world map, so the ship changes which map she sails, and
            // everyone aboard follows.
            if (m_curr->second.mapid != GetMapId() || m_curr->second.teleport)
            {
                TeleportTransport(m_curr->second.mapid, m_curr->second.x, m_curr->second.y, m_curr->second.z);
            }
            else
            {
                Relocate(m_curr->second.x, m_curr->second.y, m_curr->second.z);
            }

            m_nextNodeTime = m_curr->first;

            if (m_curr == m_WayPoints.begin())
            {
                DETAIL_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, " ************ BEGIN ************** %s", GetName());
            }

            DETAIL_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, "%s moved to %f %f %f %d", GetName(), m_curr->second.x, m_curr->second.y, m_curr->second.z, m_curr->second.mapid);

            if (m_crossing)
            {
                return;
            }
        }

        if (GetMapId() != mapBefore)
        {
            return;
        }
    }

    // LAST, AND IT MUST STAY LAST. The ship's own map runs nested inside this tick, on the
    // thread of the world map she sails and after that map has finished walking its own
    // containers -- which is what lets everything it sends go straight out, with no queue and
    // no tick of latency.
    if (m_map)
    {
        m_map->Update(update_diff);
    }
}

void Transport::DoEventIfAny(WayPointMap::value_type const& node, bool departure)
{
    if (uint32 eventid = departure ? node.second.departureEventID : node.second.arrivalEventID)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_TRANSPORT_MOVES, "Taxi %s event %u of node %u of %s \"%s\") path", departure ? "departure" : "arrival", eventid, node.first, GetGuidStr().c_str(), GetName());

        if (!sScriptMgr.OnProcessEvent(eventid, this, this, departure))
        {
            GetMap()->ScriptsStart(DBS_ON_EVENT, eventid, this, this);
        }
    }
}
