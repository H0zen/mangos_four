file(READ "${SOURCE_ROOT}/src/game/Object/GameObjectUse.cpp" use_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandlerCustomize.cpp" customize_source)

if(MUTATION STREQUAL "sender")
    string(REPLACE
        "WorldPacket data(SMSG_ENABLE_BARBER_SHOP, 0);"
        "WorldPacket data(SMSG_ENABLE_BARBER_SHOP, 1);"
        use_source "${use_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefS(SMSG_ENABLE_BARBER_SHOP, \"SMSG_ENABLE_BARBER_SHOP\");"
        "/* removed barber-shop registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE
        "case SMSG_ENABLE_BARBER_SHOP:"
        "case REMOVED_ENABLE_BARBER_SHOP:"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE
        "SMSG_ENABLE_BARBER_SHOP                      = 0x1222,"
        "SMSG_ENABLE_BARBER_SHOP                      = 0x0221,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE
        "SMSG_ENABLE_BARBER_SHOP                        0x1222  ACTIVE   [high-conf]"
        "SMSG_ENABLE_BARBER_SHOP                        0x1222  DORMANT  [low-conf]"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "semantic_comment")
    string(REPLACE
        "The 18414 terminal consumes no payload and fires BARBER_SHOP_OPEN."
        "removed direct-client semantic proof"
        use_source "${use_source}")
elseif(MUTATION STREQUAL "request_registration")
    string(REPLACE
        "DefC(CMSG_ALTER_APPEARANCE, \"CMSG_ALTER_APPEARANCE\""
        "DefC(REMOVED_ALTER_APPEARANCE, \"CMSG_ALTER_APPEARANCE\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "result_registration")
    string(REPLACE
        "DefS(SMSG_BARBER_SHOP_RESULT, \"SMSG_BARBER_SHOP_RESULT\");"
        "/* removed barber result registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "result_allowlist")
    string(REPLACE
        "case SMSG_BARBER_SHOP_RESULT:"
        "case REMOVED_BARBER_SHOP_RESULT:"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "request_order")
    string(REPLACE
        "recv_data >> skinStyle >> color >> hairStyle >> facialHairStyle;"
        "recv_data >> hairStyle >> color >> facialHairStyle >> skinStyle;"
        customize_source "${customize_source}")
elseif(MUTATION STREQUAL "request_size")
    string(REPLACE
        "if (recv_data.size() != 16)"
        "if (recv_data.size() != 15)"
        customize_source "${customize_source}")
elseif(MUTATION STREQUAL "request_skin_lookup")
    string(REPLACE
        "sBarberShopStyleStore.LookupEntry(skinStyle);"
        "sBarberShopStyleStore.LookupEntry(hairStyle);"
        customize_source "${customize_source}")
elseif(MUTATION STREQUAL "request_hair_lookup")
    string(REPLACE
        "sBarberShopStyleStore.LookupEntry(hairStyle);"
        "sBarberShopStyleStore.LookupEntry(skinStyle);"
        customize_source "${customize_source}")
elseif(MUTATION STREQUAL "request_facial_lookup")
    string(REPLACE
        "sBarberShopStyleStore.LookupEntry(facialHairStyle);"
        "sBarberShopStyleStore.LookupEntry(skinStyle);"
        customize_source "${customize_source}")
elseif(MUTATION STREQUAL "result_success")
    string(REPLACE
        "data << uint32(0);                                  // ok"
        "data << uint32(1);                                  // wrong"
        customize_source "${customize_source}")
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
    "The 18414 terminal consumes no payload and fires BARBER_SHOP_OPEN." 1
    "direct-client barber semantic comment")
require_count("${use_source}"
    "WorldPacket data(SMSG_ENABLE_BARBER_SHOP, 0);" 1
    "empty barber-shop opener sender")
require_count("${opcode_registry}"
    "DefS(SMSG_ENABLE_BARBER_SHOP, \"SMSG_ENABLE_BARBER_SHOP\");" 1
    "barber-shop opener registration")
require_count("${session_source}"
    "case SMSG_ENABLE_BARBER_SHOP:" 1
    "barber-shop opener converted-packet admission")
require_count("${opcode_header}"
    "SMSG_ENABLE_BARBER_SHOP                      = 0x1222," 1
    "barber-shop opener opcode")
require_count("${opcode_reference}"
    "SMSG_ENABLE_BARBER_SHOP                        0x1222  ACTIVE   [high-conf]" 1
    "barber-shop opener reference status")
require_count("${opcode_registry}"
    "DefC(CMSG_ALTER_APPEARANCE, \"CMSG_ALTER_APPEARANCE\"" 1
    "alter-appearance request registration")
require_count("${opcode_registry}"
    "DefS(SMSG_BARBER_SHOP_RESULT, \"SMSG_BARBER_SHOP_RESULT\");" 1
    "barber-shop result registration")
require_count("${session_source}"
    "case SMSG_BARBER_SHOP_RESULT:" 1
    "barber-shop result converted-packet admission")
require_count("${customize_source}"
    "recv_data >> skinStyle >> color >> hairStyle >> facialHairStyle;" 1
    "direct 18414 alter-appearance field order")
require_count("${customize_source}"
    "if (recv_data.size() != 16)" 1
    "exact alter-appearance body-size guard")
require_count("${customize_source}"
    "sBarberShopStyleStore.LookupEntry(skinStyle);" 1
    "skin-style DBC validation")
require_count("${customize_source}"
    "sBarberShopStyleStore.LookupEntry(hairStyle);" 1
    "hair-style DBC validation")
require_count("${customize_source}"
    "sBarberShopStyleStore.LookupEntry(facialHairStyle);" 1
    "facial-hair-style DBC validation")
require_count("${customize_source}"
    "data << uint32(0);                                  // ok" 1
    "barber-shop success result")
require_count("${opcode_reference}"
    "SMSG_BARBER_SHOP_RESULT                        0x0C3F  ACTIVE   [high-conf]" 1
    "barber-shop result reference status")
require_count("${opcode_reference}"
    "CMSG_ALTER_APPEARANCE                          0x07F0  ACTIVE" 1
    "alter-appearance reference status")
