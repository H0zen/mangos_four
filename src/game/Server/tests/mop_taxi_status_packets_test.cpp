/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 taxi-node status packet fixtures.
 */

#include "Player.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <type_traits>
#include <vector>

static_assert(!std::is_convertible<uint8_t,
    MopTaxiPackets::TaxiNodeStatus>::value,
    "taxi status builder must not accept unvalidated scalar statuses");

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

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

static WorldPacket BuildStatus(ObjectGuid guid,
    MopTaxiPackets::TaxiNodeStatus status,
    OpcodesList opcode = SMSG_TAXINODE_STATUS)
{
    WorldPacket packet(opcode, 10);
    MopTaxiPackets::BuildStatusBody(packet, guid, status);
    return packet;
}

static void test_retail_request_reply_pairs()
{
    struct Pair
    {
        uint64_t guid;
        std::vector<uint8_t> request;
        std::vector<uint8_t> reply;
    };
    std::vector<Pair> const pairs = {
        { UINT64_C(0xF130EC0000004D3A),
            { 0xAD, 0xF0, 0x4C, 0xED, 0x3B, 0x31 },
            { 0xB5, 0x40, 0x3B, 0xED, 0x4C, 0x31, 0xF0 } },
        { UINT64_C(0xF130843900006A40),
            { 0xED, 0xF0, 0x6B, 0x85, 0x38, 0x41, 0x31 },
            { 0xBD, 0x40, 0x41, 0x85, 0x6B, 0x38, 0x31, 0xF0 } }
    };

    for (Pair const& pair : pairs)
    {
        WorldPacket request = MakePacket(CMSG_TAXINODE_STATUS_QUERY,
            pair.request);
        ObjectGuid guid;
        CHECK(MopTaxiPackets::ParseStatusQuery(request, guid));
        CHECK(guid.GetRawValue() == pair.guid);
        CHECK(request.rpos() == request.size());

        WorldPacket reply = BuildStatus(guid,
            MopTaxiPackets::TaxiNodeStatus::Learned);
        CHECK(reply.GetOpcode() == SMSG_TAXINODE_STATUS);
        CHECK(ExpectBytes(reply, pair.reply));
    }
}

static void test_maximum_observed_bodies()
{
    WorldPacket request = MakePacket(CMSG_TAXINODE_STATUS_QUERY,
        { 0xEF, 0xF0, 0xD7, 0xEF, 0x97, 0xC0, 0x19, 0x31 });
    ObjectGuid guid;
    CHECK(MopTaxiPackets::ParseStatusQuery(request, guid));
    CHECK(guid.GetRawValue() == UINT64_C(0xF130EEC10096D618));

    WorldPacket reply = BuildStatus(
        ObjectGuid(UINT64_C(0xF130EEC10056A519)),
        MopTaxiPackets::TaxiNodeStatus::Learned);
    CHECK(ExpectBytes(reply,
        { 0xFD, 0x40, 0x18, 0xEF, 0x57, 0xA4, 0xC0, 0x31, 0xF0 }));
}

static void test_dense_and_single_zero_requests()
{
    WorldPacket empty = MakePacket(CMSG_TAXINODE_STATUS_QUERY, { 0x00 });
    ObjectGuid guid;
    CHECK(MopTaxiPackets::ParseStatusQuery(empty, guid));
    CHECK(guid.IsEmpty());
    CHECK(empty.rpos() == empty.size());

    WorldPacket dense = MakePacket(CMSG_TAXINODE_STATUS_QUERY,
        { 0xFF, 0x09, 0x03, 0x07, 0x02, 0x04, 0x00, 0x06, 0x05 });
    CHECK(MopTaxiPackets::ParseStatusQuery(dense, guid));
    CHECK(guid.GetRawValue() == UINT64_C(0x0807060504030201));

    std::vector<std::vector<uint8_t>> const bodies = {
        { 0xF7, 0x09, 0x03, 0x07, 0x02, 0x04, 0x06, 0x05 },
        { 0xDF, 0x09, 0x07, 0x02, 0x04, 0x00, 0x06, 0x05 },
        { 0xFD, 0x09, 0x03, 0x07, 0x04, 0x00, 0x06, 0x05 },
        { 0xEF, 0x09, 0x03, 0x07, 0x02, 0x04, 0x00, 0x06 },
        { 0xBF, 0x09, 0x03, 0x07, 0x02, 0x00, 0x06, 0x05 },
        { 0xFB, 0x09, 0x03, 0x02, 0x04, 0x00, 0x06, 0x05 },
        { 0xFE, 0x09, 0x03, 0x07, 0x02, 0x04, 0x00, 0x05 },
        { 0x7F, 0x03, 0x07, 0x02, 0x04, 0x00, 0x06, 0x05 }
    };
    for (uint8_t zeroByte = 0; zeroByte < 8; ++zeroByte)
    {
        WorldPacket request = MakePacket(CMSG_TAXINODE_STATUS_QUERY,
            bodies[zeroByte]);
        CHECK(MopTaxiPackets::ParseStatusQuery(request, guid));
        uint64_t const expected = UINT64_C(0x0807060504030201) &
            ~(UINT64_C(0xFF) << (zeroByte * 8));
        CHECK(guid.GetRawValue() == expected);
    }
}

static void test_dense_and_single_zero_replies()
{
    WorldPacket empty = BuildStatus(ObjectGuid(),
        MopTaxiPackets::TaxiNodeStatus::Learned);
    CHECK(ExpectBytes(empty, { 0x01, 0x00 }));

    ObjectGuid const denseGuid(UINT64_C(0x0807060504030201));
    WorldPacket dense = BuildStatus(denseGuid,
        MopTaxiPackets::TaxiNodeStatus::Learned);
    CHECK(ExpectBytes(dense,
        { 0xFD, 0xC0, 0x00, 0x07, 0x02, 0x03, 0x04, 0x06, 0x09, 0x05 }));

    std::vector<std::vector<uint8_t>> const bodies = {
        { 0xFD, 0x80, 0x07, 0x02, 0x03, 0x04, 0x06, 0x09, 0x05 },
        { 0xF9, 0xC0, 0x00, 0x07, 0x02, 0x04, 0x06, 0x09, 0x05 },
        { 0xBD, 0xC0, 0x00, 0x07, 0x03, 0x04, 0x06, 0x09, 0x05 },
        { 0xFD, 0x40, 0x00, 0x07, 0x02, 0x03, 0x04, 0x06, 0x09 },
        { 0xF5, 0xC0, 0x00, 0x07, 0x02, 0x03, 0x06, 0x09, 0x05 },
        { 0xED, 0xC0, 0x00, 0x02, 0x03, 0x04, 0x06, 0x09, 0x05 },
        { 0x7D, 0xC0, 0x00, 0x07, 0x02, 0x03, 0x04, 0x09, 0x05 },
        { 0xDD, 0xC0, 0x00, 0x07, 0x02, 0x03, 0x04, 0x06, 0x05 }
    };
    for (uint8_t zeroByte = 0; zeroByte < 8; ++zeroByte)
    {
        uint64_t const guid = UINT64_C(0x0807060504030201) &
            ~(UINT64_C(0xFF) << (zeroByte * 8));
        WorldPacket reply = BuildStatus(ObjectGuid(guid),
            MopTaxiPackets::TaxiNodeStatus::Learned);
        CHECK(ExpectBytes(reply, bodies[zeroByte]));
    }
}

static void test_all_status_values_and_semantics()
{
    ObjectGuid const guid(UINT64_C(0x0807060504030201));
    struct StatusFixture
    {
        MopTaxiPackets::TaxiNodeStatus status;
        uint8_t firstByte;
    };
    StatusFixture const fixtures[] = {
        { MopTaxiPackets::TaxiNodeStatus::NotEligible, 0xFC },
        { MopTaxiPackets::TaxiNodeStatus::Learned, 0xFD },
        { MopTaxiPackets::TaxiNodeStatus::None, 0xFE },
        { MopTaxiPackets::TaxiNodeStatus::Unlearned, 0xFF }
    };
    for (StatusFixture const& fixture : fixtures)
    {
        WorldPacket reply = BuildStatus(guid, fixture.status);
        CHECK(reply.size() == 10);
        CHECK(reply.contents()[0] == fixture.firstByte);
        CHECK(reply.contents()[1] == 0xC0);
    }

    CHECK(MopTaxiPackets::StatusForKnown(true) ==
        MopTaxiPackets::TaxiNodeStatus::Learned);
    CHECK(MopTaxiPackets::StatusForKnown(false) ==
        MopTaxiPackets::TaxiNodeStatus::Unlearned);

    WorldPacket bodyOnly = BuildStatus(guid,
        MopTaxiPackets::TaxiNodeStatus::Learned, SMSG_NEW_TAXI_PATH);
    CHECK(bodyOnly.GetOpcode() == SMSG_NEW_TAXI_PATH);
}

static void test_request_rejects_malformed_bodies()
{
    std::vector<uint8_t> const dense =
        { 0xFF, 0x09, 0x03, 0x07, 0x02, 0x04, 0x00, 0x06, 0x05 };
    std::vector<std::vector<uint8_t>> malformed;
    for (size_t size = 0; size < dense.size(); ++size)
    {
        malformed.push_back(std::vector<uint8_t>(dense.begin(),
            dense.begin() + size));
    }
    for (uint16_t trailer = 0; trailer <= 0xFF; ++trailer)
    {
        std::vector<uint8_t> body = dense;
        body.push_back(uint8_t(trailer));
        malformed.push_back(body);
    }
    malformed.push_back({ 0x80, 0x01 });

    for (std::vector<uint8_t> const& body : malformed)
    {
        WorldPacket request = MakePacket(CMSG_TAXINODE_STATUS_QUERY, body);
        ObjectGuid guid(UINT64_C(0x8877665544332211));
        CHECK(!MopTaxiPackets::ParseStatusQuery(request, guid));
        CHECK(guid.GetRawValue() == UINT64_C(0x8877665544332211));
        CHECK(request.rpos() == request.size());
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    test_retail_request_reply_pairs();
    test_maximum_observed_bodies();
    test_dense_and_single_zero_requests();
    test_dense_and_single_zero_replies();
    test_all_status_values_and_semantics();
    test_request_rejects_malformed_bodies();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_taxi_status_packets: all checks passed\n");
    return 0;
}
