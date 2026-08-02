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
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "UpdateMask.h"
#include "Path.h"
#include "WaypointMovementGenerator.h"
#include "movement/MoveSpline.h"

/**
 * @brief Handles a client request for the known status of a taxi node.
 *
 * @param recv_data The incoming taxi node status packet.
 */
void WorldSession::HandleTaxiNodeStatusQueryOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_TAXINODE_STATUS_QUERY");

    ObjectGuid guid;
    if (!MopTaxiPackets::ParseStatusQuery(recv_data, guid))
    {
        return;
    }

    SendTaxiStatus(guid);
}

/**
 * @brief Sends whether the nearest taxi node for a flight master is known to the player.
 *
 * @param guid The flight master guid.
 */
void WorldSession::SendTaxiStatus(ObjectGuid guid)
{
    // cheating checks
    Creature* unit = GetPlayer()->GetMap()->GetCreature(guid);
    if (!unit)
    {
        DEBUG_LOG("WorldSession::SendTaxiStatus - %s not found or you can't interact with it.", guid.GetString().c_str());
        return;
    }

    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    // not found nearest
    if (curloc == 0)
    {
        return;
    }

    DEBUG_LOG("WORLD: current location %u ", curloc);

    WorldPacket data(SMSG_TAXINODE_STATUS, 10);
    MopTaxiPackets::BuildStatusBody(data, guid,
        MopTaxiPackets::StatusForKnown(
            GetPlayer()->m_taxi.IsTaximaskNodeKnown(curloc)));
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_TAXINODE_STATUS");
}

/**
 * @brief Handles a request to open a flight master's available taxi menu.
 *
 * @param recv_data The incoming taxi query packet.
 */
void WorldSession::HandleTaxiQueryAvailableNodes(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_TAXIQUERYAVAILABLENODES");

    ObjectGuid guid;
    if (!MopTaxiPackets::ParseTaxiQueryAvailableNodes(recv_data, guid) || guid.IsEmpty())
    {
        return;
    }

    // cheating checks
    Creature* unit = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!unit)
    {
        DEBUG_LOG("WORLD: HandleTaxiQueryAvailableNodes - %s not found or you can't interact with him.", guid.GetString().c_str());
        return;
    }

    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // unknown taxi node case
    if (SendLearnNewTaxiNode(unit))
    {
        return;
    }

    // known taxi node case
    SendTaxiMenu(unit);
}

/**
 * @brief Sends the taxi route selection menu for a flight master.
 *
 * @param unit The flight master creature.
 */
void WorldSession::SendTaxiMenu(Creature* unit)
{
    // find current node
    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    if (curloc == 0)
    {
        return;
    }

    DEBUG_LOG("WORLD: CMSG_TAXINODE_STATUS_QUERY %u ", curloc);

    size_t const maskSize = GetPlayer()->m_taxi.GetTaxiMaskSize();
    WorldPacket data(SMSG_SHOWTAXINODES, 17 + maskSize);
    MopTaxiPackets::BuildShowTaxiNodes(data, unit->GetObjectGuid(), curloc,
        GetPlayer()->m_taxi.GetTaxiMask(GetPlayer()->IsTaxiCheater()), maskSize);
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_SHOWTAXINODES");
}

/**
 * @brief Starts taxi flight movement for the player.
 *
 * @param mountDisplayId The taxi mount display id.
 * @param path The taxi path id.
 * @param pathNode The starting node index.
 */
bool WorldSession::SendDoFlight(uint32 mountDisplayId, uint32 path,
    uint32 pathNode, bool preserveTaxiRoute)
{
    // remove fake death
    if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    if (preserveTaxiRoute &&
        GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        static_cast<FlightPathMovementGenerator*>(
            GetPlayer()->GetMotionMaster()->top())->PrepareForRollover();
    }

    while (GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
        GetPlayer()->GetMotionMaster()->MovementExpired(false);

    if (mountDisplayId)
    {
        GetPlayer()->Mount(mountDisplayId);
    }

    if (!GetPlayer()->GetMotionMaster()->MoveTaxiFlight(path, pathNode))
    {
        if (preserveTaxiRoute)
        {
            GetPlayer()->m_taxi.ClearTaxiDestinations();
        }
        if (mountDisplayId)
        {
            GetPlayer()->Unmount();
        }
        return false;
    }

    FlightPathMovementGenerator* flight = static_cast<FlightPathMovementGenerator*>(
        GetPlayer()->GetMotionMaster()->top());
    uint32 const sourceNode = GetPlayer()->m_taxi.GetTaxiSource();
    uint32 const destinationNode = GetPlayer()->m_taxi.GetTaxiDestination();
    uint32 const mapEnd = flight->GetPathAtMapEnd();
    if (!flight->IsContinuousRoute() && sourceNode && destinationNode && mapEnd > pathNode &&
        MopTaxiPackets::IsSameMapTaxiPath(flight->GetPath(), GetPlayer()->GetMapId()))
    {
        GetPlayer()->m_taxi.GetFlightLedger().Arm(GetPlayer()->GetMapId(), path,
            mapEnd - 1, sourceNode, destinationNode,
            GetPlayer()->movespline->GetId());
    }

    return true;
}

/**
 * @brief Learns a newly discovered taxi node and notifies the client.
 *
 * @param unit The flight master creature.
 * @return true if the node was newly learned or no valid node existed; otherwise false.
 */
bool WorldSession::SendLearnNewTaxiNode(Creature* unit)
{
    // find current node
    uint32 curloc = sObjectMgr.GetNearestTaxiNode(unit->GetPositionX(), unit->GetPositionY(), unit->GetPositionZ(), unit->GetMapId(), GetPlayer()->GetTeam());

    if (curloc == 0)
    {
        return true;                                        // `true` send to avoid WorldSession::SendTaxiMenu call with one more curlock seartch with same false result.
    }

    if (GetPlayer()->m_taxi.SetTaximaskNode(curloc))
    {
        WorldPacket msg(SMSG_NEW_TAXI_PATH, 0);
        SendPacket(&msg);

        WorldPacket update(SMSG_TAXINODE_STATUS, 10);
        MopTaxiPackets::BuildStatusBody(update, unit->GetObjectGuid(),
            MopTaxiPackets::TaxiNodeStatus::Learned);
        SendPacket(&update);

        return true;
    }
    else
    {
        return false;
    }
}

/**
 * @brief Sends the result of a taxi activation attempt.
 *
 * @param reply The taxi activation status.
 */
void WorldSession::SendActivateTaxiReply(ActivateTaxiReply reply)
{
    WorldPacket data(SMSG_ACTIVATETAXIREPLY, 1);
    MopTaxiPackets::BuildActivateTaxiReply(data, reply);
    SendPacket(&data);

    DEBUG_LOG("WORLD: Sent SMSG_ACTIVATETAXIREPLY");
}

/**
 * @brief Handles a multi-node taxi activation request.
 *
 * @param recv_data The incoming taxi express packet.
 */
void WorldSession::HandleActivateTaxiExpressOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXIEXPRESS");

    MopTaxiPackets::TaxiExpressRequest request;
    if (!MopTaxiPackets::ParseActivateTaxiExpress(recv_data, request))
    {
        return;
    }

    Creature* npc = GetPlayer()->GetNPCIfCanInteractWith(
        request.flightMaster, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        DEBUG_LOG("WORLD: HandleActivateTaxiExpressOpcode - %s not found or you can't interact with it.",
            request.flightMaster.GetString().c_str());
        return;
    }

    uint32 const currentNode = sObjectMgr.GetNearestTaxiNode(
        npc->GetPositionX(), npc->GetPositionY(), npc->GetPositionZ(),
        npc->GetMapId(), GetPlayer()->GetTeam());
    if (currentNode == 0 || request.nodes.front() != currentNode)
    {
        SendActivateTaxiReply(ERR_TAXITOOFARAWAY);
        return;
    }

    for (uint32 node : request.nodes)
    {
        TaxiNodesEntry const* nodeEntry = sTaxiNodesStore.LookupEntry(node);
        if (!GetPlayer()->m_taxi.IsValidNodeId(node) || !nodeEntry ||
            nodeEntry->ContinentID != npc->GetMapId())
        {
            SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
            return;
        }

        if (!_player->IsTaxiCheater() &&
            !_player->m_taxi.IsTaximaskNodeKnown(node))
        {
            SendActivateTaxiReply(ERR_TAXINOTVISITED);
            return;
        }
    }

    for (size_t i = 1; i < request.nodes.size(); ++i)
    {
        uint32 path = 0;
        uint32 cost = 0;
        sObjectMgr.GetTaxiPath(request.nodes[i - 1], request.nodes[i], path, cost);
        if (!path || path >= sTaxiPathNodesByPath.size() ||
            !MopTaxiPackets::IsSameMapTaxiPath(sTaxiPathNodesByPath[path], npc->GetMapId()))
        {
            SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
            return;
        }
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXIEXPRESS from %u to %u",
        request.nodes.front(), request.nodes.back());

    GetPlayer()->ActivateTaxiPathTo(request.nodes, npc);
}

/**
 * @brief Handles authenticated taxi completion, including map handoffs and chained destinations.
 *
 * @param recv_data The incoming move-spline-done packet.
 */
void WorldSession::HandleMoveSplineDoneOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_MOVE_SPLINE_DONE");

    MopTaxiPackets::MoveSplineDoneRequest request;
    if (!MopTaxiPackets::ParseMoveSplineDone(recv_data, request))
    {
        DEBUG_LOG("WORLD: Rejected CMSG_MOVE_SPLINE_DONE: malformed body");
        return;
    }

    if (!MopTaxiPackets::MatchesMoveSplinePlayer(request.movement.GetGuid(),
            GetPlayer()->GetObjectGuid()))
    {
        DEBUG_LOG("WORLD: Rejected CMSG_MOVE_SPLINE_DONE: mover %s does not match player %s",
            request.movement.GetGuid().GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
        return;
    }

    // Continuous routes are finalized by the server movement update. The
    // matching client acknowledgement can arrive immediately afterwards,
    // when the completed taxi queue has already been cleared.
    if (!GetPlayer()->m_taxi.GetTaxiDestination())
    {
        return;
    }

    if (!GetPlayer()->movespline->Finalized())
    {
        DEBUG_LOG("WORLD: Rejected CMSG_MOVE_SPLINE_DONE: spline is not finalized");
        return;
    }

    if (request.splineId != GetPlayer()->movespline->GetId())
    {
        DEBUG_LOG("WORLD: Rejected CMSG_MOVE_SPLINE_DONE: clientSpline=%u serverSpline=%u",
            request.splineId, GetPlayer()->movespline->GetId());
        return;
    }

    int32 const serverPathIndex = GetPlayer()->movespline->currentPathIdx();
    if (GetPlayer()->m_taxi.GetTaxiDestination() &&
        GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        FlightPathMovementGenerator* flight = static_cast<FlightPathMovementGenerator*>(
            GetPlayer()->GetMotionMaster()->top());
        uint32 const mapEnd = flight->GetPathAtMapEnd();
        TaxiPathNodeList const& path = flight->GetPath();
        if (mapEnd < path.size() && path[mapEnd].ContinentID != GetPlayer()->GetMapId())
        {
            if (mapEnd == 0 || serverPathIndex < 0 || uint32(serverPathIndex) != mapEnd - 1)
            {
                DEBUG_LOG("WORLD: Rejected CMSG_MOVE_SPLINE_DONE at taxi map boundary: "
                    "pathIndex=%d expected=%u", serverPathIndex, mapEnd ? mapEnd - 1 : 0);
                return;
            }

            TaxiPathNodeEntry const& transitionNode = path[mapEnd];
            flight->Interrupt(*GetPlayer());
            flight->SetCurrentNodeAfterTeleport();
            if (!GetPlayer()->TeleportTo(transitionNode.ContinentID,
                    transitionNode.x, transitionNode.y, transitionNode.z,
                    GetPlayer()->GetOrientation()))
            {
                GetPlayer()->m_taxi.ClearTaxiDestinations();
                GetPlayer()->GetMotionMaster()->MovementExpired(false);
            }
            return;
        }

        // Continuous same-map routes complete on the server movement update.
        // The authenticated client packet is only an acknowledgement; no
        // per-leg queue rollover or ledger consumption is required here.
        if (flight->IsContinuousRoute())
        {
            return;
        }
    }

    TaxiFlightLedger& flightLedger = GetPlayer()->m_taxi.GetFlightLedger();
    if (serverPathIndex < 0 ||
        !flightLedger.MarkServerEndpoint(GetPlayer()->GetMapId(),
            GetPlayer()->m_taxi.GetCurrentTaxiPath(), uint32(serverPathIndex),
            request.splineId) ||
        !flightLedger.TryConsumeCompletion(
            GetPlayer()->GetMapId(), GetPlayer()->m_taxi.GetCurrentTaxiPath(),
            GetPlayer()->m_taxi.GetTaxiSource(),
            GetPlayer()->m_taxi.GetTaxiDestination(), request.splineId,
            uint32(serverPathIndex)))
    {
        DEBUG_LOG("WORLD: Rejected CMSG_MOVE_SPLINE_DONE: clientSpline=%u serverSpline=%u "
            "pathIndex=%d ledgerPhase=%u ledgerEnd=%u",
            request.splineId, GetPlayer()->movespline->GetId(), serverPathIndex,
            uint32(flightLedger.GetPhase()), flightLedger.GetEndNode());
        return;
    }

    uint32 destinationnode = GetPlayer()->m_taxi.NextTaxiDestination();
    if (destinationnode > 0)                                // if more destinations to go
    {
        // current source node for next destination
        uint32 sourcenode = GetPlayer()->m_taxi.GetTaxiSource();

        // Add to taximask middle hubs in taxicheat mode (to prevent having player with disabled taxicheat and not having back flight path)
        if (GetPlayer()->IsTaxiCheater())
        {
            if (GetPlayer()->m_taxi.SetTaximaskNode(sourcenode))
            {
                WorldPacket data(SMSG_NEW_TAXI_PATH, 0);
                _player->GetSession()->SendPacket(&data);
            }
        }

        DEBUG_LOG("WORLD: Taxi has to go from %u to %u", sourcenode, destinationnode);

        uint32 mountDisplayId = sObjectMgr.GetTaxiMountDisplayId(sourcenode, GetPlayer()->GetTeam());

        uint32 path, cost;
        sObjectMgr.GetTaxiPath(sourcenode, destinationnode, path, cost);

        if (path && mountDisplayId && path < sTaxiPathNodesByPath.size() &&
            MopTaxiPackets::IsSameMapTaxiPath(sTaxiPathNodesByPath[path],
                GetPlayer()->GetMapId()))
        {
            uint32 const pathNode = sTaxiPathNodesByPath[path].size() > 2 ? 1 : 0;
            SendDoFlight(mountDisplayId, path, pathNode, true);
        }
        else
        {
            if (GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
            {
                GetPlayer()->GetMotionMaster()->MovementExpired(false);
            }
            else
            {
                GetPlayer()->m_taxi.ClearTaxiDestinations();
            }
        }
    }
    else
    {
        if (GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
        {
            GetPlayer()->GetMotionMaster()->MovementExpired(false);
        }
        else
        {
            GetPlayer()->m_taxi.ClearTaxiDestinations();
        }
    }
}

/**
 * @brief Handles a standard two-node taxi activation request.
 *
 * @param recv_data The incoming taxi activation packet.
 */
void WorldSession::HandleActivateTaxiOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXI");

    MopTaxiPackets::TaxiActivationRequest request;
    if (!MopTaxiPackets::ParseActivateTaxi(recv_data, request))
    {
        return;
    }

    DEBUG_LOG("WORLD: Received opcode CMSG_ACTIVATETAXI from %u to %u",
        request.sourceNode, request.destinationNode);
    Creature* npc = GetPlayer()->GetNPCIfCanInteractWith(
        request.flightMaster, UNIT_NPC_FLAG_FLIGHTMASTER);
    if (!npc)
    {
        DEBUG_LOG("WORLD: HandleActivateTaxiOpcode - %s not found or you can't interact with it.",
            request.flightMaster.GetString().c_str());
        return;
    }

    uint32 currentNode = sObjectMgr.GetNearestTaxiNode(
        npc->GetPositionX(), npc->GetPositionY(), npc->GetPositionZ(),
        npc->GetMapId(), GetPlayer()->GetTeam());
    if (currentNode == 0 || request.sourceNode != currentNode)
    {
        SendActivateTaxiReply(ERR_TAXITOOFARAWAY);
        return;
    }

    if (!GetPlayer()->m_taxi.IsValidNodeId(currentNode) ||
        !GetPlayer()->m_taxi.IsValidNodeId(request.destinationNode))
    {
        SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
        return;
    }

    TaxiNodesEntry const* currentNodeEntry = sTaxiNodesStore.LookupEntry(currentNode);
    TaxiNodesEntry const* destinationNodeEntry =
        sTaxiNodesStore.LookupEntry(request.destinationNode);
    uint32 path = 0;
    uint32 cost = 0;
    sObjectMgr.GetTaxiPath(currentNode, request.destinationNode, path, cost);
    if (currentNode == request.destinationNode ||
        !currentNodeEntry || !destinationNodeEntry ||
        currentNodeEntry->ContinentID != npc->GetMapId() ||
        destinationNodeEntry->ContinentID != npc->GetMapId() || !path ||
        path >= sTaxiPathNodesByPath.size() ||
        !MopTaxiPackets::IsSameMapTaxiPath(
            sTaxiPathNodesByPath[path], npc->GetMapId()))
    {
        SendActivateTaxiReply(ERR_TAXINOSUCHPATH);
        return;
    }

    if (!_player->IsTaxiCheater())
    {
        if (!_player->m_taxi.IsTaximaskNodeKnown(currentNode) ||
            !_player->m_taxi.IsTaximaskNodeKnown(request.destinationNode))
        {
            SendActivateTaxiReply(ERR_TAXINOTVISITED);
            return;
        }
    }

    std::vector<uint32> nodes = { currentNode, request.destinationNode };
    GetPlayer()->ActivateTaxiPathTo(nodes, npc);
}
