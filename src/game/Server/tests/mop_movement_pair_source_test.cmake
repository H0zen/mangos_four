# Movement-state changes are PAIRS, and half a pair is a visible bug.
#
# The mover is told with SMSG_MOVE_*, which carries a movement counter and goes
# to the controlling session alone. Observers are told with SMSG_SPLINE_MOVE_*,
# which carries no counter because an observer has no acknowledgement to make.
#
# Sending only the mover half lifts the GM off the ground on their own screen
# while every other client still draws them walking -- the ".gm fly on does
# nothing" report, once the wire formats were fixed. Sending only the observer
# half is the same bug seen from the other side. Sending the MOVER packet to
# observers, which two senders in this file still do, is a third variant: the
# observers receive an opcode addressed to a mover they are not.
#
# This guard grows one entry per family as each is landed. Families not listed
# here are not yet converted; adding one without its entry is the mistake this
# file exists to catch.

file(READ "${SOURCE_ROOT}/src/game/Object/PlayerMovement.cpp" PLAYER_SOURCE)
file(READ "${SOURCE_ROOT}/src/game/Object/CreatureMovement.cpp" CREATURE_SOURCE)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" SESSION_SOURCE)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" UNIT_HEADER)
file(READ "${SOURCE_ROOT}/src/game/Object/UnitSpeed.cpp" UNIT_SPEED_SOURCE)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" UNIT_SOURCE_EARLY)

if(MUTATION STREQUAL "drop_observer_half")
    string(REGEX REPLACE "SendMessageToSet[(]&spline, false[)];" ""
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "drop_mover_half")
    string(REPLACE "BuildMoveSetCanFlyPacket(&data, enable, NextMovementCounter());" ""
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "observer_half_to_self")
    string(REPLACE "SendMessageToSet(&spline, false);" "SendMessageToSet(&spline, true);"
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "drop_waterwalk_observer")
    string(REGEX REPLACE "BuildSplineMoveSetWaterWalk[(]spline, GetObjectGuid[(][)][)];" ""
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "unadmitted_water_mover")
    string(REGEX REPLACE "case SMSG_MOVE_WATER_WALK:" ""
        SESSION_SOURCE "${SESSION_SOURCE}")
elseif(MUTATION STREQUAL "feather_mover_to_observers")
    string(REPLACE "BuildMoveFeatherFallPacket(&data, enable, NextMovementCounter());
    GetSession()->SendPacket(&data);"
        "BuildMoveFeatherFallPacket(&data, enable, NextMovementCounter());
    SendMessageToSet(&data, true);"
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "unadmitted_feather_mover")
    string(REGEX REPLACE "case SMSG_MOVE_FEATHER_FALL:" ""
        SESSION_SOURCE "${SESSION_SOURCE}")
elseif(MUTATION STREQUAL "constant_counter")
    string(REPLACE "BuildMoveSetCanFlyPacket(&data, enable, NextMovementCounter());"
        "BuildMoveSetCanFlyPacket(&data, enable, 0);"
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "speed_constant_counter")
    string(REPLACE "MopCompactPackets::BuildMoveSetRunSpeed(data, guid.GetRawValue(), NextMovementCounter(), GetSpeed(mtype));"
        "MopCompactPackets::BuildMoveSetRunSpeed(data, guid.GetRawValue(), 0, GetSpeed(mtype));"
        UNIT_SPEED_SOURCE "${UNIT_SPEED_SOURCE}")
elseif(MUTATION STREQUAL "builder_discards_counter")
    string(REPLACE "*data << uint32(value);" "*data << uint32(0);"
        UNIT_SOURCE_EARLY "${UNIT_SOURCE_EARLY}")
elseif(MUTATION STREQUAL "counter_does_not_advance")
    string(REPLACE "uint32 NextMovementCounter() { return ++m_movementCounter; }"
        "uint32 NextMovementCounter() { return m_movementCounter; }"
        UNIT_HEADER "${UNIT_HEADER}")
elseif(MUTATION STREQUAL "unadmitted_spline")
    string(REGEX REPLACE "case SMSG_SPLINE_MOVE_SET_FLYING:" ""
        SESSION_SOURCE "${SESSION_SOURCE}")
endif()

# --- Player::SetCanFly must send both halves --------------------------------

string(FIND "${PLAYER_SOURCE}" "void Player::SetCanFly(bool enable)" CANFLY_START)
if(CANFLY_START EQUAL -1)
    message(FATAL_ERROR "Could not locate Player::SetCanFly")
endif()
string(LENGTH "${PLAYER_SOURCE}" PLAYER_LENGTH)
math(EXPR CANFLY_REMAINING "${PLAYER_LENGTH} - ${CANFLY_START}")
if(CANFLY_REMAINING GREATER 1400)
    set(CANFLY_REMAINING 1400)
endif()
string(SUBSTRING "${PLAYER_SOURCE}" ${CANFLY_START} ${CANFLY_REMAINING} CANFLY_BODY)

string(FIND "${CANFLY_BODY}" "BuildMoveSetCanFlyPacket" CANFLY_MOVER)
if(CANFLY_MOVER EQUAL -1)
    message(FATAL_ERROR "Player::SetCanFly must tell the mover (SMSG_MOVE_[UN]SET_CAN_FLY)")
endif()

string(FIND "${CANFLY_BODY}" "BuildSplineMoveSetFlying" CANFLY_OBS_SET)
string(FIND "${CANFLY_BODY}" "BuildSplineMoveUnsetFlying" CANFLY_OBS_UNSET)
if(CANFLY_OBS_SET EQUAL -1 OR CANFLY_OBS_UNSET EQUAL -1)
    message(FATAL_ERROR
        "Player::SetCanFly must tell observers too (SMSG_SPLINE_MOVE_[UN]SET_FLYING) -- "
        "the mover half alone leaves everyone else drawing the player walking")
endif()

# The observer half must exclude the mover, which already had the counter-bearing
# packet. Player::SendMessageToSet's second argument is 'self'.
string(REGEX MATCH "SendMessageToSet[(]&spline,[ \t]*false[)]" CANFLY_EXCLUDES_SELF "${CANFLY_BODY}")
if(CANFLY_EXCLUDES_SELF STREQUAL "")
    message(FATAL_ERROR
        "Player::SetCanFly must broadcast the observer half with self=false -- "
        "the mover must not receive a second, counter-less state change")
endif()

# --- Player::SetWaterWalk must send both halves -----------------------------
#
# This one runs far beyond .waterwalk: it also fires on death and on ghost login.

string(FIND "${PLAYER_SOURCE}" "void Player::SetWaterWalk(bool enable)" WW_START)
if(WW_START EQUAL -1)
    message(FATAL_ERROR "Could not locate Player::SetWaterWalk")
endif()
math(EXPR WW_REMAINING "${PLAYER_LENGTH} - ${WW_START}")
if(WW_REMAINING GREATER 1400)
    set(WW_REMAINING 1400)
endif()
string(SUBSTRING "${PLAYER_SOURCE}" ${WW_START} ${WW_REMAINING} WW_BODY)

string(FIND "${WW_BODY}" "BuildMoveWaterWalkPacket" WW_MOVER)
if(WW_MOVER EQUAL -1)
    message(FATAL_ERROR "Player::SetWaterWalk must tell the mover")
endif()

string(FIND "${WW_BODY}" "BuildSplineMoveSetWaterWalk" WW_OBS_ON)
string(FIND "${WW_BODY}" "BuildSplineMoveSetLandWalk" WW_OBS_OFF)
if(WW_OBS_ON EQUAL -1 OR WW_OBS_OFF EQUAL -1)
    message(FATAL_ERROR "Player::SetWaterWalk must tell observers too")
endif()

string(REGEX MATCH "SendMessageToSet[(]&spline,[ 	]*false[)]" WW_EXCLUDES_SELF "${WW_BODY}")
if(WW_EXCLUDES_SELF STREQUAL "")
    message(FATAL_ERROR "Player::SetWaterWalk must broadcast the observer half with self=false")
endif()

# The mover builders must be the reader-derived ones, not the inherited orders
# that were wrong in every field while still producing the right length.
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" UNIT_SOURCE)
string(FIND "${UNIT_SOURCE}" "MopCompactPackets::BuildMoveWaterWalk(" WW_BUILDER)
string(FIND "${UNIT_SOURCE}" "MopCompactPackets::BuildMoveLandWalk(" LW_BUILDER)
if(WW_BUILDER EQUAL -1 OR LW_BUILDER EQUAL -1)
    message(FATAL_ERROR "Unit::BuildMoveWaterWalkPacket must use the reader-derived builders")
endif()
string(FIND "${UNIT_SOURCE}" "WriteGuidMask<4, 7, 6, 0, 1, 3, 5, 2>" WW_LEGACY)
if(NOT WW_LEGACY EQUAL -1)
    message(FATAL_ERROR "The inherited water-walk mask order is still present")
endif()

# --- Player::SetFeatherFall must send the right packet to each audience -----
#
# This one was not a missing half. It broadcast the MOVER packet to everyone with
# SendMessageToSet(&data, true), handing observers a counter-bearing opcode they
# cannot acknowledge.

string(FIND "${PLAYER_SOURCE}" "void Player::SetFeatherFall(bool enable)" FF_START)
if(FF_START EQUAL -1)
    message(FATAL_ERROR "Could not locate Player::SetFeatherFall")
endif()
math(EXPR FF_REMAINING "${PLAYER_LENGTH} - ${FF_START}")
if(FF_REMAINING GREATER 1600)
    set(FF_REMAINING 1600)
endif()
string(SUBSTRING "${PLAYER_SOURCE}" ${FF_START} ${FF_REMAINING} FF_BODY)

# Two plain FINDs and an ordering check, rather than a regex spanning the line
# break -- an embedded literal tab/newline in a character class made this file
# fail git diff --check.
string(FIND "${FF_BODY}" "BuildMoveFeatherFallPacket(&data, enable, NextMovementCounter());" FF_MOVER_BUILD)
string(FIND "${FF_BODY}" "GetSession()->SendPacket(&data);" FF_MOVER_SEND)
if(FF_MOVER_BUILD EQUAL -1 OR FF_MOVER_SEND EQUAL -1)
    message(FATAL_ERROR
        "Player::SetFeatherFall must send the mover packet to the session alone")
endif()
if(NOT FF_MOVER_BUILD LESS FF_MOVER_SEND)
    message(FATAL_ERROR "The mover packet must be built before it is sent")
endif()

# ...and it must NOT be broadcast. That was the actual defect: observers were
# handed a counter-bearing opcode addressed to a mover they are not.
string(FIND "${FF_BODY}" "SendMessageToSet(&data" FF_MOVER_BROADCAST)
if(NOT FF_MOVER_BROADCAST EQUAL -1)
    message(FATAL_ERROR
        "Player::SetFeatherFall must not broadcast the MOVER packet to observers")
endif()

string(FIND "${FF_BODY}" "BuildSplineMoveSetFeatherFall" FF_OBS_ON)
string(FIND "${FF_BODY}" "BuildSplineMoveSetNormalFall" FF_OBS_OFF)
if(FF_OBS_ON EQUAL -1 OR FF_OBS_OFF EQUAL -1)
    message(FATAL_ERROR "Player::SetFeatherFall must tell observers with the spline pair")
endif()

string(FIND "${UNIT_SOURCE}" "MopCompactPackets::BuildMoveFeatherFall(" FF_BUILDER)
string(FIND "${UNIT_SOURCE}" "MopCompactPackets::BuildMoveNormalFall(" NF_BUILDER)
if(FF_BUILDER EQUAL -1 OR NF_BUILDER EQUAL -1)
    message(FATAL_ERROR "Unit::BuildMoveFeatherFallPacket must use the reader-derived builders")
endif()

# --- Creature::SetCanFly is the same pair seen from the other side ----------

string(FIND "${CREATURE_SOURCE}" "void Creature::SetCanFly(bool enable)" CR_CANFLY_START)
if(CR_CANFLY_START EQUAL -1)
    message(FATAL_ERROR "Could not locate Creature::SetCanFly")
endif()
string(LENGTH "${CREATURE_SOURCE}" CREATURE_LENGTH)
math(EXPR CR_REMAINING "${CREATURE_LENGTH} - ${CR_CANFLY_START}")
if(CR_REMAINING GREATER 1200)
    set(CR_REMAINING 1200)
endif()
string(SUBSTRING "${CREATURE_SOURCE}" ${CR_CANFLY_START} ${CR_REMAINING} CR_CANFLY_BODY)

string(FIND "${CR_CANFLY_BODY}" "BuildSplineMoveSetFlying" CR_OBS)
if(CR_OBS EQUAL -1)
    message(FATAL_ERROR "Creature::SetCanFly must use the reader-proven spline builder")
endif()

# A creature has no session, so it must also update its own movement flags or the
# server's view diverges from what every client was just told.
string(FIND "${CR_CANFLY_BODY}" "MOVEFLAG_CAN_FLY" CR_FLAG)
if(CR_FLAG EQUAL -1)
    message(FATAL_ERROR "Creature::SetCanFly must update m_movementInfo as well as the wire")
endif()

# --- Both halves must actually reach the wire -------------------------------
#
# Admission is per-opcode. Admitting one half and not the other reproduces the
# original bug exactly, with no visible difference in the sender.

foreach(GATED
        SMSG_MOVE_SET_CAN_FLY
        SMSG_MOVE_UNSET_CAN_FLY
        SMSG_SPLINE_MOVE_SET_FLYING
        SMSG_SPLINE_MOVE_UNSET_FLYING
        SMSG_MOVE_WATER_WALK
        SMSG_MOVE_LAND_WALK
        SMSG_SPLINE_MOVE_SET_WATER_WALK
        SMSG_SPLINE_MOVE_SET_LAND_WALK
        SMSG_MOVE_FEATHER_FALL
        SMSG_MOVE_NORMAL_FALL
        SMSG_SPLINE_MOVE_SET_FEATHER_FALL
        SMSG_SPLINE_MOVE_SET_NORMAL_FALL)
    string(FIND "${SESSION_SOURCE}" "case ${GATED}:" ADMITTED)
    if(ADMITTED EQUAL -1)
        message(FATAL_ERROR
            "${GATED} is not admitted by IsEnterWorldConverted -- it is built correctly "
            "and then dropped, which is how this family failed silently before")
    endif()
endforeach()

message(STATUS "mop_movement_pair_source: can-fly, water-walk and fall send and admit both halves")

# --- The movement counter must be a real sequence, not a constant ------------
#
# Every one of these call sites passed a literal 0. Retail 18414 traffic shows
# the value incrementing per mover and echoed back at offset 4 of the matching
# acknowledgement, so a constant is provably not what the client is sent. Whether
# it REJECTS a repeat is unproven; that is a reason to stop sending one, not a
# reason to assume it is harmless.

string(REGEX MATCHALL "Build(Move|ForceMove)[A-Za-z]*Packet[(]&data, enable, [0-9]+[)]"
    CONSTANT_COUNTERS "${PLAYER_SOURCE}")
if(NOT CONSTANT_COUNTERS STREQUAL "")
    message(FATAL_ERROR
        "movement-state senders must pass NextMovementCounter(), not a constant: ${CONSTANT_COUNTERS}")
endif()

# ...and the source must actually advance.
string(FIND "${UNIT_HEADER}" "uint32 NextMovementCounter() { return ++m_movementCounter; }" COUNTER_ADVANCES)
if(COUNTER_ADVANCES EQUAL -1)
    message(FATAL_ERROR
        "Unit::NextMovementCounter must pre-increment -- it must advance, and it must "
        "not emit 0, which the movement block encodes as absent")
endif()

# The speed family draws from the SAME per-mover series. That is why observed
# retail sequences skip values: a mover's packets of every kind share one
# counter. Nine mover builders here; the nine SPLINE counterparts take no
# counter at all and must not be given one.
string(REGEX MATCHALL "BuildMoveSet[A-Za-z]+[(]data, guid.GetRawValue[(][)], [0-9]+,"
    SPEED_CONSTANT_COUNTERS "${UNIT_SPEED_SOURCE}")
if(NOT SPEED_CONSTANT_COUNTERS STREQUAL "")
    message(FATAL_ERROR
        "speed senders must pass NextMovementCounter(), not a constant: ${SPEED_CONSTANT_COUNTERS}")
endif()

# A sender advancing the counter is worthless if the BUILDER then discards it.
# BuildMoveHoverPacket serialized a literal 0 in both branches while taking a
# value argument, so SetHover burned a sequence number and sent nothing. A
# reviewer caught it; the call-site guard above could not, because it only proved
# the argument was PASSED, never that it reached the wire.
#
# Checked per function, not file-wide: BuildSendPlayVisualPacket legitimately
# writes uint32(0) as an unknown field and sits between two of these builders, so
# a file-wide scan gives a false positive.

foreach(BUILDER
        BuildForceMoveRootPacket
        BuildMoveSetCanFlyPacket
        BuildMoveWaterWalkPacket
        BuildMoveFeatherFallPacket
        BuildMoveHoverPacket
        BuildMoveLevitatePacket)
    set(SIGNATURE "void Unit::${BUILDER}(WorldPacket* data, bool apply, uint32 value)")
    string(FIND "${UNIT_SOURCE_EARLY}" "${SIGNATURE}" BUILDER_START)
    if(BUILDER_START EQUAL -1)
        message(FATAL_ERROR "Could not locate ${SIGNATURE}")
    endif()

    string(LENGTH "${SIGNATURE}" SIGNATURE_LENGTH)
    math(EXPR BUILDER_BODY_START "${BUILDER_START} + ${SIGNATURE_LENGTH}")
    string(LENGTH "${UNIT_SOURCE_EARLY}" UNIT_EARLY_LENGTH)
    math(EXPR BUILDER_ROOM "${UNIT_EARLY_LENGTH} - ${BUILDER_BODY_START}")
    if(BUILDER_ROOM GREATER 1600)
        set(BUILDER_ROOM 1600)
    endif()
    string(SUBSTRING "${UNIT_SOURCE_EARLY}" ${BUILDER_BODY_START} ${BUILDER_ROOM} BUILDER_BODY)

    # Stop at the next function so a neighbour's contents cannot satisfy or fail this one.
    string(FIND "${BUILDER_BODY}" "void Unit::" NEXT_FUNCTION)
    if(NOT NEXT_FUNCTION EQUAL -1)
        string(SUBSTRING "${BUILDER_BODY}" 0 ${NEXT_FUNCTION} BUILDER_BODY)
    endif()

    string(FIND "${BUILDER_BODY}" "uint32(0)" BUILDER_DISCARDS)
    if(NOT BUILDER_DISCARDS EQUAL -1)
        message(FATAL_ERROR
            "${BUILDER} serializes a literal 0 instead of its value argument -- "
            "the caller advances the movement counter and the packet discards it")
    endif()

    string(FIND "${BUILDER_BODY}" "value" BUILDER_USES_VALUE)
    if(BUILDER_USES_VALUE EQUAL -1)
        message(FATAL_ERROR "${BUILDER} ignores its value argument entirely")
    endif()
endforeach()

# Every sender that takes a counter must draw from it.
foreach(SENDER SetRoot SetWaterWalk SetLevitate SetCanFly SetFeatherFall SetHover)
    string(FIND "${PLAYER_SOURCE}" "void Player::${SENDER}(bool enable)" SENDER_START)
    if(SENDER_START EQUAL -1)
        message(FATAL_ERROR "Could not locate Player::${SENDER}")
    endif()
    math(EXPR SENDER_LEN "${PLAYER_LENGTH} - ${SENDER_START}")
    if(SENDER_LEN GREATER 1600)
        set(SENDER_LEN 1600)
    endif()
    string(SUBSTRING "${PLAYER_SOURCE}" ${SENDER_START} ${SENDER_LEN} SENDER_BODY)
    string(FIND "${SENDER_BODY}" "NextMovementCounter()" SENDER_USES_COUNTER)
    if(SENDER_USES_COUNTER EQUAL -1)
        message(FATAL_ERROR "Player::${SENDER} must stamp a movement counter")
    endif()
endforeach()
