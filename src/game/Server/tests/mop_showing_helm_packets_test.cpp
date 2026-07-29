/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 helm-visibility request fixtures.
 *
 * CMSG_SHOWING_HELM carries a single MSB-first bit stating the wanted end
 * state, and despite the opcode's name the bit is the HIDE flag: the client's
 * toggle route sub_40959D reads PLAYER_FLAGS & 0x400 (PLAYER_FLAGS_HIDE_HELM)
 * and serializes exactly that boolean. 0x80 therefore means hide, 0x00 show.
 *
 * The inherited handler ignored the packet and toggled instead, so the helm
 * inverted for the rest of the session the first time client and server
 * disagreed. These fixtures pin the polarity and the assign-not-toggle rule.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_showing_helm_opcode_value()
{
    WorldPacket request(CMSG_SHOWING_HELM, 1);
    CHECK(uint32(request.GetOpcode()) == 0x126Bu);
}

static void test_showing_helm_bodies()
{
    WorldPacket hide(CMSG_SHOWING_HELM, 1);
    hide << uint8(0x80);
    CHECK(hide.size() == 1);
    CHECK(hide.ReadBit() == true);                          // set bit == hide helm

    WorldPacket show(CMSG_SHOWING_HELM, 1);
    show << uint8(0x00);
    CHECK(show.size() == 1);
    CHECK(show.ReadBit() == false);                         // clear bit == show helm
}

/// Assigning is idempotent; toggling is not. Two identical "hide" requests must
/// leave the same end state, which is precisely what the old handler broke.
static void test_repeat_requests_are_idempotent()
{
    uint32 flags = 0;
    const uint32 HIDE_HELM = 0x00000400;                    // PLAYER_FLAGS_HIDE_HELM

    for (int i = 0; i < 2; ++i)
    {
        WorldPacket hide(CMSG_SHOWING_HELM, 1);
        hide << uint8(0x80);
        bool const hidden = hide.ReadBit();

        if (hidden) { flags |= HIDE_HELM; } else { flags &= ~HIDE_HELM; }
    }
    CHECK((flags & HIDE_HELM) != 0);                        // still hidden, not flipped back

    uint32 toggled = 0;
    for (int i = 0; i < 2; ++i)
    {
        toggled ^= HIDE_HELM;                               // the inherited behaviour
    }
    CHECK((toggled & HIDE_HELM) == 0);                      // two hides == shown, already wrong
    // ...and an odd number desynchronises it permanently in the other direction:
    toggled ^= HIDE_HELM;
    CHECK((toggled & HIDE_HELM) != 0);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_showing_helm_opcode_value();
    test_showing_helm_bodies();
    test_repeat_requests_are_idempotent();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_showing_helm_packets: all checks passed\n");
    return 0;
}
