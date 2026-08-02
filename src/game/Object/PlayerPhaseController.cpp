/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server of World of Warcraft.
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#include "PlayerPhaseController.h"

#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"

namespace
{
char const* GetPlayerPhaseReasonName(PlayerPhaseReason reason)
{
    switch (reason)
    {
        case PlayerPhaseReason::MapEntry: return "map entry";
        case PlayerPhaseReason::ZoneChanged: return "zone changed";
        case PlayerPhaseReason::QuestStateChanged: return "quest state changed";
        case PlayerPhaseReason::AuraApply: return "aura applied";
        case PlayerPhaseReason::AuraRemove: return "aura removed";
        case PlayerPhaseReason::GameMasterChanged: return "GM state changed";
        case PlayerPhaseReason::AdministrativeOverrideChanged: return "administrative override changed";
        case PlayerPhaseReason::DefinitionReload: return "definitions reloaded";
    }

    return "unknown";
}
}

PlayerPhaseController::PlayerPhaseController(Player& owner)
    : m_owner(owner), m_definitionMask(0), m_auraMask(0), m_administrativeOverrideMask(0),
      m_definitionZoneId(0), m_gameMasterOverride(false), m_administrativeOverrideActive(false)
{
}

void PlayerPhaseController::InitializeForMapEntry(uint32 zoneId)
{
    RecalculateDefinitions(zoneId, false, PlayerPhaseReason::MapEntry);
}

void PlayerPhaseController::RecalculateDefinitions(uint32 zoneId, bool update, PlayerPhaseReason reason)
{
    m_definitionZoneId = zoneId;
    m_definitionMask = 0;

    if (PhaseDefinitionContainer const* definitions = sObjectMgr.GetPhaseDefinitions(zoneId))
    {
        m_definitionMask = EvaluatePhaseDefinitions(*definitions, [this](uint16 conditionId)
        {
            return sObjectMgr.IsPlayerMeetToCondition(conditionId, &m_owner, m_owner.GetMap(), nullptr, CONDITION_FROM_HARDCODED);
        });
    }

    Apply(update, reason);
}

void PlayerPhaseController::SetAuraMask(uint32 mask, bool update)
{
    m_auraMask = mask;
    Apply(update, PlayerPhaseReason::AuraApply);
}

void PlayerPhaseController::ClearAuraMask(bool update)
{
    m_auraMask = 0;
    Apply(update, PlayerPhaseReason::AuraRemove);
}

void PlayerPhaseController::SetGameMasterOverride(bool active, bool update)
{
    m_gameMasterOverride = active;
    Apply(update, PlayerPhaseReason::GameMasterChanged);
}

void PlayerPhaseController::SetAdministrativeOverride(uint32 mask, bool update)
{
    m_administrativeOverrideActive = true;
    m_administrativeOverrideMask = mask;
    Apply(update, PlayerPhaseReason::AdministrativeOverrideChanged);
}

void PlayerPhaseController::ClearAdministrativeOverride(bool update)
{
    m_administrativeOverrideActive = false;
    m_administrativeOverrideMask = 0;
    Apply(update, PlayerPhaseReason::AdministrativeOverrideChanged);
}

uint32 PlayerPhaseController::GetEffectiveMask() const
{
    return ComposePhaseMask(m_gameMasterOverride, m_administrativeOverrideActive,
        m_administrativeOverrideMask, m_definitionMask, m_auraMask);
}

void PlayerPhaseController::Apply(bool update, PlayerPhaseReason reason)
{
    uint32 oldMask = m_owner.GetPhaseMask();
    uint32 newMask = GetEffectiveMask();
    if (newMask == oldMask)
        return;

    DEBUG_LOG("Player phase changed: player %u, old 0x%08X, new 0x%08X, zone %u, reason %s",
        m_owner.GetGUIDLow(), oldMask, newMask, m_definitionZoneId, GetPlayerPhaseReasonName(reason));
    m_owner.ApplyComposedPhaseMask(newMask, update);
}
