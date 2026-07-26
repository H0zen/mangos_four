file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "mask_order")
    string(REPLACE "out.WriteGuidMask<2, 7, 6, 0, 5, 1, 3, 4>(guid);"
        "out.WriteGuidMask<7, 2, 6, 0, 5, 1, 3, 4>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "byte_order")
    string(REPLACE "out.WriteGuidBytes<7, 4, 6, 1>(guid);"
        "out.WriteGuidBytes<4, 7, 6, 1>(guid);" unit_header "${unit_header}")
elseif(MUTATION STREQUAL "action_position")
    string(REPLACE "out.WriteGuidBytes<7, 4, 6, 1>(guid);\n        out << action;"
        "out << action;\n        out.WriteGuidBytes<7, 4, 6, 1>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "sender")
    string(REPLACE "MopCompactPackets::BuildPetActionSound(data, GetObjectGuid(), pettalk);"
        "/* removed pet-action sound builder */" unit_source "${unit_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_PET_ACTION_SOUND, \"SMSG_PET_ACTION_SOUND\");"
        "/* removed SMSG_PET_ACTION_SOUND registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_PET_ACTION_SOUND:"
        "case REMOVED_SMSG_PET_ACTION_SOUND:" session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE "SMSG_PET_ACTION_SOUND                        = 0x15E2,"
        "SMSG_PET_ACTION_SOUND                        = 0x15E3,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_PET_ACTION_SOUND                          0x15E2  ACTIVE"
        "SMSG_PET_ACTION_SOUND                          0x15E2  DORMANT"
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
    "inline void BuildPetActionSound(WorldPacket& out, ObjectGuid guid,"
    "shared pet-action sound builder")
require_once("${unit_header}"
    "out.WriteGuidMask<2, 7, 6, 0, 5, 1, 3, 4>(guid);"
    "18414 pet-action GUID mask")
require_once("${unit_header}"
    "out.WriteGuidBytes<7, 4, 6, 1>(guid);\n        out << action;\n        out.WriteGuidBytes<2, 3, 5, 0>(guid);"
    "18414 pet-action byte phases")
require_once("${unit_source}"
    "MopCompactPackets::BuildPetActionSound(data, GetObjectGuid(), pettalk);"
    "pet talk sender")
require_once("${opcode_registry}"
    "DefS(SMSG_PET_ACTION_SOUND, \"SMSG_PET_ACTION_SOUND\");"
    "pet-action sound registration")
require_once("${session_source}" "case SMSG_PET_ACTION_SOUND:"
    "pet-action sound converted-packet admission")
require_once("${opcode_header}"
    "SMSG_PET_ACTION_SOUND                        = 0x15E2,"
    "direct 18414 opcode")
require_once("${opcode_reference}"
    "SMSG_PET_ACTION_SOUND                          0x15E2  ACTIVE"
    "active direct-client reference")

string(FIND "${unit_source}" "WorldPacket data(SMSG_PET_ACTION_SOUND, 8 + 4);"
    legacy)
if(NOT legacy EQUAL -1)
    message(FATAL_ERROR "legacy raw-GUID pet-action sound body remains")
endif()
