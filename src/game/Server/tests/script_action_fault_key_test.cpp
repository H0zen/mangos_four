/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ScriptActionFaultKey.h"

#include <set>

int main()
{
    using ScriptActionDetail::MakeFaultKey;
    using ScriptActionDetail::FaultKey;

    FaultKey const original = MakeFaultKey(1, 2, 3, 4, 5, 6);
    if (original != MakeFaultKey(1, 2, 3, 4, 5, 6))
    {
        return 1;
    }

    std::set<FaultKey> distinctFaults;
    distinctFaults.insert(original);
    distinctFaults.insert(MakeFaultKey(7, 2, 3, 4, 5, 6));
    distinctFaults.insert(MakeFaultKey(1, 7, 3, 4, 5, 6));
    distinctFaults.insert(MakeFaultKey(1, 2, 7, 4, 5, 6));
    distinctFaults.insert(MakeFaultKey(1, 2, 3, 7, 5, 6));
    distinctFaults.insert(MakeFaultKey(1, 2, 3, 4, 7, 6));
    distinctFaults.insert(MakeFaultKey(1, 2, 3, 4, 5, 7));

    return distinctFaults.size() == 7 ? 0 : 1;
}
