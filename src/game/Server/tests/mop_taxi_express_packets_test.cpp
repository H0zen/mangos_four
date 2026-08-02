/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 express-taxi activation packet fixtures.
 */

#include "Database/DatabaseEnv.h"
#include "Player.h"

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
    WorldPacket packet(CMSG_ACTIVATETAXIEXPRESS, body.size());
    if (!body.empty())
        packet.append(body.data(), body.size());
    return packet;
}

static std::vector<uint8> LiveMultiHopFixture()
{
    // Live smoke 2026-08-01 22:33:02/04, identical 23-byte requests.
    return {
        0xC0,0x00,0x03,0xEC,0x03,0xF0,0xE1,0x02,0x00,0x00,0x00,
        0x46,0x02,0x00,0x00,0x4D,0x02,0x00,0x00,0xD1,0x00,0x31,0x61
    };
}

static void test_live_multihop_request_decodes_exactly()
{
    WorldPacket packet = MakePacket(LiveMultiHopFixture());
    MopTaxiPackets::TaxiExpressRequest request;
    CHECK(MopTaxiPackets::ParseActivateTaxiExpress(packet, request));
    CHECK(request.nodes == std::vector<uint32>({2, 582, 589}));
    CHECK(request.flightMaster.GetRawValue() == UINT64_C(0xF13001600002E0D0));
    CHECK(packet.rpos() == packet.size());
}

static void CheckRejected(std::vector<uint8> const& body)
{
    WorldPacket packet = MakePacket(body);
    MopTaxiPackets::TaxiExpressRequest request;
    request.nodes = {99};
    request.flightMaster = ObjectGuid(UINT64_C(0xAAAAAAAAAAAAAAAA));
    CHECK(!MopTaxiPackets::ParseActivateTaxiExpress(packet, request));
    CHECK(request.nodes == std::vector<uint32>({99}));
    CHECK(request.flightMaster.GetRawValue() == UINT64_C(0xAAAAAAAAAAAAAAAA));
    CHECK(packet.rpos() == packet.size());
}

static void test_malformed_and_noncanonical_bodies_are_rejected()
{
    std::vector<uint8> const full = LiveMultiHopFixture();
    CheckRejected({full.begin(), full.end() - 1});

    std::vector<uint8> trailing = full;
    trailing.push_back(0xA5);
    CheckRejected(trailing);

    std::vector<uint8> padding = full;
    padding[3] |= 0x01;
    CheckRejected(padding);

    std::vector<uint8> repeated = full;
    repeated[15] = 0x46;
    repeated[16] = 0x02;
    CheckRejected(repeated);

    std::vector<uint8> zeroNode = full;
    zeroNode[7] = zeroNode[8] = zeroNode[9] = zeroNode[10] = 0;
    CheckRejected(zeroNode);
}

int main()
{
    test_live_multihop_request_decodes_exactly();
    test_malformed_and_noncanonical_bodies_are_rejected();

    if (g_fail)
    {
        std::fprintf(stderr, "%d express-taxi packet check(s) failed\n", g_fail);
        return 1;
    }

    std::puts("MoP express-taxi packet checks passed");
    return 0;
}
