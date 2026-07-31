/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server of World of Warcraft.
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#ifndef MANGOS_MOP_MAIL_PACKETS_H
#define MANGOS_MOP_MAIL_PACKETS_H

#include "Mail.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <array>
#include <vector>

namespace MopMailPackets
{
    static uint64 const PLAYER_GUID_DOMAIN = UINT64_C(0x0400000000000000);
    static size_t const MAX_MAIL_COUNT = 50;
    static size_t const MAX_SUBJECT_BYTES = 255;
    static size_t const MAX_BODY_BYTES = 7999;
    static size_t const MAX_POST_CRYPT_PAYLOAD_BYTES = 0x7FFFF;
    static size_t const ENCHANT_GROUP_COUNT = 8;

    inline uint64 BuildPlayerSenderGuid(uint32 lowGuid)
    {
        return lowGuid ? PLAYER_GUID_DOMAIN | uint64(lowGuid) : 0;
    }

    struct EnchantGroup
    {
        uint32 fieldAtPlus8 = 0;
        uint32 fieldAtPlus4 = 0;
        uint32 fieldAtPlus0 = 0;
    };

    struct ItemRecord
    {
        uint32 guidLow = 0;
        std::vector<uint8> modifierBlob;
        uint32 durability = 0;
        uint32 unknown = 0;
        std::array<EnchantGroup, ENCHANT_GROUP_COUNT> enchants = {};
        int32 randomPropertyId = 0;
        int32 spellCharges = 0;
        uint32 maxDurability = 0;
        uint32 stackCount = 0;
        uint8 index = 0;
        uint32 entry = 0;
    };

    struct MailRecord
    {
        bool senderIsNotPlayer = false;
        std::string subject;
        std::string body;
        bool hasOptionalA = false;
        bool hasOptionalB = false;
        uint32 optionalA = 0;
        uint32 optionalB = 0;
        std::vector<ItemRecord> items;
        uint64 senderGuid = 0;
        uint32 messageId = 0;
        uint32 mailTemplateId = 0;
        uint64 cod = 0;
        uint32 stationery = 0;
        float daysLeft = 0.0f;
        uint64 money = 0;
        uint32 checkedFlags = 0;
        uint32 senderEntry = 0;
        uint8 messageType = 0;
        uint32 unknown = 0;
    };

    inline std::string TruncateUtf8(std::string const& value, size_t limit)
    {
        if (value.size() <= limit)
            return value;

        size_t end = limit;
        while (end > 0 && (uint8(value[end]) & 0xC0) == 0x80)
            --end;
        return value.substr(0, end);
    }

    inline bool BuildList(WorldPacket& out, uint32 realCount,
        std::vector<MailRecord> const& mails)
    {
        if (mails.size() > MAX_MAIL_COUNT)
            return false;

        for (MailRecord const& mail : mails)
        {
            if (mail.subject.size() > MAX_SUBJECT_BYTES ||
                    mail.body.size() > MAX_BODY_BYTES ||
                    mail.items.size() > MAX_MAIL_ITEMS)
                return false;
        }

        out.Initialize(SMSG_MAIL_LIST_RESULT, 200);
        out << realCount;
        out.WriteBits(uint32(mails.size()), 18);
        for (MailRecord const& mail : mails)
        {
            ObjectGuid const senderGuid(mail.senderGuid);
            out.WriteBit(mail.senderIsNotPlayer);
            out.WriteBits(uint32(mail.subject.size()), 8);
            out.WriteBits(uint32(mail.body.size()), 13);
            out.WriteBit(mail.hasOptionalA);
            out.WriteBit(mail.hasOptionalB);
            out.WriteBits(uint32(mail.items.size()), 17);
            out.WriteBit(mail.senderGuid != 0);
            if (mail.senderGuid)
                out.WriteGuidMask<2, 6, 7, 0, 5, 3, 1, 4>(senderGuid);
            for (size_t i = 0; i < mail.items.size(); ++i)
                out.WriteBit(false);
        }
        out.FlushBits();

        for (MailRecord const& mail : mails)
        {
            ObjectGuid const senderGuid(mail.senderGuid);
            for (ItemRecord const& item : mail.items)
            {
                out << item.guidLow << uint32(item.modifierBlob.size());
                if (!item.modifierBlob.empty())
                    out.append(item.modifierBlob.data(), item.modifierBlob.size());
                out << item.durability << item.unknown;
                for (EnchantGroup const& enchant : item.enchants)
                    out << enchant.fieldAtPlus8 << enchant.fieldAtPlus4
                        << enchant.fieldAtPlus0;
                out << item.randomPropertyId << item.spellCharges
                    << item.maxDurability << item.stackCount << item.index
                    << item.entry;
            }
            out.append(mail.body.data(), mail.body.size());
            out << mail.messageId;
            if (mail.senderGuid)
                out.WriteGuidBytes<4, 0, 5, 3, 1, 7, 2, 6>(senderGuid);
            out << mail.mailTemplateId << mail.cod;
            out.append(mail.subject.data(), mail.subject.size());
            out << mail.stationery << mail.daysLeft << mail.money
                << mail.checkedFlags;
            if (mail.senderIsNotPlayer)
                out << mail.senderEntry;
            out << mail.messageType << mail.unknown;
            if (mail.hasOptionalA)
                out << mail.optionalA;
            if (mail.hasOptionalB)
                out << mail.optionalB;
        }
        return out.size() <= MAX_POST_CRYPT_PAYLOAD_BYTES;
    }
}

#endif
