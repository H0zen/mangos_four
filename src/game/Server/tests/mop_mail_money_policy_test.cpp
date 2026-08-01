#include "MailMoneyPolicy.h"

#include <cstdio>
#include <limits>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void TestCodDomain()
{
    CHECK(MailMoneyPolicy::IsValidPlayerCod(0));
    CHECK(MailMoneyPolicy::IsValidPlayerCod(1));
    CHECK(MailMoneyPolicy::IsValidPlayerCod(100000000));
    CHECK(!MailMoneyPolicy::IsValidPlayerCod(100000001));
    CHECK(!MailMoneyPolicy::IsValidPlayerCod(std::numeric_limits<uint32>::max()));
    CHECK(!MailMoneyPolicy::IsValidPlayerCod(UI64LIT(0x100000000)));
    CHECK(!MailMoneyPolicy::IsValidPlayerCod(uint64(std::numeric_limits<int64>::max())));
    CHECK(!MailMoneyPolicy::IsValidPlayerCod(std::numeric_limits<uint64>::max()));
}

static void TestCreditHeadroom()
{
    uint64 const limit = UI64LIT(9999999999);
    uint64 const aboveU32 = UI64LIT(0x100000000);

    CHECK(MailMoneyPolicy::CanCredit(0, 0, limit));
    CHECK(MailMoneyPolicy::CanCredit(0, std::numeric_limits<uint32>::max(), limit));
    CHECK(MailMoneyPolicy::CanCredit(0, aboveU32, limit));
    CHECK(MailMoneyPolicy::CanCredit(limit - aboveU32, aboveU32, limit));
    CHECK(MailMoneyPolicy::CanCredit(limit - 1, 1, limit));
    CHECK(!MailMoneyPolicy::CanCredit(limit - 1, 2, limit));
    CHECK(!MailMoneyPolicy::CanCredit(0, limit + 1, limit));
    CHECK(!MailMoneyPolicy::CanCredit(limit + 1, 0, limit));
    CHECK(!MailMoneyPolicy::CanCredit(0, uint64(std::numeric_limits<int64>::max()), limit));
    CHECK(!MailMoneyPolicy::CanCredit(0, std::numeric_limits<uint64>::max(), limit));
}

static void TestDebitWithFee()
{
    uint64 const limit = UI64LIT(9999999999);
    CHECK(MailMoneyPolicy::CanDebitWithFee(limit, limit - 30, 30));
    CHECK(!MailMoneyPolicy::CanDebitWithFee(limit, limit - 29, 30));
    CHECK(MailMoneyPolicy::CanDebitWithFee(UI64LIT(0x100000020), UI64LIT(0x100000000), 32));
    CHECK(!MailMoneyPolicy::CanDebitWithFee(29, 0, 30));
    CHECK(!MailMoneyPolicy::CanDebitWithFee(
        std::numeric_limits<uint64>::max(),
        std::numeric_limits<uint64>::max(), 1));
    CHECK(!MailMoneyPolicy::CanDebitWithFee(
        std::numeric_limits<uint64>::max(), 1,
        std::numeric_limits<uint64>::max()));
}

static void TestMailMoneyTakePlan()
{
    uint64 const limit = UI64LIT(9999999999);

    MailMoneyPolicy::MailMoneyTakePlan plan =
        MailMoneyPolicy::PlanMailMoneyTake(limit - 5000, 5000, limit);
    CHECK(plan.decision == MailMoneyPolicy::MailMoneyTakeDecision::Success);
    CHECK(plan.nextMoney == limit);

    plan = MailMoneyPolicy::PlanMailMoneyTake(limit - 1000, 2000, limit);
    CHECK(plan.decision == MailMoneyPolicy::MailMoneyTakeDecision::GoldCapExceeded);
    CHECK(plan.nextMoney == limit - 1000);

    plan = MailMoneyPolicy::PlanMailMoneyTake(limit + 1, 0, limit);
    CHECK(plan.decision == MailMoneyPolicy::MailMoneyTakeDecision::InvalidBalance);
    CHECK(plan.nextMoney == limit + 1);

    plan = MailMoneyPolicy::PlanMailMoneyTake(123, 0, limit);
    CHECK(plan.decision == MailMoneyPolicy::MailMoneyTakeDecision::Success);
    CHECK(plan.nextMoney == 123);

    plan = MailMoneyPolicy::PlanMailMoneyTake(0, UI64LIT(0x100000000), limit);
    CHECK(plan.decision == MailMoneyPolicy::MailMoneyTakeDecision::Success);
    CHECK(plan.nextMoney == UI64LIT(0x100000000));

    plan = MailMoneyPolicy::PlanMailMoneyTake(
        0, std::numeric_limits<uint64>::max(), limit);
    CHECK(plan.decision == MailMoneyPolicy::MailMoneyTakeDecision::GoldCapExceeded);
    CHECK(plan.nextMoney == 0);
}

int main()
{
    TestCodDomain();
    TestCreditHeadroom();
    TestDebitWithFee();
    TestMailMoneyTakePlan();
    return g_fail == 0 ? 0 : 1;
}
