/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOSSERVER_WORLDPACKET_H
#define MANGOSSERVER_WORLDPACKET_H

#include "Common.h"
#include "ByteBuffer.h"

// Note: m_opcode and size stored in platfom dependent format
// ignore endianess until send, and converted at receive
//
// The opcode is a plain uint16, NOT OpcodesList (src/game/Server/Opcodes.h's 1,382-entry enum).
// WorldPacket lives in src/shared/, which src/proto/ links; Opcodes.h pulls the full game header
// surface (WorldSession.h et al. -- see Opcodes.h's own opcode-table-size rationale) and proto must
// not link game. Giving two libraries the same quote-included "Opcodes.h" name for two different
// enums is also unsafe the moment both are linked into one binary. Callers keep passing OpcodesList
// values (SMSG_*/CMSG_* constants) unchanged -- an unscoped enum converts to its integer value
// implicitly -- and read the opcode back through game's own LookupOpcodeName(GetOpcode()) rather
// than a member of this class. 0x1001 below is MSG_NULL_ACTION's value (Opcodes.h:75); duplicated
// as a literal for the same reason the enum itself cannot be named here.
/**
 * @brief
 *
 */
class WorldPacket : public ByteBuffer
{
    public:
        /**
         * @brief just container for later use
         *
         */
        WorldPacket() : ByteBuffer(0), m_opcode(0x1001)
        {
        }
        /**
         * @brief
         *
         * @param opcode
         * @param res
         */
        explicit WorldPacket(uint16 opcode, size_t res = 200) : ByteBuffer(res), m_opcode(opcode) { }
        /**
         * @brief copy constructor
         *
         * @param packet
         */
        WorldPacket(const WorldPacket& packet) : ByteBuffer(packet), m_opcode(packet.m_opcode)
        {
        }

        /**
         * @brief
         *
         * @param opcode
         * @param newres
         */
        void Initialize(uint16 opcode, size_t newres = 200)
        {
            clear();
            _storage.reserve(newres);
            m_opcode = opcode;
        }

        /**
         * @brief
         *
         * @return uint16
         */
        uint16 GetOpcode() const { return m_opcode; }
        /**
         * @brief
         *
         * @param opcode
         */
        void SetOpcode(uint16 opcode) { m_opcode = opcode; }

    protected:
        uint16 m_opcode;
};
#endif
