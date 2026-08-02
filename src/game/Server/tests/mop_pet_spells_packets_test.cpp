/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 initial pet-spell snapshot packet fixtures.
 */

struct ItemPrototype;
#include "Pet.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void check_packet(WorldPacket const& packet, uint8 const* expected,
    size_t size)
{
    CHECK(packet.GetOpcode() == SMSG_PET_SPELLS);
    CHECK(packet.size() == size);
    if (packet.size() == size && size != 0 &&
            std::memcmp(packet.contents(), expected, size) != 0)
    {
        std::fprintf(stderr, "actual:");
        for (size_t index = 0; index < packet.size(); ++index)
            std::fprintf(stderr, " %02X", packet.contents()[index]);
        std::fprintf(stderr, "\n");
        CHECK(false);
    }
}

static void test_dense_snapshot()
{
    MopPetPackets::SpellSnapshot snapshot;
    snapshot.guid = ObjectGuid(uint64(0x0807060504030201ULL));
    snapshot.actionBar = {{
        0x01020304u, 0x11121314u, 0x21222324u, 0x31323334u,
        0x41424344u, 0x51525354u, 0x61626364u, 0x71727374u,
        0x81828384u, 0x91929394u
    }};
    snapshot.spells = {0xA1A2A3A4u, 0xB1B2B3B4u};
    snapshot.cooldowns = {
        {0x11223344u, 0x5566u, 0x778899AAu, 0xBBCCDDEEu},
        {0x10203040u, 0x5060u, 0x708090A0u, 0xB0C0D0E0u}
    };
    snapshot.family = 0x1234u;
    snapshot.specialization = 0x5678u;
    snapshot.duration = 0x90ABCDEFu;
    snapshot.mode = 0x13579BDFu;

    WorldPacket packet;
    CHECK(MopPetPackets::BuildSpellSnapshot(packet, snapshot));

    static uint8 const expected[] = {
        0xC0, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0xBE,
        0x04, 0x03, 0x02, 0x01, 0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21, 0x34, 0x33, 0x32, 0x31,
        0x44, 0x43, 0x42, 0x41, 0x54, 0x53, 0x52, 0x51,
        0x64, 0x63, 0x62, 0x61, 0x74, 0x73, 0x72, 0x71,
        0x84, 0x83, 0x82, 0x81, 0x94, 0x93, 0x92, 0x91,
        0xEE, 0xDD, 0xCC, 0xBB, 0x44, 0x33, 0x22, 0x11,
        0x66, 0x55, 0xAA, 0x99, 0x88, 0x77,
        0xE0, 0xD0, 0xC0, 0xB0, 0x40, 0x30, 0x20, 0x10,
        0x60, 0x50, 0xA0, 0x90, 0x80, 0x70,
        0x02,
        0xA4, 0xA3, 0xA2, 0xA1, 0xB4, 0xB3, 0xB2, 0xB1,
        0x09, 0x00, 0x05,
        0x34, 0x12, 0x78, 0x56,
        0x03, 0x04, 0x06,
        0xEF, 0xCD, 0xAB, 0x90,
        0x07,
        0xDF, 0x9B, 0x57, 0x13
    };
    check_packet(packet, expected, sizeof(expected));
}

static void test_empty_snapshot_clears_client_state()
{
    MopPetPackets::SpellSnapshot snapshot;
    WorldPacket packet;
    CHECK(MopPetPackets::BuildSpellSnapshot(packet, snapshot));

    uint8 expected[61] = {};
    check_packet(packet, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_dense_snapshot();
    test_empty_snapshot_clears_client_state();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_pet_spells_packets: all checks passed\n");
    return 0;
}
