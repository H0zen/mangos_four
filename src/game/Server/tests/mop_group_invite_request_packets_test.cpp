/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Independent byte fixtures for the 5.4.8.18414 group-invite request,
 * CMSG_GROUP_INVITE 0x072D.
 *
 * The grammar is taken from client writer sub_66CBDC
 * (Wow.exe.c:883281-883356) and cross-checked against real retail bodies:
 * each one below consumes to exactly its own end, which is what pins the
 * field order rather than merely the field sizes.
 */

#include "Group.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>
#include <string>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void Append(WorldPacket& packet, std::initializer_list<uint8> bytes)
{
    if (bytes.size())
    {
        packet.append(bytes.begin(), bytes.size());
    }
}

static void CheckSuccess(std::initializer_list<uint8> bytes,
    uint32 hint, uint32 roles, char const* realm, char const* target,
    uint64 guid)
{
    WorldPacket packet(CMSG_GROUP_INVITE, bytes.size());
    Append(packet, bytes);

    MopGroupInvitePackets::Request request;
    CHECK(MopGroupInvitePackets::ParseRequest(packet, request));
    CHECK(packet.rpos() == packet.size());
    CHECK(request.realmSelectorHint == hint);
    CHECK(request.roleMask == roles);
    CHECK(request.realmName == std::string(realm));
    CHECK(request.targetName == std::string(target));
    CHECK(request.targetGuid.GetRawValue() == guid);
}

static void CheckFailure(std::initializer_list<uint8> bytes,
    size_t initialReadPosition = 0)
{
    WorldPacket packet(CMSG_GROUP_INVITE, bytes.size());
    Append(packet, bytes);
    packet.rpos(initialReadPosition);

    MopGroupInvitePackets::Request request;
    request.targetName = "untouched";
    request.realmName = "untouched";

    CHECK(!MopGroupInvitePackets::ParseRequest(packet, request));
    // Fail closed: the body is consumed and the caller's value is unchanged,
    // so a rejected invite can neither be re-read nor half-applied.
    CHECK(packet.rpos() == packet.size());
    CHECK(request.targetName == "untouched");
    CHECK(request.realmName == "untouched");
}

static void test_retail_bodies()
{
    // Ordinary Lua InviteUnit path: zero hint, zero GUID, no separate realm,
    // bare local target. This is the body the legacy reader could not decode.
    CheckSuccess({ 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x70, 0x00,
                   'M', 'o', 'r', 'g', 'e', 'n', 'n' },
        0, 0, "", "Morgenn", 0x0000000000000000ULL);

    // Nonzero selector, five present GUID bytes and a separate realm string.
    // Exercises both interleaved GUID runs and both strings.
    CheckSuccess({ 0x0B, 0x02, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x83, 0x60, 0x89, 0x80,
                   0x05, 0xE9,
                   'B', 'u', 'r', 'n', 'i', 'n', 'g', ' ', 'B', 'l', 'a', 'd', 'e',
                   0xC9, 0x3D, 0x04,
                   'J', 'a', 'z', 'h', 'a', 'r', 'k', 'a' },
        523, 0, "Burning Blade", "Jazharka", 0x04000000053CC8E8ULL);

    // Adjacent target-length variant; nine-byte name proves the 9-bit length
    // rather than the legacy 10-bit read, which would shift every later field.
    CheckSuccess({ 0x0B, 0x02, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x83, 0x60, 0x99, 0x80,
                   0x05, 0xD5,
                   'B', 'u', 'r', 'n', 'i', 'n', 'g', ' ', 'B', 'l', 'a', 'd', 'e',
                   0x4D, 0xFF, 0x04,
                   'M', 'i', 'r', 'a', 'j', 'a', 'n', 'n', 'e' },
        523, 0, "Burning Blade", "Mirajanne", 0x0400000005FE4CD4ULL);
}

static void test_role_mask_is_carried()
{
    // Lua maps optional arguments 2/3/4 to role bits 0x02, 0x04 and 0x08.
    CheckSuccess({ 0x00, 0x00, 0x00, 0x00, 0x7F, 0x0E, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x70, 0x00,
                   'M', 'o', 'r', 'g', 'e', 'n', 'n' },
        0, 0x0E, "", "Morgenn", 0x0000000000000000ULL);
}

static void test_malformed_bodies_fail_atomically()
{
    CheckFailure({});
    // Shorter than the fixed head plus packed header.
    CheckFailure({ 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x70 });

    // Wrong marker.
    CheckFailure({ 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x70, 0x00,
                   'M', 'o', 'r', 'g', 'e', 'n', 'n' });

    // Nonzero padding in the six trailing header bits.
    CheckFailure({ 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x70, 0x01,
                   'M', 'o', 'r', 'g', 'e', 'n', 'n' });

    // Declared target length exceeds the bytes actually present.
    CheckFailure({ 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x80, 0x00,
                   'M', 'o', 'r', 'g', 'e', 'n', 'n' });

    // Trailing byte past the declared strings.
    CheckFailure({ 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x70, 0x00,
                   'M', 'o', 'r', 'g', 'e', 'n', 'n', 0x00 });

    // Present GUID byte encoded as a raw one, which would decode to zero and
    // contradict its own presence bit.
    CheckFailure({ 0x0B, 0x02, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x83, 0x60, 0x89, 0x80,
                   0x01, 0xE9,
                   'B', 'u', 'r', 'n', 'i', 'n', 'g', ' ', 'B', 'l', 'a', 'd', 'e',
                   0xC9, 0x3D, 0x04,
                   'J', 'a', 'z', 'h', 'a', 'r', 'k', 'a' });

    // Embedded NUL in the target name.
    CheckFailure({ 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x70, 0x00,
                   'M', 'o', 'r', 0x00, 'e', 'n', 'n' });

    // A non-zero starting read position must be rejected rather than
    // silently parsed from the middle of a body.
    CheckFailure({ 0xA5, 0x00, 0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00,
                   0x00, 0x00, 0x00, 0x70, 0x00,
                   'M', 'o', 'r', 'g', 'e', 'n', 'n' }, 1);
}

int main(int, char**)
{
    test_retail_bodies();
    test_role_mask_is_carried();
    test_malformed_bodies_fail_atomically();
    if (g_fail)
    {
        return 1;
    }
    std::printf("mop_group_invite_request_packets: all checks passed\n");
    return 0;
}
