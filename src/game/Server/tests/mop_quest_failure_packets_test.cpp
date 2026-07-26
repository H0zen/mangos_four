/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 quest-failure packet fixtures.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void check_packet(WorldPacket const& packet, uint32 opcode, uint8 const* expected, size_t size)
{
    CHECK(uint32(packet.GetOpcode()) == opcode);
    CHECK(packet.size() == size);
    CHECK(size == 0 || std::memcmp(packet.contents(), expected, size) == 0);
}

static void test_quest_invalid_has_absent_string_then_reason()
{
    WorldPacket packet(SMSG_QUESTGIVER_QUEST_INVALID, 5);
    packet.WriteBit(true); // 18414 nullable-string marker: custom message absent
    packet << uint32(0x11223344u);

    static uint8 const expected[] = { 0x80, 0x44, 0x33, 0x22, 0x11 };
    check_packet(packet, 0x027Du, expected, sizeof(expected));
}

static void test_quest_failed_has_quest_then_reason()
{
    WorldPacket packet(SMSG_QUESTGIVER_QUEST_FAILED, 8);
    packet << uint32(0x11223344u);
    packet << uint32(0x55667788u);

    static uint8 const expected[] = {
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
    };
    check_packet(packet, 0x12DEu, expected, sizeof(expected));
}

static void test_quest_log_full_is_empty()
{
    WorldPacket packet(SMSG_QUESTLOG_FULL, 0);
    check_packet(packet, 0x07FDu, nullptr, 0);
}

static void test_quest_failed_timer_has_quest_id()
{
    WorldPacket packet(SMSG_QUESTUPDATE_FAILEDTIMER, 4);
    packet << uint32(0x11223344u);

    static uint8 const expected[] = { 0x44, 0x33, 0x22, 0x11 };
    check_packet(packet, 0x06FFu, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_quest_invalid_has_absent_string_then_reason();
    test_quest_failed_has_quest_then_reason();
    test_quest_log_full_is_empty();
    test_quest_failed_timer_has_quest_id();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_quest_failure_packets: all checks passed\n");
    return 0;
}
