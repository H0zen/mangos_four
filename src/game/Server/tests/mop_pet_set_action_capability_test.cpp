/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Item.h"
#include "Pet.h"
#include "Database/DatabaseEnv.h"

#include <cstdio>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool SamePetSpells(PetSpellMap const& left, PetSpellMap const& right)
{
    if (left.size() != right.size())
        return false;
    for (PetSpellMap::const_iterator itr = left.begin(); itr != left.end(); ++itr)
    {
        PetSpellMap::const_iterator other = right.find(itr->first);
        if (other == right.end() || other->second.active != itr->second.active ||
            other->second.state != itr->second.state || other->second.type != itr->second.type)
            return false;
    }
    return true;
}

static void CheckPetCase(uint32 spellId, uint8 active, PetSpellState state, bool expected)
{
    Pet pet;
    if (spellId || active != ACT_DISABLED || state != PETSPELL_UNCHANGED)
        pet.m_spells[spellId] = PetSpell{ active, state, PETSPELL_NORMAL };
    PetSpellMap const beforeSpells = pet.m_spells;
    AutoSpellList const beforeAuto = pet.m_autospells;
    CHECK(pet.CanToggleAutocast(spellId) == expected);
    CHECK(SamePetSpells(pet.m_spells, beforeSpells));
    CHECK(pet.m_autospells == beforeAuto);
}

static void test_pet_capability()
{
    Pet empty;
    CHECK(!empty.CanToggleAutocast(0));
    CHECK(!empty.CanToggleAutocast(123));

    CheckPetCase(0, ACT_DISABLED, PETSPELL_NEW, false);
    PetSpellState const liveStates[] = { PETSPELL_UNCHANGED, PETSPELL_CHANGED, PETSPELL_NEW };
    for (PetSpellState state : liveStates)
    {
        CheckPetCase(123, ACT_DISABLED, state, true);
        CheckPetCase(123, ACT_ENABLED, state, true);
    }
    CheckPetCase(123, ACT_DISABLED, PETSPELL_REMOVED, false);
    CheckPetCase(123, ACT_ENABLED, PETSPELL_REMOVED, false);
    CheckPetCase(123, ACT_PASSIVE, PETSPELL_NEW, false);
    CheckPetCase(123, ACT_DECIDE, PETSPELL_NEW, false);
    CheckPetCase(123, ACT_COMMAND, PETSPELL_NEW, false);
    CheckPetCase(123, ACT_REACTION, PETSPELL_NEW, false);
    CheckPetCase(123, 0x82, PETSPELL_NEW, false);
}

static void CheckCharmCase(std::vector<std::pair<uint32, uint8>> const& entries,
    uint32 spellId, bool expected)
{
    CharmInfo charm(nullptr);
    for (size_t i = 0; i < entries.size(); ++i)
        charm.GetCharmSpell(uint8(i))->SetActionAndType(entries[i].first, ActiveStates(entries[i].second));

    uint32 before[CREATURE_MAX_SPELLS];
    for (uint8 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        before[i] = charm.GetCharmSpell(i)->packedData;

    CHECK(charm.CanToggleCreatureAutocast(spellId) == expected);
    for (uint8 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        CHECK(charm.GetCharmSpell(i)->packedData == before[i]);
}

static void test_charm_capability()
{
    CheckCharmCase({}, 0, false);
    CheckCharmCase({}, 123, false);
    CheckCharmCase({ { 123, ACT_DISABLED } }, 123, true);
    CheckCharmCase({ { 123, ACT_ENABLED } }, 123, true);
    CheckCharmCase({ { 123, ACT_DISABLED }, { 123, ACT_ENABLED } }, 123, true);
    CheckCharmCase({ { 123, ACT_PASSIVE }, { 123, ACT_ENABLED } }, 123, false);
    CheckCharmCase({ { 123, ACT_ENABLED }, { 123, ACT_DECIDE } }, 123, false);
    CheckCharmCase({ { 123, ACT_COMMAND } }, 123, false);
    CheckCharmCase({ { 123, ACT_REACTION } }, 123, false);
    CheckCharmCase({ { 123, 0x82 } }, 123, false);
    CheckCharmCase({ { 999, ACT_PASSIVE }, { 123, ACT_ENABLED } }, 123, true);
}

int main()
{
    test_pet_capability();
    test_charm_capability();
    if (g_fail)
        return 1;
    std::printf("mop_pet_set_action_capability: all checks passed\n");
    return 0;
}
