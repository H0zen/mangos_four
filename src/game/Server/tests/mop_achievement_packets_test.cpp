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

static bool ExpectPrefix(WorldPacket const& packet, std::array<uint8_t, 3> const& expected)
{
    if (packet.size() < expected.size())
    {
        std::fprintf(stderr, "  size %u, wanted at least %u\n",
                     unsigned(packet.size()), unsigned(expected.size()));
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

static void test_empty_lists()
{
    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 5);
    MopAchievementPackets::BuildAllAchievementData(packet, {}, {});
    CHECK(ExpectBytes(packet, { 0x00, 0x00, 0x00, 0x00, 0x00 }));
}

static void test_completed_sparse_guid()
{
    std::vector<MopAchievementPackets::CompletedAchievement> completed = {
        { 0x11223344u, UINT64_C(0x0077005500330011), 0xDDEEFF00u,
          0x55667788u, 0x99AABBCCu }
    };

    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 26);
    MopAchievementPackets::BuildAllAchievementData(packet, completed, {});
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x00, 0x00, 0x03, 0x1C,
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99, 0x00, 0xFF, 0xEE, 0xDD,
        0x10, 0x54, 0x76, 0x32
    }));
}

static void test_progress_sparse_guids()
{
    std::vector<MopAchievementPackets::CriteriaProgress> progress = {
        { 0x12345678u, UINT64_C(0x8800660044002200),
          UINT64_C(0x0077005500330011), 0x90ABCDEFu,
          0x10203040u, 0x50607080u }
    };

    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 32);
    MopAchievementPackets::BuildAllAchievementData(packet, {}, progress);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x34, 0xEE, 0x00, 0x00, 0x00, 0x00,
        0x89, 0x40, 0x30, 0x20, 0x10, 0x78, 0x56, 0x34,
        0x12, 0x10, 0x54, 0x76, 0x23, 0x67, 0x32, 0x45,
        0x80, 0x70, 0x60, 0x50, 0xEF, 0xCD, 0xAB, 0x90
    }));
}

static void test_completed_bytes_precede_progress_bytes_with_zero_guids()
{
    std::vector<MopAchievementPackets::CompletedAchievement> completed = {
        { 0x11223344u, 0, 0xDDEEFF00u, 0x55667788u, 0x99AABBCCu }
    };
    std::vector<MopAchievementPackets::CriteriaProgress> progress = {
        { 0x12345678u, 0, 0, 0x90ABCDEFu, 0x10203040u, 0x50607080u }
    };

    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 41);
    MopAchievementPackets::BuildAllAchievementData(packet, completed, progress);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00,
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99, 0x00, 0xFF, 0xEE, 0xDD,
        0x40, 0x30, 0x20, 0x10, 0x78, 0x56, 0x34, 0x12,
        0x80, 0x70, 0x60, 0x50, 0xEF, 0xCD, 0xAB, 0x90
    }));
}

static void test_all_guid_bytes_nonzero()
{
    std::vector<MopAchievementPackets::CompletedAchievement> completed = {
        { 0x01020304u, UINT64_C(0x0807060504030201), 0x31323334u,
          0x11121314u, 0x21222324u }
    };
    std::vector<MopAchievementPackets::CriteriaProgress> progress = {
        { 0x41424344u, UINT64_C(0x100F0E0D0C0B0A09),
          UINT64_C(0x1817161514131211), 0x51525354u,
          0x61626364u, 0x71727374u }
    };

    WorldPacket packet(SMSG_ALL_ACHIEVEMENT_DATA, 65);
    MopAchievementPackets::BuildAllAchievementData(packet, completed, progress);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x3F, 0xFF, 0xC2, 0x00, 0x00, 0x3F, 0xE0,
        0x04, 0x03, 0x02, 0x01, 0x14, 0x13, 0x12, 0x11, 0x07,
        0x09, 0x24, 0x23, 0x22, 0x21, 0x34, 0x33, 0x32, 0x31,
        0x00, 0x04, 0x03, 0x06, 0x02, 0x05,
        0x11, 0x64, 0x63, 0x62, 0x61, 0x0E, 0x13,
        0x44, 0x43, 0x42, 0x41, 0x0C, 0x10, 0x14, 0x16,
        0x0B, 0x0F, 0x19, 0x12, 0x0A, 0x08, 0x15, 0x0D,
        0x74, 0x73, 0x72, 0x71, 0x17, 0x54, 0x53, 0x52, 0x51
    }));
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

static void test_achievement_earned_full_interleave_and_scalars()
{
    // Binary-derived synthetic coverage, not captured retail evidence. Distinct,
    // all-nonzero GUIDs exercise every XOR-1 byte and scalar boundary.
    WorldPacket packet(SMSG_ACHIEVEMENT_EARNED, 35);
    MopAchievementPackets::BuildAchievementEarned(packet,
        UINT64_C(0x0807060504030201), UINT64_C(0x1817161514131211), true,
        0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u);
    CHECK(ExpectBytes(packet, {
        0xFF, 0xFF, 0x80, 0x17, 0x05, 0x16, 0x06, 0x44, 0x33, 0x22,
        0x11, 0x13, 0x02, 0x00, 0x09, 0x15, 0x04, 0x19, 0x88, 0x77,
        0x66, 0x55, 0x14, 0x03, 0x10, 0x07, 0xCC, 0xBB, 0xAA, 0x99,
        0x00, 0xFF, 0xEE, 0xDD, 0x12
    }));
}

static void test_achievement_earned_boolean_bit()
{
    // Binary-derived synthetic coverage. No captured fixture exercises true;
    // holding every other input constant isolates alreadyEarned at mask bit 6.
    WorldPacket packet(SMSG_ACHIEVEMENT_EARNED, 35);
    MopAchievementPackets::BuildAchievementEarned(packet,
        UINT64_C(0x0807060504030201), UINT64_C(0x1817161514131211), false,
        0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u);
    CHECK(ExpectPrefix(packet, { 0xFD, 0xFF, 0x80 }));
}

static void test_achievement_earned_mask_order_complements()
{
    // Synthetic coverage cases. One zero byte at a time uniquely assigns all
    // sixteen GUID presence bits; the all-nonzero fixture alone cannot do that.
    uint64 const guid1 = UINT64_C(0x0807060504030201);
    uint64 const guid2 = UINT64_C(0x1817161514131211);
    std::array<std::array<uint8_t, 3>, 8> const guid1Masks = {{
        {{ 0xF5, 0xFF, 0x80 }}, {{ 0xFD, 0xBF, 0x80 }},
        {{ 0xFD, 0xFE, 0x80 }}, {{ 0xF9, 0xFF, 0x80 }},
        {{ 0xDD, 0xFF, 0x80 }}, {{ 0xED, 0xFF, 0x80 }},
        {{ 0xFD, 0xFB, 0x80 }}, {{ 0xFD, 0x7F, 0x80 }}
    }};
    std::array<std::array<uint8_t, 3>, 8> const guid2Masks = {{
        {{ 0xFD, 0xEF, 0x80 }}, {{ 0xFD, 0xFD, 0x80 }},
        {{ 0xBD, 0xFF, 0x80 }}, {{ 0xFD, 0xDF, 0x80 }},
        {{ 0xFD, 0xF7, 0x80 }}, {{ 0xFD, 0xFF, 0x00 }},
        {{ 0x7D, 0xFF, 0x80 }}, {{ 0xFC, 0xFF, 0x80 }}
    }};

    for (size_t index = 0; index < 8; ++index)
    {
        uint64 const byteMask = ~(UINT64_C(0xFF) << (8 * index));

        WorldPacket first(SMSG_ACHIEVEMENT_EARNED, 34);
        MopAchievementPackets::BuildAchievementEarned(first,
            guid1 & byteMask, guid2, false, 0, 0, 0, 0);
        CHECK(first.size() == 34);
        CHECK(ExpectPrefix(first, guid1Masks[index]));

        WorldPacket second(SMSG_ACHIEVEMENT_EARNED, 34);
        MopAchievementPackets::BuildAchievementEarned(second,
            guid1, guid2 & byteMask, false, 0, 0, 0, 0);
        CHECK(second.size() == 34);
        CHECK(ExpectPrefix(second, guid2Masks[index]));
    }
}

static void test_achievement_earned_preserves_caller_opcode()
{
    // This proves only the body-builder contract: the caller owns opcode choice.
    WorldPacket packet(SMSG_TITLE_EARNED, 35);
    MopAchievementPackets::BuildAchievementEarned(packet,
        UINT64_C(0x0807060504030201), UINT64_C(0x1817161514131211), false,
        0x11223344u, 0x55667788u, 0x99AABBCCu, 0xDDEEFF00u);
    CHECK(packet.GetOpcode() == SMSG_TITLE_EARNED);
}

static void test_achievement_deleted_body()
{
    // Binary-derived synthetic coverage, not captured retail evidence. The
    // build-18414 client consumes two uint32 values, uses the achievement ID,
    // and never reads the second word, so the producer writes a stable zero.
    WorldPacket packet(SMSG_ACHIEVEMENT_DELETED, 8);
    MopAchievementPackets::BuildAchievementDeleted(packet, 0x11223344u);
    CHECK(ExpectBytes(packet, {
        0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0x00, 0x00
    }));
}

static void test_achievement_deleted_preserves_caller_opcode()
{
    // This proves only the body-builder contract: the caller owns opcode choice.
    WorldPacket packet(SMSG_TITLE_EARNED, 8);
    MopAchievementPackets::BuildAchievementDeleted(packet, 0x11223344u);
    CHECK(packet.GetOpcode() == SMSG_TITLE_EARNED);
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32_t(SMSG_ALL_ACHIEVEMENT_DATA) == 0x180Au);
    CHECK(uint32_t(SMSG_ALL_ACHIEVEMENT_DATA) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_ACHIEVEMENT_EARNED) == 0x080Bu);
    CHECK(uint32_t(SMSG_ACHIEVEMENT_EARNED) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_ACHIEVEMENT_DELETED) == 0x1A2Fu);
    CHECK(uint32_t(SMSG_ACHIEVEMENT_DELETED) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_empty_lists();
    test_completed_sparse_guid();
    test_progress_sparse_guids();
    test_completed_bytes_precede_progress_bytes_with_zero_guids();
    test_all_guid_bytes_nonzero();
    test_achievement_earned_captured_bodies();
    test_achievement_earned_full_interleave_and_scalars();
    test_achievement_earned_boolean_bit();
    test_achievement_earned_mask_order_complements();
    test_achievement_earned_preserves_caller_opcode();
    test_achievement_deleted_body();
    test_achievement_deleted_preserves_caller_opcode();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_achievement_packets: all checks passed\n");
    return 0;
}
