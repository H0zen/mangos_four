/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Side-effect-free mail attachment take preflight policy.
 */

#ifndef MANGOS_MAIL_TAKE_ITEM_POLICY_H
#define MANGOS_MAIL_TAKE_ITEM_POLICY_H

#include "Mail.h"

namespace MailTakeItemPolicy
{
enum class Decision : uint8
{
    ProceedNonCod,
    RejectInternal
};

struct ResolvedItem
{
    bool exists = false;
    uint32 guidLow = 0;
    uint32 itemTemplate = 0;
};

inline MailItemInfo const* FindAttachment(Mail const& mail, uint32 itemGuidLow)
{
    for (MailItemInfo const& attachment : mail.items)
    {
        if (attachment.item_guid == itemGuidLow)
        {
            return &attachment;
        }
    }

    return NULL;
}

inline bool HasTemplateCoherenceDrift(Mail const& mail,
    ObjectGuid const& playerGuid,
    uint32 requestedItemGuidLow,
    MailItemInfo const* attachment,
    ResolvedItem const& item)
{
    return mail.receiverGuid == playerGuid &&
        attachment != NULL &&
        attachment->item_guid == requestedItemGuidLow &&
        item.exists &&
        item.guidLow == requestedItemGuidLow &&
        mail.COD == 0 &&
        item.itemTemplate != attachment->item_template;
}

inline Decision Evaluate(Mail const& mail,
    ObjectGuid const& playerGuid,
    uint32 requestedItemGuidLow,
    MailItemInfo const* attachment,
    ResolvedItem const& item)
{
    if (mail.receiverGuid != playerGuid ||
        attachment == NULL ||
        attachment->item_guid != requestedItemGuidLow ||
        !item.exists ||
        item.guidLow != requestedItemGuidLow ||
        item.itemTemplate != attachment->item_template ||
        mail.COD != 0)
    {
        return Decision::RejectInternal;
    }

    return Decision::ProceedNonCod;
}
}

#endif
