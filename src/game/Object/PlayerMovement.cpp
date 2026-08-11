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

#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "SkillDiscovery.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "AchievementMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "DB2Stores.h"
#include "SQLStorages.h"
#include "Vehicle.h"
#include "Calendar.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#include <cmath>

/**
 * @brief Forces or clears rooted movement for the player.
 *
 * @param enable True to root the player; false to unroot them.
 */
void Player::SetRoot(bool enable)
{
    // Update the server's own view first. Every Creature setter does this and no
    // Player setter did, so the server told the client to fly and then went on
    // believing it could not: Player::CanFly() reads m_movementInfo, and nothing
    // wrote it until the client's next ordinary movement packet happened to
    // carry the flag. That is the brief's "changes client state without updating
    // server state", one step earlier in the sequence than the mount command
    // that used to kick the player it helped.
    //
    // The client remains authoritative for a player -- an incoming movement
    // packet still assigns the whole struct -- so this closes the window rather
    // than claiming ownership of it.
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_ROOT);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_ROOT);
    }

    // This was the last sender broadcasting the MOVER packet to everyone, and
    // the outlier among its own neighbours -- SetCanFly and SetWaterWalk had
    // already been corrected. Observers were handed SMSG_FORCE_MOVE_ROOT, which
    // carries a counter and expects an acknowledgement they cannot make, while
    // the observer opcode that exists for them was never sent at all.
    //
    // Retail splits the audience: a capture window shows the mover's root
    // arriving as SMSG_FORCE_MOVE_ROOT and acknowledged ten packets later, with
    // no SMSG_SPLINE_MOVE_ROOT anywhere in it -- that one goes to everybody else.
    //
    // This runs far beyond any GM command: death, resurrection and vehicle
    // boarding all root through here.
    WorldPacket data;
    BuildForceMoveRootPacket(&data, enable, NextMovementCounter());
    GetSession()->SendPacket(&data);

    if (!IsInWorld())
    {
        return;
    }

    WorldPacket spline(enable ? SMSG_SPLINE_MOVE_ROOT : SMSG_SPLINE_MOVE_UNROOT, 9);
    if (enable)
    {
        MopCompactPackets::BuildSplineMoveRoot(spline, GetObjectGuid().GetRawValue());
    }
    else
    {
        MopCompactPackets::BuildSplineMoveUnroot(spline, GetObjectGuid().GetRawValue());
    }

    SendMessageToSet(&spline, false);
}

/**
 * @brief Enables or disables water walking for the player.
 *
 * The same pair rule as SetCanFly: the mover is told with SMSG_MOVE_WATER_WALK /
 * SMSG_MOVE_LAND_WALK, which carry a movement counter, and observers with
 * SMSG_SPLINE_MOVE_SET_WATER_WALK / SMSG_SPLINE_MOVE_SET_LAND_WALK, which do
 * not. Only the mover half was sent, and its body was wrong in every field, so
 * this changed nothing on any screen.
 *
 * This runs well beyond .waterwalk: it also fires on death and on ghost login,
 * so a broken pair leaves corpses walking into water.
 *
 * @param enable True to enable water walking; false to restore normal movement.
 */
void Player::SetWaterWalk(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_WATERWALKING);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_WATERWALKING);
    }

    WorldPacket data;
    BuildMoveWaterWalkPacket(&data, enable, NextMovementCounter());
    GetSession()->SendPacket(&data);

    if (!IsInWorld())
    {
        return;
    }

    WorldPacket spline(enable ? SMSG_SPLINE_MOVE_SET_WATER_WALK : SMSG_SPLINE_MOVE_SET_LAND_WALK, 9);
    if (enable)
    {
        MopCompactPackets::BuildSplineMoveSetWaterWalk(spline, GetObjectGuid());
    }
    else
    {
        MopCompactPackets::BuildSplineMoveSetLandWalk(spline, GetObjectGuid());
    }

    SendMessageToSet(&spline, false);
}

/**
 * @brief Placeholder for levitation support on this client version.
 *
 * @param enable Unused levitation state flag.
 */
void Player::SetLevitate(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_LEVITATING);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_LEVITATING);
    }

    WorldPacket data;
    BuildMoveLevitatePacket(&data, enable, NextMovementCounter());
    GetSession()->SendPacket(&data);

    if (!IsInWorld())
    {
        return;
    }

    WorldPacket spline(enable ? SMSG_SPLINE_MOVE_GRAVITY_DISABLE : SMSG_SPLINE_MOVE_GRAVITY_ENABLE, 9);
    if (enable)
    {
        MopCompactPackets::BuildSplineMoveGravityDisable(spline, GetObjectGuid().GetRawValue());
    }
    else
    {
        MopCompactPackets::BuildSplineMoveGravityEnable(spline, GetObjectGuid().GetRawValue());
    }

    SendMessageToSet(&spline, false);
}

/**
 * @brief Enables or disables flying movement flags for the player.
 *
 * Flight is a PAIR of packets, and sending either alone is visibly wrong. The
 * mover is told with SMSG_MOVE_SET_CAN_FLY / SMSG_MOVE_UNSET_CAN_FLY, which
 * carry a movement counter and are addressed to the controlling session.
 * Everyone else is told with SMSG_SPLINE_MOVE_SET_FLYING /
 * SMSG_SPLINE_MOVE_UNSET_FLYING, which carry no counter because an observer has
 * no acknowledgement to make.
 *
 * Only the mover half was sent here, so ".gm fly on" lifted the GM off the
 * ground on their own screen while every other client still drew them walking.
 * Creature::SetCanFly had the opposite half and the same defect mirrored.
 *
 * The observer packet must not go to the mover as well: it would arrive as a
 * second, counter-less state change for a mover that is already mid-handshake on
 * the first.
 *
 * @param enable True to enable flight-related movement flags; false to clear them.
 */
void Player::SetCanFly(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_CAN_FLY);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_CAN_FLY);
    }

    WorldPacket data;
    BuildMoveSetCanFlyPacket(&data, enable, NextMovementCounter());
    GetSession()->SendPacket(&data);

    if (!IsInWorld())
    {
        return;
    }

    WorldPacket spline(enable ? SMSG_SPLINE_MOVE_SET_FLYING : SMSG_SPLINE_MOVE_UNSET_FLYING, 9);
    if (enable)
    {
        MopCompactPackets::BuildSplineMoveSetFlying(spline, GetObjectGuid().GetRawValue());
    }
    else
    {
        MopCompactPackets::BuildSplineMoveUnsetFlying(spline, GetObjectGuid().GetRawValue());
    }

    SendMessageToSet(&spline, false);
}

/**
 * @brief Enables or disables feather fall movement for the player.
 *
 * @param enable True to enable feather fall; false to restore normal falling.
 */
void Player::SetFeatherFall(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_SAFE_FALL);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_SAFE_FALL);
    }

    // This was the third variant of the pair defect: not a missing half, but the
    // MOVER packet broadcast to everyone. SMSG_MOVE_FEATHER_FALL carries a
    // movement counter and is addressed to the controlling session; observers
    // receiving it are being handed an acknowledgement they cannot make. They
    // need SMSG_SPLINE_MOVE_SET_FEATHER_FALL, which has no counter.
    WorldPacket data;
    BuildMoveFeatherFallPacket(&data, enable, NextMovementCounter());
    GetSession()->SendPacket(&data);

    if (IsInWorld())
    {
        WorldPacket spline(enable ? SMSG_SPLINE_MOVE_SET_FEATHER_FALL : SMSG_SPLINE_MOVE_SET_NORMAL_FALL, 9);
        if (enable)
        {
            MopCompactPackets::BuildSplineMoveSetFeatherFall(spline, GetObjectGuid());
        }
        else
        {
            MopCompactPackets::BuildSplineMoveSetNormalFall(spline, GetObjectGuid());
        }

        SendMessageToSet(&spline, false);
    }

    // start fall from current height
    if (!enable)
    {
        SetFallInformation(0, Where().Z());
    }
}

/**
 * @brief Enables or disables hover movement for the player.
 *
 * @param enable True to enable hovering; false to disable it.
 */
void Player::SetHover(bool enable)
{
    if (enable)
    {
        m_movementInfo.AddMovementFlag(MOVEFLAG_HOVER);
    }
    else
    {
        m_movementInfo.RemoveMovementFlag(MOVEFLAG_HOVER);
    }

    WorldPacket data;
    BuildMoveHoverPacket(&data, enable, NextMovementCounter());
    GetSession()->SendPacket(&data);

    if (!IsInWorld())
    {
        return;
    }

    WorldPacket spline(enable ? SMSG_SPLINE_MOVE_SET_HOVER : SMSG_SPLINE_MOVE_UNSET_HOVER, 9);
    if (enable)
    {
        MopCompactPackets::BuildSplineMoveSetHover(spline, GetObjectGuid().GetRawValue());
    }
    else
    {
        MopCompactPackets::BuildSplineMoveUnsetHover(spline, GetObjectGuid().GetRawValue());
    }

    SendMessageToSet(&spline, false);
}
