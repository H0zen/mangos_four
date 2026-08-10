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
 * Byte-exact tests for the 5.4.8 core movement layouts.
 *
 * The local sequences below are independent transcriptions of the direct
 * 18414 reader/writer operation orders. They never inspect production arrays.
 */

#include "Unit.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "movement/packet_builder.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

char const* LookupOpcodeName(PacketDirection, uint16)
{
    return "TEST_MOVEMENT_OPCODE";
}

enum class RefOp
{
    Flags, Flags2, Timestamp, HasPitch,
    GuidBit0, GuidBit1, GuidBit2, GuidBit3, GuidBit4, GuidBit5, GuidBit6, GuidBit7,
    Raw148, Raw149, Raw172, HasFlags, HasFlags2, HasTimestamp, HasOrientation,
    HasFall, HasFallDirection, HasTransport, HasTransportTime2, HasTransportTime3,
    TransportBit0, TransportBit1, TransportBit2, TransportBit3,
    TransportBit4, TransportBit5, TransportBit6, TransportBit7,
    HasSplineElevation, PositionX, PositionY, PositionZ, PositionO,
    GuidByte0, GuidByte1, GuidByte2, GuidByte3, GuidByte4, GuidByte5, GuidByte6, GuidByte7,
    Pitch, FallTime, TransportByte0, TransportByte1, TransportByte2, TransportByte3,
    TransportByte4, TransportByte5, TransportByte6, TransportByte7,
    SplineElevation, FallHorizontal, FallVertical, FallCos, FallSin,
    TransportSeat, TransportO, TransportX, TransportY, TransportZ,
    TransportTime, TransportTime2, TransportTime3,
    ForceCount, ForceIds, MovementCounter, HasUnknownUInt32, UnknownUInt32, End
};

struct RefState
{
    uint64 guid = 0x8070605040302010ull;
    uint32 movementCounter = 0xCAFEBABEu;
    uint32 flags = 0x12345678u;
    uint16 flags2 = 0x1ABCu;
    uint32 timestamp = 0x11223344u;
    float x = 1.25f, y = -2.5f, z = 3.75f, o = 0.5f;
    uint64 transportGuid = 0x8877665544332211ull;
    float tx = 4.25f, ty = 5.5f, tz = -6.75f, to = 0.75f;
    uint32 transportTime = 0x21324354u;
    uint32 transportTime2 = 0x65768798u;
    uint32 transportTime3 = 0xA9BACBDCu;
    int8 transportSeat = 3;
    float pitch = 0.25f;
    uint32 fallTime = 0x10203040u;
    float fallVertical = -7.5f, fallHorizontal = 8.5f, fallCos = 0.125f, fallSin = -0.25f;
    float splineElevation = 9.25f;
    bool raw148 = true, raw149 = false, raw172 = true;
    std::vector<uint32> forceIds { 0x0A0B0C0Du, 0xA1A2A3A4u };
    bool hasUnknownUInt32 = true;
    uint32 unknownUInt32 = 0x55667788u;
    bool hasFlags = true, hasFlags2 = true, hasTimestamp = true;
    bool hasOrientation = true, hasPitch = true, hasFall = true;
    bool hasFallDirection = true, hasTransport = true;
    bool hasTransportTime2 = true, hasTransportTime3 = true;
    bool hasSplineElevation = true;
};

class RefWriter
{
public:
    void Bit(bool value)
    {
        if (m_bit == 8)
        {
            m_bytes.push_back(0);
            m_bit = 0;
        }
        if (value)
            m_bytes.back() |= uint8(0x80u >> m_bit);
        ++m_bit;
    }

    void Bits(uint32 value, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
            Bit((value & (uint32(1) << (count - i - 1))) != 0);
    }

    void Align() { m_bit = 8; }
    void U8(uint8 value) { Align(); m_bytes.push_back(value); }
    void U32(uint32 value)
    {
        Align();
        for (size_t i = 0; i < 4; ++i)
            m_bytes.push_back(uint8(value >> (8 * i)));
    }
    void F32(float value)
    {
        uint32 raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        U32(raw);
    }
    void GuidByte(uint64 value, size_t index)
    {
        uint8 const byte = uint8(value >> (8 * index));
        if (byte)
            U8(byte ^ 1);
    }
    std::vector<uint8> const& Bytes() const { return m_bytes; }

private:
    std::vector<uint8> m_bytes;
    size_t m_bit = 8;
};

#define G(n) RefOp::GuidBit##n
#define GB(n) RefOp::GuidByte##n
#define T(n) RefOp::TransportBit##n
#define TB(n) RefOp::TransportByte##n

static RefOp const kStartForward[] = {
    RefOp::PositionZ, RefOp::PositionX, RefOp::PositionY, RefOp::HasFlags2, RefOp::Raw149,
    RefOp::HasUnknownUInt32, RefOp::Raw148, G(0), RefOp::HasOrientation, RefOp::HasFall,
    RefOp::ForceCount, G(4), G(1), RefOp::HasTimestamp, G(7), RefOp::HasPitch,
    RefOp::HasTransport, G(5), RefOp::HasFlags, G(3), RefOp::HasSplineElevation, G(2), G(6),
    RefOp::Raw172, T(1), RefOp::HasTransportTime3, T(3), T(4), T(2), T(5), T(0), T(7), T(6),
    RefOp::HasTransportTime2, RefOp::HasFallDirection, RefOp::Flags2, RefOp::Flags,
    GB(1), GB(6), GB(7), RefOp::ForceIds, GB(5), GB(0), GB(3), GB(2), GB(4),
    TB(3), TB(1), TB(6), RefOp::TransportZ, TB(4), RefOp::TransportTime3, RefOp::TransportSeat,
    TB(7), RefOp::TransportO, RefOp::TransportTime2, TB(5), TB(2), RefOp::TransportX, TB(0),
    RefOp::TransportY, RefOp::TransportTime, RefOp::FallCos, RefOp::FallSin,
    RefOp::FallHorizontal, RefOp::FallTime, RefOp::FallVertical, RefOp::Timestamp,
    RefOp::Pitch, RefOp::SplineElevation, RefOp::PositionO, RefOp::UnknownUInt32, RefOp::End };

static RefOp const kStartBackward[] = {
    RefOp::PositionY, RefOp::PositionZ, RefOp::PositionX, RefOp::HasTimestamp,
    RefOp::HasOrientation, G(7), G(2), RefOp::ForceCount, RefOp::HasFall, RefOp::Raw172,
    G(5), G(3), G(6), RefOp::HasSplineElevation, G(4), RefOp::HasTransport, G(0),
    RefOp::HasFlags, RefOp::HasPitch, RefOp::HasUnknownUInt32, RefOp::HasFlags2,
    RefOp::Raw148, G(1), RefOp::Raw149, T(1), RefOp::HasTransportTime2, T(0), T(7),
    RefOp::HasTransportTime3, T(3), T(5), T(6), T(2), T(4), RefOp::Flags2, RefOp::Flags,
    RefOp::HasFallDirection, RefOp::ForceIds, GB(1), GB(3), GB(5), GB(2), GB(0), GB(4), GB(7), GB(6),
    RefOp::UnknownUInt32, RefOp::TransportTime, TB(4), TB(1), TB(5), TB(3), TB(6), RefOp::TransportSeat,
    RefOp::TransportO, RefOp::TransportX, TB(0), RefOp::TransportY, RefOp::TransportTime3, TB(7),
    RefOp::TransportTime2, RefOp::TransportZ, TB(2), RefOp::PositionO, RefOp::FallTime,
    RefOp::FallSin, RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallVertical, RefOp::Pitch,
    RefOp::Timestamp, RefOp::SplineElevation, RefOp::End };

// Fall pair untransposed to match the client's writer; see
// MovementStructures.h. +124 is cosAngle, +128 is sinAngle.
static RefOp const kStartStrafeLeft[] = {
    RefOp::PositionY, RefOp::PositionZ, RefOp::PositionX, G(0), RefOp::HasTimestamp,
    G(3), RefOp::HasFlags2, RefOp::HasPitch, RefOp::Raw148, G(2),
    RefOp::Raw149, RefOp::HasTransport, RefOp::HasFall, G(5), RefOp::ForceCount,
    RefOp::Raw172, G(4), RefOp::HasOrientation, RefOp::HasSplineElevation, G(7),
    RefOp::HasUnknownUInt32, G(1), G(6), RefOp::HasFlags, T(0),
    T(2), T(1), T(6), T(7), T(3),
    T(5), RefOp::HasTransportTime3, RefOp::HasTransportTime2, T(4), RefOp::Flags,
    RefOp::HasFallDirection, RefOp::Flags2, GB(0), GB(2), RefOp::ForceIds,
    GB(3), GB(5), GB(1), GB(7), GB(4),
    GB(6), TB(2), RefOp::TransportZ, RefOp::TransportTime3, TB(6),
    TB(3), RefOp::TransportO, TB(5), RefOp::TransportTime2, TB(1),
    RefOp::TransportY, TB(4), RefOp::TransportTime, RefOp::TransportSeat, RefOp::TransportX,
    TB(0), TB(7), RefOp::Pitch, RefOp::Timestamp, RefOp::FallTime,
    RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallVertical, RefOp::UnknownUInt32,
    RefOp::SplineElevation, RefOp::PositionO, RefOp::End };

// Fall pair untransposed to match the client's writer; see
// MovementStructures.h. +124 is cosAngle, +128 is sinAngle.
static RefOp const kStartStrafeRight[] = {
    RefOp::PositionY, RefOp::PositionX, RefOp::PositionZ, G(0), RefOp::HasFall,
    RefOp::ForceCount, G(7), G(6), G(4), RefOp::HasFlags,
    G(5), RefOp::HasSplineElevation, G(3), RefOp::Raw149, RefOp::HasTransport,
    RefOp::HasUnknownUInt32, G(1), RefOp::Raw172, G(2), RefOp::HasPitch,
    RefOp::HasFlags2, RefOp::HasOrientation, RefOp::Raw148, RefOp::HasTimestamp, RefOp::HasFallDirection,
    T(1), T(6), T(3), T(5), T(2),
    T(0), T(4), RefOp::HasTransportTime3, T(7), RefOp::HasTransportTime2,
    RefOp::Flags, RefOp::Flags2, GB(6), GB(7), GB(0),
    GB(4), GB(1), RefOp::ForceIds, GB(2), GB(3),
    GB(5), RefOp::Pitch, TB(1), RefOp::TransportSeat, TB(3),
    RefOp::TransportTime2, TB(7), RefOp::TransportTime3, TB(5), TB(6),
    TB(2), TB(0), RefOp::TransportTime, RefOp::TransportO, RefOp::TransportY,
    RefOp::TransportZ, TB(4), RefOp::TransportX, RefOp::Timestamp, RefOp::FallVertical,
    RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallTime, RefOp::PositionO,
    RefOp::UnknownUInt32, RefOp::SplineElevation, RefOp::End };

// Fall pair untransposed to match the client's writer; see
// MovementStructures.h. +124 is cosAngle, +128 is sinAngle.
static RefOp const kStopStrafe[] = {
    RefOp::PositionZ, RefOp::PositionX, RefOp::PositionY, RefOp::HasFall, RefOp::HasOrientation,
    RefOp::HasSplineElevation, RefOp::HasTimestamp, RefOp::HasFlags, RefOp::HasUnknownUInt32, G(6),
    RefOp::HasTransport, RefOp::Raw172, RefOp::HasFlags2, G(4), RefOp::HasPitch,
    G(5), G(3), G(2), RefOp::ForceCount, RefOp::Raw149,
    G(7), G(0), RefOp::Raw148, G(1), T(7),
    RefOp::HasTransportTime3, T(3), T(1), T(6), RefOp::HasTransportTime2,
    T(2), T(5), T(4), T(0), RefOp::Flags2,
    RefOp::HasFallDirection, RefOp::Flags, GB(5), GB(3), RefOp::ForceIds,
    GB(2), GB(0), GB(1), GB(6), GB(4),
    GB(7), TB(0), RefOp::TransportTime3, TB(1), TB(6),
    RefOp::TransportTime, RefOp::TransportY, RefOp::TransportZ, TB(4), RefOp::TransportTime2,
    TB(3), RefOp::TransportSeat, RefOp::TransportX, TB(2), TB(7),
    TB(5), RefOp::TransportO, RefOp::PositionO, RefOp::SplineElevation, RefOp::Timestamp,
    RefOp::FallCos, RefOp::FallSin, RefOp::FallHorizontal, RefOp::FallTime, RefOp::FallVertical,
    RefOp::Pitch, RefOp::UnknownUInt32, RefOp::End };

// Fall pair untransposed to match the client's writer; see
// MovementStructures.h. +124 is cosAngle, +128 is sinAngle.
static RefOp const kJump[] = {
    RefOp::PositionY, RefOp::PositionX, RefOp::PositionZ, G(1), G(7),
    RefOp::HasFlags2, G(5), RefOp::HasSplineElevation, RefOp::HasOrientation, G(6),
    G(4), RefOp::Raw149, RefOp::HasTransport, RefOp::Raw148, RefOp::ForceCount,
    RefOp::HasPitch, RefOp::HasFlags, RefOp::HasTimestamp, RefOp::HasUnknownUInt32, G(3),
    RefOp::Raw172, RefOp::HasFall, G(2), G(0), T(2),
    T(3), T(1), T(4), RefOp::HasTransportTime2, T(5),
    T(6), T(0), T(7), RefOp::HasTransportTime3, RefOp::Flags,
    RefOp::Flags2, RefOp::HasFallDirection, GB(7), GB(1), GB(0),
    RefOp::ForceIds, GB(2), GB(6), GB(3), GB(4),
    GB(5), RefOp::FallVertical, RefOp::FallSin, RefOp::FallCos, RefOp::FallHorizontal,
    RefOp::FallTime, TB(5), TB(7), RefOp::TransportSeat, TB(4),
    TB(0), RefOp::TransportZ, TB(6), TB(2), RefOp::TransportY,
    RefOp::TransportTime, RefOp::TransportX, RefOp::TransportTime2, TB(1), TB(3),
    RefOp::TransportTime3, RefOp::TransportO, RefOp::SplineElevation, RefOp::PositionO, RefOp::Pitch,
    RefOp::UnknownUInt32, RefOp::Timestamp, RefOp::End };

// Fall pair untransposed to match the client's writer; see
// MovementStructures.h. +124 is cosAngle, +128 is sinAngle.
static RefOp const kStartTurnLeft[] = {
    RefOp::PositionZ, RefOp::PositionX, RefOp::PositionY, RefOp::HasOrientation, G(4),
    G(5), RefOp::Raw148, RefOp::HasTimestamp, RefOp::Raw172, RefOp::Raw149,
    RefOp::HasUnknownUInt32, G(3), G(1), RefOp::HasFlags2, RefOp::HasFlags,
    G(0), G(2), RefOp::ForceCount, RefOp::HasTransport, G(7),
    RefOp::HasPitch, RefOp::HasSplineElevation, RefOp::HasFall, G(6), RefOp::HasTransportTime3,
    T(5), T(6), T(2), T(3), T(4),
    T(7), RefOp::HasTransportTime2, T(0), T(1), RefOp::Flags,
    RefOp::Flags2, RefOp::HasFallDirection, GB(7), GB(3), GB(6),
    GB(4), GB(1), RefOp::ForceIds, GB(5), GB(0),
    GB(2), RefOp::FallTime, RefOp::FallHorizontal, RefOp::FallCos, RefOp::FallSin,
    RefOp::FallVertical, RefOp::Pitch, RefOp::TransportY, TB(3), RefOp::TransportX,
    RefOp::TransportO, TB(5), RefOp::TransportTime2, RefOp::TransportZ, TB(2),
    TB(1), TB(7), TB(4), TB(0), RefOp::TransportTime3,
    RefOp::TransportSeat, TB(6), RefOp::TransportTime, RefOp::PositionO, RefOp::SplineElevation,
    RefOp::UnknownUInt32, RefOp::Timestamp, RefOp::End };

// Fall pair untransposed to match the client's writer; see
// MovementStructures.h. +124 is cosAngle, +128 is sinAngle.
static RefOp const kStartTurnRight[] = {
    RefOp::PositionX, RefOp::PositionZ, RefOp::PositionY, RefOp::Raw148, RefOp::Raw172,
    G(1), G(0), RefOp::HasFlags, RefOp::HasFall, RefOp::HasPitch,
    RefOp::HasUnknownUInt32, RefOp::ForceCount, RefOp::HasSplineElevation, RefOp::HasFlags2, RefOp::HasOrientation,
    G(2), RefOp::HasTimestamp, G(4), G(6), G(5),
    G(3), RefOp::Raw149, RefOp::HasTransport, G(7), T(2),
    RefOp::HasTransportTime2, T(6), T(5), T(3), T(7),
    T(4), RefOp::HasTransportTime3, T(0), T(1), RefOp::Flags,
    RefOp::Flags2, RefOp::HasFallDirection, GB(5), GB(1), GB(3),
    GB(0), GB(4), GB(2), GB(6), RefOp::ForceIds,
    GB(7), RefOp::FallSin, RefOp::FallHorizontal, RefOp::FallCos, RefOp::FallVertical,
    RefOp::FallTime, RefOp::Pitch, RefOp::TransportTime3, TB(3), RefOp::TransportTime2,
    TB(7), TB(1), RefOp::TransportX, RefOp::TransportSeat, TB(5),
    TB(4), TB(2), TB(0), RefOp::TransportZ, RefOp::TransportTime,
    RefOp::TransportY, TB(6), RefOp::TransportO, RefOp::PositionO, RefOp::Timestamp,
    RefOp::SplineElevation, RefOp::UnknownUInt32, RefOp::End };

// Fall pair untransposed to match the client's writer; see
// MovementStructures.h. +124 is cosAngle, +128 is sinAngle.
static RefOp const kStopTurn[] = {
    RefOp::PositionX, RefOp::PositionZ, RefOp::PositionY, RefOp::HasTransport, RefOp::ForceCount,
    RefOp::Raw149, G(4), G(5), RefOp::HasUnknownUInt32, G(3),
    RefOp::Raw172, RefOp::HasFall, G(0), G(1), RefOp::HasPitch,
    G(6), RefOp::HasFlags, G(2), RefOp::Raw148, RefOp::HasFlags2,
    RefOp::HasSplineElevation, RefOp::HasOrientation, G(7), RefOp::HasTimestamp, RefOp::Flags2,
    T(1), RefOp::HasTransportTime3, RefOp::HasTransportTime2, T(3), T(6),
    T(2), T(0), T(5), T(7), T(4),
    RefOp::Flags, RefOp::HasFallDirection, GB(2), GB(3), GB(6),
    RefOp::ForceIds, GB(0), GB(5), GB(4), GB(7),
    GB(1), RefOp::TransportTime, RefOp::TransportTime3, RefOp::TransportSeat, RefOp::TransportY,
    RefOp::TransportX, RefOp::TransportTime2, TB(4), TB(3), RefOp::TransportO,
    TB(0), RefOp::TransportZ, TB(6), TB(7), TB(5),
    TB(1), TB(2), RefOp::PositionO, RefOp::Timestamp, RefOp::FallSin,
    RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallVertical, RefOp::FallTime, RefOp::UnknownUInt32,
    RefOp::SplineElevation, RefOp::Pitch, RefOp::End };

static RefOp const kStop[] = {
    RefOp::PositionX, RefOp::PositionY, RefOp::PositionZ, G(5), G(2), RefOp::HasFall, G(0),
    RefOp::Raw172, RefOp::Raw148, RefOp::HasUnknownUInt32, G(1), RefOp::ForceCount,
    RefOp::HasPitch, G(3), G(4), RefOp::HasTransport, RefOp::Raw149, G(6), RefOp::HasFlags,
    RefOp::HasTimestamp, RefOp::HasFlags2, RefOp::HasOrientation, RefOp::HasSplineElevation, G(7),
    RefOp::HasTransportTime2, T(7), T(4), T(1), T(0), T(5), T(2), T(3),
    RefOp::HasTransportTime3, T(6), RefOp::HasFallDirection, RefOp::Flags2, RefOp::Flags, GB(0), GB(3), RefOp::ForceIds,
    GB(6), GB(1), GB(4), GB(2), GB(5), GB(7), RefOp::PositionO, RefOp::FallVertical,
    RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallCos, RefOp::FallTime,
    RefOp::SplineElevation, RefOp::TransportX, RefOp::TransportTime, TB(3), RefOp::TransportO,
    RefOp::TransportY, TB(2), TB(6), TB(7), TB(1), TB(4), RefOp::TransportTime3, TB(0),
    RefOp::TransportSeat, RefOp::TransportZ, TB(5), RefOp::TransportTime2,
    RefOp::UnknownUInt32, RefOp::Pitch, RefOp::Timestamp, RefOp::End };

static RefOp const kHeartbeat[] = {
    RefOp::PositionZ, RefOp::PositionX, RefOp::PositionY, RefOp::ForceCount, RefOp::HasFlags,
    RefOp::Raw148, RefOp::HasUnknownUInt32, G(3), G(6), RefOp::HasPitch, RefOp::Raw149,
    RefOp::Raw172, G(7), G(2), G(4), RefOp::HasFlags2, RefOp::HasOrientation,
    RefOp::HasTimestamp, RefOp::HasTransport, RefOp::HasFall, G(5), RefOp::HasSplineElevation,
    G(1), G(0), T(5), T(3), T(6), T(0), T(7), RefOp::HasTransportTime3, T(1), T(2), T(4),
    RefOp::HasTransportTime2, RefOp::Flags, RefOp::HasFallDirection, RefOp::Flags2,
    GB(2), GB(3), GB(6), GB(1), GB(4), GB(7), RefOp::ForceIds, GB(5), GB(0),
    RefOp::FallSin, RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallVertical, RefOp::FallTime,
    TB(1), TB(3), TB(2), TB(0), RefOp::TransportTime3, RefOp::TransportSeat, TB(7),
    RefOp::TransportX, TB(4), RefOp::TransportTime2, RefOp::TransportY, TB(6), TB(5),
    RefOp::TransportZ, RefOp::TransportTime, RefOp::TransportO, RefOp::UnknownUInt32,
    RefOp::PositionO, RefOp::Pitch, RefOp::Timestamp, RefOp::SplineElevation, RefOp::End };

static RefOp const kSetFacing[] = {
    RefOp::PositionY, RefOp::PositionX, RefOp::PositionZ, G(5), RefOp::HasFlags2, G(3), G(2),
    RefOp::ForceCount, RefOp::Raw172, RefOp::HasPitch, G(0), RefOp::HasOrientation,
    RefOp::HasTimestamp, RefOp::Raw148, RefOp::HasUnknownUInt32, G(4), RefOp::Raw149,
    G(1), G(6), RefOp::HasFall, RefOp::HasFlags, RefOp::HasSplineElevation, RefOp::HasTransport, G(7),
    // T(7) before T(0): this list was transcribed from the production sequence,
    // so it carried that sequence's transposed first transport pair. The client's
    // writer sub_67FFA9 emits +63 then +56, and +56 is transport byte 0 in every
    // other opcode, so byte 7 leads.
    T(7), T(0), RefOp::HasTransportTime2, T(3), T(6), RefOp::HasTransportTime3,
    T(2), T(5), T(1), T(4), RefOp::HasFallDirection, RefOp::Flags2, RefOp::Flags,
    RefOp::ForceIds, GB(0), GB(6), GB(3), GB(1), GB(2), GB(7), GB(4), GB(5),
    TB(0), TB(2), RefOp::TransportO, TB(7), RefOp::TransportTime3, TB(5),
    RefOp::TransportTime, RefOp::TransportX, RefOp::TransportTime2, RefOp::TransportZ,
    RefOp::TransportSeat, RefOp::TransportY, TB(4), TB(3), TB(6), TB(1),
    RefOp::FallTime, RefOp::FallVertical, RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallCos,
    RefOp::UnknownUInt32, RefOp::Timestamp, RefOp::SplineElevation, RefOp::PositionO,
    RefOp::Pitch, RefOp::End };

static RefOp const kFallLand[] = {
    RefOp::PositionY, RefOp::PositionZ, RefOp::PositionX, RefOp::HasFall, RefOp::Raw172,
    RefOp::Raw148, RefOp::HasTimestamp, G(7), RefOp::Raw149, RefOp::HasSplineElevation,
    G(5), RefOp::HasPitch, RefOp::HasFlags2, G(2), G(3), G(0), RefOp::HasOrientation,
    RefOp::ForceCount, RefOp::HasFlags, RefOp::HasUnknownUInt32, G(1), RefOp::HasTransport, G(6), G(4),
    T(0), RefOp::HasTransportTime2, T(3), T(5), T(1), T(7), T(4), T(2), T(6),
    RefOp::HasTransportTime3, RefOp::Flags2, RefOp::HasFallDirection, RefOp::Flags,
    GB(4), GB(3), GB(7), GB(0), GB(2), GB(5), GB(1), GB(6), RefOp::ForceIds,
    RefOp::FallSin, RefOp::FallHorizontal, RefOp::FallCos, RefOp::FallTime, RefOp::FallVertical,
    TB(4), RefOp::TransportY, RefOp::TransportO, RefOp::TransportZ, RefOp::TransportSeat,
    TB(3), TB(6), RefOp::TransportTime2, TB(2), TB(1), TB(5), RefOp::TransportTime3,
    RefOp::TransportTime, RefOp::TransportX, TB(7), TB(0), RefOp::UnknownUInt32,
    RefOp::Timestamp, RefOp::SplineElevation, RefOp::Pitch, RefOp::PositionO, RefOp::End };

static RefOp const kSetFly[] = {
    RefOp::PositionY, RefOp::PositionZ, RefOp::PositionX, G(5), RefOp::HasTransport,
    G(3), RefOp::ForceCount, RefOp::HasFlags2, RefOp::Raw149, RefOp::HasFall,
    G(6), RefOp::Raw172, G(7), RefOp::HasTimestamp, G(0), G(2), RefOp::HasPitch,
    RefOp::HasOrientation, G(1), RefOp::Raw148, RefOp::HasSplineElevation,
    RefOp::HasUnknownUInt32, G(4), RefOp::HasFlags, T(1), T(3), T(5),
    RefOp::HasTransportTime3, T(6), T(7), T(2), T(4), RefOp::HasTransportTime2,
    T(0), RefOp::Flags2, RefOp::HasFallDirection, RefOp::Flags, GB(1), GB(6),
    GB(5), GB(2), GB(4), GB(0), GB(7), GB(3), RefOp::ForceIds, TB(7), TB(5),
    TB(1), RefOp::TransportTime, RefOp::TransportSeat, RefOp::TransportTime2,
    TB(0), TB(4), RefOp::TransportY, TB(3), RefOp::TransportZ, TB(2), TB(6),
    RefOp::TransportX, RefOp::TransportO, RefOp::TransportTime3, RefOp::FallSin,
    RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallVertical, RefOp::FallTime,
    RefOp::Timestamp, RefOp::UnknownUInt32, RefOp::SplineElevation, RefOp::Pitch,
    RefOp::PositionO, RefOp::End };

static RefOp const kStartAscend[] = {
    RefOp::PositionY, RefOp::PositionX, RefOp::PositionZ, RefOp::HasOrientation,
    G(3), RefOp::HasTransport, RefOp::HasFlags, RefOp::Raw172, G(0), G(4),
    RefOp::HasTimestamp, G(7), RefOp::Raw149, RefOp::HasPitch, G(5),
    RefOp::HasFlags2, RefOp::Raw148, G(6), G(2), RefOp::HasUnknownUInt32,
    RefOp::ForceCount, G(1), RefOp::HasSplineElevation, RefOp::HasFall, T(4),
    T(0), T(3), T(5), RefOp::HasTransportTime2, T(1), RefOp::HasTransportTime3,
    T(6), T(2), T(7), RefOp::HasFallDirection, RefOp::Flags2, RefOp::Flags,
    GB(2), GB(5), RefOp::ForceIds, GB(1), GB(0), GB(4), GB(7), GB(6), GB(3),
    RefOp::PositionO, RefOp::Timestamp, TB(3), RefOp::TransportTime,
    RefOp::TransportY, RefOp::TransportO, TB(6), RefOp::TransportTime3,
    RefOp::TransportX, TB(2), RefOp::TransportTime2, TB(1), TB(7),
    RefOp::TransportZ, RefOp::TransportSeat, TB(0), TB(4), TB(5),
    RefOp::SplineElevation, RefOp::FallVertical, RefOp::FallSin, RefOp::FallCos,
    RefOp::FallHorizontal, RefOp::FallTime, RefOp::Pitch, RefOp::UnknownUInt32,
    RefOp::End };

static RefOp const kStopAscend[] = {
    RefOp::PositionZ, RefOp::PositionX, RefOp::PositionY, RefOp::HasOrientation,
    G(0), G(3), G(7), G(2), G(6), RefOp::HasFlags2, RefOp::HasTimestamp,
    RefOp::HasUnknownUInt32, RefOp::HasTransport, RefOp::HasPitch, RefOp::Raw148,
    RefOp::Raw172, G(4), RefOp::Raw149, G(5), RefOp::ForceCount, RefOp::HasFall,
    RefOp::HasFlags, G(1), RefOp::HasSplineElevation, RefOp::HasTransportTime2,
    T(0), T(5), T(4), T(6), T(2), T(1), RefOp::HasTransportTime3, T(3), T(7),
    RefOp::Flags2, RefOp::Flags, RefOp::HasFallDirection, GB(0), RefOp::ForceIds,
    GB(4), GB(5), GB(1), GB(7), GB(6), GB(3), GB(2), TB(5), RefOp::TransportY,
    TB(4), TB(7), TB(1), TB(3), RefOp::TransportTime2, RefOp::TransportX,
    RefOp::TransportO, TB(0), TB(2), RefOp::TransportZ, RefOp::TransportTime3,
    RefOp::TransportTime, RefOp::TransportSeat, TB(6), RefOp::FallCos,
    RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallTime, RefOp::FallVertical,
    RefOp::Timestamp, RefOp::SplineElevation, RefOp::Pitch, RefOp::UnknownUInt32,
    RefOp::PositionO, RefOp::End };

static RefOp const kStartDescend[] = {
    RefOp::PositionX, RefOp::PositionY, RefOp::PositionZ, RefOp::HasFall,
    RefOp::HasFlags, G(7), G(0), G(4), RefOp::HasFlags2, RefOp::HasPitch, G(6),
    G(2), RefOp::Raw148, RefOp::HasUnknownUInt32, RefOp::ForceCount,
    RefOp::HasTransport, RefOp::HasOrientation, G(1), RefOp::Raw149,
    RefOp::Raw172, G(3), G(5), RefOp::HasSplineElevation, RefOp::HasTimestamp,
    T(0), RefOp::HasTransportTime3, T(7), RefOp::HasTransportTime2, T(1), T(4),
    T(5), T(3), T(6), T(2), RefOp::Flags2, RefOp::Flags,
    RefOp::HasFallDirection, GB(4), GB(7), GB(1), GB(3), RefOp::ForceIds,
    GB(2), GB(6), GB(0), GB(5), RefOp::TransportX, TB(0), TB(3), TB(7),
    RefOp::TransportSeat, TB(5), TB(1), RefOp::TransportY, RefOp::TransportTime3,
    RefOp::TransportTime, TB(4), RefOp::TransportTime2, RefOp::TransportO,
    RefOp::TransportZ, TB(2), TB(6), RefOp::FallTime, RefOp::FallCos,
    RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallVertical, RefOp::Pitch,
    RefOp::UnknownUInt32, RefOp::SplineElevation, RefOp::PositionO,
    RefOp::Timestamp, RefOp::End };

static RefOp const kPlayerMove[] = {
    RefOp::HasPitch, G(2), RefOp::Raw148, RefOp::Raw149, G(0), RefOp::HasOrientation,
    RefOp::HasFall, RefOp::HasUnknownUInt32, G(3), RefOp::HasFallDirection,
    RefOp::HasTransport, G(4), T(5), T(4), T(7), T(2), T(6), RefOp::HasTransportTime2,
    T(3), T(1), RefOp::HasTransportTime3, T(0), RefOp::HasSplineElevation,
    RefOp::HasFlags, RefOp::Raw172, RefOp::Flags, RefOp::HasFlags2, G(7), G(1),
    RefOp::HasTimestamp, RefOp::Flags2, G(5), RefOp::ForceCount, G(6),
    RefOp::PositionY, TB(7), RefOp::TransportTime2, RefOp::TransportX, TB(5),
    RefOp::TransportSeat, TB(2), TB(0), TB(3), RefOp::TransportTime, TB(4),
    RefOp::TransportZ, TB(1), RefOp::TransportY, RefOp::TransportO, TB(6),
    RefOp::TransportTime3, GB(5), GB(1), RefOp::PositionZ, RefOp::ForceIds,
    RefOp::Timestamp, RefOp::PositionO, GB(3), RefOp::FallSin, RefOp::FallHorizontal,
    RefOp::FallCos, RefOp::FallVertical, RefOp::FallTime, GB(0), RefOp::Pitch, GB(2), GB(6),
    RefOp::SplineElevation, RefOp::UnknownUInt32, RefOp::PositionX, GB(4), GB(7), RefOp::End };

static RefOp const kForceSwimSpeedChangeAck[] = {
    RefOp::PositionY, RefOp::MovementCounter, RefOp::PositionZ, RefOp::PositionX,
    G(4), RefOp::Raw149, RefOp::HasSplineElevation, G(2), RefOp::HasFlags2,
    G(5), G(3), RefOp::HasFlags, G(0), RefOp::HasPitch,
    RefOp::HasUnknownUInt32, RefOp::HasOrientation, RefOp::Raw172, G(1),
    RefOp::HasFall, RefOp::ForceCount, RefOp::HasTimestamp, G(7), G(6),
    RefOp::HasTransport, RefOp::Raw148, RefOp::Flags2,
    RefOp::HasFallDirection, T(4), T(2), T(7), RefOp::HasTransportTime3,
    T(1), T(6), T(3), T(0), RefOp::HasTransportTime2, T(5), RefOp::Flags,
    GB(0), GB(4), GB(5), GB(6), RefOp::ForceIds, GB(1), GB(3), GB(7), GB(2),
    TB(7), RefOp::TransportTime2, RefOp::TransportSeat, RefOp::TransportTime3,
    TB(4), RefOp::TransportY, RefOp::TransportZ, TB(0), TB(6), TB(3), TB(2),
    RefOp::TransportO, RefOp::TransportTime, TB(5), TB(1), RefOp::TransportX,
    RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallCos, RefOp::FallVertical,
    RefOp::FallTime, RefOp::Timestamp, RefOp::SplineElevation,
    RefOp::UnknownUInt32, RefOp::Pitch, RefOp::PositionO, RefOp::End };


// CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK. Derived by mapping the production
// sequence element by element; the mapping itself was recovered from the swim
// ack, whose production sequence and reference list are both already known
// correct, and it resolved all 74 positions with no conflict.
static RefOp const kForceRunBackSpeedChangeAck[] = {
    RefOp::MovementCounter, RefOp::PositionY, RefOp::PositionX, RefOp::PositionZ,
    RefOp::Raw149, G(7), RefOp::Raw148, G(1), RefOp::ForceCount, RefOp::HasSplineElevation,
    RefOp::HasFlags, RefOp::HasFlags2, G(3), G(6), G(5), G(4), RefOp::Raw172,
    RefOp::HasOrientation, RefOp::HasFall, RefOp::HasPitch, RefOp::HasUnknownUInt32,
    RefOp::HasTimestamp, RefOp::HasTransport, G(2), G(0), RefOp::HasTransportTime2,
    T(5), T(2), T(1), T(3), T(4), T(0), T(6), T(7), RefOp::HasTransportTime3,
    RefOp::Flags, RefOp::Flags2, RefOp::HasFallDirection, GB(4), RefOp::ForceIds,
    GB(2), GB(0), GB(7), GB(5), GB(1), GB(3), GB(6), RefOp::TransportTime2,
    RefOp::TransportZ, TB(3), TB(6), TB(2), RefOp::TransportSeat, TB(7), RefOp::TransportTime,
    RefOp::TransportO, TB(0), RefOp::TransportX, TB(5), TB(4), RefOp::TransportY,
    TB(1), RefOp::TransportTime3, RefOp::FallCos, RefOp::FallSin, RefOp::FallHorizontal,
    RefOp::FallVertical, RefOp::FallTime, RefOp::Pitch, RefOp::SplineElevation,
    RefOp::UnknownUInt32, RefOp::PositionO, RefOp::Timestamp, RefOp::End };
static RefOp const kForceSwimBackSpeedChangeAck[] = {
    RefOp::PositionY, RefOp::MovementCounter, RefOp::PositionZ, RefOp::PositionX,
    G(5), RefOp::ForceCount, RefOp::HasTransport, RefOp::HasUnknownUInt32,
    RefOp::HasFlags, G(7), RefOp::HasOrientation, G(4), RefOp::HasSplineElevation,
    RefOp::Raw149, G(6), G(2), RefOp::HasFlags2, RefOp::Raw148, G(0), RefOp::HasTimestamp,
    G(3), RefOp::HasFall, RefOp::Raw172, RefOp::HasPitch, G(1), T(1), T(2),
    T(7), T(3), T(0), T(5), RefOp::HasTransportTime3, T(6), RefOp::HasTransportTime2,
    T(4), RefOp::HasFallDirection, RefOp::Flags, RefOp::Flags2, RefOp::ForceIds,
    GB(5), GB(0), GB(6), GB(4), GB(3), GB(2), GB(1), GB(7), TB(7), RefOp::TransportY,
    TB(2), TB(3), TB(4), TB(0), RefOp::TransportO, RefOp::TransportTime3, RefOp::TransportX,
    RefOp::TransportTime2, RefOp::TransportZ, RefOp::TransportSeat, TB(6),
    RefOp::TransportTime, TB(5), TB(1), RefOp::UnknownUInt32, RefOp::SplineElevation,
    RefOp::FallVertical, RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallSin,
    RefOp::FallTime, RefOp::Timestamp, RefOp::PositionO, RefOp::Pitch, RefOp::End, };

static RefOp const kForceFlightBackSpeedChangeAck[] = {
    RefOp::PositionZ, RefOp::MovementCounter, RefOp::PositionX, RefOp::PositionY,
    RefOp::HasTransport, RefOp::HasFlags, RefOp::HasOrientation, RefOp::HasUnknownUInt32,
    RefOp::HasSplineElevation, RefOp::Raw149, RefOp::HasPitch, G(6), RefOp::HasFall,
    RefOp::ForceCount, RefOp::HasFlags2, G(2), RefOp::Raw172, G(5), G(4), G(3),
    RefOp::Raw148, G(1), RefOp::HasTimestamp, G(0), G(7), RefOp::HasTransportTime2,
    T(4), RefOp::HasTransportTime3, T(2), T(6), T(1), T(5), T(0), T(7), T(3),
    RefOp::Flags2, RefOp::Flags, RefOp::HasFallDirection, RefOp::ForceIds,
    GB(7), GB(1), GB(0), GB(6), GB(4), GB(5), GB(3), GB(2), TB(7), RefOp::TransportX,
    RefOp::TransportY, RefOp::TransportO, TB(0), TB(1), RefOp::TransportSeat,
    TB(4), RefOp::TransportTime2, RefOp::TransportZ, RefOp::TransportTime,
    TB(6), TB(3), TB(5), RefOp::TransportTime3, TB(2), RefOp::FallCos, RefOp::FallSin,
    RefOp::FallHorizontal, RefOp::FallVertical, RefOp::FallTime, RefOp::Pitch,
    RefOp::UnknownUInt32, RefOp::Timestamp, RefOp::SplineElevation, RefOp::PositionO,
    RefOp::End };

// CMSG_MOVE_KNOCK_BACK_ACK. Independent transcription of the 18414 client
// writer; the production reader already carries this recovered order.
static RefOp const kMoveKnockBackAck[] = {
    RefOp::MovementCounter, RefOp::PositionX, RefOp::PositionZ, RefOp::PositionY,
    G(5), RefOp::HasUnknownUInt32, RefOp::HasTimestamp, RefOp::HasFlags2,
    RefOp::Raw172, RefOp::HasTransport, RefOp::HasSplineElevation, G(3),
    RefOp::HasPitch, RefOp::ForceCount, G(1), RefOp::HasFlags, G(7), G(6),
    G(4), G(0), RefOp::HasOrientation, G(2), RefOp::Raw149, RefOp::HasFall,
    RefOp::Raw148, RefOp::Flags, T(2), RefOp::HasTransportTime3, T(3), T(1),
    T(4), T(6), T(0), RefOp::HasTransportTime2, T(5), T(7),
    RefOp::HasFallDirection, RefOp::Flags2, GB(4), GB(1), GB(0), GB(2), GB(5),
    GB(3), RefOp::ForceIds, GB(7), GB(6), RefOp::TransportTime3,
    RefOp::TransportTime2, RefOp::TransportTime, RefOp::TransportX,
    RefOp::TransportY, TB(7), RefOp::TransportSeat, TB(1), RefOp::TransportZ,
    RefOp::TransportO, TB(6), TB(2), TB(3), TB(0), TB(4), TB(5),
    RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallCos, RefOp::FallVertical,
    RefOp::FallTime, RefOp::SplineElevation, RefOp::Pitch, RefOp::Timestamp,
    RefOp::PositionO, RefOp::UnknownUInt32, RefOp::End };

// SMSG_MOVE_UPDATE_KNOCK_BACK. Independent transcription of the 18414 client
// reader, including its 22-bit movement-force count and 13-bit flags2 field.
static RefOp const kMoveUpdateKnockBack[] = {
    G(5), RefOp::HasSplineElevation, RefOp::HasTimestamp, RefOp::ForceCount,
    RefOp::HasFlags2, G(2), G(4), G(6), G(1), G(0), RefOp::Raw149,
    RefOp::HasOrientation, RefOp::Raw148, RefOp::HasTransport,
    T(5), T(2), T(0), T(7), T(1), T(6), T(4), RefOp::HasTransportTime2,
    T(3), RefOp::HasTransportTime3, G(3), RefOp::HasFall,
    RefOp::HasUnknownUInt32, RefOp::HasFallDirection, G(7), RefOp::Raw172,
    RefOp::HasPitch, RefOp::Flags2, RefOp::HasFlags, RefOp::Flags,
    GB(1), TB(5), RefOp::TransportTime3, TB(3), TB(1), TB(4),
    RefOp::TransportZ, TB(7), TB(6), TB(2), RefOp::TransportY, TB(0),
    RefOp::TransportSeat, RefOp::TransportO, RefOp::TransportX,
    RefOp::TransportTime2, RefOp::TransportTime, GB(2),
    RefOp::SplineElevation, RefOp::FallSin, RefOp::FallHorizontal,
    RefOp::FallCos, RefOp::FallVertical, RefOp::FallTime, RefOp::PositionY,
    RefOp::PositionO, RefOp::ForceIds, GB(7), GB(6), GB(4), RefOp::PositionZ,
    RefOp::UnknownUInt32, GB(3), GB(0), RefOp::PositionX, RefOp::Pitch, GB(5),
    RefOp::Timestamp, RefOp::End };

// CMSG_FORCE_MOVE_ROOT_ACK. Independent transcription of the build-18414
// client writer sub_67E304. The captured bodies below establish the exercised
// arms; the synthetic cases exercise every writer branch.
static RefOp const kForceMoveRootAck[] = {
    RefOp::PositionX, RefOp::MovementCounter, RefOp::PositionY, RefOp::PositionZ,
    RefOp::Raw149, RefOp::HasTimestamp, RefOp::HasSplineElevation, RefOp::Raw148,
    G(3), RefOp::Raw172, G(4), RefOp::HasFlags2, G(6), RefOp::HasUnknownUInt32,
    RefOp::HasPitch, RefOp::HasFall, G(2), G(1), G(7), RefOp::HasTransport,
    RefOp::ForceCount, RefOp::HasFlags, G(0), RefOp::HasOrientation, G(5),
    T(1), RefOp::HasTransportTime3, RefOp::HasTransportTime2, T(2), T(6), T(3),
    T(0), T(4), T(7), T(5), RefOp::Flags, RefOp::HasFallDirection, RefOp::Flags2,
    GB(1), GB(0), GB(5), GB(7), GB(3), GB(4), GB(2), GB(6), RefOp::ForceIds,
    RefOp::UnknownUInt32, RefOp::FallCos, RefOp::FallSin, RefOp::FallHorizontal,
    RefOp::FallTime, RefOp::FallVertical, TB(5), TB(0), TB(3), TB(2),
    RefOp::TransportX, TB(6), RefOp::TransportY, RefOp::TransportTime2,
    RefOp::TransportTime, RefOp::TransportZ, TB(7), RefOp::TransportO, TB(1),
    RefOp::TransportSeat, RefOp::TransportTime3, TB(4), RefOp::Pitch,
    RefOp::Timestamp, RefOp::PositionO, RefOp::SplineElevation, RefOp::End };

// CMSG_FORCE_MOVE_UNROOT_ACK. Independent transcription of sub_6850F1.
static RefOp const kForceMoveUnrootAck[] = {
    RefOp::PositionX, RefOp::PositionY, RefOp::MovementCounter, RefOp::PositionZ,
    G(0), RefOp::HasPitch, RefOp::HasFall, RefOp::HasSplineElevation, RefOp::Raw148,
    G(6), G(4), G(1), G(3), RefOp::Raw172, G(7), G(5), RefOp::Raw149,
    RefOp::HasTimestamp, RefOp::HasUnknownUInt32, RefOp::HasFlags,
    RefOp::HasTransport, G(2), RefOp::ForceCount, RefOp::HasFlags2,
    RefOp::HasOrientation, RefOp::HasTransportTime3, T(0), T(2),
    RefOp::HasTransportTime2, T(6), T(4), T(1), T(7), T(5), T(3),
    RefOp::Flags2, RefOp::HasFallDirection, RefOp::Flags, GB(1), GB(0),
    RefOp::ForceIds, GB(6), GB(2), GB(5), GB(3), GB(4), GB(7),
    RefOp::UnknownUInt32, RefOp::TransportSeat, TB(7), TB(4),
    RefOp::TransportTime, TB(1), RefOp::TransportTime2, RefOp::TransportZ,
    RefOp::TransportTime3, TB(5), RefOp::TransportY, TB(6), TB(2),
    RefOp::TransportX, TB(0), TB(3), RefOp::TransportO, RefOp::FallCos,
    RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallTime, RefOp::FallVertical,
    RefOp::Timestamp, RefOp::SplineElevation, RefOp::PositionO, RefOp::Pitch,
    RefOp::End };

// CMSG_MOVE_WATER_WALK_ACK. Independent transcription of sub_675FB7.
static RefOp const kMoveWaterWalkAck[] = {
    RefOp::PositionX, RefOp::PositionY, RefOp::MovementCounter, RefOp::PositionZ,
    RefOp::Raw172, G(3), RefOp::HasFall, G(2), RefOp::HasFlags, G(0),
    RefOp::HasPitch, RefOp::Raw149, RefOp::HasFlags2, G(7), G(6),
    RefOp::HasTimestamp, RefOp::Raw148, RefOp::HasSplineElevation,
    RefOp::HasTransport, G(1), G(4), RefOp::HasUnknownUInt32,
    RefOp::ForceCount, G(5), RefOp::HasOrientation, T(1),
    RefOp::HasTransportTime2, T(7), RefOp::HasTransportTime3, T(4), T(5),
    T(2), T(3), T(0), T(6), RefOp::Flags, RefOp::HasFallDirection,
    RefOp::Flags2, GB(7), RefOp::ForceIds, GB(0), GB(5), GB(3), GB(4),
    GB(1), GB(6), GB(2), RefOp::TransportTime2, TB(1), RefOp::TransportTime,
    RefOp::TransportY, TB(4), RefOp::TransportX, TB(5), TB(7), TB(3),
    RefOp::TransportO, TB(2), RefOp::TransportSeat, RefOp::TransportTime3,
    TB(6), TB(0), RefOp::TransportZ, RefOp::FallCos, RefOp::FallSin,
    RefOp::FallHorizontal, RefOp::FallVertical, RefOp::FallTime,
    RefOp::UnknownUInt32, RefOp::PositionO, RefOp::Pitch, RefOp::Timestamp,
    RefOp::SplineElevation, RefOp::End };

// CMSG_MOVE_CHNG_TRANSPORT. Independent transcription of the build-18414
// client writer sub_67EE69. This sequence tests the codec selected by
// MovementInfo::Read before the registered movement handler consumes it.
static RefOp const kMoveChangeTransport[] = {
    RefOp::PositionX, RefOp::PositionY, RefOp::PositionZ,
    G(1), RefOp::HasPitch, RefOp::HasSplineElevation, G(4),
    RefOp::HasUnknownUInt32, G(5), RefOp::ForceCount, RefOp::Raw172, G(6),
    RefOp::Raw148, G(7), G(0), RefOp::HasTransport, RefOp::HasFlags,
    RefOp::Raw149, RefOp::HasFlags2, G(2), G(3), RefOp::HasTimestamp,
    RefOp::HasFall, RefOp::HasOrientation,
    RefOp::HasTransportTime3, T(5), T(6), T(3), T(2), T(4),
    RefOp::HasTransportTime2, T(1), T(7), T(0),
    RefOp::Flags, RefOp::HasFallDirection, RefOp::Flags2,
    GB(5), GB(2), RefOp::ForceIds, GB(6), GB(3), GB(0), GB(4), GB(7), GB(1),
    RefOp::FallVertical, RefOp::FallSin, RefOp::FallHorizontal, RefOp::FallCos,
    RefOp::FallTime,
    TB(6), RefOp::TransportZ, TB(5), TB(4), RefOp::TransportTime3, TB(3),
    RefOp::TransportTime, TB(2), RefOp::TransportTime2, TB(0),
    RefOp::TransportY, TB(1), RefOp::TransportSeat, RefOp::TransportO, TB(7),
    RefOp::TransportX,
    RefOp::PositionO, RefOp::SplineElevation, RefOp::UnknownUInt32,
    RefOp::Pitch, RefOp::Timestamp, RefOp::End };

#undef G
#undef GB
#undef T
#undef TB

template <size_t N>
static std::vector<uint8> Encode(RefOp const (&sequence)[N], RefState const& s,
    uint32 countOverride = std::numeric_limits<uint32>::max(), size_t idsToWrite = std::numeric_limits<size_t>::max(),
    bool stopAfterIds = false)
{
    RefWriter w;
    uint32 const count = countOverride == std::numeric_limits<uint32>::max()
        ? uint32(s.forceIds.size()) : countOverride;
    auto guidBit = [](uint64 guid, size_t i) { return uint8(guid >> (8 * i)) != 0; };
    for (RefOp op : sequence)
    {
        switch (op)
        {
            case RefOp::Flags: if (s.hasFlags) w.Bits(s.flags, 30); break;
            case RefOp::Flags2: if (s.hasFlags2) w.Bits(s.flags2, 13); break;
            case RefOp::Timestamp: if (s.hasTimestamp) w.U32(s.timestamp); break;
            case RefOp::HasPitch: w.Bit(!s.hasPitch); break;
            case RefOp::Raw148: w.Bit(s.raw148); break;
            case RefOp::Raw149: w.Bit(s.raw149); break;
            case RefOp::Raw172: w.Bit(s.raw172); break;
            case RefOp::HasFlags: w.Bit(!s.hasFlags); break;
            case RefOp::HasFlags2: w.Bit(!s.hasFlags2); break;
            case RefOp::HasTimestamp: w.Bit(!s.hasTimestamp); break;
            case RefOp::HasOrientation: w.Bit(!s.hasOrientation); break;
            case RefOp::HasFall: w.Bit(s.hasFall); break;
            case RefOp::HasFallDirection: if (s.hasFall) w.Bit(s.hasFallDirection); break;
            case RefOp::HasTransport: w.Bit(s.hasTransport); break;
            case RefOp::HasTransportTime2: if (s.hasTransport) w.Bit(s.hasTransportTime2); break;
            case RefOp::HasTransportTime3: if (s.hasTransport) w.Bit(s.hasTransportTime3); break;
            case RefOp::HasSplineElevation: w.Bit(!s.hasSplineElevation); break;
            case RefOp::ForceCount: w.Bits(count, 22); break;
            case RefOp::MovementCounter: w.U32(s.movementCounter); break;
            case RefOp::HasUnknownUInt32: w.Bit(!s.hasUnknownUInt32); break;
            case RefOp::PositionX: w.F32(s.x); break;
            case RefOp::PositionY: w.F32(s.y); break;
            case RefOp::PositionZ: w.F32(s.z); break;
            case RefOp::PositionO: if (s.hasOrientation) w.F32(s.o); break;
            case RefOp::Pitch: if (s.hasPitch) w.F32(s.pitch); break;
            case RefOp::FallTime: if (s.hasFall) w.U32(s.fallTime); break;
            case RefOp::SplineElevation: if (s.hasSplineElevation) w.F32(s.splineElevation); break;
            case RefOp::FallHorizontal: if (s.hasFall && s.hasFallDirection) w.F32(s.fallHorizontal); break;
            case RefOp::FallVertical: if (s.hasFall) w.F32(s.fallVertical); break;
            case RefOp::FallCos: if (s.hasFall && s.hasFallDirection) w.F32(s.fallCos); break;
            case RefOp::FallSin: if (s.hasFall && s.hasFallDirection) w.F32(s.fallSin); break;
            case RefOp::TransportSeat: if (s.hasTransport) w.U8(uint8(s.transportSeat)); break;
            case RefOp::TransportO: if (s.hasTransport) w.F32(s.to); break;
            case RefOp::TransportX: if (s.hasTransport) w.F32(s.tx); break;
            case RefOp::TransportY: if (s.hasTransport) w.F32(s.ty); break;
            case RefOp::TransportZ: if (s.hasTransport) w.F32(s.tz); break;
            case RefOp::TransportTime: if (s.hasTransport) w.U32(s.transportTime); break;
            case RefOp::TransportTime2: if (s.hasTransport && s.hasTransportTime2) w.U32(s.transportTime2); break;
            case RefOp::TransportTime3: if (s.hasTransport && s.hasTransportTime3) w.U32(s.transportTime3); break;
            case RefOp::ForceIds:
            {
                size_t const n = idsToWrite == std::numeric_limits<size_t>::max() ? s.forceIds.size() : idsToWrite;
                for (size_t i = 0; i < n; ++i) w.U32(s.forceIds[i]);
                if (stopAfterIds) return w.Bytes();
                break;
            }
            case RefOp::UnknownUInt32: if (s.hasUnknownUInt32) w.U32(s.unknownUInt32); break;
            case RefOp::End: return w.Bytes();
            default:
            {
                int const value = int(op);
                if (value >= int(RefOp::GuidBit0) && value <= int(RefOp::GuidBit7))
                    w.Bit(guidBit(s.guid, size_t(value - int(RefOp::GuidBit0))));
                else if (value >= int(RefOp::TransportBit0) && value <= int(RefOp::TransportBit7))
                {
                    if (s.hasTransport) w.Bit(guidBit(s.transportGuid, size_t(value - int(RefOp::TransportBit0))));
                }
                else if (value >= int(RefOp::GuidByte0) && value <= int(RefOp::GuidByte7))
                    w.GuidByte(s.guid, size_t(value - int(RefOp::GuidByte0)));
                else if (value >= int(RefOp::TransportByte0) && value <= int(RefOp::TransportByte7))
                {
                    if (s.hasTransport) w.GuidByte(s.transportGuid, size_t(value - int(RefOp::TransportByte0)));
                }
                else CHECK(false);
                break;
            }
        }
    }
    return w.Bytes();
}

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static float FloatFromBits(uint32 bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

static void CheckDecoded(MovementInfo const& info, RefState const& s, OpcodesList opcode)
{
    CHECK(info.GetGuid().GetRawValue() == s.guid);
    if (opcode == CMSG_FORCE_MOVE_ROOT_ACK || opcode == CMSG_FORCE_MOVE_UNROOT_ACK ||
        opcode == CMSG_MOVE_WATER_WALK_ACK || opcode == CMSG_MOVE_KNOCK_BACK_ACK)
    {
        CHECK(info.GetMovementCounter() == s.movementCounter);
    }
    CHECK(uint32(info.GetMovementFlags()) == (s.hasFlags ? s.flags : 0));
    CHECK(uint16(info.GetMovementFlags2()) == (s.hasFlags2 ? s.flags2 : 0));
    CHECK(info.GetTime() == (s.hasTimestamp ? s.timestamp : 0));
    CHECK(info.GetUnknownBit148() == s.raw148);
    CHECK(info.GetUnknownBit149() == s.raw149);
    CHECK(info.GetUnknownBit172() == s.raw172);
    CHECK(info.GetMovementForceIds() == s.forceIds);
    CHECK(info.HasUnknownUInt32() == s.hasUnknownUInt32);
    CHECK(info.GetUnknownUInt32() == s.unknownUInt32);
    CHECK(info.GetPos()->x == s.x);
    CHECK(info.GetPos()->y == s.y);
    CHECK(info.GetPos()->z == s.z);
    CHECK(info.GetPos()->o == (s.hasOrientation ? s.o : 0.0f));
    CHECK(info.GetTransportGuid().GetRawValue() == (s.hasTransport ? s.transportGuid : 0));
    CHECK(info.GetTransportPos()->x == (s.hasTransport ? s.tx : 0.0f));
    CHECK(info.GetTransportPos()->y == (s.hasTransport ? s.ty : 0.0f));
    CHECK(info.GetTransportPos()->z == (s.hasTransport ? s.tz : 0.0f));
    CHECK(info.GetTransportPos()->o == (s.hasTransport ? s.to : 0.0f));
    CHECK(info.GetTransportTime() == (s.hasTransport ? s.transportTime : 0));
    CHECK(info.GetTransportTime2() == (s.hasTransport && s.hasTransportTime2 ? s.transportTime2 : 0));
    CHECK(info.GetTransportTime3() == (s.hasTransport && s.hasTransportTime3 ? s.transportTime3 : 0));
    CHECK(info.GetTransportSeat() == (s.hasTransport ? s.transportSeat : -1));
    CHECK(info.GetPitch() == (s.hasPitch ? s.pitch : 0.0f));
    CHECK(info.GetFallTime() == (s.hasFall ? s.fallTime : 0));
    CHECK(info.GetJumpInfo().velocity == (s.hasFall ? s.fallVertical : 0.0f));
    CHECK(info.GetJumpInfo().xyspeed == (s.hasFall && s.hasFallDirection ? s.fallHorizontal : 0.0f));
    CHECK(info.GetJumpInfo().cosAngle == (s.hasFall && s.hasFallDirection ? s.fallCos : 0.0f));
    CHECK(info.GetJumpInfo().sinAngle == (s.hasFall && s.hasFallDirection ? s.fallSin : 0.0f));
    CHECK(info.GetSplineElevation() == (s.hasSplineElevation ? s.splineElevation : 0.0f));
    CHECK(info.GetStatusInfo().hasTimeStamp == s.hasTimestamp);
    CHECK(info.GetStatusInfo().hasOrientation == s.hasOrientation);
    CHECK(info.GetStatusInfo().hasPitch == s.hasPitch);
    CHECK(info.GetStatusInfo().hasFallData == s.hasFall);
    CHECK(info.GetStatusInfo().hasFallDirection == s.hasFallDirection);
    CHECK(info.GetStatusInfo().hasTransportTime2 == s.hasTransportTime2);
    CHECK(info.GetStatusInfo().hasTransportTime3 == s.hasTransportTime3);
    CHECK(info.GetStatusInfo().hasSplineElevation == s.hasSplineElevation);
}

template <size_t N>
static MovementInfo Decode(OpcodesList opcode, RefOp const (&sequence)[N], RefState const& state,
    std::vector<uint8>* fixtureOut = nullptr)
{
    std::vector<uint8> const fixture = Encode(sequence, state);
    if (fixtureOut) *fixtureOut = fixture;
    WorldPacket packet(opcode, fixture.size());
    packet.append(fixture.data(), fixture.size());
    MovementInfo info;
    packet >> info;
    if (packet.rpos() != packet.size())
        std::fprintf(stderr, "opcode 0x%04X consumed %zu/%zu\n", uint32(opcode), packet.rpos(), packet.size());
    CHECK(packet.rpos() == packet.size());
    CheckDecoded(info, state, opcode);
    return info;
}

static void test_seventeen_inbound_fixtures_and_exact_relay()
{
    RefState const state;
    CHECK(state.hasFlags && state.hasFlags2 && state.hasTimestamp && state.hasOrientation &&
        state.hasPitch && state.hasFall && state.hasFallDirection && state.hasTransport &&
        state.hasTransportTime2 && state.hasTransportTime3 && state.hasSplineElevation);
    std::array<OpcodesList, 17> const opcodes {{ CMSG_MOVE_START_FORWARD, CMSG_MOVE_START_BACKWARD,
        CMSG_MOVE_STOP, MSG_MOVE_HEARTBEAT, CMSG_MOVE_SET_FACING, CMSG_MOVE_FALL_LAND,
        CMSG_MOVE_START_STRAFE_LEFT, CMSG_MOVE_START_STRAFE_RIGHT, CMSG_MOVE_STOP_STRAFE,
        CMSG_MOVE_JUMP, CMSG_MOVE_START_TURN_LEFT, CMSG_MOVE_START_TURN_RIGHT, CMSG_MOVE_STOP_TURN,
        CMSG_MOVE_SET_FLY, CMSG_MOVE_START_ASCEND, CMSG_MOVE_STOP_ASCEND, CMSG_MOVE_START_DESCEND }};
    std::array<std::vector<uint8>, 17> fixtures;
    MovementInfo infos[17] = {
        Decode(opcodes[0], kStartForward, state, &fixtures[0]),
        Decode(opcodes[1], kStartBackward, state, &fixtures[1]),
        Decode(opcodes[2], kStop, state, &fixtures[2]),
        Decode(opcodes[3], kHeartbeat, state, &fixtures[3]),
        Decode(opcodes[4], kSetFacing, state, &fixtures[4]),
        Decode(opcodes[5], kFallLand, state, &fixtures[5]),
        Decode(opcodes[6], kStartStrafeLeft, state, &fixtures[6]),
        Decode(opcodes[7], kStartStrafeRight, state, &fixtures[7]),
        Decode(opcodes[8], kStopStrafe, state, &fixtures[8]),
        Decode(opcodes[9], kJump, state, &fixtures[9]),
        Decode(opcodes[10], kStartTurnLeft, state, &fixtures[10]),
        Decode(opcodes[11], kStartTurnRight, state, &fixtures[11]),
        Decode(opcodes[12], kStopTurn, state, &fixtures[12]),
        Decode(opcodes[13], kSetFly, state, &fixtures[13]),
        Decode(opcodes[14], kStartAscend, state, &fixtures[14]),
        Decode(opcodes[15], kStopAscend, state, &fixtures[15]),
        Decode(opcodes[16], kStartDescend, state, &fixtures[16]) };
    for (size_t i = 1; i < fixtures.size(); ++i)
        CHECK(fixtures[i] != fixtures[i - 1]);

    std::vector<uint8> const expectedRelay = Encode(kPlayerMove, state);
    for (size_t i = 0; i < opcodes.size(); ++i)
    {
        WorldPacket relay(SMSG_PLAYER_MOVE, expectedRelay.size());
        relay << infos[i];
        if (!Equal(relay, expectedRelay))
        {
            size_t different = 0;
            while (different < relay.size() && different < expectedRelay.size() && relay.contents()[different] == expectedRelay[different])
                ++different;
            std::fprintf(stderr, "relay mismatch source=0x%04X size=%zu/%zu first=%zu\n",
                uint32(opcodes[i]), relay.size(), expectedRelay.size(), different);
        }
        CHECK(Equal(relay, expectedRelay));
    }
}

static void test_flight_input_retail_bodies()
{
    static uint8 const startAscend[] = {
        0xd8, 0x21, 0x64, 0x44, 0x48, 0x63, 0x04, 0x46, 0xcd, 0x66, 0x0b, 0x44,
        0x44, 0x81, 0x80, 0x00, 0x01, 0x88, 0x00, 0x0d, 0x00, 0x00, 0x08, 0x3d,
        0xc9, 0xe9, 0x05, 0x04, 0xfa, 0xbc, 0xc0, 0x3f, 0x9e, 0x2e, 0x0e, 0x00,
        0x35, 0x8d, 0xa7, 0xbe,
    };
    static uint8 const stopAscend[] = {
        0x0f, 0x08, 0x0c, 0x44, 0x70, 0x5f, 0x04, 0x46, 0xa1, 0x4e, 0x62, 0x44,
        0x78, 0x80, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x0c, 0x00, 0x00, 0x08, 0xe9,
        0xc9, 0x05, 0x04, 0x3d, 0xa7, 0x2d, 0x0e, 0x00, 0x80, 0x3b, 0xaa, 0xbe,
        0xca, 0xa1, 0xac, 0x3f,
    };
    static uint8 const startDescend[] = {
        0x62, 0xf9, 0x62, 0x44, 0xf0, 0x79, 0x90, 0x43, 0x16, 0xee, 0xe6, 0x43,
        0x32, 0xa0, 0x00, 0x00, 0x12, 0x88, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x05,
        0x28, 0x04, 0x49, 0xd0, 0x33, 0x28, 0x88, 0x40, 0x35, 0xf8, 0x87, 0x02,
    };
    static uint8 const setFlyTransport[] = {
        0x42, 0xfa, 0x8c, 0x45, 0x49, 0x15, 0x8f, 0x41, 0x0d, 0xf4, 0x12, 0x45,
        0x60, 0x00, 0x00, 0x02, 0xeb, 0x03, 0x12, 0x00, 0x03, 0x00, 0x00, 0x00,
        0xc9, 0x3d, 0xe9, 0x05, 0x04, 0x1e, 0x20, 0x1a, 0x02, 0x00, 0xff, 0x29,
        0x7e, 0xeb, 0x93, 0xbf, 0x60, 0xf7, 0x99, 0x41, 0xc1, 0xf8, 0xdc, 0xf0,
        0x41, 0xec, 0xe8, 0x86, 0x40, 0x26, 0x70, 0xe4, 0x00, 0x1a, 0xcb, 0x2b,
        0x40,
    };

    auto decode = [](OpcodesList opcode, uint8 const* body, size_t size)
    {
        WorldPacket packet(opcode, size);
        packet.append(body, size);
        MovementInfo info;
        packet >> info;
        CHECK(packet.rpos() == packet.size());
        CHECK(info.GetMovementForceIds().empty());
        CHECK(!info.HasUnknownUInt32());
        return info;
    };

    // capture-000004/639 exercises the pitch-present start-ascend arm.
    MovementInfo const ascend = decode(CMSG_MOVE_START_ASCEND, startAscend, sizeof(startAscend));
    CHECK(ascend.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(ascend.GetPos()->x == 8472.8203125f);
    CHECK(ascend.GetPos()->y == 912.52880859375f);
    CHECK(ascend.GetPos()->z == 557.60626220703125f);
    CHECK(ascend.GetPos()->o == 1.5057671070098877f);
    CHECK(uint32(ascend.GetMovementFlags()) == 0x01A00001u);
    CHECK(uint16(ascend.GetMovementFlags2()) == 0x0400u);
    CHECK(ascend.GetTime() == 929438u);
    CHECK(ascend.GetPitch() == -0.3272491991519928f);

    // capture-000004/636: pitch present and both vertical-direction bits clear.
    MovementInfo const stop = decode(CMSG_MOVE_STOP_ASCEND, stopAscend, sizeof(stopAscend));
    CHECK(stop.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(stop.GetPos()->x == 8471.859375f);
    CHECK(stop.GetPos()->y == 905.22857666015625f);
    CHECK(stop.GetPos()->z == 560.12591552734375f);
    CHECK(stop.GetPos()->o == 1.3486874103546143f);
    CHECK(uint32(stop.GetMovementFlags()) == 0x01800001u);
    CHECK(uint16(stop.GetMovementFlags2()) == 0x0400u);
    CHECK(stop.GetTime() == 929191u);
    CHECK(stop.GetPitch() == -0.33248519897460938f);

    // capture-000112/251644: the client starts descending and uses no separate
    // STOP_DESCEND opcode; STOP_ASCEND clears either vertical direction.
    MovementInfo const descend = decode(CMSG_MOVE_START_DESCEND, startDescend, sizeof(startDescend));
    CHECK(descend.GetGuid().GetRawValue() == UINT64_C(0x04000000054829D1));
    CHECK(descend.GetPos()->x == 907.8966064453125f);
    CHECK(descend.GetPos()->y == 288.95263671875f);
    CHECK(descend.GetPos()->z == 461.86004638671875f);
    CHECK(descend.GetPos()->o == 4.254907131195068f);
    CHECK(uint32(descend.GetMovementFlags()) == 0x01C00000u);
    CHECK(uint16(descend.GetMovementFlags2()) == 0x0400u);
    CHECK(descend.GetTime() == 42465333u);

    // capture-000142/325036 exercises the SET_FLY selector and transport arm.
    MovementInfo const fly = decode(CMSG_MOVE_SET_FLY, setFlyTransport, sizeof(setFlyTransport));
    CHECK(fly.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(fly.GetPos()->x == 2351.253173828125f);
    CHECK(fly.GetPos()->y == 4511.2822265625f);
    CHECK(fly.GetPos()->z == 17.885393142700195f);
    CHECK(fly.GetPos()->o == 2.6842713356018066f);
    CHECK(uint32(fly.GetMovementFlags()) == 0x01800000u);
    CHECK(uint16(fly.GetMovementFlags2()) == 0x0400u);
    CHECK(fly.GetTime() == 14970918u);
    CHECK(fly.GetTransportGuid().GetRawValue() == UINT64_C(0x1FC0000000000028));
    CHECK(fly.GetTransportTime() == 137760u);
    CHECK(fly.GetTransportSeat() == -1);
    CHECK(fly.GetTransportPos()->x == 30.107894897460938f);
    CHECK(fly.GetTransportPos()->y == -1.1556241512298584f);
    CHECK(fly.GetTransportPos()->z == 19.24578857421875f);
    CHECK(fly.GetTransportPos()->o == 4.215932846069336f);
}

static void test_force_run_back_speed_change_ack_fixture()
{
    RefState state;
    state.flags2 = 0x0ABCu;
    std::vector<uint8> const movement = Encode(kForceRunBackSpeedChangeAck, state);

    WorldPacket packet(CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK, movement.size() + sizeof(float));
    packet << float(4.5f);
    packet.append(movement.data(), movement.size());

    float speed = 0.0f;
    MovementInfo info;
    packet >> speed;
    packet >> info;
    CHECK(speed == 4.5f);
    CHECK(packet.rpos() == packet.size());
    CheckDecoded(info, state, CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK);
}

/// A real 18414 run-back acknowledgement, decoded through the production
/// sequence. This is the fixture that does NOT depend on the reference encoder.
///
/// The reference list above was derived by mapping the production sequence, so
/// the two would agree even if the production sequence were wrong. A review
/// raised exactly that. This body comes from the wire instead: capture-000004
/// sequence 23268, build 18414, catalogue 2BE10C89.
///
/// It must consume to the last byte and yield the speed, mover and position the
/// capture recorded. That catches any element that changes the bit cursor, but
/// NOT every possible error: two same-width fields could still swap and consume
/// exactly, which is why the position is asserted rather than the GUID alone.
/// The two acks with no observed traffic. Their sequences come from the client's
/// own writers, so absence from the corpus is a gap in what was recorded rather
/// than evidence against the layout -- but it does mean there is no retail body
/// to pin them, and a round-trip is all the coverage available.
static void test_back_speed_change_ack_round_trips()
{
    RefState state;
    state.flags2 = 0x0ABCu;

    {
        std::vector<uint8> const movement = Encode(kForceSwimBackSpeedChangeAck, state);
        WorldPacket packet(CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK, movement.size() + sizeof(float));
        packet << float(2.5f);
        packet.append(movement.data(), movement.size());

        float speed = 0.0f;
        MovementInfo info;
        packet >> speed;
        packet >> info;
        CHECK(speed == 2.5f);
        CHECK(packet.rpos() == packet.size());
        CheckDecoded(info, state, CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK);
    }
    {
        std::vector<uint8> const movement = Encode(kForceFlightBackSpeedChangeAck, state);
        WorldPacket packet(CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK, movement.size() + sizeof(float));
        packet << float(4.5f);
        packet.append(movement.data(), movement.size());

        float speed = 0.0f;
        MovementInfo info;
        packet >> speed;
        packet >> info;
        CHECK(speed == 4.5f);
        CHECK(packet.rpos() == packet.size());
        CheckDecoded(info, state, CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK);
    }
}

/// All four speed-leading acks must have distinct sequences. They share only the
/// leading float, so one state must encode four different ways; any two matching
/// would mean a sequence had been pointed at the wrong opcode.
/// The acks must NOT share a sequence. They agree on the leading speed and
/// nothing else -- the bit order and the GUID byte order are both their own -- so
/// the same state must encode differently, and a body built for one must not
/// decode cleanly through the other. Without this a registration could quietly
/// be pointed at the wrong sequence and nothing would complain.
template <size_t N>
static bool Rejects(OpcodesList opcode, RefOp const (&sequence)[N], RefState const& state,
    uint32 count, size_t idsToWrite, bool stopAfterIds)
{
    std::vector<uint8> const bytes = Encode(sequence, state, count, idsToWrite, stopAfterIds);
    WorldPacket packet(opcode, bytes.size());
    packet.append(bytes.data(), bytes.size());
    try
    {
        MovementInfo info;
        packet >> info;
    }
    catch (ByteBufferException const&)
    {
        return true;
    }
    return false;
}

static bool RejectsForceSwim(RefState const& state, uint32 count, size_t idsToWrite, bool stopAfterIds)
{
    std::vector<uint8> const movement =
        Encode(kForceSwimSpeedChangeAck, state, count, idsToWrite, stopAfterIds);
    WorldPacket packet(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK, movement.size() + sizeof(float));
    packet << float(13.25f);
    packet.append(movement.data(), movement.size());
    try
    {
        float speed = 0.0f;
        MovementInfo info;
        packet >> speed;
        packet >> info;
    }
    catch (ByteBufferException const&)
    {
        return true;
    }
    return false;
}

static void test_hostile_counts_rejected()
{
    RefState const state;
    CHECK(Rejects(CMSG_MOVE_START_FORWARD, kStartForward, state, (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_START_FORWARD, kStartForward, state, 2, 1, true));
    CHECK(Rejects(CMSG_MOVE_START_STRAFE_LEFT, kStartStrafeLeft, state, (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_START_STRAFE_RIGHT, kStartStrafeRight, state, (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_STOP_STRAFE, kStopStrafe, state, (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_JUMP, kJump, state, (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_START_TURN_LEFT, kStartTurnLeft, state, (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_START_TURN_RIGHT, kStartTurnRight, state, (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_STOP_TURN, kStopTurn, state, (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_KNOCK_BACK_ACK, kMoveKnockBackAck, state,
        (1u << 22) - 1u, 0, false));
    CHECK(Rejects(CMSG_MOVE_KNOCK_BACK_ACK, kMoveKnockBackAck, state, 2, 1, true));
    CHECK(RejectsForceSwim(state, (1u << 22) - 1u, 0, false));
    CHECK(RejectsForceSwim(state, 2, 1, true));
}

static MovementInfo DecodeRetailAck(OpcodesList opcode, uint8 const* body, size_t size)
{
    WorldPacket packet(opcode, size);
    packet.append(body, size);
    MovementInfo info;
    packet >> info;
    CHECK(packet.rpos() == packet.size());
    return info;
}

static void test_move_change_transport_retail_bodies()
{
    // Captured retail bodies, fetched from catalogue generation
    // 2BE10C899585BAECD237705AC13BBF9262D81B6BDC085B462808C6869CE88752.
    static uint8 const clearParent[] = {
        0x9B, 0x85, 0xF1, 0xC5, 0xD5, 0xD8, 0xEC, 0x44, 0x82, 0xED, 0x9A, 0x41,
        0xE8, 0x00, 0x00, 0x01, 0xC6, 0x00, 0x00, 0x20, 0x00, 0x00, 0x18, 0x00,
        0x28, 0x07, 0x90, 0x05, 0x63, 0x82, 0xED, 0x9A, 0x41, 0xB6, 0x08, 0x02,
        0x00, 0xD5, 0xD8, 0xEC, 0x44, 0xFF, 0xE0, 0x35, 0x4C, 0x3F, 0x9B, 0x85,
        0xF1, 0xC5, 0xE0, 0x35, 0x4C, 0x3F, 0x68, 0x16, 0x04, 0x00 };
    static uint8 const attached[] = {
        0x05, 0x01, 0x02, 0xC6, 0x2A, 0xF9, 0xB1, 0x44, 0x42, 0x9E, 0x99, 0x41,
        0xE8, 0x00, 0x00, 0x01, 0xC6, 0x08, 0x30, 0x20, 0x00, 0x00, 0x58, 0x00,
        0x3D, 0x04, 0xE9, 0x05, 0xC9, 0xC1, 0x60, 0xF7, 0x99, 0x41, 0x91, 0xEF,
        0x00, 0x00, 0x29, 0x00, 0x54, 0xA2, 0x3F, 0xFF, 0xEC, 0xE8, 0x86, 0x40,
        0x1E, 0x00, 0x5E, 0xFB, 0x41, 0xB8, 0xFB, 0x87, 0x3F, 0xDB, 0x46, 0xE3,
        0x00 };
    static uint8 const fallDirection[] = {
        0xBD, 0x4F, 0x03, 0x46, 0xA7, 0xF3, 0x75, 0x44, 0xEB, 0xAB, 0x08, 0x44,
        0xE8, 0x00, 0x00, 0x01, 0xCE, 0x80, 0x00, 0x00, 0x02, 0x00, 0x20, 0x3D,
        0x04, 0xE9, 0x05, 0xC9, 0x00, 0x00, 0x00, 0x00, 0x18, 0xBD, 0xF7, 0xBE,
        0x00, 0x00, 0x00, 0x00, 0x5D, 0x09, 0x60, 0x3F, 0x02, 0x00, 0x00, 0x00,
        0xEB, 0xAB, 0x08, 0x44, 0x00, 0x00, 0x00, 0x00, 0xA7, 0xF3, 0x75, 0x44,
        0x00, 0x5C, 0xE6, 0xB8, 0x40, 0xBD, 0x4F, 0x03, 0x46, 0x5C, 0xE6, 0xB8,
        0x40, 0xB6, 0x2B, 0x14, 0x00 };
    static uint8 const transportTime2[] = {
        0x0E, 0xDD, 0x7B, 0xC5, 0x1F, 0xBE, 0xC6, 0x45, 0x33, 0x0B, 0x81, 0x41,
        0xE8, 0x00, 0x00, 0x01, 0xC6, 0x88, 0xF0, 0x00, 0x02, 0x00, 0x28, 0x00,
        0x28, 0x07, 0x90, 0x05, 0x63, 0x00, 0x00, 0x00, 0x00, 0x28, 0x89, 0x5F,
        0xBF, 0x00, 0x00, 0x00, 0x00, 0xAB, 0x8A, 0xF9, 0xBE, 0x00, 0x00, 0x00,
        0x00, 0xC1, 0x4C, 0xBF, 0x43, 0x40, 0xFE, 0x13, 0x00, 0x00, 0xA5, 0x2C,
        0x00, 0x00, 0x80, 0x00, 0x00, 0x6E, 0xC0, 0x0A, 0x00, 0x65, 0x81, 0x86,
        0x40, 0x1E, 0x00, 0xD8, 0xD1, 0x40, 0xA9, 0x32, 0xC4, 0x40, 0x34, 0x5B,
        0x30, 0x00 };

    MovementInfo const c = DecodeRetailAck(CMSG_MOVE_CHNG_TRANSPORT,
        clearParent, sizeof(clearParent));
    CHECK(c.GetGuid().GetRawValue() == UINT64_C(0x0400000006296291));
    CHECK(c.GetPos()->x == -7728.70068359375f && c.GetPos()->y == 1894.7760009765625f);
    CHECK(c.GetPos()->z == 19.365970611572266f && c.GetPos()->o == 0.7976970672607422f);
    CHECK(uint32(c.GetMovementFlags()) == 0x00800000u && uint16(c.GetMovementFlags2()) == 0x0C00u);
    CHECK(c.GetTransportGuid().IsEmpty());
    CHECK(c.GetTransportPos()->x == c.GetPos()->x && c.GetTransportPos()->y == c.GetPos()->y);
    CHECK(c.GetTransportPos()->z == c.GetPos()->z && c.GetTransportPos()->o == c.GetPos()->o);
    CHECK(c.GetTransportTime() == 133302u && c.GetTransportSeat() == -1);
    CHECK(c.GetTime() == 267880u);

    MovementInfo const a = DecodeRetailAck(CMSG_MOVE_CHNG_TRANSPORT,
        attached, sizeof(attached));
    CHECK(a.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(a.GetTransportGuid().GetRawValue() == UINT64_C(0x1FC0000000000028));
    CHECK(a.GetPos()->x == -8320.2548828125f && a.GetPos()->y == 1423.786376953125f);
    CHECK(a.GetPos()->z == 19.202274322509766f && a.GetPos()->o == 1.0623693466186523f);
    CHECK(a.GetTransportPos()->x == 31.4208984375f && a.GetTransportPos()->y == 1.2681884765625f);
    CHECK(a.GetTransportPos()->z == 19.24578857421875f && a.GetTransportPos()->o == 4.215932846069336f);
    CHECK(uint32(a.GetMovementFlags()) == 0x00800001u && uint16(a.GetMovementFlags2()) == 0x0C00u);
    CHECK(a.GetTransportTime() == 61329u && a.GetTransportSeat() == -1);
    CHECK(a.GetTime() == 14894811u);

    MovementInfo const f = DecodeRetailAck(CMSG_MOVE_CHNG_TRANSPORT,
        fallDirection, sizeof(fallDirection));
    CHECK(f.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(f.GetTransportGuid().IsEmpty());
    CHECK(uint32(f.GetMovementFlags()) == 0x00000800u);
    CHECK(f.GetStatusInfo().hasFallDirection && f.GetFallTime() == 2u);
    CHECK(f.GetJumpInfo().velocity == 0.0f && f.GetJumpInfo().xyspeed == 0.0f);
    CHECK(f.GetJumpInfo().sinAngle == -0.48386454582214355f);
    CHECK(f.GetJumpInfo().cosAngle == 0.8751428723335266f);
    CHECK(f.GetPos()->o == 5.778120040893555f && f.GetTime() == 1321910u);

    MovementInfo const t = DecodeRetailAck(CMSG_MOVE_CHNG_TRANSPORT,
        transportTime2, sizeof(transportTime2));
    CHECK(uint32(t.GetMovementFlags()) == 0x00000800u && uint16(t.GetMovementFlags2()) == 0x0800u);
    CHECK(t.GetTransportGuid().GetRawValue() == UINT64_C(0x1FC0000000000B81));
    CHECK(t.GetTransportPos()->x == 6.5576171875f && t.GetTransportPos()->y == -3.71875f);
    CHECK(t.GetTransportPos()->z == 3.0585508346557617f && t.GetTransportPos()->o == 4.2032952308654785f);
    CHECK(t.GetTransportTime() == 5118u && t.GetTransportTime2() == 11429u);
    CHECK(t.GetStatusInfo().hasTransportTime2 && t.GetTime() == 3169076u);
}

static void test_forced_movement_ack_retail_bodies()
{
    // Captured retail bodies; unlike the binary-derived synthetic fixtures
    // below, none of these bytes was produced by this test.
    // capture-000004/2069: ordinary player mover.
    static uint8 const rootOrdinary[] = {
        0x66, 0xE7, 0x05, 0x46, 0x78, 0x00, 0x00, 0x00, 0xA1, 0x06, 0x27, 0x44,
        0x72, 0xDA, 0x08, 0x44, 0x28, 0x6E, 0x00, 0x00, 0x01, 0x00, 0x00, 0x06,
        0x00, 0x40, 0x00, 0xC9, 0xE9, 0x05, 0x04, 0x3D, 0x89, 0x92, 0x0F, 0x00,
        0x0E, 0x04, 0x90, 0x40 };
    // capture-000006/64719: fall-direction arm.
    static uint8 const rootFall[] = {
        0xD4, 0x9E, 0x2B, 0xC6, 0xD7, 0x00, 0x00, 0x00, 0x11, 0x60, 0xA1, 0xC4,
        0x47, 0x66, 0x2F, 0x41, 0x29, 0x7E, 0x00, 0x00, 0x01, 0x00, 0x08, 0x08,
        0x00, 0x80, 0xC9, 0xE9, 0x05, 0x04, 0x3D, 0x03, 0x37, 0xDD, 0xBD, 0x93,
        0x80, 0x7E, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x25,
        0x7B, 0xD5, 0xC0, 0xC3, 0x93, 0x01, 0x01, 0x06, 0xF1, 0x79, 0x40 };
    // capture-000183/110806: transport arm.
    static uint8 const rootTransport[] = {
        0xFE, 0xBD, 0xD4, 0xC3, 0x33, 0x00, 0x00, 0x00, 0xF6, 0xAE, 0x17, 0x45,
        0xCE, 0x4F, 0xEC, 0x43, 0x28, 0xEF, 0x00, 0x00, 0x01, 0x22, 0xA0, 0x00,
        0x01, 0x80, 0x10, 0x00, 0x23, 0x07, 0x00, 0x05, 0xB3, 0x81, 0xC4, 0xAE,
        0x9C, 0x8B, 0x41, 0xC1, 0x68, 0x96, 0xE3, 0x41, 0x8D, 0xD2, 0x00, 0x00,
        0xA8, 0x79, 0x10, 0x42, 0x1E, 0x5D, 0x03, 0xD7, 0x3F, 0x05, 0xFF, 0x18,
        0x12, 0xEF, 0x15, 0xB0, 0x7E, 0xEB, 0x3F };
    // capture-000086/92717: minimum body with controlled-vehicle mover.
    static uint8 const unrootVehicle[] = {
        0x7D, 0x9F, 0x26, 0xC3, 0x39, 0x6C, 0x22, 0x44, 0x1F, 0x02, 0x00, 0x00,
        0xC7, 0x68, 0x25, 0x43, 0xD7, 0x33, 0x40, 0x00, 0x00, 0xC0, 0xDF, 0xEE,
        0x51, 0x00, 0xEA, 0x18, 0xF0, 0x71, 0x65, 0x28, 0x00 };
    // capture-000006/65133: fall-direction arm.
    static uint8 const unrootFall[] = {
        0xD4, 0x9E, 0x2B, 0xC6, 0x11, 0x60, 0xA1, 0xC4, 0xD9, 0x00, 0x00, 0x00,
        0x0F, 0x0C, 0x2D, 0x41, 0xF1, 0xA2, 0x40, 0x00, 0x00, 0x10, 0x01, 0x00,
        0x00, 0x20, 0x00, 0xC9, 0xE9, 0x3D, 0x04, 0x05, 0x67, 0x4F, 0x73, 0xBF,
        0x00, 0x00, 0x00, 0x00, 0x38, 0x33, 0x9F, 0x3E, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xE6, 0xAE, 0x01, 0x01, 0x92, 0xD3, 0x34, 0x40 };
    // capture-000004/9778: vehicle transport plus fall.
    static uint8 const unrootTransportFall[] = {
        0xBD, 0x4F, 0x03, 0x46, 0xA7, 0xF3, 0x75, 0x44, 0xE1, 0x00, 0x00, 0x00,
        0xEC, 0xAB, 0x08, 0x44, 0xF1, 0xA2, 0xC0, 0x00, 0x00, 0x1B, 0xE4, 0x00,
        0x40, 0x00, 0x08, 0x00, 0xC9, 0xE9, 0x3D, 0x04, 0x05, 0x00, 0xF0, 0x24,
        0x00, 0x00, 0x00, 0x00, 0x1E, 0x00, 0x00, 0xD0, 0x3F, 0x83, 0x00, 0x00,
        0x00, 0x00, 0x51, 0x22, 0xDA, 0xAC, 0x0A, 0xBF, 0xD6, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB4, 0x2B, 0x14,
        0x00, 0x5C, 0xE6, 0xB8, 0x40 };
    // capture-000261/688214: minimum-shaped land/water ACK.
    static uint8 const waterMinimum[] = {
        0xE1, 0x22, 0x86, 0x44, 0xAE, 0x3D, 0xB0, 0xC5, 0x7F, 0x00, 0x00, 0x00,
        0x6A, 0xBC, 0x9E, 0xC3, 0x56, 0x45, 0x40, 0x00, 0x00, 0x44, 0x00, 0x00,
        0x00, 0x40, 0x00, 0x05, 0x70, 0x04, 0xBC, 0x3D, 0x66, 0xAA, 0xE2, 0x02 };
    // capture-000020/27534: oriented water-walk pair.
    static uint8 const waterOriented[] = {
        0x5D, 0xA8, 0x34, 0x44, 0x7B, 0x4F, 0x09, 0x45, 0x19, 0x00, 0x00, 0x00,
        0xAC, 0xD9, 0xC0, 0x43, 0x56, 0x65, 0x40, 0x00, 0x00, 0x04, 0x00, 0x00,
        0x00, 0x40, 0x00, 0x00, 0x07, 0x05, 0x23, 0x81, 0xB3, 0xE9, 0x8F, 0xCB,
        0x3F, 0xA9, 0xD6, 0x39, 0x16 };
    // capture-000019/112577: land-walk pair with fall direction.
    static uint8 const landFall[] = {
        0x20, 0x0D, 0x78, 0x44, 0x2F, 0x9B, 0x65, 0x44, 0x2C, 0x02, 0x00, 0x00,
        0xBA, 0xE9, 0xC1, 0x43, 0x76, 0x45, 0x40, 0x00, 0x00, 0x00, 0x80, 0x08,
        0x00, 0xB0, 0x00, 0x07, 0x02, 0x06, 0x4E, 0x2C, 0x59, 0x8B, 0x49, 0xBF,
        0x01, 0xD9, 0x1D, 0xBF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x80, 0x95, 0x73, 0x40, 0x80, 0x4F, 0x86, 0x00 };
    // capture-000360/504821: maximum observed transport/fall body.
    static uint8 const waterMaximum[] = {
        0x1B, 0x5D, 0xA5, 0x44, 0xAF, 0xF6, 0xA4, 0xC5, 0x1F, 0x02, 0x00, 0x00,
        0x12, 0xB2, 0x04, 0x41, 0x76, 0x47, 0x40, 0x00, 0x00, 0x28, 0x30, 0x00,
        0x02, 0x00, 0x28, 0x00, 0x05, 0x70, 0x04, 0xBC, 0x3D, 0x0C, 0x00, 0x00,
        0x00, 0x00, 0x88, 0xD0, 0xD1, 0xBF, 0x12, 0x78, 0xBD, 0xC0, 0x1E, 0xB2,
        0x1D, 0x0F, 0x40, 0xFF, 0xC1, 0x36, 0xDC, 0x74, 0xA9, 0x40, 0xC1, 0x0B,
        0x1E, 0xBF, 0x8F, 0x63, 0x49, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xD0, 0xEE, 0x43, 0x3F, 0x94, 0x42,
        0xB1, 0x02 };

    MovementInfo const r0 = DecodeRetailAck(CMSG_FORCE_MOVE_ROOT_ACK, rootOrdinary, sizeof(rootOrdinary));
    CHECK(r0.GetMovementCounter() == 120u);
    CHECK(r0.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(r0.GetPos()->x == 8569.849609375f && r0.GetPos()->y == 668.10357666015625f && r0.GetPos()->z == 547.4132080078125f);
    CHECK(uint32(r0.GetMovementFlags()) == 0x00000600u && uint16(r0.GetMovementFlags2()) == 0x0800u);
    CHECK(r0.GetTime() == 1020553u && r0.GetPos()->o == 4.500494956970215f);

    MovementInfo const r1 = DecodeRetailAck(CMSG_FORCE_MOVE_ROOT_ACK, rootFall, sizeof(rootFall));
    CHECK(r1.GetMovementCounter() == 215u);
    CHECK(r1.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(uint32(r1.GetMovementFlags()) == 0x00080800u && uint16(r1.GetMovementFlags2()) == 0u);
    CHECK(r1.GetJumpInfo().cosAngle == -0.10801508277654648f);
    CHECK(r1.GetJumpInfo().sinAngle == -0.994149386882782f);
    CHECK(r1.GetJumpInfo().xyspeed == 0.0f && r1.GetFallTime() == 23u);
    CHECK(r1.GetJumpInfo().velocity == -6.6712822914123535f);
    CHECK(r1.GetTime() == 16880579u && r1.GetPos()->o == 3.9053359031677246f);

    MovementInfo const r2 = DecodeRetailAck(CMSG_FORCE_MOVE_ROOT_ACK, rootTransport, sizeof(rootTransport));
    CHECK(r2.GetMovementCounter() == 51u);
    CHECK(r2.GetGuid().GetRawValue() == UINT64_C(0x0180000004B22206));
    CHECK(r2.GetTransportGuid().GetRawValue() == UINT64_C(0x1FC00000000004C5));
    CHECK(r2.GetTransportSeat() == -1 && r2.GetTransportTime() == 53901u);
    CHECK(r2.GetTransportPos()->x == 17.45150375366211f && r2.GetTransportPos()->y == 28.448440551757812f);
    CHECK(r2.GetTransportPos()->z == 36.118804931640625f && r2.GetTransportPos()->o == 1.67979014f);

    MovementInfo const u0 = DecodeRetailAck(CMSG_FORCE_MOVE_UNROOT_ACK, unrootVehicle, sizeof(unrootVehicle));
    CHECK(u0.GetMovementCounter() == 543u);
    CHECK(u0.GetGuid().GetRawValue() == UINT64_C(0xF150EB190001DEEF));
    CHECK(u0.GetPos()->x == -166.6230010986328f && u0.GetPos()->y == 649.6909790039062f && u0.GetPos()->z == 165.40928649902344f);
    CHECK(u0.GetTime() == 2647409u);

    MovementInfo const u1 = DecodeRetailAck(CMSG_FORCE_MOVE_UNROOT_ACK, unrootFall, sizeof(unrootFall));
    CHECK(u1.GetMovementCounter() == 217u);
    CHECK(u1.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(u1.GetJumpInfo().cosAngle == -0.9504303336143494f);
    CHECK(u1.GetJumpInfo().sinAngle == 0.31093764305114746f);
    CHECK(u1.GetFallTime() == 0u && u1.GetJumpInfo().velocity == 0.0f);
    CHECK(u1.GetTime() == 16887526u && u1.GetPos()->o == 2.825413227081299f);

    MovementInfo const u2 = DecodeRetailAck(CMSG_FORCE_MOVE_UNROOT_ACK, unrootTransportFall, sizeof(unrootTransportFall));
    CHECK(u2.GetMovementCounter() == 225u);
    CHECK(u2.GetTransportGuid().GetRawValue() == UINT64_C(0xF150822500231FD7));
    CHECK(u2.GetTransportSeat() == 0 && u2.GetTransportTime() == 0u);
    CHECK(u2.GetTransportPos()->x == -0.541700005531311f && u2.GetTransportPos()->z == 1.625f);
    CHECK(u2.GetJumpInfo().cosAngle == 1.0f && u2.GetJumpInfo().sinAngle == 0.0f);
    CHECK(u2.GetJumpInfo().xyspeed == 0.0f && u2.GetJumpInfo().velocity == 0.0f);
    CHECK(u2.GetTime() == 1321908u && u2.GetPos()->o == 5.778120040893555f);

    MovementInfo const w0 = DecodeRetailAck(CMSG_MOVE_WATER_WALK_ACK, waterMinimum, sizeof(waterMinimum));
    CHECK(w0.GetMovementCounter() == 127u);
    CHECK(w0.GetGuid().GetRawValue() == UINT64_C(0x04000000053CBD71));
    CHECK(uint32(w0.GetMovementFlags()) == 0x04000000u && uint16(w0.GetMovementFlags2()) == 0x0800u);
    CHECK(w0.GetTime() == 48409190u);

    MovementInfo const w1 = DecodeRetailAck(CMSG_MOVE_WATER_WALK_ACK, waterOriented, sizeof(waterOriented));
    CHECK(w1.GetMovementCounter() == 25u);
    CHECK(w1.GetGuid().GetRawValue() == UINT64_C(0x0180000004B22206));
    CHECK(w1.GetPos()->o == 1.5903292894363403f && w1.GetTime() == 372889257u);

    MovementInfo const w2 = DecodeRetailAck(CMSG_MOVE_WATER_WALK_ACK, landFall, sizeof(landFall));
    CHECK(w2.GetMovementCounter() == 556u);
    CHECK(w2.GetGuid().GetRawValue() == UINT64_C(0x06000000072D4F03));
    CHECK(uint32(w2.GetMovementFlags()) == 0x00800800u && uint16(w2.GetMovementFlags2()) == 0x0C00u);
    CHECK(w2.GetStatusInfo().hasFallDirection && w2.GetPos()->o == 3.805999755859375f);
    CHECK(w2.GetTime() == 8802176u);

    MovementInfo const w3 = DecodeRetailAck(CMSG_MOVE_WATER_WALK_ACK, waterMaximum, sizeof(waterMaximum));
    CHECK(w3.GetMovementCounter() == 543u);
    CHECK(w3.GetGuid().GetRawValue() == UINT64_C(0x04000000053CBD71));
    CHECK(w3.GetTransportGuid().GetRawValue() == UINT64_C(0x1FC0000000000D37));
    CHECK(w3.GetTransportPos()->x == -5.9209070205688477f);
    CHECK(w3.GetTransportPos()->y == -1.6391763687133789f);
    CHECK(w3.GetTransportPos()->z == 5.2955150604248047f);
    CHECK(w3.GetTransportPos()->o == 2.2361874580383301f);
    CHECK(uint32(w3.GetMovementFlags()) == 0x00000800u && uint16(w3.GetMovementFlags2()) == 0x0800u);
    CHECK(w3.GetStatusInfo().hasFallDirection);
}

template <size_t N>
static void CheckForcedAckSyntheticCoverage(OpcodesList opcode, RefOp const (&sequence)[N])
{
    RefState full;
    Decode(opcode, sequence, full);

    for (size_t byte = 0; byte < 8; ++byte)
    {
        RefState state = full;
        state.guid &= ~(UINT64_C(0xFF) << (byte * 8));
        Decode(opcode, sequence, state);
    }
    for (size_t byte = 0; byte < 8; ++byte)
    {
        RefState state = full;
        state.transportGuid &= ~(UINT64_C(0xFF) << (byte * 8));
        Decode(opcode, sequence, state);
    }
}

static void test_move_update_knock_back_retail_bodies()
{
    // Captured retail bodies prove the ordinary, fall-direction, orientation,
    // and transport arms. They contain no force IDs or rare scalar optionals;
    // the independent synthetic relay below covers those separately.
    static uint8 const body52[] = {
        0x40, 0x00, 0x00, 0x26, 0x9F, 0xC4, 0x00, 0x00,
        0x00, 0xA0, 0x04, 0x28, 0x49, 0x00, 0x00, 0x80,
        0xBF, 0x00, 0x00, 0x00, 0x00, 0x2E, 0xBD, 0x3B,
        0xB3, 0x00, 0x00, 0xA0, 0xC0, 0x00, 0x00, 0x00,
        0x00, 0x92, 0xEB, 0x95, 0xC2, 0x05, 0x51, 0x6F,
        0x20, 0x44, 0x04, 0xD0, 0x02, 0xC3, 0x38, 0xC4,
        0xD6, 0x41, 0x4E, 0x35
    };
    static uint8 const body56[] = {
        0x40, 0x00, 0x00, 0x26, 0x1F, 0xD0, 0x00, 0x00,
        0x00, 0xA0, 0x24, 0x28, 0x49, 0x5A, 0xCF, 0x0F,
        0x3D, 0x00, 0x00, 0x70, 0x41, 0x98, 0xD7, 0x7F,
        0xBF, 0x59, 0x97, 0x37, 0xBF, 0x00, 0x00, 0x00,
        0x00, 0x85, 0x77, 0xA3, 0xC4, 0xBC, 0x68, 0xE9,
        0x3F, 0x05, 0x6B, 0x6D, 0x2D, 0x41, 0x04, 0xD0,
        0x78, 0x8F, 0x2B, 0xC6, 0xEE, 0x90, 0x42, 0x35
    };
    static uint8 const body82[] = {
        0x40, 0x00, 0x00, 0x26, 0x27, 0x87, 0xF4, 0x00,
        0x00, 0x00, 0x28, 0x01, 0x1C, 0x05, 0x50, 0x4B,
        0x11, 0x42, 0x1E, 0xC1, 0x58, 0x5E, 0x30, 0xBE,
        0xC4, 0xFF, 0x06, 0xED, 0x20, 0x40, 0x27, 0x27,
        0x04, 0x42, 0xFF, 0xFF, 0x00, 0x00, 0x8A, 0x85,
        0xBE, 0xED, 0x3E, 0x00, 0x00, 0x58, 0x41, 0x47,
        0xBA, 0x62, 0xBF, 0x2E, 0x8E, 0x90, 0xC1, 0x00,
        0x00, 0x00, 0x00, 0x5D, 0x8B, 0xEF, 0x44, 0x70,
        0x3E, 0xB5, 0x40, 0x04, 0xAD, 0xEF, 0x58, 0x43,
        0x04, 0xB8, 0x7E, 0x5A, 0xF0, 0xC3, 0x61, 0x15,
        0x05, 0x80
    };

    auto decode = [](uint8 const* body, size_t size, MovementInfo& info)
    {
        WorldPacket packet(SMSG_MOVE_UPDATE_KNOCK_BACK, size);
        packet.append(body, size);
        try
        {
            packet >> info;
        }
        catch (ByteBufferException const&)
        {
            return false;
        }
        return packet.rpos() == packet.size();
    };

    MovementInfo ordinary;
    CHECK(decode(body52, sizeof(body52), ordinary));
    CHECK(ordinary.GetGuid().GetRawValue() == UINT64_C(0x04000000054829D1));
    CHECK(uint32(ordinary.GetMovementFlags()) == 0x2801u);
    CHECK(uint16(ordinary.GetMovementFlags2()) == 0x0200u);
    CHECK(ordinary.GetTime() == 894321110u);
    CHECK(ordinary.GetPos()->x == -739.0469970703125f);
    CHECK(ordinary.GetPos()->y == -74.960098266601562f);
    CHECK(ordinary.GetPos()->z == 641.73931884765625f);
    CHECK(ordinary.GetJumpInfo().sinAngle == -1.0f);
    CHECK(ordinary.GetJumpInfo().xyspeed == 0.0f);
    CHECK(ordinary.GetJumpInfo().velocity == -5.0f);

    MovementInfo oriented;
    CHECK(decode(body56, sizeof(body56), oriented));
    CHECK(oriented.GetGuid().GetRawValue() == UINT64_C(0x04000000054829D1));
    CHECK(uint32(oriented.GetMovementFlags()) == 0x2809u);
    CHECK(uint16(oriented.GetMovementFlags2()) == 0x0800u);
    CHECK(oriented.GetTime() == 893554926u);
    CHECK(oriented.GetPos()->x == -10979.8671875f);
    CHECK(oriented.GetPos()->y == -1307.7349853515625f);
    CHECK(oriented.GetPos()->z == 10.839213371276855f);
    CHECK(oriented.GetPos()->o == 1.8235087394714355f);
    CHECK(oriented.GetJumpInfo().xyspeed == 15.0f);

    MovementInfo transported;
    CHECK(decode(body82, sizeof(body82), transported));
    CHECK(transported.GetGuid().GetRawValue() == UINT64_C(0x05000000058B1DB9));
    CHECK(transported.GetTransportGuid().GetRawValue() == UINT64_C(0x1FC00000000004C5));
    CHECK(uint32(transported.GetMovementFlags()) == 0x2801u);
    CHECK(uint16(transported.GetMovementFlags2()) == 0x0800u);
    CHECK(transported.GetTime() == 2147816801u);
    CHECK(transported.GetTransportSeat() == -1);
    CHECK(transported.GetTransportTime() == 65535u);
    CHECK(transported.GetTransportPos()->x == 33.038234710693359f);
    CHECK(transported.GetTransportPos()->y == -0.1722348928451538f);
    CHECK(transported.GetTransportPos()->z == 36.32354736328125f);
    CHECK(transported.GetTransportPos()->o == 2.5144667625427246f);
}

/// CMSG_MOVE_KNOCK_BACK_ACK against a real 18414 body.
///
/// The sequence this tree carried for this opcode was wrong for the build: it
/// desynced on every real body, consuming 34 to 39 bytes of 56 to 87. It never
/// showed because the opcode is unregistered, so the wrong sequence was never
/// reached -- a trap armed for whoever registered it next.
///
/// capture-000006 sequence 64718, build 18414, catalogue 2BE10C89. Consuming
/// exactly is the check that matters most here: the old desync left twenty-odd
/// bytes unread, which no field assertion alone would reveal.
static void test_move_knock_back_ack_retail_body()
{
    static uint8 const body[] = {
        0xD6, 0x00, 0x00, 0x00, 0xAE, 0x9E, 0x2B, 0xC6, 0xB0, 0x06, 0x2D, 0x41,
        0x17, 0x55, 0xA1, 0xC4, 0x43, 0x80, 0x00, 0x01, 0x4A, 0x80, 0x00, 0x28,
        0x01, 0xA0, 0x00, 0xC9, 0xE9, 0x3D, 0x04, 0x05, 0x00, 0x00, 0x70, 0x41,
        0x93, 0x80, 0x7E, 0xBF, 0x03, 0x37, 0xDD, 0xBD, 0x25, 0x7B, 0xD5, 0xC0,
        0x00, 0x00, 0x00, 0x00, 0xAC, 0x93, 0x01, 0x01, 0x06, 0xF1, 0x79, 0x40
    };

    WorldPacket packet(CMSG_MOVE_KNOCK_BACK_ACK, sizeof(body));
    packet.append(body, sizeof(body));

    MovementInfo info;
    packet >> info;

    uint32 counter = 0;
    std::memcpy(&counter, body, sizeof(counter));
    CHECK(counter == 214u);
    CHECK(packet.rpos() == packet.size());
    CHECK(info.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(info.GetPos()->x == -10983.669922f);
    CHECK(info.GetPos()->y == -1290.659058f);
    CHECK(info.GetPos()->z == 10.814133f);
    CHECK(info.GetPos()->o == FloatFromBits(0x4079F106u));
    CHECK(info.GetTime() == 16880556u);
    CHECK(info.GetJumpInfo().xyspeed == FloatFromBits(0x41700000u));
    CHECK(info.GetJumpInfo().sinAngle == FloatFromBits(0xBF7E8093u));
    CHECK(info.GetJumpInfo().cosAngle == FloatFromBits(0xBDDD3703u));
    CHECK(info.GetJumpInfo().velocity == FloatFromBits(0xC0D57B25u));
}

static void test_move_knock_back_retail_bodies()
{
    struct Case
    {
        uint64 guid;
        uint32 counter;
        uint32 horizontalBits;
        uint32 directionYBits;
        uint32 negativeVerticalBits;
        uint32 directionXBits;
        uint8 const* body;
        size_t size;
    };

    // Captured retail bodies. Together they prove the scalar-first shape and
    // three distinct GUID masks; synthetic cases below finish discrimination.
    static uint8 const body26[] = {
        0x00, 0x00, 0x70, 0x41, 0x93, 0x80, 0x7E, 0xBF,
        0x25, 0x7B, 0xD5, 0xC0, 0xD6, 0x00, 0x00, 0x00,
        0x03, 0x37, 0xDD, 0xBD, 0xF1, 0xE9, 0x05, 0x04,
        0xC9, 0x3D
    };
    static uint8 const body27[] = {
        0x00, 0x00, 0x00, 0x00, 0x19, 0x62, 0x77, 0xBF,
        0x00, 0x00, 0x20, 0xC1, 0x3B, 0x00, 0x00, 0x00,
        0x6E, 0xB8, 0x83, 0x3E, 0xF5, 0x81, 0x07, 0x00,
        0x05, 0x23, 0xB3
    };
    static uint8 const body28[] = {
        0x00, 0x00, 0xF0, 0x41, 0x22, 0xFE, 0x54, 0xBE,
        0x00, 0x00, 0xF0, 0xC1, 0x31, 0x01, 0x00, 0x00,
        0xA6, 0x66, 0x7A, 0xBF, 0xFE, 0x51, 0x23, 0xF0,
        0xFE, 0x22, 0xD0, 0x5B
    };
    Case const cases[] = {
        { UINT64_C(0x04000000053CC8E8), 214, 0x41700000u,
          0xBF7E8093u, 0xC0D57B25u, 0xBDDD3703u,
          body26, sizeof(body26) },
        { UINT64_C(0x0180000004B22206), 59, 0x00000000u,
          0xBF776219u, 0xC1200000u, 0x3E83B86Eu,
          body27, sizeof(body27) },
        { UINT64_C(0xF150FF23005AD122), 305, 0x41F00000u,
          0xBE54FE22u, 0xC1F00000u, 0xBF7A66A6u,
          body28, sizeof(body28) }
    };

    for (Case const& c : cases)
    {
        WorldPacket packet(SMSG_MOVE_KNOCK_BACK, c.size);
        MopCompactPackets::BuildMoveKnockBack(packet, c.guid, c.counter,
            FloatFromBits(c.horizontalBits),
            -FloatFromBits(c.negativeVerticalBits),
            FloatFromBits(c.directionXBits),
            FloatFromBits(c.directionYBits));
        CHECK(packet.GetOpcode() == SMSG_MOVE_KNOCK_BACK);
        CHECK(packet.size() == c.size);
        if (packet.size() == c.size)
            CHECK(std::memcmp(packet.contents(), c.body, c.size) == 0);
    }
}

/// The three acknowledgements whose speed is NOT first in the body.
///
/// Four of the nine lead with the speed and are read before the movement block.
/// These three carry it among the leading scalars in their own per-opcode order,
/// so it arrives through the block as MSESpeedFloat -- an element this tree did
/// not have, which is the only reason they could not be expressed before.
///
/// That mattered beyond tidiness: while they were unregistered the speed
/// anti-cheat never ran for walk, run or flight, because the check lives in the
/// acknowledgement handler and the acknowledgement was being dropped. Run is the
/// one a speed hack actually abuses.
///
/// Consuming exactly is necessary but NOT sufficient: the leading scalars are all
/// four bytes wide, so any permutation of them consumes the same total. The
/// position assertions discriminate, and they are not circular. The three bodies
/// carry their scalars in three DIFFERENT orders --
///     walk    counter, speed, X, Y, Z
///     run     counter, Y, Z, X, speed
///     flight  Y, counter, X, Z, speed
/// -- yet walk and flight decode to the same position, because they are adjacent
/// sequences captured at one instant. A decoder reading a wrong order cannot
/// produce that agreement from two different byte layouts. Each float was also
/// confirmed present in the raw body at the offset its sequence names.
///
/// That mattered beyond tidiness: reading a leading float from one of these would
/// swallow a coordinate and desync everything after it, which is precisely what
/// the shared handler used to do.
static void test_speed_embedded_acks_retail_bodies()
{
    {   // walk -- capture-000004 sequence 23271
        static uint8 const body[] = {
            0xF1, 0x01, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x3F, 0x27, 0xE6, 0xC0, 0x45,
            0x21, 0xC4, 0x0D, 0x45, 0xA6, 0xFA, 0xFA, 0x43, 0x6F, 0x00, 0x00, 0x00,
            0x70, 0x10, 0x00, 0xC9, 0xE9, 0x05, 0x04, 0x3D, 0xA2, 0x3E, 0x1F, 0x00,
            0x81, 0x09, 0x3C, 0x40
        };
        WorldPacket packet(CMSG_FORCE_WALK_SPEED_CHANGE_ACK, sizeof(body));
        packet.append(body, sizeof(body));

        MovementInfo info;
        packet >> info;

        CHECK(packet.rpos() == packet.size());
        // 1.25 is exactly representable, so this one needs no bit comparison --
        // it is done by bits anyway to keep the three arms identical.
        float expectedSpeed;
        uint32 const expectedSpeedBits = 0x3FA00000u;
        std::memcpy(&expectedSpeed, &expectedSpeedBits, sizeof(expectedSpeed));
        CHECK(info.GetSpeedFloat() == expectedSpeed);
        CHECK(info.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
        CHECK(info.GetPos()->x == 6172.769043f);
        CHECK(info.GetPos()->y == 2268.258057f);
        CHECK(info.GetPos()->z == 501.958191f);
    }
    {   // run -- capture-000004 sequence 605
        static uint8 const body[] = {
            0x41, 0x00, 0x00, 0x00, 0xCD, 0xCC, 0x5E, 0x44, 0x8F, 0xD2, 0x08, 0x44,
            0x29, 0x6F, 0x04, 0x46, 0x67, 0x66, 0xF6, 0x40, 0x40, 0xE3, 0x00, 0x00,
            0x03, 0x90, 0x00, 0xE9, 0x3D, 0xC9, 0x04, 0x05, 0x94, 0x25, 0x6A, 0x40,
            0xE3, 0x27, 0x0E, 0x00
        };
        WorldPacket packet(CMSG_FORCE_RUN_SPEED_CHANGE_ACK, sizeof(body));
        packet.append(body, sizeof(body));

        MovementInfo info;
        packet >> info;

        CHECK(packet.rpos() == packet.size());
        // The capture's float is not the exact decimal: 7.7 on the wire is
        // 0x40F66667, one ULP from the literal. Compare the bits.
        float expectedSpeed;
        uint32 const expectedSpeedBits = 0x40F66667u;
        std::memcpy(&expectedSpeed, &expectedSpeedBits, sizeof(expectedSpeed));
        CHECK(info.GetSpeedFloat() == expectedSpeed);
        CHECK(info.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
        CHECK(info.GetPos()->x == 8475.790039f);
        CHECK(info.GetPos()->y == 891.200012f);
        CHECK(info.GetPos()->z == 547.289978f);
    }
    {   // flight -- capture-000004 sequence 23270
        static uint8 const body[] = {
            0x21, 0xC4, 0x0D, 0x45, 0xF0, 0x01, 0x00, 0x00, 0x27, 0xE6, 0xC0, 0x45,
            0xA6, 0xFA, 0xFA, 0x43, 0x67, 0x66, 0x76, 0x40, 0x41, 0x00, 0x00, 0x03,
            0xA9, 0x90, 0x00, 0xC9, 0x04, 0x05, 0x3D, 0xE9, 0xA0, 0x3E, 0x1F, 0x00,
            0x81, 0x09, 0x3C, 0x40
        };
        WorldPacket packet(CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK, sizeof(body));
        packet.append(body, sizeof(body));

        MovementInfo info;
        packet >> info;

        CHECK(packet.rpos() == packet.size());
        // The capture's float is not the exact decimal: 3.85 on the wire is
        // 0x40766667, one ULP from the literal. Compare the bits.
        float expectedSpeed;
        uint32 const expectedSpeedBits = 0x40766667u;
        std::memcpy(&expectedSpeed, &expectedSpeedBits, sizeof(expectedSpeed));
        CHECK(info.GetSpeedFloat() == expectedSpeed);
        CHECK(info.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
        CHECK(info.GetPos()->x == 6172.769043f);
        CHECK(info.GetPos()->y == 2268.258057f);
        CHECK(info.GetPos()->z == 501.958191f);
    }
}

/// The Write arm of MSESpeedFloat, which the read fixtures above cannot reach.
///
/// Read and Write share the sequence table, so a symmetric mistake would survive
/// a decode/encode/decode round trip untouched. This checks against the CAPTURE
/// instead: decode a retail body, re-encode it, and require the result to be the
/// retail body back, byte for byte.
///
/// The counter is the one exclusion. MSEMovementCounter writes zero rather than
/// echoing what was read, so those four bytes cannot match and their offset is
/// listed per opcode. Everything else must, which pins not just that the speed is
/// written but that it is written in the right PLACE -- the three opcodes put it
/// at different offsets, so a writer using one order for all three fails here.
/// The water-walk mover halves, against real 18414 bodies.
///
/// These are checked differently from the observer halves above. CheckSplineStatePacket
/// compares a builder to a re-implementation using the SAME constants, so it catches
/// drift but is not wire evidence. Here the expected bytes ARE the wire: each fixture is
/// a captured body, and the builder must reproduce it exactly from the GUID and counter
/// that body decodes to.
///
/// That distinction mattered. The inherited builders had the wrong mask order, the wrong
/// byte order AND the wrong counter position, yet produced bodies of the correct length,
/// because this family is a mask byte, some XOR-1 GUID bytes and a uint32 -- and every
/// permutation of that occupies the same space. Only byte-exact comparison sees it.
///
/// Two masks per opcode, chosen to differ in which GUID bytes are present.
static void test_water_walk_mover_retail_bodies()
{
    struct Case
    {
        OpcodesList opcode;
        uint64 guid;
        uint32 counter;
        uint8 const* body;
        size_t size;
        char const* origin;
    };

    // SMSG_MOVE_WATER_WALK 0x1F9A -- reader sub_C8F544
    static uint8 const water10[] = {
        0xCE, 0x05, 0xE9, 0xC9, 0x3D, 0x04, 0xC1, 0x00, 0x00, 0x00
    };
    static uint8 const water11[] = {
        0xCF, 0x00, 0x07, 0x23, 0x81, 0xB3, 0x05, 0x19, 0x00, 0x00, 0x00
    };
    // SMSG_MOVE_LAND_WALK 0x086A -- reader sub_C8DFF2
    static uint8 const land10[] = {
        0xF2, 0x07, 0x06, 0x2C, 0x02, 0x4E, 0x2C, 0x02, 0x00, 0x00
    };
    static uint8 const land11[] = {
        0xFA, 0x00, 0x81, 0x05, 0xB3, 0x07, 0x23, 0x81, 0x00, 0x00, 0x00
    };

    Case const cases[] = {
        { SMSG_MOVE_WATER_WALK, UINT64_C(0x04000000053CC8E8), 193, water10, sizeof(water10), "capture-000015/25163" },
        { SMSG_MOVE_WATER_WALK, UINT64_C(0x0180000004B22206),  25, water11, sizeof(water11), "capture-000020/27516" },
        { SMSG_MOVE_LAND_WALK,  UINT64_C(0x06000000072D4F03), 556, land10,  sizeof(land10),  "capture-000019/112562" },
        { SMSG_MOVE_LAND_WALK,  UINT64_C(0x0180000004B22206), 129, land11,  sizeof(land11),  "capture-000183/44649" },
    };

    for (Case const& c : cases)
    {
        WorldPacket packet(c.opcode, 13);
        if (c.opcode == SMSG_MOVE_WATER_WALK)
        {
            MopCompactPackets::BuildMoveWaterWalk(packet, c.guid, c.counter);
        }
        else
        {
            MopCompactPackets::BuildMoveLandWalk(packet, c.guid, c.counter);
        }

        CHECK(packet.size() == c.size);
        if (packet.size() != c.size) { continue; }
        CHECK(std::memcmp(packet.contents(), c.body, c.size) == 0);
    }
}

/// The two mover opcodes must not share a layout. They are the same shape and were
/// both wrong before, so a copy-paste between them would be invisible to the
/// fixtures above only if the fixtures were weak -- this states it directly.
/// The fall mover halves, against real 18414 bodies.
///
/// The two put their counter in different places and neither matches the
/// water-walk pair, so there is no family rule to fall back on -- each is its own
/// reader. Normal fall is the awkward one: GUID bytes 3 and 2 come first, then
/// the counter, then the rest, so the counter's byte OFFSET varies with how many
/// of those two are present. A builder that hard-coded an offset would pass on
/// some GUIDs and fail on others.
static void test_fall_mover_retail_bodies()
{
    struct Case
    {
        OpcodesList opcode;
        uint64 guid;
        uint32 counter;
        uint8 const* body;
        size_t size;
        char const* origin;
    };

    // SMSG_MOVE_FEATHER_FALL 0x0C60 -- reader sub_C8BE56. Counter first.
    static uint8 const feather1[] = { 0x7A, 0x23, 0x00, 0x00, 0x00, 0xC9, 0xE9, 0x04, 0x3D, 0x05 };
    static uint8 const feather2[] = { 0x7A, 0x72, 0x03, 0x00, 0x00, 0x63, 0x90, 0x07, 0x28, 0x05 };
    // SMSG_MOVE_NORMAL_FALL 0x08E0 -- reader sub_C898EA. Counter in the middle.
    static uint8 const normal1[] = { 0xD6, 0x04, 0x3D, 0x24, 0x00, 0x00, 0x00, 0xC9, 0x05, 0xE9 };
    static uint8 const normal2[] = { 0xD6, 0x07, 0x28, 0x7B, 0x03, 0x00, 0x00, 0x63, 0x05, 0x90 };

    Case const cases[] = {
        { SMSG_MOVE_FEATHER_FALL, UINT64_C(0x04000000053CC8E8),  35, feather1, sizeof(feather1), "capture-000006/28387" },
        { SMSG_MOVE_FEATHER_FALL, UINT64_C(0x0400000006296291), 882, feather2, sizeof(feather2), "capture-000187/65605" },
        { SMSG_MOVE_NORMAL_FALL,  UINT64_C(0x04000000053CC8E8),  36, normal1,  sizeof(normal1),  "capture-000006/28676" },
        { SMSG_MOVE_NORMAL_FALL,  UINT64_C(0x0400000006296291), 891, normal2,  sizeof(normal2),  "capture-000187/66015" },
    };

    for (Case const& c : cases)
    {
        WorldPacket packet(c.opcode, 13);
        if (c.opcode == SMSG_MOVE_FEATHER_FALL)
        {
            MopCompactPackets::BuildMoveFeatherFall(packet, c.guid, c.counter);
        }
        else
        {
            MopCompactPackets::BuildMoveNormalFall(packet, c.guid, c.counter);
        }

        CHECK(packet.size() == c.size);
        if (packet.size() != c.size) { continue; }
        CHECK(std::memcmp(packet.contents(), c.body, c.size) == 0);
    }
}

/// Normal fall's counter offset really is variable, which no captured body shows
/// because they all share one mask. Two synthetic GUIDs differing only in whether
/// byte 2 is zero must put the counter at different offsets.
/// CMSG_MOVE_SET_CAN_FLY_ACK against real 18414 bodies.
///
/// The inherited sequence read 18 bits where the client writes 42, so every body
/// would have parsed three bytes out of phase. It was never caught because the
/// opcode was unregistered -- but the sequence was already ROUTED, so adding the
/// registration alone would have armed a broken reader. A registered handler
/// with a wrong reader is strictly worse than a dropped packet.
///
/// Rebuilt from the client's writer sub_674EA6. These four bodies were decoded
/// independently before the sequence was written, not after: 35 bytes minimum,
/// a six-byte GUID, the 30-bit flags arm, and both flag arms together. Each
/// recovers the GUID the other opcodes give for the same mover.
///
/// The transport, movement-force and unknown-uint32 arms have NO captured body
/// anywhere in the corpus. They come straight from the writer and ship
/// structurally certain but corpus-unverified; that is stated rather than
/// implied by silence.
static void test_move_set_can_fly_ack_retail_bodies()
{
    struct Case
    {
        uint8 const* body;
        size_t size;
        uint64 guid;
        uint32 counter;
        uint32 timestamp;
        uint32 flags;
        uint16 flags2;
        char const* origin;
    };

    static uint8 const min35[] = {
        0x1b, 0x99, 0xca, 0x43, 0xb8, 0x00, 0x00, 0x00, 0x8a, 0xe8, 0xc3, 0x42,
        0xfb, 0x53, 0xbc, 0x44, 0x65, 0x61, 0x80, 0x00, 0x01, 0x40, 0x04, 0x49,
        0x05, 0x28, 0xd0, 0x4d, 0xe8, 0xc3, 0x40, 0xc5, 0xf2, 0xb9, 0x02
    };
    static uint8 const sixByteGuid37[] = {
        0xf3, 0xda, 0xb9, 0x43, 0x43, 0x00, 0x00, 0x00, 0x7b, 0x1c, 0x2b, 0x44,
        0xcb, 0x0b, 0x02, 0x45, 0x25, 0x69, 0x80, 0x00, 0x01, 0x50, 0x00, 0x81,
        0x05, 0xb3, 0x00, 0x23, 0x07, 0x04, 0x69, 0x7b, 0x3c, 0xa7, 0x51, 0x2d, 0x16
    };
    static uint8 const flagsArm38[] = {
        0x0c, 0x82, 0x1d, 0x43, 0x6f, 0x03, 0x00, 0x00, 0x3d, 0x6a, 0xe1, 0x44,
        0xd7, 0x61, 0x83, 0x45, 0x65, 0x61, 0x00, 0x00, 0x01, 0x40, 0x80, 0x00,
        0x00, 0x04, 0x3d, 0x05, 0xc9, 0xe9, 0x6f, 0x98, 0x8d, 0x3e, 0xae, 0x15, 0x3a, 0x00
    };
    static uint8 const bothArms40[] = {
        0x8f, 0xd2, 0x08, 0x44, 0x42, 0x00, 0x00, 0x00, 0x29, 0x6f, 0x04, 0x46,
        0xcd, 0xcc, 0x5e, 0x44, 0x25, 0x61, 0x00, 0x00, 0x01, 0x50, 0x00, 0x04,
        0x00, 0x00, 0x00, 0x04, 0x3d, 0x05, 0xc9, 0xe9, 0x94, 0x25, 0x6a, 0x40,
        0xe5, 0x27, 0x0e, 0x00
    };

    Case const cases[] = {
        { min35, sizeof(min35), UINT64_C(0x04000000054829D1), 184, 45740741,
          0, 0, "capture-000540/1165180" },
        { sixByteGuid37, sizeof(sixByteGuid37), UINT64_C(0x0180000004B22206), 67, 372068775,
          0, 0x0800, "capture-000020/3995" },
        { flagsArm38, sizeof(flagsArm38), UINT64_C(0x04000000053CC8E8), 879, 3806638,
          0x00800000, 0, "capture-000086/117991" },
        { bothArms40, sizeof(bothArms40), UINT64_C(0x04000000053CC8E8), 66, 927717,
          0x00800000, 0x0800, "capture-000004/606" },
    };

    for (Case const& c : cases)
    {
        WorldPacket packet(CMSG_MOVE_SET_CAN_FLY_ACK, c.size);
        packet.append(c.body, c.size);

        MovementInfo info;
        packet >> info;

        // Exact consumption is necessary but not sufficient -- every leading
        // scalar is four bytes wide. The GUID discriminates most of the layout,
        // being reassembled from a scattered bit order AND a scattered byte
        // order. But not all of it: GUID bytes 4 and 5 are absent from all four
        // of these movers, so swapping their mask and order positions stays
        // invisible here. That part rests on the client writer alone, which is
        // the honest boundary of what these fixtures prove.
        CHECK(packet.rpos() == packet.size());
        CHECK(info.GetGuid().GetRawValue() == c.guid);
        CHECK(uint32(info.GetMovementFlags()) == c.flags);
        CHECK(uint16(info.GetMovementFlags2()) == c.flags2);
        CHECK(info.GetTime() == c.timestamp);
    }
}

/// The prefix is Z, counter, X, Y -- NOT the Y, counter, X, Z the inherited
/// sequence had. Stated on its own because the two are the same length and
/// swapping them is invisible to a consumption check.
/// The hover family against real 18414 bodies.
///
/// The mover pair is asymmetric on the scalar -- SET writes it after two GUID
/// bytes, UNSET second-to-last -- so the two are pinned separately. That
/// asymmetry is precisely what an assumption of symmetry would have got wrong,
/// and the inherited builders were wrong in every position: they decode none of
/// the 51 real bodies in the corpus to a plausible high-GUID class.
///
/// Honest limitation: all 17 captured SET bodies share one presence mask and one
/// mover GUID, so the corpus does not discriminate byte order here -- brute force
/// shows 128 orders survive every cross-check. These layouts rest on the client
/// readers, and the readers' transcription method was validated by reproducing
/// the two already-pinned spline water-walk/land-walk layouts in this same file
/// byte-for-byte. The fixtures prove the builder matches the wire; the reader
/// proves the wire is what the client wants.
static void test_hover_family_retail_bodies()
{
    static uint8 const moverSet[]   = { 0xE9, 0x28, 0xD2, 0x00, 0x00, 0x00, 0x49, 0x04, 0xD0, 0x05 };
    static uint8 const moverUnset[] = { 0x9E, 0x04, 0x49, 0x28, 0xD0, 0xD3, 0x00, 0x00, 0x00, 0x05 };

    {   // SMSG_MOVE_SET_HOVER -- capture-000075 seq 1245414, counter 210
        WorldPacket packet(SMSG_MOVE_SET_HOVER, 13);
        MopCompactPackets::BuildMoveSetHover(packet, UINT64_C(0x04000000054829D1), 210);
        CHECK(packet.size() == sizeof(moverSet));
        if (packet.size() == sizeof(moverSet))
        {
            CHECK(std::memcmp(packet.contents(), moverSet, sizeof(moverSet)) == 0);
        }
    }
    {   // SMSG_MOVE_UNSET_HOVER -- capture-000075 seq 1245582, counter 211
        WorldPacket packet(SMSG_MOVE_UNSET_HOVER, 13);
        MopCompactPackets::BuildMoveUnsetHover(packet, UINT64_C(0x04000000054829D1), 211);
        CHECK(packet.size() == sizeof(moverUnset));
        if (packet.size() == sizeof(moverUnset))
        {
            CHECK(std::memcmp(packet.contents(), moverUnset, sizeof(moverUnset)) == 0);
        }
    }

    // The observer halves. Two high-GUID classes for SET and a third mask for
    // UNSET, so the presence masks genuinely differ rather than repeating one
    // shape.
    struct SplineCase { uint64 guid; uint8 const* body; size_t size; };
    static uint8 const splineUnit[]    = { 0xE6, 0xF0, 0x02, 0x30, 0x17, 0xD8 };
    static uint8 const splineVehicle[] = { 0xCE, 0xF0, 0x7B, 0x51, 0xF3, 0x35 };

    SplineCase const splineSets[] = {
        { UINT64_C(0xF13116D900000300), splineUnit,    sizeof(splineUnit) },
        { UINT64_C(0xF150F2340000007A), splineVehicle, sizeof(splineVehicle) },
    };
    for (SplineCase const& c : splineSets)
    {
        WorldPacket packet(SMSG_SPLINE_MOVE_SET_HOVER, 9);
        MopCompactPackets::BuildSplineMoveSetHover(packet, c.guid);
        CHECK(packet.size() == c.size);
        if (packet.size() == c.size)
        {
            CHECK(std::memcmp(packet.contents(), c.body, c.size) == 0);
        }
    }

    // UNSET has its own mask AND byte order -- pinning only SET would let a
    // wrong or copy-pasted UNSET branch through, which is exactly what a
    // reviewer found this test allowing.
    static uint8 const splineUnsetUnit[] = { 0x7C, 0x17, 0x02, 0x30, 0xF0, 0xD8 };
    static uint8 const splineUnsetOther[] = { 0x3D, 0xF7, 0x31, 0xF0, 0x3A, 0x44 };

    SplineCase const splineUnsets[] = {
        { UINT64_C(0xF13116D900000300), splineUnsetUnit,  sizeof(splineUnsetUnit) },
        { UINT64_C(0xF130F63B00000045), splineUnsetOther, sizeof(splineUnsetOther) },
    };
    for (SplineCase const& c : splineUnsets)
    {
        WorldPacket packet(SMSG_SPLINE_MOVE_UNSET_HOVER, 9);
        MopCompactPackets::BuildSplineMoveUnsetHover(packet, c.guid);
        CHECK(packet.size() == c.size);
        if (packet.size() == c.size)
        {
            CHECK(std::memcmp(packet.contents(), c.body, c.size) == 0);
        }
    }

    // Same GUID, same length, different bodies: SET and UNSET do not share a
    // layout, so a copy-paste between them cannot hide.
    {
        WorldPacket set(SMSG_SPLINE_MOVE_SET_HOVER, 9);
        MopCompactPackets::BuildSplineMoveSetHover(set, UINT64_C(0xF13116D900000300));
        WorldPacket unset(SMSG_SPLINE_MOVE_UNSET_HOVER, 9);
        MopCompactPackets::BuildSplineMoveUnsetHover(unset, UINT64_C(0xF13116D900000300));
        CHECK(set.size() == unset.size());
        CHECK(std::memcmp(set.contents(), unset.contents(), set.size()) != 0);
    }
}

/// The two mover halves must not share a layout -- they differ in mask order,
/// byte order AND scalar position, and a copy-paste between them would keep the
/// length identical.
/// The root family against real 18414 bodies.
///
/// The mover pair splits its GUID around the counter in opposite proportions --
/// ROOT six bytes before and two after, UNROOT two before and six after -- and
/// the scalar is unconditional in both. So the split POINT is the thing a wrong
/// layout gets wrong while keeping the total length identical, which is why two
/// masks are pinned per opcode rather than one.
///
/// The inherited builders scored 0/17 and 0/13 against real bodies.
static void test_root_family_retail_bodies()
{
    // SMSG_FORCE_MOVE_ROOT 0x15AE -- two masks, so the split point is exercised.
    static uint8 const root12[] = { 0xBF, 0x89, 0xF0, 0xBC, 0xAE, 0x51, 0xED,
                                    0x0D, 0x00, 0x00, 0x00, 0xF0 };
    static uint8 const root11[] = { 0xBB, 0x6C, 0xF0, 0x02, 0x51, 0x71,
                                    0x39, 0x00, 0x00, 0x00, 0x15 };
    // SMSG_SPLINE_MOVE_ROOT 0x0728 -- no scalar at all.
    static uint8 const splineRoot[] = { 0xFF, 0x03, 0x0D, 0xD4, 0xF0, 0x31, 0x6A, 0x30, 0x41 };

    {   // capture-000110 seq 191935, counter 13
        WorldPacket packet(SMSG_FORCE_MOVE_ROOT, 13);
        MopCompactPackets::BuildForceMoveRoot(packet, UINT64_C(0xF150EC8800AFBDF1), 13);
        CHECK(packet.size() == sizeof(root12));
        if (packet.size() == sizeof(root12))
        {
            CHECK(std::memcmp(packet.contents(), root12, sizeof(root12)) == 0);
        }
    }
    {   // capture-000663 seq 634810, counter 57 -- a different mask
        WorldPacket packet(SMSG_FORCE_MOVE_ROOT, 13);
        MopCompactPackets::BuildForceMoveRoot(packet, UINT64_C(0xF150706D00000314), 57);
        CHECK(packet.size() == sizeof(root11));
        if (packet.size() == sizeof(root11))
        {
            CHECK(std::memcmp(packet.contents(), root11, sizeof(root11)) == 0);
        }
    }
    {   // capture-000067 seq 62141 -- a PET mover, all eight bytes present
        WorldPacket packet(SMSG_SPLINE_MOVE_ROOT, 9);
        MopCompactPackets::BuildSplineMoveRoot(packet, UINT64_C(0xF140D50C3102306B));
        CHECK(packet.size() == sizeof(splineRoot));
        if (packet.size() == sizeof(splineRoot))
        {
            CHECK(std::memcmp(packet.contents(), splineRoot, sizeof(splineRoot)) == 0);
        }
    }

    // UNROOT, byte-exact. Previously it had only a size check and a
    // "differs from ROOT" check, so its 2/6 split could have been ordered wrongly
    // and still passed -- the claim that two masks were pinned per opcode was
    // true of ROOT and not of UNROOT.
    static uint8 const unroot12a[] = { 0x7F, 0xCF, 0xF0, 0x95, 0x00, 0x00,
                                       0x00, 0xFE, 0x22, 0x42, 0x62, 0x51 };
    static uint8 const unroot12b[] = { 0x7F, 0x60, 0xF0, 0x24, 0x00, 0x00,
                                       0x00, 0x98, 0x7F, 0x00, 0x26, 0x51 };
    static uint8 const splineUnroot[] = { 0xFF, 0x03, 0xF0, 0x31, 0x30, 0xD4, 0x6A, 0x0D, 0x41 };

    struct UnrootCase { uint64 guid; uint32 counter; uint8 const* body; size_t size; };
    UnrootCase const unroots[] = {
        { UINT64_C(0xF150FF23004363CE), 149, unroot12a, sizeof(unroot12a) },
        { UINT64_C(0xF150997E00012761),  36, unroot12b, sizeof(unroot12b) },
    };
    for (UnrootCase const& c : unroots)
    {
        WorldPacket packet(SMSG_FORCE_MOVE_UNROOT, 13);
        MopCompactPackets::BuildForceMoveUnroot(packet, c.guid, c.counter);
        CHECK(packet.size() == c.size);
        if (packet.size() == c.size)
        {
            CHECK(std::memcmp(packet.contents(), c.body, c.size) == 0);
        }
    }
    {
        WorldPacket packet(SMSG_SPLINE_MOVE_UNROOT, 9);
        MopCompactPackets::BuildSplineMoveUnroot(packet, UINT64_C(0xF140D50C3102306B));
        CHECK(packet.size() == sizeof(splineUnroot));
        if (packet.size() == sizeof(splineUnroot))
        {
            CHECK(std::memcmp(packet.contents(), splineUnroot, sizeof(splineUnroot)) == 0);
        }
    }
}

/// Root and unroot must not share a layout, and the observer halves must carry
/// no scalar -- neither client reader contains the uint32 primitive at all.
/// The gravity family against real 18414 bodies.
///
/// Three masks for DISABLE and two for ENABLE, so the split around the counter
/// is exercised rather than just its presence -- DISABLE puts five bytes before
/// and three after, ENABLE seven before and one after.
///
/// These fixtures pin the LAYOUTS. The SENSE was not decidable from the binary
/// and was settled by live test instead: with levitate on, the client holds the
/// player at altitude and refuses to jump, and acknowledges each packet with the
/// matching ack. So levitate-on = DISABLE is confirmed, not assumed.
static void test_gravity_family_retail_bodies()
{
    struct Case { uint64 guid; uint32 counter; uint8 const* body; size_t size; };

    static uint8 const dis12[] = { 0xDF, 0xED, 0xAE, 0xBC, 0x51, 0xF0, 0x0C, 0x00, 0x00, 0x00, 0x89, 0xF0 };
    static uint8 const dis11[] = { 0xF5, 0xB3, 0x23, 0x81, 0x07, 0x54, 0x00, 0x00, 0x00, 0x05, 0x00 };
    static uint8 const dis10[] = { 0x75, 0x3D, 0xC9, 0xE9, 0x77, 0x00, 0x00, 0x00, 0x04, 0x05 };
    static uint8 const ena10[] = { 0xF4, 0x04, 0x3D, 0xC9, 0x05, 0xE9, 0xE0, 0x00, 0x00, 0x00 };
    static uint8 const ena12[] = { 0x7F, 0x6C, 0x46, 0xF0, 0x51, 0x8C, 0x6C, 0x0A, 0x00, 0x00, 0x00, 0xD9 };

    Case const disables[] = {
        { UINT64_C(0xF150EC8800AFBDF1),  12, dis12, sizeof(dis12) },
        { UINT64_C(0x0180000004B22206),  84, dis11, sizeof(dis11) },
        { UINT64_C(0x04000000053CC8E8), 119, dis10, sizeof(dis10) },
    };
    for (Case const& c : disables)
    {
        WorldPacket packet(SMSG_MOVE_GRAVITY_DISABLE, 13);
        MopCompactPackets::BuildMoveGravityDisable(packet, c.guid, c.counter);
        CHECK(packet.size() == c.size);
        if (packet.size() == c.size)
        {
            CHECK(std::memcmp(packet.contents(), c.body, c.size) == 0);
        }
    }

    Case const enables[] = {
        { UINT64_C(0x04000000053CC8E8), 224, ena10, sizeof(ena10) },
        { UINT64_C(0xF150D86D006D478D),  10, ena12, sizeof(ena12) },
    };
    for (Case const& c : enables)
    {
        WorldPacket packet(SMSG_MOVE_GRAVITY_ENABLE, 13);
        MopCompactPackets::BuildMoveGravityEnable(packet, c.guid, c.counter);
        CHECK(packet.size() == c.size);
        if (packet.size() == c.size)
        {
            CHECK(std::memcmp(packet.contents(), c.body, c.size) == 0);
        }
    }

    // The two mover halves and the two observer halves must each differ.
    uint64 const guid = UINT64_C(0x8070605040302010);
    WorldPacket md(SMSG_MOVE_GRAVITY_DISABLE, 13);
    MopCompactPackets::BuildMoveGravityDisable(md, guid, 7);
    WorldPacket me(SMSG_MOVE_GRAVITY_ENABLE, 13);
    MopCompactPackets::BuildMoveGravityEnable(me, guid, 7);
    CHECK(md.size() == 13);
    CHECK(me.size() == 13);
    CHECK(std::memcmp(md.contents(), me.contents(), 13) != 0);

    WorldPacket sd(SMSG_SPLINE_MOVE_GRAVITY_DISABLE, 9);
    MopCompactPackets::BuildSplineMoveGravityDisable(sd, guid);
    WorldPacket se(SMSG_SPLINE_MOVE_GRAVITY_ENABLE, 9);
    MopCompactPackets::BuildSplineMoveGravityEnable(se, guid);
    CHECK(sd.size() == 9);          // observers get no scalar
    CHECK(se.size() == 9);
    CHECK(std::memcmp(sd.contents(), se.contents(), 9) != 0);
}

int main(int, char**)
{
    test_seventeen_inbound_fixtures_and_exact_relay();
    test_flight_input_retail_bodies();
    test_force_run_back_speed_change_ack_fixture();
    test_back_speed_change_ack_round_trips();
    test_move_knock_back_retail_bodies();
    test_move_update_knock_back_retail_bodies();
    test_move_knock_back_ack_retail_body();
    test_speed_embedded_acks_retail_bodies();
    test_water_walk_mover_retail_bodies();
    test_fall_mover_retail_bodies();
    test_move_set_can_fly_ack_retail_bodies();
    test_hover_family_retail_bodies();
    test_root_family_retail_bodies();
    test_gravity_family_retail_bodies();
    test_hostile_counts_rejected();
    test_move_change_transport_retail_bodies();
    test_forced_movement_ack_retail_bodies();
    if (g_fail) return 1;
    std::printf("mop_movement_packets: all checks passed\n");
    return 0;
}
