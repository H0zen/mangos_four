/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MANGOS_SCRIPT_ACTION_FAULT_KEY_H
#define MANGOS_SCRIPT_ACTION_FAULT_KEY_H

#include <cstdint>
#include <tuple>

namespace ScriptActionDetail
{
    using FaultKey = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t,
                                std::uint32_t, std::uint32_t, std::uint8_t>;

    inline FaultKey MakeFaultKey(std::uint32_t type, std::uint32_t id,
                                 std::uint32_t command, std::uint32_t buddyEntry,
                                 std::uint32_t searchRadiusOrGuid,
                                 std::uint8_t dataFlags)
    {
        return std::make_tuple(type, id, command, buddyEntry,
                               searchRadiusOrGuid, dataFlags);
    }
}

#endif
