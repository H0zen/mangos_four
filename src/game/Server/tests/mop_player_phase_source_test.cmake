if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(loader_path "${SOURCE_ROOT}/src/game/Object/ObjectMgrPhases.cpp")
if(NOT EXISTS "${loader_path}")
    message(FATAL_ERROR "phase definition loader is absent: ${loader_path}")
endif()

file(READ "${loader_path}" loader_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/World.cpp" world_source)
string(REPLACE "\r\n" "\n" world_source "${world_source}")

if(DEFINED MUTATION)
    set(original_loader_source "${loader_source}")
    set(original_world_source "${world_source}")

    if(MUTATION STREQUAL "startup_order")
        string(REPLACE "sObjectMgr.LoadPhaseDefinitions();"
            "/* removed phase-definition startup hook */" world_source "${world_source}")
    elseif(MUTATION STREQUAL "query_columns")
        string(REPLACE "SELECT `zoneId`, `entry`, `phasemask`, `phaseId`, `terrainswapmap`, `flags`, `condition_id` FROM `phase_definitions` ORDER BY `zoneId`, `entry`"
            "SELECT `entry`, `zoneId`, `phasemask`, `phaseId`, `terrainswapmap`, `flags`, `condition_id` FROM `phase_definitions` ORDER BY `zoneId`, `entry`"
            loader_source "${loader_source}")
    elseif(MUTATION STREQUAL "temporary_store")
        string(REPLACE "PhaseDefinitionStore phaseDefinitionStore;"
            "PhaseDefinitionStore& phaseDefinitionStore = _PhaseDefinitionStore;"
            loader_source "${loader_source}")
    elseif(MUTATION STREQUAL "publish_once")
        string(REPLACE "_PhaseDefinitionStore.swap(phaseDefinitionStore);"
            "_PhaseDefinitionStore.clear();"
            loader_source "${loader_source}")
    elseif(MUTATION STREQUAL "condition_zero")
        string(REPLACE "if (record.conditionId != 0 && record.conditionId <= UINT16_MAX)"
            "if (record.conditionId <= UINT16_MAX)" loader_source "${loader_source}")
    elseif(MUTATION STREQUAL "missing_condition")
        string(REPLACE "sConditionStorage.LookupEntry<PlayerCondition>(record.conditionId) != NULL"
            "true" loader_source "${loader_source}")
    elseif(MUTATION STREQUAL "empty_result")
        string(REPLACE "if (!result)" "if (result)" loader_source "${loader_source}")
    else()
        message(FATAL_ERROR "unknown MUTATION=${MUTATION}")
    endif()

    if("${loader_source}" STREQUAL "${original_loader_source}" AND "${world_source}" STREQUAL "${original_world_source}")
        message(STATUS "MUTATION '${MUTATION}' rewrote nothing; the mutation arm is dead.")
        return()
    endif()
endif()

function(require_once source token context)
    string(FIND "${source}" "${token}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "${context}: required token missing: ${token}")
    endif()
    math(EXPR next "${first} + 1")
    string(SUBSTRING "${source}" ${next} -1 tail)
    string(FIND "${tail}" "${token}" duplicate)
    if(NOT duplicate EQUAL -1)
        message(FATAL_ERROR "${context}: duplicate token: ${token}")
    endif()
endfunction()

require_once("${world_source}" "sObjectMgr.LoadConditions();" "condition startup hook")
require_once("${world_source}" "sObjectMgr.LoadPhaseDefinitions();" "phase-definition startup hook")
string(FIND "${world_source}" "sObjectMgr.LoadConditions();" conditions_position)
string(FIND "${world_source}" "sObjectMgr.LoadPhaseDefinitions();" phases_position)
if(phases_position LESS conditions_position)
    message(FATAL_ERROR "phase definitions must load after conditions")
endif()
math(EXPR startup_gap_length "${phases_position} - ${conditions_position}")
string(SUBSTRING "${world_source}" ${conditions_position} ${startup_gap_length} startup_gap)
set(expected_startup_gap "sObjectMgr.LoadConditions();\n    sLog.outString(\"Loading Phase Definitions...\");\n    ")
if(NOT startup_gap STREQUAL expected_startup_gap)
    message(FATAL_ERROR "phase definitions must load immediately after conditions")
endif()

require_once("${loader_source}" "SELECT `zoneId`, `entry`, `phasemask`, `phaseId`, `terrainswapmap`, `flags`, `condition_id` FROM `phase_definitions` ORDER BY `zoneId`, `entry`" "ordered phase-definition query")
require_once("${loader_source}" "PhaseDefinitionStore phaseDefinitionStore;" "temporary phase-definition store")
require_once("${loader_source}" "_PhaseDefinitionStore.swap(phaseDefinitionStore);" "atomic phase-definition publication")
require_once("${loader_source}" "if (record.conditionId != 0 && record.conditionId <= UINT16_MAX)" "conditional condition lookup guard")
require_once("${loader_source}" "sConditionStorage.LookupEntry<PlayerCondition>(record.conditionId) != NULL" "missing-condition lookup")
require_once("${loader_source}" "ValidatePhaseDefinition(record, conditionExists, definition)" "row validation")
require_once("${loader_source}" "if (!result)" "empty-result branch")

string(FIND "${loader_source}" "PhaseDefinitionStore phaseDefinitionStore;" temporary_store_position)
string(FIND "${loader_source}" "_PhaseDefinitionStore.swap(phaseDefinitionStore);" publish_position)
if(publish_position LESS temporary_store_position)
    message(FATAL_ERROR "phase-definition store must publish after loading")
endif()

string(FIND "${loader_source}" "if (!result)" empty_result_position)
if(publish_position LESS empty_result_position)
    message(FATAL_ERROR "empty phase-definition result must publish the temporary store")
endif()
