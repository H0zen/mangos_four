/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Independent byte fixtures for the 5.4.8.18414 group-invite response.
 */

#include "Group.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void Append(WorldPacket& packet, std::initializer_list<uint8> bytes)
{
    if (bytes.size())
        packet.append(bytes.begin(), bytes.size());
}

static void CheckSuccess(std::initializer_list<uint8> bytes,
    bool hasRoles, bool accepted, uint32 roles)
{
    WorldPacket packet(CMSG_GROUP_INVITE_RESPONSE, bytes.size());
    Append(packet, bytes);

    MopGroupInvitePackets::Response response;
    response.hasRoles = !hasRoles;
    response.accepted = !accepted;
    response.roles = 0xDEADBEEF;

    CHECK(MopGroupInvitePackets::ParseResponse(packet, response));
    CHECK(packet.rpos() == packet.size());
    CHECK(response.hasRoles == hasRoles);
    CHECK(response.accepted == accepted);
    CHECK(response.roles == roles);
}

static void CheckFailure(std::initializer_list<uint8> bytes,
    size_t initialReadPosition = 0)
{
    WorldPacket packet(CMSG_GROUP_INVITE_RESPONSE, bytes.size());
    Append(packet, bytes);
    packet.rpos(initialReadPosition);

    MopGroupInvitePackets::Response response;
    response.hasRoles = true;
    response.accepted = true;
    response.roles = 0xA5A55A5A;

    CHECK(!MopGroupInvitePackets::ParseResponse(packet, response));
    CHECK(packet.rpos() == packet.size());
    CHECK(response.hasRoles);
    CHECK(response.accepted);
    CHECK(response.roles == 0xA5A55A5A);
}

static void test_retail_and_synthetic_successes()
{
    CheckSuccess({ 0x7F, 0x00 }, false, false, 0);
    CheckSuccess({ 0x7F, 0x40 }, false, true, 0);
    CheckSuccess({ 0x7F, 0xC0, 0x0A, 0x00, 0x00, 0x00 },
        true, true, 0x0000000A);

    CheckSuccess({ 0x7F, 0x80, 0x0A, 0x00, 0x00, 0x00 },
        true, false, 0x0000000A);
    CheckSuccess({ 0x7F, 0xC0, 0x02, 0x04, 0x08, 0x10 },
        true, true, 0x10080402);
}

static void test_malformed_bodies_fail_atomically()
{
    CheckFailure({});
    CheckFailure({ 0x7F });
    CheckFailure({ 0x00, 0x00 });
    CheckFailure({ 0x7E, 0x00 });

    CheckFailure({ 0x7F, 0x20 });
    CheckFailure({ 0x7F, 0x10 });
    CheckFailure({ 0x7F, 0x08 });
    CheckFailure({ 0x7F, 0x04 });
    CheckFailure({ 0x7F, 0x02 });
    CheckFailure({ 0x7F, 0x01 });

    CheckFailure({ 0x7F, 0x80 });
    CheckFailure({ 0x7F, 0x80, 0x01 });
    CheckFailure({ 0x7F, 0x80, 0x01, 0x02 });
    CheckFailure({ 0x7F, 0x80, 0x01, 0x02, 0x03 });
    CheckFailure({ 0x7F, 0x00, 0x01 });
    CheckFailure({ 0x7F, 0x80, 0x01, 0x02, 0x03, 0x04, 0x05 });

    CheckFailure({ 0xA5, 0x7F, 0x00 }, 1);
}

int main(int, char**)
{
    test_retail_and_synthetic_successes();
    test_malformed_bodies_fail_atomically();
    if (g_fail)
        return 1;
    std::printf("mop_group_invite_response_packets: all checks passed\n");
    return 0;
}
