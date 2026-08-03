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

/**
 * Focused structural regression test for the MoP 5.4.8.18414 self-player
 * SMSG_UPDATE_OBJECT create block.
 */

#include "MopUpdateObject.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

namespace
{
    int NonZeroGuidBytes(uint64 guid)
    {
        int count = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (uint8(guid >> (i * 8)))
            {
                ++count;
            }
        }
        return count;
    }

    MopUpdateObject::SelfPlayer MakeSelf()
    {
        MopUpdateObject::SelfPlayer player{};
        player.guid = 0x10;
        player.mapId = 0;
        player.x = -8913.5f;
        player.y = 554.6f;
        player.z = 93.7f;
        player.o = 3.14f;
        player.moveTime = 0x11223344u;
        player.speedWalk = 1.0f;
        player.speedRun = 7.0f;
        player.speedRunBack = 4.5f;
        player.speedSwim = 4.7222f;
        player.speedSwimBack = 2.5f;
        player.speedFlight = 7.0f;
        player.speedFlightBack = 4.5f;
        player.speedTurn = 3.1415f;
        player.speedPitch = 3.1415f;
        player.race = 1;
        player.class_ = 2;
        player.gender = 0;
        player.powerType = 3;
        player.health = 100;
        player.maxHealth = 120;
        for (uint32 i = 0; i < MAX_STORED_POWERS; ++i)
        {
            player.power[i] = 50 + i;
            player.maxPower[i] = 60 + i;
        }
        player.level = 1;
        player.faction = 1;
        // Four keeps bit 0 internally while GM mode is enabled. The 18414
        // client treats it as server-controlled and refuses to parent that
        // player to an MO_TRANSPORT, so the self projection must omit it.
        player.unitFlags = 0x00000009u;
        player.scale = 1.0f;
        player.boundingRadius = 0.388f;
        player.combatReach = 1.5f;
        player.displayId = 19724;
        player.nativeDisplayId = 19724;
        return player;
    }
}
int main(int /*argc*/, char** /*argv*/)
{
    // capture-000188 / sequence 1453: a type-15 MO_TRANSPORT must reach the
    // stationary create serializer before its client-interpolated route can run.
    MopUpdateObject::StationaryGameObjectEligibility transportEligibility{};
    transportEligibility.hasTemplate = true;
    transportEligibility.isTransport = true;
    transportEligibility.hasStationaryPosition = true;
    transportEligibility.hasRotation = true;
    CHECK(MopUpdateObject::CanUseStationaryGameObjectMovement(transportEligibility));

    MopUpdateObject::StationaryGameObjectMovement transportMovement{};
    uint32 const capturedOrientation = 0x3FC0441Cu;
    std::memcpy(&transportMovement.o, &capturedOrientation, sizeof(capturedOrientation));
    transportMovement.transportTime = 0x783D4FA1u;
    transportMovement.rotation = uint64(0x00000000000AEB1Bull);
    transportMovement.isTransport = true;

    ByteBuffer transportBody;
    MopUpdateObject::AppendStationaryGameObjectMovement(transportBody, transportMovement);
    static uint8 const capturedTransportBody[] = {
        0x00, 0x00, 0x00, 0x03, 0x00, 0x40,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x1C, 0x44, 0xC0, 0x3F,
        0x00, 0x00, 0x00, 0x00,
        0xA1, 0x4F, 0x3D, 0x78,
        0x1B, 0xEB, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    CHECK(transportBody.size() == sizeof(capturedTransportBody));
    CHECK(transportBody.size() == 0 ||
        std::memcmp(transportBody.contents(), capturedTransportBody, transportBody.size()) == 0);

    MopUpdateObject::SelfPlayer const player = MakeSelf();
    WorldPacket packet;
    MopUpdateObject::BuildSelfCreate(packet, player);

    int const guidByteCount = NonZeroGuidBytes(player.guid);
    size_t const expectedSize =
        6 + (3 + guidByteCount) + 13 + (guidByteCount + 13 * 4 + 4) + 114;
    CHECK(packet.GetOpcode() == SMSG_UPDATE_OBJECT);
    CHECK(packet.size() == expectedSize);

    packet.rpos(0);
    uint16 mapId = 0;
    uint32 objectCount = 0;
    packet >> mapId >> objectCount;
    CHECK(mapId == player.mapId);
    CHECK(objectCount == 1);

    uint8 updateType = 0;
    uint8 guidMask = 0;
    packet >> updateType >> guidMask;
    uint64 decodedGuid = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (guidMask & (1 << i))
        {
            uint8 byte = 0;
            packet >> byte;
            decodedGuid |= uint64(byte) << (i * 8);
        }
    }
    uint8 typeId = 0;
    packet >> typeId;
    CHECK(updateType == 2);
    CHECK(decodedGuid == player.guid);
    CHECK(typeId == 4);

    packet.rpos(packet.size() - 114);
    uint8 blockCount = 0;
    packet >> blockCount;
    CHECK(blockCount == 3);

    uint32 mask[3] = {};
    for (uint32& word : mask)
    {
        packet >> word;
    }
    auto hasBit = [&mask](int index)
    {
        return (mask[index / 32] >> (index % 32)) & 1u;
    };
    CHECK(hasBit(0));
    CHECK(hasBit(33));
    CHECK(hasBit(61));
    CHECK(hasBit(69));
    CHECK(hasBit(70));

    uint32 fields[25] = {};
    for (uint32& field : fields)
    {
        packet >> field;
    }
    CHECK(fields[0] == uint32(player.guid));
    CHECK(fields[2] == 25u);
    CHECK(fields[4] == (uint32(player.race) |
                        (uint32(player.class_) << 8) |
                        (uint32(player.powerType) << 16) |
                        (uint32(player.gender) << 24)));
    CHECK(fields[6] == player.health);
    CHECK(fields[12] == player.maxHealth);
    CHECK(fields[18] == player.level);
    CHECK(fields[20] == 0x00000008u);
    CHECK(fields[23] == player.displayId);
    CHECK(fields[24] == player.nativeDisplayId);

    uint8 dynamicFieldCount = 0;
    packet >> dynamicFieldCount;
    CHECK(dynamicFieldCount == 0);
    CHECK(packet.rpos() == packet.size());

    MopUpdateObject::StaticField const changedUnitFlags[] = {
        { 55, 0x00000009u }
    };
    std::vector<MopUpdateObject::StaticField> projectedUnitFlags;
    MopUpdateObject::TranslateSelfPlayerFields(changedUnitFlags, 1,
        projectedUnitFlags);
    CHECK(projectedUnitFlags.size() == 1);
    CHECK(projectedUnitFlags[0].index == 61);
    CHECK(projectedUnitFlags[0].value == 0x00000008u);

    // Unit flags must reach OBSERVERS too, or another player's combat, stun,
    // fear and silence state never updates after they come into view. The
    // value has to go through the same projection as the self path: bit 0 is
    // Four's internal GM mode and must not leak to other players.
    uint16 observerFlagsIndex = 0;
    CHECK(MopUpdateObject::TranslateObserverPlayerIndex(55, observerFlagsIndex));
    CHECK(observerFlagsIndex == 61);
    CHECK(MopUpdateObject::ProjectPlayerUnitFlags(0x00000009u) == 0x00000008u);
    CHECK(MopUpdateObject::ProjectPlayerUnitFlags(0x00000000u) == 0x00000000u);
    CHECK(MopUpdateObject::ProjectPlayerUnitFlags(0xFFFFFFFFu) == 0xFFFFFFFEu);

    // Current target, so target-of-target and "who is targeting me" work.
    // Client 22/23 are CGUnitData::target; the two-word GUID must translate as
    // a contiguous ascending pair, since SetUInt64Value dirties both words and
    // the serializer asserts ascending indices.
    uint16 observerTargetLo = 0;
    uint16 observerTargetHi = 0;
    CHECK(MopUpdateObject::TranslateObserverPlayerIndex(20, observerTargetLo));
    CHECK(MopUpdateObject::TranslateObserverPlayerIndex(21, observerTargetHi));
    CHECK(observerTargetLo == 22);
    CHECK(observerTargetHi == 23);
    CHECK(observerTargetHi == observerTargetLo + 1);

    // Stand state / animation tier must reach observers too. Client index 76
    // is CGUnitData::animTier, whose byte 0 is the stand state. Creatures got
    // it; players never did, so a watcher could not see another player sit or
    // hold a looping emote even once emote state itself was delivered.
    uint16 observerBytes1Index = 0;
    CHECK(MopUpdateObject::TranslateObserverPlayerIndex(70, observerBytes1Index));
    CHECK(observerBytes1Index == 76);

    // Emote state maps to client index 89 on the incremental paths. The
    // observer CREATE deliberately omits it so the client's cached value stays
    // zero and the post-create refresh is a real 0 -> N transition; the client
    // runs the animation callback only on a changed value.
    MopUpdateObject::StaticField const changedEmoteState[] = {
        { 83, 0x000001E3u }
    };
    std::vector<MopUpdateObject::StaticField> projectedEmoteState;
    MopUpdateObject::TranslateSelfPlayerFields(changedEmoteState, 1,
        projectedEmoteState);
    CHECK(projectedEmoteState.size() == 1);
    CHECK(projectedEmoteState[0].index == 89);
    CHECK(projectedEmoteState[0].value == 0x000001E3u);

    // AppendStaticValuesNoDynamic asserts that projected indices ascend, so
    // 83 has to land between the mount display id and PLAYER_FLAGS. Getting
    // that wrong is a runtime assert, not a wrong pixel.
    MopUpdateObject::StaticField const changedAroundEmote[] = {
        { 65, 0x00000010u },
        { 83, 0x00000000u },
        { 157, 0x00000020u }
    };
    std::vector<MopUpdateObject::StaticField> projectedAroundEmote;
    MopUpdateObject::TranslateSelfPlayerFields(changedAroundEmote, 3,
        projectedAroundEmote);
    CHECK(projectedAroundEmote.size() == 3);
    CHECK(projectedAroundEmote[0].index == 71);
    CHECK(projectedAroundEmote[1].index == 89);
    CHECK(projectedAroundEmote[2].index == 162);
    // A cleared emote state is the whole point: zero is what ends the loop.
    CHECK(projectedAroundEmote[1].value == 0x00000000u);

    uint16 lastProjectedIndex = 0;
    for (size_t i = 0; i < projectedAroundEmote.size(); ++i)
    {
        CHECK(i == 0 || projectedAroundEmote[i].index > lastProjectedIndex);
        lastProjectedIndex = projectedAroundEmote[i].index;
    }

    // The observer path is a separate table; without it a watcher keeps
    // rendering the emote after the actor has stopped.
    uint16 observerEmoteIndex = 0;
    CHECK(MopUpdateObject::TranslateObserverPlayerIndex(83, observerEmoteIndex));
    CHECK(observerEmoteIndex == 89);

    if (g_failures)
    {
        std::printf("%d FAILURES\n", g_failures);
        return 1;
    }

    std::puts("mop_updateobject: all checks passed");
    return 0;
}
