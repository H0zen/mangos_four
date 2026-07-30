if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/ReputationMgr.cpp" reputation_source)
file(READ "${SOURCE_ROOT}/src/game/Object/CurrencyMgr.cpp" currency_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

string(CONCAT original_sources
    "${player_header}" "${reputation_source}" "${currency_source}"
    "${opcode_registry}" "${opcode_reference}" "${world_session}")

if(MUTATION STREQUAL "standing_order")
    string(REPLACE
        "out << standing.standing << standing.reputationIndex;"
        "out << standing.reputationIndex << standing.standing;"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "standing_sender")
    string(REPLACE
        "MopReputationPackets::BuildSetFactionStanding(data, anyRankIncreased, standings,"
        "/* removed 18414 standing builder route */"
        reputation_source "${reputation_source}")
elseif(MUTATION STREQUAL "visible_body")
    string(REPLACE
        "data << faction->ReputationListID;"
        "data << uint64(faction->ReputationListID);"
        reputation_source "${reputation_source}")
elseif(MUTATION STREQUAL "currency_order")
    string(REPLACE
        "packet << uint32(floor(cap / currency->GetPrecision()));\n    packet << uint32(currency->ID);"
        "packet << uint32(currency->ID);\n    packet << uint32(floor(cap / currency->GetPrecision()));"
        currency_source "${currency_source}")
elseif(MUTATION STREQUAL "standing_registration")
    string(REPLACE
        "DefS(SMSG_SET_FACTION_STANDING, \"SMSG_SET_FACTION_STANDING\");"
        "/* removed standing logging metadata */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "visible_registration")
    string(REPLACE
        "DefS(SMSG_SET_FACTION_VISIBLE, \"SMSG_SET_FACTION_VISIBLE\");"
        "/* removed visible logging metadata */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "currency_registration")
    string(REPLACE
        "DefS(SMSG_SET_CURRENCY_WEEK_LIMIT, \"SMSG_SET_CURRENCY_WEEK_LIMIT\");"
        "/* removed week-limit logging metadata */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "standing_admission")
    string(REPLACE "case SMSG_SET_FACTION_STANDING:"
        "/* removed standing admission */" world_session "${world_session}")
elseif(MUTATION STREQUAL "visible_admission")
    string(REPLACE "case SMSG_SET_FACTION_VISIBLE:"
        "/* removed visible admission */" world_session "${world_session}")
elseif(MUTATION STREQUAL "currency_admission")
    string(REPLACE "case SMSG_SET_CURRENCY_WEEK_LIMIT:"
        "/* removed week-limit admission */" world_session "${world_session}")
elseif(MUTATION STREQUAL "standing_reference")
    string(REPLACE
        "SMSG_SET_FACTION_STANDING                      0x10AA  ACTIVE"
        "SMSG_SET_FACTION_STANDING                      0x10AA  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "visible_reference")
    string(REPLACE
        "SMSG_SET_FACTION_VISIBLE                       0x1E8E  ACTIVE"
        "SMSG_SET_FACTION_VISIBLE                       0x1E8E  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "currency_reference")
    string(REPLACE
        "SMSG_SET_CURRENCY_WEEK_LIMIT                   0x0E2A  ACTIVE"
        "SMSG_SET_CURRENCY_WEEK_LIMIT                   0x0E2A  DORMANT"
        opcode_reference "${opcode_reference}")
endif()

if(MUTATION)
    string(CONCAT mutated_sources
        "${player_header}" "${reputation_source}" "${currency_source}"
        "${opcode_registry}" "${opcode_reference}" "${world_session}")
    if(mutated_sources STREQUAL original_sources)
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()

function(require_once text needle label)
    string(REGEX MATCHALL "${needle}" matches "${text}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${label}: expected exactly once, found ${count}")
    endif()
endfunction()

function(require_once_literal text needle label)
    set(remaining "${text}")
    set(count 0)
    while(TRUE)
        string(FIND "${remaining}" "${needle}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        string(LENGTH "${needle}" needle_length)
        math(EXPR next_position "${position} + ${needle_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${label}: expected exactly once, found ${count}")
    endif()
endfunction()

require_once("${player_header}"
    "out << standing\\.standing << standing\\.reputationIndex"
    "standing before reputation index")
require_once("${player_header}"
    "out << bonusA << bonusB"
    "two trailing reputation bonus floats")
require_once("${reputation_source}"
    "MopReputationPackets::BuildSetFactionStanding\\(data, anyRankIncreased, standings,"
    "standing builder route")
require_once("${reputation_source}"
    "WorldPacket data\\(SMSG_SET_FACTION_VISIBLE, 4\\)"
    "four-byte faction-visible packet")
require_once("${reputation_source}"
    "data << faction->ReputationListID"
    "faction-visible reputation index")
require_once_literal("${currency_source}"
    "packet << uint32(floor(cap / currency->GetPrecision()));\n    packet << uint32(currency->ID);"
    "week limit before currency ID")

set(character_pane_wave_one_opcodes
    SMSG_SET_FACTION_STANDING
    SMSG_SET_FACTION_VISIBLE
    SMSG_SET_CURRENCY_WEEK_LIMIT)
foreach(opcode IN LISTS character_pane_wave_one_opcodes)
    require_once("${world_session}" "case[ \t]+${opcode}:" "${opcode} admission")
    require_once("${opcode_registry}"
        "DefS\\(${opcode},[ \t]*\"${opcode}\"\\)"
        "${opcode} logging metadata")
    require_once("${opcode_reference}"
        "${opcode}[ \t]+0x[0-9A-F]+[ \t]+ACTIVE"
        "${opcode} active catalogue row")
endforeach()

if("${reputation_source}" MATCHES "WorldPacket[ \t]+data\\(SMSG_SET_FACTION_STANDING")
    message(FATAL_ERROR "legacy inline faction-standing sender remains")
endif()
