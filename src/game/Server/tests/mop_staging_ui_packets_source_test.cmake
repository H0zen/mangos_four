file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Calendar.h" calendar_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/NPCHandler.cpp" npc_sender)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CalendarHandler.cpp" calendar_sender)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/World.cpp" world_sender)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerMail.cpp" mail_sender)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GMTicketHandler.cpp" ticket_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

string(CONCAT original_sources
    "${unit_header}" "${calendar_header}" "${npc_sender}" "${calendar_sender}"
    "${world_sender}" "${mail_sender}" "${ticket_sender}" "${world_session}"
    "${opcode_registry}" "${opcode_reference}")

if(MUTATION STREQUAL "bank_mask")
    string(REPLACE
        "out.WriteGuidMask<2, 4, 3, 6, 5, 1, 7, 0>(guid);"
        "out.WriteGuidMask<4, 2, 3, 6, 5, 1, 7, 0>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "bank_bytes")
    string(REPLACE
        "out.WriteGuidBytes<7, 0, 5, 3, 6, 1, 4, 2>(guid);"
        "out.WriteGuidBytes<0, 7, 5, 3, 6, 1, 4, 2>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "bank_sender")
    string(REPLACE
        "MopCompactPackets::BuildShowBank(data, guid);"
        "data << ObjectGuid(guid);"
        npc_sender "${npc_sender}")
elseif(MUTATION STREQUAL "mailbox_body")
    string(REPLACE
        "WorldPacket data(SMSG_SHOW_MAILBOX, 8);"
        "WorldPacket data(SMSG_SHOW_MAILBOX, 4);"
        npc_sender "${npc_sender}")
elseif(MUTATION STREQUAL "server_message_body")
    string(REPLACE
        "data << uint32(type);"
        "data << uint16(type);"
        world_sender "${world_sender}")
elseif(MUTATION STREQUAL "received_mail_body")
    string(REPLACE
        "data << float(0.0f);"
        "data << uint32(0);"
        mail_sender "${mail_sender}")
elseif(MUTATION STREQUAL "ticket_body")
    string(REPLACE
        "data << uint32(statusCode);"
        "data << uint16(statusCode);"
        ticket_sender "${ticket_sender}")
elseif(MUTATION STREQUAL "calendar_scalar_order")
    string(REPLACE
        "out << difficulty;\n        out << mapId;"
        "out << mapId;\n        out << difficulty;"
        calendar_header "${calendar_header}")
elseif(MUTATION STREQUAL "calendar_reset_field")
    string(REPLACE
        "out << mapId;\n        WriteGuidMask"
        "out << mapId;\n        out << uint32(0);\n        WriteGuidMask"
        calendar_header "${calendar_header}")
elseif(MUTATION STREQUAL "calendar_mask")
    string(REPLACE
        "{ 2, 0, 4, 6, 5, 7, 3, 1 }"
        "{ 0, 2, 4, 6, 5, 7, 3, 1 }"
        calendar_header "${calendar_header}")
elseif(MUTATION STREQUAL "calendar_bytes")
    string(REPLACE
        "{ 6, 1, 7, 3, 4, 5, 0, 2 }"
        "{ 1, 6, 7, 3, 4, 5, 0, 2 }"
        calendar_header "${calendar_header}")
elseif(MUTATION STREQUAL "calendar_instance_guid")
    string(REPLACE
        "save->GetInstanceGuid()"
        "save->GetInstanceId()"
        calendar_sender "${calendar_sender}")
elseif(MUTATION STREQUAL "calendar_sender")
    string(REPLACE
        "MopCalendarPackets::BuildCalendarRaidLockoutRemoved(data,"
        "data << uint32(save->GetMapId()); /* removed 18414 builder */"
        calendar_sender "${calendar_sender}")
elseif(MUTATION STREQUAL "staging_gate")
    string(REPLACE
        "case SMSG_SHOW_BANK:"
        "case 0xFFFF: /* removed staging gate */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "staging_registration")
    string(REPLACE
        "DefS(SMSG_SHOW_BANK, \"SMSG_SHOW_BANK\");"
        "/* removed staging metadata */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "staging_reference")
    string(REPLACE
        "SMSG_SHOW_BANK                                 0x0007  ACTIVE   [low-conf]"
        "SMSG_SHOW_BANK                                 0x0007  DORMANT  [low-conf]"
        opcode_reference "${opcode_reference}")
endif()

if(MUTATION)
    string(CONCAT mutated_sources
        "${unit_header}" "${calendar_header}" "${npc_sender}" "${calendar_sender}"
        "${world_sender}" "${mail_sender}" "${ticket_sender}" "${world_session}"
        "${opcode_registry}" "${opcode_reference}")
    if(mutated_sources STREQUAL original_sources)
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()

function(require_once source token context)
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
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

function(require_absent source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden active occurrence remains")
    endif()
endfunction()

require_once("${unit_header}"
    "out.WriteGuidMask<2, 4, 3, 6, 5, 1, 7, 0>(guid);"
    "SHOW_BANK mask order")
require_once("${unit_header}"
    "out.WriteGuidBytes<7, 0, 5, 3, 6, 1, 4, 2>(guid);"
    "SHOW_BANK byte order")
require_once("${npc_sender}"
    "MopCompactPackets::BuildShowBank(data, guid);"
    "SHOW_BANK builder route")
require_once("${npc_sender}"
    "WorldPacket data(SMSG_SHOW_MAILBOX, 8);\n    data << ObjectGuid(guid);"
    "SHOW_MAILBOX flat uint64 body")
require_absent("${npc_sender}"
    "WorldPacket data(SMSG_SHOW_BANK, 8);\n    data << ObjectGuid(guid);"
    "legacy flat SHOW_BANK body")
require_once("${world_sender}"
    "WorldPacket data(SMSG_SERVER_MESSAGE, 50);"
    "SERVER_MESSAGE body")
require_once("${world_sender}"
    "data << uint32(type);"
    "SERVER_MESSAGE type width")
require_once("${world_sender}"
    "data << text;"
    "SERVER_MESSAGE terminated text")
require_once("${mail_sender}"
    "WorldPacket data(SMSG_RECEIVED_MAIL, 4);"
    "RECEIVED_MAIL body size")
require_once("${mail_sender}"
    "data << float(0.0f);"
    "RECEIVED_MAIL float body")
require_once("${ticket_sender}"
    "WorldPacket data(SMSG_GM_TICKET_STATUS_UPDATE, 4);"
    "GM ticket status body size")
require_once("${ticket_sender}"
    "data << uint32(statusCode);"
    "GM ticket status uint32 body")

require_once("${calendar_header}"
    "out << difficulty;\n        out << mapId;\n        WriteGuidMask"
    "calendar difficulty/map order with no reset field")
require_once("${calendar_header}"
    "{ 2, 0, 4, 6, 5, 7, 3, 1 }"
    "calendar instance GUID mask order")
require_once("${calendar_header}"
    "{ 6, 1, 7, 3, 4, 5, 0, 2 }"
    "calendar instance GUID byte order")
require_once("${calendar_sender}"
    "MopCalendarPackets::BuildCalendarRaidLockoutRemoved(data,"
    "calendar lockout removal builder route")
require_once("${calendar_sender}"
    "save->GetInstanceGuid()"
    "calendar HIGHGUID_INSTANCE accessor")

set(staging_opcodes
    SMSG_SHOW_BANK
    SMSG_SHOW_MAILBOX
    SMSG_SERVER_MESSAGE
    SMSG_RECEIVED_MAIL
    SMSG_GM_TICKET_STATUS_UPDATE
    SMSG_CALENDAR_RAID_LOCKOUT_REMOVED)
foreach(opcode IN LISTS staging_opcodes)
    require_once("${world_session}" "case ${opcode}:" "${opcode} suppression gate")
    require_once("${opcode_registry}"
        "DefS(${opcode}, \"${opcode}\");"
        "${opcode} logging metadata")
endforeach()

require_once("${opcode_reference}"
    "SMSG_SHOW_BANK                                 0x0007  ACTIVE   [low-conf]"
    "SHOW_BANK active reference with evidence annotation")
require_once("${opcode_reference}"
    "SMSG_SHOW_MAILBOX                              0x1F13  ACTIVE   [unattributed]  dynamic slot 971 installed by 0x9AADC7"
    "SHOW_MAILBOX active reference with evidence annotation")
require_once("${opcode_reference}"
    "SMSG_SERVER_MESSAGE                            0x0302  ACTIVE   [unattributed]  dynamic slot 66 installed by 0xCE2FDA"
    "SERVER_MESSAGE active reference with evidence annotation")
require_once("${opcode_reference}"
    "SMSG_RECEIVED_MAIL                             0x182B  ACTIVE"
    "RECEIVED_MAIL active reference")
require_once("${opcode_reference}"
    "SMSG_GM_TICKET_STATUS_UPDATE                   0x000B  ACTIVE   [medium-conf]"
    "GM ticket status active reference with evidence annotation")
require_once("${opcode_reference}"
    "SMSG_CALENDAR_RAID_LOCKOUT_REMOVED             0x11E0  ACTIVE   [low-conf]"
    "calendar removal active reference with evidence annotation")
require_once("${opcode_reference}"
    "STATUS TOTALS: ACTIVE=499, DOC=435, DORMANT=586"
    "staging reference totals")
require_once("${opcode_reference}"
    "SMSG: ACTIVE=300, DOC=271, DORMANT=354"
    "staging SMSG reference totals")
