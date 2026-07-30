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

static void TestSetFactionStandingCapturedBodies()
{
    std::vector<MopReputationPackets::Standing> one = {
        { 13029u, 118u },
    };

    WorldPacket onePacket;
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

    WorldPacket twoPacket;
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

    WorldPacket sevenPacket;
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
    std::vector<MopReputationPackets::Standing> standings = {
        { 0x01020304u, 0xA1B2C3D4u },
    };

    WorldPacket packet;
    MopReputationPackets::BuildSetFactionStanding(packet, false, standings,
        1.0f, -2.0f);
    CheckBytes(packet, {
        0x00, 0x00, 0x04,
        0x04, 0x03, 0x02, 0x01, 0xD4, 0xC3, 0xB2, 0xA1,
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0xC0,
    });
}

static void TestTitleUpdateUsesOpcodeForEarnedAndLost()
{
    WorldPacket earned;
    MopCharacterPanePackets::BuildTitleUpdate(earned, 224u, false);
    CHECK(earned.GetOpcode() == SMSG_TITLE_EARNED);
    CheckBytes(earned, { 0xE0, 0x00, 0x00, 0x00 });

    WorldPacket lost;
    MopCharacterPanePackets::BuildTitleUpdate(lost, 224u, true);
    CHECK(lost.GetOpcode() == SMSG_TITLE_LOST);
    CheckBytes(lost, { 0xE0, 0x00, 0x00, 0x00 });
}

static void TestPvpCreditCapturedBodies()
{
    WorldPacket kill;
    MopCharacterPanePackets::BuildPvpCredit(kill, 0u, 184u,
        ObjectGuid(UINT64_C(0x07800000054D0489)));
    CHECK(kill.GetOpcode() == SMSG_PVP_CREDIT);
    CheckBytes(kill, {
        0x00, 0x00, 0x00, 0x00, 0xB8, 0x00, 0x00, 0x00,
        0x5F, 0x81, 0x06, 0x88, 0x05, 0x04, 0x4C,
    });

    WorldPacket award;
    MopCharacterPanePackets::BuildPvpCredit(award, 5u, 990u, ObjectGuid());
    CheckBytes(award, {
        0x05, 0x00, 0x00, 0x00, 0xDE, 0x03, 0x00, 0x00, 0x00,
    });
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
        {
            UINT64_C(0x04000000051C0030), 21171u, 1u,
            { 0xA3, 0x04, 0xB3, 0x52, 0x00, 0x00, 0x01, 0x00,
              0x00, 0x00, 0x05, 0x31, 0x1D },
        },
        {
            UINT64_C(0x0400000005823240), 0u, 1u,
            { 0xA7, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
              0x00, 0x00, 0x05, 0x41, 0x83, 0x33 },
        },
        {
            UINT64_C(0x0180000004E86C30), 107499u, 1u,
            { 0xB7, 0x05, 0xEB, 0xA3, 0x01, 0x00, 0x01, 0x00,
              0x00, 0x00, 0x81, 0x00, 0x31, 0xE9, 0x6D },
        },
    };

    for (Fixture const& fixture : fixtures)
    {
        WorldPacket packet;
        MopCharacterPanePackets::BuildCrossedInebriationThreshold(packet,
            ObjectGuid(fixture.guid), fixture.itemId, fixture.drunkState);
        CHECK(packet.GetOpcode() == SMSG_CROSSED_INEBRIATION_THRESHOLD);
        CheckBytes(packet, fixture.body);
    }
}

int main()
{
    TestSetFactionStandingCapturedBodies();
    TestSetFactionStandingFalseFlagAndBonuses();
    TestTitleUpdateUsesOpcodeForEarnedAndLost();
    TestPvpCreditCapturedBodies();
    TestCrossedInebriationCapturedBodies();
    if (g_fail != 0)
        return 1;

    std::printf("mop_character_pane_packets: all checks passed\n");
    return 0;
}
