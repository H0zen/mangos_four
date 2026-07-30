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
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MovementHandler.cpp" MOVEMENT_HANDLER_SOURCE)

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
elseif(MUTATION STREQUAL "player_setter_skips_server_state")
    string(REPLACE "        m_movementInfo.AddMovementFlag(MOVEFLAG_CAN_FLY);" ""
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "root_broadcasts_mover")
    string(REPLACE "BuildForceMoveRootPacket(&data, enable, NextMovementCounter());
    GetSession()->SendPacket(&data);"
        "BuildForceMoveRootPacket(&data, enable, NextMovementCounter());
    SendMessageToSet(&data, true);"
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "unadmitted_root_observer")
    string(REGEX REPLACE "case SMSG_SPLINE_MOVE_ROOT:" ""
        SESSION_SOURCE "${SESSION_SOURCE}")
elseif(MUTATION STREQUAL "drop_hover_observer")
    string(REGEX REPLACE "MopCompactPackets::BuildSplineMoveSetHover[(]spline, GetObjectGuid[(][)].GetRawValue[(][)][)];" ""
        PLAYER_SOURCE "${PLAYER_SOURCE}")
elseif(MUTATION STREQUAL "unadmitted_hover_mover")
    string(REGEX REPLACE "case SMSG_MOVE_SET_HOVER:" ""
        SESSION_SOURCE "${SESSION_SOURCE}")
elseif(MUTATION STREQUAL "knockback_constant_counter")
    string(REPLACE "data << uint32(GetPlayer()->NextMovementCounter());" "data << uint32(0);"
        MOVEMENT_HANDLER_SOURCE "${MOVEMENT_HANDLER_SOURCE}")
elseif(MUTATION STREQUAL "collision_height_constant_counter")
    string(REPLACE "data << uint32(NextMovementCounter());" "data << uint32(sWorld.GetGameTime());"
        UNIT_SOURCE_EARLY "${UNIT_SOURCE_EARLY}")
elseif(MUTATION STREQUAL "builder_discards_counter")
    string(REPLACE "*data << uint32(value);" "*data << uint32(0);"
        UNIT_SOURCE_EARLY "${UNIT_SOURCE_EARLY}")
elseif(MUTATION STREQUAL "counter_getter_not_advancer")
    string(REPLACE "BuildForceMoveRootPacket(&data, enable, NextMovementCounter());"
        "BuildForceMoveRootPacket(&data, enable, GetMovementCounter());"
        PLAYER_SOURCE "${PLAYER_SOURCE}")
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
if(CANFLY_REMAINING GREATER 3000)
    set(CANFLY_REMAINING 3000)
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
if(WW_REMAINING GREATER 3000)
    set(WW_REMAINING 3000)
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
if(FF_REMAINING GREATER 3000)
    set(FF_REMAINING 3000)
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

# --- Player::SetHover must send both halves ---------------------------------

string(FIND "${PLAYER_SOURCE}" "void Player::SetHover(bool enable)" HOVER_START)
if(HOVER_START EQUAL -1)
    message(FATAL_ERROR "Could not locate Player::SetHover")
endif()
math(EXPR HOVER_REMAINING "${PLAYER_LENGTH} - ${HOVER_START}")
if(HOVER_REMAINING GREATER 3000)
    set(HOVER_REMAINING 3000)
endif()
string(SUBSTRING "${PLAYER_SOURCE}" ${HOVER_START} ${HOVER_REMAINING} HOVER_BODY)

string(FIND "${HOVER_BODY}" "BuildMoveHoverPacket" HOVER_MOVER)
string(FIND "${HOVER_BODY}" "BuildSplineMoveSetHover" HOVER_OBS_ON)
string(FIND "${HOVER_BODY}" "BuildSplineMoveUnsetHover" HOVER_OBS_OFF)
if(HOVER_MOVER EQUAL -1 OR HOVER_OBS_ON EQUAL -1 OR HOVER_OBS_OFF EQUAL -1)
    message(FATAL_ERROR "Player::SetHover must send both the mover and observer halves")
endif()

# The inherited orders decoded none of the 51 real bodies to a plausible GUID.
# BOTH branches, not just SET. SET and UNSET have different mask orders, byte
# orders and scalar positions, so checking one proves nothing about the other --
# a reviewer found this guard accepting a wrong UNSET branch.
string(FIND "${UNIT_SOURCE_EARLY}" "MopCompactPackets::BuildMoveSetHover(" HOVER_BUILDER)
string(FIND "${UNIT_SOURCE_EARLY}" "MopCompactPackets::BuildMoveUnsetHover(" HOVER_BUILDER_OFF)
if(HOVER_BUILDER EQUAL -1 OR HOVER_BUILDER_OFF EQUAL -1)
    message(FATAL_ERROR "Unit::BuildMoveHoverPacket must use the reader-derived builders, both branches")
endif()
string(FIND "${CREATURE_SOURCE}" "MopCompactPackets::BuildSplineMoveSetHover(" CR_HOVER)
string(FIND "${CREATURE_SOURCE}" "MopCompactPackets::BuildSplineMoveUnsetHover(" CR_HOVER_OFF)
if(CR_HOVER EQUAL -1 OR CR_HOVER_OFF EQUAL -1)
    message(FATAL_ERROR "Creature::SetHover must use the reader-derived spline builders, both branches")
endif()

# --- Player::SetRoot must not broadcast the mover packet --------------------
#
# The last sender doing it, and the outlier among its neighbours. Observers were
# handed a counter-bearing opcode expecting an acknowledgement they cannot make,
# while the observer opcode that exists for them was never sent. Death,
# resurrection and vehicle boarding all root through here.

string(FIND "${PLAYER_SOURCE}" "void Player::SetRoot(bool enable)" ROOT_START)
if(ROOT_START EQUAL -1)
    message(FATAL_ERROR "Could not locate Player::SetRoot")
endif()
math(EXPR ROOT_REMAINING "${PLAYER_LENGTH} - ${ROOT_START}")
if(ROOT_REMAINING GREATER 3000)
    set(ROOT_REMAINING 3000)
endif()
string(SUBSTRING "${PLAYER_SOURCE}" ${ROOT_START} ${ROOT_REMAINING} ROOT_BODY)

string(FIND "${ROOT_BODY}" "SendMessageToSet(&data" ROOT_BROADCASTS_MOVER)
if(NOT ROOT_BROADCASTS_MOVER EQUAL -1)
    message(FATAL_ERROR
        "Player::SetRoot must not broadcast the MOVER packet -- observers need "
        "SMSG_SPLINE_MOVE_ROOT, which carries no counter")
endif()

string(FIND "${ROOT_BODY}" "BuildSplineMoveRoot" ROOT_OBS_ON)
string(FIND "${ROOT_BODY}" "BuildSplineMoveUnroot" ROOT_OBS_OFF)
if(ROOT_OBS_ON EQUAL -1 OR ROOT_OBS_OFF EQUAL -1)
    message(FATAL_ERROR "Player::SetRoot must tell observers with the spline pair")
endif()

string(FIND "${UNIT_SOURCE_EARLY}" "MopCompactPackets::BuildForceMoveRoot(" ROOT_BUILDER)
string(FIND "${UNIT_SOURCE_EARLY}" "MopCompactPackets::BuildForceMoveUnroot(" ROOT_BUILDER_OFF)
if(ROOT_BUILDER EQUAL -1 OR ROOT_BUILDER_OFF EQUAL -1)
    message(FATAL_ERROR "Unit::BuildForceMoveRootPacket must use the reader-derived builders, both branches")
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
        SMSG_SPLINE_MOVE_SET_NORMAL_FALL
        SMSG_MOVE_SET_HOVER
        SMSG_MOVE_UNSET_HOVER
        SMSG_SPLINE_MOVE_SET_HOVER
        SMSG_SPLINE_MOVE_UNSET_HOVER
        SMSG_FORCE_MOVE_ROOT
        SMSG_FORCE_MOVE_UNROOT
        SMSG_SPLINE_MOVE_ROOT
        SMSG_SPLINE_MOVE_UNROOT)
    string(FIND "${SESSION_SOURCE}" "case ${GATED}:" ADMITTED)
    if(ADMITTED EQUAL -1)
        message(FATAL_ERROR
            "${GATED} is not admitted by IsEnterWorldConverted -- it is built correctly "
            "and then dropped, which is how this family failed silently before")
    endif()
endforeach()

message(STATUS "mop_movement_pair_source: can-fly, water-walk and fall send and admit both halves")

# --- The movement counter must be a real sequence ---------------------------
#
# Stated positively, not as "no digits". The first version of this guard rejected
# decimal literals only, so GetMovementCounter(), a stale local, or a builder that
# ignores its argument all passed -- a reviewer demonstrated the first two, and
# the hover builder was a live instance of the third. Rejecting one wrong form is
# not the same as requiring the right one.
#
# So: count the call sites, count the ones that call NextMovementCounter(), and
# require the two to be equal. Anything else fails whatever shape it takes.

string(REGEX MATCHALL "Build[A-Za-z]*Packet[(]&data, enable, "
    STATE_CALL_SITES "${PLAYER_SOURCE}")
string(REGEX MATCHALL "Build[A-Za-z]*Packet[(]&data, enable, NextMovementCounter[(][)][)]"
    STATE_CORRECT_SITES "${PLAYER_SOURCE}")
list(LENGTH STATE_CALL_SITES STATE_CALL_COUNT)
list(LENGTH STATE_CORRECT_SITES STATE_CORRECT_COUNT)
if(STATE_CALL_COUNT EQUAL 0)
    message(FATAL_ERROR "Found no movement-state senders at all -- the guard has drifted")
endif()
if(NOT STATE_CALL_COUNT EQUAL STATE_CORRECT_COUNT)
    message(FATAL_ERROR
        "movement-state senders: ${STATE_CORRECT_COUNT} of ${STATE_CALL_COUNT} pass "
        "NextMovementCounter(). Every one must; a constant, a getter or a stale local "
        "all send a sequence number the client has already seen")
endif()

# The speed family draws from the SAME series. That is why observed retail
# sequences skip values: one mover's packets of every kind advance one counter.
# Nine mover builders here; the nine SPLINE counterparts take no counter at all
# and must not be given one.
string(REGEX MATCHALL "BuildMoveSet[A-Za-z]+[(]data, guid.GetRawValue[(][)], "
    SPEED_CALL_SITES "${UNIT_SPEED_SOURCE}")
string(REGEX MATCHALL "BuildMoveSet[A-Za-z]+[(]data, guid.GetRawValue[(][)], NextMovementCounter[(][)], "
    SPEED_CORRECT_SITES "${UNIT_SPEED_SOURCE}")
list(LENGTH SPEED_CALL_SITES SPEED_CALL_COUNT)
list(LENGTH SPEED_CORRECT_SITES SPEED_CORRECT_COUNT)
if(SPEED_CALL_COUNT EQUAL 0)
    message(FATAL_ERROR "Found no speed senders at all -- the guard has drifted")
endif()
if(NOT SPEED_CALL_COUNT EQUAL SPEED_CORRECT_COUNT)
    message(FATAL_ERROR
        "speed senders: ${SPEED_CORRECT_COUNT} of ${SPEED_CALL_COUNT} pass NextMovementCounter()")
endif()



# The source of the sequence must actually advance, and must not emit 0: the
# movement block encodes a zero counter as absent, and no captured body carries
# one -- the lowest observed is 4.
string(FIND "${UNIT_HEADER}" "uint32 NextMovementCounter() { return ++m_movementCounter; }" COUNTER_ADVANCES)
if(COUNTER_ADVANCES EQUAL -1)
    message(FATAL_ERROR
        "Unit::NextMovementCounter must pre-increment -- it must advance, and it must "
        "not emit 0")
endif()

# A sender advancing the counter is worthless if the BUILDER discards it.
# BuildMoveHoverPacket serialized a literal 0 in both branches while taking a
# value argument, so SetHover burned a sequence number and sent nothing.
#
# Checked per function, not file-wide: BuildSendPlayVisualPacket legitimately
# writes uint32(0) as an unknown field and sits between two of these builders.
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

    # Stop at the next function so a neighbour cannot satisfy or fail this one.
    string(FIND "${BUILDER_BODY}" "void Unit::" NEXT_FUNCTION)
    if(NOT NEXT_FUNCTION EQUAL -1)
        string(SUBSTRING "${BUILDER_BODY}" 0 ${NEXT_FUNCTION} BUILDER_BODY)
    endif()

    string(FIND "${BUILDER_BODY}" "uint32(0)" BUILDER_DISCARDS)
    if(NOT BUILDER_DISCARDS EQUAL -1)
        message(FATAL_ERROR
            "${BUILDER} serializes a literal 0 instead of its value argument -- "
            "the caller advances the counter and the packet discards it")
    endif()

    string(FIND "${BUILDER_BODY}" "value" BUILDER_USES_VALUE)
    if(BUILDER_USES_VALUE EQUAL -1)
        message(FATAL_ERROR "${BUILDER} ignores its value argument entirely")
    endif()
endforeach()

# The two direct writers outside the builder tables. Reverting either would have
# passed every check above, because those count call sites in PlayerMovement.cpp
# and UnitSpeed.cpp only -- a reviewer pointed out the blind spot.
#
# Knockback previously sent a literal 0, which the client reads as
# "client-originated". Collision height sent sWorld.GetGameTime(), which is not a
# per-mover sequence at all: it is shared by every unit in the world.
string(FIND "${MOVEMENT_HANDLER_SOURCE}" "data << uint32(GetPlayer()->NextMovementCounter());" KNOCKBACK_COUNTER)
if(KNOCKBACK_COUNTER EQUAL -1)
    message(FATAL_ERROR
        "SMSG_MOVE_KNOCK_BACK must stamp NextMovementCounter() -- a literal 0 tells the "
        "client the knockback originated on the client")
endif()

string(FIND "${UNIT_SOURCE_EARLY}" "data << uint32(NextMovementCounter());  // Packet counter" COLLISION_COUNTER)
if(COLLISION_COUNTER EQUAL -1)
    message(FATAL_ERROR
        "SMSG_MOVE_SET_COLLISION_HGT must stamp NextMovementCounter(), not game time")
endif()

# --- Player setters must update the server's own view -----------------------
#
# Every Creature setter writes m_movementInfo; no Player setter did. So the
# server told the client to fly and went on believing it could not -- CanFly()
# reads m_movementInfo, and nothing wrote it until the client's next ordinary
# movement packet happened to carry the flag. That is "changes client state
# without updating server state", the same class as the mount command that used
# to kick the player it helped.

foreach(SETTER_NAME SetRoot SetWaterWalk SetLevitate SetCanFly SetFeatherFall SetHover)
    # Explicit mapping rather than packed "name;flag" list entries: a CMake list
    # splits on semicolons, so packing pairs into one string shreds them. That
    # exact mistake broke this file once already.
    if(SETTER_NAME STREQUAL "SetRoot")
        set(SETTER_FLAGNAME "MOVEFLAG_ROOT")
    elseif(SETTER_NAME STREQUAL "SetWaterWalk")
        set(SETTER_FLAGNAME "MOVEFLAG_WATERWALKING")
    elseif(SETTER_NAME STREQUAL "SetLevitate")
        set(SETTER_FLAGNAME "MOVEFLAG_LEVITATING")
    elseif(SETTER_NAME STREQUAL "SetCanFly")
        set(SETTER_FLAGNAME "MOVEFLAG_CAN_FLY")
    elseif(SETTER_NAME STREQUAL "SetFeatherFall")
        set(SETTER_FLAGNAME "MOVEFLAG_SAFE_FALL")
    else()
        set(SETTER_FLAGNAME "MOVEFLAG_HOVER")
    endif()

    string(FIND "${PLAYER_SOURCE}" "void Player::${SETTER_NAME}(bool enable)" SETTER_START)
    if(SETTER_START EQUAL -1)
        message(FATAL_ERROR "Could not locate Player::${SETTER_NAME}")
    endif()
    math(EXPR SETTER_ROOM "${PLAYER_LENGTH} - ${SETTER_START}")
    if(SETTER_ROOM GREATER 2400)
        set(SETTER_ROOM 2400)
    endif()
    string(SUBSTRING "${PLAYER_SOURCE}" ${SETTER_START} ${SETTER_ROOM} SETTER_BODY)

    string(FIND "${SETTER_BODY}" "AddMovementFlag(${SETTER_FLAGNAME})" SETTER_ADDS)
    string(FIND "${SETTER_BODY}" "RemoveMovementFlag(${SETTER_FLAGNAME})" SETTER_REMOVES)
    if(SETTER_ADDS EQUAL -1 OR SETTER_REMOVES EQUAL -1)
        message(FATAL_ERROR
            "Player::${SETTER_NAME} must update m_movementInfo with ${SETTER_FLAGNAME} -- "
            "otherwise the client is told and the server's own view never changes")
    endif()
endforeach()
