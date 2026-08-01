file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/TaxiHandler.cpp" taxi_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellEffectTail.cpp" spell_effect_tail)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

macro(mutate variable old new context)
    set(before "${${variable}}")
    string(REPLACE "${old}" "${new}" ${variable} "${${variable}}")
    if("${${variable}}" STREQUAL "${before}")
        message(FATAL_ERROR "${context} mutation setup guard: target not found")
    endif()
endmacro()

if(MUTATION MATCHES "^request_mask_adj_([0-9]+)$")
    set(index "${CMAKE_MATCH_1}")
    set(good "7, 4, 1, 3, 0, 5, 2, 6")
    set(swaps
        "4, 7, 1, 3, 0, 5, 2, 6"
        "7, 1, 4, 3, 0, 5, 2, 6"
        "7, 4, 3, 1, 0, 5, 2, 6"
        "7, 4, 1, 0, 3, 5, 2, 6"
        "7, 4, 1, 3, 5, 0, 2, 6"
        "7, 4, 1, 3, 0, 2, 5, 6"
        "7, 4, 1, 3, 0, 5, 6, 2")
    list(GET swaps ${index} bad)
    mutate(player_header "ReadGuidMask<${good}>" "ReadGuidMask<${bad}>"
        "request mask adjacency ${index}")
elseif(MUTATION MATCHES "^request_byte_adj_([0-9]+)$")
    set(index "${CMAKE_MATCH_1}")
    set(good "7, 1, 5, 2, 4, 0, 6, 3")
    set(swaps
        "1, 7, 5, 2, 4, 0, 6, 3"
        "7, 5, 1, 2, 4, 0, 6, 3"
        "7, 1, 2, 5, 4, 0, 6, 3"
        "7, 1, 5, 4, 2, 0, 6, 3"
        "7, 1, 5, 2, 0, 4, 6, 3"
        "7, 1, 5, 2, 4, 6, 0, 3"
        "7, 1, 5, 2, 4, 0, 3, 6")
    list(GET swaps ${index} bad)
    mutate(player_header "ReadGuidBytes<${good}>" "ReadGuidBytes<${bad}>"
        "request byte adjacency ${index}")
elseif(MUTATION MATCHES "^reply_mask_adj_([0-9]+)$")
    set(index "${CMAKE_MATCH_1}")
    set(good_masks
        "6, 2, 7, 5, 4, 1"
        "6, 2, 7, 5, 4, 1"
        "6, 2, 7, 5, 4, 1"
        "6, 2, 7, 5, 4, 1"
        "6, 2, 7, 5, 4, 1"
        "3, 0")
    set(bad_masks
        "2, 6, 7, 5, 4, 1"
        "6, 7, 2, 5, 4, 1"
        "6, 2, 5, 7, 4, 1"
        "6, 2, 7, 4, 5, 1"
        "6, 2, 7, 5, 1, 4"
        "0, 3")
    list(GET good_masks ${index} good)
    list(GET bad_masks ${index} bad)
    mutate(player_header "WriteGuidMask<${good}>" "WriteGuidMask<${bad}>"
        "reply mask adjacency ${index}")
elseif(MUTATION MATCHES "^reply_byte_adj_([0-9]+)$")
    set(index "${CMAKE_MATCH_1}")
    set(good "0, 5, 2, 1, 4, 6, 7, 3")
    set(swaps
        "5, 0, 2, 1, 4, 6, 7, 3"
        "0, 2, 5, 1, 4, 6, 7, 3"
        "0, 5, 1, 2, 4, 6, 7, 3"
        "0, 5, 2, 4, 1, 6, 7, 3"
        "0, 5, 2, 1, 6, 4, 7, 3"
        "0, 5, 2, 1, 4, 7, 6, 3"
        "0, 5, 2, 1, 4, 6, 3, 7")
    list(GET swaps ${index} bad)
    mutate(player_header "WriteGuidBytes<${good}>" "WriteGuidBytes<${bad}>"
        "reply byte adjacency ${index}")
elseif(MUTATION STREQUAL "status_before_first_mask")
    mutate(player_header
        "out.WriteGuidMask<6, 2, 7, 5, 4, 1>(guid);\n        out.WriteBits(uint8(status), 2);"
        "out.WriteBits(uint8(status), 2);\n        out.WriteGuidMask<6, 2, 7, 5, 4, 1>(guid);"
        "status before first mask")
elseif(MUTATION STREQUAL "status_after_second_mask")
    mutate(player_header
        "out.WriteBits(uint8(status), 2);\n        out.WriteGuidMask<3, 0>(guid);"
        "out.WriteGuidMask<3, 0>(guid);\n        out.WriteBits(uint8(status), 2);"
        "status after second mask")
elseif(MUTATION STREQUAL "status_width")
    mutate(player_header "out.WriteBits(uint8(status), 2);"
        "out << uint8(status);" "status width")
elseif(MUTATION STREQUAL "request_xor")
    mutate(player_header "in.ReadGuidBytes<7, 1, 5, 2, 4, 0, 6, 3>(parsed);"
        "in >> parsed;" "request XOR")
elseif(MUTATION STREQUAL "request_bit_reset")
    mutate(player_header
        "in.ResetBitReader();\n        in.ReadGuidMask<7, 4, 1, 3, 0, 5, 2, 6>(parsed);"
        "in.ReadGuidMask<7, 4, 1, 3, 0, 5, 2, 6>(parsed);"
        "request bit-reader reset")
elseif(MUTATION STREQUAL "reply_xor")
    mutate(player_header "out.WriteGuidBytes<0, 5, 2, 1, 4, 6, 7, 3>(guid);"
        "out << guid;" "reply XOR")
elseif(MUTATION STREQUAL "unlearned_mapping")
    mutate(player_header "known ? TaxiNodeStatus::Learned : TaxiNodeStatus::Unlearned"
        "known ? TaxiNodeStatus::Learned : TaxiNodeStatus::NotEligible"
        "unlearned mapping")
elseif(MUTATION STREQUAL "request_opcode")
    mutate(opcode_header "CMSG_TAXINODE_STATUS_QUERY                   = 0x02E1"
        "CMSG_TAXINODE_STATUS_QUERY                   = 0x02E2"
        "request opcode")
elseif(MUTATION STREQUAL "reply_opcode")
    mutate(opcode_header "SMSG_TAXINODE_STATUS                         = 0x169E"
        "SMSG_TAXINODE_STATUS                         = 0x169F"
        "reply opcode")
elseif(MUTATION STREQUAL "request_registration")
    mutate(opcode_registry "DefC(CMSG_TAXINODE_STATUS_QUERY,"
        "RemovedC(CMSG_TAXINODE_STATUS_QUERY," "request registration")
elseif(MUTATION STREQUAL "reply_registration")
    mutate(opcode_registry "DefS(SMSG_TAXINODE_STATUS,"
        "RemovedS(SMSG_TAXINODE_STATUS," "reply registration")
elseif(MUTATION STREQUAL "reply_admission")
    mutate(session_source "case SMSG_TAXINODE_STATUS:"
        "case REMOVED_SMSG_TAXINODE_STATUS:" "reply admission")
elseif(MUTATION STREQUAL "request_reference")
    mutate(opcode_reference
        "CMSG_TAXINODE_STATUS_QUERY                     0x02E1  ACTIVE"
        "CMSG_TAXINODE_STATUS_QUERY                     0x02E1  DORMANT"
        "request reference")
elseif(MUTATION STREQUAL "reply_reference")
    mutate(opcode_reference
        "SMSG_TAXINODE_STATUS                           0x169E  ACTIVE"
        "SMSG_TAXINODE_STATUS                           0x169E  DORMANT"
        "reply reference")
elseif(MUTATION STREQUAL "handler_parser")
    mutate(taxi_handler "MopTaxiPackets::ParseStatusQuery(recv_data, guid)"
        "LegacyTaxiStatusParser(recv_data, guid)" "handler parser")
elseif(MUTATION STREQUAL "query_producer")
    mutate(taxi_handler
        "MopTaxiPackets::BuildStatusBody(data, guid,\n        MopTaxiPackets::StatusForKnown(\n            GetPlayer()->m_taxi.IsTaximaskNodeKnown(curloc)));"
        "data << ObjectGuid(guid) << uint8(1);" "query producer")
elseif(MUTATION STREQUAL "learn_producer")
    mutate(taxi_handler
        "MopTaxiPackets::BuildStatusBody(update, unit->GetObjectGuid(),\n            MopTaxiPackets::TaxiNodeStatus::Learned);"
        "update << unit->GetObjectGuid() << uint8(1);" "learn producer")
elseif(MUTATION STREQUAL "spell_producer")
    mutate(spell_effect_tail
        "MopTaxiPackets::BuildStatusBody(data, m_caster->GetObjectGuid(),\n            MopTaxiPackets::TaxiNodeStatus::Learned);"
        "data << m_caster->GetObjectGuid() << uint8(1);" "spell producer")
elseif(MUTATION STREQUAL "exact_tail")
    mutate(player_header "remaining != 1 + guidByteCount ||"
        "remaining < 1 + guidByteCount ||" "exact tail")
elseif(MUTATION STREQUAL "authority_lookup")
    mutate(taxi_handler "GetPlayer()->GetMap()->GetCreature(guid)"
        "GetPlayer()->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_FLIGHTMASTER)"
        "authority lookup")
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

string(FIND "${player_header}" "namespace MopTaxiPackets" packets_start)
string(FIND "${player_header}" "namespace MopDuelPackets" packets_end)
if(packets_start EQUAL -1 OR packets_end LESS_EQUAL packets_start)
    message(FATAL_ERROR "taxi packet seam guard: could not isolate helpers")
endif()
math(EXPR packets_length "${packets_end} - ${packets_start}")
string(SUBSTRING "${player_header}" ${packets_start} ${packets_length} packets)

string(FIND "${packets}" "inline bool ParseStatusQuery" status_parser_start)
string(FIND "${packets}" "inline bool ParseTaxiQueryAvailableNodes" status_parser_end)
if(status_parser_start EQUAL -1 OR status_parser_end LESS_EQUAL status_parser_start)
    message(FATAL_ERROR "status request parser seam guard: could not isolate helper")
endif()
math(EXPR status_parser_length "${status_parser_end} - ${status_parser_start}")
string(SUBSTRING "${packets}" ${status_parser_start} ${status_parser_length}
    status_request_parser)

require_once("${status_request_parser}" "ReadGuidMask<7, 4, 1, 3, 0, 5, 2, 6>"
    "request mask order")
require_text("${status_request_parser}"
    "in.ResetBitReader();\n        in.ReadGuidMask<7, 4, 1, 3, 0, 5, 2, 6>(parsed);"
    "request bit-reader reset")
require_once("${status_request_parser}" "ReadGuidBytes<7, 1, 5, 2, 4, 0, 6, 3>"
    "request byte order and XOR")
require_once("${status_request_parser}" "remaining != 1 [+] guidByteCount"
    "exact request body")
require_once("${status_request_parser}" "if [(]in[.]rpos[(][)] != in[.]size[(][)][)]"
    "exact request tail")
require_once("${packets}" "WriteGuidMask<6, 2, 7, 5, 4, 1>"
    "reply first mask order")
require_text("${packets}" "out.WriteBits(uint8(status), 2);"
    "reply status width")
require_once("${packets}" "WriteGuidMask<3, 0>"
    "reply second mask order")
require_once("${packets}" "WriteGuidBytes<0, 5, 2, 1, 4, 6, 7, 3>"
    "reply byte order and XOR")
string(FIND "${packets}" "out.WriteGuidMask<6, 2, 7, 5, 4, 1>(guid)"
    first_reply_mask)
string(FIND "${packets}" "out.WriteBits(uint8(status), 2)" status_bits)
string(FIND "${packets}" "out.WriteGuidMask<3, 0>(guid)" second_reply_mask)
if(first_reply_mask EQUAL -1 OR status_bits LESS_EQUAL first_reply_mask OR
        second_reply_mask LESS_EQUAL status_bits)
    message(FATAL_ERROR "reply status position guard: ordering is wrong")
endif()
require_text("${packets}"
    "known ? TaxiNodeStatus::Learned : TaxiNodeStatus::Unlearned"
    "known and unlearned semantics")
require_once("${packets}"
    "BuildStatusBody[(]WorldPacket& out, ObjectGuid guid,[\r\n ]+TaxiNodeStatus status[)]"
    "typed body-only reply builder")
if("${packets}" MATCHES "BuildStatusBody[^}]+Initialize[(]")
    message(FATAL_ERROR "body-only reply builder guard: opcode initialization found")
endif()

string(FIND "${taxi_handler}"
    "void WorldSession::HandleTaxiNodeStatusQueryOpcode" query_start)
string(FIND "${taxi_handler}" "void WorldSession::SendTaxiStatus" query_end)
string(FIND "${taxi_handler}" "void WorldSession::HandleTaxiQueryAvailableNodes" status_end)
if(query_start EQUAL -1 OR query_end LESS_EQUAL query_start OR
        status_end LESS_EQUAL query_end)
    message(FATAL_ERROR "taxi query/status seam guard: could not isolate handlers")
endif()
math(EXPR query_length "${query_end} - ${query_start}")
string(SUBSTRING "${taxi_handler}" ${query_start} ${query_length} query_handler)
math(EXPR status_length "${status_end} - ${query_end}")
string(SUBSTRING "${taxi_handler}" ${query_end} ${status_length} status_sender)

require_once("${query_handler}"
    "if [(][!]MopTaxiPackets::ParseStatusQuery[(]recv_data, guid[)][)]"
    "handler parser route")
require_once("${query_handler}" "SendTaxiStatus[(]guid[)]"
    "handler gameplay route")
string(FIND "${query_handler}"
    "MopTaxiPackets::ParseStatusQuery(recv_data, guid)" parse_position)
string(FIND "${query_handler}" "SendTaxiStatus(guid)" side_effect_position)
if(parse_position EQUAL -1 OR side_effect_position LESS_EQUAL parse_position)
    message(FATAL_ERROR "parse-before-side-effect guard: ordering is wrong")
endif()
require_once("${status_sender}" "GetPlayer[(][)]->GetMap[(][)]->GetCreature[(]guid[)]"
    "preserved creature authority lookup")
require_once("${status_sender}"
    "MopTaxiPackets::BuildStatusBody[(]data, guid,[\r\n ]+MopTaxiPackets::StatusForKnown"
    "query reply producer")

string(FIND "${taxi_handler}" "bool WorldSession::SendLearnNewTaxiNode" learn_start)
string(FIND "${taxi_handler}" "void WorldSession::SendActivateTaxiReply" learn_end)
if(learn_start EQUAL -1 OR learn_end LESS_EQUAL learn_start)
    message(FATAL_ERROR "taxi learn seam guard: could not isolate producer")
endif()
math(EXPR learn_length "${learn_end} - ${learn_start}")
string(SUBSTRING "${taxi_handler}" ${learn_start} ${learn_length} learn_sender)
require_once("${learn_sender}"
    "MopTaxiPackets::BuildStatusBody[(]update, unit->GetObjectGuid[(][)],[\r\n ]+MopTaxiPackets::TaxiNodeStatus::Learned[)]"
    "learned-node reply producer")

string(FIND "${spell_effect_tail}" "void Spell::EffectTeachTaxiNode" spell_start)
string(FIND "${spell_effect_tail}" "void Spell::EffectQuestOffer" spell_end)
if(spell_start EQUAL -1 OR spell_end LESS_EQUAL spell_start)
    message(FATAL_ERROR "spell taxi seam guard: could not isolate producer")
endif()
math(EXPR spell_length "${spell_end} - ${spell_start}")
string(SUBSTRING "${spell_effect_tail}" ${spell_start} ${spell_length} spell_sender)
require_once("${spell_sender}"
    "MopTaxiPackets::BuildStatusBody[(]data, m_caster->GetObjectGuid[(][)],[\r\n ]+MopTaxiPackets::TaxiNodeStatus::Learned[)]"
    "spell-learned reply producer")

foreach(sender IN ITEMS status_sender learn_sender spell_sender)
    if("${${sender}}" MATCHES "SMSG_TAXINODE_STATUS[^}]+<<[\r\n ]*(ObjectGuid|uint8)")
        message(FATAL_ERROR "${sender} legacy serializer guard: raw body remains")
    endif()
endforeach()

require_once("${opcode_header}"
    "CMSG_TAXINODE_STATUS_QUERY[\t ]*=[\t ]*0x02E1"
    "request opcode value")
require_once("${opcode_header}"
    "SMSG_TAXINODE_STATUS[\t ]*=[\t ]*0x169E"
    "reply opcode value")
require_once("${opcode_registry}"
    "DefC[(]CMSG_TAXINODE_STATUS_QUERY, \"CMSG_TAXINODE_STATUS_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTaxiNodeStatusQueryOpcode[)]"
    "request registration")
require_once("${opcode_registry}"
    "DefS[(]SMSG_TAXINODE_STATUS, \"SMSG_TAXINODE_STATUS\"[)]"
    "reply registration")
require_once("${session_source}" "case[\t ]+SMSG_TAXINODE_STATUS:"
    "reply suppression admission")
require_once("${opcode_reference}"
    "CMSG_TAXINODE_STATUS_QUERY[\t ]+0x02E1[\t ]+ACTIVE"
    "active request reference")
require_once("${opcode_reference}"
    "SMSG_TAXINODE_STATUS[\t ]+0x169E[\t ]+ACTIVE"
    "active reply reference")

message(STATUS "mop_taxi_status_packets_source: source checks passed")
