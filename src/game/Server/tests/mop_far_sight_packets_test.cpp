/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 far-sight request fixtures.
 *
 * CMSG_FAR_SIGHT carries exactly one MSB-first bit plus seven zero padding
 * bits. Every sampled 18414 body is 0x80 (enable) or 0x00 (disable). These
 * fixtures exercise the production reader so malformed padding and byte tails
 * cannot drive camera state.
 */

#include "MopFarSightPackets.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void test_far_sight_opcode_value()
{
    WorldPacket request(CMSG_FAR_SIGHT, 1);
    CHECK(uint32(request.GetOpcode()) == 0x1341u);
}

static WorldPacket MakeRequest(std::initializer_list<uint8> body)
{
    WorldPacket request(CMSG_FAR_SIGHT, body.size());
    for (uint8 byte : body)
        request << byte;
    return request;
}

static void CheckAccepted(std::initializer_list<uint8> body, bool expected)
{
    WorldPacket request = MakeRequest(body);
    bool enable = !expected;
    CHECK(MopFarSightPackets::ReadRequest(request, enable));
    CHECK(enable == expected);
    CHECK(request.rpos() == request.size());
}

static void CheckRejected(std::initializer_list<uint8> body, bool sentinel)
{
    WorldPacket request = MakeRequest(body);
    bool enable = sentinel;
    CHECK(!MopFarSightPackets::ReadRequest(request, enable));
    CHECK(enable == sentinel);
    CHECK(request.rpos() == request.size());
}

static void test_far_sight_exact_canonical_bodies()
{
    CheckAccepted({0x80}, true);
    CheckAccepted({0x00}, false);
}

static void test_far_sight_rejects_malformed_frames_without_committing_output()
{
    CheckRejected({}, false);
    CheckRejected({0x01}, true);
    CheckRejected({0x7F}, false);
    CheckRejected({0x81}, true);
    CheckRejected({0xFF}, false);
    CheckRejected({0x80, 0x00}, true);
    CheckRejected({0x00, 0x00}, false);
}

int main(int /*argc*/, char** /*argv*/)
{
    test_far_sight_opcode_value();
    test_far_sight_exact_canonical_bodies();
    test_far_sight_rejects_malformed_frames_without_committing_output();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_far_sight_packets: all checks passed\n");
    return 0;
}
