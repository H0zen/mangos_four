/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 helm- and cloak-visibility request fixtures.
 *
 * CMSG_SHOWING_HELM and CMSG_SHOWING_CLOAK each carry one MSB-first bit, and the
 * opcode names are accurate: the bit is the wanted SHOWING state. 0x80 show,
 * 0x00 hide.
 *
 * Two client callers prove it, and only the second settles it:
 *
 *   The Lua setter ShowHelm(v) sends an ABSOLUTE value -- its true branch
 *   serializes 1 and its false branch 0, both through the same packet
 *   constructor. This is the UI checkbox's route. A toggle reading is
 *   impossible here.
 *
 *   The console command togglehelm (sub_40959D) reads PLAYER_FLAGS & 0x400
 *   (PLAYER_FLAGS_HIDE_HELM) and serializes that boolean unmodified. It lands on
 *   the same wire meaning by a different argument: hidden now, so show it.
 *
 * The server's own flag is HIDE, so it must store the INVERSE of the bit. Two
 * earlier readings got this wrong in opposite directions -- one toggled on
 * receipt, which desynchronised permanently the first time client and server
 * disagreed; one assigned the bit straight to the HIDE flag, which writes the
 * flag to the value it already had and left both the console command and the UI
 * checkbox visibly inert.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static uint32 const HIDE_HELM = 0x00000400;                 // PLAYER_FLAGS_HIDE_HELM
static uint32 const HIDE_CLOAK = 0x00000800;                // PLAYER_FLAGS_HIDE_CLOAK

/// The production rule, stated once. The handler in CharacterHandler.cpp must
/// agree with this; mop_showing_helm_source enforces that it does.
static uint32 ApplyShowing(uint32 flags, uint32 hideBit, bool showing)
{
    if (!showing) { return flags | hideBit; }
    return flags & ~hideBit;
}

static void test_opcode_values()
{
    WorldPacket helm(CMSG_SHOWING_HELM, 1);
    CHECK(uint32(helm.GetOpcode()) == 0x126Bu);

    WorldPacket cloak(CMSG_SHOWING_CLOAK, 1);
    CHECK(uint32(cloak.GetOpcode()) == 0x02F2u);
}

/// One bit, MSB-first, in a one-byte body -- not a uint8. A reader switching on
/// 0 and 1 would match neither value the client actually sends.
static void test_bodies_are_one_msb_first_bit()
{
    WorldPacket show(CMSG_SHOWING_HELM, 1);
    show << uint8(0x80);
    CHECK(show.size() == 1);
    CHECK(show.ReadBit() == true);                          // set bit == show helm

    WorldPacket hide(CMSG_SHOWING_HELM, 1);
    hide << uint8(0x00);
    CHECK(hide.size() == 1);
    CHECK(hide.ReadBit() == false);                         // clear bit == hide helm

    WorldPacket showCloak(CMSG_SHOWING_CLOAK, 1);
    showCloak << uint8(0x80);
    CHECK(showCloak.size() == 1);
    CHECK(showCloak.ReadBit() == true);

    WorldPacket hideCloak(CMSG_SHOWING_CLOAK, 1);
    hideCloak << uint8(0x00);
    CHECK(hideCloak.size() == 1);
    CHECK(hideCloak.ReadBit() == false);
}

/// The server flag is HIDE and the wire carries SHOW, so the stored value is the
/// bit's inverse. Assigning it directly is the defect that made both toggles
/// inert, and it is checked here explicitly so the two cannot be confused again.
static void test_stored_flag_is_the_inverse_of_the_bit()
{
    // "Show it" clears HIDE.
    WorldPacket show(CMSG_SHOWING_HELM, 1);
    show << uint8(0x80);
    CHECK(ApplyShowing(HIDE_HELM, HIDE_HELM, show.ReadBit()) == 0u);

    // "Hide it" sets HIDE.
    WorldPacket hide(CMSG_SHOWING_HELM, 1);
    hide << uint8(0x00);
    CHECK(ApplyShowing(0, HIDE_HELM, hide.ReadBit()) == HIDE_HELM);

    // Assigning the bit as sent -- the defect. From "hidden", the console
    // command sends 1 (show it); assignment would write HIDE = 1, i.e. leave it
    // hidden, which is exactly the reported symptom.
    uint32 assigned = HIDE_HELM;
    for (int i = 0; i < 3; ++i)
    {
        WorldPacket p(CMSG_SHOWING_HELM, 1);
        p << uint8(0x80);                                   // "show it"
        if (p.ReadBit()) { assigned |= HIDE_HELM; } else { assigned &= ~HIDE_HELM; }
    }
    CHECK((assigned & HIDE_HELM) != 0);                     // never moved: the bug
    CHECK(ApplyShowing(HIDE_HELM, HIDE_HELM, true) == 0u);  // the fix does move it
}

/// Assignment is idempotent; toggling is not. Repeating a request must converge,
/// because the client states its own intent rather than a delta.
static void test_repeat_requests_are_idempotent()
{
    uint32 flags = HIDE_HELM;
    for (int i = 0; i < 4; ++i)
    {
        WorldPacket p(CMSG_SHOWING_HELM, 1);
        p << uint8(0x80);                                   // "show it", four times
        flags = ApplyShowing(flags, HIDE_HELM, p.ReadBit());
    }
    CHECK((flags & HIDE_HELM) == 0);                        // shown, not flipped back

    uint32 toggled = 0;
    for (int i = 0; i < 2; ++i) { toggled ^= HIDE_HELM; }   // the inherited behaviour
    CHECK((toggled & HIDE_HELM) == 0);
    toggled ^= HIDE_HELM;
    CHECK((toggled & HIDE_HELM) != 0);                      // odd count desyncs for good
}

/// The console command's derivation: it sends the opposite of the current hide
/// flag, so applying the inverse reproduces a true toggle without the server ever
/// implementing one. Both directions, from both starting states.
static void test_console_toggle_round_trips()
{
    uint32 flags = 0;                                       // showing
    for (int i = 0; i < 2; ++i)
    {
        bool const wireBit = (flags & HIDE_HELM) != 0;      // sub_40959D
        WorldPacket p(CMSG_SHOWING_HELM, 1);
        p << uint8(wireBit ? 0x80 : 0x00);
        flags = ApplyShowing(flags, HIDE_HELM, p.ReadBit());

        CHECK((flags & HIDE_HELM) != 0);                    // now hidden

        bool const back = (flags & HIDE_HELM) != 0;
        WorldPacket q(CMSG_SHOWING_HELM, 1);
        q << uint8(back ? 0x80 : 0x00);
        flags = ApplyShowing(flags, HIDE_HELM, q.ReadBit());
        CHECK((flags & HIDE_HELM) == 0);                    // and shown again
    }
}

/// The cloak takes the identical path on its own bit, established the same way
/// rather than assumed by symmetry: ShowCloak(v) sends the absolute value through
/// the same constructor, and togglecloak (sub_4095E0) reads PLAYER_FLAGS & 0x800.
static void test_cloak_uses_its_own_bit_only()
{
    WorldPacket cloak(CMSG_SHOWING_CLOAK, 1);
    cloak << uint8(0x00);                                   // hide the cloak
    uint32 const flags = ApplyShowing(0, HIDE_CLOAK, cloak.ReadBit());

    CHECK((flags & HIDE_CLOAK) != 0);
    CHECK((flags & HIDE_HELM) == 0);                        // and only its own
    CHECK((HIDE_HELM & HIDE_CLOAK) == 0);                   // the bits are distinct
}

int main(int /*argc*/, char** /*argv*/)
{
    test_opcode_values();
    test_bodies_are_one_msb_first_bit();
    test_stored_flag_is_the_inverse_of_the_bit();
    test_repeat_requests_are_idempotent();
    test_console_toggle_round_trips();
    test_cloak_uses_its_own_bit_only();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_showing_helm_packets: all checks passed\n");
    return 0;
}
