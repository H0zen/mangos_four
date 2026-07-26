file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit_source)
file(READ "${SOURCE_ROOT}/src/game/Object/Creature.cpp" creature_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "mask_order")
    string(REPLACE "out.WriteGuidMask<5, 7, 0, 4, 6, 2, 3, 1>(guid);"
        "out.WriteGuidMask<7, 5, 0, 4, 6, 2, 3, 1>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "byte_order")
    string(REPLACE "out.WriteGuidBytes<4, 6, 5>(guid);"
        "out.WriteGuidBytes<6, 4, 5>(guid);" unit_header "${unit_header}")
elseif(MUTATION STREQUAL "reaction_position")
    string(REPLACE "out.WriteGuidBytes<4, 6, 5>(guid);\n        out << reaction;"
        "out << reaction;\n        out.WriteGuidBytes<4, 6, 5>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "creature_sender")
    string(REPLACE "MopCompactPackets::BuildAIReaction(data, GetObjectGuid(), reactionType);"
        "/* removed creature AI reaction builder */" creature_source "${creature_source}")
elseif(MUTATION STREQUAL "pet_sender")
    string(REPLACE "MopCompactPackets::BuildAIReaction(data, GetObjectGuid(), AI_REACTION_HOSTILE);"
        "/* removed pet AI reaction builder */" unit_source "${unit_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_AI_REACTION, \"SMSG_AI_REACTION\");"
        "/* removed SMSG_AI_REACTION registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_AI_REACTION:"
        "case REMOVED_SMSG_AI_REACTION:" session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE "SMSG_AI_REACTION                             = 0x06AF,"
        "SMSG_AI_REACTION                             = 0x06AE,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_AI_REACTION                               0x06AF  ACTIVE"
        "SMSG_AI_REACTION                               0x06AF  DORMANT"
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

require_once("${unit_header}"
    "inline void BuildAIReaction(WorldPacket& out, ObjectGuid guid, uint32 reaction)"
    "shared AI-reaction builder")
require_once("${unit_header}"
    "out.WriteGuidMask<5, 7, 0, 4, 6, 2, 3, 1>(guid);"
    "18414 AI-reaction GUID mask")
require_once("${unit_header}"
    "out.WriteGuidBytes<4, 6, 5>(guid);\n        out << reaction;\n        out.WriteGuidBytes<7, 1, 2, 0, 3>(guid);"
    "18414 AI-reaction byte phase")
require_once("${creature_source}"
    "MopCompactPackets::BuildAIReaction(data, GetObjectGuid(), reactionType);"
    "creature AI-reaction sender")
require_once("${unit_source}"
    "MopCompactPackets::BuildAIReaction(data, GetObjectGuid(), AI_REACTION_HOSTILE);"
    "pet AI-reaction sender")
require_once("${opcode_registry}"
    "DefS(SMSG_AI_REACTION, \"SMSG_AI_REACTION\");"
    "AI-reaction registration")
require_once("${session_source}" "case SMSG_AI_REACTION:"
    "AI-reaction converted-packet admission")
require_once("${opcode_header}"
    "SMSG_AI_REACTION                             = 0x06AF,"
    "direct 18414 opcode")
require_once("${opcode_reference}"
    "SMSG_AI_REACTION                               0x06AF  ACTIVE"
    "active direct-client reference")

foreach(legacy IN ITEMS
        "WorldPacket data(SMSG_AI_REACTION, 12);"
        "WorldPacket data(SMSG_AI_REACTION, 8 + 4);")
    string(FIND "${creature_source}${unit_source}" "${legacy}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "legacy AI-reaction body remains: ${legacy}")
    endif()
endforeach()
