file(READ "${SOURCE_ROOT}/src/game/Object/GameObjectUse.cpp" use_source)
file(READ "${SOURCE_ROOT}/src/game/Object/GameObjectUpdate.cpp" update_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "escaped_sender")
    string(REPLACE
        "WorldPacket data(SMSG_FISH_ESCAPED, 0);"
        "WorldPacket data(SMSG_FISH_ESCAPED, 1);"
        use_source "${use_source}")
elseif(MUTATION STREQUAL "not_hooked_senders")
    string(REPLACE
        "WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);"
        "WorldPacket data(SMSG_FISH_NOT_HOOKED, 1);"
        use_source "${use_source}")
    string(REPLACE
        "WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);"
        "WorldPacket data(SMSG_FISH_NOT_HOOKED, 1);"
        update_source "${update_source}")
elseif(MUTATION STREQUAL "escaped_registration")
    string(REPLACE
        "DefS(SMSG_FISH_ESCAPED, \"SMSG_FISH_ESCAPED\");"
        "/* removed fish-escaped registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "not_hooked_registration")
    string(REPLACE
        "DefS(SMSG_FISH_NOT_HOOKED, \"SMSG_FISH_NOT_HOOKED\");"
        "/* removed fish-not-hooked registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "escaped_allowlist")
    string(REPLACE
        "case SMSG_FISH_ESCAPED:"
        "case REMOVED_FISH_ESCAPED:"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "not_hooked_allowlist")
    string(REPLACE
        "case SMSG_FISH_NOT_HOOKED:"
        "case REMOVED_FISH_NOT_HOOKED:"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "escaped_opcode")
    string(REPLACE
        "SMSG_FISH_ESCAPED                            = 0x0227,"
        "SMSG_FISH_ESCAPED                            = 0x2205,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "not_hooked_opcode")
    string(REPLACE
        "SMSG_FISH_NOT_HOOKED                         = 0x10BE,"
        "SMSG_FISH_NOT_HOOKED                         = 0x0A17,"
        opcode_header "${opcode_header}")
endif()

function(require_count source token expected context)
    set(remaining "${source}")
    set(count 0)
    while(TRUE)
        string(FIND "${remaining}" "${token}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        string(LENGTH "${token}" token_length)
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL expected)
        message(FATAL_ERROR "${context}: expected ${expected} active occurrence(s), found ${count}")
    endif()
endfunction()

require_count("${use_source}"
    "WorldPacket data(SMSG_FISH_ESCAPED, 0);" 1
    "empty fish-escaped sender")
require_count("${use_source}"
    "WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);" 1
    "used-too-early fish-not-hooked sender")
require_count("${update_source}"
    "WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);" 1
    "expired-bobber fish-not-hooked sender")
require_count("${opcode_registry}"
    "DefS(SMSG_FISH_ESCAPED, \"SMSG_FISH_ESCAPED\");" 1
    "fish-escaped registration")
require_count("${opcode_registry}"
    "DefS(SMSG_FISH_NOT_HOOKED, \"SMSG_FISH_NOT_HOOKED\");" 1
    "fish-not-hooked registration")
require_count("${session_source}"
    "case SMSG_FISH_ESCAPED:" 1
    "fish-escaped converted-packet admission")
require_count("${session_source}"
    "case SMSG_FISH_NOT_HOOKED:" 1
    "fish-not-hooked converted-packet admission")
require_count("${opcode_header}"
    "SMSG_FISH_ESCAPED                            = 0x0227," 1
    "fish-escaped opcode")
require_count("${opcode_header}"
    "SMSG_FISH_NOT_HOOKED                         = 0x10BE," 1
    "fish-not-hooked opcode")
require_count("${opcode_reference}"
    "SMSG_FISH_ESCAPED                              0x0227  ACTIVE" 1
    "fish-escaped reference status")
require_count("${opcode_reference}"
    "SMSG_FISH_NOT_HOOKED                           0x10BE  ACTIVE" 1
    "fish-not-hooked reference status")
