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

/**
 * Byte-exact tests for the ten regular 5.4.8 initial UI packet bodies.
 */

#include "Player.h"
#include "Item.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool ExpectBytes(WorldPacket const& packet, std::vector<uint8_t> const& expected)
{
    if (packet.size() != expected.size())
    {
        std::fprintf(stderr, "  size %u, wanted %u\n", unsigned(packet.size()), unsigned(expected.size()));
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

static uint64_t Fnv1a64(WorldPacket const& packet)
{
    uint64_t hash = UINT64_C(0xcbf29ce484222325);
    for (size_t i = 0; i < packet.size(); ++i)
    {
        hash ^= packet.contents()[i];
        hash *= UINT64_C(0x100000001b3);
    }
    return hash;
}

static void test_initial_spells()
{
    {
        WorldPacket packet(SMSG_INITIAL_SPELLS, 3);
        MopInitialPackets::BuildInitialSpells(packet, {});
        CHECK(ExpectBytes(packet, { 0x00, 0x00, 0x00 }));
    }
    {
        WorldPacket packet(SMSG_INITIAL_SPELLS, 11);
        MopInitialPackets::BuildInitialSpells(packet, { 0x11223344u, 0xAABBCCDDu });
        CHECK(ExpectBytes(packet, {
            0x00, 0x00, 0x04,
            0x44, 0x33, 0x22, 0x11, 0xDD, 0xCC, 0xBB, 0xAA
        }));
    }
}

static void test_action_buttons()
{
    {
        std::array<MopInitialPackets::ActionButton, MopInitialPackets::ACTION_BUTTON_COUNT> buttons{};
        WorldPacket packet(SMSG_ACTION_BUTTONS, 133);
        MopInitialPackets::BuildActionButtons(packet, buttons, 1);
        std::vector<uint8_t> expected(133, 0);
        expected.back() = 1;
        CHECK(ExpectBytes(packet, expected));
    }
    {
        std::array<MopInitialPackets::ActionButton, MopInitialPackets::ACTION_BUTTON_COUNT> buttons{};
        buttons[0] = { 0x00112233u, 0x44u };
        buttons[131] = { 0x00A1B2C3u, 0xD4u };

        WorldPacket packet(SMSG_ACTION_BUTTONS, 141);
        MopInitialPackets::BuildActionButtons(packet, buttons, 1);

        CHECK(packet.size() == 141);
        CHECK(Fnv1a64(packet) == UINT64_C(0xc9ed25616a200ea3));
        CHECK(packet.contents()[0] == 0x00);
        CHECK(packet.contents()[49] == 0x08);
        CHECK(packet.contents()[65] == 0x01);
        CHECK(packet.contents()[82] == 0x08);
        CHECK(packet.contents()[98] == 0x01);
        CHECK(packet.contents()[99] == 0x80);
        CHECK(packet.contents()[115] == 0x18);
        CHECK(packet.contents()[131] == 0x01);
        CHECK(packet.contents()[132] == 0x32);
        CHECK(packet.contents()[133] == 0xC2);
        CHECK(packet.contents()[134] == 0x23);
        CHECK(packet.contents()[135] == 0xB3);
        CHECK(packet.contents()[136] == 0x45);
        CHECK(packet.contents()[137] == 0xD5);
        CHECK(packet.contents()[138] == 0x10);
        CHECK(packet.contents()[139] == 0xA0);
        CHECK(packet.contents()[140] == 0x01);
    }
}

static void test_bind_point_update()
{
    WorldPacket packet(SMSG_BINDPOINTUPDATE, 20);
    MopInitialPackets::BuildBindPointUpdate(packet, 1.0f, 2.0f, 3.0f, 0x11223344u, 0x55667788u);
    CHECK(ExpectBytes(packet, {
        0x00, 0x00, 0x80, 0x3F, 0x00, 0x00, 0x00, 0x40,
        0x00, 0x00, 0x40, 0x40, 0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55
    }));
}

static void test_set_proficiency()
{
    WorldPacket packet(SMSG_SET_PROFICIENCY, 5);
    MopInitialPackets::BuildSetProficiency(packet, 0xA1B2C3D4u, 0x55u);
    CHECK(ExpectBytes(packet, { 0xD4, 0xC3, 0xB2, 0xA1, 0x55 }));
}

static void test_weather()
{
    {
        WorldPacket packet(SMSG_WEATHER, 9);
        MopInitialPackets::BuildWeather(packet, 0x11223344u, 1.5f, false);
        CHECK(ExpectBytes(packet, {
            0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0xC0, 0x3F, 0x00
        }));
    }
    {
        WorldPacket packet(SMSG_WEATHER, 9);
        MopInitialPackets::BuildWeather(packet, 0x11223344u, 1.5f, true);
        CHECK(ExpectBytes(packet, {
            0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0xC0, 0x3F, 0x80
        }));
    }

    // The cases above use invented values, which pin the encoding but say nothing
    // about whether the field order matches 18414. These three are retail bytes.
    // All 2,891 corpus observations of this opcode are exactly 9 bytes.

    // capture-000004 seq 99: the quiescent case, repeated identically at seq 29236
    // and seq 48593 -- state 1 at zero intensity.
    {
        WorldPacket packet(SMSG_WEATHER, 9);
        MopInitialPackets::BuildWeather(packet, 1u, 0.0f, false);
        CHECK(ExpectBytes(packet, {
            0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        }));
    }

    // capture-000044 seq 63233: state 3 at half intensity. A zero intensity cannot
    // distinguish state/intensity/bit from state/bit/intensity, because both orders
    // produce the same nine bytes. A nonzero one can: this fixes the intensity
    // argument at bytes 4..7 in little-endian IEEE-754, 0x3F000000 being 0.5f.
    // It is not on its own proof that the field is declared float -- only that the
    // argument is encoded there that way.
    {
        WorldPacket packet(SMSG_WEATHER, 9);
        MopInitialPackets::BuildWeather(packet, 3u, 0.5f, false);
        CHECK(ExpectBytes(packet, {
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x00
        }));
    }

    // capture-000075 seq 888845: the same state and intensity with the trailing
    // bit set, so both polarities of that byte-aligned bit are covered by retail
    // data rather than by construction.
    {
        WorldPacket packet(SMSG_WEATHER, 9);
        MopInitialPackets::BuildWeather(packet, 3u, 0.5f, true);
        CHECK(ExpectBytes(packet, {
            0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0x80
        }));
    }
}

int main(int /*argc*/, char** /*argv*/)
{
    test_initial_spells();
    test_action_buttons();
    test_bind_point_update();
    test_set_proficiency();
    test_weather();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_initial_packets: all checks passed\n");
    return 0;
}
