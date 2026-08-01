/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#ifndef MANGOS_PHASE_DEFINITION_H
#define MANGOS_PHASE_DEFINITION_H

#include "Common.h"

#include <functional>
#include <vector>

struct PhaseDefinitionRecord
{
    uint32 zoneId;
    uint16 entry;
    uint64 phaseMask;
    uint8 phaseId;
    uint16 terrainSwapMap;
    uint8 flags;
    uint32 conditionId;
};

struct PhaseDefinition
{
    uint32 zoneId;
    uint16 entry;
    uint32 phaseMask;
    uint16 conditionId;
};

enum class PhaseDefinitionValidationResult
{
    Success,
    ZoneIdZero,
    EntryZero,
    PhaseMaskZero,
    PhaseMaskOutOfRange,
    ConditionIdOutOfRange,
    ConditionMissing,
    PhaseIdNonZero,
    TerrainSwapMapNonZero,
    FlagsNonZero,
};

PhaseDefinitionValidationResult ValidatePhaseDefinition(PhaseDefinitionRecord const& record,
    bool conditionExists, PhaseDefinition& definition);
uint32 EvaluatePhaseDefinitions(std::vector<PhaseDefinition> const& definitions,
    std::function<bool(uint16)> const& conditionMatches);
uint32 ComposePhaseMask(bool gmActive, bool administrativeOverrideActive,
    uint32 administrativeOverrideMask, uint32 definitionMask, uint32 auraMask);

#endif
