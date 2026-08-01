/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 */

#include "ObjectMgr.h"
#include "PhaseDefinition.h"
#include "Object.h"

#include <cstdint>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static PhaseDefinitionRecord MakeRecord()
{
    PhaseDefinitionRecord record = {};
    record.zoneId = 1;
    record.entry = 1;
    record.phaseMask = 0x0001;
    return record;
}

static PhaseDefinition MakeDefinition(PhaseDefinitionRecord record, bool conditionExists = true)
{
    PhaseDefinition definition = {};
    CHECK(ValidatePhaseDefinition(record, conditionExists, definition) == PhaseDefinitionValidationResult::Success);
    return definition;
}

static void TestDefinitionEvaluationAndComposition()
{
    PhaseDefinitionRecord baseline = MakeRecord();

    PhaseDefinitionRecord pre = MakeRecord();
    pre.phaseMask = 0x4000;
    pre.conditionId = 100;

    PhaseDefinitionRecord post = MakeRecord();
    post.phaseMask = 0x8000;
    post.conditionId = 101;

    std::vector<PhaseDefinition> definitions;
    definitions.push_back(MakeDefinition(baseline));
    definitions.push_back(MakeDefinition(pre));
    definitions.push_back(MakeDefinition(post));

    uint32 preMask = EvaluatePhaseDefinitions(definitions, [](uint16 conditionId) { return conditionId == 100; });
    uint32 postMask = EvaluatePhaseDefinitions(definitions, [](uint16 conditionId) { return conditionId == 101; });
    CHECK(preMask == 0x4001);
    CHECK(postMask == 0x8001);

    CHECK(ComposePhaseMask(false, false, 0, postMask, 0x0002) == 0x8003);
    CHECK(ComposePhaseMask(false, false, 0, postMask, 0x0004) == 0x8005);
    CHECK(ComposePhaseMask(true, true, 0, postMask, 0x0004) == PHASEMASK_ANYWHERE);
    CHECK(ComposePhaseMask(false, true, 0, postMask, 0x0004) == 0);
    CHECK(ComposePhaseMask(false, true, 0x1234, postMask, 0x0004) == 0x1234);
    CHECK(ComposePhaseMask(false, false, 0, 0, 0) == PHASEMASK_NORMAL);

    CHECK(PhaseMasksIntersect(preMask, 0x4000));
    CHECK(!PhaseMasksIntersect(preMask, 0x8000));
    CHECK(PhaseMasksIntersect(postMask, 0x8000));
    CHECK(!PhaseMasksIntersect(postMask, 0x4000));
}

static void TestValidation()
{
    PhaseDefinition definition = {9, 9, 9, 9};
    PhaseDefinitionRecord record = MakeRecord();

    record.zoneId = 0;
    CHECK(ValidatePhaseDefinition(record, true, definition) == PhaseDefinitionValidationResult::ZoneIdZero);
    CHECK(definition.zoneId == 9 && definition.entry == 9 && definition.phaseMask == 9 && definition.conditionId == 9);
    record = MakeRecord();
    record.entry = 0;
    CHECK(ValidatePhaseDefinition(record, true, definition) == PhaseDefinitionValidationResult::EntryZero);
    record = MakeRecord();
    record.phaseMask = 0;
    CHECK(ValidatePhaseDefinition(record, true, definition) == PhaseDefinitionValidationResult::PhaseMaskZero);
    record = MakeRecord();
    record.phaseMask = UINT64_C(0x10000);
    CHECK(ValidatePhaseDefinition(record, true, definition) == PhaseDefinitionValidationResult::PhaseMaskOutOfRange);
    record = MakeRecord();
    record.conditionId = UINT32_C(0x10000);
    CHECK(ValidatePhaseDefinition(record, true, definition) == PhaseDefinitionValidationResult::ConditionIdOutOfRange);
    record = MakeRecord();
    record.conditionId = 1;
    CHECK(ValidatePhaseDefinition(record, false, definition) == PhaseDefinitionValidationResult::ConditionMissing);
    record = MakeRecord();
    record.phaseId = 1;
    CHECK(ValidatePhaseDefinition(record, true, definition) == PhaseDefinitionValidationResult::PhaseIdNonZero);
    record = MakeRecord();
    record.terrainSwapMap = 1;
    CHECK(ValidatePhaseDefinition(record, true, definition) == PhaseDefinitionValidationResult::TerrainSwapMapNonZero);
    record = MakeRecord();
    record.flags = 1;
    CHECK(ValidatePhaseDefinition(record, true, definition) == PhaseDefinitionValidationResult::FlagsNonZero);
}

static void TestUnconditionalDefinitionSkipsConditionEvaluator()
{
    PhaseDefinitionRecord record = MakeRecord();
    PhaseDefinition definition = MakeDefinition(record, false);
    bool evaluatorCalled = false;
    CHECK(EvaluatePhaseDefinitions({definition}, [&evaluatorCalled](uint16) { evaluatorCalled = true; return false; }) == PHASEMASK_NORMAL);
    CHECK(!evaluatorCalled);
}

static void TestPublishedStoreKeepsQueryOrderAndRejectsInvalidRows()
{
    PhaseDefinitionStore stagedStore;

    PhaseDefinitionRecord first = MakeRecord();
    first.zoneId = 7;
    first.entry = 10;
    PhaseDefinition firstDefinition = MakeDefinition(first);
    stagedStore[first.zoneId].push_back(firstDefinition);

    PhaseDefinitionRecord rejected = MakeRecord();
    rejected.zoneId = 7;
    rejected.entry = 15;
    rejected.conditionId = 42;
    PhaseDefinition rejectedDefinition = {};
    CHECK(ValidatePhaseDefinition(rejected, false, rejectedDefinition) == PhaseDefinitionValidationResult::ConditionMissing);

    PhaseDefinitionRecord second = MakeRecord();
    second.zoneId = 7;
    second.entry = 20;
    PhaseDefinition secondDefinition = MakeDefinition(second);
    stagedStore[second.zoneId].push_back(secondDefinition);

    PhaseDefinitionContainer const& definitions = stagedStore[7];
    CHECK(definitions.size() == 2);
    CHECK(definitions[0].entry == 10);
    CHECK(definitions[1].entry == 20);
}

int main()
{
    TestDefinitionEvaluationAndComposition();
    TestValidation();
    TestUnconditionalDefinitionSkipsConditionEvaluator();
    TestPublishedStoreKeepsQueryOrderAndRejectsInvalidRows();
    if (g_fail != 0)
        return 1;

    std::printf("mop_player_phase: all checks passed\\n");
    return 0;
}
