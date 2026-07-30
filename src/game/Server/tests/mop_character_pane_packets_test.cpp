/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

/**
 * Byte-exact tests for directly verified 5.4.8 character-pane packets.
 */

#include "Player.h"
#include "CurrencyMgr.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void CheckBytes(WorldPacket const& packet,
    std::initializer_list<uint8_t> expected)
{
    CHECK(packet.size() == expected.size());
    size_t index = 0;
    for (uint8_t byte : expected)
    {
        if (index < packet.size())
            CHECK(packet[index] == byte);
        ++index;
    }
}

// These builders serialize packet bodies only. Opcode checks below prove that
// they preserve the caller's selection and cannot silently regress to calling
// Initialize themselves. Producer selection is pinned separately by the
// *_opcode mutations in mop_character_pane_packets_source_test.cmake.

static void TestSetFactionStandingCapturedBodies()
{
    std::vector<MopReputationPackets::Standing> one = {
        { 13029u, 118u },
    };

    // capture-000006 seq 9199
    WorldPacket onePacket(SMSG_SET_FACTION_STANDING, 19);
    MopReputationPackets::BuildSetFactionStanding(onePacket, true, one,
        0.0f, 0.0f);
    CHECK(onePacket.GetOpcode() == SMSG_SET_FACTION_STANDING);
    CheckBytes(onePacket, {
        0x80, 0x00, 0x04,
        0xE5, 0x32, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    });

    std::vector<MopReputationPackets::Standing> two = {
        { 7019u, 49u },
        { 8147u, 11u },
    };

    // capture-000044 seq 60000
    WorldPacket twoPacket(SMSG_SET_FACTION_STANDING, 27);
    MopReputationPackets::BuildSetFactionStanding(twoPacket, true, two,
        0.0f, 0.0f);
    CheckBytes(twoPacket, {
        0x80, 0x00, 0x08,
        0x6B, 0x1B, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00,
        0xD3, 0x1F, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    });

    std::vector<MopReputationPackets::Standing> seven = {
        { 11229u, 19u },
        { 3088u, 11u },
        { 4438u, 18u },
        { 3097u, 20u },
        { 3088u, 49u },
        { 3088u, 188u },
        { 4813u, 106u },
    };

    // capture-000133 seq 310166
    WorldPacket sevenPacket(SMSG_SET_FACTION_STANDING, 67);
    MopReputationPackets::BuildSetFactionStanding(sevenPacket, true, seven,
        0.0f, 0.0f);
    CheckBytes(sevenPacket, {
        0x80, 0x00, 0x1C,
        0xDD, 0x2B, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00,
        0x10, 0x0C, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00,
        0x56, 0x11, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
        0x19, 0x0C, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
        0x10, 0x0C, 0x00, 0x00, 0x31, 0x00, 0x00, 0x00,
        0x10, 0x0C, 0x00, 0x00, 0xBC, 0x00, 0x00, 0x00,
        0xCD, 0x12, 0x00, 0x00, 0x6A, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    });
}

static void TestSetFactionStandingFalseFlagAndBonuses()
{
    // Synthetic reader-derived complement: retail bodies never clear the
    // visual bit and carry only zero bonus floats.
    std::vector<MopReputationPackets::Standing> standings = {
        { 0x01020304u, 0xA1B2C3D4u },
    };

    WorldPacket packet(SMSG_SET_FACTION_STANDING, 19);
    MopReputationPackets::BuildSetFactionStanding(packet, false, standings,
        1.0f, -2.0f);
    CheckBytes(packet, {
        0x00, 0x00, 0x04,
        0x04, 0x03, 0x02, 0x01, 0xD4, 0xC3, 0xB2, 0xA1,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0xC0,
    });
}

static void TestSetFactionVisibleCapturedBody()
{
    // capture-000004 seq 78 and 29215. The body proves one uint32; the client
    // reader and server producer identify it as the reputation-list index.
    WorldPacket packet(SMSG_SET_FACTION_VISIBLE, 4);
    MopReputationPackets::BuildSetFactionVisible(packet, 114u);
    CHECK(packet.GetOpcode() == SMSG_SET_FACTION_VISIBLE);
    CheckBytes(packet, { 0x72, 0x00, 0x00, 0x00 });
}

static void TestSetCurrencyWeekLimitCapturedBodies()
{
    // capture-000026 seq 232. The client reader and the known currency IDs,
    // rather than body size alone, identify week-cap then currency-ID order.
    WorldPacket conquestA(SMSG_SET_CURRENCY_WEEK_LIMIT, 8);
    MopCurrencyPackets::BuildSetCurrencyWeekLimit(conquestA, 18200u, 390u);
    CHECK(conquestA.GetOpcode() == SMSG_SET_CURRENCY_WEEK_LIMIT);
    CheckBytes(conquestA, {
        0x18, 0x47, 0x00, 0x00, 0x86, 0x01, 0x00, 0x00,
    });

    // capture-000026 seq 233.
    WorldPacket conquestB(SMSG_SET_CURRENCY_WEEK_LIMIT, 8);
    MopCurrencyPackets::BuildSetCurrencyWeekLimit(conquestB, 17800u, 483u);
    CheckBytes(conquestB, {
        0x88, 0x45, 0x00, 0x00, 0xE3, 0x01, 0x00, 0x00,
    });
}

static void TestTitleUpdatePreservesCallerSelectedOpcodes()
{
    // capture-000069 seq 2062 (earned) and 2067 (lost): the same four-byte
    // Mask_ID body under distinct opcodes. The body cannot label the ID alone.
    WorldPacket earned(SMSG_TITLE_EARNED, 4);
    MopCharacterPanePackets::BuildTitleUpdate(earned, 224u);
    CHECK(earned.GetOpcode() == SMSG_TITLE_EARNED);
    CheckBytes(earned, { 0xE0, 0x00, 0x00, 0x00 });

    WorldPacket lost(SMSG_TITLE_LOST, 4);
    MopCharacterPanePackets::BuildTitleUpdate(lost, 224u);
    CHECK(lost.GetOpcode() == SMSG_TITLE_LOST);
    CheckBytes(lost, { 0xE0, 0x00, 0x00, 0x00 });
}

static void TestPvpCreditCapturedBodies()
{
    // capture-000072 seq 52521. The reader pins scalar/GUID order; naming the
    // first scalar as rank also uses its corpus distribution and UI strings.
    WorldPacket kill(SMSG_PVP_CREDIT, 17);
    MopCharacterPanePackets::BuildPvpCredit(kill, 0u, 184u,
        ObjectGuid(UINT64_C(0x07800000054D0489)));
    CHECK(kill.GetOpcode() == SMSG_PVP_CREDIT);
    CheckBytes(kill, {
        0x00, 0x00, 0x00, 0x00, 0xB8, 0x00, 0x00, 0x00,
        0x5F, 0x81, 0x06, 0x88, 0x05, 0x04, 0x4C,
    });

    // capture-000072 seq 64503: the zero-GUID honor-award form.
    WorldPacket award(SMSG_PVP_CREDIT, 9);
    MopCharacterPanePackets::BuildPvpCredit(award, 5u, 990u, ObjectGuid());
    CHECK(award.GetOpcode() == SMSG_PVP_CREDIT);
    CheckBytes(award, {
        0x05, 0x00, 0x00, 0x00, 0xDE, 0x03, 0x00, 0x00, 0x00,
    });
}

static void TestPvpCreditGuidGrammarComplements()
{
    // Synthetic reader-derived complements, not retail bodies. All nonzero
    // bytes pin the complete byte sequence; the single-zero family below pins
    // every mask position because an all-ones mask cannot discriminate order.
    WorldPacket allBytes(SMSG_PVP_CREDIT, 17);
    MopCharacterPanePackets::BuildPvpCredit(allBytes, 0u, 0u,
        ObjectGuid(UINT64_C(0x0807060504030201)));
    CheckBytes(allBytes, {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xFF, 0x06, 0x09, 0x07, 0x00, 0x03, 0x05, 0x04, 0x02,
    });

    struct MaskFixture
    {
        uint64 guid;
        uint8 expectedMask;
    };

    MaskFixture const masks[] = {
        { UINT64_C(0x0807060504030200), 0xF7 }, // byte 0 absent
        { UINT64_C(0x0807060504030001), 0xFD }, // byte 1 absent
        { UINT64_C(0x0807060504000201), 0xBF }, // byte 2 absent
        { UINT64_C(0x0807060500030201), 0xEF }, // byte 3 absent
        { UINT64_C(0x0807060004030201), 0x7F }, // byte 4 absent
        { UINT64_C(0x0807000504030201), 0xDF }, // byte 5 absent
        { UINT64_C(0x0800060504030201), 0xFB }, // byte 6 absent
        { UINT64_C(0x0007060504030201), 0xFE }, // byte 7 absent
    };

    for (MaskFixture const& fixture : masks)
    {
        WorldPacket packet(SMSG_PVP_CREDIT, 16);
        MopCharacterPanePackets::BuildPvpCredit(packet, 0u, 0u,
            ObjectGuid(fixture.guid));
        CHECK(packet.size() == 16);
        if (packet.size() > 8)
            CHECK(packet[8] == fixture.expectedMask);
    }
}

static void TestCrossedInebriationCapturedBodies()
{
    struct Fixture
    {
        uint64 guid;
        uint32 itemId;
        uint32 drunkState;
        std::initializer_list<uint8_t> body;
    };

    Fixture const fixtures[] = {
        // capture-000666 seq 234677
        {
            UINT64_C(0x04000000051C0030), 21171u, 1u,
            { 0xA3, 0x04, 0xB3, 0x52, 0x00, 0x00, 0x01, 0x00,
              0x00, 0x00, 0x05, 0x31, 0x1D },
        },
        // capture-000059 seq 475305
        {
            UINT64_C(0x0400000005823240), 0u, 1u,
            { 0xA7, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
              0x00, 0x00, 0x05, 0x41, 0x83, 0x33 },
        },
        // capture-000146 seq 428095
        {
            UINT64_C(0x0180000004E86C30), 107499u, 1u,
            { 0xB7, 0x05, 0xEB, 0xA3, 0x01, 0x00, 0x01, 0x00,
              0x00, 0x00, 0x81, 0x00, 0x31, 0xE9, 0x6D },
        },
    };

    for (Fixture const& fixture : fixtures)
    {
        WorldPacket packet(SMSG_CROSSED_INEBRIATION_THRESHOLD, 17);
        MopCharacterPanePackets::BuildCrossedInebriationThreshold(packet,
            ObjectGuid(fixture.guid), fixture.itemId, fixture.drunkState);
        CHECK(packet.GetOpcode() == SMSG_CROSSED_INEBRIATION_THRESHOLD);
        CheckBytes(packet, fixture.body);
    }
}

static void TestCrossedInebriationGuidGrammarComplements()
{
    // Synthetic reader-derived complements, not retail bodies. All nonzero
    // bytes pin the full interleave; the single-zero family pins mask order.
    WorldPacket allBytes(SMSG_CROSSED_INEBRIATION_THRESHOLD, 17);
    MopCharacterPanePackets::BuildCrossedInebriationThreshold(allBytes,
        ObjectGuid(UINT64_C(0x0807060504030201)), 0x11223344u, 0x55667788u);
    CheckBytes(allBytes, {
        0xFF, 0x05,
        0x44, 0x33, 0x22, 0x11, 0x88, 0x77, 0x66, 0x55,
        0x04, 0x06, 0x09, 0x00, 0x02, 0x07, 0x03,
    });

    struct MaskFixture
    {
        uint64 guid;
        uint8 expectedMask;
    };

    MaskFixture const masks[] = {
        { UINT64_C(0x0807060504030200), 0x7F }, // byte 0 absent
        { UINT64_C(0x0807060504030001), 0xFB }, // byte 1 absent
        { UINT64_C(0x0807060504000201), 0xDF }, // byte 2 absent
        { UINT64_C(0x0807060500030201), 0xFD }, // byte 3 absent
        { UINT64_C(0x0807060004030201), 0xBF }, // byte 4 absent
        { UINT64_C(0x0807000504030201), 0xF7 }, // byte 5 absent
        { UINT64_C(0x0800060504030201), 0xEF }, // byte 6 absent
        { UINT64_C(0x0007060504030201), 0xFE }, // byte 7 absent
    };

    for (MaskFixture const& fixture : masks)
    {
        WorldPacket packet(SMSG_CROSSED_INEBRIATION_THRESHOLD, 16);
        MopCharacterPanePackets::BuildCrossedInebriationThreshold(packet,
            ObjectGuid(fixture.guid), 0u, 0u);
        CHECK(packet.size() == 16);
        if (!packet.empty())
            CHECK(packet[0] == fixture.expectedMask);
    }
}

int main()
{
    TestSetFactionStandingCapturedBodies();
    TestSetFactionStandingFalseFlagAndBonuses();
    TestSetFactionVisibleCapturedBody();
    TestSetCurrencyWeekLimitCapturedBodies();
    TestTitleUpdatePreservesCallerSelectedOpcodes();
    TestPvpCreditCapturedBodies();
    TestPvpCreditGuidGrammarComplements();
    TestCrossedInebriationCapturedBodies();
    TestCrossedInebriationGuidGrammarComplements();
    if (g_fail != 0)
        return 1;

    std::printf("mop_character_pane_packets: all checks passed\n");
    return 0;
}
