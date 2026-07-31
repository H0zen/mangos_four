file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GuildHandler.cpp" guild_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "activate_mask_order")
    string(REPLACE
        "guid[1] = in.ReadBit();\n        guid[5] = in.ReadBit();"
        "guid[5] = in.ReadBit();\n        guid[1] = in.ReadBit();"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "activate_flag_position")
    string(REPLACE
        "guid[3] = in.ReadBit();\n        bool const parsedFullSlotRefresh = in.ReadBit();\n        guid[0] = in.ReadBit();"
        "guid[3] = in.ReadBit();\n        guid[0] = in.ReadBit();\n        bool const parsedFullSlotRefresh = in.ReadBit();"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "activate_byte_order")
    string(REPLACE
        "{ 7, 1, 0, 6, 4, 2, 5, 3 }"
        "{ 1, 7, 0, 6, 4, 2, 5, 3 }"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "activate_xor")
    string(REPLACE
        "in.ReadByteSeq(guid[byteOrder[index]]);"
        "in >> guid[byteOrder[index]];"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "activate_exact_length")
    string(REPLACE
        "if (remaining != 2 + guidByteCount)"
        "if (remaining < 2 + guidByteCount)"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "activate_padding")
    string(REPLACE
        "if ((secondMask & 0x7F) != 0)"
        "if (false)"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "activate_canonical_byte")
    string(REPLACE
        "if (in[index] == 1)"
        "if (false)"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "activate_all_zero")
    string(REPLACE
        "if (raw == 0 || in.rpos() != in.size())"
        "if (in.rpos() != in.size())"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "activate_handler_route")
    string(REPLACE
        "MopCompactPackets::ReadGuildBankerActivate("
        "LegacyGuildBankerActivateReader("
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "activate_interaction")
    string(REPLACE
        "GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK)"
        "GetPlayer()->GetGameObject(goGuid)"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "activate_registration")
    string(APPEND opcode_registry
        "\nDefC(CMSG_GUILD_BANKER_ACTIVATE, \"CMSG_GUILD_BANKER_ACTIVATE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGuildBankerActivate);\n")
elseif(MUTATION STREQUAL "query_registration")
    string(APPEND opcode_registry
        "\nDefC(CMSG_GUILD_BANK_QUERY_TAB, \"CMSG_GUILD_BANK_QUERY_TAB\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGuildBankQueryTab);\n")
elseif(MUTATION STREQUAL "list_registration")
    string(APPEND opcode_registry
        "\nDefS(SMSG_GUILD_BANK_LIST, \"SMSG_GUILD_BANK_LIST\");\n")
elseif(MUTATION STREQUAL "list_admission")
    string(APPEND world_session "\ncase SMSG_GUILD_BANK_LIST:\n")
elseif(MUTATION STREQUAL "activate_refresh_side_effect")
    string(REPLACE
        "uint32(fullSlotRefresh));"
        "uint32(fullSlotRefresh));\n\n    if (fullSlotRefresh)\n        return;"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "reference_activate")
    string(REPLACE
        "CMSG_GUILD_BANKER_ACTIVATE                     0x0372  DORMANT"
        "CMSG_GUILD_BANKER_ACTIVATE                     0x0372  ACTIVE"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "reference_query")
    string(REPLACE
        "CMSG_GUILD_BANK_QUERY_TAB                      0x1372  DORMANT"
        "CMSG_GUILD_BANK_QUERY_TAB                      0x1372  ACTIVE"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "reference_list")
    string(REPLACE
        "SMSG_GUILD_BANK_LIST                           0x0B79  DORMANT"
        "SMSG_GUILD_BANK_LIST                           0x0B79  ACTIVE"
        opcode_reference "${opcode_reference}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context} guard: expected exactly one match, found ${count}")
    endif()
endfunction()

function(require_text source token context)
    string(FIND "${source}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${context} guard: required text not found")
    endif()
endfunction()

string(FIND "${unit_header}" "inline bool ReadGuildBankerActivate" parser_start)
string(FIND "${unit_header}" "inline uint64 ReadPrefixedPackedValue" parser_end)
if(parser_start EQUAL -1 OR parser_end LESS_EQUAL parser_start)
    message(FATAL_ERROR "activate parser seam guard: could not isolate parser")
endif()
math(EXPR parser_length "${parser_end} - ${parser_start}")
string(SUBSTRING "${unit_header}" ${parser_start} ${parser_length} parser)

require_once("${parser}"
    "ReadGuildBankerActivate[(]WorldPacket& in,[\r\n\t ]*ObjectGuid& bankGuid, bool& fullSlotRefresh[)]"
    "activate parser signature")
require_once("${parser}"
    "if [(][(]secondMask & 0x7F[)] != 0[)]"
    "activate padding")
require_once("${parser}"
    "if [(]remaining != 2 [+] guidByteCount[)]"
    "activate exact length")
require_text("${parser}" "if (in[index] == 1)" "activate canonical byte")
require_text("${parser}" "uint8 const byteOrder[] = { 7, 1, 0, 6, 4, 2, 5, 3 }"
    "activate byte order")
require_text("${parser}" "in.ReadByteSeq(guid[byteOrder[index]]);"
    "activate XOR byte decoding")
require_text("${parser}" "if (raw == 0 || in.rpos() != in.size())"
    "activate all-zero GUID")

string(FIND "${parser}"
    "guid[3] = in.ReadBit();\n        bool const parsedFullSlotRefresh = in.ReadBit();\n        guid[0] = in.ReadBit();"
    flag_position)
if(flag_position EQUAL -1)
    message(FATAL_ERROR "activate standalone flag position guard")
endif()

string(FIND "${parser}" "guid[3] = in.ReadBit();" mask3)
string(FIND "${parser}" "guid[0] = in.ReadBit();" mask0)
string(FIND "${parser}" "guid[7] = in.ReadBit();" mask7)
string(FIND "${parser}" "guid[1] = in.ReadBit();" mask1)
string(FIND "${parser}" "guid[5] = in.ReadBit();" mask5)
string(FIND "${parser}" "guid[2] = in.ReadBit();" mask2)
string(FIND "${parser}" "guid[6] = in.ReadBit();" mask6)
string(FIND "${parser}" "guid[4] = in.ReadBit();" mask4)
if(mask3 EQUAL -1 OR mask0 LESS_EQUAL mask3 OR mask7 LESS_EQUAL mask0 OR
        mask1 LESS_EQUAL mask7 OR mask5 LESS_EQUAL mask1 OR
        mask2 LESS_EQUAL mask5 OR mask6 LESS_EQUAL mask2 OR
        mask4 LESS_EQUAL mask6)
    message(FATAL_ERROR "activate mask order guard")
endif()

string(FIND "${guild_handler}" "void WorldSession::HandleGuildBankerActivate" handler_start)
string(FIND "${guild_handler}" "void WorldSession::HandleGuildBankQueryTab" handler_end)
if(handler_start EQUAL -1 OR handler_end LESS_EQUAL handler_start)
    message(FATAL_ERROR "activate handler seam guard: could not isolate handler")
endif()
math(EXPR handler_length "${handler_end} - ${handler_start}")
string(SUBSTRING "${guild_handler}" ${handler_start} ${handler_length} handler)

require_once("${handler}"
    "MopCompactPackets::ReadGuildBankerActivate[(]"
    "activate handler route")
if("${handler}" MATCHES "recv_data[\r\n\t ]*>>[\r\n\t ]*goGuid")
    message(FATAL_ERROR "activate handler route guard: legacy raw GUID reader remains")
endif()
string(FIND "${handler}" "MopCompactPackets::ReadGuildBankerActivate(" parser_route)
string(FIND "${handler}"
    "GetPlayer()->GetGameObjectIfCanInteractWith(goGuid, GAMEOBJECT_TYPE_GUILD_BANK)"
    interaction_check)
if(parser_route EQUAL -1 OR interaction_check LESS_EQUAL parser_route)
    message(FATAL_ERROR "activate interaction guard: parser must precede guild-bank object check")
endif()
require_once("${handler}" "FullSlotRefresh %u" "activate refresh-hint log")
require_once("${handler}"
    "bool fullSlotRefresh = false"
    "activate refresh-hint declaration")
string(REGEX MATCHALL "fullSlotRefresh" refresh_hint_uses "${handler}")
list(LENGTH refresh_hint_uses refresh_hint_use_count)
if(NOT refresh_hint_use_count EQUAL 3)
    message(FATAL_ERROR
        "activate refresh-hint side-effect guard: expected parser output and log only")
endif()

require_text("${opcode_registry}"
    "The 18414 CMSG_GUILD_BANKER_ACTIVATE request body is resolved, but the\n    // wave remains held solely because the first uint32 in each present-item\n    // SMSG_GUILD_BANK_LIST record maps to the client's +48 dynamic-flags field,\n    // whose server-side meaning and state are not yet modelled."
    "guild-bank dormancy rationale")

require_once("${opcode_header}"
    "CMSG_GUILD_BANKER_ACTIVATE[\t ]*=[\t ]*0x0372"
    "activate opcode value")

if("${opcode_registry}" MATCHES "DefC[(]CMSG_GUILD_BANKER_ACTIVATE")
    message(FATAL_ERROR "activate registration dormant guard")
endif()
if("${opcode_registry}" MATCHES "DefC[(]CMSG_GUILD_BANK_QUERY_TAB")
    message(FATAL_ERROR "query registration dormant guard")
endif()
if("${opcode_registry}" MATCHES "DefS[(]SMSG_GUILD_BANK_LIST")
    message(FATAL_ERROR "list registration dormant guard")
endif()
if("${world_session}" MATCHES "case[\t ]+SMSG_GUILD_BANK_LIST:")
    message(FATAL_ERROR "list admission dormant guard")
endif()
require_once("${opcode_reference}"
    "CMSG_GUILD_BANKER_ACTIVATE[\t ]+0x0372[\t ]+DORMANT"
    "activate reference dormant")
require_once("${opcode_reference}"
    "CMSG_GUILD_BANK_QUERY_TAB[\t ]+0x1372[\t ]+DORMANT"
    "query reference dormant")
require_once("${opcode_reference}"
    "SMSG_GUILD_BANK_LIST[\t ]+0x0B79[\t ]+DORMANT"
    "list reference dormant")

message(STATUS "mop_guild_banker_activate_source: source checks passed")
