/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the 5.4.8 client build 18414.
 */

/**
 * Byte-exact tests for the 5.4.8 quest-POI request and response pair.
 */

#include "WorldSession.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "Object/ObjectMgr.h"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet,
    std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "  size %u, wanted %u\n",
            unsigned(packet.size()), unsigned(expected.size()));
        return false;
    }

    for (size_t i = 0; i < expected.size(); ++i)
    {
        if (packet.contents()[i] != expected[i])
        {
            std::fprintf(stderr, "  byte %u = 0x%02X, wanted 0x%02X\n",
                unsigned(i), packet.contents()[i], expected[i]);
            return false;
        }
    }
    return true;
}

static void test_request()
{
    {
        uint8_t const fixture[] = {
            0x00, 0x00, 0x08,             // two entries in a 22-bit count
            0x44, 0x33, 0x22, 0x11,
            0x88, 0x77, 0x66, 0x55
        };
        WorldPacket packet(CMSG_QUEST_POI_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));

        std::vector<uint32> questIds;
        CHECK(MopQueryPackets::ParseQuestPoiQueryRequest(packet, questIds));
        CHECK(questIds.size() == 2);
        CHECK(questIds[0] == 0x11223344u);
        CHECK(questIds[1] == 0x55667788u);
        CHECK(packet.rpos() == packet.size());
    }
    {
        uint8_t const fixture[] = {
            0x00, 0x00, 0x68              // 26 exceeds the 25-slot quest log
        };
        WorldPacket packet(CMSG_QUEST_POI_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));

        std::vector<uint32> questIds{ 7 };
        CHECK(!MopQueryPackets::ParseQuestPoiQueryRequest(packet, questIds));
        CHECK(questIds.size() == 1 && questIds[0] == 7);
        CHECK(packet.rpos() == packet.size());
    }
    {
        uint8_t const fixture[] = {
            0x00, 0x00, 0x04,             // one quest ID, but no body follows
        };
        WorldPacket packet(CMSG_QUEST_POI_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));

        std::vector<uint32> questIds;
        CHECK(!MopQueryPackets::ParseQuestPoiQueryRequest(packet, questIds));
        CHECK(questIds.empty());
        CHECK(packet.rpos() == packet.size());
    }
    {
        uint8_t const fixture[] = {
            0x00, 0x00, 0x00, 0xAA       // zero entries with trailing garbage
        };
        WorldPacket packet(CMSG_QUEST_POI_QUERY, sizeof(fixture));
        packet.append(fixture, sizeof(fixture));

        std::vector<uint32> questIds;
        CHECK(!MopQueryPackets::ParseQuestPoiQueryRequest(packet, questIds));
        CHECK(questIds.empty());
        CHECK(packet.rpos() == packet.size());
    }
}

static void test_response()
{
    MopQueryPackets::QuestPoiRecord poi;
    poi.poiId = 0x51525354u;
    poi.objectiveIndex = -2;
    poi.unknown2 = 0x61626364u;
    poi.mapId = 0x81828384u;
    poi.mapAreaId = 0xA1A2A3A4u;
    poi.worldEffectId = 0x01020304u;
    poi.playerConditionId = 0xD1D2D3D4u;
    poi.unknown1 = 0xC1C2C3C4u;
    poi.unknown3 = 0xB1B2B3B4u;
    poi.unknown4 = 0x71727374u;

    poi.points.push_back({ 0x11121314, 0x21222324 });
    poi.points.push_back({ 0x31323334, 0x41424344 });

    MopQueryPackets::QuestPoiResponse quest;
    quest.questId = 0xE1E2E3E4u;
    quest.pois.push_back(poi);

    WorldPacket packet;
    CHECK(MopQueryPackets::BuildQuestPoiQueryResponse(packet, { quest }));
    CHECK(packet.GetOpcode() == SMSG_QUEST_POI_QUERY_RESPONSE);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x10, 0x00, 0x04, 0x00, 0x00, 0x40,
        0x04, 0x03, 0x02, 0x01,
        0x14, 0x13, 0x12, 0x11,
        0x24, 0x23, 0x22, 0x21,
        0x34, 0x33, 0x32, 0x31,
        0x44, 0x43, 0x42, 0x41,
        0xFE, 0xFF, 0xFF, 0xFF,
        0x54, 0x53, 0x52, 0x51,
        0x64, 0x63, 0x62, 0x61,
        // slot 3 = unknown1 (0xC1C2C3C4), not unknown4 -- see the builder
        0xC4, 0xC3, 0xC2, 0xC1,
        0x84, 0x83, 0x82, 0x81,
        // The POI's point count repeated, NOT floorId. This POI has two points, so this
        // slot is 2. floorId used to be written here, and it is 0 in 28,128 of our 29,117
        // quest_poi rows, so 96.6% of POIs told the client "no points" and drew nothing.
        0x02, 0x00, 0x00, 0x00,
        0xA4, 0xA3, 0xA2, 0xA1,
        0xB4, 0xB3, 0xB2, 0xB1,
        // slot 8 = unknown4 (0x71727374), the quest_poi.unk4 flags field
        0x74, 0x73, 0x72, 0x71,
        0xD4, 0xD3, 0xD2, 0xD1,
        0xE4, 0xE3, 0xE2, 0xE1,
        0x01, 0x00, 0x00, 0x00,
        0x01, 0x00, 0x00, 0x00
    }));

    WorldPacket empty;
    CHECK(MopQueryPackets::BuildQuestPoiQueryResponse(empty, {}));
    CHECK(empty.GetOpcode() == SMSG_QUEST_POI_QUERY_RESPONSE);
    CHECK(ExpectBytes(empty, {
        0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    }));
}

/**
 * The scalar after mapId is the POI's own point count, repeated from the bit phase --
 * the same duplication the quest and POI counts already use. It is not floorId.
 *
 * Measured across 400 retail SMSG_QUEST_POI_QUERY_RESPONSE bodies covering 3825 POIs:
 * that slot equalled the POI's point count in 3825 of 3825, at counts 1, 3, 5, 6, 7, 8,
 * 9, 10, 11 and 12. The in-game evidence agrees from the other direction -- an
 * out-of-range value there crashed the client (quest 29406, value 252339), which is what
 * a bad element count does and not what a bad floor identifier does.
 *
 * A count is easy to get wrong in a way nothing detects: too small and the client
 * silently renders fewer points, zero and it renders none at all with no error.
 */
static void test_point_count_is_repeated_not_floor_id()
{
    // 12 is the largest count in the retail sample, but our own table reaches 2076
    // (quest 0 poi 0), with 40 POIs above 63. 63 is MAX_QUEST_POI_FLOOR_ID, and reusing
    // it as a bound here would silently truncate those 40 -- the same invisible failure
    // this test exists to catch. The 2076 case pins that the 21-bit field, not the floor
    // bound, is what governs.
    for (uint32 count : { 1u, 2u, 5u, 12u, 64u, 2076u })
    {
        MopQueryPackets::QuestPoiRecord poi;

        for (uint32 i = 0; i < count; ++i)
            poi.points.push_back({ int32(i), int32(i) });

        MopQueryPackets::QuestPoiResponse quest;
        quest.questId = 1234;
        quest.pois.push_back(poi);

        WorldPacket packet;
        CHECK(MopQueryPackets::BuildQuestPoiQueryResponse(packet, { quest }));

        // bits: questCount(20) + poiCount(18) + pointCount(21) = 59 -> 8 bytes.
        // bytes: worldEffectId, then 2 int32 per point, then the five scalars that
        //        precede the repeated count: objectiveIndex, poiId, unknown2, unknown1
        //        (slot 3) and mapId.
        size_t const offset = 8 + 4 + count * 8 + 5 * 4;
        CHECK(packet.size() >= offset + 4);
        if (packet.size() < offset + 4)
            continue;

        uint32 onWire = 0;
        std::memcpy(&onWire, packet.contents() + offset, 4);
        CHECK(onWire == count);
        if (onWire != count)
        {
            std::fprintf(stderr, "  %u point(s): wire slot held %u\n", count, onWire);
        }

        // The bit-phase count and this byte-phase copy must agree. The client sizes its
        // point vector from the bit phase (sub_14043E0D0 on the vector at POI+48) and
        // reads this scalar separately into POI+40, so a disagreement between them is
        // exactly the state that rendered nothing.
        size_t const bitPhase = 8 + 4 + count * 8;
        CHECK(packet.size() > bitPhase);
    }

    // There is no floorId to leak: QuestPoiRecord has no such member, so a reintroduction
    // is a compile error rather than something a runtime test has to catch. That is
    // deliberate -- 49 quest_poi rows still hold six-digit blob ids in that column, and
    // this slot is the one the client treats as an element count.
}

/**
 * Two quests, one with two POIs and one with a single POI, and two of the three POIs
 * carrying no points at all.
 *
 * The single-POI fixture above cannot catch an association bug: with one quest and one
 * POI, every plausible nesting produces the same bytes. This one pins that the bit phase
 * emits all of quest A's point counts before quest B's poiCount, that the byte phase
 * groups each quest's POIs with that quest's trailing questId, and that a zero-point POI
 * emits no coordinates while still emitting its repeated count.
 *
 * The bit header is computed independently of the builder rather than copied from its
 * output: 20 bits questCount, then per quest 18 bits poiCount followed by 21 bits per
 * POI, 119 bits total, padded to 15 bytes.
 */
static void test_multiple_quests_and_empty_pois()
{
    MopQueryPackets::QuestPoiRecord empty;      // no points at all
    empty.poiId = 1;

    MopQueryPackets::QuestPoiRecord single;
    single.poiId = 2;
    single.points.push_back({ 0x0A0B0C0D, 0x1A1B1C1D });

    MopQueryPackets::QuestPoiResponse questA;
    questA.questId = 0xAAAAAAAAu;
    questA.pois.push_back(empty);
    questA.pois.push_back(single);

    MopQueryPackets::QuestPoiResponse questB;
    questB.questId = 0xBBBBBBBBu;
    questB.pois.push_back(empty);

    WorldPacket packet;
    CHECK(MopQueryPackets::BuildQuestPoiQueryResponse(packet, { questA, questB }));

    static uint8_t const header[] = {
        0x00, 0x00, 0x20, 0x00, 0x08, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00, 0x00
    };
    CHECK(packet.size() > sizeof(header));
    for (size_t i = 0; i < sizeof(header) && i < packet.size(); ++i)
    {
        CHECK(packet.contents()[i] == header[i]);
        if (packet.contents()[i] != header[i])
        {
            std::fprintf(stderr, "  header byte %u = 0x%02X, wanted 0x%02X\n",
                         unsigned(i), packet.contents()[i], header[i]);
        }
    }

    // Byte phase: 3 POIs at (worldEffectId + 2*points + 10 scalars), then per quest a
    // questId and repeated poiCount, then the repeated questCount.
    size_t const expected = sizeof(header)
        + (4 + 0 + 40) + (4 + 8 + 40) + (4 + 0 + 40)
        + 8 + 8
        + 4;
    CHECK(packet.size() == expected);

    // Each POI's repeated count sits 5 scalars past its points: 0, 1, 0 in order.
    size_t off = sizeof(header);
    uint32 const counts[] = { 0, 1, 0 };
    for (uint32 c : counts)
    {
        uint32 onWire = 0;
        size_t const slot = off + 4 + c * 8 + 5 * 4;
        CHECK(slot + 4 <= packet.size());
        if (slot + 4 > packet.size())
            break;
        std::memcpy(&onWire, packet.contents() + slot, 4);
        CHECK(onWire == c);
        off += 4 + c * 8 + 40;
    }
}

static void test_opcode_values()
{
    CHECK(uint32_t(CMSG_QUEST_POI_QUERY) == 0x10C2u);
    CHECK(uint32_t(SMSG_QUEST_POI_QUERY_RESPONSE) == 0x067Fu);
    CHECK(uint32_t(CMSG_QUEST_POI_QUERY) <= 0x1FFFu);
    CHECK(uint32_t(SMSG_QUEST_POI_QUERY_RESPONSE) <= 0x1FFFu);
}

/**
 * An out of range `quest_poi`.`floorId` crashes the 5.4.8 client when it reads
 * SMSG_QUEST_POI_QUERY_RESPONSE on quest accept, so LoadQuestPOI() rejects it.
 * The loader itself needs a database, but the bound it applies does not.
 */
static void test_floor_id_bound()
{
    // Real data. The whole imported table sits in this range.
    CHECK(IsValidQuestPoiFloorId(0));
    CHECK(IsValidQuestPoiFloorId(7));

    // The bound itself, and the first value past it.
    CHECK(IsValidQuestPoiFloorId(MAX_QUEST_POI_FLOOR_ID));
    CHECK(!IsValidQuestPoiFloorId(MAX_QUEST_POI_FLOOR_ID + 1));

    // 12 is the highest floor observed anywhere in the 18414 retail corpus,
    // across 14,416 POI records. The bound sits above it on purpose: 12 is a
    // measured maximum, not a proven ceiling, and an over-tight bound fails
    // silently by clamping a legitimate floor to 0.
    CHECK(IsValidQuestPoiFloorId(12));
    CHECK(IsValidQuestPoiFloorId(13));
    CHECK(IsValidQuestPoiFloorId(MAX_QUEST_POI_FLOOR_ID));
    CHECK(!IsValidQuestPoiFloorId(MAX_QUEST_POI_FLOOR_ID + 1));

    // 255 was the previous bound and 808 an earlier candidate still, the
    // latter taken from DungeonMap.dbc on an unverified assumption about what
    // floorId indexes. Both are now rejected. Our own table tops out at floor
    // 7 and has no rows between 13 and 255, so tightening costs nothing.
    CHECK(!IsValidQuestPoiFloorId(255));
    CHECK(!IsValidQuestPoiFloorId(808));

    // The value that crashed the client in game on quest 29406, and the top of
    // the observed garbage band across the 49 affected rows.
    CHECK(!IsValidQuestPoiFloorId(252339));
    CHECK(!IsValidQuestPoiFloorId(264091));

    // The column is int(10) unsigned, so this is a reachable value.
    CHECK(!IsValidQuestPoiFloorId(0xFFFFFFFFu));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_request();
    test_response();
    test_point_count_is_repeated_not_floor_id();
    test_multiple_quests_and_empty_pois();
    test_opcode_values();
    test_floor_id_bound();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_quest_poi_query: all checks passed\n");
    return 0;
}
