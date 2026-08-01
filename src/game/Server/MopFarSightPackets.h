/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 build-18414 far-sight packet helpers.
 */

#ifndef MANGOS_H_MOP_FAR_SIGHT_PACKETS
#define MANGOS_H_MOP_FAR_SIGHT_PACKETS

#include "WorldPacket.h"

namespace MopFarSightPackets
{
    inline bool ReadRequest(WorldPacket& in, bool& enable)
    {
        if (in.size() - in.rpos() != 1)
        {
            in.rfinish();
            return false;
        }

        uint8 raw = 0;
        in >> raw;
        if (raw != 0x00 && raw != 0x80)
        {
            in.rfinish();
            return false;
        }

        bool const parsedEnable = raw == 0x80;
        enable = parsedEnable;
        return true;
    }
}

#endif
