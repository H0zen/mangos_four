/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 CMSG_PET_STOP_ATTACK packet fixtures.
 */

#include "Unit.h"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static WorldPacket InputPacket(std::vector<uint8> const& body)
{
    WorldPacket packet(CMSG_PET_STOP_ATTACK, uint32(body.size()));
    if (!body.empty())
    {
        packet.append(body.data(), body.size());
    }
    return packet;
}

static void CheckValid(char const* what, std::vector<uint8> const& body,
    uint64 expectedGuid)
{
    WorldPacket packet = InputPacket(body);
    ObjectGuid guid;
    bool const accepted = MopCompactPackets::ReadPetStopAttack(packet, guid);

    if (!accepted || guid.GetRawValue() != expectedGuid ||
        packet.rpos() != packet.size())
    {
        std::fprintf(stderr,
            "FAIL %s: accepted %u guid 0x%016llX consumed %u/%u\n",
            what, unsigned(accepted),
            (unsigned long long)guid.GetRawValue(),
            unsigned(packet.rpos()), unsigned(packet.size()));
        ++g_fail;
    }
}

static void CheckInvalid(char const* what, std::vector<uint8> const& body)
{
    WorldPacket packet = InputPacket(body);
    ObjectGuid guid(UINT64_C(0x0807060504030201));
    bool const accepted = MopCompactPackets::ReadPetStopAttack(packet, guid);

    if (accepted || guid.GetRawValue() != UINT64_C(0x0807060504030201))
    {
        std::fprintf(stderr,
            "FAIL %s: accepted %u guid 0x%016llX\n",
            what, unsigned(accepted),
            (unsigned long long)guid.GetRawValue());
        ++g_fail;
    }
}

static void test_retail_vehicle_bodies()
{
    // Captured retail body from catalogue generation 2BE10C89.
    CheckValid("retail vehicle F1506C6B00002F07",
        { 0xFA, 0x6D, 0x06, 0x6A, 0x2E, 0xF0, 0x51 },
        UINT64_C(0xF1506C6B00002F07));

    // Captured retail body from catalogue generation 2BE10C89.
    CheckValid("retail vehicle F1506A7A00A3DB5C",
        { 0xFE, 0xA2, 0x6B, 0x5D, 0x7B, 0xDA, 0xF0, 0x51 },
        UINT64_C(0xF1506A7A00A3DB5C));
}

static void test_binary_derived_guid_permutations()
{
    // Binary-derived synthetic fixtures for raw GUID bytes 01..08. The first
    // body is dense; each following body clears exactly one distinct GUID byte.
    CheckValid("binary-derived dense", { 0xFF, 0x02, 0x07, 0x00, 0x04,
        0x03, 0x09, 0x06, 0x05 }, UINT64_C(0x0807060504030201));
    CheckValid("binary-derived zero byte 0", { 0xF7, 0x02, 0x07, 0x04,
        0x03, 0x09, 0x06, 0x05 }, UINT64_C(0x0807060504030200));
    CheckValid("binary-derived zero byte 1", { 0xDF, 0x02, 0x07, 0x00,
        0x04, 0x09, 0x06, 0x05 }, UINT64_C(0x0807060504030001));
    CheckValid("binary-derived zero byte 2", { 0xFB, 0x07, 0x00, 0x04,
        0x03, 0x09, 0x06, 0x05 }, UINT64_C(0x0807060504000201));
    CheckValid("binary-derived zero byte 3", { 0xFE, 0x02, 0x07, 0x00,
        0x04, 0x03, 0x09, 0x06 }, UINT64_C(0x0807060500030201));
    CheckValid("binary-derived zero byte 4", { 0xFD, 0x02, 0x07, 0x00,
        0x03, 0x09, 0x06, 0x05 }, UINT64_C(0x0807060004030201));
    CheckValid("binary-derived zero byte 5", { 0xBF, 0x02, 0x00, 0x04,
        0x03, 0x09, 0x06, 0x05 }, UINT64_C(0x0807000504030201));
    CheckValid("binary-derived zero byte 6", { 0xEF, 0x02, 0x07, 0x00,
        0x04, 0x03, 0x09, 0x05 }, UINT64_C(0x0800060504030201));
    CheckValid("binary-derived zero byte 7", { 0x7F, 0x02, 0x07, 0x00,
        0x04, 0x03, 0x06, 0x05 }, UINT64_C(0x0007060504030201));
}

static void test_rejects_zero_underflow_and_trailing_bytes()
{
    CheckInvalid("empty body", {});
    CheckInvalid("zero GUID", { 0x00 });

    std::vector<uint8> const dense = {
        0xFF, 0x02, 0x07, 0x00, 0x04, 0x03, 0x09, 0x06, 0x05
    };
    for (size_t length = 1; length < dense.size(); ++length)
    {
        CheckInvalid("truncated dense body",
            std::vector<uint8>(dense.begin(), dense.begin() + length));
    }

    for (unsigned trailer = 0; trailer <= 0xFF; ++trailer)
    {
        std::vector<uint8> body = dense;
        body.push_back(uint8(trailer));
        CheckInvalid("one-byte trailer", body);
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    test_retail_vehicle_bodies();
    test_binary_derived_guid_permutations();
    test_rejects_zero_underflow_and_trailing_bytes();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_pet_stop_attack_packets: all checks passed\n");
    return 0;
}
