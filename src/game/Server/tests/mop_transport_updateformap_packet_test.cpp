/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Packet-level regression for the 18414 SMSG_UPDATE_OBJECT header, which is what every
 * vessel-related update packet is stamped from.
 *
 * The body opens with a uint16 map id, and 18414 discards a block addressed to a map the client
 * is not on. A passenger's server-side map is the hull -- an id the client was never told -- so
 * TransportMap's packets are stamped with Player::GetClientMapId(), the world map the ship
 * sails. This fixture pins the header layout that requirement rests on, and the absolute-offset
 * read that lets one built packet be inspected for several recipients without a shared cursor
 * walking forward each time.
 */

#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

char const* LookupOpcodeName(PacketDirection, uint16)
{
    return "TEST_TRANSPORT_UPDATE_OBJECT_HEADER";
}

int main(int /*argc*/, char** /*argv*/)
{
    // The world map a vessel sails, never the hull's own minted id.
    uint16 const sailedMap = 0x1234;
    WorldPacket packet(SMSG_UPDATE_OBJECT, sizeof(uint16) + sizeof(uint32));
    packet << sailedMap;
    packet << uint32(1);

    CHECK(packet.size() == sizeof(uint16) + sizeof(uint32));
    CHECK(packet.rpos() == 0);

    for (uint32 observer = 0; observer < 2; ++observer)
    {
        (void)observer;
        CHECK(packet.read<uint16>(0) == sailedMap);
        CHECK(packet.read<uint32>(sizeof(uint16)) == 1);
        CHECK(packet.rpos() == 0);
    }

    CHECK(packet.ReadUInt16() == sailedMap);
    CHECK(packet.ReadUInt16() != sailedMap);

    return g_failures;
}
