/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 taxi-menu request/reply fixtures.
 */

#include "Player.h"
#include "DBCStores.h"
#include "Database/DatabaseEnv.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket MakePacket(OpcodesList opcode,
    std::vector<uint8_t> const& body)
{
    WorldPacket packet(opcode, body.size());
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

static WorldPacket BuildMenu(ObjectGuid guid, uint32 currentNode,
    std::vector<uint8_t> const& mask)
{
    WorldPacket packet(SMSG_SHOWTAXINODES, 17 + mask.size());
    MopTaxiPackets::BuildShowTaxiNodes(packet, guid, currentNode,
        mask.data(), mask.size());
    return packet;
}

static void test_captured_and_dense_requests()
{
    {
        WorldPacket request = MakePacket(CMSG_TAXIQUERYAVAILABLENODES,
            { 0xF6, 0xA7, 0xF0, 0x77, 0x31, 0x3E, 0x1F });
        ObjectGuid guid;
        CHECK(MopTaxiPackets::ParseTaxiQueryAvailableNodes(request, guid));
        CHECK(guid.GetRawValue() == UINT64_C(0xF130763F00001EA6));
        CHECK(request.rpos() == request.size());
    }

    {
        WorldPacket request = MakePacket(CMSG_TAXIQUERYAVAILABLENODES,
            { 0xFF, 0x09, 0x04, 0x00, 0x02, 0x07, 0x03, 0x05, 0x06 });
        ObjectGuid guid;
        CHECK(MopTaxiPackets::ParseTaxiQueryAvailableNodes(request, guid));
        CHECK(guid.GetRawValue() == UINT64_C(0x0102030405060708));
        CHECK(request.rpos() == request.size());
    }

    {
        WorldPacket request = MakePacket(CMSG_TAXIQUERYAVAILABLENODES,
            { 0x00 });
        ObjectGuid guid(UINT64_C(0x8877665544332211));
        CHECK(MopTaxiPackets::ParseTaxiQueryAvailableNodes(request, guid));
        CHECK(guid.IsEmpty());
        CHECK(request.rpos() == request.size());
    }
}

static void test_request_rejects_short_trailing_and_noncanonical_bodies()
{
    std::vector<uint8_t> const dense =
        { 0xFF, 0x09, 0x04, 0x00, 0x02, 0x07, 0x03, 0x05, 0x06 };
    std::vector<std::vector<uint8_t>> malformed;
    for (size_t size = 0; size < dense.size(); ++size)
    {
        malformed.emplace_back(dense.begin(), dense.begin() + size);
    }

    for (uint16 trailer = 0; trailer <= 0xFF; ++trailer)
    {
        std::vector<uint8_t> body = dense;
        body.push_back(uint8(trailer));
        malformed.push_back(body);
    }

    malformed.push_back({ 0x80, 0x01 });

    for (std::vector<uint8_t> const& body : malformed)
    {
        WorldPacket request = MakePacket(CMSG_TAXIQUERYAVAILABLENODES,
            body);
        ObjectGuid guid(UINT64_C(0x8877665544332211));
        CHECK(!MopTaxiPackets::ParseTaxiQueryAvailableNodes(request, guid));
        CHECK(guid.GetRawValue() == UINT64_C(0x8877665544332211));
        CHECK(request.rpos() == request.size());
    }
}

static void test_exact_synthetic_reply()
{
    WorldPacket packet = BuildMenu(
        ObjectGuid(UINT64_C(0x0102030405060708)),
        UINT32_C(0x78563412), { 0xA5, 0x5A });

    CHECK(packet.GetOpcode() == SMSG_SHOWTAXINODES);
    CHECK(ExpectBytes(packet,
        { 0xFF, 0x80, 0x00, 0x01, 0x00,
          0x09, 0x04, 0x12, 0x34, 0x56, 0x78,
          0x02, 0x07, 0x03, 0x06, 0x00, 0x05,
          0xA5, 0x5A }));
}

static void CheckTail(size_t maskSize,
    std::vector<uint8_t> const& expectedHeader)
{
    std::vector<uint8_t> mask(maskSize);
    for (size_t i = 0; i < mask.size(); ++i)
    {
        mask[i] = uint8((i * 37 + 11) & 0xFF);
    }

    WorldPacket packet = BuildMenu(
        ObjectGuid(UINT64_C(0x0102030405060708)),
        UINT32_C(0x78563412), mask);
    CHECK(packet.size() == 17 + maskSize);
    for (size_t i = 0; i < expectedHeader.size(); ++i)
    {
        CHECK(packet.contents()[i] == expectedHeader[i]);
    }
    for (size_t i = 0; i < mask.size(); ++i)
    {
        CHECK(packet.contents()[17 + i] == mask[i]);
    }
}

static void test_legacy_and_current_mask_tail_counts()
{
    CheckTail(78, { 0xFF, 0x80, 0x00, 0x27, 0x00 });
    CheckTail(162, { 0xFF, 0x80, 0x00, 0x51, 0x00 });
}

static void test_player_taxi_mask_accessors()
{
    std::memset(sTaxiNodesMask, 0x5A, sizeof(sTaxiNodesMask));

    PlayerTaxi taxi;
    CHECK(taxi.GetTaxiMaskSize() == 162);
    CHECK(taxi.SetTaximaskNode(1));
    CHECK(taxi.SetTaximaskNode(1296));

    uint8 const* known = taxi.GetTaxiMask(false);
    CHECK(known != nullptr);
    CHECK(known[0] == 0x01);
    CHECK(known[161] == 0x80);

    uint8 const* all = taxi.GetTaxiMask(true);
    CHECK(all != nullptr);
    CHECK(all[0] == 0x5A);
    CHECK(all[161] == 0x5A);
}

int main()
{
    test_captured_and_dense_requests();
    test_request_rejects_short_trailing_and_noncanonical_bodies();
    test_exact_synthetic_reply();
    test_legacy_and_current_mask_tail_counts();
    test_player_taxi_mask_accessors();

    if (g_fail)
    {
        std::fprintf(stderr, "%d taxi-menu fixture(s) failed\n", g_fail);
        return 1;
    }

    std::puts("MoP taxi-menu packet fixtures passed");
    return 0;
}
