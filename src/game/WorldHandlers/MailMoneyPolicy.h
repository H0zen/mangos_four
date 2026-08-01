/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Side-effect-free mail money domain predicates.
 */

#ifndef MANGOS_MAIL_MONEY_POLICY_H
#define MANGOS_MAIL_MONEY_POLICY_H

#include "Common.h"

namespace MailMoneyPolicy
{
constexpr uint64 MAX_PLAYER_COD = UI64LIT(100000000);

inline bool IsValidPlayerCod(uint64 cod)
{
    return cod <= MAX_PLAYER_COD;
}

inline bool CanDebitWithFee(uint64 current, uint64 amount, uint64 fee)
{
    return fee <= current && amount <= current - fee;
}

inline bool CanCredit(uint64 current, uint64 amount, uint64 limit)
{
    return current <= limit && amount <= limit - current;
}
}

#endif
