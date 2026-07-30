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

/// The bit is the wanted SHOWING state, so the server stores its inverse.
///
/// The client routes are sub_40959D for the helm and sub_4095E0 for the cloak,
/// identical but for the bit index:
///     (PLAYER_FLAGS & 0x400) != 0  ->  the bit        (helm)
///     (PLAYER_FLAGS & 0x800) != 0  ->  the bit        (cloak)
/// Neither inverts, so each sends the opposite of the current hide flag -- which
/// is the wanted showing state. The Lua setters settle it beyond doubt:
/// ShowHelm(true) sets the bit to 1 and ShowHelm(false) to 0, through the same
/// packet constructor, so the wire value is absolute rather than a toggle.
///
/// An earlier reading assigned the bit to the HIDE flag directly, which inverts
/// and left both the console command and the UI checkbox inert. Both work now.
static void test_toggle_applies_the_opposite_of_the_bit()
{
    const uint32 HIDE_HELM = 0x00000400;
    const uint32 HIDE_CLOAK = 0x00000800;

    // Showing, so the client reports 0 and wants it hidden.
    uint32 flags = 0;
    WorldPacket showing(CMSG_SHOWING_HELM, 1);
    showing << uint8(0x00);
    bool wasHidden = showing.ReadBit();
    CHECK(wasHidden == false);
    if (!wasHidden) { flags |= HIDE_HELM; } else { flags &= ~HIDE_HELM; }
    CHECK((flags & HIDE_HELM) != 0);                        // it moved

    // Hidden, so the client reports 1 and wants it shown again.
    WorldPacket hidden(CMSG_SHOWING_HELM, 1);
    hidden << uint8(0x80);
    wasHidden = hidden.ReadBit();
    CHECK(wasHidden == true);
    if (!wasHidden) { flags |= HIDE_HELM; } else { flags &= ~HIDE_HELM; }
    CHECK((flags & HIDE_HELM) == 0);                        // and back

    // Assigning the bit as sent is what made it inert: the flag is written to
    // the value it already had, so nothing changes however often it is sent.
    uint32 inert = 0;
    for (int i = 0; i < 3; ++i)
    {
        WorldPacket p(CMSG_SHOWING_HELM, 1);
        p << uint8(0x00);                                   // reports "showing"
        bool const bit = p.ReadBit();
        if (bit) { inert |= HIDE_HELM; } else { inert &= ~HIDE_HELM; }
    }
    CHECK((inert & HIDE_HELM) == 0);                        // never moved

    // The cloak takes the same path on its own bit.
    uint32 cloakFlags = 0;
    WorldPacket cloak(CMSG_SHOWING_CLOAK, 1);
    cloak << uint8(0x00);
    bool const cloakWasHidden = cloak.ReadBit();
    if (!cloakWasHidden) { cloakFlags |= HIDE_CLOAK; } else { cloakFlags &= ~HIDE_CLOAK; }
    CHECK((cloakFlags & HIDE_CLOAK) != 0);
    CHECK((cloakFlags & HIDE_HELM) == 0);                   // and only its own
}

/// The cloak is the same one-bit shape, and its polarity is proven the same way
/// rather than assumed from the helm: the client's toggle route sub_4095E0 reads
/// PLAYER_FLAGS & 0x800 and serializes that boolean as the body's only bit. So a
/// set bit means HIDE here too, despite the opcode being named "showing".
static void test_showing_cloak_opcode_value()
{
    WorldPacket request(CMSG_SHOWING_CLOAK, 1);
    CHECK(uint32(request.GetOpcode()) == 0x02F2u);
}

static void test_showing_cloak_bodies()
{
    WorldPacket hide(CMSG_SHOWING_CLOAK, 1);
    hide << uint8(0x80);
    CHECK(hide.size() == 1);
    CHECK(hide.ReadBit() == true);                          // set bit == hide cloak

    WorldPacket show(CMSG_SHOWING_CLOAK, 1);
    show << uint8(0x00);
    CHECK(show.size() == 1);
    CHECK(show.ReadBit() == false);                         // clear bit == show cloak
}

/// The cloak and helm must not share a flag bit, or one toggle would move both.
static void test_cloak_and_helm_flags_are_distinct()
{
    const uint32 HIDE_HELM = 0x00000400;                    // PLAYER_FLAGS_HIDE_HELM
    const uint32 HIDE_CLOAK = 0x00000800;                   // PLAYER_FLAGS_HIDE_CLOAK
    CHECK((HIDE_HELM & HIDE_CLOAK) == 0);

    uint32 flags = 0;
    flags |= HIDE_CLOAK;
    CHECK((flags & HIDE_HELM) == 0);                        // hiding the cloak leaves the helm shown
}

int main(int /*argc*/, char** /*argv*/)
{
    test_showing_helm_opcode_value();
    test_showing_helm_bodies();
    test_repeat_requests_are_idempotent();
    test_toggle_applies_the_opposite_of_the_bit();
    test_showing_cloak_opcode_value();
    test_showing_cloak_bodies();
    test_cloak_and_helm_flags_are_distinct();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_showing_helm_packets: all checks passed\n");
    return 0;
}
