file(READ "${SOURCE_ROOT}/src/game/Object/Pet.h" pet_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerPet.cpp" player_pet_source)
file(READ "${SOURCE_ROOT}/src/game/Object/PetMgr.cpp" pet_mgr_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "spell_count_width")
    string(REPLACE "out.WriteBits(uint32(snapshot.spells.size()), 22);"
        "out.WriteBits(uint32(snapshot.spells.size()), 21);"
        pet_header "${pet_header}")
elseif(MUTATION STREQUAL "cooldown_count_width")
    string(REPLACE "out.WriteBits(uint32(snapshot.cooldowns.size()), 20);"
        "out.WriteBits(uint32(snapshot.cooldowns.size()), 19);"
        pet_header "${pet_header}")
elseif(MUTATION STREQUAL "cooldown_field_order")
    string(REPLACE "out << cooldown.categoryCooldown;\n            out << cooldown.spellId;"
        "out << cooldown.spellId;\n            out << cooldown.categoryCooldown;"
        pet_header "${pet_header}")
elseif(MUTATION STREQUAL "player_sender")
    string(REPLACE "if (!MopPetPackets::BuildSpellSnapshot(data, snapshot))"
        "if (false)" player_pet_source "${player_pet_source}")
elseif(MUTATION STREQUAL "other_senders")
    string(REPLACE "MopPetPackets::BuildSpellSnapshot(data, snapshot);"
        "/* removed snapshot builder */" player_pet_source "${player_pet_source}")
elseif(MUTATION STREQUAL "clear_sender")
    string(REPLACE "MopPetPackets::BuildSpellSnapshot(data, snapshot);"
        "/* removed snapshot builder */" pet_mgr_source "${pet_mgr_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_PET_SPELLS, \"SMSG_PET_SPELLS\");"
        "/* removed SMSG_PET_SPELLS registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_PET_SPELLS:"
        "case REMOVED_SMSG_PET_SPELLS:" session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE "SMSG_PET_SPELLS                              = 0x095A,"
        "SMSG_PET_SPELLS                              = 0x095B,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_PET_SPELLS                                0x095A  ACTIVE"
        "SMSG_PET_SPELLS                                0x095A  DORMANT"
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

function(require_count source token expected context)
    set(remaining "${source}")
    set(actual 0)
    while(TRUE)
        string(FIND "${remaining}" "${token}" found)
        if(found EQUAL -1)
            break()
        endif()
        math(EXPR actual "${actual} + 1")
        math(EXPR next "${found} + 1")
        string(SUBSTRING "${remaining}" ${next} -1 remaining)
    endwhile()
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR "${context}: expected ${expected}, found ${actual}: ${token}")
    endif()
endfunction()

require_once("${pet_header}"
    "inline bool BuildSpellSnapshot(WorldPacket& out,"
    "shared pet-spell snapshot builder")
require_once("${pet_header}"
    "out.WriteBits(uint32(0), 21);                       // spell charges"
    "18414 empty spell-charge vector")
require_once("${pet_header}"
    "out.WriteBits(uint32(snapshot.spells.size()), 22);"
    "18414 spell count")
require_once("${pet_header}"
    "out.WriteBits(uint32(snapshot.cooldowns.size()), 20);"
    "18414 cooldown count")
require_once("${pet_header}"
    "out << cooldown.categoryCooldown;\n            out << cooldown.spellId;\n            out << cooldown.category;\n            out << cooldown.spellCooldown;"
    "18414 cooldown field order")

require_once("${player_pet_source}"
    "if (!MopPetPackets::BuildSpellSnapshot(data, snapshot))"
    "controlled-pet snapshot sender")
require_count("${player_pet_source}"
    "MopPetPackets::BuildSpellSnapshot(data, snapshot)" 3
    "pet, possess, and charm snapshot senders")
require_once("${pet_mgr_source}"
    "MopPetPackets::BuildSpellSnapshot(data, snapshot);"
    "empty pet-bar snapshot sender")

require_once("${opcode_registry}"
    "DefS(SMSG_PET_SPELLS, \"SMSG_PET_SPELLS\");"
    "pet-spells registration")
require_once("${session_source}" "case SMSG_PET_SPELLS:"
    "pet-spells converted-packet admission")
require_once("${opcode_header}"
    "SMSG_PET_SPELLS                              = 0x095A,"
    "direct 18414 opcode")
require_once("${opcode_reference}"
    "SMSG_PET_SPELLS                                0x095A  ACTIVE"
    "active direct-client reference")

string(FIND "${player_pet_source}" "WorldPacket data(SMSG_PET_SPELLS" legacy_player)
if(NOT legacy_player EQUAL -1)
    message(FATAL_ERROR "legacy Player pet-spell body remains")
endif()
string(FIND "${pet_mgr_source}" "WorldPacket data(SMSG_PET_SPELLS" legacy_clear)
if(NOT legacy_clear EQUAL -1)
    message(FATAL_ERROR "legacy eight-byte pet-bar clear remains")
endif()
