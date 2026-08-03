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
#include "MailMoneyPolicy.h"
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

    m_openMailboxGuid = guid;
    return true;
}

bool WorldSession::CheckOpenedMailBox()
{
    if (!m_openMailboxGuid)
        return false;

    ObjectGuid const mailboxGuid = m_openMailboxGuid;
    if (!CheckMailBox(mailboxGuid))
    {
        m_openMailboxGuid.Clear();
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
    DEBUG_LOG("WORLD: CMSG_SEND_MAIL");
    MopCompactPackets::SendMailRequest request;
    if (!MopCompactPackets::ReadSendMail(recv_data, request))
        return;

    Player* pl = _player;
    uint8 const itemsCount = uint8(request.attachments.size());
    DEBUG_LOG("WORLD: CMSG_SEND_MAIL receiver '%s' subject '%s' body '%s' mailbox " UI64FMTD " money " UI64FMTD " COD " UI64FMTD " stationery %u package %u",
        request.receiver.c_str(), request.subject.c_str(), request.body.c_str(),
        request.mailboxGuid.GetRawValue(), request.money, request.COD,
        request.stationeryId, request.packageId);

    if (!CheckMailBox(request.mailboxGuid) || request.receiver.empty())
        return;

    if (itemsCount > MAX_MAIL_ITEMS)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_TOO_MANY_ATTACHMENTS);
        return;
    }

    // The 18414 client appends "-Realm" to a recipient it did not get typed by
    // hand - a reply, a chat link, a who-list or guild-roster click - so
    // "Gregory-Four" arrives on the wire and never resolves. Strip our own
    // realm before normalising, exactly as whisper and guild invite do. A
    // foreign realm survives the strip and correctly falls through to
    // MAIL_ERR_RECIPIENT_NOT_FOUND rather than silently delivering to a local
    // character who happens to share the name.
    StripHomeRealmSuffix(request.receiver);

    ObjectGuid rc;
    if (normalizePlayerName(request.receiver))
        rc = sObjectMgr.GetPlayerGuidByName(request.receiver);

    if (!rc)
    {
        DEBUG_LOG("%s is sending mail to %s (GUID: nonexistent!) with subject %s and body %s includes %u items, " UI64FMTD " copper and " UI64FMTD " COD copper with unk1 = %u, unk2 = %u",
            pl->GetGuidStr().c_str(), request.receiver.c_str(),
            request.subject.c_str(), request.body.c_str(), itemsCount,
            request.money, request.COD, request.stationeryId, request.packageId);
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_RECIPIENT_NOT_FOUND);
        return;
    }

    DEBUG_LOG("%s is sending mail to %s with subject %s and body %s includes %u items, " UI64FMTD " copper and " UI64FMTD " COD copper with unk1 = %u, unk2 = %u",
        pl->GetGuidStr().c_str(), rc.GetString().c_str(), request.subject.c_str(),
        request.body.c_str(), itemsCount, request.money, request.COD,
        request.stationeryId, request.packageId);

    if (pl->GetObjectGuid() == rc)
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANNOT_SEND_TO_SELF);
        return;
    }

    if ((request.money && request.COD) ||
        (request.COD && request.attachments.empty()) ||
        !MailMoneyPolicy::IsValidPlayerCod(request.COD))
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    uint64 const cost = itemsCount ? 30 * itemsCount : 30;  // price hardcoded in client
    if (!MailMoneyPolicy::CanDebitWithFee(pl->GetMoney(), request.money, cost))
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_NOT_ENOUGH_MONEY);
        return;
    }
    uint64 const nextMoney = pl->GetMoney() - cost - request.money;

    Player* receive = sObjectMgr.GetPlayer(rc);

    Team rc_team;
    uint32 mailsCount = 0;                                  // do not allow to send to one player more than 100 mails

    if (receive)
    {
        rc_team = receive->GetTeam();
        mailsCount = receive->GetMailSize();
    }
    else
    {
        rc_team = sObjectMgr.GetPlayerTeamByGUID(rc);
        if (QueryResult* result = CharacterDatabase.PQuery("SELECT COUNT(*) FROM `mail` WHERE `receiver` = '%u'", rc.GetCounter()))
        {
            Field* fields = result->Fetch();
            mailsCount = fields[0].GetUInt32();
            delete result;
        }
    }

    if (mailsCount >= 100)
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

    std::vector<Item*> items;
    items.reserve(itemsCount);
    for (uint8 i = 0; i < itemsCount; ++i)
    {
        MopCompactPackets::MailAttachmentRequest const& attachment =
            request.attachments[i];
        uint32 const itemGuidLow = attachment.itemGuid.GetCounter();
        if (attachment.slot >= MAX_MAIL_ITEMS || !itemGuidLow)
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
            return;
        }

        for (uint8 previous = 0; previous < i; ++previous)
        {
            if (request.attachments[previous].slot == attachment.slot ||
                request.attachments[previous].itemGuid.GetCounter() == itemGuidLow)
            {
                pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_MAIL_ATTACHMENT_INVALID);
                return;
            }
        }

        // MoP sends item GUIDs in its client wire domain. Inventory storage uses
        // the core's HIGHGUID_ITEM domain, while the low counter is shared.
        ObjectGuid const itemGuid(HIGHGUID_ITEM, itemGuidLow);
        Item* item = pl->GetItemByGuid(itemGuid);

        // The client attachment slots describe carried inventory, never bank or equipment.
        if (!item || !Player::IsInventoryPos(item->GetBagSlot(), item->GetSlot()))
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

        if (request.COD && item->HasFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_WRAPPED))
        {
            pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_CANT_SEND_WRAPPED_COD);
            return;
        }

        items.push_back(item);
    }

    bool needItemDelay = false;
    MailDraft draft(request.subject, request.body);
    if (!items.empty() || request.money > 0)
    {
        if (!items.empty())
        {
            for (uint8 i = 0; i < itemsCount; ++i)
            {
                Item* item = items[i];
                if (GetSecurity() > SEC_PLAYER && sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
                {
                    sLog.outCommand(GetAccountId(), "GM %s (Account: %u) mail item: %s (Entry: %u Count: %u) to player: %s (Account: %u)",
                        GetPlayerName(), GetAccountId(), item->GetProto()->Name1,
                        item->GetEntry(), item->GetCount(), request.receiver.c_str(),
                        rc_account);
                }
                draft.AddItem(item);
            }

            needItemDelay = pl->GetSession()->GetAccountId() != rc_account;
        }

        if (request.money > 0 && GetSecurity() > SEC_PLAYER &&
            sWorld.getConfig(CONFIG_BOOL_GM_LOG_TRADE))
        {
            sLog.outCommand(GetAccountId(), "GM %s (Account: %u) mail money: " UI64FMTD " to player: %s (Account: %u)",
                GetPlayerName(), GetAccountId(), request.money,
                request.receiver.c_str(), rc_account);
        }
    }

    uint32 const deliverDelay = needItemDelay ?
        sWorld.getConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY) : 0;
    MailReceiver deliveryReceiver(receive, rc);
    Mail* onlineMail = NULL;
    draft.SetMoney(request.money).SetCOD(request.COD);

    if (!CharacterDatabase.BeginTransaction())
    {
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    bool staged = CharacterDatabase.PExecute(
        "UPDATE `characters` SET `money` = '" UI64FMTD "' WHERE `guid` = '%u'",
        nextMoney, pl->GetGUIDLow());
    for (Item* item : items)
    {
        item->SaveToDB();
        staged = CharacterDatabase.PExecute(
            "DELETE FROM `character_inventory` WHERE `item` = '%u' AND `guid` = '%u'",
            item->GetGUIDLow(), pl->GetGUIDLow()) && staged;
        staged = CharacterDatabase.PExecute(
            "UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid` = '%u'",
            rc.GetCounter(), item->GetGUIDLow()) && staged;
    }
    staged = draft.StageMailToDB(deliveryReceiver, MailSender(pl),
        request.body.empty() ? MAIL_CHECK_MASK_COPIED : MAIL_CHECK_MASK_HAS_BODY,
        deliverDelay, onlineMail) && staged;
    if (!staged)
    {
        CharacterDatabase.RollbackTransaction();
        delete onlineMail;
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_INTERNAL_ERROR);
        if (!items.empty())
            KickPlayer();
        return;
    }
    if (!CharacterDatabase.CommitTransactionDirect())
    {
        delete onlineMail;
        sLog.outError("CMSG_SEND_MAIL: indeterminate commit for account %u, player %u, receiver %u",
            GetAccountId(), pl->GetGUIDLow(), rc.GetCounter());
        pl->SendMailResult(0, MAIL_SEND, MAIL_ERR_INTERNAL_ERROR);
        KickPlayer();
        return;
    }

    pl->SetMoney(nextMoney);
    for (Item* item : items)
        pl->MoveItemFromInventory(item->GetBagSlot(), item->GetSlot(), true);
    draft.CompleteMailDelivery(deliveryReceiver, onlineMail);
    pl->GetAchievementMgr().UpdateAchievementCriteria(
        ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_MAIL, cost);
    pl->SendMailResult(0, MAIL_SEND, MAIL_OK);
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
    MopCompactPackets::MailDeleteRequest request;
    if (!MopCompactPackets::ReadMailDelete(recv_data, request))
        return;

    Player* pl = _player;
    if ((request.deleteMode != 0 && request.deleteMode != 2 &&
            request.deleteMode != 3) || !CheckOpenedMailBox())
    {
        pl->SendMailResult(request.mailId, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    Mail* m = pl->GetMail(request.mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL) ||
        m->COD || m->money || !m->items.empty())
    {
        pl->SendMailResult(request.mailId, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    if (!CharacterDatabase.BeginTransaction())
    {
        pl->SendMailResult(request.mailId, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
        return;
    }
    bool const staged = CharacterDatabase.PExecute(
        "DELETE FROM `mail_items` WHERE `mail_id` = '%u' AND `receiver` = '%u'",
        request.mailId, pl->GetGUIDLow()) &&
        CharacterDatabase.PExecute(
            "DELETE FROM `mail` WHERE `id` = '%u' AND `receiver` = '%u'",
            request.mailId, pl->GetGUIDLow());
    if (!staged)
    {
        CharacterDatabase.RollbackTransaction();
        pl->SendMailResult(request.mailId, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
        return;
    }
    if (!CharacterDatabase.CommitTransactionDirect())
    {
        sLog.outError("CMSG_MAIL_DELETE: indeterminate commit for account %u, player %u, mail %u",
            GetAccountId(), pl->GetGUIDLow(), request.mailId);
        pl->SendMailResult(request.mailId, MAIL_DELETED, MAIL_ERR_INTERNAL_ERROR);
        KickPlayer();
        return;
    }

    m->state = MAIL_STATE_DELETED;
    pl->m_mailsUpdated = true;
    pl->SendMailResult(request.mailId, MAIL_DELETED, MAIL_OK);
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
    MopCompactPackets::MailReturnRequest request;
    if (!MopCompactPackets::ReadMailReturnToSender(recv_data, request))
        return;

    Player* pl = _player;
    if (!CheckOpenedMailBox())
    {
        pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER,
            MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    Mail* m = pl->GetMail(request.mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER,
            MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    ObjectGuid const expectedSender(HIGHGUID_PLAYER, m->sender);
    if (m->messageType != MAIL_NORMAL || !m->sender ||
        (m->checked & MAIL_CHECK_MASK_RETURNED) ||
        request.senderGuid.GetRawValue() !=
            MopMailPackets::BuildPlayerSenderGuid(m->sender) ||
        expectedSender == pl->GetObjectGuid())
    {
        pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER,
            MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    Player* returnReceiver = sObjectMgr.GetPlayer(expectedSender);
    uint32 const returnAccount = returnReceiver ?
        returnReceiver->GetSession()->GetAccountId() :
        sObjectMgr.GetPlayerAccountIdByGUID(expectedSender);
    if (!returnAccount)
    {
        pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER,
            MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    MailDraft draft;
    if (m->mailTemplateId)
        draft.SetMailTemplate(m->mailTemplateId, false);
    else
        draft.SetSubjectAndBody(m->subject, m->body);

    std::vector<Item*> returnedItems;
    returnedItems.reserve(m->items.size());
    for (MailItemInfo const& attachment : m->items)
    {
        Item* item = pl->GetMItem(attachment.item_guid);
        if (!item || item->GetEntry() != attachment.item_template)
        {
            pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER,
                MAIL_ERR_INTERNAL_ERROR);
            return;
        }
        returnedItems.push_back(item);
        draft.AddItem(item);
    }
    draft.SetMoney(m->money);

    uint32 const deliverDelay = !returnedItems.empty() &&
        GetAccountId() != returnAccount ?
        sWorld.getConfig(CONFIG_UINT32_MAIL_DELIVERY_DELAY) : 0;
    MailReceiver deliveryReceiver(returnReceiver, expectedSender);
    Mail* onlineMail = NULL;

    if (!CharacterDatabase.BeginTransaction())
    {
        pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER,
            MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    bool staged = CharacterDatabase.PExecute(
        "DELETE FROM `mail_items` WHERE `mail_id` = '%u' AND `receiver` = '%u'",
        request.mailId, pl->GetGUIDLow()) &&
        CharacterDatabase.PExecute(
            "DELETE FROM `mail` WHERE `id` = '%u' AND `receiver` = '%u'",
            request.mailId, pl->GetGUIDLow());
    for (Item* item : returnedItems)
    {
        item->SaveToDB();
        staged = CharacterDatabase.PExecute(
            "UPDATE `item_instance` SET `owner_guid` = '%u' WHERE `guid` = '%u'",
            expectedSender.GetCounter(), item->GetGUIDLow()) && staged;
    }
    staged = draft.StageMailToDB(deliveryReceiver,
        MailSender(MAIL_NORMAL, pl->GetGUIDLow()), MAIL_CHECK_MASK_RETURNED,
        deliverDelay, onlineMail) && staged;
    if (!staged)
    {
        CharacterDatabase.RollbackTransaction();
        delete onlineMail;
        pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER,
            MAIL_ERR_INTERNAL_ERROR);
        if (!returnedItems.empty())
            KickPlayer();
        return;
    }
    if (!CharacterDatabase.CommitTransactionDirect())
    {
        delete onlineMail;
        sLog.outError("CMSG_MAIL_RETURN_TO_SENDER: indeterminate commit for account %u, player %u, mail %u, sender %u",
            GetAccountId(), pl->GetGUIDLow(), request.mailId,
            expectedSender.GetCounter());
        pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER,
            MAIL_ERR_INTERNAL_ERROR);
        KickPlayer();
        return;
    }

    pl->RemoveMail(request.mailId);
    for (Item* item : returnedItems)
        pl->RemoveMItem(item->GetGUIDLow());
    draft.CompleteMailDelivery(deliveryReceiver, onlineMail);
    delete m;
    pl->SendMailResult(request.mailId, MAIL_RETURNED_TO_SENDER, MAIL_OK);
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
    if (MailTakeItemPolicy::HasTemplateCoherenceDrift(*m, pl->GetObjectGuid(),
            itemId, attachment, resolved))
    {
        sLog.outError("CMSG_MAIL_TAKE_ITEM: mail %u item %u has attachment template %u but resolved item template %u",
            mailId, itemId, attachment->item_template, resolved.itemTemplate);
    }

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
    MopCompactPackets::MailTakeMoneyRequest request;
    if (!MopCompactPackets::ReadMailTakeMoney(recv_data, request))
    {
        return;
    }

    if (!CheckMailBox(request.mailboxGuid))
    {
        return;
    }

    Player* pl = _player;
    uint32 const mailId = request.mailId;

    Mail* m = pl->GetMail(mailId);
    if (!m || m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL))
    {
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    uint64 const currentMoney = pl->GetMoney();
    uint64 const mailMoney = m->money;
    MailMoneyPolicy::MailMoneyTakePlan const plan =
        MailMoneyPolicy::PlanMailMoneyTake(currentMoney, mailMoney, MAX_MONEY_AMOUNT);
    if (plan.decision == MailMoneyPolicy::MailMoneyTakeDecision::InvalidBalance)
    {
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }
    if (plan.decision == MailMoneyPolicy::MailMoneyTakeDecision::GoldCapExceeded)
    {
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_EQUIP_ERROR,
            EQUIP_ERR_TOO_MUCH_GOLD);
        return;
    }
    if (mailMoney == 0)
    {
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_OK);
        return;
    }

    if (!CharacterDatabase.BeginTransaction())
    {
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }
    if (!pl->StageMailMoneyTakeToDB(mailId, plan.nextMoney))
    {
        CharacterDatabase.RollbackTransaction();
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        return;
    }
    if (!CharacterDatabase.CommitTransactionDirect())
    {
        sLog.outError("CMSG_MAIL_TAKE_MONEY: indeterminate commit for account %u, player %u, mail %u, currentMoney=" UI64FMTD ", mailMoney=" UI64FMTD,
            GetAccountId(), pl->GetGUIDLow(), mailId, currentMoney, mailMoney);
        pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_ERR_INTERNAL_ERROR);
        KickPlayer();
        return;
    }

    pl->SetMoney(plan.nextMoney);
    m->money = 0;
    m->state = MAIL_STATE_CHANGED;
    pl->m_mailsUpdated = true;
    pl->SendMailResult(mailId, MAIL_MONEY_TAKEN, MAIL_OK);
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
    MopCompactPackets::MailCreateTextItemRequest request;
    if (!MopCompactPackets::ReadMailCreateTextItem(recv_data, request))
        return;

    Player* pl = _player;
    if (!CheckMailBox(request.mailboxGuid))
    {
        pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT,
            MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    Mail* m = pl->GetMail(request.mailId);
    if (!m || (m->body.empty() && !m->mailTemplateId) ||
        m->state == MAIL_STATE_DELETED || m->deliver_time > time(NULL) ||
        (m->checked & MAIL_CHECK_MASK_COPIED))
    {
        pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT,
            MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    Item* bodyItem = new Item;                              // This is not bag and then can be used new Item.
    if (!bodyItem->Create(sObjectMgr.GenerateItemLowGuid(), MAIL_BODY_ITEM_TEMPLATE, pl))
    {
        delete bodyItem;
        pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT,
            MAIL_ERR_INTERNAL_ERROR);
        return;
    }

    // in mail template case we need create new item text
    if (m->mailTemplateId)
    {
        MailTemplateEntry const* mailTemplateEntry = sMailTemplateStore.LookupEntry(m->mailTemplateId);
        if (!mailTemplateEntry)
        {
            pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT,
                MAIL_ERR_INTERNAL_ERROR);
            delete bodyItem;
            return;
        }

        bodyItem->SetText(mailTemplateEntry->Body_lang[GetSessionDbcLocale()]);
    }
    else
    {
        bodyItem->SetText(m->body);
    }

    if (m->messageType == MAIL_NORMAL && m->sender)
        bodyItem->SetGuidValue(ITEM_FIELD_CREATOR,
            ObjectGuid(HIGHGUID_PLAYER, m->sender));
    bodyItem->SetFlag(ITEM_FIELD_FLAGS, ITEM_DYNFLAG_READABLE | ITEM_DYNFLAG_UNK15 | ITEM_DYNFLAG_UNK16);

    DETAIL_LOG("HandleMailCreateTextItem mailid=%u", request.mailId);

    ItemPosCountVec dest;
    InventoryResult msg = _player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, bodyItem, false);
    if (msg == EQUIP_ERR_OK)
    {
        uint32 const bodyItemGuid = bodyItem->GetGUIDLow();
        if (!CharacterDatabase.BeginTransaction())
        {
            delete bodyItem;
            pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT,
                MAIL_ERR_INTERNAL_ERROR);
            return;
        }

        uint32 const nextChecked = m->checked | MAIL_CHECK_MASK_COPIED;
        if (!CharacterDatabase.PExecute(
                "UPDATE `mail` SET `checked` = '%u' WHERE `id` = '%u' AND `receiver` = '%u'",
                nextChecked, request.mailId, pl->GetGUIDLow()))
        {
            CharacterDatabase.RollbackTransaction();
            delete bodyItem;
            pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT,
                MAIL_ERR_INTERNAL_ERROR);
            return;
        }

        // Enqueue the fallible mail update before exposing the new item to the
        // in-memory inventory and its logout save path.
        pl->StoreItem(dest, bodyItem, true);
        pl->SaveInventoryAndGoldToDB();
        if (!CharacterDatabase.CommitTransactionDirect())
        {
            // Commit status is indeterminate. Keep the in-memory mail and item
            // mutually consistent and requeue both for the forced logout save.
            // The item insert is idempotent because ITEM_NEW deletes its GUID
            // before inserting it again.
            bodyItem->SetState(ITEM_NEW, pl);
            m->checked = nextChecked;
            m->state = MAIL_STATE_CHANGED;
            pl->m_mailsUpdated = true;
            sLog.outError("CMSG_MAIL_CREATE_TEXT_ITEM: indeterminate commit for account %u, player %u, mail %u, item %u",
                GetAccountId(), pl->GetGUIDLow(), request.mailId,
                bodyItemGuid);
            pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT,
                MAIL_ERR_INTERNAL_ERROR);
            KickPlayer();
            return;
        }

        m->checked = nextChecked;
        m->state = MAIL_STATE_CHANGED;
        pl->m_mailsUpdated = true;
        pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT, MAIL_OK);
    }
    else
    {
        pl->SendMailResult(request.mailId, MAIL_MADE_PERMANENT,
            MAIL_ERR_EQUIP_ERROR, msg);
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
