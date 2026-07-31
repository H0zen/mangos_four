file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerDeath.cpp" player_death)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerMirror.cpp" player_mirror)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit_source)
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectMgr.h" object_mgr_header)
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectMgrGraveyard.cpp" object_mgr_graveyard)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp" misc_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellPackets.cpp" spell_packets)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GridMap.h" grid_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GridMap.cpp" grid_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

if(MUTATION STREQUAL "wire_order")
    string(REPLACE "out << mapId << y << x << z;" "out << mapId << x << y << z;"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "player_call")
    string(REPLACE "MopDeathPackets::BuildDeathReleaseLocation(data," "LegacyDeathReleaseLocation(data,"
        player_death "${player_death}")
elseif(MUTATION STREQUAL "misc_call")
    string(REPLACE "MopDeathPackets::BuildDeathReleaseLocation(data," "LegacyDeathReleaseLocation(data,"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefS(SMSG_DEATH_RELEASE_LOC, \"SMSG_DEATH_RELEASE_LOC\");"
        "/* removed death-release registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "whitelist")
    string(REPLACE
        "case SMSG_DEATH_RELEASE_LOC:"
        "/* removed death-release whitelist */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode_value")
    string(REPLACE
        "SMSG_DEATH_RELEASE_LOC                       = 0x1063"
        "SMSG_DEATH_RELEASE_LOC                       = 0x1064"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "cemetery_wire")
    string(REPLACE
        "out.WriteBits(uint32(count), 22);"
        "/* removed cemetery count */"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "cemetery_bound")
    string(REPLACE
        "cemeteryIds.size() > CEMETERY_LIST_MAX ? CEMETERY_LIST_MAX : cemeteryIds.size()"
        "cemeteryIds.size()"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "cemetery_query")
    string(REPLACE
        "sObjectMgr.GetGraveYardIds(GetPlayer()->GetZoneId(), GetPlayer()->GetTeam(), MopDeathPackets::CEMETERY_LIST_MAX)"
        "std::vector<uint32>()"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "cemetery_serializer")
    string(REPLACE
        "MopDeathPackets::BuildCemeteryListResponse(data, cemeteryIds, false);"
        "/* removed cemetery serializer */"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "cemetery_registration")
    string(REPLACE
        "DefC(CMSG_REQUEST_CEMETERY_LIST, \"CMSG_REQUEST_CEMETERY_LIST\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleRequestCemeteryListOpcode);"
        "/* removed cemetery request registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "cemetery_whitelist")
    string(REPLACE
        "case SMSG_REQUEST_CEMETERY_LIST_RESPONSE:"
        "/* removed cemetery response whitelist */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "cemetery_opcode")
    string(REPLACE
        "CMSG_REQUEST_CEMETERY_LIST                   = 0x06E4"
        "CMSG_REQUEST_CEMETERY_LIST                   = 0x06E5"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "cemetery_team_filter")
    string(REPLACE
        "data.team != TEAM_BOTH_ALLOWED && data.team != team"
        "data.team != team"
        object_mgr_graveyard "${object_mgr_graveyard}")
elseif(MUTATION STREQUAL "durability_death_body")
    string(REPLACE
        "out.Initialize(SMSG_DURABILITY_DAMAGE_DEATH, 0);"
        "out.Initialize(SMSG_DURABILITY_DAMAGE_DEATH, 1); out << uint8(0);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "durability_death_mirror_sender")
    string(REPLACE
        "MopDeathPackets::BuildDurabilityDamageDeath(data2);"
        "/* removed fall-death durability sender */"
        player_mirror "${player_mirror}")
elseif(MUTATION STREQUAL "durability_death_unit_sender")
    string(REPLACE
        "MopDeathPackets::BuildDurabilityDamageDeath(data);"
        "/* removed combat-death durability sender */"
        unit_source "${unit_source}")
elseif(MUTATION STREQUAL "durability_death_registration")
    string(REPLACE
        "DefS(SMSG_DURABILITY_DAMAGE_DEATH, \"SMSG_DURABILITY_DAMAGE_DEATH\");"
        "/* removed durability-death registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "durability_death_whitelist")
    string(REPLACE
        "case SMSG_DURABILITY_DAMAGE_DEATH:"
        "/* removed durability-death whitelist */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "durability_death_reference")
    string(REPLACE
        "SMSG_DURABILITY_DAMAGE_DEATH                   0x1E3E  ACTIVE"
        "SMSG_DURABILITY_DAMAGE_DEATH                   0x1E3E  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "reclaim_mask_order")
    string(REPLACE "ReadGuidMask<1, 5, 7, 2, 6, 3, 0, 4>" "ReadGuidMask<5, 1, 7, 2, 6, 3, 0, 4>"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "reclaim_byte_order")
    string(REPLACE "ReadGuidBytes<2, 5, 4, 6, 1, 0, 7, 3>" "ReadGuidBytes<5, 2, 4, 6, 1, 0, 7, 3>"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "reclaim_handler_parser")
    string(REPLACE "MopDeathPackets::ParseReclaimCorpseRequest(recv_data, corpseGuid)" "LegacyReclaimParser(recv_data, corpseGuid)"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "resurrect_response_order")
    string(REPLACE "in >> parsed.response;" "/* removed 32-bit response prefix */"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "resurrect_response_mask_order")
    string(REPLACE "ReadGuidMask<3, 0, 6, 4, 5, 2, 1, 7>" "ReadGuidMask<0, 3, 6, 4, 5, 2, 1, 7>"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "resurrect_response_byte_order")
    string(REPLACE "ReadGuidBytes<7, 0, 1, 3, 4, 6, 2, 5>" "ReadGuidBytes<0, 7, 1, 3, 4, 6, 2, 5>"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "resurrect_request_mask_order")
    string(REPLACE "WriteGuidMask<1, 5, 2, 6, 0, 4, 7>" "WriteGuidMask<5, 1, 2, 6, 0, 4, 7>"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "resurrect_request_byte_order")
    string(REPLACE "WriteGuidBytes<2, 4, 1, 6, 0>" "WriteGuidBytes<4, 2, 1, 6, 0>"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "resurrect_name_truncation")
    string(REPLACE "TruncateUtf8Bytes(offererName, 48)" "offererName"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "resurrect_producer")
    string(REPLACE "MopDeathPackets::BuildResurrectRequest(data," "LegacyBuildResurrectRequest(data,"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "resurrect_producer_flags")
    string(REPLACE "false, !isPlayer, 0, isPlayer ? realmID : 0" "!isPlayer, false, 0, isPlayer ? realmID : 0"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "resurrect_producer_context")
    string(REPLACE "false, !isPlayer, 0, isPlayer ? realmID : 0" "false, !isPlayer, 0, 0"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "resurrect_request_admission")
    string(REPLACE "case SMSG_RESURRECT_REQUEST:" "/* removed resurrect-request admission */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "resurrect_accept_polarity")
    string(REPLACE "if (response.response != 0)" "if (response.response == 0)"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "reclaim_registration")
    string(REPLACE "DefC(CMSG_RECLAIM_CORPSE," "RemovedDefC(CMSG_RECLAIM_CORPSE,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "resurrect_registration")
    string(REPLACE "DefC(CMSG_RESURRECT_RESPONSE," "RemovedDefC(CMSG_RESURRECT_RESPONSE,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "return_graveyard_registration")
    string(REPLACE "DefC(CMSG_RETURN_TO_GRAVEYARD," "RemovedDefC(CMSG_RETURN_TO_GRAVEYARD,"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "return_graveyard_corpse_guard")
    string(REPLACE "Corpse* corpse = pPlayer->GetCorpse();" "Corpse* corpse = NULL;"
        misc_handler "${misc_handler}")
elseif(MUTATION STREQUAL "nested_zone_parent_walk")
    string(REPLACE "while (entry->ParentAreaID != 0 && visitedCount < AREA_PARENT_LIMIT)" "while (false)"
        grid_header "${grid_header}")
elseif(MUTATION STREQUAL "nested_zone_api_delegate")
    string(REPLACE "MopTerrain::ResolveRootAreaId(entry," "LegacyImmediateParent(entry,"
        grid_source "${grid_source}")
elseif(MUTATION STREQUAL "death_recovery_reference")
    string(REPLACE "CMSG_RESURRECT_RESPONSE                        0x0B0C  ACTIVE" "CMSG_RESURRECT_RESPONSE                        0x0B0C  DORMANT"
        opcode_reference "${opcode_reference}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${player_header}"
    "out << mapId << y << x << z"
    "death-release 18414 field order")

require_once("${player_header}"
    "out\\.Initialize\\(SMSG_DURABILITY_DAMAGE_DEATH, 0\\)"
    "empty durability-death serializer")
require_once("${player_mirror}"
    "MopDeathPackets::BuildDurabilityDamageDeath\\(data2\\)"
    "fall-death durability sender")
require_once("${unit_source}"
    "MopDeathPackets::BuildDurabilityDamageDeath\\(data\\)"
    "combat-death durability sender")
require_once("${opcode_registry}"
    "DefS\\(SMSG_DURABILITY_DAMAGE_DEATH,[ \t]*\"SMSG_DURABILITY_DAMAGE_DEATH\"\\)"
    "durability-death registration")
require_once("${session_source}"
    "case[ \t]+SMSG_DURABILITY_DAMAGE_DEATH:"
    "durability-death suppression whitelist")
require_once("${opcode_reference}"
    "SMSG_DURABILITY_DAMAGE_DEATH[ \t]+0x1E3E[ \t]+ACTIVE"
    "active direct-client durability-death reference")
foreach(source IN ITEMS player_mirror unit_source)
    if("${${source}}" MATCHES "WorldPacket[ \t]+[A-Za-z0-9_]+\\(SMSG_DURABILITY_DAMAGE_DEATH")
        message(FATAL_ERROR "live inline durability-death sender remains in ${source}")
    endif()
endforeach()

string(REGEX MATCHALL
    "MopDeathPackets::BuildDeathReleaseLocation\\(data,"
    player_calls "${player_death}")
list(LENGTH player_calls player_call_count)
if(NOT player_call_count EQUAL 2)
    message(FATAL_ERROR "expected two Player death-release builder calls, found ${player_call_count}")
endif()

string(REGEX MATCHALL
    "MopDeathPackets::BuildDeathReleaseLocation\\(data,"
    misc_calls "${misc_handler}")
list(LENGTH misc_calls misc_call_count)
if(NOT misc_call_count EQUAL 1)
    message(FATAL_ERROR "expected one repop death-release builder call, found ${misc_call_count}")
endif()

foreach(source IN ITEMS player_death misc_handler)
    if("${${source}}" MATCHES "WorldPacket[ \t]+data\\(SMSG_DEATH_RELEASE_LOC")
        message(FATAL_ERROR "live inline death-release sender remains in ${source}")
    endif()
endforeach()

require_once("${opcode_registry}"
    "DefS\\(SMSG_DEATH_RELEASE_LOC,[ \t]*\"SMSG_DEATH_RELEASE_LOC\"\\)"
    "death-release registration")
require_once("${session_source}"
    "case[ \t]+SMSG_DEATH_RELEASE_LOC:"
    "death-release suppression whitelist")
require_once("${opcode_header}"
    "SMSG_DEATH_RELEASE_LOC[ \t]*=[ \t]*0x1063"
    "death-release opcode")

require_once("${player_header}"
    "static size_t const CEMETERY_LIST_MAX[ \t]*=[ \t]*16"
    "cemetery-list client bound")
require_once("${player_header}"
    "void BuildCemeteryListResponse\\(WorldPacket& out,[ \t\r\n]*std::vector<uint32> const& cemeteryIds,[ \t\r\n]*bool isGossipTriggered\\)"
    "cemetery-list serializer")

string(FIND "${player_header}" "inline void BuildCemeteryListResponse" cemetery_serializer_start)
string(FIND "${player_header}" "namespace MopBattlePetPackets" cemetery_serializer_end)
if(cemetery_serializer_start EQUAL -1 OR cemetery_serializer_end EQUAL -1 OR
        cemetery_serializer_end LESS_EQUAL cemetery_serializer_start)
    message(FATAL_ERROR "could not isolate cemetery-list serializer")
endif()
math(EXPR cemetery_serializer_length "${cemetery_serializer_end} - ${cemetery_serializer_start}")
string(SUBSTRING "${player_header}" ${cemetery_serializer_start}
    ${cemetery_serializer_length} cemetery_serializer)

require_once("${cemetery_serializer}"
    "cemeteryIds\\.size\\(\\) > CEMETERY_LIST_MAX \\? CEMETERY_LIST_MAX : cemeteryIds\\.size\\(\\)"
    "cemetery-list serializer bound")
require_once("${cemetery_serializer}"
    "out\\.Initialize\\(SMSG_REQUEST_CEMETERY_LIST_RESPONSE,"
    "cemetery-list response opcode")
require_once("${cemetery_serializer}"
    "out\\.WriteBits\\(uint32\\(count\\), 22\\)"
    "cemetery-list 22-bit count")
require_once("${cemetery_serializer}"
    "out\\.WriteBit\\(isGossipTriggered\\)"
    "cemetery-list gossip bit")
require_once("${cemetery_serializer}"
    "out\\.FlushBits\\(\\)"
    "cemetery-list byte alignment")
require_once("${cemetery_serializer}"
    "out << cemeteryIds\\[i\\]"
    "cemetery-list ids")

require_once("${object_mgr_header}"
    "std::vector<uint32> GetGraveYardIds\\(uint32 zoneId, Team team, size_t maxCount\\) const"
    "bounded graveyard query declaration")
require_once("${object_mgr_graveyard}"
    "std::vector<uint32> ObjectMgr::GetGraveYardIds\\(uint32 zoneId, Team team, size_t maxCount\\) const"
    "bounded graveyard query definition")

string(FIND "${object_mgr_graveyard}" "std::vector<uint32> ObjectMgr::GetGraveYardIds" graveyard_query_start)
string(FIND "${object_mgr_graveyard}" "bool ObjectMgr::AddGraveYardLink" graveyard_query_end)
if(graveyard_query_start EQUAL -1 OR graveyard_query_end EQUAL -1 OR
        graveyard_query_end LESS_EQUAL graveyard_query_start)
    message(FATAL_ERROR "could not isolate bounded graveyard query")
endif()
math(EXPR graveyard_query_length "${graveyard_query_end} - ${graveyard_query_start}")
string(SUBSTRING "${object_mgr_graveyard}" ${graveyard_query_start}
    ${graveyard_query_length} graveyard_query)

require_once("${graveyard_query}"
    "mGraveYardMap\\.equal_range\\(zoneId\\)"
    "graveyard-zone selection")
require_once("${graveyard_query}"
    "data\\.team != TEAM_BOTH_ALLOWED && data\\.team != team"
    "neutral and same-team graveyard selection")
require_once("${graveyard_query}"
    "result\\.size\\(\\) == maxCount"
    "graveyard query bound")

require_once("${session_header}"
    "void HandleRequestCemeteryListOpcode\\(WorldPacket& recv_data\\)"
    "cemetery-list session declaration")
require_once("${misc_handler}"
    "void WorldSession::HandleRequestCemeteryListOpcode\\(WorldPacket& /\\*recv_data\\*/\\)"
    "empty cemetery-list request handler")

string(FIND "${misc_handler}" "void WorldSession::HandleRequestCemeteryListOpcode" cemetery_handler_start)
string(FIND "${misc_handler}" "void WorldSession::HandleRepopRequestOpcode" cemetery_handler_end)
if(cemetery_handler_start EQUAL -1 OR cemetery_handler_end EQUAL -1 OR
        cemetery_handler_end LESS_EQUAL cemetery_handler_start)
    message(FATAL_ERROR "could not isolate cemetery-list request handler")
endif()
math(EXPR cemetery_handler_length "${cemetery_handler_end} - ${cemetery_handler_start}")
string(SUBSTRING "${misc_handler}" ${cemetery_handler_start}
    ${cemetery_handler_length} cemetery_handler)

require_once("${cemetery_handler}"
    "sObjectMgr\\.GetGraveYardIds\\(GetPlayer\\(\\)->GetZoneId\\(\\), GetPlayer\\(\\)->GetTeam\\(\\), MopDeathPackets::CEMETERY_LIST_MAX\\)"
    "handler bounded graveyard query")
require_once("${cemetery_handler}"
    "MopDeathPackets::BuildCemeteryListResponse\\(data, cemeteryIds, false\\)"
    "handler scheduled-response serializer")
require_once("${cemetery_handler}"
    "SendPacket\\(&data\\)"
    "handler deterministic response send")

require_once("${opcode_registry}"
    "DefC\\(CMSG_REQUEST_CEMETERY_LIST,[ \t]*\"CMSG_REQUEST_CEMETERY_LIST\",[ \t]*STATUS_LOGGEDIN,[ \t]*PROCESS_THREADUNSAFE,[ \t]*&WorldSession::HandleRequestCemeteryListOpcode\\)"
    "cemetery-list request registration")
require_once("${opcode_registry}"
    "DefS\\(SMSG_REQUEST_CEMETERY_LIST_RESPONSE,[ \t]*\"SMSG_REQUEST_CEMETERY_LIST_RESPONSE\"\\)"
    "cemetery-list response registration")
require_once("${session_source}"
    "case[ \t]+SMSG_REQUEST_CEMETERY_LIST_RESPONSE:"
    "cemetery-list response suppression whitelist")
require_once("${opcode_header}"
    "CMSG_REQUEST_CEMETERY_LIST[ \t]*=[ \t]*0x06E4"
    "cemetery-list request opcode")
require_once("${opcode_header}"
    "SMSG_REQUEST_CEMETERY_LIST_RESPONSE[ \t]*=[ \t]*0x042A"
    "cemetery-list response opcode")

require_once("${player_header}"
    "ReadGuidMask<1, 5, 7, 2, 6, 3, 0, 4>"
    "corpse-reclaim GUID mask order")
require_once("${player_header}"
    "ReadGuidBytes<2, 5, 4, 6, 1, 0, 7, 3>"
    "corpse-reclaim GUID byte order")
require_once("${misc_handler}"
    "MopDeathPackets::ParseReclaimCorpseRequest[(]recv_data, corpseGuid[)]"
    "corpse-reclaim converted parser")

require_once("${player_header}"
    "in >> parsed[.]response"
    "resurrection-response 32-bit prefix")
require_once("${player_header}"
    "ReadGuidMask<3, 0, 6, 4, 5, 2, 1, 7>"
    "resurrection-response GUID mask order")
require_once("${player_header}"
    "ReadGuidBytes<7, 0, 1, 3, 4, 6, 2, 5>"
    "resurrection-response GUID byte order")
require_once("${player_header}"
    "out << context0 << context1 << spellId"
    "resurrection-request scalar prefix")
require_once("${player_header}"
    "TruncateUtf8Bytes[(]offererName, 48[)]"
    "resurrection-request UTF-8 byte bound")
require_once("${player_header}"
    "out[.]WriteGuidMask<1, 5, 2, 6, 0, 4, 7>[(]offererGuid[)]"
    "resurrection-request GUID mask order")
require_once("${player_header}"
    "out[.]WriteGuidBytes<2, 4, 1, 6, 0>[(]offererGuid[)]"
    "resurrection-request GUID byte order")
require_once("${spell_packets}"
    "MopDeathPackets::BuildResurrectRequest[(]data,"
    "resurrection-request producer migration")
require_once("${spell_packets}"
    "false, !isPlayer, 0, isPlayer [?] realmID : 0"
    "resurrection-request flags and realm context")
require_once("${session_source}"
    "case[ \t]+SMSG_RESURRECT_REQUEST:"
    "resurrection-request in-world admission")
require_once("${misc_handler}"
    "if [(]response[.]response != 0[)]"
    "resurrection accept polarity")

string(FIND "${misc_handler}" "void WorldSession::HandleResurrectResponseOpcode" resurrect_handler_start)
string(FIND "${misc_handler}" "void WorldSession::HandleReturnToGraveyard" resurrect_handler_end)
if(resurrect_handler_start EQUAL -1 OR resurrect_handler_end LESS_EQUAL resurrect_handler_start)
    message(FATAL_ERROR "could not isolate resurrection-response handler")
endif()
math(EXPR resurrect_handler_length "${resurrect_handler_end} - ${resurrect_handler_start}")
string(SUBSTRING "${misc_handler}" ${resurrect_handler_start}
    ${resurrect_handler_length} resurrect_handler)
string(FIND "${resurrect_handler}" "response.response != 0" decline_pos)
string(FIND "${resurrect_handler}" "clearResurrectRequestData" clear_pos)
string(FIND "${resurrect_handler}" "GetPlayer()->IsAlive()" alive_pos)
string(FIND "${resurrect_handler}" "isRessurectRequestedBy" offerer_pos)
if(decline_pos EQUAL -1 OR clear_pos LESS decline_pos OR alive_pos LESS clear_pos OR
        offerer_pos LESS alive_pos)
    message(FATAL_ERROR "resurrection response must clear decline/timeout before gating accepted offers")
endif()

foreach(registration IN ITEMS
        "CMSG_RECLAIM_CORPSE.*HandleReclaimCorpseOpcode"
        "CMSG_RESURRECT_RESPONSE.*HandleResurrectResponseOpcode"
        "CMSG_RETURN_TO_GRAVEYARD.*HandleReturnToGraveyard")
    require_once("${opcode_registry}"
        "[ \t\r\n]+DefC[(]${registration}[)]"
        "death-recovery request registration")
endforeach()
foreach(reference IN ITEMS
        "CMSG_RECLAIM_CORPSE[ \t]+0x03D3[ \t]+ACTIVE"
        "CMSG_RESURRECT_RESPONSE[ \t]+0x0B0C[ \t]+ACTIVE"
        "CMSG_RETURN_TO_GRAVEYARD[ \t]+0x12EA[ \t]+ACTIVE")
    require_once("${opcode_reference}" "${reference}"
        "active death-recovery reference")
endforeach()

require_once("${misc_handler}"
    "Corpse[*] corpse = pPlayer->GetCorpse[(][)]"
    "return-to-graveyard corpse guard")
require_once("${misc_handler}"
    "GetClosestGraveYard[(]corpse->GetPositionX[(][)], corpse->GetPositionY[(][)], corpse->GetPositionZ[(][)], corpse->GetMapId[(][)], pPlayer->GetTeam[(][)][)]"
    "return-to-graveyard corpse-position selection")

require_once("${grid_header}"
    "while [(]entry->ParentAreaID != 0 && visitedCount < AREA_PARENT_LIMIT[)]"
    "bounded nested-area parent traversal")
string(REGEX MATCHALL "MopTerrain::ResolveRootAreaId[(]entry," zone_delegate_calls "${grid_source}")
list(LENGTH zone_delegate_calls zone_delegate_count)
if(NOT zone_delegate_count EQUAL 2)
    message(FATAL_ERROR "both terrain zone APIs must use root-area resolution; found ${zone_delegate_count}")
endif()
