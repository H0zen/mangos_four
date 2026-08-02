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

#ifndef PLAYERTAXI_H
#define PLAYERTAXI_H

#include "Common.h"
#include "DBCEnums.h"
#include "DBCStructure.h"
#include "SharedDefines.h"

class ByteBuffer;
struct FactionTemplateEntry;

class Player; // forward declaration
class PlayerTaxi;

namespace TaxiPersistence
{
    class Validator
    {
        public:
            virtual ~Validator() = default;
            virtual bool HasTaxiPath(uint32 source, uint32 destination) = 0;
            virtual bool HasTaxiMount(uint32 source, Team team) = 0;
    };

    bool LoadTaxiDestinations(PlayerTaxi& taxi, std::string const& values,
        Team team, Validator& validator);
}

enum class TaxiFlightPhase : uint8
{
    Inactive,
    InFlight,
    AwaitingCompletion,
    Consumed,
    Finalized
};

class TaxiFlightLedger
{
    public:
        void Arm(uint32 mapId, uint32 pathId, uint32 endNode,
            uint32 sourceNode, uint32 destinationNode, uint32 splineId)
        {
            m_mapId = mapId;
            m_pathId = pathId;
            m_endNode = endNode;
            m_sourceNode = sourceNode;
            m_destinationNode = destinationNode;
            m_splineId = splineId;
            m_phase = TaxiFlightPhase::InFlight;
        }

        bool MarkServerEndpoint(uint32 mapId, uint32 pathId,
            uint32 serverPathIndex, uint32 splineId)
        {
            if (m_phase == TaxiFlightPhase::AwaitingCompletion)
            {
                return mapId == m_mapId && pathId == m_pathId &&
                    serverPathIndex == m_endNode && splineId == m_splineId;
            }

            if (m_phase != TaxiFlightPhase::InFlight || mapId != m_mapId ||
                pathId != m_pathId || serverPathIndex != m_endNode ||
                splineId != m_splineId)
            {
                return false;
            }

            m_phase = TaxiFlightPhase::AwaitingCompletion;
            return true;
        }

        bool TryConsumeCompletion(uint32 mapId, uint32 pathId,
            uint32 sourceNode, uint32 destinationNode, uint32 splineId,
            uint32 serverPathIndex)
        {
            if (m_phase != TaxiFlightPhase::AwaitingCompletion ||
                mapId != m_mapId || pathId != m_pathId ||
                sourceNode != m_sourceNode ||
                destinationNode != m_destinationNode ||
                splineId != m_splineId || serverPathIndex != m_endNode)
            {
                return false;
            }

            m_phase = TaxiFlightPhase::Consumed;
            return true;
        }

        void Finalize() { m_phase = TaxiFlightPhase::Finalized; }
        TaxiFlightPhase GetPhase() const { return m_phase; }
        uint32 GetEndNode() const { return m_endNode; }

    private:
        uint32 m_mapId = 0;
        uint32 m_pathId = 0;
        uint32 m_endNode = 0;
        uint32 m_sourceNode = 0;
        uint32 m_destinationNode = 0;
        uint32 m_splineId = 0;
        TaxiFlightPhase m_phase = TaxiFlightPhase::Inactive;
};

class PlayerTaxi
{
    public:
        PlayerTaxi() : m_flightMasterFactionId(0) { memset(m_taximask, 0, sizeof(m_taximask)); }
        ~PlayerTaxi() { }

        // Nodes
        void InitTaxiNodes(uint32 race, uint32 chrClass, uint8 level);
        void InitTaxiNodesForClass(uint32 chrClass);
        void InitTaxiNodesForRace(uint32 race);
        void InitTaxiNodesForFaction(uint32 faction);
        void InitTaxiNodesForLvl(uint8 level);

        void LoadTaxiMask(const char* data);

        bool IsValidNodeId(uint32 nodeidx) const
        {
            TaxiMaskPosition position = {};
            return GetTaxiMaskPosition(nodeidx, position);
        }

        bool IsTaximaskNodeKnown(uint32 nodeidx) const
        {
            if (!IsValidNodeId(nodeidx))
            {
                return false;
            }

            TaxiMaskPosition position = {};
            GetTaxiMaskPosition(nodeidx, position);
            return (m_taximask[position.byteIndex] & position.bitMask) == position.bitMask;
        }

        bool SetTaximaskNode(uint32 nodeidx)
        {
            if (!IsValidNodeId(nodeidx))
            {
                return false;
            }

            TaxiMaskPosition position = {};
            GetTaxiMaskPosition(nodeidx, position);
            if ((m_taximask[position.byteIndex] & position.bitMask) != position.bitMask)
            {
                m_taximask[position.byteIndex] |= position.bitMask;
                return true;
            }
            else
            {
                return false;
            }
        }

        void AppendTaximaskTo(ByteBuffer& data, bool all);
        uint8 const* GetTaxiMask(bool all) const;
        size_t GetTaxiMaskSize() const;

        // Destinations
        bool LoadTaxiDestinationsFromString(const std::string& values, Team team);
        std::string SaveTaxiDestinationsToString();

        void ClearTaxiDestinations()
        {
            m_TaxiDestinations.clear();
            m_flightLedger.Finalize();
        }

        TaxiFlightLedger& GetFlightLedger() { return m_flightLedger; }
        TaxiFlightLedger const& GetFlightLedger() const { return m_flightLedger; }

        void AddTaxiDestination(uint32 dest)
        {
            m_TaxiDestinations.push_back(dest);
        }

        uint32 GetTaxiSource() const
        {
            return m_TaxiDestinations.empty() ? 0 : m_TaxiDestinations.front();
        }

        uint32 GetTaxiDestination() const
        {
            return m_TaxiDestinations.size() < 2 ? 0 : m_TaxiDestinations[1];
        }

        uint32 GetCurrentTaxiPath() const;
        bool BuildSameMapTaxiPath(TaxiPathNodeList& path, uint32 mapId) const;

        uint32 NextTaxiDestination()
        {
            m_TaxiDestinations.pop_front();
            return GetTaxiDestination();
        }

        bool empty() const
        {
            return m_TaxiDestinations.empty();
        }

        bool HasNextTaxiDestination() const
        {
            return m_TaxiDestinations.size() > 2;
        }

        void TruncateTaxiDestinationsAfterCurrentLeg()
        {
            while (m_TaxiDestinations.size() > 2)
            {
                m_TaxiDestinations.pop_back();
            }
        }

        FactionTemplateEntry const* GetFlightMasterFactionTemplate() const;
        void SetFlightMasterFactionTemplateId(uint32 factionTemplateId)
        {
            m_flightMasterFactionId = factionTemplateId;
        }

        friend std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi);

    private:
        TaxiFlightLedger m_flightLedger;
        TaxiMask m_taximask;
        std::deque<uint32> m_TaxiDestinations;
        uint32 m_flightMasterFactionId;
};

std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi);

#endif
