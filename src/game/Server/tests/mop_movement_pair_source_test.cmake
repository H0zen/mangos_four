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

if(MUTATION STREQUAL "drop_observer_half")
    string(REGEX REPLACE "SendMessageToSet[(]&spline, false[)];" ""
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "drop_mover_half")
    string(REPLACE "BuildMoveSetCanFlyPacket(&data, enable, 0);" ""
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
    string(REPLACE "BuildMoveFeatherFallPacket(&data, enable, 0);
    GetSession()->SendPacket(&data);"
        "BuildMoveFeatherFallPacket(&data, enable, 0);
    SendMessageToSet(&data, true);"
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "unadmitted_feather_mover")
    string(REGEX REPLACE "case SMSG_MOVE_FEATHER_FALL:" ""
        SESSION_SOURCE "${SESSION_SOURCE}")
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
string(FIND "${FF_BODY}" "BuildMoveFeatherFallPacket(&data, enable, 0);" FF_MOVER_BUILD)
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
