/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * clients including 5.4.8.18414.
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Captured-retail and binary-derived fixtures for CMSG_USE_ITEM.
 */

#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

namespace
{
    struct DenseGuids
    {
        uint64 item = UINT64_C(0x8877665544332211);
        uint64 target = UINT64_C(0x8171615141312111);
        uint64 itemTarget = UINT64_C(0x8272625242322212);
        uint64 source = UINT64_C(0x8373635343332313);
        uint64 destination = UINT64_C(0x8474645444342414);
    };

    WorldPacket Packet(std::initializer_list<uint8> bytes)
    {
        WorldPacket packet(CMSG_USE_ITEM, bytes.size());
        if (bytes.size())
            packet.append(bytes.begin(), bytes.size());
        return packet;
    }

    bool Read(WorldPacket& packet, MopSpellPackets::UseItemRequest& request)
    {
        return MopSpellPackets::ReadUseItemRequest(packet, request);
    }

    void CheckCore(MopSpellPackets::UseItemRequest const& request, uint8 slot,
        uint8 bag, uint64 itemGuid, uint32 spellId, uint8 castCount)
    {
        CHECK(request.slot == slot);
        CHECK(request.bagIndex == bag);
        CHECK(uint64(request.itemGuid) == itemGuid);
        CHECK(request.cast.spellId == spellId);
        CHECK(request.cast.castCount == castCount);
    }

    WorldPacket BuildDense(bool writeForceIds = true, uint32 forceCount = 1,
        uint64 transportGuidValue = UINT64_C(0x1817161514131211),
        bool hasTransportTime3 = true, bool hasTransportTime2 = true,
        std::string const& targetString = "Target", DenseGuids const& values = DenseGuids())
    {
        ObjectGuid const itemGuid(values.item);
        ObjectGuid const targetGuid(values.target);
        ObjectGuid const itemTargetGuid(values.itemTarget);
        ObjectGuid const sourceGuid(values.source);
        ObjectGuid const destinationGuid(values.destination);
        ObjectGuid const moverGuid(UINT64_C(0x8575655545352515));
        ObjectGuid const transportGuid(transportGuidValue);

        WorldPacket packet(CMSG_USE_ITEM, 256);
        packet << uint8(9) << uint8(20);

        packet.WriteBit(false); // elevation present
        packet.WriteGuidMask<6>(itemGuid);
        packet.WriteBit(false); // target string present
        packet.WriteGuidMask<1>(itemGuid);
        packet.WriteBit(false); // cast flags present
        packet.WriteBit(true);  // destination present
        packet.WriteGuidMask<2, 7, 0>(itemGuid);
        packet.WriteBit(false); // target mask present
        packet.WriteBit(false); // missile speed present
        packet.WriteBit(true);  // movement present
        packet.WriteBit(false); // cast count present
        packet.WriteBit(false); // spell id present
        packet.WriteBit(false); // item-target GUID is not zero
        packet.WriteBit(false); // glyph index present
        packet.WriteBit(false); // target GUID is not zero
        packet.WriteGuidMask<4>(itemGuid);
        packet.WriteBit(true);  // source present
        packet.WriteGuidMask<3, 5>(itemGuid);
        packet.WriteBits(3, 2);
        packet.WriteBits(1, 2);
        packet.WriteBits(2, 2);
        packet.WriteBits(3, 2);

        packet.WriteBit(false); // pitch present
        packet.WriteBit(true);  // transport present
        packet.WriteBit(true);  // consumed unknown bit
        packet.WriteGuidMask<7, 2, 4, 5, 6, 0, 1>(transportGuid);
        packet.WriteBit(hasTransportTime3);
        packet.WriteGuidMask<3>(transportGuid);
        packet.WriteBit(hasTransportTime2);
        packet.WriteGuidMask<6, 2, 1>(moverGuid);
        packet.WriteBits(forceCount, 22);
        packet.WriteBit(true);  // consumed unknown bit
        packet.WriteBit(false); // movement flags2 present
        packet.WriteBit(true);  // fall data present
        packet.WriteGuidMask<5>(moverGuid);
        packet.WriteBit(false); // spline elevation present
        packet.WriteBit(true);  // consumed unknown bit
        packet.WriteGuidMask<7, 0>(moverGuid);
        packet.WriteBit(true);  // fall direction present
        packet.WriteBit(false); // orientation present
        packet.WriteGuidMask<4, 3>(moverGuid);
        packet.WriteBit(false); // timestamp present
        packet.WriteBit(false); // unknown uint32 present
        packet.WriteBit(false); // movement flags present
        packet.WriteBits(0x1234, 13);
        packet.WriteBits(0x12345678, 30);

        packet.WriteGuidMask<3, 1, 7, 4, 2, 0, 6, 5>(sourceGuid);
        packet.WriteGuidMask<2, 4, 1, 7, 6, 0, 3, 5>(destinationGuid);
        packet.WriteBits(targetString.size(), 7);
        packet.WriteGuidMask<1, 0, 5, 3, 6, 4, 7, 2>(targetGuid);
        packet.WriteGuidMask<4, 5, 0, 1, 3, 7, 6, 2>(itemTargetGuid);
        packet.WriteBits(0x15, 5);
        packet.WriteBits(TARGET_FLAG_UNIT | TARGET_FLAG_ITEM |
            TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_STRING, 20);
        packet.FlushBits();

        packet.WriteGuidBytes<0, 5, 6, 3, 4, 2, 1>(itemGuid);
        packet << uint32(0x01020304) << uint32(0x11121314);
        packet << uint32(0x21222324) << uint32(0x31323334);
        packet << uint32(0x41424344) << uint32(0x51525354);
        packet.WriteGuidBytes<7>(itemGuid);

        if (writeForceIds)
            for (uint32 i = 0; i < forceCount; ++i)
                packet << uint32(0x60000000u + i);
        packet << float(21.25f) << float(-22.5f);
        packet.WriteGuidBytes<1>(transportGuid);
        if (hasTransportTime3)
            packet << uint32(0x61626364);
        packet.WriteGuidBytes<7, 5, 2, 4>(transportGuid);
        packet << float(23.75f) << float(-24.5f);
        packet.WriteGuidBytes<0>(transportGuid);
        packet << int8(-3) << uint32(0x71727374);
        packet.WriteGuidBytes<6, 3>(transportGuid);
        if (hasTransportTime2)
            packet << uint32(0x81828384);

        packet << float(-31.25f) << float(32.5f) << float(-33.75f)
               << float(34.5f) << uint32(0x91929394);
        packet.WriteGuidBytes<3, 7, 6, 1>(moverGuid);
        packet << float(41.25f) << float(-42.5f) << uint32(0xA1A2A3A4)
               << float(43.75f);
        packet.WriteGuidBytes<2>(moverGuid);
        packet << float(-44.5f) << uint32(0xB1B2B3B4) << float(45.75f);
        packet.WriteGuidBytes<5, 0>(moverGuid);
        packet << float(-46.5f);
        packet.WriteGuidBytes<4>(moverGuid);

        packet.WriteGuidBytes<7>(destinationGuid);
        packet << float(51.25f);
        packet.WriteGuidBytes<0, 6, 1, 3>(destinationGuid);
        packet << float(-52.5f);
        packet.WriteGuidBytes<5>(destinationGuid);
        packet << float(53.75f);
        packet.WriteGuidBytes<4, 2>(destinationGuid);

        packet.WriteGuidBytes<6, 7, 2, 0, 3, 4, 1, 5>(itemTargetGuid);
        packet.WriteGuidBytes<7>(sourceGuid);
        packet << float(-61.25f);
        packet.WriteGuidBytes<1, 5, 4>(sourceGuid);
        packet << float(62.5f);
        packet.WriteGuidBytes<6, 0, 3>(sourceGuid);
        packet << float(-63.75f);
        packet.WriteGuidBytes<2>(sourceGuid);

        packet << uint32(0xC1C2C3C4);
        packet.WriteGuidBytes<1, 4, 3, 6, 2, 0, 7, 5>(targetGuid);
        if (!targetString.empty())
            packet.append(reinterpret_cast<uint8 const*>(targetString.data()), targetString.size());
        packet << float(71.25f) << uint32(0xD1D2D3D4) << float(-72.5f) << uint8(73);
        return packet;
    }
}

static void test_captured_retail_bodies()
{
    struct Fixture
    {
        std::initializer_list<uint8> bytes;
        uint8 slot;
        uint8 bag;
        uint64 itemGuid;
        uint32 spellId;
        uint8 castCount;
        uint32 targetMask;
        uint64 targetGuid;
        uint64 destinationTransportGuid;
        float destinationX;
        float destinationY;
        float destinationZ;
    };

    Fixture const fixtures[] = {
        {{ 0x17,0xFF,0xBB,0xE3,0xC0,0x00,0x00,0x26,0x05,0xA4,0x10,0x45,0xF2,0x21,0x00,0x00,0xDF },
            23,255,UINT64_C(0x4400000400A51127),8690,223,0,0,0,0.0f,0.0f,0.0f},
        {{ 0x0C,0x13,0xAB,0xA3,0x51,0xA6,0x00,0x00,0x00,0x40,0x98,0x1F,0x05,0xE7,0x45,0x49,0xB0,0x00,0x00,0x9C,0x04,0x4C,0xE6,0x05,0xBB },
            12,19,UINT64_C(0x440000041EE60099),45129,187,0x2,UINT64_C(0x04000000054D9DE7),0,0.0f,0.0f,0.0f},
        {{ 0x03,0xFF,0xFF,0xA3,0xD0,0x78,0x00,0x00,0x00,0x08,0x00,0x67,0x81,0x13,0x05,0x74,0xFC,0x40,0x1E,0x20,0x26,0x61,0x41,0xC4,0xC1,0x05,0x00,0x30,0x98,0x3E,0x04,0x41,0x0C,0x42,0x25,0x0C,0x01,0x00,0xE2 },
            3,255,UINT64_C(0x418000041275FD66),68645,226,0x40,0,
            UINT64_C(0x1FC00000000004C5),14.07f,0.30f,35.06f},
        {{ 0x08,0x13,0xB3,0xF3,0xD1,0x18,0x00,0x00,0x01,0x65,0xA0,0x00,0x00,0x02,0x00,0xF5,0x0D,0x05,0xC2,0x5F,0x45,0x04,0x05,0x28,0xCA,0x2E,0x98,0xC5,0x6B,0x8C,0x9E,0x40,0x49,0xB6,0x0B,0x39,0x41,0x45,0xF9,0x1F,0x03,0x7E,0x0E,0xB2,0x44,0xD0,0xD8,0x05,0x02,0x00,0x7E },
            8,19,UINT64_C(0x440000040CC35EF4),132568,126,0,0,0,0.0f,0.0f,0.0f},
        {{ 0x05,0x13,0xB3,0xB3,0x51,0x18,0x00,0x00,0x01,0x65,0xA0,0x03,0xB8,0x02,0x00,0x10,0x00,0x75,0x57,0x05,0x15,0x40,0x45,0x04,0x05,0xB1,0xB4,0xB7,0x4A,0x45,0xAD,0x83,0x41,0x40,0x5B,0x0C,0x8F,0xD6,0x43,0x97,0xBC,0x89,0x01,0xEA,0x19,0x2A,0x45,0x6C,0xFC,0xCA,0x01,0x00,0x21,0x36,0x12,0xB4,0xF0,0x3B,0x54 },
            5,19,UINT64_C(0x4400000456144174),117500,84,0x800,UINT64_C(0xF1133A37000020B5),0,0.0f,0.0f,0.0f}
    };

    for (Fixture const& fixture : fixtures)
    {
        WorldPacket packet = Packet(fixture.bytes);
        MopSpellPackets::UseItemRequest request;
        CHECK(Read(packet, request));
        CHECK(packet.rpos() == packet.size());
        CheckCore(request, fixture.slot, fixture.bag, fixture.itemGuid,
            fixture.spellId, fixture.castCount);
        CHECK(request.cast.targetMask == fixture.targetMask);
        CHECK(uint64(request.cast.targetGuid) == fixture.targetGuid);
        CHECK(uint64(request.cast.destinationTransportGuid) == fixture.destinationTransportGuid);
        CHECK(std::fabs(request.cast.destinationX - fixture.destinationX) < 0.005f);
        CHECK(std::fabs(request.cast.destinationY - fixture.destinationY) < 0.005f);
        CHECK(std::fabs(request.cast.destinationZ - fixture.destinationZ) < 0.005f);
    }
}

static void test_binary_derived_dense_body()
{
    WorldPacket packet = BuildDense();
    MopSpellPackets::UseItemRequest request;
    CHECK(Read(packet, request));
    CHECK(packet.rpos() == packet.size());
    CheckCore(request, 9, 20, UINT64_C(0x8877665544332211), 0xC1C2C3C4, 73);
    CHECK(request.cast.castFlags == 0x15);
    CHECK(request.cast.glyphIndex == 0xD1D2D3D4);
    CHECK(request.cast.targetMask == (TARGET_FLAG_UNIT | TARGET_FLAG_ITEM |
        TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_STRING));
    CHECK(uint64(request.cast.targetGuid) == UINT64_C(0x8171615141312111));
    CHECK(uint64(request.cast.itemTargetGuid) == UINT64_C(0x8272625242322212));
    CHECK(uint64(request.cast.sourceTransportGuid) == UINT64_C(0x8373635343332313));
    CHECK(uint64(request.cast.destinationTransportGuid) == UINT64_C(0x8474645444342414));
    CHECK(request.cast.sourceX == -61.25f);
    CHECK(request.cast.sourceY == -63.75f);
    CHECK(request.cast.sourceZ == 62.5f);
    CHECK(request.cast.destinationX == 51.25f);
    CHECK(request.cast.destinationY == -52.5f);
    CHECK(request.cast.destinationZ == 53.75f);
    CHECK(request.cast.targetString == "Target");
    CHECK(request.cast.elevation == 71.25f);
    CHECK(request.cast.missileSpeed == -72.5f);
}

static void test_transport_guid_byte_three_is_consumed()
{
    WorldPacket packet = BuildDense(true, 1, UINT64_C(0x000000000A000000));
    MopSpellPackets::UseItemRequest request;
    CHECK(Read(packet, request));
    CHECK(packet.rpos() == packet.size());
    CHECK(request.cast.spellId == 0xC1C2C3C4);
}

static void test_binary_derived_boundaries_and_guid_masks()
{
    for (uint32 forceCount = 0; forceCount <= 1; ++forceCount)
    {
        WorldPacket packet = BuildDense(true, forceCount);
        MopSpellPackets::UseItemRequest request;
        CHECK(Read(packet, request));
        CHECK(packet.rpos() == packet.size());
    }

    for (uint8 variant = 0; variant < 2; ++variant)
    {
        WorldPacket packet = BuildDense(true, 1, UINT64_C(0x1817161514131211),
            variant == 0, variant != 0);
        MopSpellPackets::UseItemRequest request;
        CHECK(Read(packet, request));
        CHECK(packet.rpos() == packet.size());
    }

    for (std::string const& targetString : { std::string(), std::string(127, 'X') })
    {
        WorldPacket packet = BuildDense(true, 1, UINT64_C(0x1817161514131211),
            true, true, targetString);
        MopSpellPackets::UseItemRequest request;
        CHECK(Read(packet, request));
        CHECK(packet.rpos() == packet.size());
        CHECK(request.cast.targetString == targetString);
    }

    for (uint8 byte = 0; byte < 8; ++byte)
    {
        uint64 const byteMask = ~(UINT64_C(0xFF) << (byte * 8));
        for (uint8 field = 0; field < 5; ++field)
        {
            DenseGuids values;
            uint64* const fields[] = {
                &values.item, &values.target, &values.itemTarget,
                &values.source, &values.destination
            };
            *fields[field] &= byteMask;

            WorldPacket packet = BuildDense(true, 1, UINT64_C(0x1817161514131211),
                true, true, "Target", values);
            MopSpellPackets::UseItemRequest request;
            CHECK(Read(packet, request));
            CHECK(packet.rpos() == packet.size());
            CHECK(uint64(request.itemGuid) == values.item);
            CHECK(uint64(request.cast.targetGuid) == values.target);
            CHECK(uint64(request.cast.itemTargetGuid) == values.itemTarget);
            CHECK(uint64(request.cast.sourceTransportGuid) == values.source);
            CHECK(uint64(request.cast.destinationTransportGuid) == values.destination);
        }
    }
}

static void test_hostile_force_count_and_guid_zero_contradiction_rejected()
{
    WorldPacket hostile = BuildDense(false, (1u << 22) - 1u);
    MopSpellPackets::UseItemRequest request;
    request.slot = 0xAA;
    CHECK(!Read(hostile, request));
    CHECK(hostile.rpos() == hostile.size());
    CHECK(request.slot == 0);

    WorldPacket contradiction = BuildDense();
    contradiction[4] |= 0x80; // target GUID declared wholly zero while its mask is nonzero
    CHECK(!Read(contradiction, request));
    CHECK(contradiction.rpos() == contradiction.size());
}

static void test_every_truncated_dense_prefix_and_trailing_byte_rejected()
{
    WorldPacket dense = BuildDense();
    std::vector<uint8> const bytes(dense.contents(), dense.contents() + dense.size());

    for (size_t size = 0; size < bytes.size(); ++size)
    {
        WorldPacket truncated(CMSG_USE_ITEM, size);
        if (size)
            truncated.append(bytes.data(), size);
        MopSpellPackets::UseItemRequest request;
        CHECK(!Read(truncated, request));
        CHECK(truncated.rpos() == truncated.size());
    }

    WorldPacket trailing(CMSG_USE_ITEM, bytes.size() + 1);
    trailing.append(bytes.data(), bytes.size());
    trailing << uint8(0xCC);
    MopSpellPackets::UseItemRequest request;
    CHECK(!Read(trailing, request));
    CHECK(trailing.rpos() == trailing.size());
}

int main(int, char**)
{
    test_captured_retail_bodies();
    test_binary_derived_dense_body();
    test_transport_guid_byte_three_is_consumed();
    test_binary_derived_boundaries_and_guid_masks();
    test_hostile_force_count_and_guid_zero_contradiction_rejected();
    test_every_truncated_dense_prefix_and_trailing_byte_rejected();

    if (g_fail)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }
    std::puts("mop_use_item_packets_test: PASS");
    return 0;
}
