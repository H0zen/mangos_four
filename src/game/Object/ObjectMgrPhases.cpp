/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
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

#include "ObjectMgr.h"

#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "SQLStorages.h"

namespace
{
char const* GetPhaseDefinitionValidationReason(PhaseDefinitionValidationResult result)
{
    switch (result)
    {
        case PhaseDefinitionValidationResult::ZoneIdZero:
            return "zoneId is zero";
        case PhaseDefinitionValidationResult::EntryZero:
            return "entry is zero";
        case PhaseDefinitionValidationResult::PhaseMaskZero:
            return "phasemask is zero";
        case PhaseDefinitionValidationResult::PhaseMaskOutOfRange:
            return "phasemask exceeds UINT16_MAX";
        case PhaseDefinitionValidationResult::ConditionIdOutOfRange:
            return "condition_id exceeds UINT16_MAX";
        case PhaseDefinitionValidationResult::ConditionMissing:
            return "condition_id does not exist";
        case PhaseDefinitionValidationResult::PhaseIdNonZero:
            return "phaseId is not supported";
        case PhaseDefinitionValidationResult::TerrainSwapMapNonZero:
            return "terrainswapmap is not supported";
        case PhaseDefinitionValidationResult::FlagsNonZero:
            return "flags are not supported";
        case PhaseDefinitionValidationResult::Success:
            break;
    }

    return "unknown validation result";
}
}

void ObjectMgr::LoadPhaseDefinitions()
{
    PhaseDefinitionStore phaseDefinitionStore;
    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `zoneId`, `entry`, `phasemask`, `phaseId`, `terrainswapmap`, `flags`, `condition_id` FROM `phase_definitions` ORDER BY `zoneId`, `entry`");

    if (!result)
    {
        sLog.outString(">> Loaded 0 phase definitions");
    }
    else
    {
        do
        {
            Field* fields = result->Fetch();

            PhaseDefinitionRecord record = {};
            record.zoneId = fields[0].GetUInt32();
            record.entry = fields[1].GetUInt16();
            record.phaseMask = fields[2].GetUInt64();
            record.phaseId = fields[3].GetUInt8();
            record.terrainSwapMap = fields[4].GetUInt16();
            record.flags = fields[5].GetUInt8();
            record.conditionId = fields[6].GetUInt32();

            bool conditionExists = true;
            if (record.conditionId != 0 && record.conditionId <= UINT16_MAX)
                conditionExists = sConditionStorage.LookupEntry<PlayerCondition>(record.conditionId) != NULL;

            PhaseDefinition definition = {};
            PhaseDefinitionValidationResult validation = ValidatePhaseDefinition(record, conditionExists, definition);
            if (validation != PhaseDefinitionValidationResult::Success)
            {
                sLog.outErrorDb("Table `phase_definitions` entry %u, zone %u, mask " UI64FMTD ", condition_id %u rejected: %s",
                    record.entry, record.zoneId, record.phaseMask, record.conditionId, GetPhaseDefinitionValidationReason(validation));
                continue;
            }

            phaseDefinitionStore[definition.zoneId].push_back(definition);
            ++count;
        }
        while (result->NextRow());

        delete result;
        sLog.outString(">> Loaded %u phase definitions", count);
    }

    _PhaseDefinitionStore.swap(phaseDefinitionStore);
    sLog.outString();
}

PhaseDefinitionContainer const* ObjectMgr::GetPhaseDefinitions(uint32 zoneId) const
{
    PhaseDefinitionStore::const_iterator itr = _PhaseDefinitionStore.find(zoneId);
    if (itr == _PhaseDefinitionStore.end())
        return nullptr;

    return &itr->second;
}
