/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server of World of Warcraft.
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#ifndef MANGOS_MOP_GUILD_BANK_PACKETS_H
#define MANGOS_MOP_GUILD_BANK_PACKETS_H

#include "ByteBuffer.h"

#include <string>
#include <vector>

namespace MopGuildBankPackets
{
    static size_t const MAX_TAB_COUNT = 8;
    static size_t const MAX_ITEM_COUNT = 98;
    static size_t const MAX_SOCKET_ENCHANT_COUNT = 3;
    static size_t const MAX_TAB_NAME_BYTES = 64;
    static size_t const MAX_TAB_ICON_BYTES = 255;
    static size_t const MAX_POST_CRYPT_PAYLOAD_BYTES = 0x7FFFF;

    struct SocketEnchant
    {
        uint32 index = 0;
        uint32 enchantmentId = 0;
    };

    struct ItemRecord
    {
        bool present = false;
        uint32 dynamicFlags = 0;
        std::vector<SocketEnchant> socketEnchants;
        uint32 permanentEnchantId = 0;
        uint32 entry = 0;
        uint32 spellCharges = 0;
        uint32 stackCount = 0;
        uint32 slotId = 0;
        int32 randomPropertyId = 0;
        uint32 suffixFactor = 0;
    };

    struct TabRecord
    {
        uint32 index = 0;
        std::string icon;
        std::string name;
    };

    struct GuildBankList
    {
        uint32 tabId = 0;
        uint64 money = 0;
        int32 withdrawRemaining = 0;
        bool fullUpdate = false;
        std::vector<TabRecord> tabs;
        std::vector<ItemRecord> items;
    };

    inline std::string TruncateUtf8(std::string const& value, size_t limit)
    {
        if (value.size() <= limit)
        {
            return value;
        }

        size_t end = limit;
        while (end > 0 && (uint8(value[end]) & 0xC0) == 0x80)
        {
            --end;
        }
        return value.substr(0, end);
    }

    inline bool BuildListBody(ByteBuffer& out, GuildBankList const& list)
    {
        if (list.tabId >= MAX_TAB_COUNT || list.tabs.size() > MAX_TAB_COUNT ||
                list.items.size() > MAX_ITEM_COUNT ||
                (!list.tabs.empty() && !list.fullUpdate))
        {
            return false;
        }

        uint32 previousTab = 0;
        bool havePreviousTab = false;
        for (TabRecord const& tab : list.tabs)
        {
            if (tab.index >= MAX_TAB_COUNT ||
                    (havePreviousTab && tab.index <= previousTab) ||
                    tab.name.size() > MAX_TAB_NAME_BYTES ||
                    tab.icon.size() > MAX_TAB_ICON_BYTES)
            {
                return false;
            }
            previousTab = tab.index;
            havePreviousTab = true;
        }

        uint32 previousSlot = 0;
        bool havePreviousSlot = false;
        for (ItemRecord const& item : list.items)
        {
            if (item.slotId >= MAX_ITEM_COUNT ||
                    (havePreviousSlot && item.slotId <= previousSlot) ||
                    item.socketEnchants.size() > MAX_SOCKET_ENCHANT_COUNT ||
                    (!item.present && !item.socketEnchants.empty()))
            {
                return false;
            }

            uint32 previousSocket = 0;
            bool havePreviousSocket = false;
            for (SocketEnchant const& socket : item.socketEnchants)
            {
                if (socket.index >= MAX_SOCKET_ENCHANT_COUNT ||
                        (havePreviousSocket && socket.index <= previousSocket))
                {
                    return false;
                }
                previousSocket = socket.index;
                havePreviousSocket = true;
            }
            previousSlot = item.slotId;
            havePreviousSlot = true;
        }

        ByteBuffer staged;
        staged << list.tabId << list.money << list.withdrawRemaining;
        staged.WriteBit(list.fullUpdate);
        staged.WriteBits(uint32(list.tabs.size()), 21);
        staged.WriteBits(uint32(list.items.size()), 18);

        for (TabRecord const& tab : list.tabs)
        {
            staged.WriteBits(uint32(tab.icon.size()), 9);
            staged.WriteBits(uint32(tab.name.size()), 7);
        }
        for (ItemRecord const& item : list.items)
        {
            staged.WriteBit(false); // Parsed but unused by the 18414 client.
            staged.WriteBits(uint32(item.socketEnchants.size()), 21);
        }
        staged.FlushBits();

        for (ItemRecord const& item : list.items)
        {
            staged << uint32(item.present ? item.dynamicFlags : 0);
            staged << uint32(0);
            for (SocketEnchant const& socket : item.socketEnchants)
            {
                staged << socket.index << socket.enchantmentId;
            }
            staged << uint32(item.present ? item.permanentEnchantId : 0);
            if (item.present)
            {
                staged << uint32(4) << uint32(0); // Present, no persisted modifiers.
            }
            else
            {
                staged << uint32(0);
            }
            staged << uint32(item.present ? item.entry : 0);
            staged << uint32(item.present ? item.spellCharges : 0);
            staged << uint32(item.present ? item.stackCount : 0);
            staged << item.slotId;
            staged << int32(item.present ? item.randomPropertyId : 0);
            staged << uint32(item.present ? item.suffixFactor : 0);
        }

        for (TabRecord const& tab : list.tabs)
        {
            staged << tab.index;
            staged.append(tab.icon.data(), tab.icon.size());
            staged.append(tab.name.data(), tab.name.size());
        }

        if (out.size() + staged.size() > MAX_POST_CRYPT_PAYLOAD_BYTES)
        {
            return false;
        }
        if (!staged.empty())
        {
            out.append(staged.contents(), staged.size());
        }
        return true;
    }
}

#endif
