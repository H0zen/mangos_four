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

/**
 * @file MailHandler.cpp
 * @brief Mail system opcode handlers
 *
 * This file handles mail-related opcodes including:
 * - CMSG_SEND_MAIL: Send mail to another player
 * - CMSG_MAIL_DELETE: Delete mail
 * - CMSG_MAIL_RETURN: Return mail to sender
 * - CMSG_MAIL_MARK_AS_READ: Mark mail as read
 * - CMSG_MAIL_CREATE_TEXT_ITEM: Create item from mail text
 * - CMSG_MAIL_TAKE_ITEM: Take item from mail
 * - CMSG_MAIL_TAKE_MONEY: Take money from mail
 * - CMSG_MAIL_QUERY_NEXT_TIME: Query next mail delivery time
 *
 * Mail operations require proper validation of recipient, money,
 * and item attachments.
 */

#include "Mail.h"
#include "MailTakeItemPolicy.h"
#include "Language.h"
#include "Log.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Item.h"
#include "Player.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Chat.h"
#include "MopMailPackets.h"

/**
 * @brief Verifies that the player can legally access the requested mailbox.
 *
 * @param guid The mailbox guid or player guid used for mailbox access.
 * @return true if mailbox access is allowed; otherwise false.
 */
bool WorldSession::CheckMailBox(ObjectGuid guid)
{
    // GM case
    if (guid == GetPlayer()->GetObjectGuid())
    {
        // command case will return only if player have real access to command
        if (!ChatHandler(GetPlayer()).FindCommand("mailbox"))
        {
            DEBUG_LOG("%s attempt open mailbox in cheating way.", guid.GetString().c_str());
            return false;
        }
    }
    // mailbox case
    else if (guid.IsGameObject())
    {
        if (!GetPlayer()->GetGameObjectIfCanInteractWith(guid, GAMEOBJECT_TYPE_MAILBOX))
        {
            DEBUG_LOG("Mailbox %s not found or %s can't interact with him.", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
            return false;
        }
    }
    // squire case
    else if (guid.IsAnyTypeCreature())
    {
        Creature* creature = GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
        if (!creature)
        {
            DEBUG_LOG("%s not found or %s can't interact with him.", guid.GetString().c_str(), GetPlayer()->GetGuidStr().c_str());
            return false;
        }

        if (!(creature->GetCreatureInfo()->CreatureTypeFlags & CREATURE_TYPEFLAGS_SQUIRE))
        {
            DEBUG_LOG("%s not have access to mailbox.", creature->GetGuidStr().c_str());
            return false;
        }

        if (creature->GetOwnerGuid() != GetPlayer()->GetObjectGuid())
        {
            DEBUG_LOG("%s not owned by %s for access to mailbox.", creature->GetGuidStr().c_str(), GetPlayer()->GetGuidStr().c_str());
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

/*
 * Handles the Packet sent by the client when sending a mail.
 *
 * This methods takes the packet sent by the client and performs the following actions:
 * - Checks whether the mail is valid: i.e. can he send the selected items,
 *   does he have enough money, etc.
 * - Creates a MailDraft and adds the needed items, money, cost data.
 * - Sends the mail.
 *
 * Depending on the outcome of the checks performed the player will recieve a different
 * MailResponseResult.
 *
 * @see MailResponseResult
 * @see SendMailResult()
 *
 * @param recv_data the WorldPacket containing the data sent by the client.
 */
void WorldSession::HandleSendMail(WorldPacket& recv_data)
{
    sLog.outError("WORLD: CMSG_SEND_MAIL");
    ObjectGuid mailboxGuid;
    uint64 money, COD;
    std::string receiver, subject, body;
    uint8 receiverLen, subjectLen, bodyLen;
    uint32 unk1, unk2;

    recv_data >> unk1;                                      // stationery?
    recv_data >> unk2;                                      // 0x00000000

    recv_data >> COD >> money;                              // cod and money

    bodyLen = recv_data.ReadBits(12);
    subjectLen = recv_data.ReadBits(9);

    uint8 items_count = recv_data.ReadBits(5);              // attached items count
    if (items_count > MAX_MAIL_ITEMS)                       // client limit
    {
        GetPlayer()->SendMailResult(0, MAIL_SEND, MAIL_ERR_TOO_MANY_ATTACHMENTS);
        recv_data.rfinish();                                // set to end to avoid warnings spam
        return;
    }

    recv_data.ReadGuidMask<0>(mailboxGuid);

    ObjectGuid itemGuids[MAX_MAIL_ITEMS];
    for (uint8 i = 0; i < items_count; ++i)
    {
        recv_data.ReadGuidMask<2, 6, 3, 7, 1, 0, 4, 5>(itemGuids[i]);
    }

    recv_data.ReadGuidMask<3, 4>(mailboxGuid);

    receiverLen = recv_data.ReadBits(7);

    recv_data.ReadGuidMask<2, 6, 1, 7, 5>(mailboxGuid);

    recv_data.ReadGuidBytes<4>(mailboxGuid);

    for (uint8 i = 0; i < items_count; ++i)
    {
        recv_data.ReadGuidBytes<6, 1, 7, 2>(itemGuids[i]);
        recv_data.read_skip<uint8>();                       // item slot in mail, not used
        recv_data.ReadGuidBytes<3, 0, 4, 5>(itemGuids[i]);
    }

    recv_data.ReadGuidBytes<7, 3, 6, 5>(mailboxGuid);

    subject = recv_data.ReadString(subjectLen);
    receiver = recv_data.ReadString(receiverLen);

    recv_data.ReadGuidBytes<2, 0>(mailboxGuid);

    body = recv_data.ReadString(bodyLen);

    recv_data.ReadGuidBytes<1>(mailboxGuid);

    DEBUG_LOG("WORLD: CMSG_SEND_MAIL receiver '%s' subject '%s' body '%s' mailbox " UI64FMTD " money " UI64FMTD " COD " UI64FMTD " unkt1 %u unk2 %u",
        receiver.c_str(), subject.c_str(), body.c_str(), mailboxGuid.GetRawValue(), money, COD, unk1, unk2);
    // packet read complete, now do check

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    if (receiver.empty())
    {
        return;
    }

    Player* pl = _player;

    ObjectGuid rc;
    if (normalizePlayerName(receiver))
    {
        rc = sObjectMgr.GetPlayerGuidByName(receiver);
    }

    if (!rc)
    {
        DEBUG_LOG("%s is sending mail to %s (GUID: nonexistent!) with subject %s and body %s includes %u items, " UI64FMTD " copper and " UI64FMTD " COD copper with unk1 = %u, unk2 = %u",
                   pl->GetGuidStr().c_str(), receiver.c_str(), subject.c_str(), body.c_str(), items_count, money, COD, unk1, unk2);
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_NOT_FOUND);
        return;
    }

    DEBUG_LOG("%s is sending mail to %s with subject %s and body %s includes %u items, %u copper and %u COD copper with unk1 = %u, unk2 = %u",
               pl->GetGuidStr().c_str(), rc.GetString().c_str(), subject.c_str(), body.c_str(), items_count, money, COD, unk1, unk2);

    if (pl->GetObjectGuid() == rc)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANNOT_SEND_TO_SELF);
        return;
    }

    // safeguard against possible money dupe
    if (money && COD)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    uint32 cost = items_count ? 30 * items_count : 30;      // price hardcoded in client

    uint64 reqmoney = cost + money;

    if (pl->GetMoney() < reqmoney)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_NOT_ENOUGH_MONEY);
        return;
    }

    Player* receive = sObjectMgr.GetPlayer(rc);

    Team rc_team;
    uint8 mails_count = 0;                                  // do not allow to send to one player more than 100 mails

    if (receive)
    {
        rc_team = receive->GetTeam();
        mails_count = receive->GetMailSize();
    }
    else
    {
        rc_team = sObjectMgr.GetPlayerTeamByGUID(rc);
        if (QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(*) FROM `mail` WHERE `receiver` = '%u'", rc.GetCounter()))
        {
            Field* fields = result->Fetch();
            mails_count = fields[0].GetUInt32();
            delete result;
        }
    }

    // do not allow to have more than 100 mails in mailbox.. mails count is in opcode uint8!!! - so max can be 255..
    if (mails_count > 100)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_CAP_REACHED);
        return;
    }

    // check the receiver's Faction...
    if (!sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_MAIL) && pl->GetTeam() != rc_team && GetSecurity() == SEC_PLAYER)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_NOT_YOUR_TEAM);
        return;
    }

    uint32 rc_account = receive
                        ? receive->GetSession()->GetAccountId()
                        : sObjectMgr.GetPlayerAccountIdByGUID(rc);

    Item* items[MAX_MAIL_ITEMS];

    for (uint8 i = 0; i < items_count; ++i)
    {
        if (!itemGuids[i].IsItem())
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
            return;
        }

        Item* item = pl->GetItemByGuid(itemGuids[i]);

        // prevent sending bag with items (cheat: can be placed in bag after adding equipped empty bag to mail)
        if (!item)
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
            return;
        }

        if (!item->CanBeTraded(true))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_EQUIP_ERROR, EQUIP_ERR_MAIL_BOUND_ITEM);
            return;
        }

        if (item->IsBoundAccountWide() && item->IsSoulBound() && pl->GetSession()->GetAccountId() != rc_account)
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_EQUIP_ERROR, EQUIP_ERR_ARTEFACTS_ONLY_FOR_OWN_CHARACTERS);
            return;
        }

        if ((item->GetProto()->Flags & ITEM_FLAG_CONJURED) || item->GetUInt32Value(ITEM_FIELD_DURATION))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_EQUIP_ERROR, EQUIP_ERR_MAIL_BOUND_ITEM);
            return;
        }

        if (COD && item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANT_SEND_WRAPPED_COD);
            return;
        }

        items[i] = item;
    }

    pl->SendMailResult(0, MAIL_SEND, MAIL_OK);

    pl->ModifyMoney(-int64(reqmoney));
    pl->GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_MAIL, cost);

    bool needItemDelay = false;

    MailDraft draft(subject, body);

    if (items_count > 0 || money > 0)
    {
        if (items_count > 0)
        {
            for (uint8 i = 0; i < items_count; ++i)
            {
                Item* item = items[i];
                if (GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
                {
                    sLog.outCommand(GetAccountId(), "GM %s (Account: %u) mail item: %s (Entry: %u Count: %u) to player: %s (Account: %u)",
                                    GetPlayerName(), GetAccountId(), item->GetProto()->Name1, item->GetEntry(), item->GetCount(), receiver.c_str(), rc_account);
                }

                pl->MoveItemFromInventory(items[i]->GetBagSlot(), item->GetSlot(), true);
                CharacterDatabase.BeginTransaction();
                item->DeleteFromInventoryDB();              // deletes item from character's inventory
                item->SaveToDB();                           // recursive and not have transaction guard into self, item not in inventory and can be save standalone
                // owner in data will set at mail receive and item extracting
                CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid`='%u'", rc.GetCounter(), item->GetGUIDLow());
                CharacterDatabase.CommitTransaction();

                draft.AddItem(item);
            }

            // if item send to character at another account, then apply item delivery delay
            needItemDelay = pl->GetSession()->GetAccountId() != rc_account;
        }

        if (money > 0 &&  GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
        {
            sLog.outCommand(GetAccountId(), "GM %s (Account: %u) mail money: " UI64FMTD " to player: %s (Account: %u)",
                            GetPlayerName(), GetAccountId(), money, receiver.c_str(), rc_account);
        }
    }

    // If theres is an item, there is a one hour delivery delay if sent to another account's character.
    uint32 deliver_delay = needItemDelay ? sWorld.getConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY) : 0;

    // will delete item or place to receiver mail list
    draft
    .SetMoney(money)
    .SetCOD(COD)
    .SendMailTo(MailReceiver(receive, rc), pl, body.empty() ? MAIL_CHECK_MASK_COPIED : MAIL_CHECK_MASK_HAS_BODY, deliver_delay);

    CharacterDatabase.BeginTransaction();
    pl->SaveInventoryAndGoldToDB();
    CharacterDatabase.CommitTransaction();
}

/**
 * Handles the Packet sent by the client when reading a mail.
 *
 * This method is called when a client reads a mail that was previously unread.
 * It will add the MAIL_CHECK_MASK_READ flag to the mail being read.
 *
 * @see MailCheckMask
 *
 * @param recv_data the packet containing information about the mail the player read.
 *
 */
void WorldSession::HandleMailMarkAsRead(WorldPacket& recv_data)
{
    // 18414 leads with the mail id and packs the mailbox GUID behind it.
    uint32 mailId;
    ObjectGuid mailboxGuid = MopCompactPackets::ReadMailMarkAsRead(recv_data, mailId);

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;

    if (Mail* m = pl->GetMail(mailId))
    {
        // Only act on a mail that is actually unread. This used to decrement on
        // every request, so replaying it against one already-read mail walked the
        // unread counter down past the other unread mail the player still had --
        // a client-driven way to hide their own new mail. The opcode is
        // registered now, so the replay is reachable.
        if ((m->checked & MAIL_CHECK_MASK_READ) == 0)
        {
            if (pl->unReadMails)
            {
                --pl->unReadMails;
            }
            m->checked = m->checked | MAIL_CHECK_MASK_READ;
            pl->m_mailsUpdated = true;
            m->state = MAIL_STATE_CHANGED;
        }
    }
}

/**
 * Handles the Packet sent by the client when deleting a mail.
 *
 * This method is called when a client deletes a mail in his mailbox.
 *
 * @param recv_data The packet containing information about the mail being deleted.
 *
 */
void WorldSession::HandleMailDelete(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;
    recv_data.read_skip<uint32>();                          // mailTemplateId

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;
    pl->m_mailsUpdated = true;

    if (Mail* m = pl->GetMail(mailId))
    {
        // delete shouldn't show up for COD mails
        if (m->COD)
        {
            pl->SendMailResult(mailId, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
            return;
        }

        m->state = MAIL_STATE_DELETED;
    }
    pl->SendMailResult(mailId, MAIL_DELETED, MAIL_OK);
}
/**
 * Handles the Packet sent by the client when returning a mail to sender.
 * This method is called when a player chooses to return a mail to its sender.
 * It will create a new MailDraft and add the items, money, etc. associated with the mail
 * and then send the mail to the original sender.
 *
 * @param recv_data The packet containing information about the mail being returned.
 *
 */
void WorldSession::HandleMailReturnToSender(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;
    recv_data >> mailboxGuid;
    recv_data >> mailId;
    recv_data.read_skip<uint64>();                          // original sender GUID for return to, not used

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;
    Mail* m = pl->GetMail(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_RETURNED_TO_SENDER, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    // we can return mail now
    // so firstly delete the old one
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `id` = '%u'", mailId);
    // needed?
    CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `mail_id` = '%u'", mailId);
    CharacterDatabase.CommitTransaction();
    pl->RemoveMail(mailId);

    // send back only to existing players and simple drop for other cases
    if (m->messageType == MAIL_NORMAL && m->sender)
    {
        MailDraft draft;
        if (m->mailTemplateId)
        {
            draft.SetMailTemplate(m->mailTemplateId, false);// items already included
        }
        else
        {
            draft.SetSubjectAndBody(m->subject, m->body);
        }

        if (m->HasItems())
        {
            for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
            {
                if (Item* item = pl->GetMItem(itr2->item_guid))
                {
                    draft.AddItem(item);
                }

                pl->RemoveMItem(itr2->item_guid);
            }
        }

        draft.SetMoney(m->money).SendReturnToSender(GetAccountId(), m->receiverGuid, ObjectGuid(HIGHGUID_PLAYER, m->sender));
    }

    delete m;                                               // we can deallocate old mail
    pl->SendMailResult(mailId, MAIL_RETURNED_TO_SENDER, MAIL_OK);
}

/**
 * Handles the packet sent by the client when taking an item from the mail.
 */
void WorldSession::HandleMailTakeItem(WorldPacket& recv_data)
{
    // 18414 leads with the mail id and the item's low GUID, then packs the
    // mailbox GUID; the inherited read had all three in the opposite order.
    uint32 mailId;
    uint32 itemId;                                          // item guid low
    ObjectGuid mailboxGuid = MopCompactPackets::ReadMailTakeItem(recv_data, mailId, itemId);

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;

    Mail* m = pl->GetMail(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    MailItemInfo const* const attachment =
        MailTakeItemPolicy::FindAttachment(*m, itemId);
    Item* it = pl->GetMItem(itemId);
    MailTakeItemPolicy::ResolvedItem const resolved = {
        it != NULL,
        it ? it->GetGUIDLow() : 0,
        it ? it->GetEntry() : 0
    };
    if (MailTakeItemPolicy::Evaluate(*m, pl->GetObjectGuid(), itemId,
            attachment, resolved) == MailTakeItemPolicy::Decision::RejectInternal)
    {
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN,
            MAIL_ERR_INTERNAL_ERROR, 0, itemId, 0);
        return;
    }

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, it, false);
    if (msg == EQUIP_ERR_OK)
    {
        if (!m->RemoveItem(itemId))
        {
            pl->SendMailResult(mailId, MAIL_ITEM_TAKEN,
                MAIL_ERR_INTERNAL_ERROR, 0, itemId, 0);
            return;
        }
        m->removedItems.push_back(itemId);

        m->state = MAIL_STATE_CHANGED;
        pl->m_mailsUpdated = true;
        pl->RemoveMItem(it->GetGUIDLow());

        uint32 count = it->GetCount();                      // save counts before store and possible merge with deleting
        pl->MoveItemToInventory(dest, it, true);

        CharacterDatabase.BeginTransaction();
        pl->SaveInventoryAndGoldToDB();
        pl->_SaveMail();
        CharacterDatabase.CommitTransaction();

        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_OK, 0, itemId, count);
    }
    else
    {
        // Retail echoes the item back even when the take FAILS: the fixtured
        // equip-error body carries itemGuidLow 938134456 with itemCount 0. The
        // default of 0 dropped it, so the client was told an item it had asked
        // for could not be taken without being told which one. The count stays
        // zero, because nothing moved.
        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_EQUIP_ERROR, msg, itemId, 0);
    }
}
/**
 * Handles the packet sent by the client when taking money from the mail.
 */
void WorldSession::HandleMailTakeMoney(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;
    uint64 money;
    recv_data >> mailboxGuid;
    recv_data >> mailId;
    recv_data >> money;

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;

    Mail* m = pl->GetMail(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_OK);

    pl->ModifyMoney(m->money);
    m->money = 0;
    m->state = MAIL_STATE_CHANGED;
    pl->m_mailsUpdated = true;

    // save money and mail to prevent cheating
    CharacterDatabase.BeginTransaction();
    pl->SaveGoldToDB();
    pl->_SaveMail();
    CharacterDatabase.CommitTransaction();
}

/**
 * Handles the packet sent by the client when requesting the current mail list.
 * It will send a list of all available mails in the players mailbox to the client.
 */
void WorldSession::HandleGetMailList(WorldPacket& recv_data)
{
    // 18414 packs the mailbox GUID; the inherited read took it raw.
    ObjectGuid mailboxGuid = MopCompactPackets::ReadGetMailList(recv_data);

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    std::vector<MopMailPackets::MailRecord> stagedMails;
    stagedMails.reserve(MopMailPackets::MAX_MAIL_COUNT);
    uint32 realCount = 0;
    time_t const curTime = time(NULL);

    for (PlayerMails::iterator itr = _player->GetMailBegin(); itr != _player->GetMailEnd(); ++itr)
    {
        Mail const* mail = *itr;

        // skip deleted or not delivered (deliver delay not expired) mails
        if (mail->state == MAIL_STATE_DELETED || curTime < mail->deliver_time)
            continue;
        ++realCount;
        if (stagedMails.size() >= MopMailPackets::MAX_MAIL_COUNT)
            continue;

        MopMailPackets::MailRecord record;
        record.senderIsNotPlayer = mail->messageType != MAIL_NORMAL;
        record.subject = MopMailPackets::TruncateUtf8(mail->subject,
            MopMailPackets::MAX_SUBJECT_BYTES);
        record.body = MopMailPackets::TruncateUtf8(mail->body,
            MopMailPackets::MAX_BODY_BYTES);
        record.senderGuid = mail->messageType == MAIL_NORMAL ?
            MopMailPackets::BuildPlayerSenderGuid(mail->sender) : 0;
        record.messageId = mail->messageID;
        record.mailTemplateId = mail->mailTemplateId;
        record.cod = mail->COD;
        record.stationery = mail->stationery;
        record.daysLeft = float(mail->expire_time - curTime) / float(DAY);
        record.money = mail->money;
        record.checkedFlags = mail->checked;
        record.senderEntry = record.senderIsNotPlayer &&
            mail->messageType != MAIL_CALENDAR ? mail->sender : 0;
        record.messageType = mail->messageType;

        size_t const itemCount = std::min(mail->items.size(),
            size_t(MAX_MAIL_ITEMS));
        record.items.reserve(itemCount);
        for (size_t i = 0; i < itemCount; ++i)
        {
            Item* item = _player->GetMItem(mail->items[i].item_guid);
            if (!item)
                continue;

            MopMailPackets::ItemRecord itemRecord;
            itemRecord.guidLow = item->GetGUIDLow();
            itemRecord.durability =
                item->GetUInt32Value(ITEM_FIELD_DURABILITY);
            itemRecord.unknown = item->GetItemSuffixFactor();
            for (uint8 j = 0; j < MopMailPackets::ENCHANT_GROUP_COUNT; ++j)
            {
                MopMailPackets::EnchantGroup& enchant = itemRecord.enchants[j];
                enchant.fieldAtPlus8 =
                    item->GetEnchantmentCharges((EnchantmentSlot)j);
                enchant.fieldAtPlus4 =
                    item->GetEnchantmentDuration((EnchantmentSlot)j);
                enchant.fieldAtPlus0 =
                    item->GetEnchantmentId((EnchantmentSlot)j);
            }
            itemRecord.randomPropertyId = item->GetItemRandomPropertyId();
            itemRecord.spellCharges = item->GetSpellCharges();
            itemRecord.maxDurability =
                item->GetUInt32Value(ITEM_FIELD_MAXDURABILITY);
            itemRecord.stackCount = item->GetCount();
            itemRecord.index = uint8(i);
            itemRecord.entry = item->GetEntry();
            record.items.push_back(itemRecord);
        }
        stagedMails.push_back(record);
    }

    WorldPacket data;
    if (!MopMailPackets::BuildList(data, realCount, stagedMails))
    {
        sLog.outError("HandleGetMailList: failed to build a bounded mail list");
        return;
    }
    SendPacket(&data);

    // recalculate m_nextMailDelivereTime and unReadMails
    _player->UpdateNextMailTimeAndUnreads();
}

/*
 * Handles the packet sent by the client when he copies the body a mail to his inventory.
 *
 * When a player copies the body of a mail to his inventory this method is called. It will create
 * a new item with the text of the mail and store it in the players inventory (if possible).
 *
 */
void WorldSession::HandleMailCreateTextItem(WorldPacket& recv_data)
{
    ObjectGuid mailboxGuid;
    uint32 mailId;

    recv_data >> mailboxGuid;
    recv_data >> mailId;

    if (!CheckMailBox(mailboxGuid))
    {
        return;
    }

    Player* pl = _player;

    Mail* m = pl->GetMail(mailId);
    if (!m || (m->body.empty() && !m->mailTemplateId) || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    Item* bodyItem = new Item;                              // This is not bag and then can be used new Item.
    if (!bodyItem->Create(sObjectMgr.GenerateItemLowGuid(), MAIL_BODY_ITEM_TEMPLATE, pl))
    {
        delete bodyItem;
        return;
    }

    // in mail template case we need create new item text
    if (m->mailTemplateId)
    {
        MailTemplateEntry const* mailTemplateEntry = sMailTemplateStore.LookupEntry(m->mailTemplateId);
        if (!mailTemplateEntry)
        {
            pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_ERR_INTERNAL_ERROR);
            delete bodyItem;
            return;
        }

        bodyItem->SetText(mailTemplateEntry->Body_lang[GetSessionDbcLocale()]);
    }
    else
    {
        bodyItem->SetText(m->body);
    }

    bodyItem->SetGuidValue(ITEM_FIELD_CREATOR, ObjectGuid(HIGHGUID_PLAYER, m->sender));
    bodyItem->SetFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_READABLE | ITEM_DYNFLAG_UNK15 | ITEM_DYNFLAG_UNK16);

    DETAIL_LOG("HandleMailCreateTextItem mailid=%u", mailId);

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, bodyItem, false);
    if (msg == EQUIP_ERR_OK)
    {
        m->checked = m->checked | MAIL_CHECK_MASK_COPIED;
        m->state = MAIL_STATE_CHANGED;
        pl->m_mailsUpdated = true;

        pl->StoreItem(dest, bodyItem, true);
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_OK);
    }
    else
    {
        pl->SendMailResult(mailId, MAIL_MADE_PERMANENT, MAIL_ERR_EQUIP_ERROR, msg);
        delete bodyItem;
    }
}

void WorldSession::HandleQueryNextMailTime(WorldPacket & /**recv_data*/)
{
    std::vector<MopQueryPackets::MailNextTimeEntry> records;
    time_t const now = time(NULL);
    for (PlayerMails::iterator itr = _player->GetMailBegin();
         itr != _player->GetMailEnd() && records.size() < 3; ++itr)
    {
        Mail const* mail = *itr;
        if ((mail->checked & MAIL_CHECK_MASK_READ) || now < mail->deliver_time)
            continue;

        MopQueryPackets::MailNextTimeEntry record;
        if (mail->messageType == MAIL_NORMAL)
            record.senderGuid = MopMailPackets::BuildPlayerSenderGuid(mail->sender);
        else
            record.nonPlayerSender = mail->sender;
        record.messageType = uint8(mail->messageType);
        record.deliveryTime = float(mail->deliver_time - now);
        record.stationery = uint32(mail->stationery);
        records.push_back(record);
    }

    float const nextMailTime = records.empty() ? -1.0f : 0.0f;
    WorldPacket data(SMSG_MAIL_QUERY_NEXT_TIME_RESULT, 64);
    MopQueryPackets::BuildMailQueryNextTimeResult(data, records, nextMailTime);
    SendPacket(&data);
}

/*! @} */
