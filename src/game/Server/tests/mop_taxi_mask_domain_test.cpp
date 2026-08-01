/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 taxi-mask domain and serialization fixtures.
 */

#include "PlayerTaxi.h"
#include "ByteBuffer.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"

#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void CheckPosition(uint32 nodeId, bool expectedValid,
    size_t expectedByte = 0, uint8 expectedBit = 0)
{
    TaxiMaskPosition position = {};
    bool const valid = GetTaxiMaskPosition(nodeId, position);
    CHECK(valid == expectedValid);
    if (valid)
    {
        CHECK(position.byteIndex == expectedByte);
        CHECK(position.bitMask == expectedBit);
    }
}

static void test_domain_mapping()
{
    CHECK(TaxiMaskSize == 162);
    CHECK(TaxiMaskRequiredBytes(0) == 0);
    CHECK(TaxiMaskRequiredBytes(1) == 1);
    CHECK(TaxiMaskRequiredBytes(8) == 1);
    CHECK(TaxiMaskRequiredBytes(9) == 2);
    CHECK(TaxiMaskRequiredBytes(1294) == 162);
    CHECK(TaxiMaskRequiredBytes(1296) == 162);
    CHECK(TaxiMaskRequiredBytes(1297) == 163);
    CHECK(TaxiMaskRequiredBytes(UINT32_MAX) == size_t(536870912));

    CheckPosition(0, false);
    CheckPosition(1, true, 0, 0x01);
    CheckPosition(8, true, 0, 0x80);
    CheckPosition(9, true, 1, 0x01);
    CheckPosition(912, true, 113, 0x80);
    CheckPosition(913, true, 114, 0x01);
    CheckPosition(1294, true, 161, 0x20);
    CheckPosition(1295, true, 161, 0x40);
    CheckPosition(1296, true, 161, 0x80);
    CheckPosition(1297, false);
}

static std::string RepeatedMaskTokens(size_t count, uint32 value)
{
    std::ostringstream out;
    for (size_t i = 0; i < count; ++i)
    {
        out << value << ' ';
    }
    return out.str();
}

static size_t CountMaskTokens(std::string const& value)
{
    std::istringstream in(value);
    uint32 token = 0;
    size_t count = 0;
    while (in >> token)
    {
        ++count;
    }
    return count;
}

static void test_player_bounds_and_legacy_expansion()
{
    std::memset(sTaxiNodesMask, 0xFF, sizeof(sTaxiNodesMask));

    PlayerTaxi taxi;
    CHECK(!taxi.IsValidNodeId(0));
    CHECK(taxi.IsValidNodeId(1));
    CHECK(taxi.IsValidNodeId(1296));
    CHECK(!taxi.IsValidNodeId(1297));
    CHECK(!taxi.SetTaximaskNode(0));
    CHECK(!taxi.SetTaximaskNode(1297));
    CHECK(taxi.SetTaximaskNode(1294));
    CHECK(taxi.IsTaximaskNodeKnown(1294));
    CHECK(!taxi.IsTaximaskNodeKnown(0));
    CHECK(!taxi.IsTaximaskNodeKnown(1297));

    std::string const legacy = RepeatedMaskTokens(114, 255);
    taxi.LoadTaxiMask(legacy.c_str());
    CHECK(taxi.IsTaximaskNodeKnown(912));
    CHECK(!taxi.IsTaximaskNodeKnown(913));
    CHECK(!taxi.IsTaximaskNodeKnown(1294));
    CHECK(!taxi.IsTaximaskNodeKnown(1296));
}

static void test_full_round_trip_and_append()
{
    std::memset(sTaxiNodesMask, 0xFF, sizeof(sTaxiNodesMask));

    PlayerTaxi original;
    CHECK(original.SetTaximaskNode(1));
    CHECK(original.SetTaximaskNode(913));
    CHECK(original.SetTaximaskNode(1294));
    CHECK(original.SetTaximaskNode(1296));

    std::ostringstream saved;
    saved << original;
    CHECK(CountMaskTokens(saved.str()) == 162);

    PlayerTaxi loaded;
    CHECK(loaded.SetTaximaskNode(1295));
    loaded.LoadTaxiMask(saved.str().c_str());
    CHECK(loaded.IsTaximaskNodeKnown(1));
    CHECK(loaded.IsTaximaskNodeKnown(913));
    CHECK(loaded.IsTaximaskNodeKnown(1294));
    CHECK(!loaded.IsTaximaskNodeKnown(1295));
    CHECK(loaded.IsTaximaskNodeKnown(1296));

    ByteBuffer data;
    loaded.AppendTaximaskTo(data, false);
    CHECK(data.size() == 4 + 162);
    CHECK(data.contents()[4] == 0x01);
    CHECK(data.contents()[4 + 114] == 0x01);
    CHECK(data.contents()[4 + 161] == 0xA0);

    ByteBuffer allData;
    loaded.AppendTaximaskTo(allData, true);
    CHECK(allData.size() == 4 + 162);
    CHECK(allData.contents()[4 + 161] == 0xFF);
}

int main()
{
    test_domain_mapping();
    test_player_bounds_and_legacy_expansion();
    test_full_round_trip_and_append();

    if (g_fail)
    {
        std::fprintf(stderr, "%d taxi-mask domain fixture(s) failed\n", g_fail);
        return 1;
    }

    std::puts("MoP taxi-mask domain fixtures passed");
    return 0;
}
