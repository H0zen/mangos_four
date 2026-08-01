if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Mail.h" mail_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MailMoneyPolicy.h" policy_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Mail.cpp" mail_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MailHandler.cpp" mail_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MopMailPackets.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/ChatCommands/MailCommands.cpp" mail_commands)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ChatArgExtract.cpp" chat_arg_extract)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerLoad.cpp" player_load)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerSave.cpp" player_save)
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectMgr.cpp" object_mgr)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/modules/Eluna/LuaEngine.cpp" eluna_engine)
file(READ "${SOURCE_ROOT}/src/modules/Eluna/methods/Mangos/GlobalMethods.h"
    eluna_global_methods)

function(require_literal_once content needle context)
    set(remaining "${content}")
    set(count 0)
    while(TRUE)
        string(FIND "${remaining}" "${needle}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        math(EXPR next_position "${position} + 1")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one literal, found ${count}")
    endif()
endfunction()

function(require_none content needle context)
    string(FIND "${content}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden literal remains")
    endif()
endfunction()

function(replace_literal_once variable old_value new_value mutation_name)
    set(content "${${variable}}")
    set(remaining "${content}")
    set(count 0)
    while(TRUE)
        string(FIND "${remaining}" "${old_value}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        math(EXPR next_position "${position} + 1")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "mutation target unavailable or ambiguous: ${mutation_name}: found ${count}")
    endif()
    string(REPLACE "${old_value}" "${new_value}" content "${content}")
    set(${variable} "${content}" PARENT_SCOPE)
endfunction()

set(wide_insert [=["VALUES ('%u', '%u', '%u', '%u', '%u', '%u', '%s', '%s', '%u', '" UI64FMTD "', '" UI64FMTD "', '" UI64FMTD "', '" UI64FMTD "', '%u')",]=])
set(narrow_money_insert [=["VALUES ('%u', '%u', '%u', '%u', '%u', '%u', '%s', '%s', '%u', '" UI64FMTD "', '" UI64FMTD "', '%u', '" UI64FMTD "', '%u')",]=])
set(narrow_cod_insert [=["VALUES ('%u', '%u', '%u', '%u', '%u', '%u', '%s', '%s', '%u', '" UI64FMTD "', '" UI64FMTD "', '" UI64FMTD "', '%u', '%u')",]=])
set(wide_send_log [=[DEBUG_LOG("%s is sending mail to %s with subject %s and body %s includes %u items, " UI64FMTD " copper and " UI64FMTD " COD copper with unk1 = %u, unk2 = %u",]=])
set(narrow_send_log [=[DEBUG_LOG("%s is sending mail to %s with subject %s and body %s includes %u items, %u copper and %u COD copper with unk1 = %u, unk2 = %u",]=])

if(DEFINED MUTATION)
    if(MUTATION_TARGET_DRIFT_PROBE)
        string(REPLACE "SetCOD(uint64 COD)" "SetCOD(uint64 value)" mail_header "${mail_header}")
    endif()

    if(MUTATION STREQUAL "setcod_uint32")
        replace_literal_once(mail_header "SetCOD(uint64 COD)" "SetCOD(uint32 COD)" "${MUTATION}")
    elseif(MUTATION STREQUAL "insert_money_format")
        replace_literal_once(mail_source "${wide_insert}" "${narrow_money_insert}" "${MUTATION}")
    elseif(MUTATION STREQUAL "insert_cod_format")
        replace_literal_once(mail_source "${wide_insert}" "${narrow_cod_insert}" "${MUTATION}")
    elseif(MUTATION STREQUAL "insert_money_cast")
        replace_literal_once(mail_source "uint64(m_money)" "uint32(m_money)" "${MUTATION}")
    elseif(MUTATION STREQUAL "insert_cod_cast")
        replace_literal_once(mail_source "uint64(m_COD)" "uint32(m_COD)" "${MUTATION}")
    elseif(MUTATION STREQUAL "load_money_uint32")
        replace_literal_once(player_load "m->money = fields[8].GetUInt64();" "m->money = fields[8].GetUInt32();" "${MUTATION}")
    elseif(MUTATION STREQUAL "load_cod_uint32")
        replace_literal_once(player_load "m->COD = fields[9].GetUInt64();" "m->COD = fields[9].GetUInt32();" "${MUTATION}")
    elseif(MUTATION STREQUAL "save_money_uint32")
        replace_literal_once(player_save "stmt.addUInt64(m->money);" "stmt.addUInt32(m->money);" "${MUTATION}")
    elseif(MUTATION STREQUAL "save_cod_uint32")
        replace_literal_once(player_save "stmt.addUInt64(m->COD);" "stmt.addUInt32(m->COD);" "${MUTATION}")
    elseif(MUTATION STREQUAL "expired_cod_uint32")
        replace_literal_once(object_mgr "m->COD = fields[6].GetUInt64();" "m->COD = fields[6].GetUInt32();" "${MUTATION}")
    elseif(MUTATION STREQUAL "delete_return_money_uint32")
        replace_literal_once(player_source "uint64 money         = fields[6].GetUInt64();" "uint64 money         = fields[6].GetUInt32();" "${MUTATION}")
    elseif(MUTATION STREQUAL "send_debug_money_u32")
        replace_literal_once(mail_handler "${wide_send_log}" "${narrow_send_log}" "${MUTATION}")
    elseif(MUTATION STREQUAL "gm_send_money_uint32")
        replace_literal_once(mail_commands
            "uint64 money;\n    if (!ExtractUInt64(&args, money))"
            "uint32 money;\n    if (!ExtractUInt32(&args, money))"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "gm_uint64_parser_strtoul")
        replace_literal_once(chat_arg_extract
            "unsigned long long const valRaw = std::strtoull(*args, &tail, 10);"
            "unsigned long const valRaw = std::strtoul(*args, &tail, 10);"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "gm_uint64_parser_drop_negative")
        replace_literal_once(chat_arg_extract
            "if (*first == '-')"
            "if (false)"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "gm_uint64_parser_drop_erange")
        replace_literal_once(chat_arg_extract
            "tail == *args || errno == ERANGE || valRaw > std::numeric_limits<uint64>::max()"
            "tail == *args || valRaw > std::numeric_limits<uint64>::max()"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "eluna_uint64_number_uint32")
        replace_literal_once(eluna_engine
            "return CheckUnsignedLongLong(L, narg);"
            "return static_cast<unsigned long long>(CHECKVAL<uint32>(narg));"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "eluna_uint64_drop_negative_guard")
        replace_literal_once(eluna_engine
            "if (!(value >= 0))"
            "if (false)"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "eluna_uint64_drop_safe_bound")
        replace_literal_once(eluna_engine
            "if (value > MAX_SAFE_LUA_INTEGER)"
            "if (false)"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "eluna_mail_money_uint32")
        replace_literal_once(eluna_global_methods
            "uint64 money = E->CHECKVAL<uint64>(++i, 0);"
            "uint32 money = E->CHECKVAL<uint32>(++i, 0);"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "eluna_mail_cod_uint32")
        replace_literal_once(eluna_global_methods
            "uint64 cod = E->CHECKVAL<uint64>(++i, 0);"
            "uint32 cod = E->CHECKVAL<uint32>(++i, 0);"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "eluna_mail_mists_gate")
        replace_literal_once(eluna_global_methods
            "#if defined(ELUNA_MANGOS) && ELUNA_EXPANSION == EXP_MISTS\n        uint64 money = E->CHECKVAL<uint64>(++i, 0);\n        uint64 cod = E->CHECKVAL<uint64>(++i, 0);"
            "#if defined(ELUNA_MANGOS) && ELUNA_EXPANSION == EXP_CATA\n        uint64 money = E->CHECKVAL<uint64>(++i, 0);\n        uint64 cod = E->CHECKVAL<uint64>(++i, 0);"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "eluna_mail_money_doc_uint32")
        replace_literal_once(eluna_global_methods
            "@param uint64 money = 0 : money to send (uint32 before Mists)"
            "@param uint32 money = 0 : money to send"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "eluna_mail_cod_doc_uint32")
        replace_literal_once(eluna_global_methods
            "@param uint64 cod = 0 : cod money amount (uint32 before Mists)"
            "@param uint32 cod = 0 : cod money amount"
            "${MUTATION}")
    elseif(MUTATION STREQUAL "packet_cod_uint32")
        replace_literal_once(packet_builder "uint64 cod = 0;" "uint32 cod = 0;" "${MUTATION}")
    elseif(MUTATION STREQUAL "packet_money_uint32")
        replace_literal_once(packet_builder "uint64 money = 0;" "uint32 money = 0;" "${MUTATION}")
    elseif(MUTATION STREQUAL "cod_limit_plus_one")
        replace_literal_once(policy_header "MAX_PLAYER_COD = UI64LIT(100000000)" "MAX_PLAYER_COD = UI64LIT(100000001)" "${MUTATION}")
    elseif(MUTATION STREQUAL "cod_exclusive_limit")
        replace_literal_once(policy_header "return cod <= MAX_PLAYER_COD;" "return cod < MAX_PLAYER_COD;" "${MUTATION}")
    elseif(MUTATION STREQUAL "debit_addition")
        replace_literal_once(policy_header "return fee <= current && amount <= current - fee;" "return amount + fee <= current;" "${MUTATION}")
    elseif(MUTATION STREQUAL "debit_drop_fee_guard")
        replace_literal_once(policy_header "return fee <= current && amount <= current - fee;" "return amount <= current - fee;" "${MUTATION}")
    elseif(MUTATION STREQUAL "credit_addition")
        replace_literal_once(policy_header "return current <= limit && amount <= limit - current;" "return current + amount <= limit;" "${MUTATION}")
    elseif(MUTATION STREQUAL "credit_drop_current_guard")
        replace_literal_once(policy_header "return current <= limit && amount <= limit - current;" "return amount <= limit - current;" "${MUTATION}")
    elseif(MUTATION STREQUAL "register_send_mail")
        string(APPEND opcode_registry "\nDefC(CMSG_SEND_MAIL, STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSendMail);\n")
    elseif(MUTATION STREQUAL "activate_send_mail")
        replace_literal_once(opcode_reference "CMSG_SEND_MAIL                                 0x1DBA  DORMANT" "CMSG_SEND_MAIL                                 0x1DBA  ACTIVE" "${MUTATION}")
    else()
        message(FATAL_ERROR "unknown mail money width mutation: ${MUTATION}")
    endif()
endif()

require_literal_once("${mail_header}" "SetCOD(uint64 COD)" "MailDraft COD setter width")
require_literal_once("${policy_header}" "MAX_PLAYER_COD = UI64LIT(100000000)" "inclusive player COD maximum")
require_literal_once("${policy_header}" "return cod <= MAX_PLAYER_COD;" "inclusive player COD predicate")
require_literal_once("${policy_header}" "return fee <= current && amount <= current - fee;" "overflow-safe debit predicate")
require_literal_once("${policy_header}" "return current <= limit && amount <= limit - current;" "overflow-safe credit predicate")
require_literal_once("${player_load}" "m->money = fields[8].GetUInt64();" "login money read width")
require_literal_once("${player_load}" "m->COD = fields[9].GetUInt64();" "login COD read width")
require_literal_once("${player_save}" "stmt.addUInt64(m->money);" "dirty-mail money bind width")
require_literal_once("${player_save}" "stmt.addUInt64(m->COD);" "dirty-mail COD bind width")
require_literal_once("${object_mgr}" "m->COD = fields[6].GetUInt64();" "expired-mail COD read width")
require_literal_once("${player_source}" "uint64 money         = fields[6].GetUInt64();" "delete-return money read width")
require_literal_once("${packet_builder}" "uint64 cod = 0;" "mail-list COD field width")
require_literal_once("${packet_builder}" "uint64 money = 0;" "mail-list money field width")
require_literal_once("${packet_builder}" "out << mail.mailTemplateId << mail.cod;" "mail-list COD writer")
require_literal_once("${packet_builder}" "mail.daysLeft << mail.money" "mail-list money writer")
require_literal_once("${mail_source}" "${wide_insert}" "mail insert format widths")
require_literal_once("${mail_source}" "uint64(m_money)" "mail insert money argument width")
require_literal_once("${mail_source}" "uint64(m_COD)" "mail insert COD argument width")
require_literal_once("${mail_handler}" "${wide_send_log}" "send-mail diagnostic widths")
require_literal_once("${mail_commands}"
    "uint64 money;\n    if (!ExtractUInt64(&args, money))"
    "GM send-money parser width")
require_literal_once("${chat_arg_extract}"
    "unsigned long long const valRaw = std::strtoull(*args, &tail, 10);"
    "GM unsigned-64 platform-width parser")
require_literal_once("${chat_arg_extract}"
    "if (*first == '-')"
    "GM unsigned-64 negative-input rejection")
require_literal_once("${chat_arg_extract}"
    "tail == *args || errno == ERANGE || valRaw > std::numeric_limits<uint64>::max()"
    "GM unsigned-64 conversion and overflow rejection")
require_literal_once("${eluna_engine}"
    "constexpr double MAX_SAFE_LUA_INTEGER = 9007199254740991.0;"
    "Eluna exact Lua-number integer bound")
require_literal_once("${eluna_engine}"
    "if (!(value >= 0))"
    "Eluna unsigned-64 negative and NaN guard")
require_literal_once("${eluna_engine}"
    "if (value > MAX_SAFE_LUA_INTEGER)"
    "Eluna unsigned-64 exactness guard")
require_literal_once("${eluna_engine}"
    "return CheckUnsignedLongLong(L, narg);"
    "Eluna unsigned-64 Lua-number conversion")
require_literal_once("${eluna_global_methods}"
    "uint64 money = E->CHECKVAL<uint64>(++i, 0);"
    "Eluna SendMail money parser width")
require_literal_once("${eluna_global_methods}"
    "uint64 cod = E->CHECKVAL<uint64>(++i, 0);"
    "Eluna SendMail COD parser width")
require_literal_once("${eluna_global_methods}"
    "#if defined(ELUNA_MANGOS) && ELUNA_EXPANSION == EXP_MISTS\n        uint64 money = E->CHECKVAL<uint64>(++i, 0);\n        uint64 cod = E->CHECKVAL<uint64>(++i, 0);\n#else\n        uint32 money = E->CHECKVAL<uint32>(++i, 0);\n        uint32 cod = E->CHECKVAL<uint32>(++i, 0);\n#endif"
    "Mists-only Eluna SendMail width gate")
require_literal_once("${eluna_global_methods}"
    "@param uint64 money = 0 : money to send (uint32 before Mists)"
    "Eluna SendMail money documentation width")
require_literal_once("${eluna_global_methods}"
    "@param uint64 cod = 0 : cod money amount (uint32 before Mists)"
    "Eluna SendMail COD documentation width")
require_none("${opcode_registry}" "DefC(CMSG_SEND_MAIL" "CMSG_SEND_MAIL registration")
require_literal_once("${opcode_reference}" "CMSG_SEND_MAIL                                 0x1DBA  DORMANT" "CMSG_SEND_MAIL dormant reference")
