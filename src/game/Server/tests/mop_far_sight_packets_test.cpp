/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 far-sight request fixtures.
 *
 * CMSG_FAR_SIGHT carries a single MSB-first bit, not a uint8 boolean. Every
 * sampled 18414 body is 0x80 (enable) or 0x00 (disable), so a handler reading
 * `uint8 op` and switching on 0/1 never matches an enable. These fixtures pin
 * the encoding so that regression cannot return silently.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_far_sight_opcode_value()
{
    WorldPacket request(CMSG_FAR_SIGHT, 1);
    CHECK(uint32(request.GetOpcode()) == 0x1341u);
}

/// 0x80 is the enable body: one set bit in the most significant position.
static void test_far_sight_enable_body_is_a_leading_bit()
{
    WorldPacket enable(CMSG_FAR_SIGHT, 1);
    enable << uint8(0x80);

    CHECK(enable.size() == 1);
    CHECK(enable.ReadBit() == true);
}

/// 0x00 is the disable body.
static void test_far_sight_disable_body_is_a_clear_bit()
{
    WorldPacket disable(CMSG_FAR_SIGHT, 1);
    disable << uint8(0x00);

    CHECK(disable.size() == 1);
    CHECK(disable.ReadBit() == false);
}

/// The inherited reader consumed the byte as a scalar, which is why a real
/// enable was dropped: 0x80 is 128, and the old switch had no case for it.
static void test_inherited_scalar_read_would_miss_the_enable()
{
    WorldPacket enable(CMSG_FAR_SIGHT, 1);
    enable << uint8(0x80);

    uint8 op = 0;
    enable >> op;

    CHECK(op == 0x80);
    CHECK(op != 1);                                         // never matched "add far sight"
    CHECK(op != 0);                                         // never matched "remove far sight"
}

int main(int /*argc*/, char** /*argv*/)
{
    test_far_sight_opcode_value();
    test_far_sight_enable_body_is_a_leading_bit();
    test_far_sight_disable_body_is_a_clear_bit();
    test_inherited_scalar_read_would_miss_the_enable();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_far_sight_packets: all checks passed\n");
    return 0;
}
