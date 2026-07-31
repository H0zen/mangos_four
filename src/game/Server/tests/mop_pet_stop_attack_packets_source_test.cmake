file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/PetHandler.cpp" pet_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

if(MUTATION STREQUAL "opcode")
    string(REPLACE
        "CMSG_PET_STOP_ATTACK                         = 0x065B"
        "CMSG_PET_STOP_ATTACK                         = 0x065C"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefC(CMSG_PET_STOP_ATTACK,"
        "RemovedC(CMSG_PET_STOP_ATTACK,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE
        "CMSG_PET_STOP_ATTACK                           0x065B  ACTIVE"
        "CMSG_PET_STOP_ATTACK                           0x065B  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "reader_call")
    string(REPLACE
        "MopCompactPackets::ReadPetStopAttack(recv_data, petGuid)"
        "LegacyPetStopAttackReader(recv_data, petGuid)"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "exact_consumption")
    string(REPLACE
        "if (remaining != 1 + guidByteCount)"
        "if (remaining < 1 + guidByteCount)"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "ownership_check")
    string(REPLACE
        "GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()"
        "GetPlayer()->GetObjectGuid() == pet->GetCharmerOrOwnerGuid()"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "alive_check")
    string(REPLACE
        "if (!pet->IsAlive())"
        "if (pet->IsAlive())"
        pet_handler "${pet_handler}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
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

string(FIND "${unit_header}" "inline bool ReadPetStopAttack" reader_start)
string(FIND "${unit_header}" "inline void ReadPetNameQuery" reader_end)
if(reader_start EQUAL -1 OR reader_end LESS_EQUAL reader_start)
    message(FATAL_ERROR "reader seam guard: could not isolate reader")
endif()
math(EXPR reader_length "${reader_end} - ${reader_start}")
string(SUBSTRING "${unit_header}" ${reader_start} ${reader_length} reader)

require_once("${reader}"
    "ReadPetStopAttack[(]WorldPacket& in, ObjectGuid& petGuid[)]"
    "reader signature")
require_text("${reader}"
    "guid[7] = in.ReadBit();  guid[5] = in.ReadBit();\n        guid[1] = in.ReadBit();  guid[6] = in.ReadBit();\n        guid[0] = in.ReadBit();  guid[2] = in.ReadBit();\n        guid[4] = in.ReadBit();  guid[3] = in.ReadBit();"
    "presence-bit order")
require_text("${reader}"
    "uint8 const byteOrder[] = { 2, 5, 0, 4, 1, 7, 6, 3 };"
    "present-byte order")
require_text("${reader}"
    "in.ReadByteSeq(guid[byteOrder[index]]);"
    "XOR byte decoding")
require_text("${reader}"
    "if (remaining != 1 + guidByteCount)"
    "exact body consumption")
require_text("${reader}" "if (raw == 0)" "nonzero GUID")

string(FIND "${pet_handler}"
    "void WorldSession::HandlePetStopAttack" handler_start)
string(FIND "${pet_handler}"
    "void WorldSession::HandlePetNameQueryOpcode" handler_end)
if(handler_start EQUAL -1 OR handler_end LESS_EQUAL handler_start)
    message(FATAL_ERROR "handler seam guard: could not isolate handler")
endif()
math(EXPR handler_length "${handler_end} - ${handler_start}")
string(SUBSTRING "${pet_handler}" ${handler_start} ${handler_length} handler)

require_once("${handler}"
    "if [(]!MopCompactPackets::ReadPetStopAttack[(]recv_data, petGuid[)][)]"
    "handler reader route")
if("${handler}" MATCHES "recv_data[\r\n\t ]*>>[\r\n\t ]*petGuid")
    message(FATAL_ERROR "handler reader route guard: raw GUID read remains")
endif()
require_once("${handler}"
    "GetPlayer[(][)]->GetObjectGuid[(][)] != pet->GetCharmerOrOwnerGuid[(][)]"
    "ownership check")
require_once("${handler}"
    "if [(][!]pet->IsAlive[(][)][)]"
    "alive check")

string(FIND "${handler}"
    "MopCompactPackets::ReadPetStopAttack(recv_data, petGuid)" reader_call)
string(FIND "${handler}"
    "GetPlayer()->GetMap()->GetUnit(petGuid)" object_lookup)
string(FIND "${handler}"
    "GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()" ownership)
string(FIND "${handler}" "if (!pet->IsAlive())" alive)
string(FIND "${handler}" "pet->AttackStop();" stop_attack)
if(reader_call EQUAL -1 OR object_lookup LESS_EQUAL reader_call OR
        ownership LESS_EQUAL object_lookup OR alive LESS_EQUAL ownership OR
        stop_attack LESS_EQUAL alive)
    message(FATAL_ERROR
        "handler ordering guard: parse, lookup, ownership, alive, stop required")
endif()

require_once("${opcode_header}"
    "CMSG_PET_STOP_ATTACK[\t ]*=[\t ]*0x065B"
    "opcode value")
require_once("${opcode_registry}"
    "DefC[(]CMSG_PET_STOP_ATTACK, \"CMSG_PET_STOP_ATTACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePetStopAttack[)]"
    "DefC registration")
require_once("${opcode_reference}"
    "CMSG_PET_STOP_ATTACK[\t ]+0x065B[\t ]+ACTIVE"
    "ACTIVE reference")

message(STATUS "mop_pet_stop_attack_packets_source: source checks passed")
