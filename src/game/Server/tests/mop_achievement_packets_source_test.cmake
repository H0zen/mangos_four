if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/AchievementMgr.h" achievement_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/AchievementMgr.cpp" achievement_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

string(CONCAT original_sources
    "${achievement_header}" "${achievement_source}" "${opcode_header}"
    "${opcode_registry}" "${world_session}" "${opcode_reference}")

if(MUTATION STREQUAL "earned_mask_order")
    string(REPLACE
        "out.WriteGuidMask<6, 2>(secondGuid);"
        "out.WriteGuidMask<2, 6>(secondGuid);"
        achievement_header "${achievement_header}")
elseif(MUTATION STREQUAL "earned_boolean")
    string(REPLACE
        "out.WriteBit(alreadyEarned);"
        "out.WriteBit(!alreadyEarned);"
        achievement_header "${achievement_header}")
elseif(MUTATION STREQUAL "earned_byte_order")
    string(REPLACE
        "out.WriteGuidBytes<5>(secondGuid);\n        out.WriteGuidBytes<3>(firstGuid);"
        "out.WriteGuidBytes<3>(firstGuid);\n        out.WriteGuidBytes<5>(secondGuid);"
        achievement_header "${achievement_header}")
elseif(MUTATION STREQUAL "earned_scalar_order")
    string(REPLACE
        "out << packedDate;"
        "out << achievementId;"
        achievement_header "${achievement_header}")
elseif(MUTATION STREQUAL "earned_producer_mapping")
    string(REPLACE
        "playerGuid, playerGuid, false, packedDate, uint32(achievement->ID),"
        "0, playerGuid, false, packedDate, uint32(achievement->ID),"
        achievement_source "${achievement_source}")
elseif(MUTATION STREQUAL "earned_sender_opcode")
    string(REPLACE
        "WorldPacket data(SMSG_ACHIEVEMENT_EARNED, 35);"
        "WorldPacket data(SMSG_ALL_ACHIEVEMENT_DATA, 35);"
        achievement_source "${achievement_source}")
elseif(MUTATION STREQUAL "earned_broadcast_scope")
    string(REPLACE
        "GetPlayer()->SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);"
        "GetPlayer()->GetSession()->SendPacket(&data);"
        achievement_source "${achievement_source}")
elseif(MUTATION STREQUAL "earned_legacy_body")
    string(REPLACE
        "uint64 const playerGuid = GetPlayer()->GetObjectGuid().GetRawValue();"
        "data << GetPlayer()->GetPackGUID();\n    uint64 const playerGuid = GetPlayer()->GetObjectGuid().GetRawValue();"
        achievement_source "${achievement_source}")
elseif(MUTATION STREQUAL "earned_opcode_value")
    string(REPLACE
        "SMSG_ACHIEVEMENT_EARNED                      = 0x080B"
        "SMSG_ACHIEVEMENT_EARNED                      = 0x080A"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "earned_registration")
    string(REPLACE
        "DefS(SMSG_ACHIEVEMENT_EARNED, \"SMSG_ACHIEVEMENT_EARNED\");"
        "/* removed achievement-earned logging metadata */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "earned_admission")
    string(REPLACE
        "case SMSG_ACHIEVEMENT_EARNED:"
        "/* removed achievement-earned admission */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "earned_reference")
    string(REGEX REPLACE
        "(SMSG_ACHIEVEMENT_EARNED[ \t]+0x080B[ \t]+)ACTIVE"
        "\\1DORMANT" opcode_reference "${opcode_reference}")
elseif(MUTATION MATCHES "^(criteria_update|criteria_deleted|achievement_deleted)_(registration|admission|reference)$")
    if(CMAKE_MATCH_1 STREQUAL "criteria_update")
        set(blocked_opcode SMSG_CRITERIA_UPDATE)
        set(blocked_value 0x0E9B)
    elseif(CMAKE_MATCH_1 STREQUAL "criteria_deleted")
        set(blocked_opcode SMSG_CRITERIA_DELETED)
        set(blocked_value 0x1C33)
    else()
        set(blocked_opcode SMSG_ACHIEVEMENT_DELETED)
        set(blocked_value 0x1A2F)
    endif()

    if(CMAKE_MATCH_2 STREQUAL "registration")
        string(APPEND opcode_registry
            "\nDefS(${blocked_opcode}, \"${blocked_opcode}\");")
    elseif(CMAKE_MATCH_2 STREQUAL "admission")
        string(APPEND world_session "\ncase ${blocked_opcode}:")
    else()
        string(REGEX REPLACE
            "(${blocked_opcode}[ \t]+${blocked_value}[ \t]+)DORMANT"
            "\\1ACTIVE" opcode_reference "${opcode_reference}")
    endif()
endif()

if(MUTATION)
    string(CONCAT mutated_sources
        "${achievement_header}" "${achievement_source}" "${opcode_header}"
        "${opcode_registry}" "${world_session}" "${opcode_reference}")
    if(mutated_sources STREQUAL original_sources)
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()

function(require_literal_once text needle label)
    string(FIND "${text}" "${needle}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "${label}: expected exactly once, found 0")
    endif()
    math(EXPR next "${first} + 1")
    string(SUBSTRING "${text}" ${next} -1 tail)
    string(FIND "${tail}" "${needle}" second)
    if(NOT second EQUAL -1)
        message(FATAL_ERROR "${label}: expected exactly once, found more than 1")
    endif()
endfunction()

function(require_literal_none text needle label)
    string(FIND "${text}" "${needle}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "${label}: expected absent")
    endif()
endfunction()

function(require_regex_once text needle label)
    string(REGEX MATCHALL "${needle}" matches "${text}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${label}: expected exactly once, found ${count}")
    endif()
endfunction()

function(require_regex_none text needle label)
    if("${text}" MATCHES "${needle}")
        message(FATAL_ERROR "${label}: expected absent")
    endif()
endfunction()

require_literal_once("${achievement_header}" "out.WriteBit(alreadyEarned);"
    "achievement-earned alreadyEarned bit")

set(mask_sequence "        out.WriteGuidMask<6, 2>(secondGuid);
        out.WriteGuidMask<4, 5, 0, 3>(firstGuid);
        out.WriteBit(alreadyEarned);
        out.WriteGuidMask<7>(secondGuid);
        out.WriteGuidMask<7, 1>(firstGuid);
        out.WriteGuidMask<3, 0, 4>(secondGuid);
        out.WriteGuidMask<6>(firstGuid);
        out.WriteGuidMask<1>(secondGuid);
        out.WriteGuidMask<2>(firstGuid);
        out.WriteGuidMask<5>(secondGuid);
        out.FlushBits();")
require_literal_once("${achievement_header}" "${mask_sequence}"
    "achievement-earned 17-bit mask order")
require_literal_once("${achievement_header}" "out << packedDate;"
    "achievement-earned scalar order")

set(byte_sequence "        out.WriteGuidBytes<5>(secondGuid);
        out.WriteGuidBytes<3>(firstGuid);
        out.WriteGuidBytes<6>(secondGuid);
        out.WriteGuidBytes<6>(firstGuid);
        out << packedDate;
        out.WriteGuidBytes<1>(secondGuid);
        out.WriteGuidBytes<2, 0, 7>(firstGuid);
        out.WriteGuidBytes<3>(secondGuid);
        out.WriteGuidBytes<4>(firstGuid);
        out.WriteGuidBytes<7>(secondGuid);
        out << achievementId;
        out.WriteGuidBytes<4>(secondGuid);
        out.WriteGuidBytes<1>(firstGuid);
        out.WriteGuidBytes<0>(secondGuid);
        out.WriteGuidBytes<5>(firstGuid);
        out << realm1 << realm2;
        out.WriteGuidBytes<2>(secondGuid);")
require_literal_once("${achievement_header}" "${byte_sequence}"
    "achievement-earned GUID byte interleave")

string(FIND "${achievement_source}"
    "void AchievementMgr::SendAchievementEarned" earned_start)
string(FIND "${achievement_source}"
    "void AchievementMgr::SendCriteriaUpdate" earned_end)
if(earned_start EQUAL -1 OR earned_end EQUAL -1 OR earned_end LESS earned_start)
    message(FATAL_ERROR "achievement-earned producer function bounds are missing")
endif()
math(EXPR earned_length "${earned_end} - ${earned_start}")
string(SUBSTRING "${achievement_source}" ${earned_start} ${earned_length} earned_source)

set(producer_mapping "    MopAchievementPackets::BuildAchievementEarned(data,
        playerGuid, playerGuid, false, packedDate, uint32(achievement->ID),
        realmID, realmID);")
require_literal_once("${earned_source}" "${producer_mapping}"
    "achievement-earned producer mapping")
require_literal_once("${earned_source}"
    "WorldPacket data(SMSG_ACHIEVEMENT_EARNED, 35);"
    "achievement-earned producer opcode")
require_literal_once("${earned_source}"
    "GetPlayer()->SendMessageToSetInRange(&data, sWorld.getConfig(CONFIG_FLOAT_LISTEN_RANGE_SAY), true);"
    "achievement-earned nearby broadcast")
require_literal_none("${earned_source}" "GetPlayer()->GetPackGUID()"
    "legacy achievement-earned inline body")

require_regex_once("${opcode_header}"
    "SMSG_ACHIEVEMENT_EARNED[ \t]*=[ \t]*0x080B"
    "achievement-earned direct 18414 opcode")
require_literal_once("${opcode_registry}"
    "DefS(SMSG_ACHIEVEMENT_EARNED, \"SMSG_ACHIEVEMENT_EARNED\");"
    "achievement-earned logging metadata")
require_literal_once("${world_session}" "case SMSG_ACHIEVEMENT_EARNED:"
    "achievement-earned admission")
require_regex_once("${opcode_reference}"
    "SMSG_ACHIEVEMENT_EARNED[ \t]+0x080B[ \t]+ACTIVE"
    "achievement-earned active catalogue row")

set(blocked_opcodes
    SMSG_CRITERIA_UPDATE
    SMSG_CRITERIA_DELETED
    SMSG_ACHIEVEMENT_DELETED)
foreach(blocked_opcode IN LISTS blocked_opcodes)
    if(blocked_opcode STREQUAL "SMSG_CRITERIA_UPDATE")
        set(blocked_value 0x0E9B)
    elseif(blocked_opcode STREQUAL "SMSG_CRITERIA_DELETED")
        set(blocked_value 0x1C33)
    else()
        set(blocked_value 0x1A2F)
    endif()
    require_regex_none("${opcode_registry}"
        "DefS\\(${blocked_opcode},[ \t]*\"${blocked_opcode}\"\\)"
        "${blocked_opcode} remains unregistered")
    require_regex_none("${world_session}" "case[ \t]+${blocked_opcode}:"
        "${blocked_opcode} remains unadmitted")
    require_regex_once("${opcode_reference}"
        "${blocked_opcode}[ \t]+${blocked_value}[ \t]+DORMANT"
        "${blocked_opcode} dormant catalogue row")
    require_regex_none("${opcode_reference}"
        "${blocked_opcode}[ \t]+${blocked_value}[ \t]+ACTIVE"
        "${blocked_opcode} remains dormant")
endforeach()
