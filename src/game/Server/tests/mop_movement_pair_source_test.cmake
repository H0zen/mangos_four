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
        SMSG_SPLINE_MOVE_UNSET_FLYING)
    string(FIND "${SESSION_SOURCE}" "case ${GATED}:" ADMITTED)
    if(ADMITTED EQUAL -1)
        message(FATAL_ERROR
            "${GATED} is not admitted by IsEnterWorldConverted -- it is built correctly "
            "and then dropped, which is how this family failed silently before")
    endif()
endforeach()

message(STATUS "mop_movement_pair_source: can-fly sends and admits both halves")
