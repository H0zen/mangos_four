/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 pet-mode packet fixtures.
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
    CHECK(packet.GetOpcode() == SMSG_PET_MODE);
    CHECK(packet.size() == size);
    if (size != 0 && std::memcmp(packet.contents(), expected, size) != 0)
    {
        std::fprintf(stderr, "actual:");
        for (size_t index = 0; index < packet.size(); ++index)
            std::fprintf(stderr, " %02X", packet.contents()[index]);
        std::fprintf(stderr, "\n");
        CHECK(false);
    }
}

static void test_dense_default_mode()
{
    WorldPacket packet;
    MopPetPackets::BuildMode(packet,
        ObjectGuid(uint64(0x0807060504030201ULL)), uint32(PET_MODE_DEFAULT));

    static uint8 const expected[] = {
        0xFF,
        0x01, 0x01, 0x00, 0x00,
        0x02, 0x07, 0x04, 0x00, 0x03, 0x09, 0x05, 0x06,
    };
    check_packet(packet, expected, sizeof(expected));
}

static void test_sparse_disabled_mode()
{
    WorldPacket packet;
    MopPetPackets::BuildMode(packet,
        ObjectGuid(uint64(0xAA000000000000BBULL)), uint32(PET_MODE_DISABLE_ACTIONS));

    static uint8 const expected[] = {
        0x48,
        0x00, 0x00, 0x00, 0x08,
        0xBA, 0xAB,
    };
    check_packet(packet, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_dense_default_mode();
    test_sparse_disabled_mode();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_pet_mode_packets: all checks passed\n");
    return 0;
}
