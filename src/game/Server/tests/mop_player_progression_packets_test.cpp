/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for directly verified 5.4.8 player-progression packets.
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void CheckBytes(WorldPacket const& packet,
    std::initializer_list<uint8_t> expected)
{
    CHECK(packet.size() == expected.size());
    size_t index = 0;
    for (uint8_t byte : expected)
    {
        if (index < packet.size())
            CHECK(packet[index] == byte);
        ++index;
    }
}

static void TestLevelUpInfo()
{
    MopProgressionPackets::LevelUpInfo info;
    info.talentDelta = 0x01020304u;
    info.healthDelta = 0x11121314u;
    info.statDeltas = {{
        0x21222324u, 0x31323334u, 0x41424344u,
        0x51525354u, 0x61626364u,
    }};
    info.level = 0x71727374u;
    info.powerDeltas = {{
        0x81828384u, 0x91929394u, 0xA1A2A3A4u,
        0xB1B2B3B4u, 0xC1C2C3C4u,
    }};

    WorldPacket packet;
    MopProgressionPackets::BuildLevelUpInfo(packet, info);
    CHECK(packet.GetOpcode() == SMSG_LEVELUP_INFO);
    CheckBytes(packet, {
        0x04, 0x03, 0x02, 0x01,
        0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21,
        0x34, 0x33, 0x32, 0x31,
        0x44, 0x43, 0x42, 0x41,
        0x54, 0x53, 0x52, 0x51,
        0x64, 0x63, 0x62, 0x61,
        0x74, 0x73, 0x72, 0x71,
        0x84, 0x83, 0x82, 0x81,
        0x94, 0x93, 0x92, 0x91,
        0xA4, 0xA3, 0xA2, 0xA1,
        0xB4, 0xB3, 0xB2, 0xB1,
        0xC4, 0xC3, 0xC2, 0xC1,
    });
}

int main()
{
    TestLevelUpInfo();
    if (g_fail != 0)
        return 1;

    std::printf("mop_player_progression_packets: all checks passed\n");
    return 0;
}
