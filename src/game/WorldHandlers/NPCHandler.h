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

#ifndef MANGOS_H_NPCHANDLER
#define MANGOS_H_NPCHANDLER

#include "ObjectGuid.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>

namespace MopNpcTextPackets
{
    struct Request
    {
        uint32 textId = 0;
        ObjectGuid sourceGuid;
    };

    struct Response
    {
        uint32 textId = 0;
        bool found = false;
        std::array<float, 8> probabilities = {};
        std::array<uint32, 8> broadcastTextIds = {};
    };

    inline bool RejectRequest(WorldPacket& in)
    {
        in.rfinish();
        return false;
    }

    inline size_t PresentByteCount(uint8 mask)
    {
        size_t count = 0;
        for (; mask != 0; mask >>= 1)
        {
            count += mask & 1;
        }
        return count;
    }

    inline bool ParseRequest(WorldPacket& in, Request& request)
    {
        if (in.size() - in.rpos() < 5)
        {
            return RejectRequest(in);
        }

        uint8 const mask = in[in.rpos() + 4];
        if (in.size() - in.rpos() != 5 + PresentByteCount(mask))
        {
            return RejectRequest(in);
        }

        Request parsed;
        in >> parsed.textId;
        in.ReadGuidMask<4, 5, 1, 7, 0, 2, 6, 3>(parsed.sourceGuid);
        in.ReadGuidBytes<4, 0, 2, 5, 1, 7, 3, 6>(parsed.sourceGuid);
        if (in.rpos() != in.size())
        {
            return RejectRequest(in);
        }

        request = parsed;
        return true;
    }

    inline void BuildResponse(WorldPacket& out, Response const& response)
    {
        uint32 const recordSize = response.found ? 64 : 0;
        out.Initialize(SMSG_NPC_TEXT_UPDATE, 9 + recordSize);
        out << response.textId << recordSize;
        if (response.found)
        {
            for (float probability : response.probabilities)
            {
                out << probability;
            }
            for (uint32 broadcastTextId : response.broadcastTextIds)
            {
                out << broadcastTextId;
            }
        }

        // The final bit controls insertion into the client's WNPC cache.
        out.WriteBit(response.found);
        out.FlushBits();
    }
}

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct PageText
{
    uint32 Page_ID;
    char* Text;

    uint32 Next_Page;
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

struct PageTextLocale
{
    std::vector<std::string> Text;
};

struct NpcTextLocale
{
    NpcTextLocale() { Text_0.resize(8); Text_1.resize(8); }

    std::vector<std::vector<std::string> > Text_0;
    std::vector<std::vector<std::string> > Text_1;
};

struct QEmote
{
    uint32 _Emote;
    uint32 _Delay;
};

struct GossipTextOption
{
    std::string Text_0;
    std::string Text_1;
    // Build 18414 resolves NPC text client-side through BroadcastText.db2.
    uint32 BroadcastTextId = 0;
    uint32 Language;
    float Probability;
    QEmote Emotes[3];
};

#define MAX_GOSSIP_TEXT_OPTIONS 8

struct GossipText
{
    GossipTextOption Options[MAX_GOSSIP_TEXT_OPTIONS];
};

namespace MopNpcTextPackets
{
    // The 18414 client resolves a BroadcastText id against its own
    // BroadcastText.db2 and, when that fails, asks the server for the record
    // over CMSG_REQUEST_HOTFIX. That second route is how retail delivered text
    // the client never shipped: the client carries 936 records, while retail
    // captures reference 1,707 distinct ids, only 4 of which the client has.
    //
    // Measured on the live client, the id decides everything. Zero is refused,
    // an id the client cannot resolve is refused once the hotfix goes
    // unanswered, and an id it can resolve opens the window. So an unmapped
    // row needs an id we invent and then serve ourselves.
    //
    // Inventing one makes collision the hazard: the client's own records span
    // 1..77161, and landing on one would silently render its Mists text for a
    // Classic npc_text row -- wrong text, no error, very hard to trace. The
    // base clears that range entirely, and the static_assert keeps it cleared
    // if anyone revisits the scheme.
    static constexpr uint32 ClientHighestShippedBroadcastTextId = 77161;
    static constexpr uint32 SynthesisedBroadcastTextBase = 100000;
    static_assert(SynthesisedBroadcastTextBase >
        ClientHighestShippedBroadcastTextId,
        "synthesised BroadcastText ids must not collide with the records the "
        "client ships, or it renders its own text instead of ours");

    // One id per option, so a row's eight alternatives stay distinguishable,
    // and the mapping inverts for debugging.
    inline uint32 SynthesiseBroadcastTextId(uint32 textId, uint32 option)
    {
        return SynthesisedBroadcastTextBase +
            textId * MAX_GOSSIP_TEXT_OPTIONS + option;
    }

    inline bool DecodeSynthesisedBroadcastTextId(uint32 id, uint32& textId,
        uint32& option)
    {
        if (id < SynthesisedBroadcastTextBase)
        {
            return false;
        }
        uint32 const packed = id - SynthesisedBroadcastTextBase;
        textId = packed / MAX_GOSSIP_TEXT_OPTIONS;
        option = packed % MAX_GOSSIP_TEXT_OPTIONS;
        return true;
    }

    // Whether MakeResponse would actually have minted this id for this option.
    //
    // Sitting above the base is not proof of authorship: nothing bounds the
    // synthesised namespace from above, so a real BroadcastText id larger than
    // the base decodes to some unrelated npc_text row. Answering that with the
    // row's text would be worse than not answering at all, because a plausible
    // record produces a wrong string with nothing to trace it by. An option
    // only owns an id when it carries text and had no real mapping -- a mapped
    // option travels as its retail id and never as a synthesised one.
    inline bool OwnsSynthesisedBroadcastTextId(uint32 entry, uint32 textId,
        uint32 option, GossipTextOption const& candidate)
    {
        return candidate.BroadcastTextId == 0 &&
            (!candidate.Text_0.empty() || !candidate.Text_1.empty()) &&
            SynthesiseBroadcastTextId(textId, option) == entry;
    }

    inline Response MakeResponse(uint32 textId, GossipText const* gossip)
    {
        Response response;
        response.textId = textId;
        if (!gossip)
        {
            return response;
        }

        for (size_t index = 0; index < MAX_GOSSIP_TEXT_OPTIONS; ++index)
        {
            GossipTextOption const& option = gossip->Options[index];
            uint32 broadcastTextId = option.BroadcastTextId;

            // A real mapping is kept as-is: where the world database knows the
            // retail id, the client may already hold that record and needs no
            // hotfix. Otherwise an option that carries text gets an invented
            // id, which SendBroadcastTextDb2Reply answers.
            if (!broadcastTextId &&
                (!option.Text_0.empty() || !option.Text_1.empty()))
            {
                broadcastTextId = SynthesiseBroadcastTextId(textId,
                    uint32(index));
            }

            response.broadcastTextIds[index] = broadcastTextId;

            // A probability without an id lets the client select a blank
            // record, so an option with nothing behind it must not participate.
            response.probabilities[index] = broadcastTextId
                ? option.Probability
                : 0.0f;
            response.found = response.found || broadcastTextId != 0;
        }

        return response;
    }
}

#endif
