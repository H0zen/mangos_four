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









// Byte-exact fixture against REAL captured retail traffic, not against a
// reimplementation of our own writer. Source packet: build 18414,
// capture-000075 sequence 1746844, the 38-byte corpus minimum for
// SMSG_SPELLNONMELEEDAMAGELOG (opcode 0x1450, 5,272,845 packets observed).
// Corpus generation 2BE10C899585BAECD237705AC13BBF9262D81B6BDC085B462808C6869CE88752.
//
// Field values were recovered by decoding those bytes through the client reader
// at Wow.exe sub_C75861. Both GUIDs are equal in this sample, which is simply
// what the capture contains.
static void test_spell_non_melee_damage_log_matches_capture()
{
    static uint8 const captured[] = {
        0xD0, 0x84, 0x80,                               // packed GUID presence mask
        0x00, 0x00, 0x00, 0x00,                         // blocked = 0
        0xA1,                                           // attacker[1] = 0xA0 ^ 1
        0xFF, 0xFF, 0xFF, 0xFF,                         // overkill = -1, not lethal
        0x05,                                           // attacker[7] = 0x04 ^ 1
        0x00, 0x00, 0x00, 0x00,                         // resist = 0
        0x00, 0x00, 0x00, 0x00,                         // absorb = 0
        0x97,                                           // attacker[2] = 0x96 ^ 1
        0x97,                                           // target[2]   = 0x96 ^ 1
        0x1E, 0x0F, 0x02, 0x00,                         // damage = 134942
        0x04,                                           // schoolMask = 4
        0x05,                                           // target[7] = 0x04 ^ 1
        0x04, 0x00, 0x00, 0x00,                         // hitInfo = 4
        0xA1,                                           // target[1] = 0xA0 ^ 1
        0x4B, 0x34, 0x02, 0x00                          // spellId = 144459
    };

    MopCombatLogPackets::SpellNonMeleeDamageLog log = {};
    log.attackerGuid = 0x040000000096A000ull;
    log.targetGuid = 0x040000000096A000ull;
    log.spellId = 144459u;
    log.damage = 134942u;
    log.overkill = uint32(-1);
    log.schoolMask = 4u;
    log.absorb = 0u;
    log.resist = 0u;
    log.blocked = 0u;
    log.hitInfo = 4u;

    WorldPacket packet(SMSG_SPELLNONMELEEDAMAGELOG, 48);
    MopCombatLogPackets::BuildSpellNonMeleeDamageLog(packet, log);

    CHECK(packet.GetOpcode() == SMSG_SPELLNONMELEEDAMAGELOG);
    CHECK(packet.size() == sizeof(captured));
    if (packet.size() == sizeof(captured))
    {
        CHECK(std::memcmp(packet.contents(), captured, sizeof(captured)) == 0);
    }
}

// Second real-traffic fixture, and the load-bearing one: capture-000625
// sequence 35331, the 48-byte corpus maximum, in which ALL sixteen GUID bytes
// are present with distinct values. The 38-byte sample above happens to carry
// equal attacker and target GUIDs, so it cannot catch a swapped GUID or a
// transposed byte index; this one can. Both GUIDs decode to a 0xF1.. high part,
// which is what an 18414 unit GUID should look like.
static void test_spell_non_melee_damage_log_matches_dense_capture()
{
    static uint8 const captured[] = {
        0xFB, 0xE5, 0xF0,                               // mask: all sixteen present
        0x00, 0x00, 0x00, 0x00,                         // blocked = 0
        0x98,                                           // attacker[1] = 0x99 ^ 1
        0xFF, 0xFF, 0xFF, 0xFF,                         // overkill = -1
        0xC0, 0x5C, 0x40, 0xBB, 0xF0,                   // target[3] attacker[0] target[6] target[4] attacker[7]
        0x00, 0x00, 0x00, 0x00,                         // resist = 0
        0x00, 0x00, 0x00, 0x00,                         // absorb = 0
        0x0C, 0xED, 0x81, 0x03, 0x03, 0x43, 0x28, 0x53, // attacker[5] target[5] attacker[3] attacker[2] target[2] attacker[6] target[0] attacker[4]
        0x0B, 0x63, 0x00, 0x00,                         // damage = 25355
        0x20,                                           // schoolMask = 0x20
        0xF0,                                           // target[7] = 0xF1 ^ 1
        0x07, 0x00, 0x00, 0x00,                         // hitInfo = 7
        0x9B,                                           // target[1] = 0x9A ^ 1
        0x24, 0xC4, 0x01, 0x00                          // spellId = 115748
    };

    MopCombatLogPackets::SpellNonMeleeDamageLog log = {};
    log.attackerGuid = 0xF1420D528002995Dull;
    log.targetGuid = 0xF141ECBAC1029A29ull;
    log.spellId = 115748u;
    log.damage = 25355u;
    log.overkill = uint32(-1);
    log.schoolMask = 0x20u;
    log.absorb = 0u;
    log.resist = 0u;
    log.blocked = 0u;
    log.hitInfo = 7u;

    WorldPacket packet(SMSG_SPELLNONMELEEDAMAGELOG, 48);
    MopCombatLogPackets::BuildSpellNonMeleeDamageLog(packet, log);

    CHECK(packet.size() == sizeof(captured));
    if (packet.size() == sizeof(captured))
    {
        CHECK(std::memcmp(packet.contents(), captured, sizeof(captured)) == 0);
    }
}

int main(int, char**)
{
    test_spell_instakill_log();
    test_spell_damage_shield_log();
    test_spell_non_melee_damage_log_matches_capture();
    test_spell_non_melee_damage_log_matches_dense_capture();
    if (g_fail) return 1;
    std::printf("mop_combat_log_packets: all checks passed\n");
    return 0;
}
