/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MoP 5.4.8 mail attachment authority policy tests.
 */

#include "MailTakeItemPolicy.h"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static ObjectGuid const receiver(HIGHGUID_PLAYER, 0x42u);

static Mail MakeMailA()
{
    Mail mail;
    mail.messageID = 0xA001u;
    mail.receiverGuid = receiver;
    mail.COD = 0;
    mail.AddItem(0xAA01u, 0x1001u);
    return mail;
}

static Mail MakeMailB()
{
    Mail mail;
    mail.messageID = 0xB002u;
    mail.receiverGuid = receiver;
    mail.COD = 50000;
    mail.AddItem(0xBB02u, 0x2002u);
    return mail;
}

static MailTakeItemPolicy::ResolvedItem const itemB = {
    true, 0xBB02u, 0x2002u
};

// Break caught: resolving an item independently of the selected mail.
static void test_cross_mail_item_is_rejected()
{
    Mail const mailA = MakeMailA();
    MailItemInfo const* const attachment =
        MailTakeItemPolicy::FindAttachment(mailA, 0xBB02u);

    CHECK(attachment == NULL);
    CHECK(MailTakeItemPolicy::Evaluate(mailA, receiver, 0xBB02u,
        attachment, itemB) == MailTakeItemPolicy::Decision::RejectInternal);
}

// Break caught: mutating mail state during preflight. COD is now a route, not a
// refusal -- the handler settles the payment -- but the preflight that selects
// that route must still be side-effect free, which is what the rest asserts.
static void test_cod_item_proceeds_without_mutation()
{
    Mail mailB = MakeMailB();
    MailItemInfo const* const attachment =
        MailTakeItemPolicy::FindAttachment(mailB, 0xBB02u);
    uint64 const codBefore = mailB.COD;
    size_t const itemCountBefore = mailB.items.size();
    uint32 const itemGuidBefore = mailB.items[0].item_guid;
    uint32 const itemTemplateBefore = mailB.items[0].item_template;
    size_t const removedCountBefore = mailB.removedItems.size();
    MailState const stateBefore = mailB.state;

    CHECK(MailTakeItemPolicy::Evaluate(mailB, receiver, 0xBB02u,
        attachment, itemB) == MailTakeItemPolicy::Decision::ProceedWithCod);
    CHECK(mailB.COD == codBefore);
    CHECK(mailB.items.size() == itemCountBefore);
    CHECK(mailB.items[0].item_guid == itemGuidBefore);
    CHECK(mailB.items[0].item_template == itemTemplateBefore);
    CHECK(mailB.removedItems.size() == removedCountBefore);
    CHECK(mailB.state == stateBefore);
}

// Break caught: rejecting the safe non-COD attachment path.
static void test_valid_non_cod_item_proceeds()
{
    Mail mailB = MakeMailB();
    mailB.COD = 0;
    MailItemInfo const* const attachment =
        MailTakeItemPolicy::FindAttachment(mailB, 0xBB02u);

    CHECK(MailTakeItemPolicy::Evaluate(mailB, receiver, 0xBB02u,
        attachment, itemB) == MailTakeItemPolicy::Decision::ProceedNonCod);
}

// Break caught: accepting an attachment for a different receiver.
static void test_receiver_mismatch_is_rejected()
{
    Mail mailB = MakeMailB();
    mailB.COD = 0;
    ObjectGuid const otherReceiver(HIGHGUID_PLAYER, 0x43u);
    MailItemInfo const* const attachment =
        MailTakeItemPolicy::FindAttachment(mailB, 0xBB02u);

    CHECK(MailTakeItemPolicy::Evaluate(mailB, otherReceiver, 0xBB02u,
        attachment, itemB) == MailTakeItemPolicy::Decision::RejectInternal);
}

// Break caught: accepting a missing, differently identified, or wrong-template item.
static void test_missing_or_mismatched_item_is_rejected()
{
    Mail mailB = MakeMailB();
    mailB.COD = 0;
    MailItemInfo const* const attachment =
        MailTakeItemPolicy::FindAttachment(mailB, 0xBB02u);
    MailTakeItemPolicy::ResolvedItem const invalidItems[] = {
        { false, 0xBB02u, 0x2002u },
        { true, 0xBB03u, 0x2002u },
        { true, 0xBB02u, 0x2003u }
    };

    for (MailTakeItemPolicy::ResolvedItem const& item : invalidItems)
    {
        CHECK(MailTakeItemPolicy::Evaluate(mailB, receiver, 0xBB02u,
            attachment, item) == MailTakeItemPolicy::Decision::RejectInternal);
    }
}

// Break caught: real non-COD template drift being rejected without a diagnostic.
static void test_non_cod_template_drift_is_diagnostic()
{
    Mail mailB = MakeMailB();
    mailB.COD = 0;
    MailItemInfo const* const attachment =
        MailTakeItemPolicy::FindAttachment(mailB, 0xBB02u);
    MailTakeItemPolicy::ResolvedItem const driftedItem = {
        true, 0xBB02u, 0x2003u
    };

    CHECK(MailTakeItemPolicy::HasTemplateCoherenceDrift(mailB, receiver,
        0xBB02u, attachment, driftedItem));
}

// Break caught: attacker-driven cross-mail/authority rejects reaching logs.
// A COD mail is deliberately NOT exempt any more: template drift is a genuine
// integrity fault worth a log line whether or not money is attached, and the
// attacker cannot choose it, so it is covered by the drift test instead.
static void test_attacker_driven_rejections_are_not_diagnostic()
{
    Mail mailA = MakeMailA();
    Mail mailB = MakeMailB();
    MailItemInfo const* const attachmentB =
        MailTakeItemPolicy::FindAttachment(mailB, 0xBB02u);
    MailTakeItemPolicy::ResolvedItem const driftedItem = {
        true, 0xBB02u, 0x2003u
    };

    CHECK(!MailTakeItemPolicy::HasTemplateCoherenceDrift(mailA, receiver,
        0xBB02u, MailTakeItemPolicy::FindAttachment(mailA, 0xBB02u), driftedItem));

    mailB.COD = 0;
    CHECK(!MailTakeItemPolicy::HasTemplateCoherenceDrift(mailB,
        ObjectGuid(HIGHGUID_PLAYER, 0x43u), 0xBB02u, attachmentB, driftedItem));
    CHECK(!MailTakeItemPolicy::HasTemplateCoherenceDrift(mailB, receiver,
        0xBB02u, attachmentB, { true, 0xBB03u, 0x2003u }));
    CHECK(!MailTakeItemPolicy::HasTemplateCoherenceDrift(mailB, receiver,
        0xBB02u, attachmentB, { false, 0xBB02u, 0x2003u }));
}

int main()
{
    test_cross_mail_item_is_rejected();
    test_cod_item_proceeds_without_mutation();
    test_valid_non_cod_item_proceeds();
    test_receiver_mismatch_is_rejected();
    test_missing_or_mismatched_item_is_rejected();
    test_non_cod_template_drift_is_diagnostic();
    test_attacker_driven_rejections_are_not_diagnostic();
    return g_fail == 0 ? 0 : 1;
}
