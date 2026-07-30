if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/ReputationMgr.cpp" reputation_source)
file(READ "${SOURCE_ROOT}/src/game/Object/CurrencyMgr.cpp" currency_source)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_source)
file(READ "${SOURCE_ROOT}/src/game/Object/HonorMgr.cpp" honor_source)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerMirror.cpp" player_mirror_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

string(CONCAT original_sources
    "${player_header}" "${reputation_source}" "${currency_source}"
    "${player_source}" "${honor_source}" "${player_mirror_source}"
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
elseif(MUTATION STREQUAL "title_sender")
    string(REPLACE
        "MopCharacterPanePackets::BuildTitleUpdate(data, uint32(title->Mask_ID), lost);"
        "/* removed 18414 title builder route */"
        player_source "${player_source}")
elseif(MUTATION STREQUAL "pvp_mask")
    string(REPLACE
        "out.WriteGuidMask<4, 2, 5, 3, 0, 6, 1, 7>(victimGuid);"
        "out.WriteGuidMask<2, 4, 5, 3, 0, 6, 1, 7>(victimGuid);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "pvp_bytes")
    string(REPLACE
        "out.WriteGuidBytes<6, 7, 5, 0, 1, 3, 4, 2>(victimGuid);"
        "out.WriteGuidBytes<7, 6, 5, 0, 1, 3, 4, 2>(victimGuid);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "pvp_sender")
    string(REPLACE
        "MopCharacterPanePackets::BuildPvpCredit(data, uint32(victim_rank),"
        "/* removed 18414 PvP-credit builder route */"
        honor_source "${honor_source}")
elseif(MUTATION STREQUAL "inebriation_mask")
    string(REPLACE
        "out.WriteGuidMask<0, 4, 2, 6, 5, 1, 3, 7>(playerGuid);"
        "out.WriteGuidMask<4, 0, 2, 6, 5, 1, 3, 7>(playerGuid);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "inebriation_byte3")
    string(REPLACE
        "out.WriteGuidBytes<3>(playerGuid);"
        "/* removed interleaved GUID byte 3 */"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "inebriation_bytes")
    string(REPLACE
        "out.WriteGuidBytes<4, 6, 7, 0, 2, 5, 1>(playerGuid);"
        "out.WriteGuidBytes<6, 4, 7, 0, 2, 5, 1>(playerGuid);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "inebriation_sender")
    string(REPLACE
        "MopCharacterPanePackets::BuildCrossedInebriationThreshold(data,"
        "/* removed 18414 inebriation builder route */"
        player_mirror_source "${player_mirror_source}")
elseif(MUTATION MATCHES "^(title_earned|title_lost|pvp|inebriation)_(registration|admission|reference)$")
    if(CMAKE_MATCH_1 STREQUAL "title_earned")
        set(opcode SMSG_TITLE_EARNED)
    elseif(CMAKE_MATCH_1 STREQUAL "title_lost")
        set(opcode SMSG_TITLE_LOST)
    elseif(CMAKE_MATCH_1 STREQUAL "pvp")
        set(opcode SMSG_PVP_CREDIT)
    else()
        set(opcode SMSG_CROSSED_INEBRIATION_THRESHOLD)
    endif()

    if(CMAKE_MATCH_2 STREQUAL "registration")
        string(REPLACE "DefS(${opcode}, \"${opcode}\");"
            "/* removed ${opcode} logging metadata */"
            opcode_registry "${opcode_registry}")
    elseif(CMAKE_MATCH_2 STREQUAL "admission")
        string(REPLACE "case ${opcode}:" "/* removed ${opcode} admission */"
            world_session "${world_session}")
    else()
        string(REGEX REPLACE
            "(${opcode}[ \t]+0x[0-9A-F]+[ \t]+)ACTIVE"
            "\\1DORMANT" opcode_reference "${opcode_reference}")
    endif()
endif()

if(MUTATION)
    string(CONCAT mutated_sources
        "${player_header}" "${reputation_source}" "${currency_source}"
        "${player_source}" "${honor_source}" "${player_mirror_source}"
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

require_once("${player_source}"
    "MopCharacterPanePackets::BuildTitleUpdate\\(data, uint32\\(title->Mask_ID\\), lost\\)"
    "title update builder route")
require_once("${honor_source}"
    "MopCharacterPanePackets::BuildPvpCredit\\(data, uint32\\(victim_rank\\),"
    "PvP-credit builder route")
require_once("${player_header}"
    "out\\.WriteGuidMask<4, 2, 5, 3, 0, 6, 1, 7>\\(victimGuid\\)"
    "PvP-credit GUID mask order")
require_once("${player_header}"
    "out\\.WriteGuidBytes<6, 7, 5, 0, 1, 3, 4, 2>\\(victimGuid\\)"
    "PvP-credit GUID byte order")
require_once("${player_mirror_source}"
    "MopCharacterPanePackets::BuildCrossedInebriationThreshold\\(data,"
    "inebriation builder route")
require_once("${player_header}"
    "out\\.WriteGuidMask<0, 4, 2, 6, 5, 1, 3, 7>\\(playerGuid\\)"
    "inebriation GUID mask order")
require_once("${player_header}"
    "out\\.WriteGuidBytes<3>\\(playerGuid\\)"
    "interleaved inebriation GUID byte 3")
require_once("${player_header}"
    "out << itemId << drunkState"
    "inebriation item/state order")
require_once("${player_header}"
    "out\\.WriteGuidBytes<4, 6, 7, 0, 2, 5, 1>\\(playerGuid\\)"
    "inebriation remaining GUID byte order")

set(character_pane_wave_two_opcodes
    SMSG_TITLE_EARNED
    SMSG_TITLE_LOST
    SMSG_PVP_CREDIT
    SMSG_CROSSED_INEBRIATION_THRESHOLD)
foreach(opcode IN LISTS character_pane_wave_two_opcodes)
    require_once("${world_session}" "case[ \t]+${opcode}:" "${opcode} admission")
    require_once("${opcode_registry}"
        "DefS\\(${opcode},[ \t]*\"${opcode}\"\\)"
        "${opcode} logging metadata")
    require_once("${opcode_reference}"
        "${opcode}[ \t]+0x[0-9A-F]+[ \t]+ACTIVE"
        "${opcode} active catalogue row")
endforeach()

if("${player_source}" MATCHES "WorldPacket[ \t]+data\\(SMSG_TITLE_EARNED")
    message(FATAL_ERROR "legacy inline title sender remains")
endif()
if("${honor_source}" MATCHES "WorldPacket[ \t]+data\\(SMSG_PVP_CREDIT")
    message(FATAL_ERROR "legacy inline PvP-credit sender remains")
endif()
if("${player_mirror_source}" MATCHES "WorldPacket[ \t]+data\\(SMSG_CROSSED_INEBRIATION_THRESHOLD")
    message(FATAL_ERROR "legacy inline inebriation sender remains")
endif()
