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
        player.unitFlags = 0x00000008u;
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
    CHECK(fields[20] == player.unitFlags);
    CHECK(fields[23] == player.displayId);
    CHECK(fields[24] == player.nativeDisplayId);

    uint8 dynamicFieldCount = 0;
    packet >> dynamicFieldCount;
    CHECK(dynamicFieldCount == 0);
    CHECK(packet.rpos() == packet.size());

    if (g_failures)
    {
        std::printf("%d FAILURES\n", g_failures);
        return 1;
    }

    std::puts("mop_updateobject: all checks passed");
    return 0;
}
