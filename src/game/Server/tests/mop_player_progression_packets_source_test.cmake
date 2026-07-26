if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "wire_order")
        string(REPLACE
            "out << info.talentDelta << info.healthDelta;"
            "out << info.healthDelta << info.talentDelta;"
            player_header "${player_header}")
    elseif(MUTATION STREQUAL "sender")
        string(REPLACE
            "MopProgressionPackets::BuildLevelUpInfo(data, packetInfo);"
            "/* removed level-up sender */"
            player_source "${player_source}")
    elseif(MUTATION STREQUAL "registration")
        string(REPLACE
            "DefS(SMSG_LEVELUP_INFO, \"SMSG_LEVELUP_INFO\");"
            "/* removed level-up registration */"
            opcode_registry "${opcode_registry}")
    elseif(MUTATION STREQUAL "admission")
        string(REPLACE
            "case SMSG_LEVELUP_INFO:"
            "/* removed level-up admission */"
            world_session "${world_session}")
    elseif(MUTATION STREQUAL "reference")
        string(REPLACE
            "SMSG_LEVELUP_INFO                              0x1961  ACTIVE"
            "SMSG_LEVELUP_INFO                              0x1961  DORMANT"
            opcode_reference "${opcode_reference}")
    else()
        message(FATAL_ERROR "unknown MUTATION=${MUTATION}")
    endif()
endif()

function(require_once text needle label)
    string(REGEX MATCHALL "${needle}" matches "${text}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${label}: expected exactly once, found ${count}")
    endif()
endfunction()

require_once("${player_header}"
    "out << info\\.talentDelta << info\\.healthDelta"
    "18414 level-up leading field order")
require_once("${player_header}"
    "for \\(uint32 delta : info\\.statDeltas\\)"
    "five level-up stat deltas")
require_once("${player_header}"
    "out << info\\.level"
    "18414 level-up level slot")
require_once("${player_header}"
    "for \\(uint32 delta : info\\.powerDeltas\\)"
    "five level-up power deltas")
require_once("${player_source}"
    "MopProgressionPackets::BuildLevelUpInfo\\(data, packetInfo\\)"
    "level-up sender")
require_once("${opcode_registry}"
    "DefS\\(SMSG_LEVELUP_INFO,[ \\t]*\"SMSG_LEVELUP_INFO\"\\)"
    "level-up registration")
require_once("${world_session}"
    "case[ \t]+SMSG_LEVELUP_INFO:"
    "level-up admission")
require_once("${opcode_reference}"
    "SMSG_LEVELUP_INFO[ \t]+0x1961[ \t]+ACTIVE"
    "active direct-client level-up reference")

if("${player_source}" MATCHES "WorldPacket[ \\t]+[A-Za-z0-9_]+\\(SMSG_LEVELUP_INFO")
    message(FATAL_ERROR "legacy inline level-up sender remains")
endif()
