file(READ "${SOURCE_ROOT}/src/game/Server/DBCStructure.h" dbc_structure)
file(READ "${SOURCE_ROOT}/src/game/Server/DBCStores.cpp" dbc_stores)
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectMgrTaxi.cpp" object_mgr)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerTaxi.h" player_taxi_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerTaxi.cpp" player_taxi_source)

macro(mutate variable old new context)
    set(before "${${variable}}")
    string(REPLACE "${old}" "${new}" ${variable} "${${variable}}")
    if("${${variable}}" STREQUAL "${before}")
        message(FATAL_ERROR "${context} mutation setup guard: target not found")
    endif()
endmacro()

if(MUTATION STREQUAL "restore_114")
    mutate(dbc_structure "#define TaxiMaskSize 162" "#define TaxiMaskSize 114" "mask capacity")
elseif(MUTATION STREQUAL "required_bytes_overflow")
    mutate(dbc_structure
        "return size_t(maxNodeId / 8U) + (maxNodeId % 8U == 0 ? 0U : 1U);"
        "return (size_t(maxNodeId) + 7U) / 8U;"
        "overflow-safe ceiling division")
elseif(MUTATION STREQUAL "drop_zero_guard")
    mutate(dbc_structure "nodeId == 0 || " "" "zero guard")
elseif(MUTATION STREQUAL "drop_upper_guard")
    mutate(dbc_structure "nodeId > uint32(TaxiMaskSize) * 8U" "false" "upper guard")
elseif(MUTATION STREQUAL "narrow_index")
    mutate(dbc_structure "position.byteIndex = zeroBased / 8U;" "position.byteIndex = uint8(zeroBased / 8U);" "non-narrowing byte index")
elseif(MUTATION STREQUAL "startup_guard_missing")
    mutate(dbc_stores "if (taxiMaskBytesRequired > TaxiMaskSize)" "if (false)" "startup guard")
elseif(MUTATION STREQUAL "startup_guard_after_global_writes")
    mutate(dbc_stores "LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiNodesStore,           dbcPath, \"TaxiNodes.dbc\");\n\n    uint32 maxTaxiNodeId"
        "LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiNodesStore,           dbcPath, \"TaxiNodes.dbc\");\n\n    memset(sTaxiNodesMask, 0, sizeof(sTaxiNodesMask));\n    sTaxiNodesMask[0] = 1;\n\n    uint32 maxTaxiNodeId"
        "startup guard order")
elseif(MUTATION STREQUAL "startup_record_count")
    mutate(dbc_stores "maxTaxiNodeId = std::max(maxTaxiNodeId, node->ID);" "maxTaxiNodeId = sTaxiNodesStore.GetNumRows();" "actual maximum node id")
elseif(MUTATION STREQUAL "global_write_unchecked")
    mutate(dbc_stores "if (!GetTaxiMaskPosition(node->ID, maskPosition))" "if (false)" "global write mapping")
elseif(MUTATION STREQUAL "nearest_read_unchecked")
    mutate(object_mgr "if (!GetTaxiMaskPosition(node->ID, maskPosition))" "if (false)" "nearest-node mapping")
elseif(MUTATION STREQUAL "player_known_unchecked")
    mutate(player_taxi_header
        "if (!IsValidNodeId(nodeidx))\n            {\n                return false;\n            }\n\n            TaxiMaskPosition position = {};\n            GetTaxiMaskPosition(nodeidx, position);\n            return (m_taximask[position.byteIndex] & position.bitMask) == position.bitMask;"
        "TaxiMaskPosition position = {};\n            GetTaxiMaskPosition(nodeidx, position);\n            return (m_taximask[position.byteIndex] & position.bitMask) == position.bitMask;"
        "known-node validation")
elseif(MUTATION STREQUAL "player_set_unchecked")
    mutate(player_taxi_header
        "if (!IsValidNodeId(nodeidx))\n            {\n                return false;\n            }\n\n            TaxiMaskPosition position = {};\n            GetTaxiMaskPosition(nodeidx, position);\n            if ((m_taximask[position.byteIndex] & position.bitMask) != position.bitMask)"
        "TaxiMaskPosition position = {};\n            GetTaxiMaskPosition(nodeidx, position);\n            if ((m_taximask[position.byteIndex] & position.bitMask) != position.bitMask)"
        "set-node validation")
elseif(MUTATION STREQUAL "load_no_clear")
    mutate(player_taxi_source "memset(m_taximask, 0, sizeof(m_taximask));" "" "load state clear")
elseif(MUTATION STREQUAL "load_legacy_limit")
    mutate(player_taxi_source "index < TaxiMaskSize" "index < 114" "load capacity")
elseif(MUTATION STREQUAL "append_all_narrow_loop")
    mutate(player_taxi_source
        "for (size_t i = 0; i < TaxiMaskSize; ++i)\n        {\n            data << uint8(sTaxiNodesMask[i]);"
        "for (uint8 i = 0; i < TaxiMaskSize; ++i)\n        {\n            data << uint8(sTaxiNodesMask[i]);"
        "all-nodes append loop width")
elseif(MUTATION STREQUAL "append_known_narrow_loop")
    mutate(player_taxi_source
        "for (size_t i = 0; i < TaxiMaskSize; ++i)\n        {\n            data << uint8(m_taximask[i]);"
        "for (uint8 i = 0; i < TaxiMaskSize; ++i)\n        {\n            data << uint8(m_taximask[i]);"
        "known-nodes append loop width")
elseif(MUTATION STREQUAL "save_narrow_loop")
    mutate(player_taxi_source
        "for (size_t i = 0; i < TaxiMaskSize; ++i)\n    {\n        ss << uint32(taxi.m_taximask[i])"
        "for (uint8 i = 0; i < TaxiMaskSize; ++i)\n    {\n        ss << uint32(taxi.m_taximask[i])"
        "save loop width")
endif()

function(require_text source token context)
    string(FIND "${source}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${context} guard: required text not found")
    endif()
endfunction()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context} guard: expected one match, found ${count}")
    endif()
endfunction()

require_text("${dbc_structure}" "#define TaxiMaskSize 162" "162-byte capacity")
require_text("${dbc_structure}" "TaxiMaskRequiredBytes" "ceiling-division helper")
require_text("${dbc_structure}" "return size_t(maxNodeId / 8U) + (maxNodeId % 8U == 0 ? 0U : 1U);" "overflow-safe ceiling division")
require_text("${dbc_structure}" "nodeId == 0 || nodeId > uint32(TaxiMaskSize) * 8U" "closed node-id domain")
require_text("${dbc_structure}" "position.byteIndex = zeroBased / 8U;" "non-narrowing byte index")
require_text("${dbc_structure}" "position.bitMask = uint8(1U << (zeroBased % 8U));" "bit mapping")

string(FIND "${dbc_stores}" "LoadDBC(availableDbcLocales, bar, bad_dbc_files, sTaxiNodesStore,           dbcPath, \"TaxiNodes.dbc\");" taxi_load)
string(FIND "${dbc_stores}" "if (taxiMaskBytesRequired > TaxiMaskSize)" startup_guard)
string(FIND "${dbc_stores}" "memset(sTaxiNodesMask, 0, sizeof(sTaxiNodesMask));" first_global_write)
if(taxi_load EQUAL -1 OR startup_guard LESS_EQUAL taxi_load OR first_global_write LESS_EQUAL startup_guard)
    message(FATAL_ERROR "startup domain guard ordering guard: load/guard/global-write order is wrong")
endif()
require_text("${dbc_stores}" "if (TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i))\n        {\n            maxTaxiNodeId = std::max(maxTaxiNodeId, node->ID);" "actual loaded maximum node id")
require_text("${dbc_stores}" "TaxiMaskRequiredBytes(maxTaxiNodeId)" "startup ceiling division")
require_text("${dbc_stores}" "if (!GetTaxiMaskPosition(node->ID, maskPosition))" "global writes checked mapping")
require_text("${dbc_stores}" "sTaxiNodesMask[maskPosition.byteIndex]" "global node mask position")
require_text("${dbc_stores}" "sOldContinentsNodesMask[maskPosition.byteIndex]" "old-continent mask position")
require_text("${dbc_stores}" "sHordeTaxiNodesMask[maskPosition.byteIndex]" "horde mask position")
require_text("${dbc_stores}" "sAllianceTaxiNodesMask[maskPosition.byteIndex]" "alliance mask position")
require_text("${dbc_stores}" "sDeathKnightTaxiNodesMask[maskPosition.byteIndex]" "death-knight mask position")

require_text("${object_mgr}" "if (!GetTaxiMaskPosition(node->ID, maskPosition))" "nearest-node checked mapping")
require_text("${object_mgr}" "sTaxiNodesMask[maskPosition.byteIndex]" "nearest-node checked read")

require_text("${player_taxi_header}" "bool IsValidNodeId(uint32 nodeidx) const" "player node validator")
string(REGEX MATCHALL "if [(][!]IsValidNodeId[(]nodeidx[)][)]" validation_guards "${player_taxi_header}")
list(LENGTH validation_guards validation_guard_count)
if(NOT validation_guard_count EQUAL 2)
    message(FATAL_ERROR "known/set validation guard: expected two guards, found ${validation_guard_count}")
endif()
require_text("${player_taxi_header}" "GetTaxiMaskPosition(nodeidx, position)" "player checked mapping")
require_text("${player_taxi_source}" "memset(m_taximask, 0, sizeof(m_taximask));" "load clears stale tail")
require_text("${player_taxi_source}" "index < TaxiMaskSize" "load uses current capacity")

string(FIND "${player_taxi_source}" "void PlayerTaxi::AppendTaximaskTo" append_start)
string(FIND "${player_taxi_source}" "bool PlayerTaxi::LoadTaxiDestinationsFromString" append_end)
if(append_start EQUAL -1 OR append_end LESS_EQUAL append_start)
    message(FATAL_ERROR "append serializer seam guard: method not found")
endif()
math(EXPR append_length "${append_end} - ${append_start}")
string(SUBSTRING "${player_taxi_source}" ${append_start} ${append_length} append_source)
string(REGEX MATCHALL "for [(]size_t" append_loops "${append_source}")
list(LENGTH append_loops append_loop_count)
if(NOT append_loop_count EQUAL 2)
    message(FATAL_ERROR "append serializer width guard: expected two loops, found ${append_loop_count}")
endif()

string(FIND "${player_taxi_source}" "std::ostringstream& operator<<" save_start)
if(save_start EQUAL -1)
    message(FATAL_ERROR "save serializer seam guard: operator not found")
endif()
string(SUBSTRING "${player_taxi_source}" ${save_start} -1 save_source)
require_once("${save_source}" "for [(]size_t" "save serializer width")
