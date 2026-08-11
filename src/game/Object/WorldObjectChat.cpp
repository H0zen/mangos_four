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

#include "TransportMap.h"
#include "Object.h"
#include "GameObject.h"
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
 * @file WorldObjectChat.cpp
 * @brief Cohesion split of Object.cpp -- WorldObject chat/messaging: monster say/yell/emote/whisper, localized text broadcast, message-to-set delivery and despawn/custom-anim packets. Same WorldObject class; no behaviour change. CMake file(GLOB Object/*.cpp) picks this file up automatically; Object.h is unchanged.
 */

/**
 * @brief Send monster say message
 * @param text Text to say
 * @param language Language (unused)
 * @param target Target unit
 *
 * Sends a monster say message to nearby players.
 */
void WorldObject::MonsterSay(const char* text, uint32 language, Unit const* target) const
{
    WorldPacket data(SMSG_MESSAGECHAT, 200);
    ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_SAY, text, LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid(), GetName(),
                                 target ? target->GetObjectGuid() : ObjectGuid(), target ? target->GetName() : "");
    SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);
}

/**
 * @brief Send monster yell message
 * @param text Text to yell
 * @param language Language (unused)
 * @param target Target unit
 *
 * Sends a monster yell message to nearby players.
 */
void WorldObject::MonsterYell(const char* text, uint32 language, Unit const* target) const
{
    WorldPacket data(SMSG_MESSAGECHAT, 200);
    ChatHandler::BuildChatPacket(data, CHAT_MSG_MONSTER_YELL, text, LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid(), GetName(),
                                 target ? target->GetObjectGuid() : ObjectGuid(), target ? target->GetName() : "");
    SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL), true);
}

/**
 * @brief Send monster text emote message
 * @param text Text to emote
 * @param target Target unit
 * @param IsBossEmote If true, use boss emote range
 *
 * Sends a monster text emote message to nearby players.
 */
void WorldObject::MonsterTextEmote(const char* text, Unit const* target, bool IsBossEmote) const
{
    WorldPacket data(SMSG_MESSAGECHAT, 200);
    ChatHandler::BuildChatPacket(data, IsBossEmote ? CHAT_MSG_RAID_BOSS_EMOTE : CHAT_MSG_MONSTER_EMOTE, text, LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid(), GetName(),
                                 target ? target->GetObjectGuid() : ObjectGuid(), target ? target->GetName() : "");
    SendMessageToSetInRange(&data, sWorld.getConfig(IsBossEmote ? CONFIG_FLOAT_LISTEN_RANGE_YELL : CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), true);
}

/**
 * @brief Send monster whisper message
 * @param text Text to whisper
 * @param target Target unit
 * @param IsBossWhisper If true, use boss whisper
 *
 * Sends a monster whisper message to the target player.
 */
void WorldObject::MonsterWhisper(const char* text, Unit const* target, bool IsBossWhisper) const
{
    if (!target || target->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    WorldPacket data(SMSG_MESSAGECHAT, 200);
    ChatHandler::BuildChatPacket(data, IsBossWhisper ? CHAT_MSG_RAID_BOSS_WHISPER : CHAT_MSG_MONSTER_WHISPER, text, LANG_UNIVERSAL, CHAT_TAG_NONE, GetObjectGuid(), GetName(),
                                 target->GetObjectGuid(), target->GetName());
    ((Player*)target)->GetSession()->SendPacket(&data);
}

namespace MaNGOS
{
    /**
     * @brief Monster chat builder functor
     *
     * Builds localized chat packets for monster messages.
     */
    class MonsterChatBuilder
    {
        public:
            /**
             * @brief Constructor
             * @param obj Source object
             * @param msgtype Chat message type
             * @param textData Localized text data
             * @param language Language
             * @param target Target unit
             */
            MonsterChatBuilder(WorldObject const& obj, ChatMsg msgtype, MangosStringLocale const* textData, Language language, Unit const* target)
                : i_object(obj), i_msgtype(msgtype), i_textData(textData), i_language(language), i_target(target) {}

            /**
             * @brief Build chat packet
             * @param data Packet to build
             * @param loc_idx Locale index
             */
            void operator()(WorldPacket& data, int32 loc_idx)
            {
                char const* text = NULL;
                if ((int32)i_textData->Content.size() > loc_idx + 1 && !i_textData->Content[loc_idx + 1].empty())
                {
                    text = i_textData->Content[loc_idx + 1].c_str();
                }
                else
                {
                    text = i_textData->Content[0].c_str();
                }

                ChatHandler::BuildChatPacket(data, i_msgtype, text, i_language, CHAT_TAG_NONE, i_object.GetObjectGuid(), i_object.GetNameForLocaleIdx(loc_idx),
                                             i_target ? i_target->GetObjectGuid() : ObjectGuid(), i_target ? i_target->GetNameForLocaleIdx(loc_idx) : "");
            }

        private:
            WorldObject const& i_object; ///< Source object
            ChatMsg i_msgtype; ///< Chat message type
            MangosStringLocale const* i_textData; ///< Localized text data
            Language i_language; ///< Language
            Unit const* i_target; ///< Target unit
    };
}                                                           // namespace MaNGOS

/**
 * @brief Send localized text around source
 * @param source Source object
 * @param textData Localized text data
 * @param msgtype Chat message type
 * @param language Language
 * @param target Target unit
 * @param range Range to send message
 *
 * Helper function to create localized chat around a source.
 */
/**
 * @brief Sends localized monster chat packets to players around a source object.
 *
 * @param source The source world object.
 * @param textData The localized text definition.
 * @param msgtype The chat packet type.
 * @param language The chat language.
 * @param target Optional target unit.
 * @param range Broadcast range.
 */
void _DoLocalizedTextAround(WorldObject const* source, MangosStringLocale const* textData, ChatMsg msgtype, Language language, Unit const* target, float range)
{
    MaNGOS::MonsterChatBuilder say_build(*source, msgtype, textData, language, target);
    MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> > say_worker(source, range, say_do);
    Cell::VisitWorldObjects(source, say_worker, range);
}

/**
 * @brief Send monster text
 * @param textData Localized text data
 * @param target Target unit
 *
 * Sends a text message associated with a MangosString,
 * localized for each player's locale.
 */
/**
 * @brief Sends a localized monster text defined by a Mangos string entry.
 *
 * @param textData The localized text definition.
 * @param target Optional target unit.
 */
void WorldObject::MonsterText(MangosStringLocale const* textData, Unit const* target) const
{
    MANGOS_ASSERT(textData);

    switch (textData->Type)
    {
        case CHAT_TYPE_SAY:
            _DoLocalizedTextAround(this, textData, CHAT_MSG_MONSTER_SAY, textData->LanguageId, target, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY));
            break;
        case CHAT_TYPE_YELL:
            _DoLocalizedTextAround(this, textData, CHAT_MSG_MONSTER_YELL, textData->LanguageId, target, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL));
            break;
        case CHAT_TYPE_TEXT_EMOTE:
            _DoLocalizedTextAround(this, textData, CHAT_MSG_MONSTER_EMOTE, LANG_UNIVERSAL, target, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));
            break;
        case CHAT_TYPE_BOSS_EMOTE:
            _DoLocalizedTextAround(this, textData, CHAT_MSG_RAID_BOSS_EMOTE, LANG_UNIVERSAL, target, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_YELL));
            break;
        case CHAT_TYPE_WHISPER:
        {
            if (!target || target->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }
            MaNGOS::MonsterChatBuilder say_build(*this, CHAT_MSG_MONSTER_WHISPER, textData, LANG_UNIVERSAL, target);
            MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
            say_do((Player*)target);
            break;
        }
        case CHAT_TYPE_BOSS_WHISPER:
        {
            if (!target || target->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }
            MaNGOS::MonsterChatBuilder say_build(*this, CHAT_MSG_RAID_BOSS_WHISPER, textData, LANG_UNIVERSAL, target);
            MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
            say_do((Player*)target);
            break;
        }
        case CHAT_TYPE_ZONE_YELL:
        {
            MaNGOS::MonsterChatBuilder say_build(*this, CHAT_MSG_MONSTER_YELL, textData, textData->LanguageId, target);
            MaNGOS::LocalizedPacketDo<MaNGOS::MonsterChatBuilder> say_do(say_build);
            uint32 zoneid = GetZoneId();
            Map::PlayerList const& pList = GetMap()->GetPlayers();
            for (Map::PlayerList::const_iterator itr = pList.begin(); itr != pList.end(); ++itr)
                if (itr->getSource()->GetZoneId() == zoneid)
                {
                    say_do(itr->getSource());
                }
            break;
        }
    }
}

/**
 * @brief Broadcasts a packet to all players in the object's visibility set.
 *
 * @param data The packet to send.
 * @param bToSelf Unused self-delivery flag.
 */
namespace
{
    /**
     * @brief Send `data` across a vessel's boundary, both directions.
     *
     * @param from   The object that is speaking.
     * @param dist   The broadcast range, or 0 for an unranged one.
     * @param ranged Whether `dist` means anything.
     * @return True when `from` is on a deck, in which case the ordinary broadcast has
     *         already reached everyone on that map and there is nothing else to do.
     *
     * THE DISTANCE TEST IS AT GRID GRANULARITY, and that is not a compromise -- it is the
     * best statement that can be made truthfully. The server does not know where the hull
     * is: its pose is a `GameTime % period` estimate that snaps between waypoints while the
     * client interpolates the real curve. What IS known is which square she is in, and by
     * how much that square can be wrong: NodeSlack() is half the longest gap between two
     * consecutive nodes, so estimate +/- (NodeSlack + HullRadius) provably contains her.
     * Testing against that is exact about a coarse thing, rather than precise about a
     * number that is not true.
     */
    bool RelayAcrossVessels(WorldObject const* from, WorldPacket* data, float dist, bool ranged)
    {
        Map* on = from->GetMap();

        // OUTBOUND. The people ashore are on another map and no cell of theirs will ever
        // hold this deckhand, so the same packet goes out again to the watchers the vessel
        // gathered at the top of this tick. Sent immediately: the deck runs INSIDE the tick
        // of the map it sails, on that map's own thread, so there is nothing to wait for.
        //
        // The gathering itself was already bounded by visibility + hull + slack, so the
        // range test has been applied once and does not need repeating here.
        if (on->AsTransport())
        {
            for (Player* observer : on->ExternalObservers())
            {
                if (observer && observer->GetSession())
                {
                    observer->GetSession()->SendPacket(data);
                }
            }

            return true;
        }

        // INBOUND. Whatever happens on the water a ship is crossing has to reach the people
        // standing on her: they are on another map and no cell of theirs will ever hold the
        // thing that spoke, and a passenger who is told nothing watches a harbour of statues.
        MapManager::TransportsByMapType::const_iterator vessels =
            sMapMgr.m_TransportsByMap.find(from->GetMapId());
        if (vessels == sMapMgr.m_TransportsByMap.end())
        {
            return false;
        }

        for (Transport* vessel : vessels->second)
        {
            TransportMap* hull = vessel->AsMap();
            if (!hull || vessel->GetMap() != on)
            {
                continue;
            }

            // The grid the vessel is in, widened by everything the estimate can be wrong by.
            // Inside it she may be anywhere; outside it she cannot be.
            if (ranged && !vessel->Where().WithinDist(
                    from->Where(), dist + hull->HullRadius() + vessel->NodeSlack(), false))
            {
                continue;
            }

            Map::PlayerList const& aboard = hull->GetPlayers();
            for (Map::PlayerList::const_iterator itr = aboard.begin(); itr != aboard.end(); ++itr)
            {
                Player* passenger = itr->getSource();
                if (passenger && passenger->GetSession())
                {
                    passenger->GetSession()->SendPacket(data);
                }
            }
        }

        return false;
    }
}

void WorldObject::SendMessageToSet(WorldPacket* data, bool /*bToSelf*/) const
{
    // if object is in world, map for it already created!
    if (IsInWorld())
    {
        // A boarded unit's audience is EVERYONE ON ITS OWN MAP -- which is the deck, and
        // which is exactly where the passengers are. That is the point of the vessel being
        // a map: the ordinary broadcast already reaches the people standing next to him.
        GetMap()->MessageBroadcast(this, data);
        RelayAcrossVessels(this, data, 0.0f, false);
    }
}

/**
 * @brief Broadcasts a packet to players within a specified range.
 *
 * @param data The packet to send.
 * @param dist The broadcast distance.
 * @param bToSelf Unused self-delivery flag.
 */
void WorldObject::SendMessageToSetInRange(WorldPacket* data, float dist, bool /*bToSelf*/) const
{
    // if object is in world, map for it already created!
    if (IsInWorld())
    {
        GetMap()->MessageDistBroadcast(this, data, dist);
        RelayAcrossVessels(this, data, dist, true);
    }
}

/**
 * @brief Broadcasts a packet to visible players except one receiver.
 *
 * @param data The packet to send.
 * @param skipped_receiver The player to exclude.
 */
void WorldObject::SendMessageToSetExcept(WorldPacket* data, Player const* skipped_receiver) const
{
    // if object is in world, map for it already created!
    if (IsInWorld())
    {
        MaNGOS::MessageDelivererExcept notifier(this, data, skipped_receiver);
        Cell::VisitWorldObjects(this, notifier, GetMap()->GetBroadcastRadius());
    }
}

void WorldObject::SendObjectDeSpawnAnim(ObjectGuid guid)
{
    WorldPacket data;
    // 18414 consumes a packed GUID here, not the legacy raw uint64 body.
    MopGameObjectPackets::BuildDespawnAnimation(data, guid.GetRawValue());
    SendMessageToSet(&data, true);
}

void WorldObject::SendGameObjectCustomAnim(ObjectGuid guid, uint32 animId /*= 0*/)
{
    MopGameObjectPackets::CustomAnimation animation;
    animation.gameObjectGuid = guid.GetRawValue();
    animation.animId = animId;

    WorldPacket data;
    if (!MopGameObjectPackets::BuildCustomAnimation(data, animation))
    {
        // The 18414 reader has only four animation slots. Do not put a
        // client-out-of-range index on the wire.
        sLog.outError("WorldObject::SendGameObjectCustomAnim: animation id %u is out of range",
            animId);
        return;
    }
    SendMessageToSet(&data, true);
}
