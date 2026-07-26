file(READ "${SOURCE_ROOT}/src/game/Object/PlayerQuest.cpp" quest_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "invalid_body")
    string(REPLACE "data.WriteBit(true);                 // no custom failure text"
        "data.WriteBit(false);                // wrong nullable-string marker"
        quest_source "${quest_source}")
elseif(MUTATION STREQUAL "failed_order")
    string(REPLACE "data << uint32(quest_id);\n        data << uint32(reason);"
        "data << uint32(reason);\n        data << uint32(quest_id);"
        quest_source "${quest_source}")
elseif(MUTATION STREQUAL "log_full_body")
    string(REPLACE "WorldPacket data(SMSG_QUESTLOG_FULL, 0);"
        "WorldPacket data(SMSG_QUESTLOG_FULL, 1);"
        quest_source "${quest_source}")
elseif(MUTATION STREQUAL "timer_body")
    string(REPLACE "WorldPacket data(SMSG_QUESTUPDATE_FAILEDTIMER, 4);"
        "WorldPacket data(SMSG_QUESTUPDATE_FAILEDTIMER, 8);"
        quest_source "${quest_source}")
elseif(MUTATION MATCHES "^registration_(.+)$")
    set(name "${CMAKE_MATCH_1}")
    string(REPLACE "DefS(${name}, \"${name}\");"
        "/* removed ${name} registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION MATCHES "^allowlist_(.+)$")
    set(name "${CMAKE_MATCH_1}")
    string(REPLACE "case ${name}:" "case REMOVED_${name}:"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "opcodes")
    string(REPLACE "SMSG_QUESTGIVER_QUEST_INVALID                = 0x027D,"
        "SMSG_QUESTGIVER_QUEST_INVALID                = 0x027C,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_QUESTLOG_FULL                             0x07FD  ACTIVE   [high-conf]"
        "SMSG_QUESTLOG_FULL                             0x07FD  DORMANT  [low-conf]"
        opcode_reference "${opcode_reference}")
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

require_once("${quest_source}" "WorldPacket data(SMSG_QUESTLOG_FULL, 0);"
    "empty quest-log-full sender")
require_once("${quest_source}" "WorldPacket data(SMSG_QUESTGIVER_QUEST_FAILED, 4 + 4);"
    "quest-failed sender")
require_once("${quest_source}" "data << uint32(quest_id);\n        data << uint32(reason);"
    "quest-failed 18414 field order")
require_once("${quest_source}" "WorldPacket data(SMSG_QUESTUPDATE_FAILEDTIMER, 4);"
    "failed-timer sender")
require_once("${quest_source}" "WorldPacket data(SMSG_QUESTGIVER_QUEST_INVALID, 1 + 4);"
    "quest-invalid sender")
require_once("${quest_source}" "data.WriteBit(true);                 // no custom failure text"
    "quest-invalid nullable-string marker")

foreach(name IN ITEMS SMSG_QUESTGIVER_QUEST_INVALID SMSG_QUESTGIVER_QUEST_FAILED
        SMSG_QUESTLOG_FULL SMSG_QUESTUPDATE_FAILEDTIMER)
    require_once("${opcode_registry}" "DefS(${name}, \"${name}\");"
        "${name} registration")
    require_once("${session_source}" "case ${name}:"
        "${name} converted-packet admission")
endforeach()

foreach(row IN ITEMS
        "SMSG_QUESTGIVER_QUEST_INVALID                = 0x027D,"
        "SMSG_QUESTGIVER_QUEST_FAILED                 = 0x12DE,"
        "SMSG_QUESTLOG_FULL                           = 0x07FD,"
        "SMSG_QUESTUPDATE_FAILEDTIMER                 = 0x06FF,")
    require_once("${opcode_header}" "${row}" "direct 18414 opcode")
endforeach()

foreach(row IN ITEMS
        "SMSG_QUESTGIVER_QUEST_INVALID                  0x027D  ACTIVE   [high-conf]"
        "SMSG_QUESTGIVER_QUEST_FAILED                   0x12DE  ACTIVE   [high-conf]"
        "SMSG_QUESTLOG_FULL                             0x07FD  ACTIVE   [high-conf]"
        "SMSG_QUESTUPDATE_FAILEDTIMER                   0x06FF  ACTIVE   [high-conf]")
    require_once("${opcode_reference}" "${row}" "active direct-client reference")
endforeach()
