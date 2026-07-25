file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)

if(MUTATION STREQUAL "request_reader")
    string(REPLACE "MopInspectPackets::ParseRequest(recv_data, guid)" "false" misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "response_builder")
    string(REPLACE "MopInspectPackets::BuildResponse(data, response)" "false" misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "client_registration")
    string(REPLACE "DefC(CMSG_INSPECT," "DefC(CMSG_UNUSED_INSPECT," opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "server_registration")
    string(REPLACE "DefS(SMSG_INSPECT_RESULTS," "DefS(SMSG_UNUSED_INSPECT_RESULTS," opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "whitelist")
    string(REPLACE "case SMSG_INSPECT_RESULTS:" "case SMSG_UNUSED_INSPECT_RESULTS:" world_session "${world_session}")
elseif(MUTATION STREQUAL "request_value")
    string(REPLACE "CMSG_INSPECT                                 = 0x1259" "CMSG_INSPECT                                 = 0x0000" opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "response_value")
    string(REPLACE "SMSG_INSPECT_RESULTS                         = 0x1842" "SMSG_INSPECT_RESULTS                         = 0x0000" opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "request_reference")
    string(REPLACE "CMSG_INSPECT                                   0x1259  ACTIVE" "CMSG_INSPECT                                   0x1259  DORMANT" opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "response_reference")
    string(REPLACE "SMSG_INSPECT_RESULTS                           0x1842  ACTIVE" "SMSG_UNKNOWN_0x1842                            0x1842  DORMANT" opcode_reference "${opcode_reference}")
endif()

string(FIND "${misc_handler}" "void WorldSession::HandleInspectOpcode" start)
string(FIND "${misc_handler}" "void WorldSession::HandleInspectHonorStatsOpcode" finish)
if(start EQUAL -1 OR finish EQUAL -1 OR NOT start LESS finish)
    message(FATAL_ERROR "could not isolate inspect handler")
endif()
math(EXPR length "${finish} - ${start}")
string(SUBSTRING "${misc_handler}" ${start} ${length} inspect_handler)

foreach(forbidden IN ITEMS
        "recv_data >> guid"
        "BuildPlayerTalentsInfoData"
        "BuildEnchantmentsInfoData"
        "data << plr->GetObjectGuid()")
    string(FIND "${inspect_handler}" "${forbidden}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "legacy inspect body remains: ${forbidden}")
    endif()
endforeach()

foreach(required IN ITEMS
        "MopInspectPackets::ParseRequest(recv_data, guid)"
        "MopInspectPackets::BuildResponse(data, response)"
        "SendPacket(&data)")
    string(FIND "${inspect_handler}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "inspect handler is missing: ${required}")
    endif()
endforeach()

string(FIND "${opcode_registry}"
    "DefC(CMSG_INSPECT, \"CMSG_INSPECT\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleInspectOpcode);"
    client_registration)
if(client_registration EQUAL -1)
    message(FATAL_ERROR "CMSG_INSPECT is not registered")
endif()

string(FIND "${opcode_registry}"
    "DefS(SMSG_INSPECT_RESULTS, \"SMSG_INSPECT_RESULTS\");"
    server_registration)
if(server_registration EQUAL -1)
    message(FATAL_ERROR "SMSG_INSPECT_RESULTS lacks outbound metadata")
endif()

string(FIND "${world_session}" "case SMSG_INSPECT_RESULTS:" whitelist)
if(whitelist EQUAL -1)
    message(FATAL_ERROR "SMSG_INSPECT_RESULTS is not admitted through suppression")
endif()

string(FIND "${opcode_header}" "CMSG_INSPECT                                 = 0x1259" request_value)
string(FIND "${opcode_header}" "SMSG_INSPECT_RESULTS                         = 0x1842" response_value)
if(request_value EQUAL -1 OR response_value EQUAL -1)
    message(FATAL_ERROR "inspect opcode value drifted")
endif()

string(FIND "${opcode_reference}" "CMSG_INSPECT                                   0x1259  ACTIVE" request_reference)
string(FIND "${opcode_reference}" "SMSG_INSPECT_RESULTS                           0x1842  ACTIVE" response_reference)
if(request_reference EQUAL -1 OR response_reference EQUAL -1)
    message(FATAL_ERROR "inspect reference status is stale")
endif()
