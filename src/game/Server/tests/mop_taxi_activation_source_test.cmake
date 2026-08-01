# SPDX-License-Identifier: GPL-3.0-or-later

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" PLAYER_H)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" PLAYER_CPP)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerTaxi.cpp" PLAYER_TAXI_CPP)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/TaxiHandler.cpp" TAXI_HANDLER)
file(READ "${SOURCE_ROOT}/src/game/MotionGenerators/MotionMaster.h" MOTION_MASTER_H)
file(READ "${SOURCE_ROOT}/src/game/MotionGenerators/MotionMaster.cpp" MOTION_MASTER_CPP)
file(READ "${SOURCE_ROOT}/src/game/MotionGenerators/WaypointMovementGenerator.h" WAYPOINT_H)
file(READ "${SOURCE_ROOT}/src/game/MotionGenerators/WaypointMovementGenerator.cpp" WAYPOINT_CPP)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" WORLD_SESSION_H)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" OPCODES_CPP)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" WORLD_SESSION_CPP)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" OPCODES_REFERENCE_H)

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "request_scalar_order")
        string(REPLACE "in >> parsed.destinationNode >> parsed.sourceNode;"
            "in >> parsed.sourceNode >> parsed.destinationNode;" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "request_mask")
        string(REPLACE "ReadGuidMask<4, 0, 1, 2, 5, 6, 7, 3>"
            "ReadGuidMask<0, 4, 1, 2, 5, 6, 7, 3>" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "request_bytes")
        string(REPLACE "ReadGuidBytes<1, 0, 6, 5, 2, 4, 3, 7>"
            "ReadGuidBytes<0, 1, 6, 5, 2, 4, 3, 7>" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "request_xor")
        string(REPLACE "ReadGuidBytes<1, 0, 6, 5, 2, 4, 3, 7>"
            "ReadGuidBytesRaw<1, 0, 6, 5, 2, 4, 3, 7>" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "request_canonical")
        string(REPLACE "!HasCanonicalPackedGuidBytes(in, in.rpos() + 9, guidByteCount)"
            "false" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "request_eof")
        string(REPLACE "in.rpos() != in.size() || parsed.flightMaster.IsEmpty()"
            "false || parsed.flightMaster.IsEmpty()" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "request_zero_guid")
        string(REPLACE "in.rpos() != in.size() || parsed.flightMaster.IsEmpty()"
            "in.rpos() != in.size() || false" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "handler_parser")
        string(REPLACE "MopTaxiPackets::ParseActivateTaxi(recv_data, request)"
            "true" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "interaction_before_parse")
        string(REPLACE "if (!MopTaxiPackets::ParseActivateTaxi(recv_data, request))"
            "GetPlayer()->GetNPCIfCanInteractWith(request.flightMaster, UNIT_NPC_FLAG_FLIGHTMASTER);\n    if (!MopTaxiPackets::ParseActivateTaxi(recv_data, request))"
            TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "npc_authority")
        string(REPLACE "UNIT_NPC_FLAG_FLIGHTMASTER" "UNIT_NPC_FLAG_VENDOR"
            TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "current_node_lookup")
        string(REPLACE "npc->GetMapId(), GetPlayer()->GetTeam()"
            "npc->GetMapId(), TEAM_NONE" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "source_authority")
        string(REPLACE "request.sourceNode != currentNode" "false"
            TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "too_far_reply")
        string(REPLACE "SendActivateTaxiReply(ERR_TAXITOOFARAWAY);"
            "SendActivateTaxiReply(ERR_TAXINOSUCHPATH);" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "node_bounds")
        string(REPLACE "m_taxi.IsValidNodeId(request.destinationNode)"
            "m_taxi.IsTaximaskNodeKnown(request.destinationNode)" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "same_node_guard")
        string(REPLACE "currentNode == request.destinationNode ||" "false ||"
            TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "same_map_guard")
        string(REPLACE "destinationNodeEntry->ContinentID != npc->GetMapId()"
            "false" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "path_node_map_guard")
        string(REPLACE "!MopTaxiPackets::IsSameMapTaxiPath("
            "false && MopTaxiPackets::IsSameMapTaxiPath("
            TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "no_path_guard")
        string(REPLACE "|| !path ||" "|| false ||" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "known_nodes")
        string(REPLACE "m_taxi.IsTaximaskNodeKnown(request.destinationNode)"
            "m_taxi.IsValidNodeId(request.destinationNode)" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "activation_nodes")
        string(REPLACE "{ currentNode, request.destinationNode }"
            "{ request.destinationNode, currentNode }" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "reply_ok_mapping")
        string(REPLACE "case ERR_TAXIOK: return 8;" "case ERR_TAXIOK: return 0;"
            PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "reply_no_path_mapping")
        string(REPLACE "case ERR_TAXINOSUCHPATH: return 6;"
            "case ERR_TAXINOSUCHPATH: return 2;" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "reply_too_far_mapping")
        string(REPLACE "case ERR_TAXITOOFARAWAY: return 13;"
            "case ERR_TAXITOOFARAWAY: return 4;" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "reply_width")
        string(REPLACE "out.WriteBits(ActivateTaxiReplyValue(reply), 4);"
            "out.WriteBits(ActivateTaxiReplyValue(reply), 8);" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "reply_flush")
        string(REPLACE "out.WriteBits(ActivateTaxiReplyValue(reply), 4);\n        out.FlushBits();"
            "out.WriteBits(ActivateTaxiReplyValue(reply), 4);" PLAYER_H "${PLAYER_H}")
    elseif(MUTATION STREQUAL "reply_builder")
        string(REPLACE "MopTaxiPackets::BuildActivateTaxiReply(data, reply);"
            "data << uint32(reply);" TAXI_HANDLER "${TAXI_HANDLER}")
    elseif(MUTATION STREQUAL "launch_result")
        string(REPLACE "bool MoveTaxiFlight(uint32 path, uint32 pathnode);"
            "void MoveTaxiFlight(uint32 path, uint32 pathnode);" MOTION_MASTER_H "${MOTION_MASTER_H}")
    elseif(MUTATION STREQUAL "spline_launch_result")
        string(REPLACE "m_splineLaunched = init.Launch() > 0;"
            "m_splineLaunched = true; init.Launch();" WAYPOINT_CPP "${WAYPOINT_CPP}")
    elseif(MUTATION STREQUAL "flight_min_nodes")
        string(REPLACE "end - GetCurrentNode() < 2" "false"
            WAYPOINT_CPP "${WAYPOINT_CPP}")
    elseif(MUTATION STREQUAL "failed_generator_cleanup")
        string(REPLACE "        MovementExpired(false);\n        return false;"
            "        return false;" MOTION_MASTER_CPP "${MOTION_MASTER_CPP}")
    elseif(MUTATION STREQUAL "send_flight_result")
        string(REPLACE "bool SendDoFlight(uint32 mountDisplayId, uint32 path, uint32 pathNode = 0);"
            "void SendDoFlight(uint32 mountDisplayId, uint32 path, uint32 pathNode = 0);"
            WORLD_SESSION_H "${WORLD_SESSION_H}")
    elseif(MUTATION STREQUAL "success_before_launch")
        string(REPLACE "if (!GetSession()->SendDoFlight(mountDisplayId, path))"
            "GetSession()->SendActivateTaxiReply(ERR_TAXIOK);\n    if (!GetSession()->SendDoFlight(mountDisplayId, path))"
            PLAYER_TAXI_CPP "${PLAYER_TAXI_CPP}")
    elseif(MUTATION STREQUAL "achievement_before_launch")
        string(REPLACE "if (!GetSession()->SendDoFlight(mountDisplayId, path))"
            "GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_FLIGHT_PATHS_TAKEN, 1);\n    if (!GetSession()->SendDoFlight(mountDisplayId, path))"
            PLAYER_TAXI_CPP "${PLAYER_TAXI_CPP}")
    elseif(MUTATION STREQUAL "fare_after_launch")
        string(REPLACE "ModifyMoney(-(int64)totalCost);" ""
            PLAYER_TAXI_CPP "${PLAYER_TAXI_CPP}")
    elseif(MUTATION STREQUAL "insufficient_money")
        string(REPLACE "if (money < totalcost)" "if (false)"
            PLAYER_TAXI_CPP "${PLAYER_TAXI_CPP}")
    elseif(MUTATION STREQUAL "launch_failure_cleanup")
        string(REPLACE "ModifyMoney(totalCost);" "" PLAYER_TAXI_CPP "${PLAYER_TAXI_CPP}")
    elseif(MUTATION STREQUAL "duplicate_flight_guard")
        string(REPLACE "IsTaxiFlying()" "false" PLAYER_CPP "${PLAYER_CPP}")
    elseif(MUTATION STREQUAL "request_registration")
        string(REPLACE "DefC(CMSG_ACTIVATETAXI," "DefX(CMSG_ACTIVATETAXI,"
            OPCODES_CPP "${OPCODES_CPP}")
    elseif(MUTATION STREQUAL "reply_registration")
        string(REPLACE "DefS(SMSG_ACTIVATETAXIREPLY," "DefX(SMSG_ACTIVATETAXIREPLY,"
            OPCODES_CPP "${OPCODES_CPP}")
    elseif(MUTATION STREQUAL "reply_admission")
        string(REPLACE "case SMSG_ACTIVATETAXIREPLY:"
            "case SMSG_ACTIVATETAXIREPLY_MUTATED:" WORLD_SESSION_CPP "${WORLD_SESSION_CPP}")
    elseif(MUTATION STREQUAL "request_reference")
        string(REPLACE "CMSG_ACTIVATETAXI                              0x03C9  ACTIVE"
            "CMSG_ACTIVATETAXI                              0x03C9  DORMANT"
            OPCODES_REFERENCE_H "${OPCODES_REFERENCE_H}")
    elseif(MUTATION STREQUAL "reply_reference")
        string(REPLACE "SMSG_ACTIVATETAXIREPLY                         0x02A7  ACTIVE"
            "SMSG_ACTIVATETAXIREPLY                         0x02A7  DORMANT"
            OPCODES_REFERENCE_H "${OPCODES_REFERENCE_H}")
    elseif(MUTATION STREQUAL "express_registration")
        string(APPEND OPCODES_CPP "\nDefC(CMSG_ACTIVATETAXIEXPRESS, accidental_neighbor);\n")
    elseif(MUTATION STREQUAL "completion_registration")
        string(APPEND OPCODES_CPP "\nDefC(CMSG_MOVE_SPLINE_DONE, accidental_neighbor);\n")
    elseif(MUTATION STREQUAL "express_reference")
        string(REPLACE "CMSG_ACTIVATETAXIEXPRESS                       0x06FB  DORMANT"
            "CMSG_ACTIVATETAXIEXPRESS                       0x06FB  ACTIVE"
            OPCODES_REFERENCE_H "${OPCODES_REFERENCE_H}")
    elseif(MUTATION STREQUAL "completion_reference")
        string(REPLACE "CMSG_MOVE_SPLINE_DONE                          0x11D9  DORMANT"
            "CMSG_MOVE_SPLINE_DONE                          0x11D9  ACTIVE"
            OPCODES_REFERENCE_H "${OPCODES_REFERENCE_H}")
    else()
        message(FATAL_ERROR "Unknown mutation: ${MUTATION}")
    endif()
endif()

function(require_text HAYSTACK NEEDLE LABEL)
    string(FIND "${${HAYSTACK}}" "${NEEDLE}" POSITION)
    if(POSITION EQUAL -1)
        message(FATAL_ERROR "Missing ${LABEL}")
    endif()
endfunction()

function(require_once_literal HAYSTACK TOKEN LABEL)
    string(FIND "${${HAYSTACK}}" "${TOKEN}" FIRST_POSITION)
    if(FIRST_POSITION EQUAL -1)
        message(FATAL_ERROR "Missing ${LABEL}")
    endif()
    string(LENGTH "${TOKEN}" TOKEN_LENGTH)
    math(EXPR TAIL_POSITION "${FIRST_POSITION} + ${TOKEN_LENGTH}")
    string(SUBSTRING "${${HAYSTACK}}" ${TAIL_POSITION} -1 TAIL)
    string(FIND "${TAIL}" "${TOKEN}" SECOND_POSITION)
    if(NOT SECOND_POSITION EQUAL -1)
        message(FATAL_ERROR "${LABEL}: expected exactly one occurrence")
    endif()
endfunction()

function(require_before HAYSTACK FIRST SECOND LABEL)
    string(FIND "${${HAYSTACK}}" "${FIRST}" FIRST_POSITION)
    string(FIND "${${HAYSTACK}}" "${SECOND}" SECOND_POSITION)
    if(FIRST_POSITION EQUAL -1 OR SECOND_POSITION EQUAL -1 OR
        NOT FIRST_POSITION LESS SECOND_POSITION)
        message(FATAL_ERROR "Wrong ${LABEL} ordering")
    endif()
endfunction()

function(forbid_text HAYSTACK NEEDLE LABEL)
    string(FIND "${${HAYSTACK}}" "${NEEDLE}" POSITION)
    if(NOT POSITION EQUAL -1)
        message(FATAL_ERROR "Unexpected ${LABEL}")
    endif()
endfunction()

string(FIND "${PLAYER_H}" "inline bool ParseActivateTaxi" PARSER_BEGIN)
string(FIND "${PLAYER_H}" "inline TaxiNodeStatus StatusForKnown" PARSER_END)
if(PARSER_BEGIN EQUAL -1 OR PARSER_END LESS_EQUAL PARSER_BEGIN)
    message(FATAL_ERROR "Unable to isolate direct activation parser")
endif()
math(EXPR PARSER_LENGTH "${PARSER_END} - ${PARSER_BEGIN}")
string(SUBSTRING "${PLAYER_H}" ${PARSER_BEGIN} ${PARSER_LENGTH} ACTIVATION_PARSER)

string(FIND "${PLAYER_H}" "inline uint8 ActivateTaxiReplyValue" REPLY_BEGIN)
string(FIND "${PLAYER_H}" "inline void BuildShowTaxiNodes" REPLY_END)
if(REPLY_BEGIN EQUAL -1 OR REPLY_END LESS_EQUAL REPLY_BEGIN)
    message(FATAL_ERROR "Unable to isolate direct activation reply builder")
endif()
math(EXPR REPLY_LENGTH "${REPLY_END} - ${REPLY_BEGIN}")
string(SUBSTRING "${PLAYER_H}" ${REPLY_BEGIN} ${REPLY_LENGTH} ACTIVATION_REPLY)

string(FIND "${TAXI_HANDLER}" "void WorldSession::HandleActivateTaxiOpcode" HANDLER_BEGIN)
if(HANDLER_BEGIN EQUAL -1)
    message(FATAL_ERROR "Unable to isolate direct activation handler")
endif()
string(SUBSTRING "${TAXI_HANDLER}" ${HANDLER_BEGIN} -1 ACTIVATION_HANDLER)

string(FIND "${TAXI_HANDLER}" "bool WorldSession::SendDoFlight" SEND_FLIGHT_BEGIN)
string(FIND "${TAXI_HANDLER}" "bool WorldSession::SendLearnNewTaxiNode" SEND_FLIGHT_END)
if(SEND_FLIGHT_BEGIN EQUAL -1 OR SEND_FLIGHT_END LESS_EQUAL SEND_FLIGHT_BEGIN)
    message(FATAL_ERROR "Unable to isolate taxi launch helper")
endif()
math(EXPR SEND_FLIGHT_LENGTH "${SEND_FLIGHT_END} - ${SEND_FLIGHT_BEGIN}")
string(SUBSTRING "${TAXI_HANDLER}" ${SEND_FLIGHT_BEGIN} ${SEND_FLIGHT_LENGTH} SEND_FLIGHT)

string(FIND "${TAXI_HANDLER}" "void WorldSession::SendActivateTaxiReply" SEND_REPLY_BEGIN)
string(FIND "${TAXI_HANDLER}" "void WorldSession::HandleActivateTaxiExpressOpcode" SEND_REPLY_END)
if(SEND_REPLY_BEGIN EQUAL -1 OR SEND_REPLY_END LESS_EQUAL SEND_REPLY_BEGIN)
    message(FATAL_ERROR "Unable to isolate taxi activation reply sender")
endif()
math(EXPR SEND_REPLY_LENGTH "${SEND_REPLY_END} - ${SEND_REPLY_BEGIN}")
string(SUBSTRING "${TAXI_HANDLER}" ${SEND_REPLY_BEGIN} ${SEND_REPLY_LENGTH} SEND_REPLY)

string(FIND "${PLAYER_TAXI_CPP}" "bool Player::ActivateTaxiPathTo(std::vector<uint32> const& nodes" ACTIVATE_BEGIN)
string(FIND "${PLAYER_TAXI_CPP}" "bool Player::ActivateTaxiPathTo(uint32 taxi_path_id" ACTIVATE_END)
if(ACTIVATE_BEGIN EQUAL -1 OR ACTIVATE_END LESS_EQUAL ACTIVATE_BEGIN)
    message(FATAL_ERROR "Unable to isolate taxi route authority")
endif()
math(EXPR ACTIVATE_LENGTH "${ACTIVATE_END} - ${ACTIVATE_BEGIN}")
string(SUBSTRING "${PLAYER_TAXI_CPP}" ${ACTIVATE_BEGIN} ${ACTIVATE_LENGTH} ROUTE_AUTHORITY)

string(FIND "${MOTION_MASTER_CPP}" "bool MotionMaster::MoveTaxiFlight" MOTION_TAXI_BEGIN)
string(FIND "${MOTION_MASTER_CPP}" "void MotionMaster::MoveDistract" MOTION_TAXI_END)
if(MOTION_TAXI_BEGIN EQUAL -1 OR MOTION_TAXI_END LESS_EQUAL MOTION_TAXI_BEGIN)
    message(FATAL_ERROR "Unable to isolate taxi movement launch")
endif()
math(EXPR MOTION_TAXI_LENGTH "${MOTION_TAXI_END} - ${MOTION_TAXI_BEGIN}")
string(SUBSTRING "${MOTION_MASTER_CPP}" ${MOTION_TAXI_BEGIN} ${MOTION_TAXI_LENGTH} MOTION_TAXI)

require_text(ACTIVATION_PARSER "remaining < 9" "fixed scalar and mask minimum")
require_text(ACTIVATION_PARSER "PackedGuidByteCount(in[in.rpos() + 8])" "GUID mask offset")
require_text(ACTIVATION_PARSER "remaining != 9 + guidByteCount" "exact request size")
require_text(ACTIVATION_PARSER
    "!HasCanonicalPackedGuidBytes(in, in.rpos() + 9, guidByteCount)"
    "canonical packed GUID bytes")
require_text(ACTIVATION_PARSER "in >> parsed.destinationNode >> parsed.sourceNode;"
    "destination then source scalars")
require_text(ACTIVATION_PARSER "ReadGuidMask<4, 0, 1, 2, 5, 6, 7, 3>"
    "request GUID mask order")
require_text(ACTIVATION_PARSER "ReadGuidBytes<1, 0, 6, 5, 2, 4, 3, 7>"
    "request GUID byte order and XOR")
require_text(ACTIVATION_PARSER
    "in.rpos() != in.size() || parsed.flightMaster.IsEmpty()"
    "EOF and zero-GUID rejection")

foreach(MAPPING IN ITEMS
        "ERR_TAXIOK: return 8" "ERR_TAXIUNSPECIFIEDSERVERERROR: return 5"
        "ERR_TAXINOSUCHPATH: return 6" "ERR_TAXINOTENOUGHMONEY: return 4"
        "ERR_TAXITOOFARAWAY: return 13" "ERR_TAXINOVENDORNEARBY: return 12"
        "ERR_TAXINOTVISITED: return 15" "ERR_TAXIPLAYERBUSY: return 10"
        "ERR_TAXIPLAYERALREADYMOUNTED: return 7"
        "ERR_TAXIPLAYERSHAPESHIFTED: return 9")
    require_text(ACTIVATION_REPLY "case ${MAPPING};" "18414 reply mapping ${MAPPING}")
endforeach()
require_text(ACTIVATION_REPLY "default: return 5;" "unproven legacy reply fail-safe")
require_text(ACTIVATION_REPLY "out.WriteBits(ActivateTaxiReplyValue(reply), 4);"
    "four-bit reply")
require_text(ACTIVATION_REPLY "out.FlushBits();" "reply bit flush")

require_text(SEND_REPLY "WorldPacket data(SMSG_ACTIVATETAXIREPLY, 1);"
    "one-byte activation reply")
require_text(SEND_REPLY "MopTaxiPackets::BuildActivateTaxiReply(data, reply);"
    "activation reply builder integration")
string(FIND "${SEND_REPLY}" "data << uint32(reply)" LEGACY_REPLY)
if(NOT LEGACY_REPLY EQUAL -1)
    message(FATAL_ERROR "Legacy uint32 taxi activation reply remains")
endif()

require_text(ACTIVATION_HANDLER "MopTaxiPackets::ParseActivateTaxi(recv_data, request)"
    "handler parser")
require_text(ACTIVATION_HANDLER "GetPlayer()->GetNPCIfCanInteractWith("
    "interaction authority call")
require_text(ACTIVATION_HANDLER "request.flightMaster, UNIT_NPC_FLAG_FLIGHTMASTER)"
    "flight-master authority")
require_before(ACTIVATION_HANDLER
    "MopTaxiPackets::ParseActivateTaxi(recv_data, request)"
    "GetPlayer()->GetNPCIfCanInteractWith("
    "parse before interaction")
require_text(ACTIVATION_HANDLER
    "npc->GetPositionX(), npc->GetPositionY(), npc->GetPositionZ(),\n        npc->GetMapId(), GetPlayer()->GetTeam()"
    "server-derived current node")
require_text(ACTIVATION_HANDLER "currentNode == 0 || request.sourceNode != currentNode"
    "source-to-NPC equality")
require_text(ACTIVATION_HANDLER "SendActivateTaxiReply(ERR_TAXITOOFARAWAY);"
    "wrong-source reply")
require_text(ACTIVATION_HANDLER "m_taxi.IsValidNodeId(currentNode)"
    "source node domain guard")
require_text(ACTIVATION_HANDLER "m_taxi.IsValidNodeId(request.destinationNode)"
    "destination node domain guard")
require_text(ACTIVATION_HANDLER "currentNode == request.destinationNode ||"
    "same-node rejection")
require_text(ACTIVATION_HANDLER "destinationNodeEntry->ContinentID != npc->GetMapId()"
    "same-map restriction")
require_text(ACTIVATION_HANDLER "|| !path ||" "server route guard")
require_text(ACTIVATION_HANDLER "path >= sTaxiPathNodesByPath.size()"
    "server path-node bounds")
require_text(ACTIVATION_HANDLER "!MopTaxiPackets::IsSameMapTaxiPath("
    "every DBC path node same-map guard")
require_text(PLAYER_H "if (path[i].ContinentID != mapId)"
    "same-map path-node implementation")
require_text(ACTIVATION_HANDLER "SendActivateTaxiReply(ERR_TAXINOSUCHPATH);"
    "no-path reply")
require_text(ACTIVATION_HANDLER "m_taxi.IsTaximaskNodeKnown(currentNode)"
    "known source node")
require_text(ACTIVATION_HANDLER "m_taxi.IsTaximaskNodeKnown(request.destinationNode)"
    "known destination node")
require_text(ACTIVATION_HANDLER "SendActivateTaxiReply(ERR_TAXINOTVISITED);"
    "unknown-node reply")
require_text(ACTIVATION_HANDLER "{ currentNode, request.destinationNode }"
    "authoritative single-leg route")
require_text(ACTIVATION_HANDLER "ActivateTaxiPathTo(nodes, npc);"
    "existing route/fare authority call")
require_before(ACTIVATION_HANDLER "request.sourceNode != currentNode"
    "m_taxi.IsTaximaskNodeKnown(currentNode)" "source authority before discovery")
require_before(ACTIVATION_HANDLER "|| !path ||"
    "ActivateTaxiPathTo(nodes, npc);" "server route before activation")

require_text(WORLD_SESSION_H
    "bool SendDoFlight(uint32 mountDisplayId, uint32 path, uint32 pathNode = 0);"
    "launch result declaration")
require_text(MOTION_MASTER_H "bool MoveTaxiFlight(uint32 path, uint32 pathnode);"
    "movement launch result declaration")
require_text(MOTION_TAXI "bool MotionMaster::MoveTaxiFlight(uint32 path, uint32 pathnode)"
    "movement launch result implementation")
require_text(MOTION_TAXI "pathnode >= sTaxiPathNodesByPath[path].size()"
    "movement path-node bounds")
require_text(WAYPOINT_H "bool HasLaunched() const { return m_splineLaunched; }"
    "flight spline launch result accessor")
require_text(WAYPOINT_CPP "end - GetCurrentNode() < 2"
    "minimum flight spline nodes")
require_text(WAYPOINT_CPP "m_splineLaunched = init.Launch() > 0;"
    "actual flight spline launch result")
require_text(MOTION_TAXI "if (!mgen->HasLaunched())"
    "actual launch result gate")
require_text(MOTION_TAXI "MovementExpired(false);"
    "failed flight generator finalization")
forbid_text(MOTION_TAXI
    "return GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE;"
    "generator-type launch confirmation")
require_text(SEND_FLIGHT "if (!GetPlayer()->GetMotionMaster()->MoveTaxiFlight(path, pathNode))"
    "movement launch failure")
require_text(SEND_FLIGHT "GetPlayer()->Unmount();" "failed launch mount cleanup")
require_text(SEND_FLIGHT "return true;" "successful launch result")

require_text(PLAYER_H "bool StartTaxiFlight(uint32 mountDisplayId, uint32 path, uint32 totalCost);"
    "prepared flight transaction declaration")
require_text(ROUTE_AUTHORITY "return StartTaxiFlight(mount_display_id, sourcepath, totalcost);"
    "prepared flight transaction call")
require_once_literal(ROUTE_AUTHORITY "ModifyMoney(-(int64)totalCost);" "single fare debit")
require_text(ROUTE_AUTHORITY "if (!GetSession()->SendDoFlight(mountDisplayId, path))"
    "launch result gate")
require_text(ROUTE_AUTHORITY "ModifyMoney(totalCost);" "failed launch fare rollback")
require_text(ROUTE_AUTHORITY "m_taxi.ClearTaxiDestinations();"
    "failed launch destination cleanup")
require_before(ROUTE_AUTHORITY "ModifyMoney(-(int64)totalCost);"
    "GetSession()->SendDoFlight(mountDisplayId, path)" "fare before launch")
require_before(ROUTE_AUTHORITY "GetSession()->SendDoFlight(mountDisplayId, path)"
    "GetSession()->SendActivateTaxiReply(ERR_TAXIOK);" "confirmed launch before success")
require_before(ROUTE_AUTHORITY "GetSession()->SendDoFlight(mountDisplayId, path)"
    "GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_GOLD_SPENT_FOR_TRAVELLING"
    "confirmed launch before travel-spend achievement")
require_before(ROUTE_AUTHORITY "GetSession()->SendDoFlight(mountDisplayId, path)"
    "GetAchievementMgr().UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_FLIGHT_PATHS_TAKEN"
    "confirmed launch before flight-count achievement")
require_before(ROUTE_AUTHORITY "if (money < totalcost)"
    "ModifyMoney(-(int64)totalCost);" "funds check before fare")
require_before(ROUTE_AUTHORITY "if (!path)" "ModifyMoney(-(int64)totalCost);"
    "route checks before fare")

require_text(PLAYER_CPP "!guid || !IsInWorld() || IsTaxiFlying()"
    "duplicate request flight guard")
require_text(OPCODES_CPP "DefC(CMSG_ACTIVATETAXI," "direct request registration")
require_text(OPCODES_CPP "STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleActivateTaxiOpcode"
    "thread-unsafe direct handler registration")
require_text(OPCODES_CPP "DefS(SMSG_ACTIVATETAXIREPLY," "activation reply registration")
require_text(WORLD_SESSION_CPP "case SMSG_ACTIVATETAXIREPLY:" "activation reply admission")
require_text(OPCODES_REFERENCE_H
    "SMSG_ACTIVATETAXIREPLY                         0x02A7  ACTIVE"
    "activation reply active reference")
require_text(OPCODES_REFERENCE_H
    "CMSG_ACTIVATETAXI                              0x03C9  ACTIVE"
    "activation request active reference")

foreach(FORBIDDEN IN ITEMS CMSG_ACTIVATETAXIEXPRESS CMSG_MOVE_SPLINE_DONE)
    string(FIND "${OPCODES_CPP}" "DefC(${FORBIDDEN}," FORBIDDEN_REGISTRATION)
    if(NOT FORBIDDEN_REGISTRATION EQUAL -1)
        message(FATAL_ERROR "Unexpected neighboring taxi registration: ${FORBIDDEN}")
    endif()
endforeach()
require_text(OPCODES_REFERENCE_H
    "CMSG_ACTIVATETAXIEXPRESS                       0x06FB  DORMANT"
    "express remains dormant")
require_text(OPCODES_REFERENCE_H
    "CMSG_MOVE_SPLINE_DONE                          0x11D9  DORMANT"
    "completion remains dormant")

message(STATUS "MoP taxi-activation source checks passed")
