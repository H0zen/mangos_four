if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MailHandler.cpp" mail_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MopMailPackets.h" builder)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" reference)

if(MUTATION STREQUAL "builder_route")
    string(REPLACE "MopMailPackets::BuildList(data, realCount, stagedMails)"
        "LegacyMailPackets::BuildList(data, realCount, stagedMails)"
        mail_handler "${mail_handler}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefC(CMSG_GET_MAIL_LIST," "DefC(CMSG_GET_MAIL_LIST_MUTATED,"
        registry "${registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_MAIL_LIST_RESULT:" "case SMSG_MAIL_LIST_RESULT_MUTATED:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_MAIL_LIST_RESULT                          0x1C0B  ACTIVE"
        "SMSG_MAIL_LIST_RESULT                          0x1C0B  DORMANT"
        reference "${reference}")
elseif(MUTATION STREQUAL "missing_item")
    string(REPLACE "if (!item)" "if (item)" mail_handler "${mail_handler}")
elseif(MUTATION STREQUAL "utf8_bounds")
    string(REPLACE "MopMailPackets::TruncateUtf8"
        "MopMailPackets::DoNotTruncateUtf8" mail_handler "${mail_handler}")
elseif(MUTATION STREQUAL "enchant_count")
    string(REPLACE "j < MopMailPackets::ENCHANT_GROUP_COUNT"
        "j < MAX_INSPECTED_ENCHANTMENT_SLOT" mail_handler "${mail_handler}")
elseif(MUTATION STREQUAL "legacy_limit")
    string(APPEND mail_handler "\nconst uint32 maxPacketSize = 32767;\n")
elseif(MUTATION STREQUAL "mail_count")
    string(REPLACE "++realCount;" "" mail_handler "${mail_handler}")
elseif(MUTATION STREQUAL "item_cap")
    string(REPLACE "size_t(MAX_MAIL_ITEMS)" "mail->items.size()"
        mail_handler "${mail_handler}")
elseif(MUTATION STREQUAL "player_guid_domain")
    string(REPLACE "MopMailPackets::BuildPlayerSenderGuid(mail->sender)"
        "ObjectGuid(HIGHGUID_PLAYER, mail->sender).GetRawValue()"
        mail_handler "${mail_handler}")
elseif(MUTATION STREQUAL "frame_limit")
    string(REPLACE "return out.size() <= MAX_POST_CRYPT_PAYLOAD_BYTES;"
        "return true;" builder "${builder}")
endif()

set(ws "[ \t\r\n]")

function(mop_strip_cxx_comments in_text out_var)
    string(REGEX REPLACE "/[*][^*]*[*]+/" "" stripped "${in_text}")
    string(REGEX REPLACE "//[^
]*" "" stripped "${stripped}")
    set(${out_var} "${stripped}" PARENT_SCOPE)
endfunction()

mop_strip_cxx_comments("${mail_handler}" handler_code)
mop_strip_cxx_comments("${registry}" registry_code)
mop_strip_cxx_comments("${world_session}" session_code)

if(NOT mail_handler MATCHES "[#]include${ws}*\"MopMailPackets[.]h\"")
    message(FATAL_ERROR "mail handler does not include the 18414 packet builder")
endif()
if(NOT handler_code MATCHES
        "MopMailPackets::BuildList${ws}*[(]${ws}*data${ws}*,${ws}*realCount${ws}*,${ws}*stagedMails${ws}*[)]")
    message(FATAL_ERROR "mail-list producer bypasses the staged 18414 builder")
endif()
if(handler_code MATCHES "maxPacketSize${ws}*=${ws}*32767" OR
        handler_code MATCHES "next_mail_size" OR
        handler_code MATCHES "data[.]put<uint8>${ws}*[(]${ws}*4")
    message(FATAL_ERROR "legacy 3.3.5 mail-list framing or 32767 limit remains")
endif()
if(NOT handler_code MATCHES "if${ws}*[(]${ws}*!item${ws}*[)]${ws}*continue")
    message(FATAL_ERROR "missing mail items are not filtered before count emission")
endif()
if(NOT handler_code MATCHES
        "[+][+]realCount${ws}*;${ws}*if${ws}*[(]${ws}*stagedMails[.]size[(][)]${ws}*>=${ws}*MopMailPackets::MAX_MAIL_COUNT${ws}*[)]${ws}*continue")
    message(FATAL_ERROR "mail count does not retain deliverable records beyond the 50-mail display cap")
endif()
if(NOT handler_code MATCHES
        "std::min${ws}*[(]${ws}*mail->items[.]size[(][)]${ws}*,${ws}*size_t${ws}*[(]${ws}*MAX_MAIL_ITEMS${ws}*[)]${ws}*[)]")
    message(FATAL_ERROR "production staging does not cap attachments at MAX_MAIL_ITEMS")
endif()
if(NOT handler_code MATCHES "MopMailPackets::TruncateUtf8${ws}*[(]")
    message(FATAL_ERROR "mail text is not bounded on a UTF-8 boundary before staging")
endif()
if(NOT handler_code MATCHES "j${ws}*<${ws}*MopMailPackets::ENCHANT_GROUP_COUNT")
    message(FATAL_ERROR "mail attachment writer does not use the binary-proven eight groups")
endif()
string(REGEX MATCHALL
    "MopMailPackets::BuildPlayerSenderGuid${ws}*[(]${ws}*mail->sender${ws}*[)]"
    player_guid_conversions "${handler_code}")
list(LENGTH player_guid_conversions player_guid_conversion_count)
if(NOT player_guid_conversion_count EQUAL 2)
    message(FATAL_ERROR "mail producers do not convert stored LowGUIDs to the 18414 player domain")
endif()
if(NOT builder MATCHES
        "MAX_POST_CRYPT_PAYLOAD_BYTES${ws}*=${ws}*0x7FFFF" OR
        NOT builder MATCHES
        "return${ws}+out[.]size[(][)]${ws}*<=${ws}*MAX_POST_CRYPT_PAYLOAD_BYTES")
    message(FATAL_ERROR "mail builder does not enforce the final post-crypt frame limit")
endif()
if(handler_code MATCHES "modifierBlob${ws}*=")
    message(FATAL_ERROR "production invents item-modifier data without a backend")
endif()
if(handler_code MATCHES "hasOptional[AB]${ws}*=")
    message(FATAL_ERROR "production invents unresolved optional sender identity fields")
endif()
if(NOT registry_code MATCHES "DefC${ws}*[(]${ws}*CMSG_GET_MAIL_LIST${ws}*," OR
        NOT registry_code MATCHES "DefS${ws}*[(]${ws}*SMSG_MAIL_LIST_RESULT${ws}*,")
    message(FATAL_ERROR "mail-list request/reply registration is not atomic")
endif()
if(NOT session_code MATCHES "case${ws}+SMSG_MAIL_LIST_RESULT${ws}*:")
    message(FATAL_ERROR "mail-list reply is not admitted to the in-world send gate")
endif()
if(NOT reference MATCHES
        "SMSG_MAIL_LIST_RESULT${ws}+0x1C0B${ws}+ACTIVE" OR
        NOT reference MATCHES "CMSG_GET_MAIL_LIST${ws}+0x077A${ws}+ACTIVE")
    message(FATAL_ERROR "mail-list opcode reference state is stale")
endif()
