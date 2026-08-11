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

#include <ctime>

#include "WaypointMovementGenerator.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "WaypointManager.h"
#include "WorldPacket.h"
#include "ScriptMgr.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "PathFinder.h"

#include <cassert>

using Movement::PointsArray;

namespace
{
    /**
     * @brief Pathfinds one waypoint leg and appends its points to a smoothed path.
     * @param creature Reference to the creature.
     * @param startX Leg start X-coordinate.
     * @param startY Leg start Y-coordinate.
     * @param startZ Leg start Z-coordinate.
     * @param endNode Waypoint the leg ends at.
     * @param pathPoints Path being built; the leg start point is added only when empty.
     * @return False if the leg could not be routed, or if it does not start where the
     *         previous one was committed. Near-duplicate points are dropped, so a
     *         degenerate leg may append nothing while still succeeding.
     */
    bool AppendWaypointPathSegment(Creature& creature, float startX, float startY, float startZ, WaypointNode const& endNode, PointsArray& pathPoints)
    {
        PathFinder path(&creature);
        if (!path.calculate(startX, startY, startZ, endNode.x, endNode.y, endNode.z))
        {
            return false;
        }

        if (path.getPathType() & (PATHFIND_NOPATH | PATHFIND_NOT_USING_PATH))
        {
            return false;
        }

        PointsArray const& segment = path.getPath();
        if (segment.size() < 2)
        {
            return false;
        }

        // Later legs are welded by omitting segment.front(), which only holds if this
        // leg really does start where the previous one was committed. That was
        // guaranteed while the shortcut fallback returned the exact requested
        // endpoints, but a routed leg ends on a navmesh-adjusted point while the next
        // leg is calculated from the raw waypoint, so the two can part company.
        //
        // The resulting seam is small and finite, and survives the point filter below
        // whenever it is at least MIN_SEGMENT_LENGTH. It still cannot be sent: the
        // linear path packs intermediate offsets by truncating each component toward
        // zero at 0.25yd (ByteBuffer::appendPackXYZ), so a seam under a quarter yard
        // per component packs to all-zero fields and the client reconstructs the point
        // on top of its neighbour. That zero-length segment makes the client divide by
        // the segment length, compute 0/0, and store NaN as the mover's position.
        //
        // Refuse the leg instead. The builder already treats a later failed leg as a
        // clean chunk boundary and keeps what it has assembled.
        if (!pathPoints.empty() &&
            (segment.front() - pathPoints.back()).length() >= WAYPOINT_SMOOTHING_MIN_SEGMENT_LENGTH)
        {
            return false;
        }

        if (pathPoints.empty())
        {
            pathPoints.push_back(segment.front());
        }

        for (PointsArray::const_iterator itr = segment.begin() + 1; itr != segment.end(); ++itr)
        {
            // Drop zero-length segments: the client cannot interpolate across them.
            if ((*itr - pathPoints.back()).length() < WAYPOINT_SMOOTHING_MIN_SEGMENT_LENGTH)
            {
                continue;
            }

            pathPoints.push_back(*itr);
        }

        return true;
    }
}

/**
 * @brief Loads the waypoint path for the creature.
 * @param creature Reference to the creature.
 * @param pathId ID of the waypoint path.
 * @param wpOrigin Origin of the waypoint path.
 * @param overwriteEntry Entry to overwrite.
 */
void WaypointMovementGenerator<Creature>::LoadPath(Creature& creature, int32 pathId, WaypointPathOrigin wpOrigin, uint32 overwriteEntry)
{
    DETAIL_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "LoadPath: loading waypoint path for %s", creature.GetGuidStr().c_str());

    ClearActiveSegment();

    if (!overwriteEntry)
    {
        overwriteEntry = creature.GetEntry();
    }

    if (wpOrigin == PATH_NO_PATH && pathId == 0)
    {
        i_path = sWaypointMgr.GetDefaultPath(overwriteEntry, creature.GetGUIDLow(), &m_PathOrigin);
    }
    else
    {
        m_PathOrigin = wpOrigin == PATH_NO_PATH ? PATH_FROM_ENTRY : wpOrigin;
        i_path = sWaypointMgr.GetPathFromOrigin(overwriteEntry, creature.GetGUIDLow(), pathId, m_PathOrigin);
    }
    m_pathId = pathId;

    // No movement found for entry nor guid
    if (!i_path)
    {
        if (m_PathOrigin == PATH_FROM_EXTERNAL)
        {
            sLog.outErrorScriptLib("#1 WaypointMovementGenerator::LoadPath: %s doesn't have waypoint path %i", creature.GetGuidStr().c_str(), pathId);
        }
        else
        {
            sLog.outErrorDb("#2 WaypointMovementGenerator::LoadPath: %s doesn't have waypoint path %i", creature.GetGuidStr().c_str(), pathId);
        }
        return;
    }

    if (i_path->empty())
    {
        return;
    }
    // Initialize the i_currentNode to point to the first node
    i_currentNode = i_path->begin()->first;
    m_lastReachedWaypoint = 0;
}

/**
 * @brief Initializes the WaypointMovementGenerator.
 * @param creature Reference to the creature.
 */
void WaypointMovementGenerator<Creature>::Initialize(Creature& creature)
{
    creature.addUnitState(UNIT_STAT_ROAMING);
    creature.clearUnitState(UNIT_STAT_WAYPOINT_PAUSED);
}

/**
 * @brief Initializes the waypoint path for the creature.
 * @param u Reference to the creature.
 * @param id ID of the waypoint path.
 * @param wpSource Source of the waypoint path.
 * @param initialDelay Initial delay before starting the movement.
 * @param overwriteEntry Entry to overwrite.
 */
void WaypointMovementGenerator<Creature>::InitializeWaypointPath(Creature& u, int32 id, WaypointPathOrigin wpSource, uint32 initialDelay, uint32 overwriteEntry)
{
    LoadPath(u, id, wpSource, overwriteEntry);
    i_nextMoveTime.Reset(initialDelay);
    // Start moving if possible
    StartMove(u);
}

/**
 * @brief Finalizes the WaypointMovementGenerator.
 * @param creature Reference to the creature.
 */
void WaypointMovementGenerator<Creature>::Finalize(Creature& creature)
{
    ClearActiveSegment();
    creature.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    creature.SetWalk(!creature.hasUnitState(UNIT_STAT_RUNNING_STATE), false);
}

/**
 * @brief Interrupts the WaypointMovementGenerator.
 * @param creature Reference to the creature.
 */
void WaypointMovementGenerator<Creature>::Interrupt(Creature& creature)
{
    ClearActiveSegment();
    creature.InterruptMoving();
    creature.clearUnitState(UNIT_STAT_ROAMING | UNIT_STAT_ROAMING_MOVE);
    creature.SetWalk(!creature.hasUnitState(UNIT_STAT_RUNNING_STATE), false);
}

/**
 * @brief Resets the WaypointMovementGenerator.
 * @param creature Reference to the creature.
 */
void WaypointMovementGenerator<Creature>::Reset(Creature& creature)
{
    creature.addUnitState(UNIT_STAT_ROAMING);
    StartMove(creature);
}

/**
 * @brief Called when the creature arrives at a waypoint.
 * @param creature Reference to the creature.
 */
void WaypointMovementGenerator<Creature>::OnArrived(Creature& creature)
{
    if (!i_path || i_path->empty())
    {
        return;
    }

    m_lastReachedWaypoint = i_currentNode;

    if (m_isArrivalDone)
    {
        return;
    }

    creature.clearUnitState(UNIT_STAT_ROAMING_MOVE);
    m_isArrivalDone = true;

    WaypointPath::const_iterator currPoint = i_path->find(i_currentNode);
    MANGOS_ASSERT(currPoint != i_path->end());
    WaypointNode const& node = currPoint->second;

    if (node.script_id)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Creature movement start script %u at point %u for %s.", node.script_id, i_currentNode, creature.GetGuidStr().c_str());
        creature.GetMap()->ScriptsStart(DBS_ON_CREATURE_MOVEMENT, node.script_id, &creature, &creature);
    }

    // We have reached the destination and can process behavior
    if (WaypointBehavior* behavior = node.behavior)
    {
        if (behavior->emote != 0)
        {
            creature.HandleEmote(behavior->emote);
        }

        if (behavior->spell != 0)
        {
            creature.CastSpell(&creature, behavior->spell, false);
        }

        if (behavior->model1 != 0)
        {
            creature.SetDisplayId(behavior->model1);
        }

        if (behavior->textid[0])
        {
            int32 textId = behavior->textid[0];
            // Not only one text is set
            if (behavior->textid[1])
            {
                // Select one from max 5 texts (0 and 1 already checked)
                int i = 2;
                for (; i < MAX_WAYPOINT_TEXT; ++i)
                {
                    if (!behavior->textid[i])
                    {
                        break;
                    }
                }

                textId = behavior->textid[urand(0, i - 1)];
            }

            if (MangosStringLocale const* textData = sObjectMgr.GetMangosStringLocale(textId))
            {
                creature.MonsterText(textData, NULL);
            }
            else
            {
                sLog.outErrorDb("%s reached waypoint %u, attempted to do text %i, but required text-data could not be found", creature.GetGuidStr().c_str(), i_currentNode, textId);
            }
        }
    }

    // Inform script
    if (creature.AI())
    {
        uint32 type = WAYPOINT_MOTION_TYPE;
        if (m_PathOrigin == PATH_FROM_EXTERNAL && m_pathId > 0)
        {
            type = EXTERNAL_WAYPOINT_MOVE + m_pathId;
        }
        creature.AI()->MovementInform(type, i_currentNode);
    }

    // Wait delay ms
    Stop(node.delay);
}

/**
 * @brief Whether smoothing is allowed for the current path origin.
 * @return True for all origins except externally-scripted paths.
 */
bool WaypointMovementGenerator<Creature>::IsSmoothingEnabled() const
{
    return m_PathOrigin != PATH_FROM_EXTERNAL;
}

/**
 * @brief Whether a segment may smooth through the given node without stopping.
 * @param node Waypoint node to test.
 * @return True if the node has no delay, script or behavior.
 */
bool WaypointMovementGenerator<Creature>::CanSmoothThrough(WaypointNode const& node) const
{
    WaypointSmoothingNode smoothingNode;
    smoothingNode.hasDelay = node.delay != 0;
    smoothingNode.hasScript = node.script_id != 0;
    smoothingNode.hasBehavior = node.behavior != NULL && !node.behavior->isEmpty();

    return IsWaypointSmoothingSafe(smoothingNode);
}

/**
 * @brief Drops any tracked active smoothed segment.
 */
void WaypointMovementGenerator<Creature>::ClearActiveSegment()
{
    m_activeSegmentWaypoints.clear();
    m_activeSegmentArrivals = 0;
}

/**
 * @brief Records a waypoint reached within the active smoothed segment.
 * @param pointId Waypoint id in the path.
 * @param pathPointIndex Index of its endpoint within the spline path.
 */
void WaypointMovementGenerator<Creature>::AddActiveSegmentWaypoint(uint32 pointId, size_t pathPointIndex)
{
    ActiveSegmentWaypoint waypoint = { pointId, pathPointIndex };
    m_activeSegmentWaypoints.push_back(waypoint);
}

/**
 * @brief Fires arrival handling for any smoothed waypoints the spline has passed.
 * @param creature Reference to the creature.
 */
void WaypointMovementGenerator<Creature>::ProcessActiveSegmentProgress(Creature& creature)
{
    while (m_activeSegmentArrivals < m_activeSegmentWaypoints.size() &&
           HasReachedWaypointEndpoint(creature.movespline->currentPathIdx(), m_activeSegmentWaypoints[m_activeSegmentArrivals].pathPointIndex))
    {
        i_currentNode = m_activeSegmentWaypoints[m_activeSegmentArrivals].pointId;
        m_isArrivalDone = false;
        OnArrived(creature);
        ++m_activeSegmentArrivals;

        if (!creature.movespline->Finalized() && !Stopped(creature))
        {
            creature.addUnitState(UNIT_STAT_ROAMING_MOVE);
        }
    }
}

/**
 * @brief Builds a smoothed multi-waypoint path starting at the given waypoint.
 * @param creature Reference to the creature.
 * @param startPoint Iterator to the first waypoint of the segment.
 * @param pathPoints Output spline points; left empty when smoothing is skipped.
 */
void WaypointMovementGenerator<Creature>::BuildSmoothPath(Creature& creature, WaypointPath::const_iterator startPoint, PointsArray& pathPoints)
{
    ClearActiveSegment();
    pathPoints.clear();

    // Skip the (up to WAYPOINT_SMOOTHING_MAX_LOOKAHEAD) smoothing navmesh queries when
    // no player is on the map to see the result: leaving pathPoints empty makes StartMove
    // fall back to plain per-waypoint MoveTo. Under LivingWorld this stops thousands of
    // player-less creatures from saturating the single per-continent update thread.
    if (!IsSmoothingEnabled() || !creature.GetMap()->HavePlayers())
    {
        return;
    }

    float startX = creature.Where().X();
    float startY = creature.Where().Y();
    float startZ = creature.Where().Z();

    // Bounding box of the points committed to the smoothed path so far. We keep extending
    // the chunk through consecutive smoothable waypoints until the box would exceed the
    // packable offset budget (see WaypointSmoothing.h) rather than stopping at a fixed
    // count, so the spline carries as many waypoints as the SMSG_MONSTER_MOVE encoding
    // safely allows. This minimises spline finalize/relaunch boundaries (each of which is a
    // visible stop/relaunch) while guaranteeing the packed offsets never wrap.
    WaypointSmoothingBounds bounds;

    WaypointPath::const_iterator currPoint = startPoint;
    for (size_t segment = 0; segment < WAYPOINT_SMOOTHING_MAX_LOOKAHEAD; ++segment)
    {
        WaypointNode const& node = currPoint->second;

        size_t const committedSize = pathPoints.size();
        if (!AppendWaypointPathSegment(creature, startX, startY, startZ, node, pathPoints))
        {
            // The first leg failing means nothing is usable; fall back to per-waypoint
            // movement. A later leg failing keeps the chunk built so far.
            if (committedSize == 0)
            {
                ClearActiveSegment();
                pathPoints.clear();
                return;
            }
            break;
        }

        // The first waypoint is always accepted (a one-waypoint chunk falls back to MoveTo
        // below). Each subsequent waypoint is only kept while the whole path stays within
        // the packable budget; otherwise roll it back and end the chunk here.
        WaypointSmoothingBounds trial = bounds;
        for (size_t i = committedSize; i < pathPoints.size(); ++i)
        {
            AddWaypointSmoothingPoint(trial, pathPoints[i].x, pathPoints[i].y, pathPoints[i].z);
        }

        if (committedSize != 0 && !IsWaypointSmoothingWithinBudget(trial))
        {
            pathPoints.resize(committedSize);
            break;
        }

        bounds = trial;
        AddActiveSegmentWaypoint(currPoint->first, pathPoints.size() - 1);

        if (!CanSmoothThrough(node))
        {
            break;
        }

        WaypointPath::const_iterator nextPoint = currPoint;
        ++nextPoint;
        if (nextPoint == i_path->end())
        {
            nextPoint = i_path->begin();
        }

        if (nextPoint == startPoint)
        {
            break;
        }

        startX = node.x;
        startY = node.y;
        startZ = node.z;
        currPoint = nextPoint;
    }

    // A merged path can collapse below a usable spline when every leg was
    // degenerate; fall back to per-waypoint movement in that case too.
    if (m_activeSegmentWaypoints.size() <= 1 || pathPoints.size() < 2)
    {
        ClearActiveSegment();
        pathPoints.clear();
    }
}

/**
 * @brief Starts moving the creature along the waypoint path.
 * @param creature Reference to the creature.
 */
void WaypointMovementGenerator<Creature>::StartMove(Creature& creature)
{
    if (!i_path || i_path->empty())
    {
        return;
    }

    if (Stopped(creature))
    {
        return;
    }

    if (!creature.IsAlive() || creature.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        return;
    }

    WaypointPath::const_iterator currPoint = i_path->find(i_currentNode);
    MANGOS_ASSERT(currPoint != i_path->end());

    if (WaypointBehavior* behavior = currPoint->second.behavior)
    {
        if (behavior->model2 != 0)
        {
            creature.SetDisplayId(behavior->model2);
        }
        creature.SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);
    }

    if (m_isArrivalDone)
    {
        bool reachedLast = false;
        ++currPoint;
        if (currPoint == i_path->end())
        {
            reachedLast = true;
            currPoint = i_path->begin();
        }

        // Inform AI
        if (creature.AI() && m_PathOrigin == PATH_FROM_EXTERNAL &&  m_pathId > 0)
        {
            if (!reachedLast)
            {
                creature.AI()->MovementInform(EXTERNAL_WAYPOINT_MOVE_START + m_pathId, currPoint->first);
            }
            else
            {
                creature.AI()->MovementInform(EXTERNAL_WAYPOINT_FINISHED_LAST + m_pathId, currPoint->first);
            }

            if (creature.IsDead() || !creature.IsInWorld()) // Might have happened with above calls
            {
                return;
            }
        }

        i_currentNode = currPoint->first;
    }

    m_isArrivalDone = false;

    creature.addUnitState(UNIT_STAT_ROAMING_MOVE);

    WaypointNode const& nextNode = currPoint->second;
    Movement::MoveSplineInit init(creature);
    PointsArray smoothPath;
    BuildSmoothPath(creature, currPoint, smoothPath);

    WaypointNode const* finalNode = &nextNode;
    if (!smoothPath.empty())
    {
        init.MovebyPath(smoothPath);

        WaypointPath::const_iterator finalPoint = i_path->find(m_activeSegmentWaypoints.back().pointId);
        MANGOS_ASSERT(finalPoint != i_path->end());
        finalNode = &finalPoint->second;
    }
    else
    {
        // generatePath only when a player can observe path quality; on a player-less
        // continent a straight 2-point segment between adjacent on-mesh waypoints avoids
        // the last per-leg navmesh query (and isn't broadcast to anyone either - see
        // Map::MessageBroadcast). Players present -> full navmesh-hugging movement.
        init.MoveTo(nextNode.x, nextNode.y, nextNode.z, creature.GetMap()->HavePlayers());
    }

    if (finalNode->orientation != 100 && finalNode->delay != 0)
    {
        init.SetFacing(finalNode->orientation);
    }
    creature.SetWalk(!creature.hasUnitState(UNIT_STAT_RUNNING_STATE) && !creature.IsLevitating(), false);
    init.Launch();
}

/**
 * @brief Updates the WaypointMovementGenerator.
 * @param creature Reference to the creature.
 * @param diff Time difference.
 * @return True if the update was successful, false otherwise.
 */
bool WaypointMovementGenerator<Creature>::Update(Creature& creature, const uint32& diff)
{
    // Waypoint movement can be switched on/off
    // This is quite handy for escort quests and other stuff
    if (creature.hasUnitState(UNIT_STAT_NOT_MOVE))
    {
        creature.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return true;
    }

    // Prevent a crash at empty waypoint path.
    if (!i_path || i_path->empty())
    {
        creature.clearUnitState(UNIT_STAT_ROAMING_MOVE);
        return true;
    }

    if (Stopped(creature))
    {
        if (CanMove(diff, creature))
        {
            StartMove(creature);
        }
    }
    else
    {
        // Sample the stopped state before arrival handling clears UNIT_STAT_ROAMING_MOVE,
        // so an externally force-stopped unit (e.g. a player talking to it, which also
        // finalizes the spline) is paused rather than mistaken for a finishing spline.
        switch (GetWaypointSegmentUpdateState(creature.movespline->Finalized(), creature.IsStopped()))
        {
            case WaypointSegmentUpdateState::Stopped:
            {
                ClearActiveSegment();
                Stop(STOP_TIME_FOR_PLAYER);
                break;
            }
            case WaypointSegmentUpdateState::Finalized:
            {
                ProcessActiveSegmentProgress(creature);
                if (!m_isArrivalDone)
                {
                    OnArrived(creature);
                }
                ClearActiveSegment();
                StartMove(creature);
                break;
            }
            case WaypointSegmentUpdateState::Moving:
            {
                ProcessActiveSegmentProgress(creature);
                break;
            }
        }
    }
    return true;
}

/**
 * @brief Checks if the creature is stopped.
 * @param u Reference to the creature.
 * @return True if the creature is stopped, false otherwise.
 */
bool WaypointMovementGenerator<Creature>::Stopped(Creature& u)
{
    return !i_nextMoveTime.Passed() || u.hasUnitState(UNIT_STAT_WAYPOINT_PAUSED);
}

/**
 * @brief Checks if the creature can move.
 * @param diff Time difference.
 * @param u Reference to the creature.
 * @return True if the creature can move, false otherwise.
 */
bool WaypointMovementGenerator<Creature>::CanMove(int32 diff, Creature& u)
{
    i_nextMoveTime.Update(diff);
    if (i_nextMoveTime.Passed() && u.hasUnitState(UNIT_STAT_WAYPOINT_PAUSED))
    {
        i_nextMoveTime.Reset(1);
    }

    return i_nextMoveTime.Passed() && !u.hasUnitState(UNIT_STAT_WAYPOINT_PAUSED);
}

/**
 * @brief Gets the reset position for the creature.
 * @param x Reference to the X-coordinate.
 * @param y Reference to the Y-coordinate.
 * @param z Reference to the Z-coordinate.
 * @param o Reference to the orientation.
 * @return True if the reset position was successfully obtained, false otherwise.
 */
bool WaypointMovementGenerator<Creature>::GetResetPosition(Creature& creature, float& x, float& y, float& z, float& o) const
{
    // Prevent a crash at empty waypoint path.
    if (!i_path || i_path->empty())
    {
        return false;
    }

    // Prefer resuming from the point where combat pulled the creature off
    // its path (its departure point) rather than the last reached waypoint.
    // This removes the visible backtrack where an evading patroller runs
    // back to the previous waypoint and then re-walks the leg it was on.
    // The combat anchor is captured for every creature at the start of a new
    // battle (Unit::Attack), so a creature that fought and is now evading
    // holds the on-path position it had when it engaged. A zero sentinel
    // means no combat start was recorded; in that case fall back below.
    const Geometry::Vector3& combat = creature.CombatAnchor();
    if (combat.x != 0.0f || combat.y != 0.0f || combat.z != 0.0f)
    {
        x = combat.x;
        y = combat.y;
        z = combat.z;

        // Face toward the waypoint the creature was heading for, so it keeps
        // moving forward on resume; fall back to current facing if that point
        // is unavailable or coincident.
        WaypointPath::const_iterator nextPoint = i_path->find(i_currentNode);
        if (nextPoint != i_path->end())
        {
            float dx = nextPoint->second.x - x;
            float dy = nextPoint->second.y - y;
            if (dx != 0.0f || dy != 0.0f)
            {
                o = atan2(dy, dx);
                o = (o >= 0) ? o : 2 * M_PI_F + o;
            }
            else
            {
                o = creature.Where().Facing();
            }
        }
        else
        {
            o = creature.Where().Facing();
        }

        return true;
    }

    WaypointPath::const_iterator lastPoint = i_path->find(m_lastReachedWaypoint);
    // Special case: Before the first waypoint is reached, m_lastReachedWaypoint is set to 0 (which may not be contained in i_path)
    if (!m_lastReachedWaypoint && lastPoint == i_path->end())
    {
        return false;
    }

    MANGOS_ASSERT(lastPoint != i_path->end());

    WaypointNode const* curWP = &(lastPoint->second);

    x = curWP->x;
    y = curWP->y;
    z = curWP->z;

    if (curWP->orientation != 100)
    {
        o = curWP->orientation;
    }
    else                                                    // Calculate the resulting angle based on positions between previous and current waypoint
    {
        WaypointNode const* prevWP;
        if (lastPoint != i_path->begin())                   // Not the first waypoint
        {
            --lastPoint;
            prevWP = &(lastPoint->second);
        }
        else                                                // Take the last waypoint (crbegin()) as previous
        {
            prevWP = &(i_path->rbegin()->second);
        }

        float dx = x - prevWP->x;
        float dy = y - prevWP->y;
        o = atan2(dy, dx);                                  // returns value between -Pi..Pi

        o = (o >= 0) ? o : 2 * M_PI_F + o;
    }

    return true;
}

/**
 * @brief Gets the waypoint path information.
 * @param oss Output stream to store the waypoint path information.
 */
void WaypointMovementGenerator<Creature>::GetPathInformation(std::ostringstream& oss) const
{
    oss << "WaypointMovement: Last Reached WP: " << m_lastReachedWaypoint << " ";
    oss << "(Loaded path " << m_pathId << " from " << WaypointManager::GetOriginString(m_PathOrigin) << ")\n";
}

/**
 * @brief Adds time to the waypoint pause time.
 * @param waitTimeDiff Time difference to add.
 */
void WaypointMovementGenerator<Creature>::AddToWaypointPauseTime(int32 waitTimeDiff)
{
    i_nextMoveTime.Update(waitTimeDiff);
    if (i_nextMoveTime.Passed())
    {
        i_nextMoveTime.Reset(0);
        return;
    }
}

/**
 * @brief Sets the next waypoint for the creature.
 * @param pointId ID of the next waypoint.
 * @return True if the next waypoint was successfully set, false otherwise.
 */
bool WaypointMovementGenerator<Creature>::SetNextWaypoint(uint32 pointId)
{
    if (!i_path || i_path->empty())
    {
        return false;
    }

    WaypointPath::const_iterator currPoint = i_path->find(pointId);
    if (currPoint == i_path->end())
    {
        return false;
    }

    // Allow Moving with next tick
    // Handle allow movement this way to not interact with PAUSED state.
    // If this function is called while PAUSED, it will move properly when unpaused.
    i_nextMoveTime.Reset(1);
    m_isArrivalDone = false;
    ClearActiveSegment();

    // Set the point
    i_currentNode = pointId;
    return true;
}

//----------------------------------------------------//
/**
 * @brief Gets the path at the end of the map.
 * @return The path at the end of the map.
 */
uint32 FlightPathMovementGenerator::GetPathAtMapEnd() const
{
    if (i_currentNode >= i_path->size())
    {
        return i_path->size();
    }

    uint32 curMapId = (*i_path)[i_currentNode].ContinentID;

    for (uint32 i = i_currentNode; i < i_path->size(); ++i)
    {
        if ((*i_path)[i].ContinentID != curMapId)
        {
            return i;
        }
    }

    return i_path->size();
}

/**
 * @brief Initializes the FlightPathMovementGenerator.
 * @param player Reference to the player.
 */
void FlightPathMovementGenerator::Initialize(Player& player)
{
    Reset(player);
}

/**
 * @brief Finalizes the FlightPathMovementGenerator.
 * @param player Reference to the player.
 */
void FlightPathMovementGenerator::Finalize(Player& player)
{
    // Remove flag to prevent send object build movement packets for flight state and crash (movement generator already not at top of stack)
    player.clearUnitState(UNIT_STAT_TAXI_FLIGHT);

    if (!m_preserveTaxiRouteOnFinalize)
    {
        player.m_taxi.ClearTaxiDestinations();
    }
    player.Unmount();
    player.RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_TAXI_FLIGHT);
    player.SetClientControl(&player, 1);

    if (player.m_taxi.empty())
    {
        player.GetHostileRefManager().setOnlineOfflineState(true);
        if (player.pvpInfo.inHostileArea)
        {
            player.CastSpell(&player, 2479, true);
        }

        // Update z position to ground and orientation for landing point
        // This prevent cheating with landing  point at lags
        // When client side flight end early in comparison server side
        player.StopMoving(true);
    }
}

/**
 * @brief Interrupts the FlightPathMovementGenerator.
 * @param player Reference to the player.
 */
void FlightPathMovementGenerator::Interrupt(Player& player)
{
    player.clearUnitState(UNIT_STAT_TAXI_FLIGHT);
}

#define PLAYER_FLIGHT_SPEED        32.0f

/**
 * @brief Resets the FlightPathMovementGenerator.
 * @param player Reference to the player.
 */
void FlightPathMovementGenerator::Reset(Player& player)
{
    // Set the player to offline state for hostile references
    player.GetHostileRefManager().setOnlineOfflineState(false);
    // Add the taxi flight state to the player
    player.addUnitState(UNIT_STAT_TAXI_FLIGHT);

    // Set the client control lost and taxi flight flags
    player.SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_TAXI_FLIGHT);
    player.SetClientControl(&player, 0);

    // Initialize the movement spline for the player
    Movement::MoveSplineInit init(player);
    uint32 const splineStart = GetCurrentNode();
    uint32 end = GetPathAtMapEnd();
    if (splineStart >= end || end - splineStart < 2)
    {
        m_splineLaunched = false;
        return;
    }

    // MoveSplineInit replaces its first path vertex with the owner's current
    // position. Keep that control point separate so the first authored taxi
    // node is not silently discarded from a combined multi-leg route.
    if (m_continuousRoute)
    {
        init.Path().push_back(G3D::Vector3(player.Where().X(),
            player.Where().Y(), player.Where().Z()));
    }

    for (uint32 i = splineStart; i != end; ++i)
    {
        G3D::Vector3 vertice((*i_path)[i].x, (*i_path)[i].y, (*i_path)[i].z);
        init.Path().push_back(vertice);
    }
    init.SetFirstPointId(splineStart);
    init.SetFly();
    init.SetSmooth();
    init.SetWalk(true);
    init.SetVelocity(PLAYER_FLIGHT_SPEED);
    m_splineLaunched = init.Launch() > 0;
}

/**
 * @brief Updates the FlightPathMovementGenerator.
 * @param player Reference to the player.
 * @param diff Time difference.
 * @return True if the update was successful, false otherwise.
 */
bool FlightPathMovementGenerator::Update(Player& player, const uint32& /*diff*/)
{
    int32 splinePointId = player.movespline->currentPathIdx();
    if (m_continuousRoute && splinePointId > 0)
    {
        --splinePointId;
    }
    uint32 pointId = splinePointId > 0 ? uint32(splinePointId) : 0;
    if (pointId > i_currentNode)
    {
        bool departureEvent = true;
        do
        {
            DoEventIfAny(player, (*i_path)[i_currentNode], departureEvent);
            if (pointId == i_currentNode)
            {
                break;
            }
            if (departureEvent)
            {
                uint32 const previousPathId = (*i_path)[i_currentNode].PathID;
                ++i_currentNode;
                if (m_continuousRoute && i_currentNode < i_path->size() &&
                    (*i_path)[i_currentNode].PathID != previousPathId &&
                    player.m_taxi.HasNextTaxiDestination())
                {
                    player.m_taxi.NextTaxiDestination();
                }
            }
            departureEvent = !departureEvent;
        }
        while (true);
    }

    if (!m_continuousRoute && i_currentNode >= (i_path->size() - 1) &&
        player.m_taxi.HasNextTaxiDestination())
    {
        if (!player.movespline->Finalized())
        {
            return true;
        }

        if (player.m_taxi.GetFlightLedger().MarkServerEndpoint(player.GetMapId(),
                player.m_taxi.GetCurrentTaxiPath(), i_currentNode,
                player.movespline->GetId()))
        {
            m_preserveTaxiRouteOnFinalize = true;
            return false;
        }
    }

    return i_currentNode < (i_path->size() - 1);
}

/**
 * @brief Sets the current node after teleporting the player.
 */
void FlightPathMovementGenerator::SetCurrentNodeAfterTeleport()
{
    if (i_path->empty() || i_currentNode >= i_path->size())
    {
        return;
    }

    uint32 const currentMap = (*i_path)[i_currentNode].ContinentID;

    for (size_t i = i_currentNode + 1; i < i_path->size(); ++i)
    {
        if ((*i_path)[i].ContinentID != currentMap)
        {
            i_currentNode = i;
            return;
        }
    }
}

void FlightPathMovementGenerator::DoEventIfAny(Player& player, TaxiPathNodeEntry const& node, bool departure)
{
    if (uint32 eventid = departure ? node.DepartureEventID : node.ArrivalEventID)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Taxi %s event %u of node %u of path %u for player %s", departure ? "departure" : "arrival", eventid, node.NodeIndex, node.PathID, player.GetName());
        StartEvents_Event(player.GetMap(), eventid, &player, &player, departure);
    }
}

/**
 * @brief Gets the reset position for the player.
 * @param player Reference to the player.
 * @param x Reference to the X-coordinate.
 * @param y Reference to the Y-coordinate.
 * @param z Reference to the Z-coordinate.
 * @param o Reference to the orientation.
 * @return True if the reset position was successfully obtained, false otherwise.
 */
bool FlightPathMovementGenerator::GetResetPosition(Player& /*player*/, float& x, float& y, float& z, float& o) const
{
    const TaxiPathNodeEntry& node = (*i_path)[i_currentNode];
    x = node.x;
    y = node.y;
    z = node.z;

    return true;
}
