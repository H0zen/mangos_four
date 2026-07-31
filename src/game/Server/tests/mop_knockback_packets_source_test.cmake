file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MovementHandler.cpp" movement_handler)
file(READ "${SOURCE_ROOT}/src/game/movement/MovementStructures.h" movement_structures)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

set(original_sources "${unit_header}${movement_handler}${movement_structures}${opcode_registry}${opcode_header}${world_session}${opcode_reference}")

if(MUTATION STREQUAL "direct_scalar_order")
    string(REPLACE "out << horizontalSpeed << directionY << -verticalSpeed << counter"
        "out << directionY << horizontalSpeed << -verticalSpeed << counter"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "direct_vertical_sign")
    string(REPLACE "directionY << -verticalSpeed << counter"
        "directionY << verticalSpeed << counter" unit_header "${unit_header}")
elseif(MUTATION STREQUAL "direct_mask_order")
    string(REPLACE "WriteGuidMask<2, 0, 7, 1, 4, 6, 5, 3>"
        "WriteGuidMask<0, 2, 7, 1, 4, 6, 5, 3>" unit_header "${unit_header}")
elseif(MUTATION STREQUAL "direct_byte_order")
    string(REPLACE "WriteGuidBytes<6, 0, 7, 5, 4, 3, 1, 2>"
        "WriteGuidBytes<0, 6, 7, 5, 4, 3, 1, 2>" unit_header "${unit_header}")
elseif(MUTATION STREQUAL "direct_builder_bypass")
    string(REPLACE "MopCompactPackets::BuildMoveKnockBack(data, guid.GetRawValue(), counter,"
        "MopCompactPackets::BuildMoveKnockBackRemoved(data, guid.GetRawValue(), counter,"
        movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "direct_producer_mapping")
    string(REPLACE "horizontalSpeed, verticalSpeed, vcos, vsin);"
        "horizontalSpeed, verticalSpeed, vsin, vcos);" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "direct_route")
    string(REPLACE "        horizontalSpeed, verticalSpeed, vcos, vsin);
    SendPacket(&data);"
        "        horizontalSpeed, verticalSpeed, vcos, vsin);
    GetPlayer()->SendMessageToSet(&data, true);" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "update_sequence")
    string(REPLACE "MovementStatusElements MoveUpdateKnockBackSequence[] =
{
    MSEGuidBit5,
    MSEHasSplineElevation,"
        "MovementStatusElements MoveUpdateKnockBackSequence[] =
{
    MSEHasSplineElevation,
    MSEGuidBit5," movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "update_drop_force_count")
    string(REPLACE "    MSEMovementForceCount,
    MSEHasMovementFlags2,"
        "    MSEHasMovementFlags2," movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "update_drop_force_ids")
    string(REPLACE "    MSEPositionO,
    MSEMovementForceIds,
    MSEGuidByte7,"
        "    MSEPositionO,
    MSEGuidByte7," movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "update_flags2_width")
    string(REPLACE "    MSEFlags2_13,
    MSEHasMovementFlags,"
        "    MSEFlags2,
    MSEHasMovementFlags," movement_structures "${movement_structures}")
elseif(MUTATION STREQUAL "registration_direct")
    string(REPLACE "DefS(SMSG_MOVE_KNOCK_BACK, \"SMSG_MOVE_KNOCK_BACK\");"
        "DefS_disabled(SMSG_MOVE_KNOCK_BACK, \"SMSG_MOVE_KNOCK_BACK\");"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "registration_ack")
    string(REPLACE "DefC(CMSG_MOVE_KNOCK_BACK_ACK, \"CMSG_MOVE_KNOCK_BACK_ACK\""
        "DefC_disabled(CMSG_MOVE_KNOCK_BACK_ACK, \"CMSG_MOVE_KNOCK_BACK_ACK\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "registration_update")
    string(REPLACE "DefS(SMSG_MOVE_UPDATE_KNOCK_BACK, \"SMSG_MOVE_UPDATE_KNOCK_BACK\");"
        "DefS_disabled(SMSG_MOVE_UPDATE_KNOCK_BACK, \"SMSG_MOVE_UPDATE_KNOCK_BACK\");"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "admission_direct")
    string(REPLACE "case SMSG_MOVE_KNOCK_BACK:" "case SMSG_MOVE_KNOCK_BACK_REMOVED:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "admission_update")
    string(REPLACE "case SMSG_MOVE_UPDATE_KNOCK_BACK:" "case SMSG_MOVE_UPDATE_KNOCK_BACK_REMOVED:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "reference_atomic")
    string(REPLACE "SMSG_MOVE_UPDATE_KNOCK_BACK                    0x0251  ACTIVE"
        "SMSG_MOVE_UPDATE_KNOCK_BACK                    0x0251  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "observer_route")
    string(REPLACE "mover->SendMessageToSetExcept(&data, _player);"
        "mover->SendMessageToSet(&data, true);" movement_handler "${movement_handler}")
elseif(MUTATION STREQUAL "skip_mover_validation")
    string(REPLACE "VerifyMovementInfo(movementInfo, movementInfo.GetGuid())"
        "VerifyMovementInfo(movementInfo)" movement_handler "${movement_handler}")
endif()

set(mutated_sources "${unit_header}${movement_handler}${movement_structures}${opcode_registry}${opcode_header}${world_session}${opcode_reference}")
if(DEFINED MUTATION AND NOT MUTATION STREQUAL "" AND mutated_sources STREQUAL original_sources)
    message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
    return()
endif()

function(require_text source needle label)
    string(FIND "${source}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "knockback guard failed: ${label}")
    endif()
endfunction()

string(REPLACE "\r" "" unit_header "${unit_header}")
string(REPLACE "\r" "" movement_handler "${movement_handler}")
string(REPLACE "\r" "" movement_structures "${movement_structures}")

require_text("${unit_header}"
    "out << horizontalSpeed << directionY << -verticalSpeed << counter
            << directionX;" "direct scalar order and vertical sign")
require_text("${unit_header}" "WriteGuidMask<2, 0, 7, 1, 4, 6, 5, 3>(guid);"
    "direct GUID mask order")
require_text("${unit_header}" "WriteGuidBytes<6, 0, 7, 5, 4, 3, 1, 2>(guid);"
    "direct GUID byte order")
require_text("${unit_header}" "Body-only 18414 SMSG_MOVE_KNOCK_BACK serializer"
    "body-only builder contract")

string(REGEX MATCH "void WorldSession::SendKnockBack[(].*void WorldSession::HandleMoveHoverAck" direct_body "${movement_handler}")
require_text("${direct_body}" "uint32 const counter = GetPlayer()->NextMovementCounter();"
    "advancing direct counter")
require_text("${direct_body}" "MopCompactPackets::BuildMoveKnockBack(data, guid.GetRawValue(), counter,"
    "direct builder delegation")
require_text("${direct_body}" "horizontalSpeed, verticalSpeed, vcos, vsin);"
    "direct direction mapping")
require_text("${direct_body}" "SendPacket(&data);" "owner-only direct route")

set(expected_update [=[MovementStatusElements MoveUpdateKnockBackSequence[] =
{
    MSEGuidBit5,
    MSEHasSplineElevation,
    MSEHasTimestamp,
    MSEMovementForceCount,
    MSEHasMovementFlags2,
    MSEGuidBit2,
    MSEGuidBit4,
    MSEGuidBit6,
    MSEGuidBit1,
    MSEGuidBit0,
    MSEUnknownBit149,
    MSEHasOrientation,
    MSEUnknownBit148,
    MSEHasTransportData,
    MSETransportGuidBit5,
    MSETransportGuidBit2,
    MSETransportGuidBit0,
    MSETransportGuidBit7,
    MSETransportGuidBit1,
    MSETransportGuidBit6,
    MSETransportGuidBit4,
    MSEHasTransportTime2,
    MSETransportGuidBit3,
    MSEHasTransportTime3,
    MSEGuidBit3,
    MSEHasFallData,
    MSEHasUnknownUInt32,
    MSEHasFallDirection,
    MSEGuidBit7,
    MSEUnknownBit172,
    MSEHasPitch,
    MSEFlags2_13,
    MSEHasMovementFlags,
    MSEFlags,
    MSEGuidByte1,
    MSETransportGuidByte5,
    MSETransportTime3,
    MSETransportGuidByte3,
    MSETransportGuidByte1,
    MSETransportGuidByte4,
    MSETransportPositionZ,
    MSETransportGuidByte7,
    MSETransportGuidByte6,
    MSETransportGuidByte2,
    MSETransportPositionY,
    MSETransportGuidByte0,
    MSETransportSeat,
    MSETransportPositionO,
    MSETransportPositionX,
    MSETransportTime2,
    MSETransportTime,
    MSEGuidByte2,
    MSESplineElevation,
    MSEFallSinAngle,
    MSEFallHorizontalSpeed,
    MSEFallCosAngle,
    MSEFallVerticalSpeed,
    MSEFallTime,
    MSEPositionY,
    MSEPositionO,
    MSEMovementForceIds,
    MSEGuidByte7,
    MSEGuidByte6,
    MSEGuidByte4,
    MSEPositionZ,
    MSEUnknownUInt32,
    MSEGuidByte3,
    MSEGuidByte0,
    MSEPositionX,
    MSEPitch,
    MSEGuidByte5,
    MSETimestamp,
    MSEEnd,
};]=])
require_text("${movement_structures}" "${expected_update}" "exact observer update sequence")

require_text("${opcode_header}" "SMSG_MOVE_KNOCK_BACK                         = 0x0562"
    "direct opcode value")
require_text("${opcode_header}" "CMSG_MOVE_KNOCK_BACK_ACK                     = 0x00F2"
    "ack opcode value")
require_text("${opcode_header}" "SMSG_MOVE_UPDATE_KNOCK_BACK                  = 0x0251"
    "observer opcode value")
require_text("${opcode_registry}" "DefS(SMSG_MOVE_KNOCK_BACK, \"SMSG_MOVE_KNOCK_BACK\");"
    "direct registration")
require_text("${opcode_registry}"
    "DefC(CMSG_MOVE_KNOCK_BACK_ACK, \"CMSG_MOVE_KNOCK_BACK_ACK\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleMoveKnockBackAck);"
    "ack registration")
require_text("${opcode_registry}" "DefS(SMSG_MOVE_UPDATE_KNOCK_BACK, \"SMSG_MOVE_UPDATE_KNOCK_BACK\");"
    "observer registration")
require_text("${world_session}" "case SMSG_MOVE_KNOCK_BACK:" "direct admission")
require_text("${world_session}" "case SMSG_MOVE_UPDATE_KNOCK_BACK:" "observer admission")

foreach(active_row IN ITEMS
        "SMSG_MOVE_KNOCK_BACK                           0x0562  ACTIVE"
        "SMSG_MOVE_UPDATE_KNOCK_BACK                    0x0251  ACTIVE"
        "CMSG_MOVE_KNOCK_BACK_ACK                       0x00F2  ACTIVE")
    require_text("${opcode_reference}" "${active_row}" "atomic ACTIVE reference rows")
endforeach()
string(REGEX MATCH "void WorldSession::HandleMoveKnockBackAck[(].*void WorldSession::SendKnockBack" ack_body "${movement_handler}")
foreach(required IN ITEMS
        "recv_data >> movementInfo"
        "VerifyMovementInfo(movementInfo, movementInfo.GetGuid())"
        "HandleMoverRelocation(movementInfo)"
        "WorldPacket data(SMSG_MOVE_UPDATE_KNOCK_BACK, recv_data.size() + 15)"
        "data << movementInfo"
        "mover->SendMessageToSetExcept(&data, _player)")
    require_text("${ack_body}" "${required}" "validated observer relay: ${required}")
endforeach()

message(STATUS "mop_knockback_packets_source: direct, ack and observer triplet is atomic")
