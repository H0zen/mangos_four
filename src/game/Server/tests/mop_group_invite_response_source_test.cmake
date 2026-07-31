if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Group.h" group_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Group.cpp" group_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GroupHandler.cpp" group_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

macro(mutate variable old new context)
    set(before "${${variable}}")
    string(REPLACE "${old}" "${new}" ${variable} "${${variable}}")
    if("${${variable}}" STREQUAL "${before}")
        message(FATAL_ERROR "${context} mutation setup guard: target not found")
    endif()
endmacro()

if(MUTATION STREQUAL "group_invite_response_opcode_value")
    mutate(opcode_header
        "CMSG_GROUP_INVITE_RESPONSE                   = 0x0D61"
        "CMSG_GROUP_INVITE_RESPONSE                   = 0x0D62"
        "opcode value")
elseif(MUTATION STREQUAL "group_invite_response_registration")
    mutate(opcode_registry
        "DefC(CMSG_GROUP_INVITE_RESPONSE,"
        "RemovedC(CMSG_GROUP_INVITE_RESPONSE,"
        "registration")
elseif(MUTATION STREQUAL "group_invite_response_reference")
    mutate(opcode_reference
        "CMSG_GROUP_INVITE_RESPONSE                     0x0D61  ACTIVE"
        "CMSG_GROUP_INVITE_RESPONSE                     0x0D61  DORMANT"
        "reference")
elseif(MUTATION STREQUAL "group_invite_response_skip_marker")
    mutate(group_source "in >> marker;" "marker = 0x7F;" "marker read")
elseif(MUTATION STREQUAL "group_invite_response_marker_value")
    mutate(group_source "marker != 0x7F" "marker != 0x7E" "marker value")
elseif(MUTATION STREQUAL "group_invite_response_swap_bits")
    mutate(group_source
        "parsed.hasRoles = in.ReadBit();\n    parsed.accepted = in.ReadBit();"
        "parsed.accepted = in.ReadBit();\n    parsed.hasRoles = in.ReadBit();"
        "bit order")
elseif(MUTATION STREQUAL "group_invite_response_drop_padding_guard")
    mutate(group_source "if (padding != 0)" "if (false)" "padding guard")
elseif(MUTATION STREQUAL "group_invite_response_unconditional_roles")
    mutate(group_source "if (parsed.hasRoles)\n        in >> parsed.roles;"
        "if (true)\n        in >> parsed.roles;" "conditional roles")
elseif(MUTATION STREQUAL "group_invite_response_roles_endian")
    mutate(group_source "in >> parsed.roles;" "parsed.roles = 0;" "roles endian")
elseif(MUTATION STREQUAL "group_invite_response_drop_exact_tail")
    mutate(group_source "remaining != expectedRemaining"
        "remaining < expectedRemaining" "exact tail")
elseif(MUTATION STREQUAL "group_invite_response_side_effect_before_parse")
    mutate(group_handler
        "MopGroupInvitePackets::Response response;"
        "Group* prematureGroup = GetPlayer()->GetGroupInvite();\n    MopGroupInvitePackets::Response response;"
        "side effect before parse")
elseif(MUTATION STREQUAL "group_invite_response_branch_on_has_roles")
    mutate(group_handler "if (response.accepted)" "if (response.hasRoles)"
        "acceptance authority")
elseif(MUTATION STREQUAL "group_invite_response_decline_group_uaf")
    mutate(group_handler
        "ObjectGuid const leaderGuid = group->GetLeaderGuid();\n\n        // uninvite, group can be deleted\n        GetPlayer()->UninviteFromGroup();\n\n        // remember leader if online\n        Player* leader = sObjectMgr.GetPlayer(leaderGuid);"
        "// uninvite, group can be deleted\n        GetPlayer()->UninviteFromGroup();\n\n        // remember leader if online\n        Player* leader = sObjectMgr.GetPlayer(group->GetLeaderGuid());"
        "decline group lifetime")
elseif(MUTATION STREQUAL "group_invite_response_self_accept_after_remove")
    mutate(group_handler
        "if (group->GetLeaderGuid() == GetPlayer()->GetObjectGuid())\n        {\n            sLog.outError(\"HandleGroupInviteResponseOpcode: %s tried to accept an invite to his own group\",\n                          GetPlayer()->GetGuidStr().c_str());\n            return;\n        }\n\n        // remove from invites only after authority checks\n        group->RemoveInvite(GetPlayer());"
        "// remove from invites before authority checks\n        group->RemoveInvite(GetPlayer());\n\n        if (group->GetLeaderGuid() == GetPlayer()->GetObjectGuid())\n        {\n            sLog.outError(\"HandleGroupInviteResponseOpcode: %s tried to accept an invite to his own group\",\n                          GetPlayer()->GetGuidStr().c_str());\n            return;\n        }"
        "self-accept ordering")
elseif(MUTATION STREQUAL "group_invite_response_bot_logout_skip_invite_cleanup")
    mutate(world_session
        "_player->ReadyCheckComplete();\n        _player->UninviteFromGroup();\n#ifndef ENABLE_PLAYERBOTS"
        "_player->ReadyCheckComplete();\n#ifndef ENABLE_PLAYERBOTS\n        _player->UninviteFromGroup();"
        "playerbot invite cleanup")
elseif(MUTATION STREQUAL "group_invite_response_unexpected_smsg_activation")
    string(APPEND opcode_registry
        "\nDefS(SMSG_GROUP_DECLINE, \"SMSG_GROUP_DECLINE\");\n")
endif()

set(ws "[ \t\r\n]")

function(require_once source pattern context)
    string(REGEX MATCHALL "${pattern}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "${context} guard: expected exactly one match, found ${count}")
    endif()
endfunction()

function(require_text source token context)
    string(FIND "${source}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${context} guard: required text not found")
    endif()
endfunction()

require_once("${opcode_header}"
    "CMSG_GROUP_INVITE_RESPONSE${ws}*=${ws}*0x0D61"
    "opcode value")
require_once("${opcode_registry}"
    "DefC[(]CMSG_GROUP_INVITE_RESPONSE,"
    "registration cardinality")
require_once("${opcode_registry}"
    "DefC[(]CMSG_GROUP_INVITE_RESPONSE,${ws}*\"CMSG_GROUP_INVITE_RESPONSE\",${ws}*STATUS_LOGGEDIN,${ws}*PROCESS_THREADUNSAFE,${ws}*&WorldSession::HandleGroupInviteResponseOpcode[)]"
    "registration binding")
require_once("${opcode_reference}"
    "CMSG_GROUP_INVITE_RESPONSE${ws}+0x0D61${ws}+ACTIVE"
    "reference state")

foreach(required IN ITEMS
        "namespace MopGroupInvitePackets"
        "struct Response"
        "bool hasRoles = false;"
        "bool accepted = false;"
        "uint32 roles = 0;"
        "bool ParseResponse(WorldPacket& in, Response& out);")
    require_text("${group_header}" "${required}" "packet declaration ${required}")
endforeach()

string(FIND "${group_source}" "bool MopGroupInvitePackets::ParseResponse" parser_start)
string(FIND "${group_source}" "Group::Group()" parser_end)
if(parser_start EQUAL -1 OR parser_end LESS_EQUAL parser_start)
    message(FATAL_ERROR "response parser seam guard: could not isolate parser")
endif()
math(EXPR parser_length "${parser_end} - ${parser_start}")
string(SUBSTRING "${group_source}" ${parser_start} ${parser_length} parser)

require_text("${parser}" "in.rpos() != 0" "nonzero initial read position")
require_text("${parser}" "in >> marker;" "marker read")
require_text("${parser}" "marker != 0x7F" "marker value")
require_text("${parser}" "parsed.hasRoles = in.ReadBit();\n    parsed.accepted = in.ReadBit();"
    "roles then accepted bit order")
require_text("${parser}" "uint8 const padding = uint8(in.ReadBits(6));"
    "six padding bits")
require_text("${parser}" "if (padding != 0)" "zero padding")
require_text("${parser}" "in.ResetBitReader();" "bit alignment")
require_text("${parser}" "parsed.hasRoles ? sizeof(uint32) : 0"
    "conditional tail size")
require_text("${parser}" "remaining != expectedRemaining" "exact tail size")
require_text("${parser}" "if (parsed.hasRoles)" "conditional roles branch")
require_text("${parser}" "in >> parsed.roles;" "little-endian roles")
require_text("${parser}" "if (in.rpos() != in.size())" "complete consumption")
require_text("${parser}" "out = parsed;" "atomic output commit")
require_text("${parser}" "in.rfinish();" "failure consumption")

string(FIND "${parser}" "in >> marker;" marker_read)
string(FIND "${parser}" "marker != 0x7F" marker_check)
string(FIND "${parser}" "parsed.hasRoles = in.ReadBit();" roles_bit)
string(FIND "${parser}" "parsed.accepted = in.ReadBit();" accepted_bit)
if(marker_read EQUAL -1 OR marker_check LESS_EQUAL marker_read OR
        roles_bit LESS_EQUAL marker_check OR accepted_bit LESS_EQUAL roles_bit)
    message(FATAL_ERROR "marker validation and response-bit ordering guard failed")
endif()

string(FIND "${group_handler}"
    "void WorldSession::HandleGroupInviteResponseOpcode" handler_start)
string(FIND "${group_handler}"
    "void WorldSession::HandleGroupUninviteGuidOpcode" handler_end)
if(handler_start EQUAL -1 OR handler_end LESS_EQUAL handler_start)
    message(FATAL_ERROR "response handler seam guard: could not isolate handler")
endif()
math(EXPR handler_length "${handler_end} - ${handler_start}")
string(SUBSTRING "${group_handler}" ${handler_start} ${handler_length} handler)

require_once("${handler}"
    "MopGroupInvitePackets::ParseResponse[(]recv_data,${ws}*response[)]"
    "handler parser route")
require_once("${handler}" "if${ws}*[(]response[.]accepted[)]"
    "accepted branch authority")
string(FIND "${handler}" "MopGroupInvitePackets::ParseResponse(recv_data, response)" parse_pos)
string(FIND "${handler}" "GetGroupInvite()" group_lookup_pos)
if(parse_pos EQUAL -1 OR group_lookup_pos LESS_EQUAL parse_pos)
    message(FATAL_ERROR "parse-before-group-state guard failed")
endif()
string(SUBSTRING "${handler}" 0 ${parse_pos} before_parse)
if(before_parse MATCHES "GetPlayer|GetGroupInvite|group->|sObjectMgr|SendPacket|SendPartyResult|Uninvite")
    message(FATAL_ERROR "state effect appears before exact response parsing")
endif()
if(handler MATCHES "response[.]roles[^;]*(Set|Role)" OR
        handler MATCHES "(Set|Role)[^;]*response[.]roles")
    message(FATAL_ERROR "parsed role metadata became gameplay authority")
endif()

require_text("${handler}" "ObjectGuid const leaderGuid = group->GetLeaderGuid();"
    "leader GUID lifetime capture")
require_text("${handler}" "Player* leader = sObjectMgr.GetPlayer(leaderGuid);"
    "copied leader GUID lookup")
string(FIND "${handler}" "ObjectGuid const leaderGuid = group->GetLeaderGuid();" leader_copy_pos)
string(FIND "${handler}" "GetPlayer()->UninviteFromGroup();" uninvite_pos)
if(leader_copy_pos EQUAL -1 OR uninvite_pos LESS_EQUAL leader_copy_pos)
    message(FATAL_ERROR "leader GUID is not captured before destructive uninvite")
endif()
string(SUBSTRING "${handler}" ${uninvite_pos} -1 after_uninvite)
if(after_uninvite MATCHES "group->")
    message(FATAL_ERROR "decline path dereferences group after destructive uninvite")
endif()

string(FIND "${handler}"
    "if (group->GetLeaderGuid() == GetPlayer()->GetObjectGuid())"
    self_accept_check_pos)
string(FIND "${handler}" "group->RemoveInvite(GetPlayer());"
    accepted_remove_pos)
if(self_accept_check_pos EQUAL -1 OR
        accepted_remove_pos LESS_EQUAL self_accept_check_pos)
    message(FATAL_ERROR
        "self-accept authority must precede invite-state mutation")
endif()

require_text("${world_session}"
    "_player->ReadyCheckComplete();\n        _player->UninviteFromGroup();\n#ifndef ENABLE_PLAYERBOTS"
    "playerbot-independent invite cleanup")

foreach(name IN ITEMS SMSG_GROUP_INVITE SMSG_PARTY_COMMAND_RESULT SMSG_GROUP_DECLINE)
    if(opcode_registry MATCHES "DefS[(]${name},")
        message(FATAL_ERROR "unexpected response-wave SMSG registration: ${name}")
    endif()
    if(world_session MATCHES "case${ws}+${name}${ws}*:")
        message(FATAL_ERROR "unexpected response-wave enter-world admission: ${name}")
    endif()
endforeach()
foreach(row IN ITEMS
        "CMSG_GROUP_INVITE                              0x072D  DORMANT"
        "SMSG_GROUP_INVITE                              0x0A8F  DORMANT"
        "SMSG_PARTY_COMMAND_RESULT                      0x0F86  DORMANT"
        "SMSG_GROUP_DECLINE                             0x17A3  DORMANT")
    require_text("${opcode_reference}" "${row}" "ordinary invite flow remains dormant")
endforeach()
