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
 * Independent reader-inverse fixtures for the 5.4.8.18414 combat-log packets.
 */

#include "Spell.h"
#include "MopWireCodec.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

class RefWriter
{
public:
    void Bit(bool value)
    {
        if (m_bit == 8)
        {
            m_bytes.push_back(0);
            m_bit = 0;
        }
        if (value)
            m_bytes.back() |= uint8(0x80u >> m_bit);
        ++m_bit;
    }

    void Bits(uint32 value, uint8 count)
    {
        for (uint8 i = 0; i < count; ++i)
            Bit((value & (uint32(1) << (count - i - 1))) != 0);
    }

    void Align() { m_bit = 8; }

    void U32(uint32 value)
    {
        Align();
        for (uint8 i = 0; i < 4; ++i)
            m_bytes.push_back(uint8(value >> (8 * i)));
    }

    void F32(float value)
    {
        uint32 raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        U32(raw);
    }

    void U8(uint8 value) { Align(); m_bytes.push_back(value); }

    void GuidBit(uint64 guid, uint8 index) { Bit(GuidByteValue(guid, index) != 0); }

    void GuidByte(uint64 guid, uint8 index)
    {
        uint8 const value = GuidByteValue(guid, index);
        if (value)
            U8(value ^ 1);
    }

    std::vector<uint8> const& Bytes() const { return m_bytes; }

private:
    static uint8 GuidByteValue(uint64 guid, uint8 index) { return uint8(guid >> (8 * index)); }

    std::vector<uint8> m_bytes;
    uint8 m_bit = 8;
};

static void GuidBits(RefWriter& writer, uint64 guid, uint8 const* order, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        writer.GuidBit(guid, order[i]);
}


static std::vector<uint8> ExpectedInstakill(MopCombatLogPackets::SpellInstakillLog const& log)
{
    static uint8 const casterMaskA[] = { 6 };
    static uint8 const victimMaskA[] = { 0 };
    static uint8 const casterMaskB[] = { 7 };
    static uint8 const victimMaskB[] = { 2 };
    static uint8 const casterMaskC[] = { 3, 1, 2, 0, 4 };
    static uint8 const victimMaskC[] = { 4, 7, 1, 6, 5 };
    static uint8 const casterMaskD[] = { 5 };
    static uint8 const victimMaskD[] = { 3 };
    static uint8 const bytesA[][2] = {
        { 0, 0 }, { 1, 1 }, { 3, 0 }, { 4, 0 }, { 5, 0 }, { 7, 0 },
        { 0, 1 }, { 6, 0 }, { 2, 1 }, { 4, 1 }, { 1, 0 }
    };
    static uint8 const bytesB[][2] = {
        { 3, 1 }, { 2, 0 }, { 7, 1 }, { 6, 1 }, { 5, 1 }
    };

    RefWriter writer;
    GuidBits(writer, log.casterGuid, casterMaskA, 1);
    GuidBits(writer, log.victimGuid, victimMaskA, 1);
    GuidBits(writer, log.casterGuid, casterMaskB, 1);
    GuidBits(writer, log.victimGuid, victimMaskB, 1);
    GuidBits(writer, log.casterGuid, casterMaskC, 5);
    GuidBits(writer, log.victimGuid, victimMaskC, 5);
    GuidBits(writer, log.casterGuid, casterMaskD, 1);
    GuidBits(writer, log.victimGuid, victimMaskD, 1);
    writer.Align();
    for (auto const& byte : bytesA)
        writer.GuidByte(byte[1] ? log.victimGuid : log.casterGuid, byte[0]);
    writer.U32(log.spellId);
    for (auto const& byte : bytesB)
        writer.GuidByte(byte[1] ? log.victimGuid : log.casterGuid, byte[0]);
    return writer.Bytes();
}



static std::vector<uint8> ExpectedDamageShield(MopCombatLogPackets::SpellDamageShieldLog const& log)
{
    static uint8 const mask[][2] = {
        { 1, 1 }, { 2, 0 }, { 6, 0 }, { 3, 1 }, { 4, 0 }, { 2, 1 },
        { 5, 1 }, { 6, 1 }, { 3, 0 }, { 0, 1 }, { 5, 0 }, { 1, 0 },
        { 0, 0 }, { 7, 1 }, { 4, 1 }, { 7, 0 }
    };

    RefWriter writer;
    for (size_t i = 0; i < 2; ++i)
        writer.GuidBit(mask[i][1] ? log.targetGuid : log.casterGuid, mask[i][0]);
    writer.Bit(false); // no optional spell-cast-log data
    for (size_t i = 2; i < 16; ++i)
        writer.GuidBit(mask[i][1] ? log.targetGuid : log.casterGuid, mask[i][0]);
    writer.Align();
    writer.GuidByte(log.targetGuid, 2);
    writer.GuidByte(log.casterGuid, 6);
    writer.GuidByte(log.targetGuid, 6);
    writer.GuidByte(log.targetGuid, 4);
    writer.GuidByte(log.casterGuid, 3);
    writer.GuidByte(log.targetGuid, 7);
    writer.U32(log.resist);
    writer.GuidByte(log.casterGuid, 4);
    writer.GuidByte(log.targetGuid, 1);
    writer.U32(log.damage);
    writer.GuidByte(log.casterGuid, 7);
    writer.U32(log.spellId);
    writer.U32(log.overkill);
    writer.GuidByte(log.targetGuid, 5);
    writer.GuidByte(log.casterGuid, 5);
    writer.GuidByte(log.targetGuid, 0);
    writer.GuidByte(log.casterGuid, 1);
    writer.GuidByte(log.casterGuid, 0);
    writer.GuidByte(log.casterGuid, 2);
    writer.U32(log.schoolMask);
    writer.GuidByte(log.targetGuid, 3);
    return writer.Bytes();
}






static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    if (packet.size() != expected.size())
        return false;
    return std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}



static void test_spell_instakill_log()
{
    MopCombatLogPackets::SpellInstakillLog dense = {};
    dense.casterGuid = 0x0123456789ABCDEFull;
    dense.victimGuid = 0xF1E2D3C4B5A69788ull;
    dense.spellId = 0x11223344u;

    WorldPacket densePacket(SMSG_SPELLINSTAKILLLOG, 24);
    MopCombatLogPackets::BuildSpellInstakillLog(densePacket, dense);
    CHECK(densePacket.GetOpcode() == SMSG_SPELLINSTAKILLLOG);
    CHECK(Equal(densePacket, ExpectedInstakill(dense)));

    MopCombatLogPackets::SpellInstakillLog sparse = {};
    sparse.casterGuid = 0x0002000400060008ull;
    sparse.victimGuid = 0x0100030005000700ull;
    sparse.spellId = 5;

    WorldPacket sparsePacket(SMSG_SPELLINSTAKILLLOG, 24);
    MopCombatLogPackets::BuildSpellInstakillLog(sparsePacket, sparse);
    CHECK(Equal(sparsePacket, ExpectedInstakill(sparse)));
}



static void test_spell_damage_shield_log()
{
    MopCombatLogPackets::SpellDamageShieldLog dense = {};
    dense.casterGuid = 0x0123456789ABCDEFull;
    dense.targetGuid = 0xF1E2D3C4B5A69788ull;
    dense.spellId = 0x11223344u;
    dense.damage = 0x55667788u;
    dense.overkill = 0x01020304u;
    dense.schoolMask = 0xA1A2A3A4u;
    dense.resist = 0x05060708u;

    WorldPacket densePacket(SMSG_SPELLDAMAGESHIELD, 40);
    MopCombatLogPackets::BuildSpellDamageShieldLog(densePacket, dense);
    CHECK(densePacket.GetOpcode() == SMSG_SPELLDAMAGESHIELD);
    CHECK(Equal(densePacket, ExpectedDamageShield(dense)));

    MopCombatLogPackets::SpellDamageShieldLog sparse = {};
    sparse.casterGuid = 0x0002000400060008ull;
    sparse.targetGuid = 0x0100030005000700ull;
    sparse.spellId = 1;
    sparse.damage = 2;
    sparse.schoolMask = 4;

    WorldPacket sparsePacket(SMSG_SPELLDAMAGESHIELD, 40);
    MopCombatLogPackets::BuildSpellDamageShieldLog(sparsePacket, sparse);
    CHECK(Equal(sparsePacket, ExpectedDamageShield(sparse)));
}









int main(int, char**)
{
    test_spell_instakill_log();
    test_spell_damage_shield_log();
    if (g_fail) return 1;
    std::printf("mop_combat_log_packets: all checks passed\n");
    return 0;
}
