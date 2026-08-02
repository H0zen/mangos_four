/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server of World of Warcraft.
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 */

#ifndef MANGOS_PLAYER_PHASE_CONTROLLER_H
#define MANGOS_PLAYER_PHASE_CONTROLLER_H

#include "Common.h"

class Player;

enum class PlayerPhaseReason : uint8
{
    MapEntry,
    ZoneChanged,
    QuestStateChanged,
    AuraApply,
    AuraRemove,
    GameMasterChanged,
    AdministrativeOverrideChanged,
    DefinitionReload
};

class PlayerPhaseController
{
    public:
        explicit PlayerPhaseController(Player& owner);

        void InitializeForMapEntry(uint32 zoneId);
        void RecalculateDefinitions(uint32 zoneId, bool update, PlayerPhaseReason reason);
        void SetAuraMask(uint32 mask, bool update);
        void ClearAuraMask(bool update);
        void SetGameMasterOverride(bool active, bool update);
        void SetAdministrativeOverride(uint32 mask, bool update);
        void ClearAdministrativeOverride(bool update);

        uint32 GetDefinitionMask() const { return m_definitionMask; }
        uint32 GetAuraMask() const { return m_auraMask; }
        bool IsGameMasterOverrideActive() const { return m_gameMasterOverride; }
        bool IsAdministrativeOverrideActive() const { return m_administrativeOverrideActive; }
        uint32 GetAdministrativeOverrideMask() const { return m_administrativeOverrideMask; }
        uint32 GetEffectiveMask() const;

    private:
        void Apply(bool update, PlayerPhaseReason reason);

        Player& m_owner;
        uint32 m_definitionMask;
        uint32 m_auraMask;
        uint32 m_administrativeOverrideMask;
        uint32 m_definitionZoneId;
        bool m_gameMasterOverride;
        bool m_administrativeOverrideActive;
};

#endif
