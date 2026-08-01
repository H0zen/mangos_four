/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 direct taxi-activation request/reply fixtures.
 */

#include "Player.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket MakePacket(std::vector<uint8_t> const& body)
{
    WorldPacket packet(CMSG_ACTIVATETAXI, body.size());
    if (!body.empty())
    {
        packet.append(body.data(), body.size());
    }
    return packet;
}

static bool ExpectBytes(WorldPacket const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            return false;
        }
    }
    return true;
}

static void test_full_guid_request()
{
    WorldPacket packet = MakePacket({
        0x88, 0x77, 0x66, 0x55,
        0x44, 0x33, 0x22, 0x11,
        0xFF, 0x06, 0x09, 0x03, 0x02, 0x07, 0x05, 0x04, 0x00
    });
    MopTaxiPackets::TaxiActivationRequest request;
    CHECK(MopTaxiPackets::ParseActivateTaxi(packet, request));
    CHECK(request.destinationNode == UINT32_C(0x55667788));
    CHECK(request.sourceNode == UINT32_C(0x11223344));
    CHECK(request.flightMaster.GetRawValue() == UINT64_C(0x0102030405060708));
    CHECK(packet.rpos() == packet.size());
}

static void test_real_compact_capture()
{
    WorldPacket packet = MakePacket({
        0x36, 0x01, 0x00, 0x00,
        0x54, 0x01, 0x00, 0x00,
        0xEE, 0x6B, 0x41, 0x31, 0x85, 0x38, 0xF0
    });
    MopTaxiPackets::TaxiActivationRequest request;
    CHECK(MopTaxiPackets::ParseActivateTaxi(packet, request));
    CHECK(request.destinationNode == 310);
    CHECK(request.sourceNode == 340);
    CHECK(request.flightMaster.GetRawValue() == UINT64_C(0xF130843900006A40));
    CHECK(packet.rpos() == packet.size());
}

static void CheckRejected(std::vector<uint8_t> const& body)
{
    WorldPacket packet = MakePacket(body);
    MopTaxiPackets::TaxiActivationRequest request;
    request.destinationNode = UINT32_C(0xAAAAAAAA);
    request.sourceNode = UINT32_C(0xBBBBBBBB);
    request.flightMaster = ObjectGuid(UINT64_C(0xCCCCCCCCCCCCCCCC));

    CHECK(!MopTaxiPackets::ParseActivateTaxi(packet, request));
    CHECK(request.destinationNode == UINT32_C(0xAAAAAAAA));
    CHECK(request.sourceNode == UINT32_C(0xBBBBBBBB));
    CHECK(request.flightMaster.GetRawValue() == UINT64_C(0xCCCCCCCCCCCCCCCC));
    CHECK(packet.rpos() == packet.size());
}

static void test_request_rejects_every_truncation_and_trailing_byte()
{
    std::vector<uint8_t> const full = {
        0x88, 0x77, 0x66, 0x55,
        0x44, 0x33, 0x22, 0x11,
        0xFF, 0x06, 0x09, 0x03, 0x02, 0x07, 0x05, 0x04, 0x00
    };

    for (size_t size = 0; size < full.size(); ++size)
    {
        CheckRejected({ full.begin(), full.begin() + size });
    }

    std::vector<uint8_t> trailing = full;
    trailing.push_back(0xA5);
    CheckRejected(trailing);
}

static void test_request_rejects_noncanonical_legacy_and_zero_guid_shapes()
{
    CheckRejected({
        0x88, 0x77, 0x66, 0x55,
        0x44, 0x33, 0x22, 0x11,
        0xFF, 0x01, 0x09, 0x03, 0x02, 0x07, 0x05, 0x04, 0x00
    });

    CheckRejected({
        0xFF, 0x06, 0x09, 0x03, 0x02, 0x07, 0x05, 0x04, 0x00,
        0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11
    });

    CheckRejected({
        0x88, 0x77, 0x66, 0x55,
        0x44, 0x33, 0x22, 0x11,
        0xFF, 0x07, 0x08, 0x02, 0x03, 0x06, 0x04, 0x05, 0x01
    });

    CheckRejected({
        0x88, 0x77, 0x66, 0x55,
        0x44, 0x33, 0x22, 0x11,
        0x00
    });
}

static void CheckReply(ActivateTaxiReply reply, uint8 expected)
{
    WorldPacket packet(SMSG_ACTIVATETAXIREPLY, 1);
    MopTaxiPackets::BuildActivateTaxiReply(packet, reply);
    CHECK(packet.GetOpcode() == SMSG_ACTIVATETAXIREPLY);
    CHECK(ExpectBytes(packet, { expected }));
}

static void test_exact_one_byte_reply_vocabulary()
{
    CheckReply(ERR_TAXIOK, 0x80);
    CheckReply(ERR_TAXIUNSPECIFIEDSERVERERROR, 0x50);
    CheckReply(ERR_TAXINOSUCHPATH, 0x60);
    CheckReply(ERR_TAXINOTENOUGHMONEY, 0x40);
    CheckReply(ERR_TAXITOOFARAWAY, 0xD0);
    CheckReply(ERR_TAXINOVENDORNEARBY, 0xC0);
    CheckReply(ERR_TAXINOTVISITED, 0xF0);
    CheckReply(ERR_TAXIPLAYERBUSY, 0xA0);
    CheckReply(ERR_TAXIPLAYERALREADYMOUNTED, 0x70);
    CheckReply(ERR_TAXIPLAYERSHAPESHIFTED, 0x90);

    // The remaining legacy-only values have no proven distinct 18414 code.
    CheckReply(ERR_TAXIPLAYERMOVING, 0x50);
    CheckReply(ERR_TAXISAMENODE, 0x50);
    CheckReply(ERR_TAXINOTSTANDING, 0x50);
}

int main()
{
    test_full_guid_request();
    test_real_compact_capture();
    test_request_rejects_every_truncation_and_trailing_byte();
    test_request_rejects_noncanonical_legacy_and_zero_guid_shapes();
    test_exact_one_byte_reply_vocabulary();

    if (g_fail)
    {
        std::fprintf(stderr, "%d taxi-activation fixture(s) failed\n", g_fail);
        return 1;
    }

    std::puts("MoP taxi-activation packet fixtures passed");
    return 0;
}
