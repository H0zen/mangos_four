if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(notification_path "${SOURCE_ROOT}/src/game/Server/MopNotificationPackets.h")
file(READ "${notification_path}" notification)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerAreaTrigger.cpp" area_trigger)
file(READ "${SOURCE_ROOT}/src/game/ChatCommands/CommunicationCommands.cpp" communication)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/World.cpp" world)

function(count_text source token result)
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
    set(${result} ${count} PARENT_SCOPE)
endfunction()

function(replace_once variable old new)
    count_text("${${variable}}" "${old}" count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "mutation ${MUTATION}: expected exactly one replacement seam, found ${count}")
    endif()
    string(REPLACE "${old}" "${new}" value "${${variable}}")
    set(${variable} "${value}" PARENT_SCOPE)
endfunction()

if(MUTATION STREQUAL "builder_width")
    replace_once(notification "out.WriteBits(text.size(), 12);"
        "out.WriteBits(text.size(), 13);")
elseif(MUTATION STREQUAL "builder_flush")
    replace_once(notification "out.FlushBits();" "/* flush removed */")
elseif(MUTATION STREQUAL "builder_nul_append")
    replace_once(notification "out.append(text.data(), text.size());"
        "out.append(text.data(), text.size() + 1);")
elseif(MUTATION STREQUAL "builder_limit")
    replace_once(notification "constexpr size_t MAX_TEXT_BYTES = 1023;"
        "constexpr size_t MAX_TEXT_BYTES = 4095;")
elseif(MUTATION STREQUAL "builder_nul_policy")
    replace_once(notification " ||\n            text.find('\\0') != std::string::npos" "")
elseif(MUTATION STREQUAL "builder_preinit")
    replace_once(notification
        "        if (text.size() > MAX_TEXT_BYTES ||\n            text.find('\\0') != std::string::npos)"
        "        out.Initialize(SMSG_NOTIFICATION, 2 + text.size());\n        if (text.size() > MAX_TEXT_BYTES ||\n            text.find('\\0') != std::string::npos)")
elseif(MUTATION STREQUAL "session_bypass")
    replace_once(world_session
        "        WorldPacket data;\n        if (!MopNotificationPackets::Build(data, std::string(szStr)))\n        {\n            sLog.outError(\"A formatted notification violated the 1023-byte NUL-free packet contract; message dropped.\");\n            return;\n        }"
        "        WorldPacket data(SMSG_NOTIFICATION, strlen(szStr) + 1);\n        data.WriteBits(strlen(szStr), 12);\n        data.FlushBits();\n        data.append(szStr, strlen(szStr));")
elseif(MUTATION STREQUAL "notify_bypass")
    replace_once(communication
        "if (!MopNotificationPackets::Build(data, str))" "if (false)")
elseif(MUTATION STREQUAL "notify_legacy_string")
    replace_once(communication "    sWorld.SendGlobalMessage(&data);"
        "    data << str;\n    sWorld.SendGlobalMessage(&data);")
elseif(MUTATION STREQUAL "notify_send_order")
    replace_once(communication "    WorldPacket data;"
        "    sWorld.SendGlobalMessage(&data);\n    WorldPacket data;")
elseif(MUTATION STREQUAL "notify_prefix")
    replace_once(communication
        "std::string str = GetMangosString(LANG_GLOBAL_NOTIFY);"
        "std::string str;")
elseif(MUTATION STREQUAL "notify_truncate")
    replace_once(communication "    WorldPacket data;"
        "    str.resize(MopNotificationPackets::MAX_TEXT_BYTES);\n    WorldPacket data;")
elseif(MUTATION STREQUAL "registration")
    replace_once(opcode_registry
        "DefS(SMSG_NOTIFICATION, \"SMSG_NOTIFICATION\");"
        "/* removed notification registration */")
elseif(MUTATION STREQUAL "allowlist")
    replace_once(world_session "case SMSG_NOTIFICATION:"
        "case 0xFFFF: /* removed notification allowlist */")
elseif(MUTATION STREQUAL "reference_status")
    replace_once(opcode_reference
        "SMSG_NOTIFICATION                              0x0C2A  ACTIVE"
        "SMSG_NOTIFICATION                              0x0C2A  DORMANT")
elseif(MUTATION STREQUAL "opcode_value")
    replace_once(opcode_header
        "SMSG_NOTIFICATION                            = 0x0C2A"
        "SMSG_NOTIFICATION                            = 0x0C2B")
elseif(MUTATION STREQUAL "announce_route")
    replace_once(communication
        "sWorld.SendWorldText(LANG_SYSTEMMESSAGE, args);"
        "WorldPacket data; MopNotificationPackets::Build(data, args);")
elseif(MUTATION STREQUAL "server_message_route")
    replace_once(world "WorldPacket data(SMSG_SERVER_MESSAGE, 50);"
        "WorldPacket data(SMSG_NOTIFICATION, 50);")
elseif(MUTATION STREQUAL "area_trigger_route")
    replace_once(area_trigger
        "GetSession()->SendNotification(GetSession()->GetMangosString(LANG_LEVEL_MINREQUIRED), miscRequirement);"
        "GetSession()->SendAreaTriggerMessage(GetSession()->GetMangosString(LANG_LEVEL_MINREQUIRED), miscRequirement);")
elseif(MUTATION STREQUAL "admin_route")
    replace_once(communication
        "rPlayerSession->SendNotification(\"%s\", args);"
        "rPlayerSession->SendAreaTriggerMessage(\"%s\", args);")
elseif(MUTATION STREQUAL "compatibility_forward")
    replace_once(session_header "SendNotification(format, args...);"
        "/* removed Eluna compatibility forwarding */")
elseif(MUTATION STREQUAL "legacy_backend")
    replace_once(world_session
        "void WorldSession::SendNotification(int32 string_id, ...)"
        "void WorldSession::SendAreaTriggerMessage(const char*, ...) {}\n\nvoid WorldSession::SendNotification(int32 string_id, ...)")
elseif(MUTATION STREQUAL "legacy_opcode")
    replace_once(opcode_header
        "SMSG_NOTIFICATION                            = 0x0C2A"
        "SMSG_NOTIFICATION                            = 0x0C2A\n    SMSG_AREA_TRIGGER_MESSAGE                    = 0x0C2B")
elseif(DEFINED MUTATION AND NOT MUTATION STREQUAL "")
    message(FATAL_ERROR "unknown mutation: ${MUTATION}")
endif()

function(require_count source token expected context)
    count_text("${source}" "${token}" count)
    if(NOT count EQUAL expected)
        message(FATAL_ERROR
            "${context}: expected ${expected} active occurrences, found ${count}")
    endif()
endfunction()

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden token remains: ${token}")
    endif()
endfunction()

require_count("${notification}"
    "inline bool Build(WorldPacket& out, std::string const& text)" 1
    "canonical notification builder")
require_count("${notification}" "constexpr size_t MAX_TEXT_BYTES = 1023;" 1
    "1023-byte server policy")
require_count("${notification}" "text.size() > MAX_TEXT_BYTES" 1
    "length rejection")
require_count("${notification}" "text.find('\\0') != std::string::npos" 1
    "embedded-NUL rejection")
require_count("${notification}"
    "out.Initialize(SMSG_NOTIFICATION, 2 + text.size());" 1
    "transactional packet initialization")
require_count("${notification}" "out.WriteBits(text.size(), 12);" 1
    "12-bit byte length")
require_count("${notification}" "out.FlushBits();" 1
    "length padding flush")
require_count("${notification}" "out.append(text.data(), text.size());" 1
    "raw non-NUL text body")
forbid("${notification}" "text.size() + 1" "notification trailing NUL")
forbid("${notification}" "out << text" "legacy notification string encoding")

string(FIND "${notification}" "text.size() > MAX_TEXT_BYTES" validation_at)
string(FIND "${notification}"
    "out.Initialize(SMSG_NOTIFICATION, 2 + text.size());" initialize_at)
string(FIND "${notification}" "out.WriteBits(text.size(), 12);" length_at)
string(FIND "${notification}" "out.FlushBits();" flush_at)
string(FIND "${notification}" "out.append(text.data(), text.size());" append_at)
if(validation_at EQUAL -1 OR initialize_at LESS_EQUAL validation_at OR
        length_at LESS_EQUAL initialize_at OR flush_at LESS_EQUAL length_at OR
        append_at LESS_EQUAL flush_at)
    message(FATAL_ERROR
        "builder order: validation, initialize, length, flush, raw text")
endif()

require_count("${world_session}" "#include \"MopNotificationPackets.h\"" 1
    "session builder include")
require_count("${world_session}"
    "MopNotificationPackets::Build(data, std::string(szStr))" 2
    "both session notification writers")
forbid("${world_session}" "WorldPacket data(SMSG_NOTIFICATION"
    "session-local notification constructor")
forbid("${world_session}" "data.WriteBits(strlen(szStr)"
    "session-local notification length writer")
forbid("${world_session}" "data.append(szStr"
    "session-local notification body writer")

string(FIND "${communication}" "bool ChatHandler::HandleNotifyCommand" notify_begin)
string(FIND "${communication}" "bool ChatHandler::HandleMuteCommand" notify_end)
if(notify_begin EQUAL -1 OR notify_end LESS_EQUAL notify_begin)
    message(FATAL_ERROR "cannot isolate HandleNotifyCommand")
endif()
math(EXPR notify_length "${notify_end} - ${notify_begin}")
string(SUBSTRING "${communication}" ${notify_begin} ${notify_length} notify)

require_count("${communication}" "#include \"MopNotificationPackets.h\"" 1
    "notify builder include")
require_count("${notify}" "GetMangosString(LANG_GLOBAL_NOTIFY)" 1
    "localized notify prefix")
require_count("${notify}" "str += args;" 1 "notify argument append")
require_count("${notify}" "MopNotificationPackets::Build(data, str)" 1
    "notify canonical builder")
require_count("${notify}" "SendSysMessage(LANG_BAD_VALUE);" 1
    "notify invalid-input response")
require_count("${notify}" "SetSentErrorMessage(true);" 1
    "notify invalid-input command state")
require_count("${notify}" "sWorld.SendGlobalMessage(&data);" 1
    "notify global fan-out")
foreach(token IN ITEMS "data << str" "str.resize(" "str.substr("
        "str.size() + 1" "WorldPacket data(SMSG_NOTIFICATION")
    forbid("${notify}" "${token}" "notify legacy or truncating writer")
endforeach()

string(FIND "${notify}" "GetMangosString(LANG_GLOBAL_NOTIFY)" prefix_at)
string(FIND "${notify}" "str += args;" args_at)
string(FIND "${notify}" "MopNotificationPackets::Build(data, str)" build_at)
string(FIND "${notify}" "sWorld.SendGlobalMessage(&data);" send_at)
if(prefix_at EQUAL -1 OR args_at LESS_EQUAL prefix_at OR
        build_at LESS_EQUAL args_at OR send_at LESS_EQUAL build_at)
    message(FATAL_ERROR "notify order: prefix, args, validate/build, fan-out")
endif()

file(GLOB_RECURSE game_sources LIST_DIRECTORIES false
    "${SOURCE_ROOT}/src/game/*.cpp" "${SOURCE_ROOT}/src/game/*.h")
set(production_sources "")
foreach(path IN LISTS game_sources)
    if(path MATCHES "[/\\\\]tests[/\\\\]" OR path STREQUAL notification_path)
        continue()
    endif()
    file(READ "${path}" source)
    string(APPEND production_sources "\n${source}")
endforeach()
string(REGEX MATCHALL
    "WorldPacket[ \t\r\n]+[A-Za-z_][A-Za-z0-9_]*[ \t\r\n]*[(]SMSG_NOTIFICATION"
    direct_constructors "${production_sources}")
list(LENGTH direct_constructors direct_constructor_count)
if(NOT direct_constructor_count EQUAL 0)
    message(FATAL_ERROR
        "notification constructor inventory: expected zero outside helper, found ${direct_constructor_count}")
endif()
string(REGEX MATCHALL "Initialize[ \t\r\n]*[(]SMSG_NOTIFICATION"
    direct_initializers "${production_sources}")
list(LENGTH direct_initializers direct_initializer_count)
if(NOT direct_initializer_count EQUAL 0)
    message(FATAL_ERROR
        "notification initializer inventory: expected zero outside helper, found ${direct_initializer_count}")
endif()

require_count("${opcode_registry}"
    "DefS(SMSG_NOTIFICATION, \"SMSG_NOTIFICATION\");" 1
    "notification opcode registration")
require_count("${world_session}" "case SMSG_NOTIFICATION:" 1
    "notification suppression allowlist")
require_count("${opcode_header}"
    "SMSG_NOTIFICATION                            = 0x0C2A" 1
    "binary-proven notification opcode")
require_count("${opcode_reference}"
    "SMSG_NOTIFICATION                              0x0C2A  ACTIVE" 1
    "active notification reference row")
require_count("${area_trigger}" "GetSession()->SendNotification(" 2
    "area-trigger feedback route")
require_count("${communication}" "rPlayerSession->SendNotification(" 2
    "administrator message route")
require_count("${session_header}" "SendNotification(format, args...);" 1
    "Eluna compatibility forwarding facade")

string(FIND "${communication}" "bool ChatHandler::HandleAnnounceCommand" announce_begin)
string(FIND "${communication}" "bool ChatHandler::HandleNotifyCommand" announce_end)
math(EXPR announce_length "${announce_end} - ${announce_begin}")
string(SUBSTRING "${communication}" ${announce_begin} ${announce_length} announce)
require_count("${announce}" "sWorld.SendWorldText(LANG_SYSTEMMESSAGE, args);" 1
    "announce chat route")
forbid("${announce}" "MopNotificationPackets" "announce notification route")

string(FIND "${world}" "void World::SendServerMessage" server_message_begin)
string(FIND "${world}" "void World::SendZoneUnderAttackMessage" server_message_end)
if(server_message_begin EQUAL -1 OR server_message_end LESS_EQUAL server_message_begin)
    message(FATAL_ERROR "cannot isolate World::SendServerMessage")
endif()
math(EXPR server_message_length "${server_message_end} - ${server_message_begin}")
string(SUBSTRING "${world}" ${server_message_begin} ${server_message_length} server_message)
require_count("${server_message}" "WorldPacket data(SMSG_SERVER_MESSAGE, 50);" 1
    "server-message opcode separation")
require_count("${server_message}" "data << uint32(type);" 1
    "server-message type field")
require_count("${server_message}" "data << text;" 1
    "server-message NUL string")
forbid("${server_message}" "MopNotificationPackets" "server-message notification route")

set(production "${notification}${production_sources}")
forbid("${world_session}" "WorldSession::SendAreaTriggerMessage"
    "retired area-trigger message backend")
forbid("${opcode_header}" "SMSG_AREA_TRIGGER_MESSAGE"
    "retired area-trigger message opcode")
foreach(token IN ITEMS "WorldSession::SendAreaTriggerMessage" "SMSG_AREA_TRIGGER_MESSAGE")
    forbid("${production}" "${token}" "retired area-trigger message endpoint")
endforeach()
