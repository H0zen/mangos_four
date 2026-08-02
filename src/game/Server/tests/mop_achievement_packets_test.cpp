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
 * Byte-exact tests for the 5.4.8 achievement packet bodies.
 */

#include "AchievementMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet, std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "  size %u, wanted %u\n", unsigned(packet.size()), unsigned(expected.size()));
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "  byte %u = 0x%02X, wanted 0x%02X\n",
                         unsigned(i), packet.contents()[i], expected[i]);
            return false;
        }
    }
    return true;
}







static void test_achievement_earned_captured_bodies()
{
    // Captured retail body: build 18414, catalogue 2BE10C89,
    // capture-000006 sequence 48502. Independent reader decode consumed 29/29.
    // Because g1 == g2, this fixture does not distinguish the two GUID roles.
    WorldPacket first(SMSG_ACHIEVEMENT_EARNED, 29);
    MopAchievementPackets::BuildAchievementEarned(first,
        UINT64_C(0x04000000054829D1), UINT64_C(0x04000000054829D1), false,
        0x0E634C13u, 5291u, 0x03010018u, 0x0304000Du);
    CHECK(ExpectBytes(first, {
        0x4D, 0xF3, 0x00, 0x04, 0x13, 0x4C, 0x63, 0x0E, 0x28, 0x49,
        0xD0, 0x05, 0x04, 0x05, 0xAB, 0x14, 0x00, 0x00, 0x28, 0xD0,
        0x18, 0x00, 0x01, 0x03, 0x0D, 0x00, 0x04, 0x03, 0x49
    }));

    // Captured retail body: build 18414, catalogue 2BE10C89,
    // capture-000135 sequence 26505. Independent reader decode consumed 24/24.
    // It distinguishes g1 from g2, but does not establish their semantic names.
    WorldPacket second(SMSG_ACHIEVEMENT_EARNED, 24);
    MopAchievementPackets::BuildAchievementEarned(second,
        0, UINT64_C(0x04000000053CC8E8), false,
        0x0E675AB2u, 6616u, 0, 0);
    CHECK(ExpectBytes(second, {
        0x41, 0x32, 0x00, 0xB2, 0x5A, 0x67, 0x0E, 0xC9, 0x04, 0x05,
        0xD8, 0x19, 0x00, 0x00, 0xE9, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x3D
    }));
}







static void test_criteria_update_captured_mode_9_body()
{
    // Captured retail body: build 18414, catalogue 2BE10C89,
    // capture-000692 sequence 34066. Independent decode consumed 34/34.
    // It proves this exact removal/reset body and grammar, not a universal
    // producer selector.
    WorldPacket packet(SMSG_CRITERIA_UPDATE, 34);
    MopAchievementPackets::BuildCriteriaUpdate(packet,
        UINT64_C(0x06000000072D4F03), 20976u, UINT64_C(0), 9u,
        0x0E06334Au, 1411156734u, 59940u);
    CHECK(ExpectBytes(packet, {
        0x3D, 0x06, 0x2C, 0xF0, 0x51, 0x00, 0x00, 0x09, 0x00, 0x00,
        0x00, 0x4E, 0x4A, 0x33, 0x06, 0x0E, 0xFE, 0x8A, 0x1C, 0x54,
        0x24, 0xEA, 0x00, 0x00, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));
}








int main(int /*argc*/, char** /*argv*/)
{
    test_achievement_earned_captured_bodies();
    test_criteria_update_captured_mode_9_body();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_achievement_packets: all checks passed\n");
    return 0;
}
