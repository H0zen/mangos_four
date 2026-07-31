file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/PetHandler.cpp" pet_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

string(CONCAT original_sources "${unit_header}" "${pet_handler}"
    "${opcode_registry}" "${opcode_header}" "${opcode_reference}")

if(MUTATION STREQUAL "stop_mask_order")
    string(REPLACE
        "in.ReadGuidMask<7, 5, 1, 6, 0, 2, 4, 3>(petGuid);"
        "in.ReadGuidMask<5, 7, 1, 6, 0, 2, 4, 3>(petGuid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "stop_byte_order")
    string(REPLACE
        "in.ReadGuidBytes<2, 5, 0, 4, 1, 7, 6, 3>(petGuid);"
        "in.ReadGuidBytes<5, 2, 0, 4, 1, 7, 6, 3>(petGuid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "stop_exact_length")
    string(REPLACE
        "if (in.size() - in.rpos() != presentByteCount)"
        "if (in.size() - in.rpos() < presentByteCount)"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "stop_canonical_byte")
    string(REPLACE "if (in[index] == 1)" "if (false)"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "stop_empty_guid")
    string(REPLACE
        "if (petGuid.IsEmpty() || in.rpos() != in.size())"
        "if (in.rpos() != in.size())"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "stop_handler_route")
    string(REPLACE
        "MopCompactPackets::ReadPetStopAttack(recv_data, petGuid)"
        "LegacyPetStopAttackReader(recv_data, petGuid)"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_restore_raw_guid")
    string(REPLACE
        "if (!MopCompactPackets::ReadPetStopAttack(recv_data, petGuid))"
        "if ((recv_data >> petGuid, false) || !MopCompactPackets::ReadPetStopAttack(recv_data, petGuid))"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_registration")
    string(REPLACE "DefC(CMSG_PET_STOP_ATTACK,"
        "RemovedC(CMSG_PET_STOP_ATTACK,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "stop_reference")
    string(REPLACE
        "CMSG_PET_STOP_ATTACK                           0x065B  ACTIVE"
        "CMSG_PET_STOP_ATTACK                           0x065B  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "stop_drop_owner")
    string(REPLACE
        "GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()"
        "false"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_drop_slot_membership")
    string(REPLACE
        "(petGuid != GetPlayer()->GetPetGuid() &&\n            petGuid != GetPlayer()->GetCharmGuid()) ||\n        "
        ""
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_bypass_slot_membership")
    string(REPLACE
        "(petGuid != GetPlayer()->GetPetGuid() &&\n            petGuid != GetPlayer()->GetCharmGuid())"
        "false"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_weaken_conjunction")
    string(REPLACE
        "petGuid != GetPlayer()->GetCharmGuid()) ||\n        GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()"
        "petGuid != GetPlayer()->GetCharmGuid()) &&\n        GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_drop_alive")
    string(REPLACE "if (!pet->IsAlive())" "if (false)"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_move_attack_before_owner")
    string(REPLACE
        "    pet->AttackStop();\n}\n\n/**\n * @brief Handles a client request for a pet name query."
        "}\n\n/**\n * @brief Handles a client request for a pet name query."
        pet_handler "${pet_handler}")
    string(REPLACE
        "GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()"
        "pet->AttackStop();\n\n        GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_move_attack_before_membership")
    string(REPLACE
        "    pet->AttackStop();\n}\n\n/**\n * @brief Handles a client request for a pet name query."
        "}\n\n/**\n * @brief Handles a client request for a pet name query."
        pet_handler "${pet_handler}")
    string(REPLACE
        "if ((petGuid != GetPlayer()->GetPetGuid()"
        "pet->AttackStop();\n\n    if ((petGuid != GetPlayer()->GetPetGuid()"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_not_found_error")
    string(REPLACE
        "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=not_found"
        "sLog.outError(\"PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=not_found"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_not_controlled_error")
    string(REPLACE
        "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=not_controlled"
        "sLog.outError(\"PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=not_controlled"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "stop_log_shape")
    string(REPLACE
        "PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=malformed"
        "PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=bad_packet"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "action_not_found_error")
    string(REPLACE
        "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=not_found"
        "sLog.outError(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=not_found"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "action_not_controlled_error")
    string(REPLACE
        "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=not_controlled"
        "sLog.outError(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=not_controlled"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "action_unknown_command_error")
    string(REPLACE
        "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_command"
        "sLog.outError(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_command"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "action_unknown_spell_error")
    string(REPLACE
        "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_spell"
        "sLog.outError(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_spell"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "action_unknown_action_error")
    string(REPLACE
        "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_action"
        "sLog.outError(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_action"
        pet_handler "${pet_handler}")
elseif(MUTATION STREQUAL "action_drop_charminfo_error")
    string(REPLACE
        "sLog.outError(\"WorldSession::HandlePetAction: object (GUID:"
        "DEBUG_LOG(\"WorldSession::HandlePetAction: object (GUID:"
        pet_handler "${pet_handler}")
elseif(MUTATION)
    message(FATAL_ERROR "unknown MUTATION=${MUTATION}")
endif()

if(MUTATION)
    string(CONCAT mutated_sources "${unit_header}" "${pet_handler}"
        "${opcode_registry}" "${opcode_header}" "${opcode_reference}")
    if(mutated_sources STREQUAL original_sources)
        message(STATUS
            "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 "
            "so WILL_FAIL reports it")
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
        message(FATAL_ERROR
            "${context} guard: expected exactly one match, found ${count}")
    endif()
endfunction()

function(require_count source token expected context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL expected)
        message(FATAL_ERROR
            "${context} guard: expected ${expected} matches, found ${count}")
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
    message(FATAL_ERROR "stop reader seam guard")
endif()
math(EXPR reader_length "${reader_end} - ${reader_start}")
string(SUBSTRING "${unit_header}" ${reader_start} ${reader_length} reader)

require_once("${reader}"
    "inline bool ReadPetStopAttack(WorldPacket& in, ObjectGuid& petGuid)"
    "stop reader signature")
require_once("${reader}"
    "in.ReadGuidMask<7, 5, 1, 6, 0, 2, 4, 3>(petGuid);"
    "stop mask order")
require_once("${reader}"
    "in.ReadGuidBytes<2, 5, 0, 4, 1, 7, 6, 3>(petGuid);"
    "stop byte order")
require_once("${reader}"
    "if (in.size() - in.rpos() != presentByteCount)"
    "stop exact length")
require_once("${reader}" "if (in[index] == 1)"
    "stop canonical byte")
require_once("${reader}"
    "if (petGuid.IsEmpty() || in.rpos() != in.size())"
    "stop empty GUID and exact consumption")

string(FIND "${reader}"
    "if (in.size() - in.rpos() != presentByteCount)" exact_length)
string(FIND "${reader}" "if (in[index] == 1)" canonical_byte)
string(FIND "${reader}"
    "in.ReadGuidBytes<2, 5, 0, 4, 1, 7, 6, 3>(petGuid);" read_bytes)
string(FIND "${reader}"
    "if (petGuid.IsEmpty() || in.rpos() != in.size())" final_check)
if(exact_length EQUAL -1 OR canonical_byte LESS_EQUAL exact_length OR
        read_bytes LESS_EQUAL canonical_byte OR final_check LESS_EQUAL read_bytes)
    message(FATAL_ERROR
        "stop reader order guard: length, canonicality, bytes, final check")
endif()

string(FIND "${pet_handler}"
    "void WorldSession::HandlePetAction" action_start)
string(FIND "${pet_handler}"
    "void WorldSession::HandlePetStopAttack" action_end)
if(action_start EQUAL -1 OR action_end LESS_EQUAL action_start)
    message(FATAL_ERROR "pet action handler seam guard")
endif()
math(EXPR action_length "${action_end} - ${action_start}")
string(SUBSTRING "${pet_handler}" ${action_start} ${action_length}
    action_handler)

string(FIND "${pet_handler}"
    "void WorldSession::HandlePetStopAttack" stop_start)
string(FIND "${pet_handler}"
    "void WorldSession::HandlePetNameQueryOpcode" stop_end)
if(stop_start EQUAL -1 OR stop_end LESS_EQUAL stop_start)
    message(FATAL_ERROR "pet stop handler seam guard")
endif()
math(EXPR stop_length "${stop_end} - ${stop_start}")
string(SUBSTRING "${pet_handler}" ${stop_start} ${stop_length}
    stop_handler)

require_once("${stop_handler}"
    "MopCompactPackets::ReadPetStopAttack(recv_data, petGuid)"
    "stop handler route")
if("${stop_handler}" MATCHES "recv_data[\r\n\t ]*>>[\r\n\t ]*petGuid")
    message(FATAL_ERROR "stop raw GUID guard: legacy raw read present")
endif()
require_once("${stop_handler}"
    "(petGuid != GetPlayer()->GetPetGuid() &&\n            petGuid != GetPlayer()->GetCharmGuid())"
    "stop active-slot membership")
require_once("${stop_handler}"
    "GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()"
    "stop ownership")
require_once("${stop_handler}" "if (!pet->IsAlive())" "stop alive")
require_once("${stop_handler}" "pet->AttackStop();" "stop side effect")

string(FIND "${stop_handler}"
    "MopCompactPackets::ReadPetStopAttack(recv_data, petGuid)" reader_call)
string(FIND "${stop_handler}"
    "GetPlayer()->GetMap()->GetUnit(petGuid)" object_lookup)
string(FIND "${stop_handler}"
    "petGuid != GetPlayer()->GetPetGuid()" slot_membership)
string(FIND "${stop_handler}"
    "GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()" owner_check)
string(FIND "${stop_handler}" "if (!pet->IsAlive())" alive_check)
string(FIND "${stop_handler}" "pet->AttackStop();" attack_stop)
if(reader_call EQUAL -1 OR object_lookup LESS_EQUAL reader_call OR
        slot_membership LESS_EQUAL object_lookup OR
        owner_check LESS_EQUAL slot_membership OR alive_check LESS_EQUAL owner_check OR
        attack_stop LESS_EQUAL alive_check)
    message(FATAL_ERROR
        "stop handler order guard: reader, lookup, active slot, owner, alive, AttackStop")
endif()

require_once("${stop_handler}"
    "petGuid != GetPlayer()->GetCharmGuid()) ||\n        GetPlayer()->GetObjectGuid() != pet->GetCharmerOrOwnerGuid()"
    "stop authority conjunction")

require_once("${stop_handler}"
    "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=malformed account=%u player=%s\",\n            GetAccountId(), GetPlayer()->GetGuidStr().c_str());"
    "stop malformed log shape")
require_once("${stop_handler}"
    "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=not_found account=%u player=%s pet=%s\",\n            GetAccountId(), GetPlayer()->GetGuidStr().c_str(),\n            petGuid.GetString().c_str());"
    "stop not-found log shape")
require_once("${stop_handler}"
    "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK reason=not_controlled account=%u player=%s pet=%s\",\n            GetAccountId(), GetPlayer()->GetGuidStr().c_str(),\n            petGuid.GetString().c_str());"
    "stop not-controlled log shape")
require_count("${stop_handler}"
    "DEBUG_LOG[(]\"PET_CONTROL_REJECT opcode=CMSG_PET_STOP_ATTACK"
    3 "stop rejection DEBUG count")
require_count("${stop_handler}" "sLog[.]outError[(]" 0
    "stop unconditional error count")

require_once("${action_handler}"
    "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=not_found account=%u player=%s pet=%s\",\n            GetAccountId(), _player->GetGuidStr().c_str(), petGuid.GetString().c_str());"
    "action not-found log shape")
require_once("${action_handler}"
    "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=not_controlled account=%u player=%s pet=%s\",\n            GetAccountId(), _player->GetGuidStr().c_str(), petGuid.GetString().c_str());"
    "action not-controlled log shape")
require_once("${action_handler}"
    "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_command account=%u player=%s pet=%s action_type=%u action=%u\",\n                            GetAccountId(), _player->GetGuidStr().c_str(), petGuid.GetString().c_str(),\n                            uint32(flag), spellid);"
    "action unknown-command log shape")
require_once("${action_handler}"
    "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_spell account=%u player=%s pet=%s spell=%u\",\n                    GetAccountId(), _player->GetGuidStr().c_str(), petGuid.GetString().c_str(), spellid);"
    "action unknown-spell log shape")
require_once("${action_handler}"
    "DEBUG_LOG(\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION reason=unknown_action account=%u player=%s pet=%s action_type=%u action=%u\",\n                    GetAccountId(), _player->GetGuidStr().c_str(), petGuid.GetString().c_str(),\n                    uint32(flag), spellid);"
    "action unknown-action log shape")
require_count("${action_handler}"
    "DEBUG_LOG[(]\"PET_CONTROL_REJECT opcode=CMSG_PET_ACTION"
    5 "action rejection DEBUG count")
require_count("${action_handler}" "sLog[.]outError[(]" 1
    "action unconditional error count")
require_once("${action_handler}" "doesn't have a charminfo!"
    "action missing-CharmInfo invariant")

require_once("${opcode_header}"
    "CMSG_PET_STOP_ATTACK                         = 0x065B"
    "stop opcode")
require_once("${opcode_registry}"
    "DefC(CMSG_PET_STOP_ATTACK, \"CMSG_PET_STOP_ATTACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandlePetStopAttack)"
    "stop DefC")
require_once("${opcode_reference}"
    "CMSG_PET_STOP_ATTACK                           0x065B  ACTIVE"
    "stop ACTIVE reference")

message(STATUS "mop_pet_stop_attack_source: source checks passed")
