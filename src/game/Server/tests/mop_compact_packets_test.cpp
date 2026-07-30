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
 * Byte-exact tests for compact 5.4.8 server packet bodies recovered from the
 * client readers at 0x6F568B, 0xC8CBBE, 0x6D18F6, 0x94E111, 0xCCDD26,
 * and 0x6D9F28.
 */

#include "Player.h"
#include "InstanceData.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// InstanceData is exported on Windows, so merely including its owning header emits
// its vtable in this standalone fixture. These two policy hooks are unrelated to the
// inline packet builder under test; provide inert fixture definitions instead of
// linking the complete game library (and its database process globals).
bool InstanceData::CheckAchievementCriteriaMeet(uint32, Player const*, Unit const*, uint32) const
{
    return false;
}

bool InstanceData::CheckConditionCriteriaMeet(Player const*, uint32, WorldObject const*, uint32) const
{
    return false;
}

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool BytesEqual(WorldPacket const& packet, std::vector<uint8_t> const& expected)
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

static void test_attack_swing_reasons()
{
    const uint8_t expected[] = { 0x00, 0x40, 0x80, 0xC0 };
    for (uint8_t reason = 0; reason < 4; ++reason)
    {
        WorldPacket packet(SMSG_ATTACKSWING_ERROR, 1);
        MopCompactPackets::BuildAttackSwingError(packet, reason);
        CHECK(BytesEqual(packet, { expected[reason] }));
        CHECK(packet.GetOpcode() == SMSG_ATTACKSWING_ERROR);
    }
}

static void test_attack_packets()
{
    uint64_t const attacker = UINT64_C(0x0002030005060008);
    uint64_t const victim = UINT64_C(0x1100334400667700);

    WorldPacket start;
    MopCompactPackets::BuildAttackStart(start, attacker, victim);
    CHECK(start.GetOpcode() == SMSG_ATTACKSTART);
    CHECK(BytesEqual(start, {
        0xA9, 0xBE,
        0x02, 0x09, 0x32, 0x03, 0x76,
        0x45, 0x07, 0x10, 0x67, 0x04
    }));

    WorldPacket stop;
    MopCompactPackets::BuildAttackStop(stop, attacker, victim, true);
    CHECK(stop.GetOpcode() == SMSG_ATTACKSTOP);
    CHECK(BytesEqual(stop, {
        0xCE, 0xF6, 0x00,
        0x09, 0x04, 0x02, 0x07, 0x76,
        0x45, 0x03, 0x32, 0x10, 0x67
    }));

    WorldPacket rejected;
    MopCompactPackets::BuildAttackStop(rejected, attacker, victim, false);
    CHECK(BytesEqual(rejected, {
        0xCE, 0x76, 0x00,
        0x09, 0x04, 0x02, 0x07, 0x76,
        0x45, 0x03, 0x32, 0x10, 0x67
    }));

    uint8_t const swingBody[] = { 0x67, 0x10, 0x76, 0x67, 0x45, 0x32 };
    WorldPacket swing(CMSG_ATTACKSWING, sizeof(swingBody));
    swing.append(swingBody, sizeof(swingBody));
    CHECK(MopCompactPackets::ReadAttackSwingTarget(swing).GetRawValue() == victim);

    WorldPacket denseCancel;
    MopCompactPackets::BuildCancelAutoRepeat(
        denseCancel, UINT64_C(0x0807060504030201));
    CHECK(denseCancel.GetOpcode() == SMSG_CANCEL_AUTO_REPEAT);
    CHECK(BytesEqual(denseCancel, {
        0xFF,
        0x09, 0x06, 0x02, 0x07, 0x00, 0x04, 0x03, 0x05
    }));

    WorldPacket sparseCancel;
    MopCompactPackets::BuildCancelAutoRepeat(
        sparseCancel, UINT64_C(0x000000BB0000AA00));
    CHECK(BytesEqual(sparseCancel, { 0x90, 0xBA, 0xAB }));
}

static void test_attacker_state_update()
{
    MopCompactPackets::AttackStateUpdateData update;
    update.hitInfo = 0x00000200u;
    update.attacker = ObjectGuid(UINT64_C(0x0807060504030201));
    update.target = ObjectGuid(UINT64_C(0x100F0E0D0C0B0A09));
    update.damage = 1000;
    update.overkill = 100;
    update.schoolMask = 1;
    update.victimState = 1;

    WorldPacket normal;
    MopCompactPackets::BuildAttackerStateUpdate(normal, update);
    CHECK(normal.GetOpcode() == SMSG_ATTACKERSTATEUPDATE);
    CHECK(BytesEqual(normal, {
        0x00, 0x34, 0x00, 0x00, 0x00,
        0x00, 0x02, 0x00, 0x00,
        0xFF, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
        0xE8, 0x03, 0x00, 0x00,
        0x64, 0x00, 0x00, 0x00,
        0x01,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x7A, 0x44,
        0xE8, 0x03, 0x00, 0x00,
        0x01,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));

    update = MopCompactPackets::AttackStateUpdateData();
    update.hitInfo = 0x000020A0u;
    update.damage = 50;
    update.schoolMask = 1;
    update.absorb = 10;
    update.resist = 5;
    update.victimState = 5;
    update.blocked = 3;

    WorldPacket mitigated;
    MopCompactPackets::BuildAttackerStateUpdate(mitigated, update);
    CHECK(BytesEqual(mitigated, {
        0x00, 0x34, 0x00, 0x00, 0x00,
        0xA0, 0x20, 0x00, 0x00,
        0x00, 0x00,
        0x32, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x01,
        0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x48, 0x42,
        0x32, 0x00, 0x00, 0x00,
        0x0A, 0x00, 0x00, 0x00,
        0x05, 0x00, 0x00, 0x00,
        0x05,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x03, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));

    update = MopCompactPackets::AttackStateUpdateData();
    update.hitInfo = 0x00000001u;
    WorldPacket extended;
    MopCompactPackets::BuildAttackerStateUpdate(extended, update);
    std::vector<uint8_t> expectedExtended(97, 0);
    expectedExtended[1] = 0x5C;
    expectedExtended[5] = 0x01;
    expectedExtended[19] = 0x01;
    CHECK(BytesEqual(extended, expectedExtended));
}

/// SMSG_MOVE_SET_RUN_SPEED, recovered from the client reader sub_C8B928 and
/// pinned here against a REAL retail body rather than a synthetic one.
///
/// capture-000004 seq 579, build 18414, catalogue 2BE10C89. Decoding it under
/// the reader's sequence yields guid 0x04000000053CC8E8, counter 65, speed 7.7 --
/// and 0x0400 is the same high pair the creatures in that capture's name
/// queries carry, so the GUID is corroborated independently of this packet.
static void test_run_speed_matches_retail_body()
{
    // The captured speed is 0x40F66667, one ULP above what the literal 7.7f
    // compiles to (0x40F66666), so the exact bits are reconstructed here. These
    // bits are taken verbatim from the wire; the provenance of retail's own
    // arithmetic is not established -- 7.0f * 1.1f in float lands on ...67 while
    // the same product evaluated in double and narrowed lands on ...66, so the
    // capture is the authority rather than any reconstruction of it. Using the
    // literal fails on byte 9 alone, which is a fair demonstration that this
    // fixture is byte-exact and not merely shape-exact.
    float speed;
    uint32 const speedBits = 0x40F66667u;
    std::memcpy(&speed, &speedBits, sizeof(speed));

    WorldPacket packet(SMSG_MOVE_SET_RUN_SPEED, 17);
    MopCompactPackets::BuildMoveSetRunSpeed(packet, 0x04000000053CC8E8ull, 65u, speed);
    CHECK(BytesEqual(packet, {
        0xD5,                                           // mask, guid order 1,7,4,2,5,3,6,0
        0xC9,                                           // guid[1] ^ 1
        0x41, 0x00, 0x00, 0x00,                         // counter 65
        0x05, 0x04, 0xE9,                               // guid[7], guid[3], guid[0] ^ 1
        0x67, 0x66, 0xF6, 0x40,                         // 7.7f
        0x3D                                            // guid[2] ^ 1; 4,6,5 are zero
    }));
}

/// SMSG_MOVE_SET_WALK_SPEED, from client reader sub_C8F849, pinned against
/// capture-000004 seq 23263 (build 18414, catalogue 2BE10C89). The mover is the
/// same creature as the run-speed body above -- guid 0x04000000053CC8E8 -- which
/// cross-checks that these really are distinct per-opcode interleaves.
static void test_walk_speed_matches_retail_body()
{
    float speed;
    uint32 const speedBits = 0x3FA00000u;                   // 1.25f
    std::memcpy(&speed, &speedBits, sizeof(speed));

    WorldPacket packet(SMSG_MOVE_SET_WALK_SPEED, 17);
    MopCompactPackets::BuildMoveSetWalkSpeed(packet, 0x04000000053CC8E8ull, 497u, speed);
    // guid 5, 6 and 4 are all zero in this mover, so the {5,6} and {4} groups
    // emit nothing at all and the counter follows the mask byte directly.
    CHECK(BytesEqual(packet, {
        0x7C,                                               // mask, guid order 6,7,3,1,2,0,4,5
        0xF1, 0x01, 0x00, 0x00,                             // counter 497
        0x00, 0x00, 0xA0, 0x3F,                             // 1.25f
        0x3D, 0x04, 0xE9, 0xC9, 0x05                        // guid[2,3,0,1,7] ^ 1
    }));
}

/// SMSG_SPLINE_MOVE_SET_RUN_SPEED, from client reader sub_C8C923, pinned against
/// capture-000004 seq 2506. The observer broadcast carries NO counter, only the
/// mover and the speed, which is what distinguishes it from every direct packet.
static void test_spline_run_speed_matches_retail_body()
{
    float speed;
    uint32 const speedBits = 0x409B3333u;                   // 4.85f
    std::memcpy(&speed, &speedBits, sizeof(speed));

    WorldPacket packet(SMSG_SPLINE_MOVE_SET_RUN_SPEED, 13);
    MopCompactPackets::BuildSplineMoveSetRunSpeed(packet, 0xF1308319002275D5ull, speed);
    CHECK(BytesEqual(packet, {
        0x7F,                                               // mask, guid order 3,0,1,4,7,5,6,2
        0x18,                                               // guid[4] ^ 1
        0x33, 0x33, 0x9B, 0x40,                             // 4.85f
        0x74, 0x82, 0xF0, 0x31, 0x23, 0xD4                  // guid[1,5,3,7,6,2,0] ^ 1, minus the absent one
    }));
}

/// SMSG_MOVE_SET_RUN_BACK_SPEED, reader sub_C8977A, pinned to capture-000004
/// seq 23260. Same mover as the run, walk and flight fixtures.
static void test_run_back_speed_matches_retail_body()
{
    float speed;
    uint32 const bits = 0x40100000u;                        // 2.25f
    std::memcpy(&speed, &bits, sizeof(speed));

    WorldPacket packet(SMSG_MOVE_SET_RUN_BACK_SPEED, 17);
    MopCompactPackets::BuildMoveSetRunBackSpeed(packet, 0x04000000053CC8E8ull, 494u, speed);
    CHECK(BytesEqual(packet, {
        0xF4,                                               // mask, guid order 7,1,0,2,4,3,6,5
        0xEE, 0x01, 0x00, 0x00,                             // counter 494
        0xE9, 0x04, 0x05, 0x3D, 0xC9,                       // guid[0,3,7,5,2,4,1] ^ 1, zeros absent
        0x00, 0x00, 0x10, 0x40                              // 2.25f
    }));
}

/// SMSG_MOVE_SET_FLIGHT_SPEED, reader sub_C8A820, pinned to capture-000004
/// seq 582. This one writes the float and counter BEFORE the mask byte, which no
/// sibling does -- the fixture exists mainly to pin that.
static void test_flight_speed_scalars_precede_mask()
{
    float speed;
    uint32 const bits = 0x41FC8F5Du;                        // 31.57f
    std::memcpy(&speed, &bits, sizeof(speed));

    WorldPacket packet(SMSG_MOVE_SET_FLIGHT_SPEED, 17);
    MopCompactPackets::BuildMoveSetFlightSpeed(packet, 0x04000000053CC8E8ull, 68u, speed);
    CHECK(BytesEqual(packet, {
        0x5D, 0x8F, 0xFC, 0x41,                             // 31.57f, first
        0x44, 0x00, 0x00, 0x00,                             // counter 68
        0x2F,                                               // mask, guid order 6,5,0,4,1,7,3,2
        0xE9, 0x05, 0x3D, 0x04, 0xC9                        // guid[0,7,4,5,6,2,3,1] ^ 1
    }));
}

/// SMSG_MOVE_SET_SWIM_BACK_SPEED, reader sub_C8AF44. NOT a retail fixture --
/// this opcode has zero observations at 18414, so there is no captured body to
/// pin it against and it stays outside the send gate. This test is structural:
/// an all-nonzero GUID forces every byte to be emitted, which catches the defect
/// the old serializer had (it wrote guid byte 0 twice and byte 2 never) and pins
/// the counter and float being adjacent.
static void test_swim_back_speed_structure_reader_derived()
{
    WorldPacket packet(SMSG_MOVE_SET_SWIM_BACK_SPEED, 17);
    MopCompactPackets::BuildMoveSetSwimBackSpeed(packet, 0x0123456789ABCDEFull, 0x12345678u, 1.0f);
    CHECK(BytesEqual(packet, {
        0xFF,                                               // every GUID byte present
        0x44, 0x22, 0xEE, 0x66,                             // guid[5,6,0,4] ^ 1
        0x78, 0x56, 0x34, 0x12,                             // counter
        0x00, 0x00, 0x80, 0x3F,                             // 1.0f, adjacent to the counter
        0xCC, 0x00, 0xAA, 0x88                              // guid[1,7,2,3] ^ 1
    }));
}

/// The interleave is what distinguishes run from swim: run writes one GUID byte
/// before the counter, swim writes none. Reusing the swim builder would produce
/// a body the client cannot parse, so pin that they differ.
static void test_run_speed_differs_from_swim_interleave()
{
    WorldPacket run(SMSG_MOVE_SET_RUN_SPEED, 17);
    MopCompactPackets::BuildMoveSetRunSpeed(run, 0x0123456789ABCDEFull, 0x12345678u, 1.0f);

    WorldPacket swim(SMSG_MOVE_SET_SWIM_SPEED, 17);
    MopCompactPackets::BuildMoveSetSwimSpeed(swim, 0x0123456789ABCDEFull, 0x12345678u, 1.0f);

    CHECK(run.size() == swim.size());                   // same field set, same width
    bool differs = false;
    for (size_t i = 0; i < run.size(); ++i)
    {
        if (run.contents()[i] != swim.contents()[i])
        {
            differs = true;
            break;
        }
    }
    CHECK(differs);                                     // ...but a different body
}

static void test_swim_speed_guid_layouts()
{
    {
        WorldPacket packet(SMSG_MOVE_SET_SWIM_SPEED, 17);
        MopCompactPackets::BuildMoveSetSwimSpeed(packet, 0x0123456789ABCDEFull, 0x12345678u, 1.0f);
        CHECK(BytesEqual(packet, {
            0xFF,
            0x78, 0x56, 0x34, 0x12,
            0xCC, 0x88,
            0x00, 0x00, 0x80, 0x3F,
            0x22, 0x00, 0xEE, 0x44, 0xAA, 0x66
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_SET_SWIM_SPEED, 10);
        MopCompactPackets::BuildMoveSetSwimSpeed(packet, 0xFFull, 0, 1.0f);
        CHECK(BytesEqual(packet, {
            0x40,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F,
            0xFE
        }));
    }
    {
        WorldPacket packet(SMSG_MOVE_SET_SWIM_SPEED, 9);
        MopCompactPackets::BuildMoveSetSwimSpeed(packet, 0, 7, 1.0f);
        CHECK(BytesEqual(packet, {
            0x00,
            0x07, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x80, 0x3F
        }));
    }
}

static void test_random_roll_guid_layouts()
{
    {
        WorldPacket packet(SMSG_RANDOM_ROLL, 21);
        MopCompactPackets::BuildRandomRoll(packet, 0x0123456789ABCDEFull, 1, 100, 42);
        CHECK(BytesEqual(packet, {
            0x2A, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x64, 0x00, 0x00, 0x00,
            0xFF,
            0x44, 0x66, 0xAA, 0xEE, 0x88, 0xCC, 0x22, 0x00
        }));
    }
    {
        WorldPacket packet(SMSG_RANDOM_ROLL, 14);
        MopCompactPackets::BuildRandomRoll(packet, 0xFFull, 5, 9, 7);
        CHECK(BytesEqual(packet, {
            0x07, 0x00, 0x00, 0x00,
            0x05, 0x00, 0x00, 0x00,
            0x09, 0x00, 0x00, 0x00,
            0x80, 0xFE
        }));
    }
    {
        WorldPacket packet(SMSG_RANDOM_ROLL, 13);
        MopCompactPackets::BuildRandomRoll(packet, 0, 0, 0, 0);
        CHECK(BytesEqual(packet, {
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00
        }));
    }
}

static void test_instance_encounter_variants()
{
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 5);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 0, 0, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, { 0x00, 0x00, 0x00, 0x00, 0xA5 }));
    }
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 4);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 1, 0, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, { 0x01, 0x00, 0x00, 0x00 }));
    }
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 14);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 2, 0x0123456789ABCDEFull, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, {
            0x02, 0x00, 0x00, 0x00,
            0xFF, 0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01,
            0xA5
        }));
    }
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 7);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 3, 0xFFull, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, {
            0x03, 0x00, 0x00, 0x00,
            0x01, 0xFF,
            0xA5
        }));
    }
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 6);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, 7, 0, 0xA5, 0x5A));
        CHECK(BytesEqual(packet, { 0x07, 0x00, 0x00, 0x00, 0xA5, 0x5A }));
    }

    for (uint32_t type : { 4u, 5u, 6u, 8u, 9u, 10u })
    {
        WorldPacket packet(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 14);
        CHECK(MopCompactPackets::BuildInstanceEncounter(packet, type, 0xFFull, 0xA5, 0x5A));
        if (type == 4)
            CHECK(BytesEqual(packet, { 0x04, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xA5 }));
        else if (type == 5 || type == 6 || type == 8)
            CHECK(BytesEqual(packet, { uint8_t(type), 0x00, 0x00, 0x00, 0xA5 }));
        else
            CHECK(BytesEqual(packet, { uint8_t(type), 0x00, 0x00, 0x00 }));
    }

    WorldPacket invalid(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT, 4);
    CHECK(!MopCompactPackets::BuildInstanceEncounter(invalid, 11, 0, 0, 0));
    CHECK(invalid.empty());
}

static void test_raid_difficulty()
{
    WorldPacket packet(SMSG_SET_RAID_DIFFICULTY, 4);
    MopCompactPackets::BuildSetRaidDifficulty(packet, 3);
    CHECK(BytesEqual(packet, { 0x03, 0x00, 0x00, 0x00 }));
}

static void test_dungeon_difficulty()
{
    WorldPacket packet(SMSG_SET_DUNGEON_DIFFICULTY, 4);
    MopCompactPackets::BuildSetDungeonDifficulty(packet, 2);
    CHECK(BytesEqual(packet, { 0x02, 0x00, 0x00, 0x00 }));
}

static void test_cancel_combat()
{
    WorldPacket packet(SMSG_CANCEL_COMBAT, 0);
    CHECK(packet.empty());
    CHECK(uint32_t(packet.GetOpcode()) == 0x0E8Bu);
}

static void test_party_kill_log()
{
    WorldPacket packet;
    MopCompactPackets::BuildPartyKillLog(
        packet,
        ObjectGuid(UINT64_C(0x8877665544332211)),
        ObjectGuid(UINT64_C(0xFFEEDDCCBBAA9901)));
    CHECK(packet.GetOpcode() == SMSG_PARTYKILLLOG);
    CHECK(BytesEqual(packet, {
        0xFF, 0xFF,
        0x00, 0xDC, 0x10, 0x32,
        0xFE, 0xEF, 0x98, 0xCD,
        0x54, 0x23, 0xAB, 0x76,
        0x45, 0x67, 0x89, 0xBA,
    }));
}

static void test_duel_state_packets()
{
    WorldPacket outOfBounds;
    MopDuelPackets::BuildOutOfBounds(outOfBounds);
    CHECK(outOfBounds.GetOpcode() == SMSG_DUEL_OUTOFBOUNDS);
    CHECK(outOfBounds.empty());

    WorldPacket inBounds;
    MopDuelPackets::BuildInBounds(inBounds);
    CHECK(inBounds.GetOpcode() == SMSG_DUEL_INBOUNDS);
    CHECK(inBounds.empty());

    WorldPacket completed;
    MopDuelPackets::BuildComplete(completed, true);
    CHECK(completed.GetOpcode() == SMSG_DUEL_COMPLETE);
    CHECK(BytesEqual(completed, { 0x80 }));

    WorldPacket interrupted;
    MopDuelPackets::BuildComplete(interrupted, false);
    CHECK(BytesEqual(interrupted, { 0x00 }));

    WorldPacket countdown;
    MopDuelPackets::BuildCountdown(countdown, 0x12345678u);
    CHECK(countdown.GetOpcode() == SMSG_DUEL_COUNTDOWN);
    CHECK(BytesEqual(countdown, { 0x78, 0x56, 0x34, 0x12 }));
}

static void test_duel_request_and_winner_packets()
{
    WorldPacket requested;
    MopDuelPackets::BuildRequested(
        requested,
        ObjectGuid(UINT64_C(0x0807060504030201)),
        ObjectGuid(UINT64_C(0x100F0E0D0C0B0A09)));
    CHECK(requested.GetOpcode() == SMSG_DUEL_REQUESTED);
    CHECK(BytesEqual(requested, {
        0xFF, 0xFF,
        0x07, 0x05, 0x11, 0x0C,
        0x09, 0x0D, 0x0E, 0x08,
        0x04, 0x0A, 0x0B, 0x00,
        0x02, 0x06, 0x03, 0x0F,
    }));

    WorldPacket winner;
    CHECK(MopDuelPackets::BuildWinner(
        winner, false, "Winner", 0x10203040u, "Loser", 0xA1B2C3D4u));
    CHECK(winner.GetOpcode() == SMSG_DUEL_WINNER);
    CHECK(BytesEqual(winner, {
        0x0C, 0x28,
        0xD4, 0xC3, 0xB2, 0xA1,
        'W', 'i', 'n', 'n', 'e', 'r',
        0x40, 0x30, 0x20, 0x10,
        'L', 'o', 's', 'e', 'r',
    }));

    WorldPacket retreat;
    CHECK(MopDuelPackets::BuildWinner(
        retreat, true, "Winner", 0x10203040u, "Loser", 0xA1B2C3D4u));
    CHECK(retreat[0] == 0x8C);

    WorldPacket maximum;
    CHECK(MopDuelPackets::BuildWinner(
        maximum, false, std::string(63, 'W'), 1, std::string(63, 'L'), 2));
    WorldPacket tooLong;
    CHECK(!MopDuelPackets::BuildWinner(
        tooLong, false, std::string(64, 'W'), 1, "L", 2));
    CHECK(tooLong.empty());
}

static void test_mirror_timer_packets()
{
    WorldPacket started;
    MopMirrorTimerPackets::BuildStart(
        started, 0xDDEEFF00u, 0x11223344u, 0x99AABBCCu,
        int32_t(-2), 0x55667788u, true);
    CHECK(started.GetOpcode() == SMSG_START_MIRROR_TIMER);
    CHECK(BytesEqual(started, {
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99,
        0xFE, 0xFF, 0xFF, 0xFF,
        0x00, 0xFF, 0xEE, 0xDD,
        0x80,
    }));

    WorldPacket running;
    MopMirrorTimerPackets::BuildStart(
        running, 1, 1000, 750, -1, 0, false);
    CHECK(running[20] == 0x00);

    WorldPacket stopped;
    MopMirrorTimerPackets::BuildStop(stopped, 0x12345678u);
    CHECK(stopped.GetOpcode() == SMSG_STOP_MIRROR_TIMER);
    CHECK(BytesEqual(stopped, { 0x78, 0x56, 0x34, 0x12 }));
}

static void test_rune_packets()
{
    std::array<MopRunePackets::RuneState, MAX_RUNES> const runes = {{
        { RUNE_BLOOD, 0x10 },
        { RUNE_BLOOD, 0x20 },
        { RUNE_UNHOLY, 0x30 },
        { RUNE_UNHOLY, 0x40 },
        { RUNE_FROST, 0x50 },
        { RUNE_DEATH, 0x60 },
    }};

    WorldPacket resync;
    MopRunePackets::BuildResync(resync, runes);
    CHECK(resync.GetOpcode() == SMSG_RESYNC_RUNES);
    CHECK(BytesEqual(resync, {
        0x00, 0x00, 0x0C,
        0x10, 0x00, 0x20, 0x00,
        0x30, 0x01, 0x40, 0x01,
        0x50, 0x02, 0x60, 0x03,
    }));

    WorldPacket power;
    MopRunePackets::BuildAddPower(power, 0x20u);
    CHECK(power.GetOpcode() == SMSG_ADD_RUNE_POWER);
    CHECK(BytesEqual(power, { 0x20, 0x00, 0x00, 0x00 }));

    WorldPacket converted;
    MopRunePackets::BuildConvert(converted, RUNE_DEATH, 4);
    CHECK(converted.GetOpcode() == SMSG_CONVERT_RUNE);
    CHECK(BytesEqual(converted, { 0x03, 0x04 }));
}

static void test_threat_packets()
{
    ObjectGuid const owner(UINT64_C(0x0807060504030201));
    ObjectGuid const selected(UINT64_C(0x100F0E0D0C0B0A09));
    MopThreatPackets::ThreatEntries const entries = {{
        ObjectGuid(UINT64_C(0x1817161514131211)), 0xA1B2C3D4u
    }};

    WorldPacket update;
    MopThreatPackets::BuildUpdate(update, owner, entries);
    CHECK(update.GetOpcode() == SMSG_THREAT_UPDATE);
    CHECK(BytesEqual(update, {
        0xFE, 0x00, 0x00, 0x1F, 0xF8,
        0x16, 0x19, 0x10, 0x13, 0x12, 0x17, 0x15, 0x14,
        0xD4, 0xC3, 0xB2, 0xA1,
        0x03, 0x04, 0x02, 0x05, 0x07, 0x06, 0x00, 0x09,
    }));

    WorldPacket highest;
    MopThreatPackets::BuildHighest(highest, owner, selected, entries);
    CHECK(highest.GetOpcode() == SMSG_HIGHEST_THREAT_UPDATE);
    CHECK(BytesEqual(highest, {
        0xFF, 0xF8, 0x00, 0x00, 0x7F, 0xF8,
        0x04, 0x16, 0xD4, 0xC3, 0xB2, 0xA1,
        0x14, 0x10, 0x15, 0x17, 0x12, 0x13, 0x19,
        0x0D, 0x07, 0x0A, 0x03, 0x00, 0x02, 0x0E, 0x0B,
        0x09, 0x08, 0x0C, 0x11, 0x05, 0x06, 0x0F,
    }));

    WorldPacket clear;
    MopThreatPackets::BuildClear(clear, owner);
    CHECK(clear.GetOpcode() == SMSG_THREAT_CLEAR);
    CHECK(BytesEqual(clear, {
        0xFF, 0x09, 0x00, 0x04, 0x05, 0x02, 0x03, 0x06, 0x07,
    }));

    WorldPacket remove;
    MopThreatPackets::BuildRemove(remove, owner, selected);
    CHECK(remove.GetOpcode() == SMSG_THREAT_REMOVE);
    CHECK(BytesEqual(remove, {
        0xFF, 0xFF, 0x0D, 0x08, 0x0A, 0x07, 0x04, 0x09, 0x05,
        0x00, 0x0C, 0x03, 0x0B, 0x06, 0x11, 0x0E, 0x02, 0x0F,
    }));
}

static void test_dismount_packet()
{
    WorldPacket packet;
    MopCompactPackets::BuildDismount(
        packet, ObjectGuid(UINT64_C(0x0807060504030201)));
    CHECK(packet.GetOpcode() == SMSG_DISMOUNT);
    CHECK(BytesEqual(packet, {
        0xFF, 0x05, 0x06, 0x09, 0x07, 0x03, 0x04, 0x02, 0x00,
    }));
}

static void test_combo_points_packet()
{
    WorldPacket packet;
    MopComboPointPackets::BuildUpdate(
        packet, ObjectGuid(UINT64_C(0x0807060504030201)), 5);
    CHECK(packet.GetOpcode() == SMSG_UPDATE_COMBO_POINTS);
    CHECK(BytesEqual(packet, {
        0xFF, 0x07, 0x06, 0x04, 0x09, 0x05, 0x00, 0x05, 0x02, 0x03,
    }));
}

static void test_pre_resurrect_packet()
{
    WorldPacket packet;
    MopCompactPackets::BuildPreResurrect(
        packet, ObjectGuid(UINT64_C(0x0807060504030201)));
    CHECK(packet.GetOpcode() == SMSG_PRE_RESURRECT);
    CHECK(BytesEqual(packet, {
        0xFF, 0x07, 0x03, 0x09, 0x00, 0x06, 0x04, 0x02, 0x05,
    }));

    // A zero byte clears its mask bit and is omitted entirely, so the body
    // shortens. Guards against writing a fixed nine-byte body.
    WorldPacket sparse;
    MopCompactPackets::BuildPreResurrect(
        sparse, ObjectGuid(UINT64_C(0x0000060000030001)));
    CHECK(BytesEqual(sparse, {
        0x34, 0x07, 0x00, 0x02,
    }));
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32_t(SMSG_ATTACKSWING_ERROR) == 0x11E1u);
    CHECK(uint32_t(SMSG_MOVE_SET_SWIM_SPEED) == 0x0817u);
    CHECK(uint32_t(SMSG_MOVE_SET_RUN_SPEED) == 0x184Cu);
    CHECK(uint32_t(SMSG_MOVE_SET_WALK_SPEED) == 0x0469u);
    CHECK(uint32_t(SMSG_SPLINE_MOVE_SET_RUN_SPEED) == 0x02F1u);
    CHECK(uint32_t(SMSG_MOVE_SET_SWIM_BACK_SPEED) == 0x0962u);
    CHECK(uint32_t(SMSG_MOVE_SET_RUN_SPEED) <= 0x1FFFu);   // must fit the 13-bit wire header
    CHECK(uint32_t(SMSG_RANDOM_ROLL) == 0x141Au);
    CHECK(uint32_t(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT) == 0x0332u);
    CHECK(uint32_t(SMSG_SET_RAID_DIFFICULTY) == 0x0591u);
    CHECK(uint32_t(SMSG_SET_DUNGEON_DIFFICULTY) == 0x1283u);
    CHECK(uint32_t(SMSG_ATTACKSTART) == 0x1A9Eu);
    CHECK(uint32_t(SMSG_ATTACKSTOP) == 0x12AFu);
    CHECK(uint32_t(SMSG_ATTACKERSTATEUPDATE) == 0x06AAu);
    CHECK(uint32_t(SMSG_CANCEL_COMBAT) == 0x0E8Bu);
    CHECK(uint32_t(SMSG_PARTYKILLLOG) == 0x048Au);
    CHECK(uint32_t(SMSG_DUEL_OUTOFBOUNDS) == 0x001Au);
    CHECK(uint32_t(SMSG_DUEL_INBOUNDS) == 0x163Au);
    CHECK(uint32_t(SMSG_DUEL_COMPLETE) == 0x1C0Au);
    CHECK(uint32_t(SMSG_DUEL_COUNTDOWN) == 0x129Fu);
    CHECK(uint32_t(SMSG_DUEL_REQUESTED) == 0x0022u);
    CHECK(uint32_t(SMSG_DUEL_WINNER) == 0x10E1u);
    CHECK(uint32_t(SMSG_START_MIRROR_TIMER) == 0x0E12u);
    CHECK(uint32_t(SMSG_STOP_MIRROR_TIMER) == 0x1026u);
    CHECK(uint32_t(SMSG_RESYNC_RUNES) == 0x15E3u);
    CHECK(uint32_t(SMSG_ADD_RUNE_POWER) == 0x1860u);
    CHECK(uint32_t(SMSG_CONVERT_RUNE) == 0x1A1Bu);
    CHECK(uint32_t(SMSG_THREAT_UPDATE) == 0x0632u);
    CHECK(uint32_t(SMSG_HIGHEST_THREAT_UPDATE) == 0x14AEu);
    CHECK(uint32_t(SMSG_THREAT_CLEAR) == 0x180Bu);
    CHECK(uint32_t(SMSG_THREAT_REMOVE) == 0x1960u);
    CHECK(uint32_t(SMSG_DISMOUNT) == 0x0E3Au);
    CHECK(uint32_t(SMSG_PRE_RESURRECT) == 0x19C0u);
    CHECK(uint32_t(SMSG_UPDATE_COMBO_POINTS) == 0x082Fu);

    CHECK(uint32_t(SMSG_ATTACKSWING_ERROR) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_MOVE_SET_SWIM_SPEED) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_RANDOM_ROLL) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_SET_RAID_DIFFICULTY) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_SET_DUNGEON_DIFFICULTY) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_ATTACKSTART) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_ATTACKSTOP) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_ATTACKERSTATEUPDATE) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_CANCEL_COMBAT) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_PARTYKILLLOG) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_DUEL_OUTOFBOUNDS) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_DUEL_INBOUNDS) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_DUEL_COMPLETE) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_DUEL_COUNTDOWN) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_DUEL_REQUESTED) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_DUEL_WINNER) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_START_MIRROR_TIMER) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_STOP_MIRROR_TIMER) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_RESYNC_RUNES) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_ADD_RUNE_POWER) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_CONVERT_RUNE) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_THREAT_UPDATE) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_HIGHEST_THREAT_UPDATE) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_THREAT_CLEAR) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_THREAT_REMOVE) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_DISMOUNT) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_PRE_RESURRECT) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_UPDATE_COMBO_POINTS) <= 0x1FFFu);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_attack_swing_reasons();
    test_attack_packets();
    test_attacker_state_update();
    test_run_speed_matches_retail_body();
    test_walk_speed_matches_retail_body();
    test_run_back_speed_matches_retail_body();
    test_swim_back_speed_structure_reader_derived();
    test_flight_speed_scalars_precede_mask();
    test_spline_run_speed_matches_retail_body();
    test_run_speed_differs_from_swim_interleave();
    test_swim_speed_guid_layouts();
    test_random_roll_guid_layouts();
    test_instance_encounter_variants();
    test_raid_difficulty();
    test_dungeon_difficulty();
    test_cancel_combat();
    test_party_kill_log();
    test_duel_state_packets();
    test_duel_request_and_winner_packets();
    test_mirror_timer_packets();
    test_rune_packets();
    test_threat_packets();
    test_dismount_packet();
    test_pre_resurrect_packet();
    test_combo_points_packet();
    test_opcode_values_are_framable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_compact_packets: all checks passed\n");
    return 0;
}
