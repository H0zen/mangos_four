/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 SMSG_CONTACT_LIST body fixtures.
 *
 * This packet is byte-aligned throughout -- it carries no bit-packing. The
 * client's reader is sub_A6AAB5 (asserts "FriendList.cpp") and takes, per entry:
 *
 *     uint64  guid                raw LE, not packed, not XOR'd
 *     uint32  virtualRealm
 *     uint32  nativeRealm
 *     uint32  typeFlags           1 friend, 2 ignored, 4 muted
 *     cstring note                NUL-terminated
 *     if (typeFlags & 1) {
 *         uint8 status            0 = offline
 *         if (status) { uint32 areaId; uint32 level; uint32 classId; }
 *     }
 *
 * The core wrote no counterpart for either realm address until this was fixed,
 * so the client lost alignment immediately after the GUID: the first realm
 * address landed where the type flags are read, the second began the note, and
 * the reader ran on into the following entry.
 *
 * The trap that let this survive is that the EMPTY case agrees byte for byte.
 * Retail's empty list is 07 00 00 00 00 00 00 00, exactly the uint32(7) +
 * uint32(0) header the old code wrote, so any check that stopped at "sizes and
 * header match" passed. Only a populated list disagrees -- hence this fixture.
 */

#include "SocialMgr.h"
#include "WorldPacket.h"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(ByteBuffer const& got, uint8 const* want, size_t n)
{
    if (got.size() != n)
    {
        std::fprintf(stderr, "  size %u, expected %u\n", unsigned(got.size()), unsigned(n));
        return false;
    }
    for (size_t i = 0; i < n; ++i)
    {
        if (got.contents()[i] != want[i])
        {
            std::fprintf(stderr, "  byte %u: got %02X want %02X\n",
                         unsigned(i), got.contents()[i], want[i]);
            return false;
        }
    }
    return true;
}

/// A real 18414 two-entry list, recorded beside the CMSG_CONTACT_LIST note in
/// Opcodes.cpp. Both contacts are on the viewer's realm, so both carry the same
/// pair of addresses. Neither has the friend bit, so neither has a status byte --
/// which is why each entry is 21 bytes and not 22.
static uint8 const kRetailTwoEntry[] =
{
    0x07, 0x00, 0x00, 0x00,                                 // listFlags 7
    0x02, 0x00, 0x00, 0x00,                                 // count 2

    0x68, 0xD1, 0x19, 0x07, 0x00, 0x00, 0x00, 0x06,         // guid
    0x19, 0x00, 0x01, 0x03,                                 // virtualRealm 0x03010019
    0x16, 0x00, 0x06, 0x03,                                 // nativeRealm  0x03060016
    0x04, 0x00, 0x00, 0x00,                                 // typeFlags 4 (muted)
    0x00,                                                   // note ""

    0xE5, 0xFE, 0x23, 0x07, 0x00, 0x00, 0x00, 0x06,         // guid
    0x19, 0x00, 0x01, 0x03,                                 // virtualRealm
    0x16, 0x00, 0x06, 0x03,                                 // nativeRealm
    0x02, 0x00, 0x00, 0x00,                                 // typeFlags 2 (ignored)
    0x00                                                    // note ""
};

static void test_retail_two_entry_list()
{
    MopSocialPackets::ContactEntry a;
    a.guid = UINT64_C(0x060000000719D168);
    a.virtualRealm = 0x03010019;
    a.nativeRealm = 0x03060016;
    a.typeFlags = SOCIAL_FLAG_MUTED;

    MopSocialPackets::ContactEntry b;
    b.guid = UINT64_C(0x060000000723FEE5);
    b.virtualRealm = 0x03010019;
    b.nativeRealm = 0x03060016;
    b.typeFlags = SOCIAL_FLAG_IGNORED;

    ByteBuffer out;
    MopSocialPackets::BuildContactList(out, 7, { a, b });
    CHECK(Equal(out, kRetailTwoEntry, sizeof(kRetailTwoEntry)));
    CHECK(out.size() == 50);
}

/// The empty list is the case that hid the defect: it is identical with or
/// without the fix, so it can never be the only thing checked.
static void test_empty_list_is_header_only()
{
    ByteBuffer out;
    MopSocialPackets::BuildContactList(out, 7, {});
    static uint8 const want[] = { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    CHECK(Equal(out, want, sizeof(want)));
}

/// Entry widths, which are what the missing fields actually changed. An
/// ignore-only entry with an empty note is 21 bytes; the friend bit adds a status
/// byte, and an online friend adds the area/level/class triple.
static void test_entry_widths()
{
    MopSocialPackets::ContactEntry e;
    e.guid = UINT64_C(0x060000000719D168);
    e.virtualRealm = 1;
    e.nativeRealm = 1;

    e.typeFlags = SOCIAL_FLAG_IGNORED;
    ByteBuffer ignored;
    MopSocialPackets::BuildContactList(ignored, 7, { e });
    CHECK(ignored.size() == 8 + 21);

    e.typeFlags = SOCIAL_FLAG_FRIEND;
    e.status = 0;                                           // offline
    ByteBuffer offline;
    MopSocialPackets::BuildContactList(offline, 7, { e });
    CHECK(offline.size() == 8 + 22);

    e.status = 1;                                           // online
    e.areaId = 1519;
    e.level = 90;
    e.classId = 8;
    ByteBuffer online;
    MopSocialPackets::BuildContactList(online, 7, { e });
    CHECK(online.size() == 8 + 34);

    // A note is written NUL-terminated, so it costs its length plus one.
    e.note = "alt";
    ByteBuffer noted;
    MopSocialPackets::BuildContactList(noted, 7, { e });
    CHECK(noted.size() == 8 + 34 + 3);
}

/// The realm addresses must sit BETWEEN the guid and the type flags. Placing them
/// anywhere else still produces a plausible length, so assert the position.
static void test_realm_addresses_precede_type_flags()
{
    MopSocialPackets::ContactEntry e;
    e.guid = 0;
    e.virtualRealm = 0xAABBCCDD;
    e.nativeRealm = 0x11223344;
    e.typeFlags = SOCIAL_FLAG_IGNORED;

    ByteBuffer out;
    MopSocialPackets::BuildContactList(out, 7, { e });
    uint8 const* p = out.contents() + 8 + 8;                 // header + guid
    CHECK(p[0] == 0xDD && p[1] == 0xCC && p[2] == 0xBB && p[3] == 0xAA);
    CHECK(p[4] == 0x44 && p[5] == 0x33 && p[6] == 0x22 && p[7] == 0x11);
    CHECK(p[8] == 0x02 && p[9] == 0x00 && p[10] == 0x00 && p[11] == 0x00);
}

int main()
{
    test_retail_two_entry_list();
    test_empty_list_is_header_only();
    test_entry_widths();
    test_realm_addresses_precede_type_flags();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }
    std::printf("mop_contact_list_packets: all checks passed\n");
    return 0;
}
