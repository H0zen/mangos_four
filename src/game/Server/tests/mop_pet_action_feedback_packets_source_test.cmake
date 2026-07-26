file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "presence_bit")
    string(REPLACE "out.WriteBit(spellId == 0);"
        "out.WriteBit(spellId != 0);" unit_header "${unit_header}")
elseif(MUTATION STREQUAL "field_order")
    string(REPLACE "out << feedback;\n        if (spellId != 0)\n            out << spellId;"
        "if (spellId != 0)\n            out << spellId;\n        out << feedback;"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "optional_field")
    string(REPLACE "if (spellId != 0)\n            out << spellId;"
        "/* removed optional spell context */" unit_header "${unit_header}")
elseif(MUTATION STREQUAL "sender")
    string(REPLACE "MopCompactPackets::BuildPetActionFeedback(data, msg, spellId);"
        "/* removed pet-action feedback builder */" unit_source "${unit_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_PET_ACTION_FEEDBACK, \"SMSG_PET_ACTION_FEEDBACK\");"
        "/* removed SMSG_PET_ACTION_FEEDBACK registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_PET_ACTION_FEEDBACK:"
        "case REMOVED_SMSG_PET_ACTION_FEEDBACK:" session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE "SMSG_PET_ACTION_FEEDBACK                     = 0x080E,"
        "SMSG_PET_ACTION_FEEDBACK                     = 0x080F,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_PET_ACTION_FEEDBACK                       0x080E  ACTIVE"
        "SMSG_PET_ACTION_FEEDBACK                       0x080E  DORMANT"
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
    "inline void BuildPetActionFeedback(WorldPacket& out, uint8 feedback,"
    "shared pet-action feedback builder")
require_once("${unit_header}"
    "out.WriteBit(spellId == 0);\n        out.FlushBits();\n        out << feedback;\n        if (spellId != 0)\n            out << spellId;"
    "18414 inverse-presence and field order")
require_once("${unit_header}"
    "void SendPetActionFeedback(uint8 msg, uint32 spellId = 0);"
    "optional spell-context sender contract")
require_once("${unit_source}"
    "MopCompactPackets::BuildPetActionFeedback(data, msg, spellId);"
    "pet-action feedback sender")
require_once("${opcode_registry}"
    "DefS(SMSG_PET_ACTION_FEEDBACK, \"SMSG_PET_ACTION_FEEDBACK\");"
    "pet-action feedback registration")
require_once("${session_source}" "case SMSG_PET_ACTION_FEEDBACK:"
    "pet-action feedback converted-packet admission")
require_once("${opcode_header}"
    "SMSG_PET_ACTION_FEEDBACK                     = 0x080E,"
    "direct 18414 opcode")
require_once("${opcode_reference}"
    "SMSG_PET_ACTION_FEEDBACK                       0x080E  ACTIVE"
    "active direct-client reference")

string(FIND "${unit_source}" "WorldPacket data(SMSG_PET_ACTION_FEEDBACK, 1);"
    legacy)
if(NOT legacy EQUAL -1)
    message(FATAL_ERROR "legacy one-byte pet-action feedback body remains")
endif()
