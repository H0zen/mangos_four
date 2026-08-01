/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Packet-level regression for Transport::UpdateForMap's shared read cursor.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

char const* LookupOpcodeName(PacketDirection, uint16)
{
    return "TEST_TRANSPORT_UPDATE_FOR_MAP";
}

int main(int /*argc*/, char** /*argv*/)
{
    uint16 const recipientMap = 0x1234;
    WorldPacket packet(SMSG_UPDATE_OBJECT, sizeof(uint16) + sizeof(uint32));
    packet << recipientMap;
    packet << uint32(1);

    CHECK(packet.rpos() == 0);
    for (uint32 observer = 0; observer < 2; ++observer)
    {
        (void)observer;
        CHECK(packet.read<uint16>(0) == recipientMap);
        CHECK(packet.rpos() == 0);
    }

    CHECK(packet.ReadUInt16() == recipientMap);
    CHECK(packet.ReadUInt16() != recipientMap);

    return g_failures;
}
