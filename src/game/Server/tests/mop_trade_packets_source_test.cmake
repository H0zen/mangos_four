file(READ "${SOURCE_ROOT}/src/game/Server/SharedDefines.h" shared_defines)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/TradeHandler.cpp" trade_source)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.h" session_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "status_values")
    string(REGEX REPLACE "TRADE_STATUS_PROPOSED[ \t]*=[ \t]*24"
        "TRADE_STATUS_PROPOSED = 12"
        shared_defines "${shared_defines}")
elseif(MUTATION STREQUAL "status_registration")
    string(REPLACE "DefS(SMSG_TRADE_STATUS, \"SMSG_TRADE_STATUS\");"
        "/* removed compact registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "extended_registration")
    string(REPLACE "DefS(SMSG_TRADE_STATUS_EXTENDED, \"SMSG_TRADE_STATUS_EXTENDED\");"
        "/* removed extended registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "status_gate")
    string(REPLACE "case SMSG_TRADE_STATUS:"
        "/* removed compact gate */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "extended_gate")
    string(REPLACE "case SMSG_TRADE_STATUS_EXTENDED:"
        "/* removed extended gate */"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "status_builder")
    string(REPLACE "MopTradePackets::BuildStatus(data, status, statusData)"
        "false"
        trade_source "${trade_source}")
elseif(MUTATION STREQUAL "proposal_builder")
    string(REPLACE "TRADE_STATUS_PROPOSED, proposalData"
        "TRADE_STATUS_PROPOSED"
        trade_source "${trade_source}")
elseif(MUTATION STREQUAL "extended_builder")
    string(REPLACE "MopTradePackets::BuildUpdate(data, update)"
        "false"
        trade_source "${trade_source}")
elseif(MUTATION STREQUAL "count_width")
    string(REPLACE "out.WriteBits(uint32(update.items.size()), 20);"
        "out.WriteBits(uint32(update.items.size()), 22);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "rollback_status")
    string(REPLACE "SendTradeStatus(TRADE_STATUS_UNACCEPTED)"
        "SendTradeStatus(TRADE_STATUS_PROPOSED)"
        player_source "${player_source}")
elseif(MUTATION STREQUAL "over_money")
    string(REPLACE "m_money = money;"
        "m_player->GetSession()->SendTradeStatus(TRADE_STATUS_PROPOSED);\n    m_money = money;"
        player_source "${player_source}")
endif()

function(require_once source token context)
    string(REGEX MATCHALL "${token}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

foreach(pair IN ITEMS
        "FAILED;0" "TARGET_STUNNED;1" "INITIATED;2"
        "CURRENCY_NOT_TRADABLE;3" "PLAYER_NOT_FOUND;4" "TOO_FAR_AWAY;5"
        "ACCEPTED;6" "DEAD;7" "STATE_CHANGED;9" "WRONG_FACTION;10"
        "ALREADY_TRADING;11" "RESTRICTED_ACCOUNT;13" "COMPLETE;14"
        "LOGGING_OUT;15" "PLAYER_IGNORED;16" "TARGET_LOGGING_OUT;17"
        "PETITION;18" "STUNNED;20" "PLAYER_BUSY;21" "WRONG_REALM;22"
        "NOT_ENOUGH_CURRENCY;23" "PROPOSED;24" "UNACCEPTED;27"
        "TARGET_DEAD;28" "CANCELLED;30" "NOT_ON_TAPLIST;31")
    string(REPLACE ";" ";" fields "${pair}")
    list(GET fields 0 name)
    list(GET fields 1 value)
    require_once("${shared_defines}"
        "TRADE_STATUS_${name}[ \t]*=[ \t]*${value}"
        "trade status ${name}")
endforeach()

require_once("${player_header}" "namespace MopTradePackets"
    "owning trade packet helper namespace")
require_once("${player_header}"
    "out\\.WriteBits\\(uint32\\(update\\.items\\.size\\(\\)\\), 20\\)"
    "20-bit extended item count")
require_once("${trade_source}"
    "MopTradePackets::BuildStatus\\(data, status, statusData\\)"
    "compact status production builder")
require_once("${trade_source}"
    "TRADE_STATUS_PROPOSED, proposalData"
    "proposal routed through common builder")
require_once("${trade_source}"
    "MopTradePackets::BuildUpdate\\(data, update\\)"
    "extended update production builder")
require_once("${session_header}"
    "void SendTradeStatus\\(TradeStatus status,[ \t]*MopTradePackets::StatusData const& statusData"
    "status branch-data API")
require_once("${opcode_registry}"
    "DefS\\(SMSG_TRADE_STATUS,[ \t]*\"SMSG_TRADE_STATUS\"\\)"
    "compact status registration")
require_once("${opcode_registry}"
    "DefS\\(SMSG_TRADE_STATUS_EXTENDED,[ \t]*\"SMSG_TRADE_STATUS_EXTENDED\"\\)"
    "extended status registration")
require_once("${session_source}" "case[ \t]+SMSG_TRADE_STATUS:"
    "compact status suppression gate")
require_once("${session_source}" "case[ \t]+SMSG_TRADE_STATUS_EXTENDED:"
    "extended status suppression gate")

string(REGEX MATCHALL "WorldPacket data\\(SMSG_TRADE_STATUS" raw_status_writers
    "${trade_source}")
list(LENGTH raw_status_writers raw_status_writer_count)
if(NOT raw_status_writer_count EQUAL 0)
    message(FATAL_ERROR
        "raw compact/extended trade packet writer remains in TradeHandler.cpp")
endif()

string(REGEX MATCHALL "SendTradeStatus\\(TRADE_STATUS_UNACCEPTED\\)"
    rollback_calls "${player_source}")
list(LENGTH rollback_calls rollback_count)
if(NOT rollback_count EQUAL 2)
    message(FATAL_ERROR
        "expected two acceptance rollback sends, found ${rollback_count}")
endif()

string(FIND "${player_source}"
    "SendTradeStatus(TRADE_STATUS_PROPOSED)" over_money_status)
if(NOT over_money_status EQUAL -1)
    message(FATAL_ERROR "status 24 remains in the over-money path")
endif()

# The trade conversation. Only cancel was registered before, so a player could
# abort a trade they had no way to start. Pin the whole registration tuple for each
# step so a changed handler, status or processing mode fails here.
set(trade_registrations
    "DefC(CMSG_BEGIN_TRADE, \"CMSG_BEGIN_TRADE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleBeginTradeOpcode);"
    "DefC(CMSG_INITIATE_TRADE, \"CMSG_INITIATE_TRADE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleInitiateTradeOpcode);"
    "DefC(CMSG_UNACCEPT_TRADE, \"CMSG_UNACCEPT_TRADE\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleUnacceptTradeOpcode);"
    "DefC(CMSG_SET_TRADE_GOLD, \"CMSG_SET_TRADE_GOLD\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSetTradeGoldOpcode);"
    "DefC(CMSG_SET_TRADE_ITEM, \"CMSG_SET_TRADE_ITEM\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleSetTradeItemOpcode);"
    "DefC(CMSG_CLEAR_TRADE_ITEM, \"CMSG_CLEAR_TRADE_ITEM\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleClearTradeItemOpcode);")
foreach(trade_registration IN LISTS trade_registrations)
    string(FIND "${opcode_registry}" "${trade_registration}" trade_found)
    if(trade_found EQUAL -1)
        message(FATAL_ERROR "trade registration missing or altered: ${trade_registration}")
    endif()
endforeach()

# The initiate-trade guid order is binary-derived. The inherited permutation
# consumed the same six bytes while decoding a different byte set, so a size check
# would not catch a regression here; pin the orders themselves.
string(FIND "${player_header}" "uint8 const maskOrder[] = { 5, 1, 4, 2, 3, 7, 0, 6 };" initiate_mask)
string(FIND "${player_header}" "uint8 const byteOrder[] = { 4, 6, 2, 0, 3, 7, 5, 1 };" initiate_bytes)
if(initiate_mask EQUAL -1 OR initiate_bytes EQUAL -1)
    message(FATAL_ERROR "initiate-trade guid order is not the 18414 client order")
endif()
string(FIND "${trade_source}" "recvPacket >> tradeSlot;" set_item_trade_slot)
string(FIND "${trade_source}" "recvPacket >> slot;" set_item_inventory_slot)
string(FIND "${trade_source}" "recvPacket >> bag;" set_item_bag)
if(set_item_trade_slot EQUAL -1 OR set_item_inventory_slot EQUAL -1 OR set_item_bag EQUAL -1)
    message(FATAL_ERROR "set-trade-item does not read all three 18414 fields")
endif()
if(NOT set_item_trade_slot LESS set_item_inventory_slot OR NOT set_item_inventory_slot LESS set_item_bag)
    message(FATAL_ERROR "set-trade-item must read trade slot, then inventory slot, then bag")
endif()

string(FIND "${trade_source}" "MopTradePackets::ReadInitiateTrade(recvPacket)" initiate_reader)
if(initiate_reader EQUAL -1)
    message(FATAL_ERROR "initiate-trade handler does not use the shared 18414 reader")
endif()

# CMSG_ACCEPT_TRADE must not be registered while its state token is discarded. The
# token is the only thing that rejects an accept aimed at an offer that has since
# changed: clearing both accepted flags on mutation cannot invalidate an accept the
# server has not materialised yet, and sessions drain independently.
string(FIND "${opcode_registry}" "DefC(CMSG_ACCEPT_TRADE," accept_registered)
if(NOT accept_registered EQUAL -1)
    string(FIND "${trade_source}" "read_skip<uint32>()" accept_token_discarded)
    if(NOT accept_token_discarded EQUAL -1)
        message(FATAL_ERROR
            "CMSG_ACCEPT_TRADE is registered while its trade-state token is still discarded")
    endif()
endif()
