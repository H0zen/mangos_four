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
    RefOp::FallSin, RefOp::FallHorizontal, RefOp::FallCos, RefOp::FallVertical, RefOp::UnknownUInt32,
    RefOp::SplineElevation, RefOp::PositionO, RefOp::End };

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
    RefOp::FallSin, RefOp::FallHorizontal, RefOp::FallCos, RefOp::FallTime, RefOp::PositionO,
    RefOp::UnknownUInt32, RefOp::SplineElevation, RefOp::End };

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
    RefOp::FallSin, RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallTime, RefOp::FallVertical,
    RefOp::Pitch, RefOp::UnknownUInt32, RefOp::End };

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
    GB(5), RefOp::FallVertical, RefOp::FallCos, RefOp::FallSin, RefOp::FallHorizontal,
    RefOp::FallTime, TB(5), TB(7), RefOp::TransportSeat, TB(4),
    TB(0), RefOp::TransportZ, TB(6), TB(2), RefOp::TransportY,
    RefOp::TransportTime, RefOp::TransportX, RefOp::TransportTime2, TB(1), TB(3),
    RefOp::TransportTime3, RefOp::TransportO, RefOp::SplineElevation, RefOp::PositionO, RefOp::Pitch,
    RefOp::UnknownUInt32, RefOp::Timestamp, RefOp::End };

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
    GB(2), RefOp::FallTime, RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallCos,
    RefOp::FallVertical, RefOp::Pitch, RefOp::TransportY, TB(3), RefOp::TransportX,
    RefOp::TransportO, TB(5), RefOp::TransportTime2, RefOp::TransportZ, TB(2),
    TB(1), TB(7), TB(4), TB(0), RefOp::TransportTime3,
    RefOp::TransportSeat, TB(6), RefOp::TransportTime, RefOp::PositionO, RefOp::SplineElevation,
    RefOp::UnknownUInt32, RefOp::Timestamp, RefOp::End };

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
    GB(7), RefOp::FallCos, RefOp::FallHorizontal, RefOp::FallSin, RefOp::FallVertical,
    RefOp::FallTime, RefOp::Pitch, RefOp::TransportTime3, TB(3), RefOp::TransportTime2,
    TB(7), TB(1), RefOp::TransportX, RefOp::TransportSeat, TB(5),
    TB(4), TB(2), TB(0), RefOp::TransportZ, RefOp::TransportTime,
    RefOp::TransportY, TB(6), RefOp::TransportO, RefOp::PositionO, RefOp::Timestamp,
    RefOp::SplineElevation, RefOp::UnknownUInt32, RefOp::End };

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
    TB(1), TB(2), RefOp::PositionO, RefOp::Timestamp, RefOp::FallCos,
    RefOp::FallSin, RefOp::FallHorizontal, RefOp::FallVertical, RefOp::FallTime, RefOp::UnknownUInt32,
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
    T(0), T(7), RefOp::HasTransportTime2, T(3), T(6), RefOp::HasTransportTime3,
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
            case RefOp::MovementCounter: w.U32(0xCAFEBABEu); break;
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

static std::vector<uint8> EncodePackedGuid(uint64 guid,
    std::initializer_list<uint8> maskOrder, std::initializer_list<uint8> byteOrder)
{
    RefWriter writer;
    for (uint8 index : maskOrder)
        writer.Bit(uint8(guid >> (8 * index)) != 0);
    writer.Align();
    for (uint8 index : byteOrder)
        writer.GuidByte(guid, index);
    return writer.Bytes();
}

typedef void (*SplineStateBuilder)(WorldPacket&, ObjectGuid);

static void CheckSplineStatePacket(OpcodesList opcode, SplineStateBuilder builder,
    std::initializer_list<uint8> maskOrder, std::initializer_list<uint8> byteOrder)
{
    for (uint64 guid : { 0x8070605040302010ull, 0x0000005000002000ull })
    {
        WorldPacket packet(opcode, 9);
        builder(packet, ObjectGuid(guid));
        CHECK(Equal(packet, EncodePackedGuid(guid, maskOrder, byteOrder)));
    }
}

static void test_spline_state_packets()
{
    CheckSplineStatePacket(SMSG_SPLINE_MOVE_SET_NORMAL_FALL,
        &MopCompactPackets::BuildSplineMoveSetNormalFall,
        { 6, 1, 4, 5, 2, 7, 0, 3 }, { 7, 5, 1, 0, 6, 4, 2, 3 });
    CheckSplineStatePacket(SMSG_SPLINE_MOVE_SET_WATER_WALK,
        &MopCompactPackets::BuildSplineMoveSetWaterWalk,
        { 3, 1, 5, 6, 4, 0, 7, 2 }, { 4, 3, 6, 2, 1, 5, 7, 0 });
    CheckSplineStatePacket(SMSG_SPLINE_MOVE_SET_FEATHER_FALL,
        &MopCompactPackets::BuildSplineMoveSetFeatherFall,
        { 1, 5, 6, 3, 7, 2, 4, 0 }, { 7, 1, 6, 4, 5, 3, 2, 0 });
    CheckSplineStatePacket(SMSG_SPLINE_MOVE_SET_LAND_WALK,
        &MopCompactPackets::BuildSplineMoveSetLandWalk,
        { 1, 5, 6, 0, 7, 2, 3, 4 }, { 1, 6, 4, 3, 7, 0, 2, 5 });
}

static void CheckDecoded(MovementInfo const& info, RefState const& s, OpcodesList opcode)
{
    (void)opcode;
    CHECK(info.GetGuid().GetRawValue() == s.guid);
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

static void test_thirteen_inbound_fixtures_and_exact_relay()
{
    RefState const state;
    CHECK(state.hasFlags && state.hasFlags2 && state.hasTimestamp && state.hasOrientation &&
        state.hasPitch && state.hasFall && state.hasFallDirection && state.hasTransport &&
        state.hasTransportTime2 && state.hasTransportTime3 && state.hasSplineElevation);
    std::array<OpcodesList, 13> const opcodes {{ CMSG_MOVE_START_FORWARD, CMSG_MOVE_START_BACKWARD,
        CMSG_MOVE_STOP, MSG_MOVE_HEARTBEAT, CMSG_MOVE_SET_FACING, CMSG_MOVE_FALL_LAND,
        CMSG_MOVE_START_STRAFE_LEFT, CMSG_MOVE_START_STRAFE_RIGHT, CMSG_MOVE_STOP_STRAFE,
        CMSG_MOVE_JUMP, CMSG_MOVE_START_TURN_LEFT, CMSG_MOVE_START_TURN_RIGHT, CMSG_MOVE_STOP_TURN }};
    std::array<std::vector<uint8>, 13> fixtures;
    MovementInfo infos[13] = {
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
        Decode(opcodes[12], kStopTurn, state, &fixtures[12]) };
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

static void test_complementary_and_empty_state()
{
    RefState state;
    state.raw148 = false;
    state.raw149 = true;
    state.raw172 = false;
    state.forceIds.clear();
    state.hasUnknownUInt32 = false;
    state.unknownUInt32 = 0;
    state.hasFlags = state.hasFlags2 = state.hasTimestamp = false;
    state.hasOrientation = state.hasPitch = state.hasFall = false;
    state.hasFallDirection = state.hasTransport = false;
    state.hasTransportTime2 = state.hasTransportTime3 = false;
    state.hasSplineElevation = false;
    MovementInfo const info = Decode(CMSG_MOVE_START_FORWARD, kStartForward, state);
    WorldPacket relay(SMSG_PLAYER_MOVE, 64);
    relay << info;
    CHECK(Equal(relay, Encode(kPlayerMove, state)));
}

static void test_server_built_embedded_guid()
{
    MovementInfo info;
    info.SetMoverGuid(ObjectGuid(uint64(0x8070605040302010ull)));
    RefState state;
    state.guid = 0x8070605040302010ull;
    state.flags = state.flags2 = state.timestamp = 0;
    state.x = state.y = state.z = state.o = 0;
    state.transportGuid = 0;
    state.forceIds.clear();
    state.hasUnknownUInt32 = false;
    state.unknownUInt32 = 0;
    state.raw148 = state.raw149 = state.raw172 = false;
    state.hasFlags = state.hasFlags2 = state.hasTimestamp = false;
    state.hasOrientation = state.hasPitch = state.hasFall = false;
    state.hasFallDirection = state.hasTransport = false;
    state.hasTransportTime2 = state.hasTransportTime3 = false;
    state.hasSplineElevation = false;
    WorldPacket packet(SMSG_PLAYER_MOVE, 64);
    packet << info;
    CHECK(Equal(packet, Encode(kPlayerMove, state)));
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
static void test_force_run_back_speed_change_ack_retail_body()
{
    static uint8 const body[] = {
        0x00, 0x00, 0x10, 0x40, 0xEE, 0x01, 0x00, 0x00, 0x21, 0xC4, 0x0D, 0x45,
        0x27, 0xE6, 0xC0, 0x45, 0xA6, 0xFA, 0xFA, 0x43, 0x50, 0x00, 0x00, 0x34,
        0x0C, 0xD0, 0x00, 0x3D, 0xE9, 0x05, 0xC9, 0x04, 0x81, 0x09, 0x3C, 0x40,
        0x9B, 0x3E, 0x1F, 0x00
    };

    WorldPacket packet(CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK, sizeof(body));
    packet.append(body, sizeof(body));

    float speed = 0.0f;
    MovementInfo info;
    packet >> speed;
    packet >> info;

    CHECK(speed == 2.25f);
    CHECK(packet.rpos() == packet.size());                  // consumes exactly
    CHECK(info.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));

    // The three coordinates are the same width, so a swap among them would still
    // consume exactly and preserve the GUID. Pin each individually, and note the
    // wire order for this opcode is Y, X, Z -- not the order the fields are named
    // in, which is exactly the kind of thing a length check cannot catch.
    CHECK(info.GetPos()->y == 2268.258056640625f);          // wire offset 8
    CHECK(info.GetPos()->x == 6172.76904296875f);           // wire offset 12
    CHECK(info.GetPos()->z == 501.95819091796875f);         // wire offset 16
}

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
static void test_all_speed_ack_sequences_differ()
{
    RefState state;
    state.flags2 = 0x0ABCu;
    std::vector<uint8> const bodies[] = {
        Encode(kForceSwimSpeedChangeAck, state),
        Encode(kForceRunBackSpeedChangeAck, state),
        Encode(kForceSwimBackSpeedChangeAck, state),
        Encode(kForceFlightBackSpeedChangeAck, state),
    };
    for (size_t i = 0; i < 4; ++i)
    {
        for (size_t j = i + 1; j < 4; ++j)
        {
            CHECK(bodies[i] != bodies[j]);
        }
    }
}

/// The acks must NOT share a sequence. They agree on the leading speed and
/// nothing else -- the bit order and the GUID byte order are both their own -- so
/// the same state must encode differently, and a body built for one must not
/// decode cleanly through the other. Without this a registration could quietly
/// be pointed at the wrong sequence and nothing would complain.
static void test_speed_ack_sequences_are_distinct()
{
    RefState state;
    state.flags2 = 0x0ABCu;
    std::vector<uint8> const runBack = Encode(kForceRunBackSpeedChangeAck, state);
    std::vector<uint8> const swim = Encode(kForceSwimSpeedChangeAck, state);
    CHECK(runBack != swim);

    // Cross-decode: a run-back body read as swim must not come out clean. The
    // earlier version of this test only compared the two encodings, which did
    // not actually show the sequences are not interchangeable.
    WorldPacket crossed(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK, runBack.size() + sizeof(float));
    crossed << float(4.5f);
    crossed.append(runBack.data(), runBack.size());

    bool clean = true;
    try
    {
        float speed = 0.0f;
        MovementInfo info;
        crossed >> speed;
        crossed >> info;
        if (crossed.rpos() != crossed.size())
        {
            clean = false;                                  // left a remainder
        }
    }
    catch (...)
    {
        clean = false;                                      // or ran off the end
    }
    CHECK(!clean);
}

static void test_force_swim_speed_change_ack_fixture()
{
    RefState state;
    state.flags2 = 0x0ABCu;
    std::vector<uint8> const movement = Encode(kForceSwimSpeedChangeAck, state);

    WorldPacket packet(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK, movement.size() + sizeof(float));
    packet << float(13.25f);
    packet.append(movement.data(), movement.size());
    CHECK(packet.size() >= 4);
    CHECK(packet.contents()[0] == 0x00);
    CHECK(packet.contents()[1] == 0x00);
    CHECK(packet.contents()[2] == 0x54);
    CHECK(packet.contents()[3] == 0x41);

    float speed = 0.0f;
    MovementInfo info;
    packet >> speed;
    packet >> info;
    CHECK(speed == 13.25f);
    CHECK(packet.rpos() == packet.size());
    CheckDecoded(info, state, CMSG_FORCE_SWIM_SPEED_CHANGE_ACK);
}

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
    CHECK(RejectsForceSwim(state, (1u << 22) - 1u, 0, false));
    CHECK(RejectsForceSwim(state, 2, 1, true));
}

static void test_opcode_values_are_framable()
{
    CHECK(uint32(MSG_MOVE_HEARTBEAT) == 0x01F2u);
    CHECK(uint32(CMSG_MOVE_START_FORWARD) == 0x095Au);
    CHECK(uint32(CMSG_MOVE_START_BACKWARD) == 0x09D8u);
    CHECK(uint32(CMSG_MOVE_STOP) == 0x08F1u);
    CHECK(uint32(CMSG_MOVE_SET_FACING) == 0x1050u);
    CHECK(uint32(CMSG_MOVE_FALL_LAND) == 0x08FAu);
    CHECK(uint32(CMSG_MOVE_START_STRAFE_LEFT) == 0x01F8u);
    CHECK(uint32(CMSG_MOVE_START_STRAFE_RIGHT) == 0x1058u);
    CHECK(uint32(CMSG_MOVE_STOP_STRAFE) == 0x0171u);
    CHECK(uint32(CMSG_MOVE_JUMP) == 0x1153u);
    CHECK(uint32(CMSG_MOVE_START_TURN_LEFT) == 0x01D0u);
    CHECK(uint32(CMSG_MOVE_START_TURN_RIGHT) == 0x107Bu);
    CHECK(uint32(CMSG_MOVE_STOP_TURN) == 0x1170u);
    CHECK(uint32(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK) == 0x1853u);
    CHECK(uint32(SMSG_PLAYER_MOVE) == 0x1A32u);
    CHECK(uint32(SMSG_SPLINE_MOVE_SET_NORMAL_FALL) == 0x0B08u);
    CHECK(uint32(SMSG_SPLINE_MOVE_SET_WATER_WALK) == 0x1823u);
    CHECK(uint32(SMSG_SPLINE_MOVE_SET_FEATHER_FALL) == 0x1893u);
    CHECK(uint32(SMSG_SPLINE_MOVE_SET_LAND_WALK) == 0x18B6u);
    CHECK(uint32(SMSG_PLAYER_MOVE) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(CMSG_FORCE_SWIM_SPEED_CHANGE_ACK) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(SMSG_SPLINE_MOVE_SET_LAND_WALK) < uint32(OPCODE_TABLE_SIZE));
}

static void test_transport_stop_monster_move_fixture()
{
    WorldPacket packet(SMSG_MONSTER_MOVE, 64);
    Movement::PacketBuilder::WriteStopMovement(G3D::Vector3(7.0f, 8.0f, 9.0f), 0xAABBCCDDu,
        packet, ObjectGuid(uint64(0x8877665544332211ull)), ObjectGuid(uint64(0xA8A7A6A5A4A3A2A1ull)), int8(3));

    std::vector<uint8> const expected {
        0x00, 0x00, 0x10, 0x41, 0x00, 0x00, 0xE0, 0x40, 0xDD, 0xCC, 0xBB, 0xAA,
        0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xCE, 0x00, 0x00, 0x0F, 0xFC, 0x00, 0x00, 0x0B,
        0xFC, 0xC0, 0x23, 0xA6, 0xA4, 0xA3, 0xA9, 0xA0, 0xA5, 0xA7, 0xA2, 0x67,
        0x45, 0x76, 0x10, 0x03, 0x89, 0x32, 0x54
    };
    CHECK(Equal(packet, expected));
}

static void test_linear_monster_move_fixture()
{
    Movement::MonsterMoveData move;
    move.position = G3D::Vector3(1.0f, 2.0f, 3.0f);
    move.splineId = 0x11223344u;
    move.moverGuid = ObjectGuid(uint64(0x0807060504030201ull));
    move.duration = 1000;
    move.compressedPath.push_back(G3D::Vector3(1.0f, 1.0f, 1.0f));
    move.uncompressedPath.push_back(G3D::Vector3(9.0f, 10.0f, 11.0f));

    WorldPacket packet(SMSG_MONSTER_MOVE, 64);
    Movement::PacketBuilder::WriteMonsterMove(move, packet);
    std::vector<uint8> const expected {
        0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x80, 0x3F, 0x44, 0x33, 0x22, 0x11,
        0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0xC7, 0x00, 0x00, 0x1F, 0xBC, 0x00, 0x00, 0x18,
        0x00, 0xC0, 0x04, 0x20, 0x00, 0x01, 0x03, 0x00, 0x00, 0x20, 0x41, 0x00,
        0x00, 0x10, 0x41, 0x00, 0x00, 0x30, 0x41, 0x07, 0x05, 0x06, 0x00, 0x09,
        0x02, 0x04, 0xE8, 0x03, 0x00, 0x00
    };
    CHECK(Equal(packet, expected));
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

    CHECK(packet.rpos() == packet.size());
    CHECK(info.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
    CHECK(info.GetPos()->x == -10983.669922f);
    CHECK(info.GetPos()->y == -1290.659058f);
    CHECK(info.GetPos()->z == 10.814133f);
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
/// Consuming exactly is the load-bearing assertion. Reading a leading float from
/// one of these would swallow a coordinate and desync everything after it, which
/// is precisely what the shared handler used to do.
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
        // The capture's float is not the exact decimal: 1.25 on the wire is
        // 0x3FA00000, one ULP from the literal. Compare the bits.
        float expectedSpeed;
        uint32 const expectedSpeedBits = 0x3FA00000u;
        std::memcpy(&expectedSpeed, &expectedSpeedBits, sizeof(expectedSpeed));
        CHECK(info.GetSpeedFloat() == expectedSpeed);
        CHECK(info.GetGuid().GetRawValue() == UINT64_C(0x04000000053CC8E8));
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
    }
}

int main(int, char**)
{
    test_thirteen_inbound_fixtures_and_exact_relay();
    test_complementary_and_empty_state();
    test_server_built_embedded_guid();
    test_force_swim_speed_change_ack_fixture();
    test_force_run_back_speed_change_ack_fixture();
    test_force_run_back_speed_change_ack_retail_body();
    test_back_speed_change_ack_round_trips();
    test_move_knock_back_ack_retail_body();
    test_speed_embedded_acks_retail_bodies();
    test_all_speed_ack_sequences_differ();
    test_speed_ack_sequences_are_distinct();
    test_spline_state_packets();
    test_hostile_counts_rejected();
    test_opcode_values_are_framable();
    test_transport_stop_monster_move_fixture();
    test_linear_monster_move_fixture();
    if (g_fail) return 1;
    std::printf("mop_movement_packets: all checks passed\n");
    return 0;
}
