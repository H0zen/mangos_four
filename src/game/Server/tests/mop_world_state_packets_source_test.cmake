file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_source)
file(READ "${SOURCE_ROOT}/src/game/BattleGround/BattleGroundMgr.cpp" battleground_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "bit_order")
    string(REPLACE "out.WriteBit(hidden);\n        out.FlushBits();"
        "out.FlushBits();\n        out.WriteBit(hidden);" player_header "${player_header}")
elseif(MUTATION STREQUAL "field_order")
    string(REPLACE "out << value << field;" "out << field << value;"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "player_sender")
    string(REPLACE "MopWorldEntryPackets::BuildUpdateWorldState(data, Field, Value);"
        "/* removed player world-state builder */" player_source "${player_source}")
elseif(MUTATION STREQUAL "battleground_sender")
    string(REPLACE "MopWorldEntryPackets::BuildUpdateWorldState(*data, field, value);"
        "/* removed battleground world-state builder */" battleground_source "${battleground_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_UPDATE_WORLD_STATE, \"SMSG_UPDATE_WORLD_STATE\");"
        "/* removed SMSG_UPDATE_WORLD_STATE registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_UPDATE_WORLD_STATE:"
        "case REMOVED_SMSG_UPDATE_WORLD_STATE:" session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE "SMSG_UPDATE_WORLD_STATE                      = 0x121B,"
        "SMSG_UPDATE_WORLD_STATE                      = 0x121A,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_UPDATE_WORLD_STATE                        0x121B  ACTIVE"
        "SMSG_UPDATE_WORLD_STATE                        0x121B  DORMANT"
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

require_once("${player_header}" "inline void BuildUpdateWorldState(WorldPacket& out, uint32 field,"
    "shared incremental world-state builder")
require_once("${player_header}" "out.WriteBit(hidden);\n        out.FlushBits();"
    "18414 leading hidden bit")
require_once("${player_header}" "out << value << field;"
    "18414 value-before-field byte phase")
require_once("${player_source}"
    "MopWorldEntryPackets::BuildUpdateWorldState(data, Field, Value);"
    "player world-state sender")
require_once("${battleground_source}"
    "MopWorldEntryPackets::BuildUpdateWorldState(*data, field, value);"
    "battleground world-state sender")
require_once("${opcode_registry}"
    "DefS(SMSG_UPDATE_WORLD_STATE, \"SMSG_UPDATE_WORLD_STATE\");"
    "world-state registration")
require_once("${session_source}" "case SMSG_UPDATE_WORLD_STATE:"
    "world-state converted-packet admission")
require_once("${opcode_header}"
    "SMSG_UPDATE_WORLD_STATE                      = 0x121B,"
    "direct 18414 opcode")
require_once("${opcode_reference}"
    "SMSG_UPDATE_WORLD_STATE                        0x121B  ACTIVE"
    "active direct-client reference")

foreach(legacy IN ITEMS
        "data << Field;"
        "data << Value;"
        "*data << uint32(field);"
        "*data << uint32(value);")
    string(FIND "${player_source}${battleground_source}" "${legacy}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "legacy incremental world-state body remains: ${legacy}")
    endif()
endforeach()
