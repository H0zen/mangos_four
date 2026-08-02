/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 sound-notification packet fixtures.
 */

#include "Object.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void check_packet(WorldPacket const& packet, uint16 opcode,
    uint8 const* expected, size_t size)
{
    CHECK(packet.GetOpcode() == opcode);
    CHECK(packet.size() == size);
    CHECK(size == 0 || std::memcmp(packet.contents(), expected, size) == 0);
}

static void test_direct_sound_dense_guid()
{
    WorldPacket packet;
    MopSoundPackets::BuildPlaySound(
        packet, 0x11223344, ObjectGuid(uint64(0x0807060504030201ULL)));

    static uint8 const expected[] = {
        0xFF,
        0x44, 0x33, 0x22, 0x11,
        0x05, 0x02, 0x04, 0x09, 0x07, 0x00, 0x06, 0x03,
    };
    check_packet(packet, SMSG_PLAY_SOUND, expected, sizeof(expected));
}

static void test_direct_sound_without_source()
{
    WorldPacket packet;
    MopSoundPackets::BuildPlaySound(packet, 0x11223344, ObjectGuid());

    static uint8 const expected[] = {0x00, 0x44, 0x33, 0x22, 0x11};
    check_packet(packet, SMSG_PLAY_SOUND, expected, sizeof(expected));
}

static void test_object_sound_dense_guids()
{
    WorldPacket packet;
    MopSoundPackets::BuildPlayObjectSound(
        packet, 0x11223344, ObjectGuid(uint64(0x0807060504030201ULL)),
        ObjectGuid(uint64(0x1817161514131211ULL)));

    static uint8 const expected[] = {
        0xFF, 0xFF,
        0x06, 0x02, 0x12, 0x17, 0x09, 0x07, 0x05, 0x03, 0x15, 0x13,
        0x44, 0x33, 0x22, 0x11,
        0x04, 0x14, 0x19, 0x10, 0x16, 0x00,
    };
    check_packet(packet, SMSG_PLAY_OBJECT_SOUND, expected, sizeof(expected));
}

static void test_music()
{
    WorldPacket packet;
    MopSoundPackets::BuildPlayMusic(packet, 0x11223344);

    static uint8 const expected[] = {0x44, 0x33, 0x22, 0x11};
    check_packet(packet, SMSG_PLAY_MUSIC, expected, sizeof(expected));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_direct_sound_dense_guid();
    test_direct_sound_without_source();
    test_object_sound_dense_guids();
    test_music();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_sound_packets: all checks passed\n");
    return 0;
}
