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

#include "Object.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "World.h"
#include "Creature.h"
#include "Player.h"
#include "Vehicle.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "UpdateData.h"
#include "UpdateMask.h"
#include "Util.h"
#include "MapManager.h"
#include "Log.h"
#include "Transports.h"
#include "TargetedMovementGenerator.h"
#include "WaypointMovementGenerator.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "ObjectPosSelector.h"
#include "TemporarySummon.h"
#include "movement/packet_builder.h"
#include "CreatureLinkingMgr.h"
#include "Chat.h"
#include "GameTime.h"

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#include "ElunaConfig.h"
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @file WorldObjectPosition.cpp
 * @brief Cohesion split of Object.cpp -- WorldObject lifecycle and geometry: relocation, zone/area lookup, distance/range/angle math, line-of-sight and position validation. Same WorldObject class; no behaviour change. CMake file(GLOB Object/*.cpp) picks this file up automatically; Object.h is unchanged.
 */

/**
 * @brief WorldObject constructor
 *
 * Initializes a new WorldObject with default values.
 */
WorldObject::WorldObject() :
#ifdef ENABLE_ELUNA
    elunaEvents(nullptr),
#endif /* ENABLE_ELUNA */
    m_transportInfo(NULL),
    m_currMap(NULL),
    m_mapId(0), m_InstanceId(0), m_phaseMask(PHASEMASK_NORMAL),
    m_placement(DEFAULT_WORLD_OBJECT_SIZE),
    m_isActiveObject(false),
    m_visibilityDistanceOverride(0.0f)
{
}

/**
 * @brief WorldObject destructor
 *
 * Cleans up Eluna events if enabled.
 */
WorldObject::~WorldObject()
{
#ifdef ENABLE_ELUNA
    delete elunaEvents;
    elunaEvents = nullptr;
#endif /* ENABLE_ELUNA */
}

/**
 * @brief Cleanups before delete
 *
 * Removes the object from the world before deletion.
 */
void WorldObject::CleanupsBeforeDelete()
{
    RemoveFromWorld();
}

/**
 * @brief Update world object
 * @param update_diff Time since last update
 * @param time_diff Time parameter (unused)
 *
 * Updates Eluna events if enabled.
 */
void WorldObject::Update(uint32 update_diff, uint32 time_diff)
{
#ifdef ENABLE_ELUNA
    if (elunaEvents) // can be null on maps without eluna
    {
        elunaEvents->Update(update_diff);
    }
#endif /* ENABLE_ELUNA */
}

/**
 * @brief Create world object
 * @param guidlow Low GUID
 * @param guidhigh High GUID type
 *
 * Creates the world object with the specified GUID.
 */
void WorldObject::_Create(uint32 guidlow, HighGuid guidhigh, uint32 phaseMask)
{
    Object::_Create(guidlow, 0, guidhigh);
    m_phaseMask = phaseMask;
}


/**
 * @brief Get zone ID
 * @return Zone ID
 *
 * Returns the zone ID based on the object's position.
 */
uint32 WorldObject::GetZoneId() const
{
    return GetTerrain()->GetZoneId(Where().X(), Where().Y(), Where().Z());
}

/**
 * @brief Get area ID
 * @return Area ID
 *
 * Returns the area ID based on the object's position.
 */
uint32 WorldObject::GetAreaId() const
{
    return GetTerrain()->GetAreaId(Where().X(), Where().Y(), Where().Z());
}

/**
 * @brief Get zone and area IDs
 * @param zoneid Output zone ID
 * @param areaid Output area ID
 *
 * Returns both zone and area IDs based on the object's position.
 */
void WorldObject::GetZoneAndAreaId(uint32& zoneid, uint32& areaid) const
{
    GetTerrain()->GetZoneAndAreaId(zoneid, areaid, Where().X(), Where().Y(), Where().Z());
}

/**
 * @brief Get instance data
 * @return Instance data pointer
 *
 * Returns the instance data for the map this object is on.
 */
InstanceData* WorldObject::GetInstanceData() const
{
    return GetMap()->GetInstanceData();
}

/**
 * @brief A random ground point around a centre, in this object's own frame.
 *
 * The roll is injected rather than drawn here, so the pick stays pinnable in a test.
 */
Geometry::Vector3 RandomGroundPointNear(WorldObject const& obj, Geometry::Vector3 const& centre,
                                        float distance, float minDist, float const* ori)
{
    if (distance == 0.0f)
    {
        return centre;
    }

    const float angle = ori ? *ori : (rand_norm_f() * Geometry::Placement::TwoPi());

    Geometry::Placement around;
    around.EnterFrame(obj.Where().CurrentFrame(), centre, angle);

    Geometry::Vector3 point = around.RandomPointAround(minDist, distance, angle, rand_norm_f());
    MaNGOS::NormalizeMapCoord(point.x);
    MaNGOS::NormalizeMapCoord(point.y);
    DropToGround(obj, point.x, point.y, point.z);
    return point;
}

/**
 * @brief Put z on the floor under (x, y), if there is one.
 *
 * Nothing happens where the map has no floor to offer: an absent answer is absent, not a
 * sentinel height that arithmetic will happily consume.
 */
void DropToGround(WorldObject const& obj, float x, float y, float& z)
{
    if (auto floor = obj.GetMap()->Floor(obj.GetPhaseMask(), x, y, z))
    {
        z = *floor + 0.05f;                                 // just to be sure that we are not a few pixel under the surface
    }
}

/**
 * @brief Hold z between the floor and the highest surface this object may occupy.
 */
void ClampToAllowedZ(WorldObject const& obj, float x, float y, float& z, Map* atMap /*=NULL*/)
{
    if (!atMap)
    {
        atMap = obj.GetMap();
    }

    const auto floor = atMap->Floor(obj.GetPhaseMask(), x, y, z);
    if (!floor)
    {
        return;
    }

    // Anything that is not a unit has no say in the matter: it sits on the floor.
    const bool isUnit = obj.GetTypeId() == TYPEID_UNIT || obj.GetTypeId() == TYPEID_PLAYER;
    if (!isUnit)
    {
        z = *floor;
        return;
    }

    const Unit& unit = static_cast<const Unit&>(obj);
    if (unit.CanFly())
    {
        if (z < *floor)
        {
            z = *floor;
        }
        return;
    }

    // Held between the floor and the highest surface this unit may occupy: the water it
    // can swim in, or the floor itself when it cannot.
    float ceiling = *floor;
    if (unit.CanSwim())
    {
        ceiling = atMap->GetTerrain()->GetWaterOrGroundLevel(
                      x, y, z, NULL, !unit.HasAuraType(SPELL_AURA_WATER_WALK));
    }

    if (z > ceiling)
    {
        z = ceiling;
    }
    else if (z < *floor)
    {
        z = *floor;
    }
}

// ---- not geometry, so neither the object's nor the component's --------------
//
// Phasing and world membership are game state; line of sight and a map's coordinate
// bounds are the terrain engine's. Each of these asks the placement for the geometry and
// contributes only the part the placement must never know about.

/**
 * @brief The frame both objects can be answered for, if one exists.
 *
 * A frame is a map instance, so today this is simply "the same map copy", and the answer
 * is the two placements unchanged. THE SEAM IS HERE rather than inlined at the call sites
 * on purpose: 00, 01 and 02 carry a vessel branch here, because in those cores a ship IS
 * a map and a deck is its own frame. This core still runs the older TransportSystem, where
 * a boarded object's pose stays in the map's frame and its deck offset is carried beside
 * it -- so there is no second frame to reconcile yet, and adding one changes this function
 * and nothing else.
 */
static bool InCommonFrame(WorldObject const& a, WorldObject const& b,
                          Geometry::Placement& outA, Geometry::Placement& outB)
{
    outA = a.Where();
    outB = b.Where();
    return true;
}

/**
 * @brief CAN A REACH B AT ALL -- the question every melee swing, spell, threat entry and
 *        aggro check is really asking.
 *
 * It demands a COMMON FRAME and a shared phase. This is NOT the question "can B see A":
 * seeing a crow overhead is not being able to hit it. For that, ask CanBeSeen.
 */
bool CanInteract(WorldObject const& a, WorldObject const& b)
{
    Geometry::Placement pa, pb;
    return a.IsInWorld() && b.IsInWorld() && a.InSamePhase(&b) &&
           InCommonFrame(a, b, pa, pb) && pa.ShareFrame(pb);
}

/**
 * @brief CAN B BE SHOWN A -- a wider question, and a cheaper one.
 *
 * Wider than reach because a thing may be drawn without being touchable. In this core the
 * two coincide, since nothing here is measured in a frame it cannot also be reached in;
 * they are kept apart all the same, because every caller means one or the other and the
 * distinction is what the transport rework needs already drawn.
 */
bool CanBeSeen(WorldObject const& seen, WorldObject const& viewer)
{
    if (!seen.IsInWorld() || !viewer.IsInWorld() || !seen.InSamePhase(&viewer))
    {
        return false;
    }

    return seen.Where().ShareFrame(viewer.Where());
}

bool SeenWithin(WorldObject const& seen, WorldObject const& viewer, float dist, bool is3D)
{
    return CanBeSeen(seen, viewer) && seen.Where().WithinDist(viewer.Where(), dist, is3D);
}

bool InReach(WorldObject const& a, WorldObject const& b, float dist, bool is3D)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.WithinDist(pb, dist, is3D);
}

bool InFrontPhased(WorldObject const& a, WorldObject const& b, float dist, float arc)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.IsInFront(pb, dist, arc);
}

bool InBackPhased(WorldObject const& a, WorldObject const& b, float dist, float arc)
{
    Geometry::Placement pa, pb;
    return CanInteract(a, b) && InCommonFrame(a, b, pa, pb) &&
           pa.IsInBack(pb, dist, arc);
}

namespace
{
    // Shift `origin` toward `dest` by min(distance, reach). Mirrors TC's
    // GetHitSpherePointFor at tc-preservation/src/server/game/Entities/
    // Object/Object.cpp:1376-1402. Used to start the LoS ray at the
    // edge of a unit's collision sphere instead of its center so the
    // ray doesn't immediately self-collide with WMO geometry sitting
    // right next to the unit (e.g. cathedral columns whose AABB grazes
    // the unit's footprint). Players have small collision capsules and
    // are NOT shifted; only non-player units shift by combat reach.
    //
    // THIS CORE ONLY. 00, 01 and 02 cast the ray between bare positions; the shift was
    // established here against a live 5.4.8 client (.debug losdebug, 2026-05-23) and the
    // Stormwind Cathedral case it fixes is reproducible. Do not "align" it away.
    void ShiftBySphereRadius(float reach, float destX, float destY, float destZ,
                             float& x, float& y, float& z)
    {
        float dx = destX - x;
        float dy = destY - y;
        float dz = destZ - z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist < 1e-4f)
        {
            return;
        }
        float shift = std::min(dist, reach);
        x += (dx / dist) * shift;
        y += (dy / dist) * shift;
        z += (dz / dist) * shift;
    }

    /// The combat-reach shift a non-player endpoint contributes, if it is a unit at all.
    void ShiftEndpoint(WorldObject const& obj, float destX, float destY, float destZ,
                       float& x, float& y, float& z)
    {
        if (obj.GetTypeId() == TYPEID_PLAYER)
        {
            return;
        }

        if (Unit const* unit = obj.ToUnit())
        {
            ShiftBySphereRadius(unit->GetFloatValue(UNIT_FIELD_COMBATREACH),
                                destX, destY, destZ, x, y, z);
        }
    }
}

bool HasLineOfSight(WorldObject const& a, Geometry::Vector3 const& point,
                    world::terrain::ModelIgnoreFlags ignore)
{
    // The two-yard lift is eye height: a sight line is cast between heads, not feet.
    float x = a.Where().X(), y = a.Where().Y(), z = a.Where().Z() + 2.0f;
    const float ox = point.x, oy = point.y, oz = point.z + 2.0f;

    ShiftEndpoint(a, ox, oy, oz, x, y, z);

    return a.GetMap()->IsInLineOfSight(x, y, z, ox, oy, oz, a.GetPhaseMask(), ignore);
}

bool HasLineOfSight(WorldObject const& a, WorldObject const& b,
                    world::terrain::ModelIgnoreFlags ignore)
{
    if (!CanInteract(a, b))
    {
        return false;
    }

    float x = a.Where().X(), y = a.Where().Y(), z = a.Where().Z() + 2.0f;
    float ox = b.Where().X(), oy = b.Where().Y(), oz = b.Where().Z() + 2.0f;

    // Each endpoint is shifted off its own centre toward the ORIGINAL other end, so the
    // two shifts do not chase each other: the second must not aim at an endpoint the
    // first has already moved.
    const float srcX = x, srcY = y, srcZ = z;
    ShiftEndpoint(a, ox, oy, oz, x, y, z);
    ShiftEndpoint(b, srcX, srcY, srcZ, ox, oy, oz);

    return a.GetMap()->IsInLineOfSight(x, y, z, ox, oy, oz, a.GetPhaseMask(), ignore);
}

bool IsPlaceable(WorldObject const& obj)
{
    return obj.Where().IsFinite() &&
           MaNGOS::IsValidMapCoord(obj.Where().X(), obj.Where().Y(),
                                   obj.Where().Z(), obj.Where().Facing());
}
