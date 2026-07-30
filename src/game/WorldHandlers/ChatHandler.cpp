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

/**
 * @file ChatHandler.cpp
 * @brief Chat message opcode handlers
 *
 * This file handles chat-related opcodes including:
 * - CMSG_MESSAGECHAT: Send chat messages (say, yell, whisper, channel, etc.)
 * - CMSG_TEXT_EMOTE: Send text emotes
 * - CMSG_CHAT_MESSAGE_AFK: Set AFK status
 * - CMSG_CHAT_MESSAGE_DND: Set DND status
 * - CMSG_CHAT_IGNORED: Manage ignore list
 *
 * Chat messages are routed based on type (say, yell, whisper, channel, emote)
 * and filtered by language, distance, and other rules.
 */

#include "Common.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "ChannelMgr.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Player.h"
#include "SpellAuras.h"
#include "Language.h"
#include "Util.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

namespace
{
    /// The 18414 client appends "-Realm" to a whisper target it did not get by
    /// hand: a reply, a chat link, or a who-list click all arrive as
    /// "Name-RealmWithoutSpaces", while a typed name arrives bare. Both forms
    /// name the same character, so the home-realm suffix has to come off before
    /// the lookup or replying to anyone fails with "player not found".
    ///
    /// Only OUR realm's suffix is stripped. A genuinely foreign "Name-Other"
    /// keeps it and fails to resolve, which is correct -- silently matching a
    /// local character of the same name would whisper the wrong person.
    void StripHomeRealmSuffix(std::string& name)
    {
        std::string const suffix = "-" + NormalizeRealmName(CachedRealmName());
        if (name.size() <= suffix.size())
        {
            return;
        }

        size_t const at = name.size() - suffix.size();
        for (size_t i = 0; i < suffix.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(name[at + i])) !=
                std::tolower(static_cast<unsigned char>(suffix[i])))
            {
                return;
            }
        }
        name.resize(at);
    }
}

/**
 * @brief Applies post-parse security checks to a chat message before broadcast.
 *
 * @param msg The chat message to validate and normalize.
 * @param lang The message language.
 * @return true if the message may continue processing; otherwise false.
 */
bool WorldSession::processChatmessageFurtherAfterSecurityChecks(std::string& msg, uint32 lang)
{
    if (lang != LANG_ADDON)
    {
        // strip invisible characters for non-addon messages
        if (sWorld.getConfig(CONFIG_BOOL_CHAT_FAKE_MESSAGE_PREVENTING))
        {
            stripLineInvisibleChars(msg);
        }

        if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_SEVERITY) && GetSecurity() < SEC_MODERATOR
            && !ChatHandler(this).isValidChatMessage(msg.c_str()))
        {
            sLog.outError("Player %s (GUID: %u) sent a chatmessage with an invalid link: %s", GetPlayer()->GetName(),
                          GetPlayer()->GetGUIDLow(), msg.c_str());
            if (sWorld.getConfig(CONFIG_UINT32_CHAT_STRICT_LINK_CHECKING_KICK))
            {
                KickPlayer();
            }
            return false;
        }
    }

    return true;
}

/**
 * @brief Handles incoming chat packets for all chat message types.
 *
 * @param recv_data The incoming chat packet.
 */
void WorldSession::HandleMessagechatOpcode(WorldPacket& recv_data)
{
    uint32 type;
    uint32 lang;

    switch (recv_data.GetOpcode())
    {
        case CMSG_MESSAGECHAT_SAY:          type = CHAT_MSG_SAY;            break;
        case CMSG_MESSAGECHAT_YELL:         type = CHAT_MSG_YELL;           break;
        case CMSG_MESSAGECHAT_CHANNEL:      type = CHAT_MSG_CHANNEL;        break;
        case CMSG_MESSAGECHAT_WHISPER:      type = CHAT_MSG_WHISPER;        break;
        case CMSG_MESSAGECHAT_GUILD:        type = CHAT_MSG_GUILD;          break;
        case CMSG_MESSAGECHAT_OFFICER:      type = CHAT_MSG_OFFICER;        break;
        case CMSG_MESSAGECHAT_AFK:          type = CHAT_MSG_AFK;            break;
        case CMSG_MESSAGECHAT_DND:          type = CHAT_MSG_DND;            break;
        case CMSG_MESSAGECHAT_EMOTE:        type = CHAT_MSG_EMOTE;          break;
        case CMSG_MESSAGECHAT_PARTY:        type = CHAT_MSG_PARTY;          break;
        case CMSG_MESSAGECHAT_RAID:         type = CHAT_MSG_RAID;           break;
        case CMSG_MESSAGECHAT_INSTANCE:     type = CHAT_MSG_BATTLEGROUND;   break;
        case CMSG_MESSAGECHAT_RAID_WARNING: type = CHAT_MSG_RAID_WARNING;   break;
        default:
            sLog.outError("HandleMessagechatOpcode : Unknown chat opcode (0x%X)", recv_data.GetOpcode());
            recv_data.rfinish();
            return;
    }

    // no language sent with emote packet.
    if (type != CHAT_MSG_EMOTE && type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
    {
        recv_data >> lang;

        // prevent talking at unknown language (cheating)
        LanguageDesc const* langDesc = GetLanguageDescByID(lang);
        if (!langDesc)
        {
            SendNotification(LANG_UNKNOWN_LANGUAGE);
            return;
        }
        // In 18414, racial languages are persisted as known
        // SPELL_EFFECT_LANGUAGE spells. Accept that authoritative state as well
        // as the legacy skill row so older characters are not locked out when
        // their already-known language spell never rebuilt mSkillStatus.
        if (langDesc->skill_id != 0 && !_player->HasSkill(langDesc->skill_id) &&
                !_player->HasSpell(langDesc->spell_id))
        {
            // also check SPELL_AURA_COMPREHEND_LANGUAGE (client offers option to speak in that language)
            Unit::AuraList const& langAuras = _player->GetAurasByType(SPELL_AURA_COMPREHEND_LANGUAGE);
            bool foundAura = false;
            for (Unit::AuraList::const_iterator i = langAuras.begin(); i != langAuras.end(); ++i)
            {
                if ((*i)->GetModifier()->m_miscvalue == int32(lang))
                {
                    foundAura = true;
                    break;
                }
            }
            if (!foundAura)
            {
                SendNotification(LANG_NOT_LEARNED_LANGUAGE);
                return;
            }
        }

        if (lang == LANG_ADDON)
        {
            // Disabled addon channel?
            if (!sWorld.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
            {
                return;
            }
        }
        // LANG_ADDON should not be changed nor be affected by flood control
        else
        {
            // send in universal language if player in .gmon mode (ignore spell effects)
            if (_player->isGameMaster())
            {
                lang = LANG_UNIVERSAL;
            }
            else
            {
                // send in universal language in two side iteration allowed mode
                if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT))
                {
                    lang = LANG_UNIVERSAL;
                }
                else
                {
                    switch (type)
                    {
                        case CHAT_MSG_PARTY:
                        case CHAT_MSG_PARTY_LEADER:
                        case CHAT_MSG_RAID:
                        case CHAT_MSG_RAID_LEADER:
                        case CHAT_MSG_RAID_WARNING:
                            // allow two side chat at group channel if two side group allowed
                            if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GROUP))
                            {
                                lang = LANG_UNIVERSAL;
                            }
                            break;
                        case CHAT_MSG_GUILD:
                        case CHAT_MSG_OFFICER:
                            // allow two side chat at guild channel if two side guild allowed
                            if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_GUILD))
                            {
                                lang = LANG_UNIVERSAL;
                            }
                            break;
                    }
                }

                // but overwrite it by SPELL_AURA_MOD_LANGUAGE auras (only single case used)
                Unit::AuraList const& ModLangAuras = _player->GetAurasByType(SPELL_AURA_MOD_LANGUAGE);
                if (!ModLangAuras.empty())
                {
                    lang = ModLangAuras.front()->GetModifier()->m_miscvalue;
                }
            }

        }
    }
    else
    {
        lang = LANG_UNIVERSAL;
    }

    // Muting applies to anything the player says, emotes included. This check
    // used to sit inside the language block above, which excludes emotes
    // because they carry no language -- so a muted player could still emit
    // arbitrary text to everyone nearby through TextEmote. Away and busy are
    // status toggles rather than speech and stay exempt, as they were.
    if (type != CHAT_MSG_AFK && type != CHAT_MSG_DND)
    {
        if (!_player->CanSpeak())
        {
            std::string timeStr = secsToTimeString(m_muteTime - time(NULL));
            SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
            return;
        }

        GetPlayer()->UpdateSpeakTime();
    }

    DEBUG_LOG("CHAT: packet received. type %u lang %u", type, lang);

    switch (type)
    {
        case CHAT_MSG_SAY:
        case CHAT_MSG_EMOTE:
        case CHAT_MSG_YELL:
        {
            std::string msg;
            if (type == CHAT_MSG_SAY)
            {
                if (!MopChatPackets::ReadSayMessageRequest(recv_data, msg))
                {
                    return;
                }
            }
            else
            {
                msg = recv_data.ReadString(recv_data.ReadBits(8));
            }

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (type == CHAT_MSG_SAY)
            {
#ifdef ENABLE_ELUNA
                if (Eluna* e = sWorld.GetEluna())
                {
                    if (!e->OnChat(GetPlayer(), type, lang, msg))
                    {
                        return;
                    }
                }
#endif /* ENABLE_ELUNA */
                 GetPlayer()->Say(msg, lang);
            }
            else if (type == CHAT_MSG_EMOTE)
            {
#ifdef ENABLE_ELUNA
                if (Eluna* e = sWorld.GetEluna())
                {
                    if (!e->OnChat(GetPlayer(), type, LANG_UNIVERSAL, msg))
                    {
                        return;
                    }
                }
#endif /* ENABLE_ELUNA */
                 GetPlayer()->TextEmote(msg);
            }
            else if (type == CHAT_MSG_YELL)
            {
#ifdef ENABLE_ELUNA
                if (Eluna* e = sWorld.GetEluna())
                {
                    if (!e->OnChat(GetPlayer(), type, lang, msg))
                    {
                        return;
                    }
                }
#endif /* ENABLE_ELUNA */
                 GetPlayer()->Yell(msg, lang);
            }
         } break;

        case CHAT_MSG_WHISPER:
        {
            std::string to, msg;
            // 18414 writes the MESSAGE length first in 8 bits, then the TARGET
            // length in 9 bits (as len>>1 in eight bits plus a separate low
            // bit), then the message string, then the target string. The
            // inherited 10+9 read had both the first width and the two
            // assignments wrong, so a whisper was addressed to its own text.
            uint32 msgLength = recv_data.ReadBits(8);
            uint32 toLength = recv_data.ReadBits(9);
            msg = recv_data.ReadString(msgLength);
            to = recv_data.ReadString(toLength);

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            StripHomeRealmSuffix(to);

            if (!normalizePlayerName(to))
            {
                SendPlayerNotFoundNotice(to);
                {
                    break;
                }
            }

            Player* player = sObjectMgr.GetPlayer(to.c_str());
            uint32 tSecurity = GetSecurity();
            uint32 pSecurity = player ? player->GetSession()->GetSecurity() : SEC_PLAYER;
            if (!player || (tSecurity == SEC_PLAYER && pSecurity > SEC_PLAYER && !player->isAcceptWhispers()))
            {
                SendPlayerNotFoundNotice(to);
                return;
            }

            if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) && tSecurity == SEC_PLAYER && pSecurity == SEC_PLAYER)
            {
                if (GetPlayer()->GetTeam() != player->GetTeam())
                {
                    return;
                }
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                if (!e->OnChat(GetPlayer(), type, lang, msg, player))
                {
                    return;
                }
            }
#endif /* ENABLE_ELUNA */
            GetPlayer()->Whisper(msg, lang, player->GetObjectGuid());
        } break;

        case CHAT_MSG_PARTY:
        case CHAT_MSG_PARTY_LEADER:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // if player is in battleground, he can not say to battleground members by /p
            Group* group = GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = _player->GetGroup();
                if (!group || group->isBGGroup())
                {
                    return;
                }
            }

            if ((type == CHAT_MSG_PARTY_LEADER) && !group->IsLeader(_player->GetObjectGuid()))
            {
                return;
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                if (!e->OnChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }
            }
#endif /* ENABLE_ELUNA */

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, ChatMsg(type), msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false, group->GetMemberGroup(GetPlayer()->GetObjectGuid()));

            break;
        }
        case CHAT_MSG_GUILD:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                {
                    // Used by Eluna
#ifdef ENABLE_ELUNA
                    if (Eluna* e = sWorld.GetEluna())
                    {
                        if (!e->OnChat(GetPlayer(), type, lang, msg, guild))
                        {
                            return;
                        }
                    }
#endif /* ENABLE_ELUNA */

                    guild->BroadcastToGuild(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }

            break;
        }
        case CHAT_MSG_OFFICER:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (GetPlayer()->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(GetPlayer()->GetGuildId()))
                {
                    // Used by Eluna
#ifdef ENABLE_ELUNA
                    if (Eluna* e = sWorld.GetEluna())
                    {
                        if (!e->OnChat(GetPlayer(), type, lang, msg, guild))
                        {
                            return;
                        }
                    }
#endif /* ENABLE_ELUNA */

                    guild->BroadcastToOfficers(this, msg, lang == LANG_ADDON ? LANG_ADDON : LANG_UNIVERSAL);
                }

            break;
        }
        case CHAT_MSG_RAID:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // if player is in battleground, he can not say to battleground members by /ra
            Group* group = GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup())
                {
                    return;
                }
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                if (!e->OnChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }
            }
#endif /* ENABLE_ELUNA */

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;
        case CHAT_MSG_RAID_LEADER:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            if (msg.empty())
            {
                break;
            }

            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // if player is in battleground, he can not say to battleground members by /ra
            Group* group = GetPlayer()->GetOriginalGroup();
            if (!group)
            {
                group = GetPlayer()->GetGroup();
                if (!group || group->isBGGroup() || !group->isRaidGroup() || !group->IsLeader(_player->GetObjectGuid()))
                {
                    return;
                }
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                if (!e->OnChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }
            }
#endif /* ENABLE_ELUNA */

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_LEADER, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_RAID_WARNING:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            // A dot command typed with this destination selected has to be
            // executed, not announced. Every other destination does this; the
            // two that did not would have broadcast the command text to
            // everyone instead, which is worse than the dropped commands this
            // registration set out to fix.
            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            Group* group = GetPlayer()->GetGroup();
            if (!group || !group->isRaidGroup() ||
                !(group->IsLeader(GetPlayer()->GetObjectGuid()) || group->IsAssistant(GetPlayer()->GetObjectGuid())))
                {
                    return;
                }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                if (!e->OnChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }
            }
#endif /* ENABLE_ELUNA */

            WorldPacket data;
            // in battleground, raid warning is sent only to players in battleground - code is ok
            ChatHandler::BuildChatPacket(data, CHAT_MSG_RAID_WARNING, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup())
            {
                return;
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                if (!e->OnChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }
            }
#endif /* ENABLE_ELUNA */

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_BATTLEGROUND_LEADER:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            // battleground raid is always in Player->GetGroup(), never in GetOriginalGroup()
            Group* group = GetPlayer()->GetGroup();
            if (!group || !group->isBGGroup() || !group->IsLeader(GetPlayer()->GetObjectGuid()))
            {
                return;
            }

            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                if (!e->OnChat(GetPlayer(), type, lang, msg, group))
                {
                    return;
                }
            }
#endif /* ENABLE_ELUNA */

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, CHAT_MSG_BATTLEGROUND_LEADER, msg.c_str(), Language(lang), _player->GetChatTag(), _player->GetObjectGuid(), _player->GetName());
            group->BroadcastPacket(&data, false);
        } break;

        case CHAT_MSG_CHANNEL:
        {
            std::string channel, msg;
            // 18414 writes the CHANNEL length in 9 bits, then the MESSAGE
            // length in 8 bits, then the message string, then the channel
            // string. The string order was already right; only the two widths
            // were inherited, and 10+9 shifted every field that followed.
            uint32 channelLength = recv_data.ReadBits(9);
            uint32 msgLength = recv_data.ReadBits(8);
            msg = recv_data.ReadString(msgLength);
            channel = recv_data.ReadString(channelLength);

            // As above: execute a dot command rather than saying it into the
            // channel for everyone to read.
            if (ChatHandler(this).ParseCommands(msg.c_str()))
            {
                break;
            }

            if (!processChatmessageFurtherAfterSecurityChecks(msg, lang))
            {
                return;
            }

            if (msg.empty())
            {
                break;
            }

            if (ChannelMgr* cMgr = channelMgr(_player->GetTeam()))
            {
                if (Channel* chn = cMgr->GetChannel(channel, _player))
                {
                    // Used by Eluna
#ifdef ENABLE_ELUNA
                    if (Eluna* e = sWorld.GetEluna())
                    {
                        if (!e->OnChat(GetPlayer(), type, lang, msg, chn))
                        {
                            return;
                        }
                    }
#endif /* ENABLE_ELUNA */

                    chn->Say(_player, msg.c_str(), lang);
                }
            }
        } break;

        case CHAT_MSG_AFK:
        {
            std::string msg;
            if (!MopChatPackets::ReadAfkMessageRequest(recv_data, msg))
            {
                return;
            }

            if (!_player->IsInCombat())
            {
                if (_player->isAFK())                       // Already AFK
                {
                    if (msg.empty())
                    {
                        _player->ToggleAFK();                // Remove AFK
                    }
                    else
                    {
                        _player->autoReplyMsg = msg;         // Update message
                    }
                }
                else                                        // New AFK mode
                {
                    _player->autoReplyMsg = msg.empty() ? GetMangosString(LANG_PLAYER_AFK_DEFAULT) : msg;

                    if (_player->isDND())
                    {
                        _player->ToggleDND();
                    }

                    _player->ToggleAFK();
                }
                // Used by Eluna
#ifdef ENABLE_ELUNA
                if (Eluna* e = sWorld.GetEluna())
                {
                    if (!e->OnChat(GetPlayer(), type, lang, msg))
                    {
                        return;
                    }
                }
#endif /* ENABLE_ELUNA */
            }
            break;
        }
        case CHAT_MSG_DND:
        {
            std::string msg;
            msg = recv_data.ReadString(recv_data.ReadBits(8));

            if (_player->isDND())                           // Already DND
            {
                if (msg.empty())
                {
                    _player->ToggleDND();                    // Remove DND
                }
                else
                {
                    _player->autoReplyMsg = msg;             // Update message
                }
            }
            else                                            // New DND mode
            {
                _player->autoReplyMsg = msg.empty() ? GetMangosString(LANG_PLAYER_DND_DEFAULT) : msg;

                if (_player->isAFK())
                {
                    _player->ToggleAFK();
                }

                _player->ToggleDND();
            }
            // Used by Eluna
#ifdef ENABLE_ELUNA
            if (Eluna* e = sWorld.GetEluna())
            {
                if (!e->OnChat(GetPlayer(), type, lang, msg))
                {
                    return;
                }
            }
#endif /* ENABLE_ELUNA */

            break;
        }

        default:
            sLog.outError("CHAT: unknown message type %u, lang: %u", type, lang);
            break;
    }
}

void WorldSession::HandleAddonMessagechatOpcode(WorldPacket& recv_data)
{
    ChatMsg type;

    switch (recv_data.GetOpcode())
    {
        case CMSG_MESSAGECHAT_ADDON_INSTANCE:       type = CHAT_MSG_BATTLEGROUND;   break;
        case CMSG_MESSAGECHAT_ADDON_GUILD:          type = CHAT_MSG_GUILD;          break;
        case CMSG_MESSAGECHAT_ADDON_OFFICER:        type = CHAT_MSG_OFFICER;        break;
        case CMSG_MESSAGECHAT_ADDON_PARTY:          type = CHAT_MSG_PARTY;          break;
        case CMSG_MESSAGECHAT_ADDON_RAID:           type = CHAT_MSG_RAID;           break;
        case CMSG_MESSAGECHAT_ADDON_WHISPER:        type = CHAT_MSG_WHISPER;        break;
        default:
            sLog.outError("HandleAddonMessagechatOpcode: Unknown addon chat opcode (0x%X)", recv_data.GetOpcode());
            recv_data.rfinish();
            return;
    }

    // Disabled addon channel?
    if (!sWorld.getConfig(CONFIG_BOOL_ADDON_CHANNEL))
    {
        return;
    }

    switch (type)
    {
        case CHAT_MSG_BATTLEGROUND:
        {
            // Addon instance: 5-bit prefix length, then 8-bit message length,
            // then the message, then the prefix. Capture 38 08 "RHealBot"
            // decodes as prefix 7, message 1 -- "HealBot" with the message "R".
            uint32 prefixLen = recv_data.ReadBits(5);
            uint32 msgLen = recv_data.ReadBits(8);
            std::string msg = recv_data.ReadString(msgLen);
            std::string prefix = recv_data.ReadString(prefixLen);

            Group* group = _player->GetGroup();
            if (!group || !group->isBGGroup())
            {
                return;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, type, msg.c_str(), LANG_ADDON, CHAT_TAG_NONE,
                _player->GetObjectGuid(), _player->GetName(), ObjectGuid(), NULL, NULL, 0, prefix.c_str());
            group->BroadcastAddonMessagePacket(&data, prefix, false);
            break;
        }
        case CHAT_MSG_GUILD:
        {
            // Addon guild: 8-bit message length, then 5-bit prefix length, then
            // the PREFIX, then the message. Officer shares this layout, and
            // whisper also places its prefix before its message. Capture
            // 01 38 "HealBotG" decodes as message 1, prefix 7; reading
            // message-first would yield the prefix "ealBotG".
            uint32 msgLen = recv_data.ReadBits(8);
            uint32 prefixLen = recv_data.ReadBits(5);
            std::string prefix = recv_data.ReadString(prefixLen);
            std::string msg = recv_data.ReadString(msgLen);

            if (_player->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId()))
                {
                    guild->BroadcastAddonToGuild(this, msg, prefix);
                }

            break;
        }
        case CHAT_MSG_OFFICER:
        {
            // Addon officer has zero corpus observations, but that does not make
            // it unprovable: the client writer sub_C888C4 settles it outright.
            // It writes strlen(+16) through the 8-bit writer, strlen(+272)
            // through the 5-bit writer, flushes, then appends +272 before +16 --
            // that is message length, prefix length, prefix string, message
            // string, which is exactly the guild layout.
            uint32 msgLen = recv_data.ReadBits(8);
            uint32 prefixLen = recv_data.ReadBits(5);
            std::string prefix = recv_data.ReadString(prefixLen);
            std::string msg = recv_data.ReadString(msgLen);

            if (_player->GetGuildId())
                if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId()))
                {
                    guild->BroadcastAddonToOfficers(this, msg, prefix);
                }
            break;
        }
        case CHAT_MSG_WHISPER:
        {
            // Addon whisper: 9-bit target length, 8-bit message length, 5-bit
            // prefix length, then target, prefix, message. Capture
            // 03 00 9C "ChasisHealBotR" decodes as target 6, message 1,
            // prefix 7 -- "Chasis" + "HealBot" + "R", which totals the body
            // exactly and is the only assignment that yields a real addon
            // prefix and a plausible character name.
            uint32 targetLen = recv_data.ReadBits(9);
            uint32 msgLen = recv_data.ReadBits(8);
            uint32 prefixLen = recv_data.ReadBits(5);
            std::string targetName = recv_data.ReadString(targetLen);
            std::string prefix = recv_data.ReadString(prefixLen);
            std::string msg = recv_data.ReadString(msgLen);

            StripHomeRealmSuffix(targetName);

            if (!normalizePlayerName(targetName))
            {
                break;
            }

            Player* receiver = sObjectMgr.GetPlayer(targetName.c_str());
            if (!receiver)
            {
                break;
            }

            // Addon payloads are still cross-faction chat. The regular whisper
            // path refuses to carry them between teams when the server has
            // two-side chat disabled, and routing arbitrary data around that
            // over a second opcode would make the setting meaningless.
            if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHAT) &&
                GetSecurity() == SEC_PLAYER &&
                receiver->GetSession()->GetSecurity() == SEC_PLAYER &&
                GetPlayer()->GetTeam() != receiver->GetTeam())
            {
                break;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, type, msg.c_str(), LANG_ADDON, CHAT_TAG_NONE,
                _player->GetObjectGuid(), _player->GetName(), _player->GetObjectGuid(), _player->GetName(), NULL, 0, prefix.c_str());
            if (receiver->GetSession()->IsAddonRegistered(prefix))
                receiver->GetSession()->SendPacket(&data);
            break;
        }
        // Messages sent to "RAID" while in a party will get delivered to "PARTY"
        case CHAT_MSG_PARTY:
        case CHAT_MSG_RAID:
        {
            // Party and raid cannot share one reader: they agree on the string
            // order but reverse the two length fields.
            //
            //   raid   38 20 "VQ:0BigWigs"  prefix 7 then message 4
            //   party  04 38 "VQ:0BigWigs"  message 4 then prefix 7
            //
            // Each is the only reading that fits an 11-byte body -- taking the
            // first byte of the raid capture as an 8-bit message length gives 56,
            // and reading party prefix-first gives a message length of 135.
            uint32 prefixLen;
            uint32 msgLen;
            if (type == CHAT_MSG_RAID)
            {
                prefixLen = recv_data.ReadBits(5);
                msgLen = recv_data.ReadBits(8);
            }
            else
            {
                msgLen = recv_data.ReadBits(8);
                prefixLen = recv_data.ReadBits(5);
            }
            std::string msg = recv_data.ReadString(msgLen);
            std::string prefix = recv_data.ReadString(prefixLen);

            Group* group = _player->GetGroup();
            if (!group || group->isBGGroup())
            {
                break;
            }

            WorldPacket data;
            ChatHandler::BuildChatPacket(data, type, msg.c_str(), LANG_ADDON, CHAT_TAG_NONE,
                _player->GetObjectGuid(), _player->GetName(), ObjectGuid(), NULL, NULL, 0, prefix.c_str());
            group->BroadcastAddonMessagePacket(&data, prefix, false,
                group->GetMemberGroup(_player->GetObjectGuid()));
            break;
        }
        default:
        {
            sLog.outError("HandleAddonMessagechatOpcode: unknown addon message type %u", type);
            break;
        }
    }
}

void WorldSession::HandleUnregisterAddonPrefixesOpcode(WorldPacket& recv_data)
{
    recv_data.rfinish();
    m_registeredAddonPrefixes.clear();
}

void WorldSession::HandleAddonRegisteredPrefixesOpcode(WorldPacket& recv_data)
{
    m_filterAddonMessages = MopChatPackets::ReadAddonPrefixBatch(
        recv_data, m_registeredAddonPrefixes);
}

bool WorldSession::IsAddonRegistered(std::string const& prefix) const
{
    return !m_filterAddonMessages ||
        std::find(m_registeredAddonPrefixes.begin(),
            m_registeredAddonPrefixes.end(), prefix) !=
        m_registeredAddonPrefixes.end();
}

/**
 * @brief Handles a basic emote opcode from the client.
 *
 * @param recv_data The incoming emote packet.
 */
void WorldSession::HandleEmoteOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->IsAlive() || GetPlayer()->hasUnitState(UNIT_STAT_DIED))
    {
        return;
    }

    uint32 emote;
    recv_data >> emote;

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetPlayer()->GetEluna())
    {
        e->OnEmote(GetPlayer(), emote);
    }
#endif /* ENABLE_ELUNA */
    GetPlayer()->HandleEmoteCommand(emote);
}

namespace MaNGOS
{
    class EmoteChatBuilder
    {
        public:
            EmoteChatBuilder(Player const& pl, uint32 text_emote, uint32 emote_num, Unit const* target)
                : i_player(pl), i_text_emote(text_emote), i_emote_num(emote_num), i_target(target) {}

            void operator()(WorldPacket& data, int32 loc_idx)
            {
                (void)loc_idx;
                uint64 const targetGuid = i_target ?
                    i_target->GetObjectGuid().GetRawValue() : 0;
                MopChatPackets::BuildTextEmote(data,
                    i_player.GetObjectGuid().GetRawValue(), targetGuid,
                    i_text_emote, i_emote_num);

                DEBUG_LOG("SMSG_TEXT_EMOTE i_text_emote %u i_emote_num %u",
                    i_text_emote, i_emote_num);
            }

        private:
            Player const& i_player;
            uint32        i_text_emote;
            uint32        i_emote_num;
            Unit const*   i_target;
    };
}                                                           // namespace MaNGOS

/**
 * @brief Handles text emotes, local broadcast, and creature emote notifications.
 *
 * @param recv_data The incoming text emote packet.
 */
void WorldSession::HandleTextEmoteOpcode(WorldPacket& recv_data)
{
    if (!GetPlayer()->IsAlive())
    {
        return;
    }

    if (!GetPlayer()->CanSpeak())
    {
        std::string timeStr = secsToTimeString(m_muteTime - time(NULL));
        SendNotification(GetMangosString(LANG_WAIT_BEFORE_SPEAKING), timeStr.c_str());
        return;
    }

    MopChatPackets::TextEmoteRequest const request =
        MopChatPackets::ReadTextEmoteRequest(recv_data);
    uint32 const text_emote = request.textEmote;
    uint32 const emoteNum = request.emoteNumber;
    ObjectGuid const guid = request.targetGuid;

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetPlayer()->GetEluna())
    {
        e->OnTextEmote(GetPlayer(), text_emote, emoteNum, guid);
    }
#endif /* ENABLE_ELUNA */

    EmotesTextEntry const* em = sEmotesTextStore.LookupEntry(text_emote);
    if (!em)
    {
        return;
    }

    uint32 emote_id = em->EmoteID;

    switch (emote_id)
    {
        case EMOTE_STATE_SLEEP:
        case EMOTE_STATE_SIT:
        case EMOTE_STATE_KNEEL:
        case EMOTE_ONESHOT_NONE:
            break;
        case EMOTE_STATE_DANCE:
        case EMOTE_STATE_READ:
            GetPlayer()->SetUInt32Value(UNIT_NPC_EMOTESTATE, emote_id);
        default:
        {
            // in feign death state allowed only text emotes.
            if (GetPlayer()->hasUnitState(UNIT_STAT_DIED))
            {
                break;
            }

            GetPlayer()->HandleEmoteCommand(emote_id);
            break;
        }
    }

    Unit* unit = GetPlayer()->GetMap()->GetUnit(guid);

    MaNGOS::EmoteChatBuilder emote_builder(*GetPlayer(), text_emote, emoteNum, unit);
    MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > emote_do(emote_builder);
    MaNGOS::CameraDistWorker<MaNGOS::LocalizedPacketDo<MaNGOS::EmoteChatBuilder > > emote_worker(GetPlayer(), sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE), emote_do);
    Cell::VisitWorldObjects(GetPlayer(), emote_worker,  sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_TEXTEMOTE));

    GetPlayer()->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_DO_EMOTE, text_emote, 0, unit);

    // Send scripted event call
    if (unit && unit->GetTypeId() == TYPEID_UNIT && ((Creature*)unit)->AI())
    {
        ((Creature*)unit)->AI()->ReceiveEmote(GetPlayer(), text_emote);
    }
}

/**
 * @brief Notifies a player that their whisper target is ignoring them.
 *
 * @param recv_data The incoming ignored notification packet.
 */
void WorldSession::HandleChatIgnoredOpcode(WorldPacket& recv_data)
{
    ObjectGuid iguid;
    uint8 unk;
    // DEBUG_LOG("WORLD: Received opcode CMSG_CHAT_IGNORED");

    recv_data >> unk;                                       // probably related to spam reporting
    recv_data.ReadGuidMask<2, 5, 6, 4, 7, 0, 1, 3>(iguid);
    recv_data.ReadGuidBytes<0, 6, 5, 1, 4, 3, 7, 2>(iguid);

    Player* player = sObjectMgr.GetPlayer(iguid);
    if (!player || !player->GetSession())
    {
        return;
    }

    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_IGNORED, _player->GetName(), LANG_UNIVERSAL, CHAT_TAG_NONE,
        _player->GetObjectGuid(), _player->GetName(), _player->GetObjectGuid(), _player->GetName());
    player->GetSession()->SendPacket(&data);
}

/**
 * @brief Sends the standard player-not-found chat error.
 *
 * @param name The unresolved player name.
 */
void WorldSession::SendPlayerNotFoundNotice(const std::string& name)
{
    WorldPacket data;
    if (!MopChatPackets::BuildPlayerNotFound(data, name))
        return;
    SendPacket(&data);
}

void WorldSession::SendPlayerAmbiguousNotice(const std::string& name)
{
    WorldPacket data;
    if (!MopChatPackets::BuildPlayerAmbiguous(data, name))
        return;
    SendPacket(&data);
}

/**
 * @brief Sends the standard restricted-chat notice.
 */
void WorldSession::SendChatRestrictedNotice(ChatRestrictionType restriction)
{
    WorldPacket data;
    MopChatPackets::BuildChatRestrictedNotice(data, uint8(restriction));
    SendPacket(&data);
}
