file(READ "${SOURCE_ROOT}/src/game/Server/WorldGateway.cpp" gateway)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session)
file(READ "${SOURCE_ROOT}/src/shared/Log/Log.h" log_header)

function(require_text content needle description)
    string(FIND "${content}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Missing ${description}")
    endif()
endfunction()

require_text("${gateway}"
    "sLog.outWorldPacketDump(session, packet.GetOpcode(),"
    "incoming packet dump at WorldGateway::Deliver")
require_text("${gateway}"
    "LookupOpcodeName(DIR_CLIENT, packet.GetOpcode()), &packet, true);"
    "incoming packet direction and payload")
require_text("${session}"
    "sLog.outWorldPacketDump(GetAccountId(), packet->GetOpcode(),"
    "outgoing packet dump at WorldSession::SendPacket")
require_text("${session}"
    "LookupOpcodeName(DIR_SERVER, packet->GetOpcode()), packet, false);"
    "outgoing packet direction and payload")
require_text("${session}"
    "sLog.outWorldPacketDumpRedacted(GetAccountId(), packet->GetOpcode(),"
    "outgoing auth-response redaction")
require_text("${session}"
    "if (packet->GetOpcode() == SMSG_AUTH_RESPONSE)"
    "SMSG_AUTH_RESPONSE redaction guard")
require_text("${log_header}"
    "Hooked into WorldGateway::Deliver (incoming) and"
    "packet-logging call-site documentation")

string(FIND "${gateway}" "if (WorldSession* target = Find(session))" find_session)
string(FIND "${gateway}" "sLog.outWorldPacketDump(session" incoming_dump)
string(FIND "${gateway}" "target->QueuePacket" queue_packet)
if(find_session EQUAL -1 OR incoming_dump LESS find_session OR queue_packet LESS incoming_dump)
    message(FATAL_ERROR
        "Incoming logging must run after session resolution and before queue ownership transfer")
endif()

string(FIND "${session}" "if (m_suppressWorldSends" suppression_gate)
string(FIND "${session}" "sLog.outWorldPacketDump(GetAccountId()" outgoing_dump)
string(FIND "${session}" "m_Socket->SendPacket(*packet);" link_send)
if(suppression_gate EQUAL -1 OR outgoing_dump LESS suppression_gate OR link_send LESS outgoing_dump)
    message(FATAL_ERROR
        "Outgoing logging must run after suppression and before the link send")
endif()
