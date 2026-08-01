/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "MopNotificationPackets.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
        return false;

    for (size_t i = 0; i < expected.size(); ++i)
        if (packet.contents()[i] != expected[i])
            return false;

    return true;
}

static void CheckAccepted(std::string const& text, uint8_t first, uint8_t second)
{
    WorldPacket packet;
    CHECK(MopNotificationPackets::Build(packet, text));
    CHECK(packet.GetOpcode() == SMSG_NOTIFICATION);
    CHECK(packet.size() == 2 + text.size());
    CHECK(packet.contents()[0] == first);
    CHECK(packet.contents()[1] == second);

    for (size_t index = 0; index < text.size(); ++index)
        CHECK(packet.contents()[index + 2] == uint8_t(text[index]));
}

static void CheckRejected(std::string const& text)
{
    uint8_t const sentinel[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    WorldPacket packet(0x0777, sizeof(sentinel));
    packet.append(sentinel, sizeof(sentinel));

    CHECK(!MopNotificationPackets::Build(packet, text));
    CHECK(packet.GetOpcode() == 0x0777);
    CHECK(Equal(packet, { 0xDE, 0xAD, 0xBE, 0xEF }));
}

int main(int, char**)
{
    CHECK(uint32(SMSG_NOTIFICATION) == 0x0C2Au);

    CheckAccepted("", 0x00, 0x00);
    CheckAccepted("A", 0x00, 0x10);
    CheckAccepted("abc", 0x00, 0x30);
    CheckAccepted(std::string(15, 'X'), 0x00, 0xF0);
    CheckAccepted(std::string(16, 'X'), 0x01, 0x00);
    CheckAccepted(std::string(255, 'X'), 0x0F, 0xF0);
    CheckAccepted(std::string(256, 'X'), 0x10, 0x00);
    CheckAccepted(std::string("\xE2\x82\xAC", 3), 0x00, 0x30);
    CheckAccepted(std::string(1023, 'X'), 0x3F, 0xF0);

    CheckRejected(std::string(1024, 'X'));
    CheckRejected(std::string(4095, 'X'));
    CheckRejected(std::string(4096, 'X'));
    CheckRejected(std::string("\0A", 2));
    CheckRejected(std::string("A\0B", 3));
    CheckRejected(std::string("A\0", 2));

    std::string nulAtBoundary(1023, 'X');
    nulAtBoundary[511] = '\0';
    CheckRejected(nulAtBoundary);

    std::string multibyte1024;
    multibyte1024.reserve(1024);
    for (size_t index = 0; index < 512; ++index)
        multibyte1024.append("\xC2\xA2", 2);
    CheckRejected(multibyte1024);

    return g_fail ? 1 : 0;
}
