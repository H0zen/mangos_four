# Guards the 5.4.8 CMSG_PING field order against a silent revert to the 3.3.5
# layout. See ClientConnection::HandlePing for the Wow.exe 18414 evidence.
file(READ "${SOURCE_ROOT}/src/proto/ClientConnection.cpp" client_connection)

if(MUTATION STREQUAL "swapped_read")
    string(REPLACE
        "packet >> latency;\n        packet >> ping;"
        "packet >> ping;\n        packet >> latency;"
        client_connection "${client_connection}")
elseif(MUTATION STREQUAL "echoed_latency")
    string(REPLACE
        "pong << ping;"
        "pong << latency;"
        client_connection "${client_connection}")
endif()

function(strip_cpp_comments output source)
    set(text "${source}")
    while(TRUE)
        string(FIND "${text}" "/*" comment_start)
        if(comment_start EQUAL -1)
            break()
        endif()
        math(EXPR tail_start "${comment_start} + 2")
        string(SUBSTRING "${text}" ${tail_start} -1 tail)
        string(FIND "${tail}" "*/" comment_end)
        if(comment_end EQUAL -1)
            message(FATAL_ERROR "Unterminated block comment while scanning source")
        endif()
        string(SUBSTRING "${text}" 0 ${comment_start} before)
        math(EXPR after_start "${comment_end} + 2")
        string(SUBSTRING "${tail}" ${after_start} -1 after)
        set(text "${before}${after}")
    endwhile()
    string(REGEX REPLACE "//[^\r\n]*" "" text "${text}")
    set(${output} "${text}" PARENT_SCOPE)
endfunction()

function(extract_body output source start_marker end_marker)
    string(FIND "${source}" "${start_marker}" start)
    string(FIND "${source}" "${end_marker}" end)
    if(start EQUAL -1 OR end EQUAL -1 OR NOT start LESS end)
        message(FATAL_ERROR "Could not isolate ${start_marker}")
    endif()
    math(EXPR length "${end} - ${start}")
    string(SUBSTRING "${source}" ${start} ${length} body)
    set(${output} "${body}" PARENT_SCOPE)
endfunction()

function(require_ordered source context)
    set(remaining "${source}")
    foreach(token IN LISTS ARGN)
        string(FIND "${remaining}" "${token}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR "${context}: missing active ordered token: ${token}")
        endif()
        string(LENGTH "${token}" token_length)
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endforeach()
endfunction()

strip_cpp_comments(client_connection "${client_connection}")

extract_body(handler "${client_connection}"
    "bool ClientConnection::HandlePing"
    "void ClientConnection::RejectAuth")

# Wow.exe 18414 sub_66F403 writes *(this+6) before *(this+5) - byte offsets
# 0x18 then 0x14 - so the latency dword precedes the sequence dword. The
# sequence is what SMSG_PONG must echo; echoing the latency is what produces
# the client's "Received pong with old sequence".
require_ordered("${handler}" "CMSG_PING field order"
    "packet >> latency;"
    "packet >> ping;"
    "OnPing(m_session, latency"
    "pong << ping;")
