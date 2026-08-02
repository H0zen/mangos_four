/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 taxi spline-completion packet fixtures.
 */

#include "Database/DatabaseEnv.h"
#include "Player.h"

#include <cmath>
#include <cstdio>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket MakePacket(std::vector<uint8> const& body)
{
    WorldPacket packet(CMSG_MOVE_SPLINE_DONE, body.size());
    if (!body.empty())
        packet.append(body.data(), body.size());
    return packet;
}

static std::vector<uint8> ShortestFixture()
{
    // MoPSniff build 18414, capture-000476 sequence 491511, 34 bytes.
    return {
        0x6F, 0x71, 0x1C, 0x03, 0x19, 0x3B, 0x53, 0xC3,
        0x44, 0x12, 0x97, 0x45, 0x54, 0x01, 0xAB, 0x45,
        0xEB, 0x58, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00,
        0x00, 0xB1, 0x6C, 0x05, 0x5B, 0x04, 0x09, 0xF5,
        0x6D, 0x02
    };
}

static bool NearlyEqual(float left, float right)
{
    return std::fabs(left - right) < 0.001f;
}

static void CheckRejected(std::vector<uint8> const& body)
{
    WorldPacket packet = MakePacket(body);
    MopTaxiPackets::MoveSplineDoneRequest request;
    request.splineId = UINT32_C(0xAAAAAAAA);

    CHECK(!MopTaxiPackets::ParseMoveSplineDone(packet, request));
    CHECK(request.splineId == UINT32_C(0xAAAAAAAA));
    CHECK(packet.rpos() == packet.size());
}

static void test_shortest_real_fixture_decodes_exactly()
{
    WorldPacket packet = MakePacket(ShortestFixture());
    MopTaxiPackets::MoveSplineDoneRequest request;

    CHECK(MopTaxiPackets::ParseMoveSplineDone(packet, request));
    CHECK(request.splineId == UINT32_C(52195695));
    CHECK(request.movement.GetGuid().GetRawValue() == UINT64_C(0x04000000055AB06D));
    CHECK(uint32(request.movement.GetMovementFlags()) == UINT32_C(0x00100000));
    CHECK(request.movement.GetTime() == UINT32_C(40760585));
    CHECK(NearlyEqual(request.movement.GetPos()->x, 5472.166f));
    CHECK(NearlyEqual(request.movement.GetPos()->y, 4834.283f));
    CHECK(NearlyEqual(request.movement.GetPos()->z, -211.2309f));
    CHECK(packet.rpos() == packet.size());
}

static void test_larger_real_fixture_consumes_every_optional_field()
{
    // MoPSniff build 18414, capture-000183 sequence 129012, 87 bytes.
    WorldPacket packet = MakePacket({
        0x13, 0xBC, 0x33, 0x00, 0x78, 0xE8, 0xEA, 0x43,
        0x3C, 0x6B, 0x1A, 0x45, 0xC8, 0xDB, 0xDF, 0xC3,
        0xAB, 0x5B, 0x00, 0x00, 0x00, 0x44, 0xB4, 0x00,
        0x00, 0x00, 0x10, 0x01, 0x81, 0x23, 0x07, 0x00,
        0xB3, 0x05, 0x18, 0x3D, 0x67, 0x3F, 0x05, 0xE0,
        0x50, 0x44, 0xC1, 0xC1, 0xFF, 0x1E, 0xC5, 0x00,
        0x04, 0x5F, 0xC0, 0x20, 0x97, 0xA3, 0x41, 0xDC,
        0x76, 0x00, 0x00, 0x4B, 0x0D, 0x49, 0x3F, 0x70,
        0x79, 0x1E, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC3,
        0x2E, 0x7E, 0x3F, 0xAD, 0xE5, 0xF1, 0x15
    });
    MopTaxiPackets::MoveSplineDoneRequest request;

    CHECK(MopTaxiPackets::ParseMoveSplineDone(packet, request));
    CHECK(request.splineId == UINT32_C(3390483));
    CHECK(NearlyEqual(request.movement.GetPos()->z, 469.8162f));
    CHECK(NearlyEqual(request.movement.GetPos()->y, 2470.7021f));
    CHECK(NearlyEqual(request.movement.GetPos()->x, -447.7170f));
    CHECK(packet.rpos() == packet.size());
}

static void test_mover_identity_accepts_only_core_or_18414_player_domains()
{
    ObjectGuid const corePlayer(HIGHGUID_PLAYER, UINT32_C(0x055AB06D));
    CHECK(MopTaxiPackets::MatchesMoveSplinePlayer(
        ObjectGuid(UINT64_C(0x04000000055AB06D)), corePlayer));
    CHECK(MopTaxiPackets::MatchesMoveSplinePlayer(corePlayer, corePlayer));
    CHECK(!MopTaxiPackets::MatchesMoveSplinePlayer(
        ObjectGuid(UINT64_C(0x04000000055AB06E)), corePlayer));
    CHECK(!MopTaxiPackets::MatchesMoveSplinePlayer(
        ObjectGuid(UINT64_C(0xF1300000055AB06D)), corePlayer));
    CHECK(!MopTaxiPackets::MatchesMoveSplinePlayer(ObjectGuid(), corePlayer));
}

static void test_completion_authority_requires_server_endpoint_and_every_live_binding()
{
    TaxiFlightLedger ledger;
    ledger.Arm(1, 7, 8, 10, 20, 99);
    CHECK(ledger.MarkServerEndpoint(1, 7, 8, 99));
    CHECK(!ledger.TryConsumeCompletion(1, 7, 10, 20, 100, 8));
    CHECK(ledger.TryConsumeCompletion(1, 7, 10, 20, 99, 8));
    CHECK(!ledger.TryConsumeCompletion(1, 7, 10, 20, 99, 8));
}

static void test_malformed_and_noncanonical_bodies_are_rejected()
{
    std::vector<uint8> const full = ShortestFixture();
    CheckRejected({full.begin(), full.end() - 1});

    std::vector<uint8> trailing = full;
    trailing.push_back(0xA5);
    CheckRejected(trailing);

    std::vector<uint8> reservedBit = full;
    reservedBit[16] |= 0x04;
    CheckRejected(reservedBit);
}

int main()
{
    test_shortest_real_fixture_decodes_exactly();
    test_larger_real_fixture_consumes_every_optional_field();
    test_mover_identity_accepts_only_core_or_18414_player_domains();
    test_completion_authority_requires_server_endpoint_and_every_live_binding();
    test_malformed_and_noncanonical_bodies_are_rejected();

    if (g_fail)
    {
        std::fprintf(stderr, "%d taxi-completion packet check(s) failed\n", g_fail);
        return 1;
    }

    std::puts("MoP taxi-completion packet checks passed");
    return 0;
}
