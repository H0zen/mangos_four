/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "PhaseDefinition.h"

#include "Object.h"

PhaseDefinitionValidationResult ValidatePhaseDefinition(PhaseDefinitionRecord const& record,
    bool conditionExists, PhaseDefinition& definition)
{
    if (record.zoneId == 0)
        return PhaseDefinitionValidationResult::ZoneIdZero;
    if (record.entry == 0)
        return PhaseDefinitionValidationResult::EntryZero;
    if (record.phaseMask == 0)
        return PhaseDefinitionValidationResult::PhaseMaskZero;
    if (record.phaseMask > UINT16_MAX)
        return PhaseDefinitionValidationResult::PhaseMaskOutOfRange;
    if (record.conditionId > UINT16_MAX)
        return PhaseDefinitionValidationResult::ConditionIdOutOfRange;
    if (record.conditionId != 0 && !conditionExists)
        return PhaseDefinitionValidationResult::ConditionMissing;
    if (record.phaseId != 0)
        return PhaseDefinitionValidationResult::PhaseIdNonZero;
    if (record.terrainSwapMap != 0)
        return PhaseDefinitionValidationResult::TerrainSwapMapNonZero;
    if (record.flags != 0)
        return PhaseDefinitionValidationResult::FlagsNonZero;

    definition.zoneId = record.zoneId;
    definition.entry = record.entry;
    definition.phaseMask = static_cast<uint32>(record.phaseMask);
    definition.conditionId = static_cast<uint16>(record.conditionId);
    return PhaseDefinitionValidationResult::Success;
}

uint32 EvaluatePhaseDefinitions(std::vector<PhaseDefinition> const& definitions,
    std::function<bool(uint16)> const& conditionMatches)
{
    uint32 phaseMask = 0;
    for (PhaseDefinition const& definition : definitions)
    {
        if (definition.conditionId == 0 || conditionMatches(definition.conditionId))
            phaseMask |= definition.phaseMask;
    }

    return phaseMask;
}

bool PhaseMasksIntersect(uint32 firstMask, uint32 secondMask)
{
    return (firstMask & secondMask) != 0;
}

uint32 ComposePhaseMask(bool gmActive, bool administrativeOverrideActive,
    uint32 administrativeOverrideMask, uint32 definitionMask, uint32 auraMask)
{
    if (gmActive)
        return PHASEMASK_ANYWHERE;
    if (administrativeOverrideActive)
        return administrativeOverrideMask;
    if (definitionMask != 0 || auraMask != 0)
        return definitionMask | auraMask;

    return PHASEMASK_NORMAL;
}
