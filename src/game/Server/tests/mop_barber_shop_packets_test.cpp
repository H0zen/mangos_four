/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 barber-shop opener packet fixture.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_barber_shop_open_packet_is_empty()
{
    WorldPacket packet(SMSG_ENABLE_BARBER_SHOP, 0);
    CHECK(uint32(packet.GetOpcode()) == 0x1222u);
    CHECK(packet.empty());
}

static void test_alter_appearance_request_uses_18414_field_order()
{
    WorldPacket packet(CMSG_ALTER_APPEARANCE, 16);
    packet << uint32(0x11223344u); // skin style ID
    packet << uint32(0x55667788u); // color value
    packet << uint32(0x99AABBCCu); // hair style ID
    packet << uint32(0xDDEEFF00u); // facial-hair style ID

    static uint8 const expected[] = {
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0xCC, 0xBB, 0xAA, 0x99,
        0x00, 0xFF, 0xEE, 0xDD,
    };
    CHECK(uint32(packet.GetOpcode()) == 0x07F0u);
    CHECK(packet.size() == sizeof(expected));
    CHECK(std::memcmp(packet.contents(), expected, sizeof(expected)) == 0);
}

static void test_barber_shop_result_is_one_uint32()
{
    for (uint32 result : { 0u, 1u, 2u, 3u })
    {
        WorldPacket packet(SMSG_BARBER_SHOP_RESULT, 4);
        packet << result;
        CHECK(uint32(packet.GetOpcode()) == 0x0C3Fu);
        CHECK(packet.size() == 4u);
        CHECK(packet.contents()[0] == uint8(result));
        CHECK(packet.contents()[1] == 0u);
        CHECK(packet.contents()[2] == 0u);
        CHECK(packet.contents()[3] == 0u);
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    test_barber_shop_open_packet_is_empty();
    test_alter_appearance_request_uses_18414_field_order();
    test_barber_shop_result_is_one_uint32();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_barber_shop_packets: all checks passed\n");
    return 0;
}
