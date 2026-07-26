file(READ "${SOURCE_ROOT}/src/game/Object/Pet.h" pet_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Pet.cpp" pet_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "mask_order")
    string(REPLACE "out.WriteGuidMask<5, 0, 6, 3, 7, 2, 4, 1>(guid);"
        "out.WriteGuidMask<0, 5, 6, 3, 7, 2, 4, 1>(guid);"
        pet_header "${pet_header}")
elseif(MUTATION STREQUAL "byte_order")
    string(REPLACE "out.WriteGuidBytes<2, 5, 4, 0, 1, 7, 3, 6>(guid);"
        "out.WriteGuidBytes<5, 2, 4, 0, 1, 7, 3, 6>(guid);"
        pet_header "${pet_header}")
elseif(MUTATION STREQUAL "mode_position")
    string(REPLACE "out << modeFlags;\n        out.WriteGuidBytes<2, 5, 4, 0, 1, 7, 3, 6>(guid);"
        "out.WriteGuidBytes<2, 5, 4, 0, 1, 7, 3, 6>(guid);\n        out << modeFlags;"
        pet_header "${pet_header}")
elseif(MUTATION STREQUAL "set_sender")
    string(REPLACE "MopPetPackets::BuildMode(data, GetObjectGuid(), uint32(mode));"
        "/* removed SetModeFlags packet builder */" pet_source "${pet_source}")
elseif(MUTATION STREQUAL "apply_sender")
    string(REPLACE "MopPetPackets::BuildMode(data, GetObjectGuid(), uint32(m_petModeFlags));"
        "/* removed ApplyModeFlags packet builder */" pet_source "${pet_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_PET_MODE, \"SMSG_PET_MODE\");"
        "/* removed SMSG_PET_MODE registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_PET_MODE:"
        "case REMOVED_SMSG_PET_MODE:" session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE "SMSG_PET_MODE                                = 0x163F,"
        "SMSG_PET_MODE                                = 0x1640,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_PET_MODE                                  0x163F  ACTIVE"
        "SMSG_PET_MODE                                  0x163F  DORMANT"
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

require_once("${pet_header}"
    "inline void BuildMode(WorldPacket& out, ObjectGuid guid, uint32 modeFlags)"
    "shared pet-mode builder")
require_once("${pet_header}"
    "out.WriteGuidMask<5, 0, 6, 3, 7, 2, 4, 1>(guid);"
    "18414 pet-mode GUID mask")
require_once("${pet_header}"
    "out << modeFlags;\n        out.WriteGuidBytes<2, 5, 4, 0, 1, 7, 3, 6>(guid);"
    "18414 pet-mode value and GUID byte phases")

require_once("${pet_source}"
    "MopPetPackets::BuildMode(data, GetObjectGuid(), uint32(mode));"
    "SetModeFlags pet-mode sender")
require_once("${pet_source}"
    "MopPetPackets::BuildMode(data, GetObjectGuid(), uint32(m_petModeFlags));"
    "ApplyModeFlags pet-mode sender")

require_once("${opcode_registry}"
    "DefS(SMSG_PET_MODE, \"SMSG_PET_MODE\");"
    "pet-mode registration")
require_once("${session_source}" "case SMSG_PET_MODE:"
    "pet-mode converted-packet admission")
require_once("${opcode_header}"
    "SMSG_PET_MODE                                = 0x163F,"
    "direct 18414 opcode")
require_once("${opcode_reference}"
    "SMSG_PET_MODE                                  0x163F  ACTIVE"
    "active direct-client reference")

string(FIND "${pet_source}" "WorldPacket data(SMSG_PET_MODE, 12);" legacy)
if(NOT legacy EQUAL -1)
    message(FATAL_ERROR "legacy raw-GUID pet-mode body remains")
endif()
