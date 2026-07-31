file(READ "${SOURCE_ROOT}/src/game/Object/Guild.h" packet_builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" login_sender)
file(READ "${SOURCE_ROOT}/src/game/Object/Guild.cpp" guild_sender)
file(READ "${SOURCE_ROOT}/src/game/Object/GuildBank.cpp" guild_bank_sender)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" world_session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/NPCHandler.cpp" npc_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GuildHandler.cpp" guild_handler)

if(MUTATION STREQUAL "login_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildMotd(data, guild->GetMOTD())"
        "false /* removed login MOTD builder */"
        login_sender "${login_sender}")
elseif(MUTATION STREQUAL "invite_realm_suffix")
    string(REPLACE
        "    StripHomeRealmSuffix(Invitedname);\n\n    if (normalizePlayerName(Invitedname))"
        "    if (normalizePlayerName(Invitedname))"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "broadcast_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildMotd(data, motd)"
        "false /* removed broadcast MOTD builder */"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "outbound_registration")
    string(REPLACE
        "DefS(SMSG_GUILD_EVENT_MOTD, \"SMSG_GUILD_EVENT_MOTD\");"
        "/* removed guild MOTD registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "opcode_value")
    string(REPLACE
        "SMSG_GUILD_EVENT_MOTD                        = 0x0B68"
        "SMSG_GUILD_EVENT_MOTD                        = 0x0B69"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "tabard_reader")
    string(REPLACE
        "MopGuildPackets::ReadTabardVendorActivate(recv_data)"
        "uint64(0) /* removed tabard request reader */"
        npc_handler "${npc_handler}")
elseif(MUTATION STREQUAL "tabard_builder")
    string(REPLACE
        "MopGuildPackets::BuildTabardVendorActivate(data, guid.GetRawValue())"
        "/* removed tabard response builder */"
        npc_handler "${npc_handler}")
elseif(MUTATION STREQUAL "save_reader")
    string(REPLACE
        "MopGuildPackets::ReadSaveGuildEmblem(recvPacket)"
        "MopGuildPackets::EmblemDesign{} /* removed emblem request reader */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "save_builder")
    string(REPLACE
        "MopGuildPackets::BuildSaveGuildEmblemResult(data, msg)"
        "/* removed emblem result builder */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "tabard_registration")
    string(REPLACE
        "DefC(CMSG_TABARD_VENDOR_ACTIVATE, \"CMSG_TABARD_VENDOR_ACTIVATE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTabardVendorActivateOpcode);"
        "/* removed tabard request registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "save_registration")
    string(REPLACE
        "DefC(CMSG_SAVE_GUILD_EMBLEM, \"CMSG_SAVE_GUILD_EMBLEM\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSaveGuildEmblemOpcode);"
        "/* removed emblem request registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "tabard_allowlist")
    string(REPLACE
        "case SMSG_TABARD_VENDOR_ACTIVATE:"
        "case 0xFFFF: /* removed tabard response allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "design_bounds")
    string(REPLACE
        "design.emblemStyle >= 196"
        "false /* removed emblem design bounds */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "joined_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildMemberJoined(data"
        "MopGuildPackets::RemovedGuildMemberJoined(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "presence_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildPresenceChange(data"
        "MopGuildPackets::RemovedGuildPresenceChange(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "rank_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildMemberRankUpdate(data"
        "MopGuildPackets::RemovedGuildMemberRankUpdate(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "leader_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildNewLeader(data"
        "MopGuildPackets::RemovedGuildNewLeader(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "left_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildPlayerLeft(data"
        "MopGuildPackets::RemovedGuildPlayerLeft(data"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "disband_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildDisbanded(data)"
        "MopGuildPackets::RemovedGuildDisbanded(data)"
        guild_sender "${guild_sender}")
elseif(MUTATION STREQUAL "event_registration")
    string(REPLACE
        "DefS(SMSG_GUILD_EVENT_PLAYER_JOINED, \"SMSG_GUILD_EVENT_PLAYER_JOINED\");"
        "/* removed joined-event registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "event_allowlist")
    string(REPLACE
        "case SMSG_GUILD_EVENT_PLAYER_JOINED:"
        "case 0xFFFF: /* removed joined-event allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "event_opcode")
    string(REPLACE
        "SMSG_GUILD_EVENT_PLAYER_JOINED               = 0x0B69"
        "SMSG_GUILD_EVENT_PLAYER_JOINED               = 0x0B6A"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "presence_calls")
    string(REPLACE
        "guild->BroadcastMemberPresence(pCurrChar->GetObjectGuid(), pCurrChar->GetName(), true);"
        "/* removed login presence broadcast */"
        login_sender "${login_sender}")
elseif(MUTATION STREQUAL "removed_member_copy")
    string(REPLACE
        "ObjectGuid const removedGuid = slot->guid;"
        "ObjectGuid const removedGuid; /* removed pre-erase copy */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "legacy_info")
    string(APPEND opcode_header "\nWorldPacket legacy(SMSG_GUILD_INFO);\n")
elseif(MUTATION STREQUAL "legacy_event")
    string(APPEND guild_sender "\nWorldPacket legacy(SMSG_GUILD_EVENT);\n")
elseif(MUTATION STREQUAL "money_client_registration")
    string(REPLACE
        "DefC(CMSG_GUILD_BANK_MONEY_WITHDRAWN, \"CMSG_GUILD_BANK_MONEY_WITHDRAWN\""
        "/* removed guild-bank money client registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "money_server_registration")
    string(REPLACE
        "DefS(SMSG_GUILD_BANK_MONEY_WITHDRAWN, \"SMSG_GUILD_BANK_MONEY_WITHDRAWN\");"
        "/* removed guild-bank money server registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "money_gate")
    string(REPLACE
        "case SMSG_GUILD_BANK_MONEY_WITHDRAWN:"
        "/* removed guild-bank money framing gate */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "command_builder")
    string(REPLACE
        "MopGuildPackets::BuildGuildCommandResult(data, typecmd, str, cmdresult)"
        "false /* removed guild-command-result builder */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "command_registration")
    string(REPLACE
        "DefS(SMSG_GUILD_COMMAND_RESULT, \"SMSG_GUILD_COMMAND_RESULT\");"
        "/* removed guild-command-result registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "command_allowlist")
    string(REPLACE
        "case SMSG_GUILD_COMMAND_RESULT:"
        "case 0xFFFF: /* removed guild-command-result allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "command_opcode")
    string(REPLACE
        "SMSG_GUILD_COMMAND_RESULT                    = 0x0EF1"
        "SMSG_GUILD_COMMAND_RESULT                    = 0x0EF0"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "invite_reader")
    string(REPLACE
        "MopGuildPackets::ReadGuildInvite(recvPacket, Invitedname)"
        "false /* removed guild-invite reader */"
        guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "invite_registration")
    string(REPLACE
        "DefC(CMSG_GUILD_INVITE, \"CMSG_GUILD_INVITE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGuildInviteOpcode);"
        "/* removed guild-invite registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "invite_length_width")
    string(REPLACE
        "uint32 const nameLength = in.ReadBits(9);"
        "uint32 const nameLength = in.ReadBits(7);"
        packet_builder "${packet_builder}")
elseif(MUTATION STREQUAL "invite_opcode")
    string(REPLACE
        "CMSG_GUILD_INVITE                            = 0x0869"
        "CMSG_GUILD_INVITE                            = 0x0868"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "tracking_count_bits_21")
    string(REPLACE
        "uint32 const count = in.ReadBits(22);"
        "uint32 const count = in.ReadBits(21);"
        packet_builder "${packet_builder}")
elseif(MUTATION STREQUAL "tracking_count_bits_24")
    string(REPLACE
        "uint32 const count = in.ReadBits(22);"
        "uint32 const count = in.ReadBits(24);"
        packet_builder "${packet_builder}")
elseif(MUTATION STREQUAL "tracking_ids_big_endian")
    string(REPLACE
        "in >> achievementId; // little-endian uint32"
        "in >> achievementId;
            achievementId = ((achievementId & 0x000000FFu) << 24) |
                ((achievementId & 0x0000FF00u) << 8) |
                ((achievementId & 0x00FF0000u) >> 8) |
                ((achievementId & 0xFF000000u) >> 24); // mutated big-endian scalar"
        packet_builder "${packet_builder}")
elseif(MUTATION STREQUAL "tracking_remove_count_limit")
    string(REPLACE
        "if (count > 10)"
        "if (false /* removed client count limit */)"
        packet_builder "${packet_builder}")
elseif(MUTATION STREQUAL "tracking_allow_truncated")
    string(REPLACE
        "if (remaining < expected)"
        "if (false /* allowed truncated body */)"
        packet_builder "${packet_builder}")
elseif(MUTATION STREQUAL "tracking_allow_trailing")
    string(REPLACE
        "if (remaining > expected)"
        "if (false /* allowed trailing body */)"
        packet_builder "${packet_builder}")
elseif(MUTATION STREQUAL "tracking_wrong_opcode_value")
    string(REPLACE
        "CMSG_GUILD_SET_ACHIEVEMENT_TRACKING          = 0x0CF0"
        "CMSG_GUILD_SET_ACHIEVEMENT_TRACKING          = 0x0CF1"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "tracking_remove_defc")
    string(REPLACE
        "DefC(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, \"CMSG_GUILD_SET_ACHIEVEMENT_TRACKING\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGuildSetAchievementTracking);"
        "/* removed guild-achievement-tracking registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "tracking_wrong_handler")
    string(REPLACE
        "&WorldSession::HandleGuildSetAchievementTracking);"
        "&WorldSession::Handle_NULL);"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "tracking_wrong_reference_state")
    string(REPLACE
        "CMSG_GUILD_SET_ACHIEVEMENT_TRACKING            0x0CF0  ACTIVE"
        "CMSG_GUILD_SET_ACHIEVEMENT_TRACKING            0x0CF0  DOC"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "tracking_add_persistence")
    string(REPLACE
        "(void)achievementIds;"
        "GetPlayer()->SaveToDB(); // mutated persistence side effect"
        guild_handler "${guild_handler}")
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
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once("${login_sender}"
    "MopGuildPackets::BuildGuildMotd(data, guild->GetMOTD())"
    "login MOTD builder call")
require_once("${guild_sender}"
    "MopGuildPackets::BuildGuildMotd(data, motd)"
    "broadcast MOTD builder call")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_EVENT_MOTD, \"SMSG_GUILD_EVENT_MOTD\");"
    "guild MOTD registration")
require_once("${opcode_header}"
    "SMSG_GUILD_EVENT_MOTD                        = 0x0B68"
    "guild MOTD opcode value")
require_once("${npc_handler}"
    "MopGuildPackets::ReadTabardVendorActivate(recv_data)"
    "tabard-vendor request reader")
require_once("${npc_handler}"
    "MopGuildPackets::BuildTabardVendorActivate(data, guid.GetRawValue())"
    "tabard-vendor response builder")
require_once("${guild_handler}"
    "MopGuildPackets::ReadSaveGuildEmblem(recvPacket)"
    "guild-emblem request reader")
require_once("${guild_handler}"
    "MopGuildPackets::BuildSaveGuildEmblemResult(data, msg)"
    "guild-emblem result builder")
require_once("${opcode_registry}"
    "DefC(CMSG_TABARD_VENDOR_ACTIVATE, \"CMSG_TABARD_VENDOR_ACTIVATE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTabardVendorActivateOpcode);"
    "tabard-vendor request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_TABARD_VENDOR_ACTIVATE, \"SMSG_TABARD_VENDOR_ACTIVATE\");"
    "tabard-vendor response registration")
require_once("${opcode_registry}"
    "DefC(CMSG_SAVE_GUILD_EMBLEM, \"CMSG_SAVE_GUILD_EMBLEM\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSaveGuildEmblemOpcode);"
    "guild-emblem request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_SAVE_GUILD_EMBLEM, \"SMSG_SAVE_GUILD_EMBLEM\");"
    "guild-emblem result registration")
require_once("${world_session}"
    "case SMSG_TABARD_VENDOR_ACTIVATE:"
    "tabard-vendor response allowlist")
require_once("${world_session}"
    "case SMSG_SAVE_GUILD_EMBLEM:"
    "guild-emblem result allowlist")
require_once("${opcode_registry}"
    "DefC(CMSG_GUILD_BANK_MONEY_WITHDRAWN, \"CMSG_GUILD_BANK_MONEY_WITHDRAWN\""
    "guild-bank money client registration")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_BANK_MONEY_WITHDRAWN, \"SMSG_GUILD_BANK_MONEY_WITHDRAWN\");"
    "guild-bank money server registration")
require_once("${guild_bank_sender}"
    "data << uint64(GetMemberMoneyWithdrawRem(LowGuid));"
    "guild-bank money uint64 body")
require_once("${world_session}"
    "case SMSG_GUILD_BANK_MONEY_WITHDRAWN:"
    "guild-bank money framing gate")

# The guild read-only queries. Registering the request is not enough on its own:
# WorldSession::SendPacket drops any opcode missing from IsEnterWorldConverted, so a
# reply whose grammar is proven but whose opcode is not admitted is silently discarded
# in world. Both halves are pinned here.
require_once("${opcode_registry}"
    "DefC(CMSG_GUILD_PERMISSIONS, \"CMSG_GUILD_PERMISSIONS\""
    "guild-permissions request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_PERMISSIONS, \"SMSG_GUILD_PERMISSIONS\");"
    "guild-permissions response registration")
require_once("${world_session}"
    "case SMSG_GUILD_PERMISSIONS:"
    "guild-permissions response allowlist")
require_once("${opcode_registry}"
    "DefC(CMSG_GUILD_QUERY_RANKS, \"CMSG_GUILD_QUERY_RANKS\""
    "guild-rank-query request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_QUERY_RANKS_RESULT, \"SMSG_GUILD_QUERY_RANKS_RESULT\");"
    "guild-rank-query response registration")
require_once("${world_session}"
    "case SMSG_GUILD_QUERY_RANKS_RESULT:"
    "guild-rank-query response allowlist")
require_once("${packet_builder}"
    "out.WriteBits(uint32(ranks.size()), 17)"
    "guild-rank-query 17-bit rank count")
require_once("${packet_builder}"
    "out.WriteBits(GUILD_BANK_MAX_TABS, 21)"
    "guild-permissions 21-bit tab count")
# Byte fixtures call the builders directly, so on their own they would still pass if
# the production call sites were deleted or the rank adapter miswired. Pin the wiring.
require_once("${guild_handler}"
    "MopGuildPackets::BuildGuildPermissions(data, rankId,"
    "guild-permissions production builder call")
require_once("${guild_sender}"
    "MopGuildPackets::BuildGuildRanks(data, ranks);"
    "guild-rank-query production builder call")
require_once("${guild_sender}"
    "entry.bankMoneyPerDay = m_Ranks[i].BankMoneyPerDay;"
    "guild-rank-query money adapter field")
require_once("${guild_sender}"
    "entry.rights = m_Ranks[i].Rights;"
    "guild-rank-query rights adapter field")
require_once("${opcode_registry}"
    "DefC(CMSG_GUILD_ROSTER, \"CMSG_GUILD_ROSTER\""
    "guild-roster request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_ROSTER, \"SMSG_GUILD_ROSTER\");"
    "guild-roster response registration")
require_once("${world_session}"
    "case SMSG_GUILD_ROSTER:"
    "guild-roster response allowlist")
require_once("${packet_builder}"
    "out.WriteBits(uint32(members.size()), 17)"
    "guild-roster 17-bit member count")
require_once("${packet_builder}"
    "out.WriteBits(uint32(motd.length()), 10)"
    "guild-roster 10-bit motd length")
require_once("${packet_builder}"
    "out.WriteBits(uint32(info.length()), 11)"
    "guild-roster 11-bit info length")
require_once("${guild_sender}"
    "MopGuildPackets::BuildGuildRoster(data, roster, MOTD, GINFO, GetAccountsNumber(),"
    "guild-roster production builder call")
require_once("${guild_sender}"
    "entry.virtualRealm = realmID;"
    "guild-roster virtual realm adapter field")
require_once("${guild_sender}"
    "entry.gender = player ? player->getGender() : 0;"
    "guild-roster gender adapter field")
require_once("${guild_handler}"
    "MopGuildPackets::BuildGuildCommandResult(data, typecmd, str, cmdresult)"
    "guild-command-result builder call")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_COMMAND_RESULT, \"SMSG_GUILD_COMMAND_RESULT\");"
    "guild-command-result registration")
require_once("${world_session}"
    "case SMSG_GUILD_COMMAND_RESULT:"
    "guild-command-result framing gate")
require_once("${opcode_header}"
    "SMSG_GUILD_COMMAND_RESULT                    = 0x0EF1"
    "guild-command-result opcode value")
require_once("${guild_handler}"
    "MopGuildPackets::ReadGuildInvite(recvPacket, Invitedname)"
    "guild-invite request reader")
require_once("${opcode_registry}"
    "DefC(CMSG_GUILD_INVITE, \"CMSG_GUILD_INVITE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGuildInviteOpcode);"
    "guild-invite request registration")
require_once("${packet_builder}"
    "uint32 const nameLength = in.ReadBits(9);"
    "guild-invite 9-bit name length")
require_once("${opcode_header}"
    "CMSG_GUILD_INVITE                            = 0x0869"
    "guild-invite opcode value")
require_once("${packet_builder}"
    "uint32 const count = in.ReadBits(22);"
    "guild-achievement-tracking 22-bit count")
require_once("${packet_builder}"
    "if (count > 10)"
    "guild-achievement-tracking hostile-count guard")
require_once("${packet_builder}"
    "if (remaining < expected)"
    "guild-achievement-tracking truncated-body guard")
require_once("${packet_builder}"
    "if (remaining > expected)"
    "guild-achievement-tracking trailing-body guard")
require_once("${packet_builder}"
    "in >> achievementId; // little-endian uint32"
    "guild-achievement-tracking little-endian scalar read")
require_once("${guild_handler}"
    "MopGuildPackets::ReadGuildAchievementTracking(recvPacket, achievementIds)"
    "guild-achievement-tracking parser wiring")
require_once("${guild_handler}"
    "Intentionally no persistence or gameplay side effects."
    "guild-achievement-tracking compatibility-sink semantics")
require_once("${guild_handler}"
    "(void)achievementIds;"
    "guild-achievement-tracking no-persistence sink boundary")
require_once("${world_session_header}"
    "void HandleGuildSetAchievementTracking(WorldPacket& recvPacket);"
    "guild-achievement-tracking handler declaration")
require_once("${opcode_registry}"
    "DefC(CMSG_GUILD_SET_ACHIEVEMENT_TRACKING, \"CMSG_GUILD_SET_ACHIEVEMENT_TRACKING\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGuildSetAchievementTracking);"
    "guild-achievement-tracking request registration")
require_once("${opcode_header}"
    "CMSG_GUILD_SET_ACHIEVEMENT_TRACKING          = 0x0CF0"
    "guild-achievement-tracking opcode value")
require_once("${opcode_reference}"
    "CMSG_GUILD_SET_ACHIEVEMENT_TRACKING            0x0CF0  ACTIVE"
    "guild-achievement-tracking reference state")

foreach(token IN ITEMS
        "CMSG_TABARD_VENDOR_ACTIVATE                 = 0x11C3"
        "SMSG_TABARD_VENDOR_ACTIVATE                  = 0x0A3E"
        "CMSG_SAVE_GUILD_EMBLEM                      = 0x1D60"
        "SMSG_SAVE_GUILD_EMBLEM                      = 0x089F")
    string(FIND "${opcode_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "tabard opcode missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "motd.size() >= (size_t(1) << 10)"
        "out.WriteBits(motd.size(), 10)"
        "out.FlushBits()"
        "out.append(motd.data(), motd.size())")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "guild MOTD builder missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "uint8 const maskOrder[] = { 2, 1, 4, 6, 3, 5, 0, 7 }"
        "uint8 const byteOrder[] = { 7, 6, 2, 5, 0, 1, 4, 3 }"
        "uint8 const maskOrder[] = { 1, 5, 0, 7, 4, 6, 3, 2 }"
        "uint8 const byteOrder[] = { 5, 4, 2, 3, 6, 0, 1, 7 }"
        "in >> design.borderStyle"
        "in >> design.backgroundColor"
        "in >> design.borderColor"
        "in >> design.emblemColor"
        "in >> design.emblemStyle"
        "uint8 const maskOrder[] = { 0, 7, 4, 6, 5, 1, 2, 3 }"
        "uint8 const byteOrder[] = { 6, 2, 7, 5, 0, 4, 1, 3 }"
        "out.Initialize(SMSG_SAVE_GUILD_EMBLEM, 4)")
    string(FIND "${packet_builder}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "tabard packet codec missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "design.emblemStyle >= 196"
        "design.emblemColor >= 17"
        "design.borderStyle >= 6"
        "design.borderColor >= 17"
        "design.backgroundColor >= 51"
        "SendSaveGuildEmblem(ERR_GUILDEMBLEM_INVALID_TABARD_COLORS)")
    string(FIND "${guild_handler}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "guild-emblem validation missing: ${token}")
    endif()
endforeach()

string(FIND "${login_sender}" "data.Initialize(SMSG_GUILD_EVENT," legacy_login)
if(NOT legacy_login EQUAL -1)
    message(FATAL_ERROR "login still sends the legacy generic guild event")
endif()

string(FIND "${npc_handler}" "MSG_TABARDVENDOR_ACTIVATE" legacy_tabard)
if(NOT legacy_tabard EQUAL -1)
    message(FATAL_ERROR "NPC handler still uses legacy tabard-vendor opcode")
endif()

string(FIND "${guild_handler}" "WorldPacket data(MSG_SAVE_GUILD_EMBLEM" legacy_emblem)
if(NOT legacy_emblem EQUAL -1)
    message(FATAL_ERROR "guild handler still uses legacy emblem opcode")
endif()

foreach(token IN ITEMS
        "MopGuildPackets::BuildGuildMemberJoined(data"
        "MopGuildPackets::BuildGuildPresenceChange(data"
        "MopGuildPackets::BuildGuildMemberRankUpdate(data"
        "MopGuildPackets::BuildGuildNewLeader(data"
        "MopGuildPackets::BuildGuildPlayerLeft(data"
        "MopGuildPackets::BuildGuildDisbanded(data)")
    string(FIND "${guild_sender}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "typed guild-event sender missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "DefS(SMSG_GUILD_EVENT_PLAYER_JOINED, \"SMSG_GUILD_EVENT_PLAYER_JOINED\");"
        "DefS(SMSG_GUILD_EVENT_PRESENCE_CHANGE, \"SMSG_GUILD_EVENT_PRESENCE_CHANGE\");"
        "DefS(SMSG_GUILD_EVENT_PLAYER_LEFT, \"SMSG_GUILD_EVENT_PLAYER_LEFT\");"
        "DefS(SMSG_GUILD_RANKS_UPDATE, \"SMSG_GUILD_RANKS_UPDATE\");"
        "DefS(SMSG_GUILD_EVENT_NEW_LEADER, \"SMSG_GUILD_EVENT_NEW_LEADER\");"
        "DefS(SMSG_GUILD_EVENT_DISBANDED, \"SMSG_GUILD_EVENT_DISBANDED\");")
    string(FIND "${opcode_registry}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "typed guild-event registration missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "case SMSG_GUILD_EVENT_PLAYER_JOINED:"
        "case SMSG_GUILD_EVENT_PRESENCE_CHANGE:"
        "case SMSG_GUILD_EVENT_PLAYER_LEFT:"
        "case SMSG_GUILD_RANKS_UPDATE:"
        "case SMSG_GUILD_EVENT_NEW_LEADER:"
        "case SMSG_GUILD_EVENT_DISBANDED:")
    string(FIND "${world_session}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "typed guild-event allowlist entry missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "SMSG_GUILD_RANKS_UPDATE                      = 0x0A60"
        "SMSG_GUILD_EVENT_PLAYER_JOINED               = 0x0B69"
        "SMSG_GUILD_EVENT_PRESENCE_CHANGE             = 0x0B70"
        "SMSG_GUILD_EVENT_PLAYER_LEFT                 = 0x0BF8"
        "SMSG_GUILD_EVENT_NEW_LEADER                  = 0x0E69"
        "SMSG_GUILD_EVENT_DISBANDED                   = 0x1E68")
    string(FIND "${opcode_header}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "typed guild-event opcode missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "guild->BroadcastMemberPresence(pCurrChar->GetObjectGuid(), pCurrChar->GetName(), true);"
        "guild->BroadcastMemberPresence(_player->GetObjectGuid(), _player->GetName(), false);")
    string(FIND "${login_sender}${world_session}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "guild presence call missing: ${token}")
    endif()
endforeach()

foreach(token IN ITEMS
        "ObjectGuid const removedGuid = slot->guid;"
        "std::string const removedName = slot->Name;"
        "guild->DelMember(removedGuid)"
        "guild->BroadcastMemberRemoved(removedGuid, removedName")
    string(FIND "${guild_handler}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "safe typed guild removal missing: ${token}")
    endif()
endforeach()

file(GLOB_RECURSE guild_production_sources
    "${SOURCE_ROOT}/src/game/*.cpp"
    "${SOURCE_ROOT}/src/game/*.h")
list(FILTER guild_production_sources EXCLUDE REGEX "/tests/")

set(legacy_patterns
    "(^|[^A-Z0-9_])CMSG_GUILD_INFO([^A-Z0-9_]|$)"
    "(^|[^A-Z0-9_])SMSG_GUILD_INFO([^A-Z0-9_]|$)"
    "(^|[^A-Z0-9_])SMSG_GUILD_EVENT([^A-Z0-9_]|$)"
    "(^|[^A-Za-z0-9_])HandleGuildInfoOpcode([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])BroadcastEvent([^A-Za-z0-9_]|$)"
    "(^|[^A-Za-z0-9_])GuildEvents([^A-Za-z0-9_]|$)")

foreach(path IN LISTS guild_production_sources)
    if(path MATCHES "Opcodes_reference\\.h$")
        continue()
    endif()
    file(READ "${path}" source)
    foreach(pattern IN LISTS legacy_patterns)
        string(REGEX MATCH "${pattern}" found "${source}")
        if(found)
            message(FATAL_ERROR "legacy generic guild packet remains in ${path}: ${pattern}")
        endif()
    endforeach()
endforeach()

set(mutation_source "${opcode_header}\n${guild_sender}")
foreach(pattern IN LISTS legacy_patterns)
    string(REGEX MATCH "${pattern}" found "${mutation_source}")
    if(found)
        message(FATAL_ERROR "legacy guild mutation escaped: ${pattern}")
    endif()
endforeach()

# CMSG_GUILD_QUERY: reader converted, request intentionally still dormant. Pin the
# handler wiring so the conversion cannot silently regress, and pin the dormancy so
# the request cannot be registered while its reply is still unregistered and
# unadmitted -- that combination builds a reply and drops it at the send gate.
require_once("${guild_handler}"
    "MopGuildPackets::ReadGuildQuery(recvPacket, rawPlayerGuid, rawGuildGuid)"
    "guild-query production reader call")
string(FIND "${opcode_registry}" "DefC(CMSG_GUILD_QUERY," guild_query_registered)
if(NOT guild_query_registered EQUAL -1)
    string(FIND "${opcode_registry}" "DefS(SMSG_GUILD_QUERY_RESPONSE," guild_query_response_registered)
    string(FIND "${world_session}" "case SMSG_GUILD_QUERY_RESPONSE:" guild_query_response_gate)
    if(guild_query_response_registered EQUAL -1 OR guild_query_response_gate EQUAL -1)
        message(FATAL_ERROR
            "CMSG_GUILD_QUERY is registered but SMSG_GUILD_QUERY_RESPONSE is not registered and admitted")
    endif()
endif()

# Guild info query, both halves. The dormancy guard above becomes a positive pin now
# that the response is derived, registered and admitted.
require_once("${opcode_registry}"
    "DefC(CMSG_GUILD_QUERY, \"CMSG_GUILD_QUERY\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleGuildQueryOpcode);"
    "guild-query request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_GUILD_QUERY_RESPONSE, \"SMSG_GUILD_QUERY_RESPONSE\");"
    "guild-query response registration")
require_once("${world_session}"
    "case SMSG_GUILD_QUERY_RESPONSE:"
    "guild-query response allowlist")
require_once("${packet_builder}"
    "out.WriteBits(uint32(ranks.size()), 21)"
    "guild-query 21-bit rank count")
require_once("${guild_sender}"
    "MopGuildPackets::BuildGuildQueryResponse(data, GetObjectGuid().GetRawValue(),"
    "guild-query production builder call")

# The 18414 roster and who-list pass GuildInvite a realm-qualified name, so the
# home-realm suffix has to come off BEFORE normalizePlayerName lowercases it.
# The token spans both statements so the ordering is pinned, not just the call.
require_once("${guild_handler}"
    "    StripHomeRealmSuffix(Invitedname);\n\n    if (normalizePlayerName(Invitedname))"
    "guild invite strips the home-realm suffix before normalizing")
