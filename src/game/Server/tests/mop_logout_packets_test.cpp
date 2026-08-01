/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Byte-exact MaNGOS 5.4.8 build-18414 logout packet fixtures.
 */

#include "MopLogoutPackets.h"

#include <cstdint>
#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::initializer_list<uint8_t> expected)
{
    if (packet.size() != expected.size())
        return false;

    size_t index = 0;
    for (uint8_t byte : expected)
    {
        if (packet.contents()[index++] != byte)
            return false;
    }

    return true;
}

static WorldPacket SeededPacket()
{
    WorldPacket packet(0x0777, 4);
    uint8_t const sentinel[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    packet.append(sentinel, sizeof(sentinel));
    return packet;
}

static void CheckResponse(uint32 reason, bool instant, std::initializer_list<uint8_t> expected)
{
    WorldPacket packet = SeededPacket();
    MopLogoutPackets::BuildResponse(packet, reason, instant);

    CHECK(packet.GetOpcode() == SMSG_LOGOUT_RESPONSE);
    CHECK(Equal(packet, expected));
}

static void TestResponses()
{
    CheckResponse(0, false, { 0x00, 0x00, 0x00, 0x00, 0x00 });
    CheckResponse(0, true, { 0x00, 0x00, 0x00, 0x00, 0x80 });
    CheckResponse(0x01020304u, false, { 0x04, 0x03, 0x02, 0x01, 0x00 });
    CheckResponse(3, true, { 0x03, 0x00, 0x00, 0x00, 0x80 });
}

static void TestCancelAck()
{
    WorldPacket packet = SeededPacket();
    MopLogoutPackets::BuildCancelAck(packet);

    CHECK(packet.GetOpcode() == SMSG_LOGOUT_CANCEL_ACK);
    CHECK(packet.empty());
}

static void TestComplete()
{
    WorldPacket packet = SeededPacket();
    MopLogoutPackets::BuildComplete(packet);

    CHECK(packet.GetOpcode() == SMSG_LOGOUT_COMPLETE);
    CHECK(Equal(packet, { 0x80, 0x00 }));
}

int main(int, char**)
{
    TestResponses();
    TestCancelAck();
    TestComplete();
    return g_fail ? 1 : 0;
}
