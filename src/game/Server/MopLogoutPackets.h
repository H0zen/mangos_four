/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 build-18414 logout packet helpers.
 */

#ifndef MANGOS_H_MOP_LOGOUT_PACKETS
#define MANGOS_H_MOP_LOGOUT_PACKETS

#include "Opcodes.h"
#include "WorldPacket.h"

namespace MopLogoutPackets
{
    inline void BuildResponse(WorldPacket& out, uint32 reason, bool instant)
    {
        out.Initialize(SMSG_LOGOUT_RESPONSE, 5);
        out << uint32(reason);
        out.WriteBit(instant);
        out.FlushBits();
    }

    inline void BuildCancelAck(WorldPacket& out)
    {
        out.Initialize(SMSG_LOGOUT_CANCEL_ACK, 0);
    }

    inline void BuildComplete(WorldPacket& out)
    {
        out.Initialize(SMSG_LOGOUT_COMPLETE, 2);

        // Retail 18414 writes one observed true bit followed by an all-zero
        // eight-bit GUID mask. The leading bit's semantic meaning is unknown.
        out.WriteBit(true);
        for (uint8 index = 0; index < 8; ++index)
            out.WriteBit(false);
        out.FlushBits();
    }
}

#endif
