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

/// Reader-derived structural cases, NOT retail fixtures.
///
/// The retail bodies available for this family all come from one mover whose
/// GUID has three zero bytes, so those byte positions are never exercised. These
/// use an all-nonzero GUID so every position is emitted, which pins the full
/// interleave rather than the subset a sparse GUID happens to reach.
///
/// TURN_RATE, FLIGHT_BACK and PITCH_RATE have ZERO observations at 18414 and so
/// have no retail body at all; these are their only serializer tests, and all
/// three remain outside the send gate.
static void test_speed_family_full_interleaves_reader_derived()
{
    uint64 const guid = 0x0123456789ABCDEFull;
    uint32 const counter = 0x12345678u;

    {   // run-back: u32 leads, then seven bytes, float, one byte
        WorldPacket p(SMSG_MOVE_SET_RUN_BACK_SPEED, 17);
        MopCompactPackets::BuildMoveSetRunBackSpeed(p, guid, counter, 1.0f);
        CHECK(BytesEqual(p, { 0xFF, 0x78, 0x56, 0x34, 0x12, 0xEE, 0x88, 0x00,
                              0x44, 0xAA, 0x66, 0xCC, 0x00, 0x00, 0x80, 0x3F, 0x22 }));
    }
    {   // flight: float and counter BEFORE the mask byte
        WorldPacket p(SMSG_MOVE_SET_FLIGHT_SPEED, 17);
        MopCompactPackets::BuildMoveSetFlightSpeed(p, guid, counter, 1.0f);
        CHECK(BytesEqual(p, { 0x00, 0x00, 0x80, 0x3F, 0x78, 0x56, 0x34, 0x12,
                              0xFF, 0xEE, 0x00, 0x66, 0x44, 0x22, 0xAA, 0x88, 0xCC }));
    }
    {   // turn rate: float first, counter almost last
        WorldPacket p(SMSG_MOVE_SET_TURN_RATE, 17);
        MopCompactPackets::BuildMoveSetTurnRate(p, guid, counter, 1.0f);
        CHECK(BytesEqual(p, { 0xFF, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x88, 0x44,
                              0xEE, 0x66, 0x22, 0xAA, 0x78, 0x56, 0x34, 0x12, 0xCC }));
    }
    {   // flight back: float trails everything
        WorldPacket p(SMSG_MOVE_SET_FLIGHT_BACK_SPEED, 17);
        MopCompactPackets::BuildMoveSetFlightBackSpeed(p, guid, counter, 1.0f);
        CHECK(BytesEqual(p, { 0xFF, 0x66, 0xCC, 0x22, 0xEE, 0xAA, 0x78, 0x56,
                              0x34, 0x12, 0x00, 0x88, 0x44, 0x00, 0x00, 0x80, 0x3F }));
    }
    {   // pitch rate
        WorldPacket p(SMSG_MOVE_SET_PITCH_RATE, 17);
        MopCompactPackets::BuildMoveSetPitchRate(p, guid, counter, 1.0f);
        CHECK(BytesEqual(p, { 0xFF, 0x66, 0x78, 0x56, 0x34, 0x12, 0xAA, 0x44,
                              0x22, 0xCC, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x88, 0xEE }));
    }
}

/// The four spline speed broadcasts, each pinned to a retail body. Sequences
/// 2510, 2507, 2508 and 2509 of capture-000004 are consecutive packets for one
/// mover, 0xF1308319002275D5, and every speed is exactly half its base for that
/// movement type -- one creature uniformly slowed, decoded under four different
/// interleaves.
static void test_spline_speed_family_matches_retail_bodies()
{
    uint64 const guid = 0xF1308319002275D5ull;
    float f;

    {   // walk 1.25, speed trails everything
        uint32 const bits = 0x3FA00000u; std::memcpy(&f, &bits, sizeof(f));
        WorldPacket p(SMSG_SPLINE_MOVE_SET_WALK_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetWalkSpeed(p, guid, f);
        CHECK(BytesEqual(p, { 0xF7, 0x23, 0x74, 0xD4, 0x31, 0x82, 0x18, 0xF0,
                              0x00, 0x00, 0xA0, 0x3F }));
    }
    {   // run-back 2.25, one GUID byte after the speed
        uint32 const bits = 0x40100000u; std::memcpy(&f, &bits, sizeof(f));
        WorldPacket p(SMSG_SPLINE_MOVE_SET_RUN_BACK_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetRunBackSpeed(p, guid, f);
        CHECK(BytesEqual(p, { 0xEF, 0x31, 0x18, 0x74, 0x82, 0x23, 0xF0,
                              0x00, 0x00, 0x10, 0x40, 0xD4 }));
    }
    {   // swim 2.361110, three GUID bytes after the speed
        uint32 const bits = 0x40171C6Du; std::memcpy(&f, &bits, sizeof(f));
        WorldPacket p(SMSG_SPLINE_MOVE_SET_SWIM_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetSwimSpeed(p, guid, f);
        CHECK(BytesEqual(p, { 0xEF, 0x18, 0x74, 0x31, 0xF0, 0x6D, 0x1C, 0x17,
                              0x40, 0x82, 0xD4, 0x23 }));
    }
    {   // flight 3.5, speed LEADS before the mask byte
        uint32 const bits = 0x40600000u; std::memcpy(&f, &bits, sizeof(f));
        WorldPacket p(SMSG_SPLINE_MOVE_SET_FLIGHT_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetFlightSpeed(p, guid, f);
        CHECK(BytesEqual(p, { 0x00, 0x00, 0x60, 0x40, 0xEF, 0x82, 0x74, 0xD4,
                              0x31, 0x23, 0x18, 0xF0 }));
    }
}

/// Every spline speed builder under an all-nonzero GUID.
///
/// The retail bodies above all come from mover 0xF1308319002275D5, exactly ONE
/// of whose GUID bytes is zero -- byte 3 -- and so is never emitted. That is why
/// those bodies are 12 bytes: one mask, seven emitted bytes and the float. Two
/// reviews of the previous commit both caught this described as three; the
/// commit message for it says three and is wrong.
///
/// So the retail fixtures pin byte 3's mask position and the interleave of the
/// other seven, but cannot catch byte 3 omitted from or misplaced within the
/// byte interleave. A GUID with no zero byte forces all eight through every
/// builder and closes exactly that gap.
///
/// The mask is 0xFF in every case here, so this test pins no mask position at
/// all. Mask order is covered separately, below.
///
/// The last four have no observed body at all and are reader-derived only, so
/// this and the mask-order test are their whole coverage. They are admitted
/// nonetheless, on binary proof, as are their four direct counterparts.
static void test_spline_speed_family_full_interleaves_reader_derived()
{
    uint64 const guid = 0x0123456789ABCDEFull;  // guid[7]^1 is 0x00, not absent
    float const speed = 1.0f;                   // 0x00, 0x00, 0x80, 0x3F

    {   // run: one byte, speed, seven bytes
        WorldPacket p(SMSG_SPLINE_MOVE_SET_RUN_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetRunSpeed(p, guid, speed);
        CHECK(BytesEqual(p, { 0xFF, 0x66, 0x00, 0x00, 0x80, 0x3F, 0xCC, 0x44,
                              0x88, 0x00, 0x22, 0xAA, 0xEE }));
    }
    {   // walk: all eight bytes, then the speed
        WorldPacket p(SMSG_SPLINE_MOVE_SET_WALK_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetWalkSpeed(p, guid, speed);
        CHECK(BytesEqual(p, { 0xFF, 0xAA, 0x88, 0xCC, 0xEE, 0x22, 0x44, 0x66,
                              0x00, 0x00, 0x00, 0x80, 0x3F }));
    }
    {   // run back: seven bytes, speed, one byte
        WorldPacket p(SMSG_SPLINE_MOVE_SET_RUN_BACK_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetRunBackSpeed(p, guid, speed);
        CHECK(BytesEqual(p, { 0xFF, 0x22, 0x66, 0xCC, 0x44, 0xAA, 0x88, 0x00,
                              0x00, 0x00, 0x80, 0x3F, 0xEE }));
    }
    {   // swim: five bytes, speed, three bytes
        WorldPacket p(SMSG_SPLINE_MOVE_SET_SWIM_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetSwimSpeed(p, guid, speed);
        CHECK(BytesEqual(p, { 0xFF, 0x66, 0xCC, 0x22, 0x00, 0x88, 0x00, 0x00,
                              0x80, 0x3F, 0x44, 0xEE, 0xAA }));
    }
    {   // flight: the speed LEADS, before the mask byte
        WorldPacket p(SMSG_SPLINE_MOVE_SET_FLIGHT_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetFlightSpeed(p, guid, speed);
        CHECK(BytesEqual(p, { 0x00, 0x00, 0x80, 0x3F, 0xFF, 0x44, 0xCC, 0xEE,
                              0x22, 0xAA, 0x66, 0x00, 0x88 }));
    }
    {   // swim back: all eight bytes, then the speed
        WorldPacket p(SMSG_SPLINE_MOVE_SET_SWIM_BACK_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetSwimBackSpeed(p, guid, speed);
        CHECK(BytesEqual(p, { 0xFF, 0x00, 0x22, 0x44, 0x88, 0xAA, 0x66, 0xCC,
                              0xEE, 0x00, 0x00, 0x80, 0x3F }));
    }
    {   // turn rate: two bytes precede the rate, not zero as this tree once wrote
        WorldPacket p(SMSG_SPLINE_MOVE_SET_TURN_RATE, 13);
        MopCompactPackets::BuildSplineMoveSetTurnRate(p, guid, speed);
        CHECK(BytesEqual(p, { 0xFF, 0xCC, 0x00, 0x00, 0x00, 0x80, 0x3F, 0x22,
                              0xEE, 0x66, 0xAA, 0x44, 0x88 }));
    }
    {   // flight back: three bytes, speed, five bytes
        WorldPacket p(SMSG_SPLINE_MOVE_SET_FLIGHT_BACK_SPEED, 13);
        MopCompactPackets::BuildSplineMoveSetFlightBackSpeed(p, guid, speed);
        CHECK(BytesEqual(p, { 0xFF, 0x00, 0x22, 0x66, 0x00, 0x00, 0x80, 0x3F,
                              0xCC, 0x88, 0xAA, 0xEE, 0x44 }));
    }
    {   // pitch rate: one byte, speed, seven bytes
        WorldPacket p(SMSG_SPLINE_MOVE_SET_PITCH_RATE, 13);
        MopCompactPackets::BuildSplineMoveSetPitchRate(p, guid, speed);
        CHECK(BytesEqual(p, { 0xFF, 0x44, 0x00, 0x00, 0x80, 0x3F, 0x66, 0xEE,
                              0x88, 0x22, 0xCC, 0xAA, 0x00 }));
    }
}

/// Mask ORDER, which neither the retail fixtures nor the all-nonzero test pins.
///
/// A GUID with exactly one zero byte emits exactly one CLEAR mask bit, and the
/// position of that clear bit is where the byte sits in the mask order. Probing
/// all eight slots therefore recovers the whole permutation, so no transposition
/// survives. Both reviews of the previous commit raised this independently:
/// without it, any of the 8! = 40320 orders would satisfy the suite for the four
/// builders that have no retail body to constrain them, and promotion is one
/// case label away.
static void CheckSplineMaskOrder(char const* what, uint16 opcode, size_t maskOffset,
    void (*build)(WorldPacket&, uint64, float), uint8 const (&expected)[8])
{
    uint8 recovered[8];
    for (uint8 slot = 0; slot < 8; ++slot)
    {
        recovered[slot] = 0xFF;
    }

    for (uint8 slot = 0; slot < 8; ++slot)
    {
        uint64 const guid = 0x0123456789ABCDEFull & ~(uint64(0xFF) << (8 * slot));
        WorldPacket packet(opcode, 13);
        build(packet, guid, 1.0f);

        uint8 const mask = packet.contents()[maskOffset];
        uint8 const cleared = uint8(0xFF ^ mask);
        if (cleared == 0 || (cleared & uint8(cleared - 1)) != 0)
        {
            std::fprintf(stderr, "FAIL %s: zeroing guid[%u] gave mask 0x%02X, wanted one clear bit\n",
                         what, unsigned(slot), mask);
            ++g_fail;
            continue;
        }

        uint8 position = 0;
        while (uint8(0x80 >> position) != cleared)
        {
            ++position;
        }
        recovered[position] = slot;
    }

    for (uint8 position = 0; position < 8; ++position)
    {
        if (recovered[position] != expected[position])
        {
            std::fprintf(stderr, "FAIL %s: mask bit %u is guid[%u], wanted guid[%u]\n",
                         what, unsigned(position), unsigned(recovered[position]),
                         unsigned(expected[position]));
            ++g_fail;
        }
    }
}

/// The same probe for the DIRECT speed builders, which carry a counter and so
/// need their own signature. Four of these were held back from the send gate
/// while their spline counterparts were admitted, which left observers told of a
/// speed change that the mover's own client never heard about. Pinning mask
/// order here is what makes closing that gap safe.
static void CheckDirectMaskOrder(char const* what, uint16 opcode, size_t maskOffset,
    void (*build)(WorldPacket&, uint64, uint32, float), uint8 const (&expected)[8])
{
    uint8 recovered[8];
    for (uint8 slot = 0; slot < 8; ++slot)
    {
        recovered[slot] = 0xFF;
    }

    for (uint8 slot = 0; slot < 8; ++slot)
    {
        uint64 const guid = 0x0123456789ABCDEFull & ~(uint64(0xFF) << (8 * slot));
        WorldPacket packet(opcode, 17);
        build(packet, guid, 0x12345678u, 1.0f);

        uint8 const mask = packet.contents()[maskOffset];
        uint8 const cleared = uint8(0xFF ^ mask);
        if (cleared == 0 || (cleared & uint8(cleared - 1)) != 0)
        {
            std::fprintf(stderr, "FAIL %s: zeroing guid[%u] gave mask 0x%02X, wanted one clear bit\n",
                         what, unsigned(slot), mask);
            ++g_fail;
            continue;
        }

        uint8 position = 0;
        while (uint8(0x80 >> position) != cleared)
        {
            ++position;
        }
        recovered[position] = slot;
    }

    for (uint8 position = 0; position < 8; ++position)
    {
        if (recovered[position] != expected[position])
        {
            std::fprintf(stderr, "FAIL %s: mask bit %u is guid[%u], wanted guid[%u]\n",
                         what, unsigned(position), unsigned(recovered[position]),
                         unsigned(expected[position]));
            ++g_fail;
        }
    }
}

static void test_direct_speed_family_mask_order()
{
    uint8 const run[8]        = { 1, 7, 4, 2, 5, 3, 6, 0 };
    uint8 const swim[8]       = { 5, 0, 6, 3, 7, 2, 4, 1 };
    uint8 const walk[8]       = { 6, 7, 3, 1, 2, 0, 4, 5 };
    uint8 const runBack[8]    = { 7, 1, 0, 2, 4, 3, 6, 5 };
    uint8 const swimBack[8]   = { 5, 0, 4, 2, 1, 3, 6, 7 };
    uint8 const turnRate[8]   = { 6, 5, 1, 4, 0, 7, 3, 2 };
    uint8 const flight[8]     = { 6, 5, 0, 4, 1, 7, 3, 2 };
    uint8 const flightBack[8] = { 2, 7, 6, 4, 0, 1, 5, 3 };
    uint8 const pitchRate[8]  = { 7, 5, 4, 1, 6, 3, 2, 0 };

    CheckDirectMaskOrder("run", SMSG_MOVE_SET_RUN_SPEED, 0,
        &MopCompactPackets::BuildMoveSetRunSpeed, run);
    CheckDirectMaskOrder("swim", SMSG_MOVE_SET_SWIM_SPEED, 0,
        &MopCompactPackets::BuildMoveSetSwimSpeed, swim);
    CheckDirectMaskOrder("walk", SMSG_MOVE_SET_WALK_SPEED, 0,
        &MopCompactPackets::BuildMoveSetWalkSpeed, walk);
    CheckDirectMaskOrder("run back", SMSG_MOVE_SET_RUN_BACK_SPEED, 0,
        &MopCompactPackets::BuildMoveSetRunBackSpeed, runBack);
    CheckDirectMaskOrder("swim back", SMSG_MOVE_SET_SWIM_BACK_SPEED, 0,
        &MopCompactPackets::BuildMoveSetSwimBackSpeed, swimBack);
    CheckDirectMaskOrder("turn rate", SMSG_MOVE_SET_TURN_RATE, 0,
        &MopCompactPackets::BuildMoveSetTurnRate, turnRate);
    // Flight reads the speed and the counter BEFORE the mask byte.
    CheckDirectMaskOrder("flight", SMSG_MOVE_SET_FLIGHT_SPEED, 8,
        &MopCompactPackets::BuildMoveSetFlightSpeed, flight);
    CheckDirectMaskOrder("flight back", SMSG_MOVE_SET_FLIGHT_BACK_SPEED, 0,
        &MopCompactPackets::BuildMoveSetFlightBackSpeed, flightBack);
    CheckDirectMaskOrder("pitch rate", SMSG_MOVE_SET_PITCH_RATE, 0,
        &MopCompactPackets::BuildMoveSetPitchRate, pitchRate);
}

static void test_spline_speed_family_mask_order()
{
    uint8 const run[8]        = { 3, 0, 1, 4, 7, 5, 6, 2 };
    uint8 const walk[8]       = { 4, 1, 7, 6, 3, 2, 5, 0 };
    uint8 const runBack[8]    = { 7, 4, 0, 3, 2, 5, 6, 1 };
    uint8 const swim[8]       = { 5, 6, 7, 3, 4, 2, 1, 0 };
    uint8 const flight[8]     = { 1, 4, 7, 3, 2, 6, 5, 0 };
    uint8 const swimBack[8]   = { 2, 6, 5, 0, 4, 3, 1, 7 };
    uint8 const turnRate[8]   = { 5, 7, 4, 0, 1, 6, 3, 2 };
    uint8 const flightBack[8] = { 6, 0, 2, 7, 5, 4, 3, 1 };
    uint8 const pitchRate[8]  = { 2, 6, 0, 5, 1, 3, 7, 4 };

    CheckSplineMaskOrder("spline run", SMSG_SPLINE_MOVE_SET_RUN_SPEED, 0,
        &MopCompactPackets::BuildSplineMoveSetRunSpeed, run);
    CheckSplineMaskOrder("spline walk", SMSG_SPLINE_MOVE_SET_WALK_SPEED, 0,
        &MopCompactPackets::BuildSplineMoveSetWalkSpeed, walk);
    CheckSplineMaskOrder("spline run back", SMSG_SPLINE_MOVE_SET_RUN_BACK_SPEED, 0,
        &MopCompactPackets::BuildSplineMoveSetRunBackSpeed, runBack);
    CheckSplineMaskOrder("spline swim", SMSG_SPLINE_MOVE_SET_SWIM_SPEED, 0,
        &MopCompactPackets::BuildSplineMoveSetSwimSpeed, swim);
    // The flight speed leads, so its mask byte sits after the float.
    CheckSplineMaskOrder("spline flight", SMSG_SPLINE_MOVE_SET_FLIGHT_SPEED, 4,
        &MopCompactPackets::BuildSplineMoveSetFlightSpeed, flight);
    CheckSplineMaskOrder("spline swim back", SMSG_SPLINE_MOVE_SET_SWIM_BACK_SPEED, 0,
        &MopCompactPackets::BuildSplineMoveSetSwimBackSpeed, swimBack);
    CheckSplineMaskOrder("spline turn rate", SMSG_SPLINE_MOVE_SET_TURN_RATE, 0,
        &MopCompactPackets::BuildSplineMoveSetTurnRate, turnRate);
    CheckSplineMaskOrder("spline flight back", SMSG_SPLINE_MOVE_SET_FLIGHT_BACK_SPEED, 0,
        &MopCompactPackets::BuildSplineMoveSetFlightBackSpeed, flightBack);
    CheckSplineMaskOrder("spline pitch rate", SMSG_SPLINE_MOVE_SET_PITCH_RATE, 0,
        &MopCompactPackets::BuildSplineMoveSetPitchRate, pitchRate);
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

/// CMSG_PET_ACTION, pinned to real 18414 bodies.
///
/// Eight bodies at catalogue 2BE10C89 across FIVE DISTINCT PRESENCE MASKS:
/// 0x7904, 0x7DAE, 0x7DAF, 0xFEBF and 0xFFBF. The mask is what matters, not the
/// length. Bodies sharing a mask reproduce the reader instead of proving it,
/// because bits that are always present together can be permuted without
/// changing any decode. Two reviews of the first version of this test made that
/// point independently, and they were right: it had only two distinct masks.
///
/// 0x7DAE and 0x7DAF are the strongest pair here. They differ in exactly ONE
/// mask bit, for the same pet, and the targets that fall out differ accordingly
/// -- 0xF1311C18000000D4 against 0xF1311C180000012F. That pins the position of
/// that single bit directly, which no amount of same-mask evidence can.
///
/// Each body must consume EXACTLY. Because the length is 18 + popcount of the
/// sixteen presence bits, exact consumption is a real constraint, not a
/// tautology.
///
/// The decoded GUIDs are then checked against a fact outside the packet: 0xF14
/// is HIGHGUID_PET, 0xF13 HIGHGUID_UNIT and 0xF15 HIGHGUID_VEHICLE.
///
/// WHAT THIS STILL DOES NOT PIN. Bits that are present together in every one of
/// these bodies remain mutually permutable, so the interleave is constrained but
/// NOT unique. A review enumerated the residue: three free classes and 86,400
/// equivalent orders, of which only three bit positions are actually pinned --
/// pet[3] and target[3], which are never present, and target[1], by the
/// 0x7DAE/0x7DAF pair. Only the client's own writer can close the rest, and the
/// consequence of a wrong choice inside a free class is not a desync but a
/// silently wrong GUID whenever a real one carries a zero byte in a free slot.
static void CheckPetAction(char const* what, uint8_t const* body, size_t length,
    uint32 expectedAction, uint64 expectedPet, uint64 expectedTarget)
{
    WorldPacket packet(CMSG_PET_ACTION, uint32(length));
    packet.append(body, length);

    uint32 action = 0;
    float posY = 0.0f, posZ = 0.0f, posX = 0.0f;
    ObjectGuid pet;
    ObjectGuid target;
    MopCompactPackets::ReadPetAction(packet, action, posY, posZ, posX, pet, target);

    if (action != expectedAction || pet.GetRawValue() != expectedPet ||
        target.GetRawValue() != expectedTarget || packet.rpos() != packet.size())
    {
        std::fprintf(stderr,
            "FAIL %s: action 0x%08X pet 0x%016llX target 0x%016llX consumed %u/%u\n",
            what, action, (unsigned long long)pet.GetRawValue(),
            (unsigned long long)target.GetRawValue(),
            unsigned(packet.rpos()), unsigned(packet.size()));
        ++g_fail;
    }
}

static void test_pet_action_matches_retail_bodies()
{
    // Command actions carry no target. UNIT_ACTION_BUTTON_TYPE 0x07 is ACT_COMMAND.
    // Action 3 is COMMAND_ABANDON, not COMMAND_STAY -- this was mislabelled when
    // the fixture landed. The distinction matters: for a hunter pet the abandon
    // path unsummons with PET_SAVE_AS_DELETED, which is permanent.
    uint8_t const abandon[] = {
        0x03, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x79, 0x04, 0xF0, 0x43, 0x0D, 0x9A, 0x29, 0x02
    };
    CheckPetAction("pet action abandon", abandon, sizeof(abandon),
        0x07000003u, UINT64_C(0xF1420C9B28000003), 0);

    uint8_t const follow[] = {
        0x01, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x79, 0x04, 0xF0, 0x43, 0x0D, 0x9B, 0xB8, 0x5E
    };
    CheckPetAction("pet action follow", follow, sizeof(follow),
        0x07000001u, UINT64_C(0xF1420C9AB900005F), 0);

    // The only target-bearing shape sampled: fifteen of the sixteen bits set.
    uint8_t const attack[] = {
        0x02, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0xBF, 0xF0, 0x40, 0x76, 0x03, 0x73, 0x8C, 0xEF, 0x30, 0x3C, 0xA0,
        0xF0, 0x05, 0x31, 0xAF, 0x28
    };
    CheckPetAction("pet action attack", attack, sizeof(attack),
        0x07000002u, UINT64_C(0xF141728D31027729), UINT64_C(0xF130EE0400AEA13D));

    // The only body of this build carrying a non-zero position. It is what
    // identifies the middle slot as z, by coordinate BAND rather than by exact
    // match: the movement bodies alongside it span x 1383..1501, y 548..775 and
    // z 246.835..246.866, and this tuple is (772.5943, 246.8356, 1473.7810). An
    // earlier comment here claimed every neighbour reported 246.8356 exactly;
    // that was wrong, they vary in the fourth decimal and one by more.
    //
    // The same bands place the first value in the y range and the third in the x
    // range, so y, z, x is well supported. It is not proven, since no body ties
    // the tuple to a known actor, and the server does not consume the position.
    uint8_t const moveTo[] = {
        0x04, 0x00, 0x00, 0x07,
        0x09, 0x26, 0x41, 0x44,
        0xEA, 0xD5, 0x76, 0x43,
        0xFE, 0x38, 0xB8, 0x44,
        0x79, 0x04, 0xF0, 0x43, 0x0D, 0x9B, 0xB8, 0x02
    };
    CheckPetAction("pet action move to", moveTo, sizeof(moveTo),
        0x07000004u, UINT64_C(0xF1420C9AB9000003), 0);

    // Masks 0x7DAE and 0x7DAF: identical but for one bit, same pet, targets that
    // differ only in the byte that bit admits. This is the pair that pins the
    // bit order rather than merely reproducing it.
    uint8_t const targetBitClear[] = {
        0x02, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x7D, 0xAE, 0xF0, 0x43, 0x0D, 0x9B, 0x1D, 0xB8, 0xD5, 0xF0, 0x19, 0x30,
        0x00
    };
    CheckPetAction("pet action target bit clear", targetBitClear, sizeof(targetBitClear),
        0x07000002u, UINT64_C(0xF1420C9AB9000001), UINT64_C(0xF1311C18000000D4));

    uint8_t const targetBitSet[] = {
        0x02, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x7D, 0xAF, 0xF0, 0x43, 0x0D, 0x9B, 0x1D, 0xB8, 0x2E, 0x00, 0xF0, 0x19,
        0x30, 0x00
    };
    CheckPetAction("pet action target bit set", targetBitSet, sizeof(targetBitSet),
        0x07000002u, UINT64_C(0xF1420C9AB9000001), UINT64_C(0xF1311C180000012F));

    // Mask 0xFEBF, and the mover is a VEHICLE (0xF15), not a pet. These bodies
    // are why HandlePetAction guards its Pet downcasts: ordinary traffic drives
    // a non-pet through the command path.
    uint8_t const vehicleMover[] = {
        0x02, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFE, 0xBF, 0xF0, 0x51, 0x1E, 0x22, 0x83, 0x24, 0x82, 0xD4, 0x74, 0xF0,
        0x18, 0x31, 0x23, 0xD6
    };
    CheckPetAction("pet action vehicle mover", vehicleMover, sizeof(vehicleMover),
        0x07000002u, UINT64_C(0xF150822500231FD7), UINT64_C(0xF1308319002275D5));

    uint8_t const vehicleMoverTwo[] = {
        0x02, 0x00, 0x00, 0x07,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFE, 0xBF, 0xF0, 0x51, 0x68, 0x44, 0x83, 0x27, 0x82, 0x62, 0xB8, 0xF0,
        0x16, 0x31, 0x41, 0xC2
    };
    CheckPetAction("pet action vehicle mover two", vehicleMoverTwo, sizeof(vehicleMoverTwo),
        0x07000002u, UINT64_C(0xF1508226004569C3), UINT64_C(0xF13083170040B963));

    // Pin the position itself, so a reordering of the three reads is caught.
    WorldPacket packet(CMSG_PET_ACTION, uint32(sizeof(moveTo)));
    packet.append(moveTo, sizeof(moveTo));

    uint32 action = 0;
    float posY = 0.0f, posZ = 0.0f, posX = 0.0f;
    ObjectGuid pet;
    ObjectGuid target;
    MopCompactPackets::ReadPetAction(packet, action, posY, posZ, posX, pet, target);

    float expectedY, expectedZ, expectedX;
    uint32 const bitsY = 0x44412609u;
    uint32 const bitsZ = 0x4376D5EAu;
    uint32 const bitsX = 0x44B838FEu;
    std::memcpy(&expectedY, &bitsY, sizeof(expectedY));
    std::memcpy(&expectedZ, &bitsZ, sizeof(expectedZ));
    std::memcpy(&expectedX, &bitsX, sizeof(expectedX));
    CHECK(posY == expectedY);
    CHECK(posZ == expectedZ);
    CHECK(posX == expectedX);
}

/// CMSG_PET_NAME_QUERY and its response, pinned to real 18414 bodies.
///
/// The request carries sixteen presence bits interleaved across the pet GUID and
/// the pet number. Four bodies across three distinct masks -- 0xB7DA, 0xB79A and
/// 0xB7DE.
///
/// That is WEAKER than it looks and an earlier version of this comment overstated
/// it. Because a pet number is small, its top four bytes are absent in every
/// body, and ten further slots are present in all of them, so a review counted
/// 87,091,200 equivalent bit orders. Only number[3] and pet[2] are pinned. These
/// fixtures prove the reader consumes real bodies exactly and recovers the right
/// values for them; they do not prove the interleave.
///
/// The response is tied to the request by evidence rather than by assumption:
/// the "Blue" body below is the actual reply to the first request body, three
/// packets later in the same capture, and the pet number it echoes is the one
/// that falls out of that request. Decoding the two independently and finding
/// the same number is what confirms the pair.
static void test_pet_name_query_matches_retail_bodies()
{
    struct Request
    {
        char const* what;
        uint8_t body[16];
        size_t length;
        uint64 guid;
        uint64 number;
    };

    Request const requests[] = {
        { "pet name query mask B7DA",
          { 0xB7, 0xDA, 0x8F, 0x41, 0x30, 0x8F, 0x61, 0x41, 0x40, 0x00, 0x30, 0x10, 0xF0 },
          13, UINT64_C(0xF1418E4031001160), 26099761 },
        { "pet name query mask B79A",
          { 0xB7, 0x9A, 0xE1, 0x10, 0xC2, 0xE1, 0xAE, 0x10, 0x41, 0xC2, 0x0E, 0xF0 },
          12, UINT64_C(0xF140E011C3000FAF), 14684611 },
        { "pet name query mask B7DE",
          { 0xB7, 0xDE, 0x6A, 0xF8, 0xB3, 0x6A, 0x5C, 0xF8, 0x03, 0x40, 0x00, 0xB3, 0x04, 0xF0 },
          14, UINT64_C(0xF1416BF9B202055D), 23853490 },
        { "pet name query second B7DA",
          { 0xB7, 0xDA, 0x3C, 0x32, 0x24, 0x3C, 0x7B, 0x32, 0x40, 0x00, 0x24, 0xDF, 0xF0 },
          13, UINT64_C(0xF1413D332500DE7A), 20788005 },
    };

    for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); ++i)
    {
        Request const& r = requests[i];
        WorldPacket packet(CMSG_PET_NAME_QUERY, uint32(r.length));
        packet.append(r.body, r.length);

        ObjectGuid guid;
        uint64 number = 0;
        MopCompactPackets::ReadPetNameQuery(packet, guid, number);

        if (guid.GetRawValue() != r.guid || number != r.number ||
            packet.rpos() != packet.size())
        {
            std::fprintf(stderr,
                "FAIL %s: guid 0x%016llX number %llu consumed %u/%u\n",
                r.what, (unsigned long long)guid.GetRawValue(),
                (unsigned long long)number,
                unsigned(packet.rpos()), unsigned(packet.size()));
            ++g_fail;
        }
    }

    {   // The reply to the first request above, from the same capture.
        std::string name = "Blue";
        WorldPacket p(SMSG_PET_NAME_QUERY_RESPONSE, 22);
        MopCompactPackets::BuildPetNameQueryResponse(p, 26099761, &name,
            1403635742u, NULL);
        CHECK(BytesEqual(p, {
            0x80,                                           // hasData, then 5x7 declined lengths,
            0x00, 0x00, 0x00, 0x00, 0x20,                   // one spare bit, and name length 4
            0x42, 0x6C, 0x75, 0x65,                         // "Blue", unterminated
            0x1E, 0xC8, 0xA9, 0x53,                         // timestamp
            0x31, 0x40, 0x8E, 0x01, 0x00, 0x00, 0x00, 0x00  // pet number, eight bytes, trailing
        }));
    }
    {   // A longer name, which moves the length field's low bits.
        std::string name = "Werenika";
        WorldPacket p(SMSG_PET_NAME_QUERY_RESPONSE, 26);
        MopCompactPackets::BuildPetNameQueryResponse(p, 23853490, &name, 0, NULL);
        CHECK(BytesEqual(p, {
            0x80, 0x00, 0x00, 0x00, 0x00, 0x40,
            0x57, 0x65, 0x72, 0x65, 0x6E, 0x69, 0x6B, 0x61,
            0x00, 0x00, 0x00, 0x00,
            0xB2, 0xF9, 0x6B, 0x01, 0x00, 0x00, 0x00, 0x00
        }));
    }
    {   // No pet found: one clear bit, then the number echoed so the client can
        // retire the request. Reader-derived shape; no observed body of this form.
        WorldPacket p(SMSG_PET_NAME_QUERY_RESPONSE, 9);
        MopCompactPackets::BuildPetNameQueryResponse(p, 26099761, NULL, 0, NULL);
        CHECK(BytesEqual(p, {
            0x00,
            0x31, 0x40, 0x8E, 0x01, 0x00, 0x00, 0x00, 0x00
        }));
    }
}

/// CMSG_LFG_JOIN, pinned to real 18414 bodies across the observed count range.
///
/// The interesting field is the 22-bit dungeon count, which is packed with an
/// 8-bit comment length and a flag into one 32-bit block. Counts of 1, 6 and 15
/// exercise the low bits of that field, and each body must consume exactly.
///
/// Every decoded slot carries its LFG type in the high byte and the dungeon id
/// in the low three, which is the check that does not come from the packet: the
/// ids land in a plausible LFGDungeons range and the type tags are uniform
/// within a request, as a queue built from one category should be.
static void test_lfg_join_matches_retail_bodies()
{
    struct Case
    {
        char const* what;
        uint8_t const* body;
        size_t length;
        uint32 roles;
        size_t dungeons;
        uint32 firstId;
        uint32 firstType;
    };

    static uint8_t const oneDungeon[] = {
        0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x03, 0x01, 0x00,
        0x06
    };
    static uint8_t const sixDungeons[] = {
        0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x02, 0x8E, 0x02, 0x00,
        0x01, 0x86, 0x02, 0x00, 0x01, 0x1B, 0x02, 0x00, 0x01, 0xF8, 0x01, 0x00,
        0x01, 0x4A, 0x02, 0x00, 0x01, 0x53, 0x02, 0x00, 0x01
    };
    static uint8_t const fifteenDungeons[] = {
        0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x02, 0x05, 0x02, 0x00,
        0x01, 0x6B, 0x02, 0x00, 0x01, 0xFF, 0x01, 0x00, 0x01, 0x19, 0x02, 0x00,
        0x01, 0x8E, 0x02, 0x00, 0x01, 0x86, 0x02, 0x00, 0x01, 0x1B, 0x02, 0x00,
        0x01, 0xF8, 0x01, 0x00, 0x01, 0x4A, 0x02, 0x00, 0x01, 0x87, 0x02, 0x00,
        0x01, 0x53, 0x02, 0x00, 0x01, 0xEC, 0x01, 0x00, 0x01, 0x89, 0x02, 0x00,
        0x01, 0x37, 0x02, 0x00, 0x01, 0xF3, 0x01, 0x00, 0x01
    };

    Case const cases[] = {
        { "lfg join one dungeon", oneDungeon, sizeof(oneDungeon), 7, 1, 259, 6 },
        { "lfg join six dungeons", sixDungeons, sizeof(sixDungeons), 8, 6, 654, 1 },
        { "lfg join fifteen dungeons", fifteenDungeons, sizeof(fifteenDungeons), 8, 15, 517, 1 },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        Case const& c = cases[i];
        WorldPacket packet(CMSG_LFG_JOIN, uint32(c.length));
        packet.append(c.body, c.length);

        uint8 partyIndex = 0;
        uint32 roles = 0, flag = 0;
        std::vector<uint32> dungeons;
        std::string comment;

        bool const ok = MopCompactPackets::ReadLfgJoin(packet, partyIndex, roles,
            flag, dungeons, comment);

        if (!ok || partyIndex != 0x7F || roles != c.roles ||
            dungeons.size() != c.dungeons || flag != 1 || !comment.empty() ||
            packet.rpos() != packet.size() ||
            (dungeons[0] & 0x00FFFFFF) != c.firstId || (dungeons[0] >> 24) != c.firstType)
        {
            std::fprintf(stderr,
                "FAIL %s: ok=%d party=0x%02X roles=%u count=%u flag=%u consumed %u/%u\n",
                c.what, int(ok), unsigned(partyIndex), roles,
                unsigned(dungeons.size()), flag,
                unsigned(packet.rpos()), unsigned(packet.size()));
            ++g_fail;
        }
    }

    {   // A claimed count that the body cannot hold must be refused outright,
        // not resized from. The count field is 22 bits, so this is 0x3FFFFF
        // dungeons in a body with four bytes left.
        uint8_t const liar[] = {
            0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x07, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFC, 0x02, 0x00, 0x00, 0x00,
            0x00
        };
        WorldPacket packet(CMSG_LFG_JOIN, uint32(sizeof(liar)));
        packet.append(liar, sizeof(liar));

        uint8 partyIndex = 0;
        uint32 roles = 0, flag = 0;
        std::vector<uint32> dungeons;
        std::string comment;
        CHECK(!MopCompactPackets::ReadLfgJoin(packet, partyIndex, roles, flag,
                                              dungeons, comment));
        CHECK(dungeons.empty());
    }
    {   // The grammar's total is exact, so a body claiming ONE dungeon while
        // carrying an extra trailing byte is malformed and must be refused too.
        // Accepting it would leave unread suffix data behind a successful parse.
        uint8_t const trailing[] = {
            0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x02, 0x03, 0x01, 0x00,
            0x06, 0xEE
        };
        WorldPacket packet(CMSG_LFG_JOIN, uint32(sizeof(trailing)));
        packet.append(trailing, sizeof(trailing));

        uint8 partyIndex = 0;
        uint32 roles = 0, flag = 0;
        std::vector<uint32> dungeons;
        std::string comment;
        CHECK(!MopCompactPackets::ReadLfgJoin(packet, partyIndex, roles, flag,
                                              dungeons, comment));
    }
}

/// The mailbox family and the guild-bank tab query, pinned to real 18414 bodies.
///
/// The cross-opcode evidence is what makes these strong. capture-000879 and
/// capture-000025 each show one session where the SAME mailbox is recovered from
/// GET_MAIL_LIST, MARK_AS_READ and TAKE_ITEM -- three different mask orders and
/// three different byte orders -- and all three yield the same GUID byte for
/// byte. A wrong byte order permutes distinct byte values, so agreement across
/// three independent orders is evidence no single opcode's fixtures could give.
/// The mail id likewise matches between MARK_AS_READ and TAKE_ITEM.
///
/// 0x1372 also settles a naming disagreement. The reference overlay carried it
/// as CMSG_LFG_GET_PARTY_INFO; its bodies decode to a HIGHGUID_GAMEOBJECT guid,
/// a tab index and a send-all-slots boolean, which is a guild bank query and is
/// nothing an LFG party-info request would carry.
static void test_mail_family_matches_retail_bodies()
{
    {   // GET_MAIL_LIST: three distinct masks, three body lengths.
        struct Case { char const* what; uint8_t body[8]; size_t length; uint64 guid; };
        Case const cases[] = {
            { "get mail list B9", { 0xB9, 0xF0, 0x12, 0x5A, 0xA5, 0x31 }, 6,
              UINT64_C(0xF1135BA400000030) },
            { "get mail list B5", { 0xB5, 0xF0, 0x05, 0x12, 0x39, 0x04 }, 6,
              UINT64_C(0xF113380000000405) },
            { "get mail list BD", { 0xBD, 0xF0, 0x06, 0x12, 0x3D, 0x96, 0x14 }, 7,
              UINT64_C(0xF1133C9700000715) },
        };
        for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        {
            WorldPacket p(CMSG_GET_MAIL_LIST, uint32(cases[i].length));
            p.append(cases[i].body, cases[i].length);
            ObjectGuid const guid = MopCompactPackets::ReadGetMailList(p);
            if (guid.GetRawValue() != cases[i].guid || p.rpos() != p.size())
            {
                std::fprintf(stderr, "FAIL %s: 0x%016llX consumed %u/%u\n", cases[i].what,
                             (unsigned long long)guid.GetRawValue(),
                             unsigned(p.rpos()), unsigned(p.size()));
                ++g_fail;
            }
        }
    }
    {   // MARK_AS_READ, same two mailboxes as above through a different order.
        uint8_t const a[] = { 0x1F, 0x37, 0x7C, 0x57, 0x8E, 0x80, 0xF0, 0x5A, 0x12, 0xA5, 0x31 };
        WorldPacket p(CMSG_MAIL_MARK_AS_READ, sizeof(a));
        p.append(a, sizeof(a));
        uint32 mailId = 0;
        ObjectGuid const guid = MopCompactPackets::ReadMailMarkAsRead(p, mailId);
        CHECK(mailId == 1467758367u);
        CHECK(guid.GetRawValue() == UINT64_C(0xF1135BA400000030));
        CHECK(p.rpos() == p.size());

        uint8_t const b[] = { 0x16, 0xC5, 0x7C, 0x57, 0x8F, 0x80, 0x06, 0xF0, 0x3D, 0x12, 0x96, 0x14 };
        WorldPacket q(CMSG_MAIL_MARK_AS_READ, sizeof(b));
        q.append(b, sizeof(b));
        uint32 mailIdB = 0;
        ObjectGuid const guidB = MopCompactPackets::ReadMailMarkAsRead(q, mailIdB);
        CHECK(mailIdB == 1467794710u);
        CHECK(guidB.GetRawValue() == UINT64_C(0xF1133C9700000715));
        CHECK(q.rpos() == q.size());
    }
    {   // TAKE_ITEM: same mailboxes and the same mail ids again, third order.
        uint8_t const a[] = { 0x1F, 0x37, 0x7C, 0x57, 0xBE, 0x6D, 0x86, 0x39,
                              0xCB, 0x31, 0xA5, 0x5A, 0x12, 0xF0 };
        WorldPacket p(CMSG_MAIL_TAKE_ITEM, sizeof(a));
        p.append(a, sizeof(a));
        uint32 mailId = 0, itemId = 0;
        ObjectGuid const guid = MopCompactPackets::ReadMailTakeItem(p, mailId, itemId);
        CHECK(mailId == 1467758367u);                       // as MARK_AS_READ above
        CHECK(itemId == 965111230u);
        CHECK(guid.GetRawValue() == UINT64_C(0xF1135BA400000030));
        CHECK(p.rpos() == p.size());

        uint8_t const b[] = { 0x16, 0xC5, 0x7C, 0x57, 0xCE, 0x00, 0x87, 0x39,
                              0xCF, 0x14, 0x06, 0x96, 0x3D, 0x12, 0xF0 };
        WorldPacket q(CMSG_MAIL_TAKE_ITEM, sizeof(b));
        q.append(b, sizeof(b));
        uint32 mailIdB = 0, itemIdB = 0;
        ObjectGuid const guidB = MopCompactPackets::ReadMailTakeItem(q, mailIdB, itemIdB);
        CHECK(mailIdB == 1467794710u);
        CHECK(itemIdB == 965148878u);
        CHECK(guidB.GetRawValue() == UINT64_C(0xF1133C9700000715));
        CHECK(q.rpos() == q.size());
    }
    {   // GUILD_BANK_QUERY_TAB. The first two differ ONLY in the send-all-slots
        // bit and the tab id, which is what proves that bit is a standalone
        // boolean and not a ninth GUID presence bit.
        uint8_t const noSlots[] = { 0x00, 0x97, 0x80, 0xF0, 0x12, 0x70, 0x43, 0x11, 0x06 };
        WorldPacket p(CMSG_GUILD_BANK_QUERY_TAB, sizeof(noSlots));
        p.append(noSlots, sizeof(noSlots));
        uint8 tabId = 0xFF;
        bool sendAll = true;
        ObjectGuid const guid = MopCompactPackets::ReadGuildBankQueryTab(p, tabId, sendAll);
        CHECK(tabId == 0);
        CHECK(sendAll == false);
        CHECK(guid.GetRawValue() == UINT64_C(0xF113427100000710));
        CHECK(p.rpos() == p.size());

        uint8_t const allSlots[] = { 0x03, 0xB7, 0x80, 0xF0, 0x12, 0x70, 0x43, 0x11, 0x06 };
        WorldPacket q(CMSG_GUILD_BANK_QUERY_TAB, sizeof(allSlots));
        q.append(allSlots, sizeof(allSlots));
        uint8 tabIdB = 0xFF;
        bool sendAllB = false;
        ObjectGuid const guidB = MopCompactPackets::ReadGuildBankQueryTab(q, tabIdB, sendAllB);
        CHECK(tabIdB == 3);
        CHECK(sendAllB == true);
        CHECK(guidB.GetRawValue() == UINT64_C(0xF113427100000710));  // same bank
        CHECK(q.rpos() == q.size());

        uint8_t const other[] = { 0x00, 0x9F, 0x80, 0xF0, 0x12, 0x0B, 0x12, 0x26, 0x74, 0xD6 };
        WorldPacket r(CMSG_GUILD_BANK_QUERY_TAB, sizeof(other));
        r.append(other, sizeof(other));
        uint8 tabIdC = 0xFF;
        bool sendAllC = true;
        ObjectGuid const guidC = MopCompactPackets::ReadGuildBankQueryTab(r, tabIdC, sendAllC);
        CHECK(tabIdC == 0);
        CHECK(guidC.GetRawValue() == UINT64_C(0xF113270A0013D775));
        CHECK(r.rpos() == r.size());
    }
}

/// CMSG_TOTEM_DESTROYED and CMSG_SET_ACTION_BUTTON, pinned to real 18414 bodies.
///
/// Both are a plain byte, a mask byte, then the present bytes of a packed value.
/// Four distinct masks each. The action button is additionally byte-for-byte
/// identical to the client's writer sub_669CAE, which is what proves its orders
/// rather than merely constraining them.
///
/// The empty action-button body is the important one: mask 0x00 means every byte
/// of the packed value is absent, so the whole request is two bytes and clears
/// the slot. A reader expecting a fixed uint32 cannot express that at all.
static void test_totem_and_action_button_bodies()
{
    struct Totem { char const* what; uint8_t body[9]; size_t length; uint8 slot; uint64 guid; };
    Totem const totems[] = {
        { "totem mask A7", { 0x00, 0xA7, 0x31, 0x0C, 0x92, 0x67, 0xF0 }, 7, 0,
          UINT64_C(0xF130660D00009300) },
        { "totem mask 8F", { 0x00, 0x8F, 0x31, 0x07, 0xE9, 0x80, 0xF0 }, 7, 0,
          UINT64_C(0xF130E80600000081) },
        { "totem mask AF", { 0x01, 0xAF, 0x31, 0x20, 0x09, 0xBB, 0x84, 0xF0 }, 8, 1,
          UINT64_C(0xF130BA2100000885) },
        { "totem mask E7", { 0x00, 0xE7, 0x31, 0x26, 0xEA, 0xB6, 0xA8, 0xF0 }, 8, 0,
          UINT64_C(0xF130A9EB0027B700) },
    };
    for (size_t i = 0; i < sizeof(totems) / sizeof(totems[0]); ++i)
    {
        WorldPacket p(CMSG_TOTEM_DESTROYED, uint32(totems[i].length));
        p.append(totems[i].body, totems[i].length);
        uint8 slot = 0xFF;
        ObjectGuid const guid = MopCompactPackets::ReadTotemDestroyed(p, slot);
        if (slot != totems[i].slot || guid.GetRawValue() != totems[i].guid ||
            p.rpos() != p.size())
        {
            std::fprintf(stderr, "FAIL %s: slot %u guid 0x%016llX consumed %u/%u\n",
                         totems[i].what, unsigned(slot),
                         (unsigned long long)guid.GetRawValue(),
                         unsigned(p.rpos()), unsigned(p.size()));
            ++g_fail;
        }
        // Every totem here is a creature, which is the check outside the packet.
        CHECK((guid.GetRawValue() >> 52) == 0xF13);
    }

    struct Button { char const* what; uint8_t body[6]; size_t length; uint8 slot; uint32 action; };
    Button const buttons[] = {
        { "action button cleared", { 0x0C, 0x00 }, 2, 12, 0 },
        { "action button mask 40", { 0x09, 0x40, 0x8A }, 3, 9, 139 },
        { "action button mask 48", { 0x00, 0x48, 0x00, 0x92 }, 4, 0, 403 },
        { "action button mask 18", { 0x3F, 0x18, 0x00, 0xA6 }, 4, 63, 108288 },
    };
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
    {
        WorldPacket p(CMSG_SET_ACTION_BUTTON, uint32(buttons[i].length));
        p.append(buttons[i].body, buttons[i].length);
        uint8 slot = 0xFF, type = 0xFF;
        uint32 action = 0xFFFFFFFF;
        MopCompactPackets::ReadSetActionButton(p, slot, action, type);
        if (slot != buttons[i].slot || action != buttons[i].action || type != 0 ||
            p.rpos() != p.size())
        {
            std::fprintf(stderr, "FAIL %s: slot %u action %u type %u consumed %u/%u\n",
                         buttons[i].what, unsigned(slot), action, unsigned(type),
                         unsigned(p.rpos()), unsigned(p.size()));
            ++g_fail;
        }
    }

    {   // CONSTRUCTED, not observed: no corpus body sets the action's high byte,
        // so this one is built to the layout to pin the field's WIDTH. The action
        // occupies the full low 32 bits, and the inherited macro cut it at 24, so
        // a value above 0x00FFFFFF would have been silently truncated.
        //
        // Mask 0x42 marks byte0 (bit 6) and byte3 (bit 1) present. The byte order
        // reaches byte3 before byte0, and each is sent XOR 1, giving 0x03 then
        // 0x00 for the real values 0x02 and 0x01.
        uint8_t const wide[] = { 0x01, 0x42, 0x03, 0x00 };
        WorldPacket p(CMSG_SET_ACTION_BUTTON, sizeof(wide));
        p.append(wide, sizeof(wide));
        uint8 slot = 0, type = 0;
        uint32 action = 0;
        MopCompactPackets::ReadSetActionButton(p, slot, action, type);
        CHECK(p.rpos() == p.size());
        CHECK(slot == 0x01);
        CHECK(action == 0x02000001u);                       // survives past bit 24
        CHECK(type == 0);
    }
}

/// SMSG_SEND_MAIL_RESULT, pinned to real 18414 bodies.
///
/// Always 24 bytes, six little-endian uint32. The inherited sender wrote three
/// of them and made the rest conditional, so it produced 12, 16 or 20 bytes in a
/// different order.
///
/// The equip-error body is the one that discriminates the field order. Across
/// every body of this build, a non-zero word at offset 4 occurs in exactly the
/// bodies carrying 1 at offset 8 -- an equip error is only meaningful alongside
/// MAIL_ERR_EQUIP_ERROR. Swap the two and 50 lands in a field whose range stops
/// around 21, on a take that also reports success.
static void test_send_mail_result_matches_retail_bodies()
{
    {   // An item taken successfully: action 2, one item, no error.
        WorldPacket p(SMSG_SEND_MAIL_RESULT, 24);
        MopCompactPackets::BuildSendMailResult(p, 1467794710u, 0, 0, 2, 965148878u, 1);
        CHECK(BytesEqual(p, {
            0x16, 0xC5, 0x7C, 0x57,                         // mailId
            0x00, 0x00, 0x00, 0x00,                         // equipError
            0x00, 0x00, 0x00, 0x00,                         // mailError
            0x02, 0x00, 0x00, 0x00,                         // mailAction
            0xCE, 0x00, 0x87, 0x39,                         // itemGuidLow
            0x01, 0x00, 0x00, 0x00                          // itemCount
        }));
    }
    {   // The discriminating body: equipError 50 with mailError 1, and the take
        // reports no items because it failed.
        WorldPacket p(SMSG_SEND_MAIL_RESULT, 24);
        MopCompactPackets::BuildSendMailResult(p, 1442599846u, 50, 1, 2, 938134456u, 0);
        CHECK(BytesEqual(p, {
            0xA6, 0x53, 0xFC, 0x55,
            0x32, 0x00, 0x00, 0x00,
            0x01, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00,
            0xB8, 0xCB, 0xEA, 0x37,
            0x00, 0x00, 0x00, 0x00
        }));
    }
    {   // A send that created no mail: everything zero but the error.
        WorldPacket p(SMSG_SEND_MAIL_RESULT, 24);
        MopCompactPackets::BuildSendMailResult(p, 0, 0, 2, 0, 0, 0);
        CHECK(BytesEqual(p, {
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        }));
    }
    {   // Returned to sender: action 4, no item fields.
        WorldPacket p(SMSG_SEND_MAIL_RESULT, 24);
        MopCompactPackets::BuildSendMailResult(p, 1467794710u, 0, 0, 4, 0, 0);
        CHECK(p.size() == 24);                              // never conditional
        CHECK(BytesEqual(p, {
            0x16, 0xC5, 0x7C, 0x57,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x04, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        }));
    }
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
    test_speed_family_full_interleaves_reader_derived();
    test_flight_speed_scalars_precede_mask();
    test_spline_run_speed_matches_retail_body();
    test_spline_speed_family_matches_retail_bodies();
    test_spline_speed_family_full_interleaves_reader_derived();
    test_direct_speed_family_mask_order();
    test_spline_speed_family_mask_order();
    test_pet_action_matches_retail_bodies();
    test_pet_name_query_matches_retail_bodies();
    test_lfg_join_matches_retail_bodies();
    test_mail_family_matches_retail_bodies();
    test_send_mail_result_matches_retail_bodies();
    test_totem_and_action_button_bodies();
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
