file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/movement/MovementInfo.cpp" movement_codec)
file(READ "${SOURCE_ROOT}/src/game/movement/MovementStructures.h" movement_structures)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MovementHandler.cpp" movement_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

set(original_sources "${unit_header}${movement_codec}${movement_structures}${misc_handler}${movement_handler}${opcode_registry}${opcode_reference}")

if(MUTATION STREQUAL "root_prefix_order")
    string(REPLACE "MSEPositionX,\n    MSEMovementCounter,\n    MSEPositionY" "MSEMovementCounter,\n    MSEPositionX,\n    MSEPositionY" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "unroot_prefix_order")
    string(REPLACE "MSEPositionX,\n    MSEPositionY,\n    MSEMovementCounter" "MSEPositionY,\n    MSEPositionX,\n    MSEMovementCounter" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "root_bit_order")
    string(REPLACE "MSEUnknownBit149,\n    MSEHasTimestamp" "MSEHasTimestamp,\n    MSEUnknownBit149" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "unroot_bit_order")
    string(REPLACE "MSEGuidBit0,\n    MSEHasPitch" "MSEHasPitch,\n    MSEGuidBit0" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "root_guid_byte_order")
    string(REPLACE "MSEGuidByte1,\n    MSEGuidByte0,\n    MSEGuidByte5" "MSEGuidByte0,\n    MSEGuidByte1,\n    MSEGuidByte5" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "unroot_guid_byte_order")
    string(REPLACE "MSEGuidByte1,\n    MSEGuidByte0,\n    MSEMovementForceIds" "MSEGuidByte0,\n    MSEGuidByte1,\n    MSEMovementForceIds" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "root_transport_order")
    string(REPLACE "MSETransportGuidByte5,\n    MSETransportGuidByte0" "MSETransportGuidByte0,\n    MSETransportGuidByte5" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "unroot_transport_order")
    string(REPLACE "MSETransportSeat,\n    MSETransportGuidByte7" "MSETransportGuidByte7,\n    MSETransportSeat" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "root_fall_order")
    string(REPLACE "MSEFallCosAngle,\n    MSEFallSinAngle" "MSEFallSinAngle,\n    MSEFallCosAngle" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "unroot_fall_order")
    string(REPLACE "MSEFallCosAngle,\n    MSEFallHorizontalSpeed" "MSEFallHorizontalSpeed,\n    MSEFallCosAngle" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "root_flags_order")
    string(REPLACE "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2_13" "MSEFlags2_13,\n    MSEHasFallDirection,\n    MSEFlags" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "unroot_flags_order")
    string(REPLACE "MSEFlags2_13,\n    MSEHasFallDirection,\n    MSEFlags" "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2_13" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "flags2_13_width")
    string(REPLACE "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2_13" "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "root_sequence_route")
    string(REPLACE "case CMSG_FORCE_MOVE_ROOT_ACK:\n            return MovementForceMoveRootAckSequence;" "case CMSG_FORCE_MOVE_ROOT_ACK:\n            return MovementForceMoveUnrootAckSequence;" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "unroot_sequence_route")
    string(REPLACE "case CMSG_FORCE_MOVE_UNROOT_ACK:\n            return MovementForceMoveUnrootAckSequence;" "case CMSG_FORCE_MOVE_UNROOT_ACK:\n            return MovementForceMoveRootAckSequence;" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "root_registration")
    string(REPLACE "DefC(CMSG_FORCE_MOVE_ROOT_ACK" "DefC_disabled(CMSG_FORCE_MOVE_ROOT_ACK" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "unroot_registration")
    string(REPLACE "DefC(CMSG_FORCE_MOVE_UNROOT_ACK" "DefC_disabled(CMSG_FORCE_MOVE_UNROOT_ACK" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "root_handler_parser")
    string(REPLACE "void WorldSession::HandleMoveRootAck(WorldPacket& recv_data)\n{\n    MovementInfo movementInfo;\n    recv_data >> movementInfo;" "void WorldSession::HandleMoveRootAck(WorldPacket& recv_data)\n{\n    MovementInfo movementInfo;\n    recv_data.rfinish();" misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "unroot_handler_parser")
    string(REPLACE "void WorldSession::HandleMoveUnRootAck(WorldPacket& recv_data)\n{\n    MovementInfo movementInfo;\n    recv_data >> movementInfo;" "void WorldSession::HandleMoveUnRootAck(WorldPacket& recv_data)\n{\n    MovementInfo movementInfo;\n    recv_data.rfinish();" misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "root_hostile_force_count")
    string(REPLACE "MSEMovementForceCount,\n    MSEHasMovementFlags,\n    MSEGuidBit0" "MSEHasMovementFlags,\n    MSEGuidBit0" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "unroot_hostile_force_count")
    string(REPLACE "MSEGuidBit2,\n    MSEMovementForceCount,\n    MSEHasMovementFlags2" "MSEGuidBit2,\n    MSEHasMovementFlags2" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_prefix_order")
    string(REPLACE "MSEPositionX,\n    MSEPositionY,\n    MSEMovementCounter,\n    MSEPositionZ,\n\n    MSEUnknownBit172" "MSEPositionY,\n    MSEPositionX,\n    MSEMovementCounter,\n    MSEPositionZ,\n\n    MSEUnknownBit172" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_bit_order")
    string(REPLACE "MSEUnknownBit172,\n    MSEGuidBit3" "MSEGuidBit3,\n    MSEUnknownBit172" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_flags_order")
    string(REPLACE "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2_13,\n\n    MSEGuidByte7" "MSEFlags2_13,\n    MSEHasFallDirection,\n    MSEFlags,\n\n    MSEGuidByte7" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_flags2_width")
    string(REPLACE "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2_13,\n\n    MSEGuidByte7" "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2,\n\n    MSEGuidByte7" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_guid_byte_order")
    string(REPLACE "MSEGuidByte7,\n    MSEMovementForceIds,\n    MSEGuidByte0" "MSEGuidByte0,\n    MSEMovementForceIds,\n    MSEGuidByte7" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_transport_order")
    string(REPLACE "MSETransportTime2,\n    MSETransportGuidByte1" "MSETransportGuidByte1,\n    MSETransportTime2" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_transport_position_orientation_order")
    string(REPLACE "MSETransportGuidByte3,\n    MSETransportPositionO,\n    MSETransportGuidByte2,\n    MSETransportSeat,\n    MSETransportTime3,\n    MSETransportGuidByte6,\n    MSETransportGuidByte0,\n    MSETransportPositionZ"
        "MSETransportGuidByte3,\n    MSETransportPositionZ,\n    MSETransportGuidByte2,\n    MSETransportSeat,\n    MSETransportTime3,\n    MSETransportGuidByte6,\n    MSETransportGuidByte0,\n    MSETransportPositionO" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_fall_order")
    string(REPLACE "MSEFallCosAngle,\n    MSEFallSinAngle,\n    MSEFallHorizontalSpeed,\n    MSEFallVerticalSpeed" "MSEFallSinAngle,\n    MSEFallCosAngle,\n    MSEFallHorizontalSpeed,\n    MSEFallVerticalSpeed" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_force_elements")
    string(REPLACE "MSEMovementForceCount,\n    MSEGuidBit5" "MSEGuidBit5" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_sequence_route")
    string(REPLACE "case CMSG_MOVE_WATER_WALK_ACK:\n            return MovementWaterWalkAckSequence;" "case CMSG_MOVE_WATER_WALK_ACK:\n            return MovementForceMoveRootAckSequence;" movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "water_ack_handler_parser")
    string(REPLACE "void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recv_data)\n{\n    MovementInfo movementInfo;\n    recv_data >> movementInfo;" "void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recv_data)\n{\n    MovementInfo movementInfo;\n    recv_data.rfinish();" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "water_ack_mover_validation")
    string(REPLACE "VerifyMovementInfo(movementInfo, movementInfo.GetGuid())" "VerifyMovementInfo(movementInfo)" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "water_ack_forbid_state_mutation")
    string(REPLACE "DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" "GetPlayer()->SetWaterWalk(true);\n    DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "water_ack_registration")
    string(REPLACE "DefC(CMSG_MOVE_WATER_WALK_ACK" "DefC_disabled(CMSG_MOVE_WATER_WALK_ACK" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "water_pair_metadata")
    string(REPLACE "DefS(SMSG_MOVE_WATER_WALK" "DefS_disabled(SMSG_MOVE_WATER_WALK" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "water_reference_atomic")
    string(REPLACE "SMSG_MOVE_WATER_WALK                           0x1F9A  ACTIVE" "SMSG_MOVE_WATER_WALK                           0x1F9A  DORMANT" opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "active_mover_verify")
    string(REPLACE "VerifyMovementInfo(movementInfo, movementInfo.GetGuid())" "VerifyMovementInfo(movementInfo)" misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "exact_tail_consumption")
    string(REPLACE "recv_data.rpos() != recv_data.size()" "false" misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "movement_counter_storage")
    string(REPLACE "data >> movementCounter;" "data.read_skip<uint32>();" movement_codec "${movement_codec}")
elseif(MUTATION STREQUAL "no_relocation")
    string(REPLACE "DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" "HandleMoverRelocation(movementInfo);\n    DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "no_flag_assignment")
    string(REPLACE "DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" "GetPlayer()->m_movementInfo.SetMovementFlags(movementInfo.GetMovementFlags());\n    DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "no_aura_mutation")
    string(REPLACE "DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" "GetPlayer()->RemoveSpellsCausingAura(SPELL_AURA_WATER_WALK);\n    DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "no_observer_broadcast")
    string(REPLACE "DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" "GetPlayer()->SendMessageToSet(&recv_data, true);\n    DEBUG_LOG(\"CMSG_MOVE_WATER_WALK_ACK" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "allocation_before_resize")
    string(REPLACE "if (movementForceCount > (data.size() - data.rpos()) / sizeof(uint32))\n                {\n                    throw ByteBufferException(false, data.rpos(), movementForceCount * sizeof(uint32), data.size());\n                }\n                movementForceIds.resize(movementForceCount);"
        "movementForceIds.resize(movementForceCount);\n                if (movementForceCount > (data.size() - data.rpos()) / sizeof(uint32))\n                {\n                    throw ByteBufferException(false, data.rpos(), movementForceCount * sizeof(uint32), data.size());\n                }" movement_codec "${movement_codec}")
elseif(MUTATION STREQUAL "paired_smsg_metadata")
    string(REPLACE "DefS(SMSG_FORCE_MOVE_ROOT" "DefS_disabled(SMSG_FORCE_MOVE_ROOT" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "reference_triplet_atomic")
    string(REPLACE "CMSG_FORCE_MOVE_ROOT_ACK                       0x107A  ACTIVE" "CMSG_FORCE_MOVE_ROOT_ACK                       0x107A  DORMANT" opcode_reference "${opcode_reference}")
endif()

set(mutated_sources "${unit_header}${movement_codec}${movement_structures}${misc_handler}${movement_handler}${opcode_registry}${opcode_reference}")
if(DEFINED MUTATION AND NOT MUTATION STREQUAL "" AND mutated_sources STREQUAL original_sources)
    message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm")
    return()
endif()

function(require_text source needle label)
    string(FIND "${source}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "forced movement ACK guard failed: ${label}")
    endif()
endfunction()

string(REPLACE "\r" "" movement_structures "${movement_structures}")
string(REPLACE "\r" "" misc_handler "${misc_handler}")
string(REPLACE "\r" "" movement_handler "${movement_handler}")
string(REPLACE "\r" "" movement_codec "${movement_codec}")

foreach(required IN ITEMS
        "MSEPositionX,\n    MSEMovementCounter,\n    MSEPositionY"
        "MSEUnknownBit149,\n    MSEHasTimestamp"
        "MSEMovementForceCount,\n    MSEHasMovementFlags,\n    MSEGuidBit0"
        "MSEGuidByte1,\n    MSEGuidByte0,\n    MSEGuidByte5"
        "MSETransportGuidByte5,\n    MSETransportGuidByte0"
        "MSEFallCosAngle,\n    MSEFallSinAngle"
        "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2_13")
    require_text("${movement_structures}" "${required}" "exact root grammar")
endforeach()
foreach(required IN ITEMS
        "MSEPositionX,\n    MSEPositionY,\n    MSEMovementCounter"
        "MSEGuidBit0,\n    MSEHasPitch"
        "MSEGuidBit2,\n    MSEMovementForceCount,\n    MSEHasMovementFlags2"
        "MSEGuidByte1,\n    MSEGuidByte0,\n    MSEMovementForceIds"
        "MSETransportSeat,\n    MSETransportGuidByte7"
        "MSEFallCosAngle,\n    MSEFallHorizontalSpeed"
        "MSEFlags2_13,\n    MSEHasFallDirection,\n    MSEFlags")
    require_text("${movement_structures}" "${required}" "exact unroot grammar")
endforeach()
foreach(required IN ITEMS
        "MSEPositionX,\n    MSEPositionY,\n    MSEMovementCounter,\n    MSEPositionZ,\n\n    MSEUnknownBit172"
        "MSEUnknownBit172,\n    MSEGuidBit3"
        "MSEMovementForceCount,\n    MSEGuidBit5"
        "MSEGuidByte7,\n    MSEMovementForceIds,\n    MSEGuidByte0"
        "MSETransportTime2,\n    MSETransportGuidByte1"
        "MSETransportGuidByte3,\n    MSETransportPositionO,\n    MSETransportGuidByte2,\n    MSETransportSeat,\n    MSETransportTime3,\n    MSETransportGuidByte6,\n    MSETransportGuidByte0,\n    MSETransportPositionZ"
        "MSEFallCosAngle,\n    MSEFallSinAngle,\n    MSEFallHorizontalSpeed,\n    MSEFallVerticalSpeed"
        "MSEFlags,\n    MSEHasFallDirection,\n    MSEFlags2_13,\n\n    MSEGuidByte7")
    require_text("${movement_structures}" "${required}" "exact water-walk grammar")
endforeach()
foreach(required IN ITEMS
        "case CMSG_FORCE_MOVE_ROOT_ACK:\n            return MovementForceMoveRootAckSequence;"
        "case CMSG_FORCE_MOVE_UNROOT_ACK:\n            return MovementForceMoveUnrootAckSequence;"
        "case CMSG_MOVE_WATER_WALK_ACK:\n            return MovementWaterWalkAckSequence;")
    require_text("${movement_structures}" "${required}" "opcode-specific selector routes")
endforeach()

require_text("${unit_header}" "uint32 GetMovementCounter() const { return movementCounter; }" "movement counter accessor")
require_text("${movement_codec}" "case MSEMovementCounter:\n                data >> movementCounter;" "movement counter storage")
require_text("${movement_codec}" "if (movementForceCount > (data.size() - data.rpos()) / sizeof(uint32))" "force-count bound before allocation")
string(FIND "${movement_codec}" "if (movementForceCount > (data.size() - data.rpos()) / sizeof(uint32))" bound_position)
string(FIND "${movement_codec}" "movementForceIds.resize(movementForceCount);" resize_position)
if(bound_position GREATER resize_position)
    message(FATAL_ERROR "forced movement ACK guard failed: allocation before resize")
endif()

string(REGEX MATCH "void WorldSession::HandleMoveUnRootAck[(].*void WorldSession::HandleMoveRootAck" unroot_body "${misc_handler}")
string(REGEX MATCH "void WorldSession::HandleMoveRootAck[(].*void WorldSession::HandleSetActionBarTogglesOpcode" root_body "${misc_handler}")
string(REGEX MATCH "void WorldSession::HandleMoveWaterWalkAck[(].*void WorldSession::HandleSummonResponseOpcode" water_body "${movement_handler}")
foreach(handler_body IN ITEMS "${root_body}" "${unroot_body}" "${water_body}")
    foreach(required IN ITEMS "recv_data >> movementInfo;" "recv_data.rpos() != recv_data.size()" "VerifyMovementInfo(movementInfo, movementInfo.GetGuid())" "movementInfo.GetMovementCounter()")
        require_text("${handler_body}" "${required}" "parse, exact-tail and active-mover validation")
    endforeach()
    foreach(forbidden IN ITEMS "HandleMoverRelocation" "SetMovementFlags" "SetWaterWalk" "RemoveSpellsCausingAura" "SendMessageToSet")
        string(FIND "${handler_body}" "${forbidden}" forbidden_position)
        if(NOT forbidden_position EQUAL -1)
            message(FATAL_ERROR "forced movement ACK guard failed: forbidden handler mutation ${forbidden}")
        endif()
    endforeach()
endforeach()
require_text("${movement_handler}" "guid != _player->GetMover()->GetObjectGuid()"
    "active mover accepts player or controlled vehicle and rejects foreign GUID")
require_text("${movement_handler}"
    "MaNGOS::IsValidMapCoord(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o)"
    "invalid world coordinates rejected")
require_text("${movement_handler}"
    "movementInfo.GetTransportPos()->x > 50 || movementInfo.GetTransportPos()->y > 50 || movementInfo.GetTransportPos()->z > 100"
    "invalid transport offsets rejected")

foreach(registration IN ITEMS
        "DefC(CMSG_FORCE_MOVE_ROOT_ACK, \"CMSG_FORCE_MOVE_ROOT_ACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMoveRootAck);"
        "DefC(CMSG_FORCE_MOVE_UNROOT_ACK, \"CMSG_FORCE_MOVE_UNROOT_ACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMoveUnRootAck);"
        "DefC(CMSG_MOVE_WATER_WALK_ACK, \"CMSG_MOVE_WATER_WALK_ACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMoveWaterWalkAck);"
        "DefS(SMSG_FORCE_MOVE_ROOT, \"SMSG_FORCE_MOVE_ROOT\");"
        "DefS(SMSG_FORCE_MOVE_UNROOT, \"SMSG_FORCE_MOVE_UNROOT\");"
        "DefS(SMSG_MOVE_WATER_WALK, \"SMSG_MOVE_WATER_WALK\");"
        "DefS(SMSG_MOVE_LAND_WALK, \"SMSG_MOVE_LAND_WALK\");")
    require_text("${opcode_registry}" "${registration}" "atomic CMSG/SMSG metadata")
endforeach()

foreach(active_row IN ITEMS
        "SMSG_MOVE_LAND_WALK                            0x086A  ACTIVE"
        "SMSG_FORCE_MOVE_ROOT                           0x15AE  ACTIVE"
        "SMSG_MOVE_WATER_WALK                           0x1F9A  ACTIVE"
        "SMSG_FORCE_MOVE_UNROOT                         0x1FAE  ACTIVE"
        "CMSG_FORCE_MOVE_UNROOT_ACK                     0x1051  ACTIVE"
        "CMSG_FORCE_MOVE_ROOT_ACK                       0x107A  ACTIVE"
        "CMSG_MOVE_WATER_WALK_ACK                       0x10F2  ACTIVE")
    require_text("${opcode_reference}" "${active_row}" "seven ACTIVE reference rows")
endforeach()
require_text("${opcode_reference}" "STATUS TOTALS: ACTIVE=528, DOC=434, DORMANT=558" "reference global totals")
require_text("${opcode_reference}" "SMSG: ACTIVE=316, DOC=271, DORMANT=338" "reference SMSG totals")
require_text("${opcode_reference}" "CMSG: ACTIVE=212, DOC=163, DORMANT=220" "reference CMSG totals")

message(STATUS "mop_forced_movement_ack_source: exact readers and parse-only handlers are atomic")
