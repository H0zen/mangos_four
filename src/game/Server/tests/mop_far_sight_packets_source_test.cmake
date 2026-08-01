if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/Server/MopFarSightPackets.h" reader)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcodes)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" reference)

set(original_reader "${reader}")
set(original_handler "${handler}")
set(original_registry "${registry}")
set(original_opcodes "${opcodes}")
set(original_reference "${reference}")

if(MUTATION STREQUAL "exact_size")
    string(REPLACE "in.size() - in.rpos() != 1" "in.size() - in.rpos() < 1" reader "${reader}")
elseif(MUTATION STREQUAL "disable_literal")
    string(REPLACE "raw != 0x00" "raw != 0x01" reader "${reader}")
elseif(MUTATION STREQUAL "enable_literal")
    string(REPLACE "raw != 0x80" "raw != 0x81" reader "${reader}")
elseif(MUTATION STREQUAL "staged_output")
    string(REPLACE "bool const parsedEnable = raw == 0x80;" "enable = raw == 0x80;" reader "${reader}")
elseif(MUTATION STREQUAL "handler_reader")
    string(REPLACE "MopFarSightPackets::ReadRequest(recv_data, enable)" "LegacyFarSightReader(recv_data, enable)" handler "${handler}")
elseif(MUTATION STREQUAL "remove_reader_return")
    string(REPLACE
        "if (!MopFarSightPackets::ReadRequest(recv_data, enable))\n        return;"
        "MopFarSightPackets::ReadRequest(recv_data, enable);"
        handler "${handler}")
elseif(MUTATION STREQUAL "handler_parse_order")
    string(REPLACE
        "    bool enable = false;\n    if (!MopFarSightPackets::ReadRequest(recv_data, enable))\n        return;\n\n    DEBUG_LOG(\"WORLD: Received opcode CMSG_FAR_SIGHT\");"
        "    bool enable = false;\n    DEBUG_LOG(\"WORLD: Received opcode CMSG_FAR_SIGHT\");\n\n    if (!MopFarSightPackets::ReadRequest(recv_data, enable))\n        return;"
        handler "${handler}")
elseif(MUTATION STREQUAL "handler_readbit")
    string(REPLACE "MopFarSightPackets::ReadRequest(recv_data, enable)" "(enable = recv_data.ReadBit(), true)" handler "${handler}")
elseif(MUTATION STREQUAL "authority_guid")
    string(REPLACE "GetFarSightGuid()" "GetObjectGuid()" handler "${handler}")
elseif(MUTATION STREQUAL "global_accessor")
    string(REPLACE "_player->GetMap()->GetWorldObject" "sObjectAccessor.GetWorldObject" handler "${handler}")
elseif(MUTATION STREQUAL "disable_polarity")
    string(REPLACE "if (!enable)" "if (enable)" handler "${handler}")
elseif(MUTATION STREQUAL "reset_view")
    string(REPLACE "ResetView(false)" "ResetView(true)" handler "${handler}")
elseif(MUTATION STREQUAL "missing_object")
    string(REPLACE "if (!obj)" "if (false)" handler "${handler}")
elseif(MUTATION STREQUAL "set_view")
    string(REPLACE "SetView(obj, false)" "SetView(obj, true)" handler "${handler}")
elseif(MUTATION STREQUAL "direct_response")
    string(REPLACE "_player->GetCamera().SetView(obj, false);" "SendPacket(NULL);\n    _player->GetCamera().SetView(obj, false);" handler "${handler}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefC(CMSG_FAR_SIGHT, \"CMSG_FAR_SIGHT\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleFarSightOpcode);" "/* removed far-sight registration */" registry "${registry}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE "CMSG_FAR_SIGHT                               = 0x1341" "CMSG_FAR_SIGHT                               = 0x1342" opcodes "${opcodes}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "CMSG_FAR_SIGHT                                 0x1341  ACTIVE" "CMSG_FAR_SIGHT                                 0x1341  DORMANT" reference "${reference}")
elseif(DEFINED MUTATION)
    message(FATAL_ERROR "unknown mutation: ${MUTATION}")
endif()

if(DEFINED MUTATION AND
   reader STREQUAL original_reader AND
   handler STREQUAL original_handler AND
   registry STREQUAL original_registry AND
   opcodes STREQUAL original_opcodes AND
   reference STREQUAL original_reference)
    message(STATUS "dead mutation arm: ${MUTATION}")
    return()
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

function(require_order source before after context)
    string(FIND "${source}" "${before}" before_pos)
    string(FIND "${source}" "${after}" after_pos)
    if(before_pos EQUAL -1 OR after_pos EQUAL -1 OR NOT before_pos LESS after_pos)
        message(FATAL_ERROR "${context}: required order is absent")
    endif()
endfunction()

function(require_text source token context)
    string(FIND "${source}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${context}: required text is absent")
    endif()
endfunction()

string(FIND "${handler}" "void WorldSession::HandleFarSightOpcode" handler_start)
string(FIND "${handler}" "void WorldSession::HandleSetTitleOpcode" handler_end)
if(handler_start EQUAL -1 OR handler_end EQUAL -1 OR NOT handler_start LESS handler_end)
    message(FATAL_ERROR "far-sight handler seam is missing")
endif()
math(EXPR handler_length "${handler_end} - ${handler_start}")
string(SUBSTRING "${handler}" ${handler_start} ${handler_length} far_sight_handler)

require_once("${reader}" "if \\(in\\.size\\(\\) - in\\.rpos\\(\\) != 1\\)" "exact one-byte admission")
require_once("${reader}" "raw != 0x00" "canonical disable literal")
require_once("${reader}" "raw != 0x80" "canonical enable literal")
require_once("${reader}" "bool const parsedEnable = raw == 0x80" "staged parsed output")
require_once("${reader}" "enable = parsedEnable" "validated output commit")
require_once("${far_sight_handler}" "MopFarSightPackets::ReadRequest\\(recv_data, enable\\)" "production reader route")
require_text("${far_sight_handler}"
    "if (!MopFarSightPackets::ReadRequest(recv_data, enable))\n        return;"
    "parser failure early return")
require_once("${far_sight_handler}" "if \\(!enable\\)" "disable polarity")
require_once("${far_sight_handler}" "ResetView\\(false\\)" "disable reset semantics")
require_once("${far_sight_handler}" "_player->GetMap\\(\\)->GetWorldObject\\(_player->GetFarSightGuid\\(\\)\\)" "map-bounded authenticated far-sight authority")
require_once("${far_sight_handler}" "if \\(!obj\\)" "missing object is inert")
require_once("${far_sight_handler}" "SetView\\(obj, false\\)" "enable view semantics")

require_order("${far_sight_handler}" "MopFarSightPackets::ReadRequest(recv_data, enable)" "DEBUG_LOG(\"WORLD: Received opcode CMSG_FAR_SIGHT\")" "parse before logging")
require_order("${far_sight_handler}" "if (!enable)" "GetWorldObject(_player->GetFarSightGuid())" "disable before object resolution")

if(far_sight_handler MATCHES "recv_data\\.ReadBit|SendPacket")
    message(FATAL_ERROR "far-sight handler contains a legacy direct reader or speculative response")
endif()

require_once("${registry}" "DefC\\(CMSG_FAR_SIGHT, \"CMSG_FAR_SIGHT\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleFarSightOpcode\\)" "logged-in world-thread registration")
require_once("${opcodes}" "CMSG_FAR_SIGHT[ ]+= 0x1341" "build-18414 opcode value")
require_once("${reference}" "CMSG_FAR_SIGHT[ ]+0x1341[ ]+ACTIVE" "active reference state")
