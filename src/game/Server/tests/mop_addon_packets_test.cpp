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
 * Tests for the SMSG_ADDON_INFO public-key scatter order.
 *
 * The 18414 client does not read the addon public key in modulus order. Its generated
 * parser stores wire byte i at key[kAddonKeyWireOrder[i]]. Getting this wrong is silent:
 * the client simply ends up with a different modulus, signature verification fails, and
 * every "## Secure:" Blizzard addon loads untrusted with no error anywhere in the log.
 * These tests exist because that failure mode has no other alarm.
 */

#include "WorldSession.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

/**
 * The table must be a bijection on 0..255. If it is not, some modulus byte is dropped and
 * another duplicated, and the client can never reconstruct the key no matter what we send.
 */
static void TestScatterOrderIsAPermutation()
{
    std::set<uint8> seen;
    for (uint32 i = 0; i < 256; ++i)
    {
        seen.insert(MopAddonPackets::kAddonKeyWireOrder[i]);
    }

    CHECK(seen.size() == 256);
    CHECK(*seen.begin() == 0);
    CHECK(*seen.rbegin() == 255);
}

/**
 * Anchors against the client. These specific pairs were read out of the client's parser and
 * then independently confirmed against the 44 .pub files it wrote back to disk, so they are
 * the one part of the table that is externally pinned. A round-trip test alone would happily
 * accept a differently-scrambled but still bijective table; these pairs would not.
 */
static void TestScatterOrderMatchesTheClient()
{
    static uint8 const head[8] = { 5, 176, 148, 43, 28, 135, 64, 8 };
    static uint8 const tail[8] = { 44, 86, 195, 174, 87, 105, 51, 221 };

    for (uint32 i = 0; i < 8; ++i)
    {
        CHECK(MopAddonPackets::kAddonKeyWireOrder[i] == head[i]);
        CHECK(MopAddonPackets::kAddonKeyWireOrder[248 + i] == tail[i]);
    }
}

/**
 * Pin all 256 entries, not just the anchors above.
 *
 * The other tests are self-consistent: swapping two middle entries preserves the bijection,
 * the fixed-point count, the head/tail anchors, the identity comparison and the reconstruction
 * round trip -- and still ships a corrupt modulus, silently, because nothing on the client
 * logs a bad key. This digest is the only check that constrains the interior of the table.
 *
 * FNV-1a/32 over the 256 entries in order. The value comes from the table as recovered from
 * the client parser and confirmed against the 44 .pub files the client wrote back.
 */
static void TestScatterOrderDigest()
{
    uint32 hash = 0x811C9DC5u;
    for (uint32 i = 0; i < 256; ++i)
    {
        hash ^= MopAddonPackets::kAddonKeyWireOrder[i];
        hash *= 0x01000193u;
    }

    CHECK(hash == 0x11A63A0Fu);
    if (hash != 0x11A63A0Fu)
    {
        std::fprintf(stderr, "  table digest 0x%08X, wanted 0x11A63A0F\n", hash);
    }
}

/**
 * Emitting an identity key makes the wire bytes equal the table itself, which pins the
 * direction of the mapping. Sending key[i] instead of key[kAddonKeyWireOrder[i]] -- the
 * bug this replaced -- fails here immediately.
 */
static void TestIdentityKeyEmitsTheTable()
{
    uint8 key[256];
    for (uint32 i = 0; i < 256; ++i)
    {
        key[i] = uint8(i);
    }

    WorldPacket packet(SMSG_ADDON_INFO, 256);
    MopAddonPackets::AppendAddonPublicKey(packet, key);

    CHECK(packet.size() == 256);
    if (packet.size() != 256)
    {
        return;
    }

    for (uint32 i = 0; i < 256; ++i)
    {
        CHECK(packet.contents()[i] == MopAddonPackets::kAddonKeyWireOrder[i]);
    }
}

/**
 * Replay what the client does: scatter each wire byte to its destination and check the
 * modulus comes back out intact. This is the property the whole change exists to hold.
 */
static void TestClientReconstructsTheModulus()
{
    uint8 key[256];
    for (uint32 i = 0; i < 256; ++i)
    {
        key[i] = uint8((i * 7u + 13u) & 0xFF);       // arbitrary, just not the identity
    }

    WorldPacket packet(SMSG_ADDON_INFO, 256);
    MopAddonPackets::AppendAddonPublicKey(packet, key);

    CHECK(packet.size() == 256);
    if (packet.size() != 256)
    {
        return;
    }

    uint8 reconstructed[256] = { 0 };
    for (uint32 i = 0; i < 256; ++i)
    {
        reconstructed[MopAddonPackets::kAddonKeyWireOrder[i]] = packet.contents()[i];
    }

    for (uint32 i = 0; i < 256; ++i)
    {
        CHECK(reconstructed[i] == key[i]);
    }
}

/**
 * Guard the regression directly: a raw append is wrong at 254 of 256 positions against the
 * real modulus. Only the two fixed points of the permutation coincide.
 */
static void TestRawOrderIsWrongAtAlmostEveryPosition()
{
    uint32 fixedPoints = 0;
    for (uint32 i = 0; i < 256; ++i)
    {
        if (MopAddonPackets::kAddonKeyWireOrder[i] == i)
        {
            ++fixedPoints;
        }
    }

    CHECK(fixedPoints == 2);
}

int main()
{
    TestScatterOrderIsAPermutation();
    TestScatterOrderMatchesTheClient();
    TestScatterOrderDigest();
    TestIdentityKeyEmitsTheTable();
    TestClientReconstructsTheModulus();
    TestRawOrderIsWrongAtAlmostEveryPosition();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_addon_packets_test: all checks passed\n");
    return 0;
}
