# Ties the helm/cloak visibility handlers to the polarity the packet test models.
#
# mop_showing_helm_packets states the rule -- the wire bit is the wanted SHOWING
# state, the server flag is HIDE, so the stored value is the bit's inverse -- but
# it cannot call the handler, which needs a live session. Nothing stopped the
# handler drifting back to assigning the bit directly, which is what left both
# the console command and the UI checkbox inert. This closes that gap.

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" HANDLER_SOURCE)

# Optional mutations, exercised by WILL_FAIL tests so a silent guard is caught.
if(MUTATION STREQUAL "helm_assigns_bit")
    string(REPLACE "PLAYER_FLAGS_HIDE_HELM, !showing" "PLAYER_FLAGS_HIDE_HELM, showing"
        HANDLER_SOURCE "${HANDLER_SOURCE}")
elseif(MUTATION STREQUAL "cloak_assigns_bit")
    string(REPLACE "PLAYER_FLAGS_HIDE_CLOAK, !showing" "PLAYER_FLAGS_HIDE_CLOAK, showing"
        HANDLER_SOURCE "${HANDLER_SOURCE}")
elseif(MUTATION STREQUAL "helm_reads_uint8")
    string(REPLACE "bool const showing = recv_data.ReadBit();" "uint8 showing; recv_data >> showing;"
        HANDLER_SOURCE "${HANDLER_SOURCE}")
elseif(MUTATION STREQUAL "helm_toggles")
    string(REPLACE "_player->ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM, !showing);"
        "_player->ToggleFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);"
        HANDLER_SOURCE "${HANDLER_SOURCE}")
elseif(MUTATION STREQUAL "cloak_uses_helm_bit")
    string(REPLACE "PLAYER_FLAGS_HIDE_CLOAK, !showing" "PLAYER_FLAGS_HIDE_HELM, !showing"
        HANDLER_SOURCE "${HANDLER_SOURCE}")
endif()

# Isolate each handler so the helm's rule cannot be satisfied by the cloak's line.
string(FIND "${HANDLER_SOURCE}" "void WorldSession::HandleShowingHelmOpcode" HELM_START)
string(FIND "${HANDLER_SOURCE}" "void WorldSession::HandleShowingCloakOpcode" CLOAK_START)
if(HELM_START EQUAL -1 OR CLOAK_START EQUAL -1)
    message(FATAL_ERROR "Could not locate the helm/cloak visibility handlers")
endif()
if(NOT HELM_START LESS CLOAK_START)
    message(FATAL_ERROR "Expected the helm handler to precede the cloak handler")
endif()

math(EXPR HELM_LENGTH "${CLOAK_START} - ${HELM_START}")
string(SUBSTRING "${HANDLER_SOURCE}" ${HELM_START} ${HELM_LENGTH} HELM_BODY)
string(LENGTH "${HANDLER_SOURCE}" SOURCE_LENGTH)
math(EXPR CLOAK_REMAINING "${SOURCE_LENGTH} - ${CLOAK_START}")
if(CLOAK_REMAINING GREATER 1200)
    set(CLOAK_REMAINING 1200)
endif()
string(SUBSTRING "${HANDLER_SOURCE}" ${CLOAK_START} ${CLOAK_REMAINING} CLOAK_BODY)

# The body is one MSB-first bit, not a uint8. A reader switching on 0 and 1 would
# match neither 0x80 nor 0x00 as the client sends them.
foreach(WHICH helm cloak)
    if(WHICH STREQUAL "helm")
        set(BODY "${HELM_BODY}")
    else()
        set(BODY "${CLOAK_BODY}")
    endif()

    string(FIND "${BODY}" "recv_data.ReadBit()" READS_BIT)
    if(READS_BIT EQUAL -1)
        message(FATAL_ERROR "${WHICH}: body must be read as a single MSB-first bit")
    endif()

    string(REGEX MATCH "recv_data[ 	]*>>[ 	]*[A-Za-z_]" READS_SCALAR "${BODY}")
    if(NOT READS_SCALAR STREQUAL "")
        message(FATAL_ERROR "${WHICH}: body must not be read as a scalar (${READS_SCALAR})")
    endif()

    # A toggle desynchronises permanently the first time client and server
    # disagree, which is how this handler was originally wrong.
    string(FIND "${BODY}" "ToggleFlag" TOGGLES)
    if(NOT TOGGLES EQUAL -1)
        message(FATAL_ERROR "${WHICH}: must assign from the request, not toggle")
    endif()
endforeach()

# The load-bearing assertion: the stored flag is the INVERSE of the wire bit.
# Character classes throughout -- CMake strips one level of backslash escaping,
# so \\( and \\* do not survive to the regex engine.
string(REGEX MATCH
    "ApplyModFlag[(]PLAYER_FLAGS,[ \t]*PLAYER_FLAGS_HIDE_HELM,[ \t]*[!]showing[)]"
    HELM_RULE "${HELM_BODY}")
if(HELM_RULE STREQUAL "")
    message(FATAL_ERROR
        "helm: expected ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM, !showing) -- "
        "the wire bit is the wanted SHOWING state and the flag is HIDE, so it must be inverted")
endif()

string(REGEX MATCH
    "ApplyModFlag[(]PLAYER_FLAGS,[ \t]*PLAYER_FLAGS_HIDE_CLOAK,[ \t]*[!]showing[)]"
    CLOAK_RULE "${CLOAK_BODY}")
if(CLOAK_RULE STREQUAL "")
    message(FATAL_ERROR
        "cloak: expected ApplyModFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK, !showing)")
endif()

# Each handler must touch only its own flag, or one toggle moves both items.
string(FIND "${HELM_BODY}" "PLAYER_FLAGS_HIDE_CLOAK" HELM_TOUCHES_CLOAK)
string(FIND "${CLOAK_BODY}" "PLAYER_FLAGS_HIDE_HELM" CLOAK_TOUCHES_HELM)
if(NOT HELM_TOUCHES_CLOAK EQUAL -1)
    message(FATAL_ERROR "helm handler must not touch the cloak flag")
endif()
if(NOT CLOAK_TOUCHES_HELM EQUAL -1)
    message(FATAL_ERROR "cloak handler must not touch the helm flag")
endif()

message(STATUS "mop_showing_helm_source: helm and cloak store the inverse of the showing bit")
