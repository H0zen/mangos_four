# Guards against two opcode symbols sharing one value.
#
# This is the check that was missing when MSG_MOVE_WORLDPORT_ACK was registered on
# its inherited 0x00E0, which is CMSG_CHAR_ENUM in 18414. The second registration
# displaced the first, every client hung on "Retrieving character list", and nothing
# reported why. DefC/DefS now assert at runtime, but only for opcodes that are
# actually registered -- a collision between two unregistered names sits latent until
# someone registers the second one, and then it is a live incident rather than a test
# failure.
#
# DIRECTION MATTERS. MoP opcode values are direction-scoped and the dispatch tables
# are separate arrays (clientOpcodeTable / serverOpcodeTable), so a CMSG and an SMSG
# sharing a value is legitimate and common -- 72 values do it. Only a collision within
# one direction is a defect. A test that flagged all 72 would be turned off within a
# week, so it groups by (direction, value) and says nothing about the cross pairs.
#
# MSG_ constants are direction-ambiguous and are treated as their own class.
#
# Run:
#   cmake -DSOURCE_ROOT=<repo> -P mop_opcode_collision_source_test.cmake

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(_hdr "${SOURCE_ROOT}/src/game/Server/Opcodes.h")
set(_cpp "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp")
set(_inc "${SOURCE_ROOT}/src/game/Server/opcode_register.inc")

foreach(_f "${_hdr}" "${_cpp}" "${_inc}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "missing source: ${_f}")
    endif()
endforeach()

file(READ "${_hdr}" _header_raw)
file(READ "${_cpp}" _cpp_raw)
file(READ "${_inc}" _inc_raw)

# Strip // comments BEFORE any regex list work. Opcode lines carry semicolons inside
# their trailing comments, e.g. "(Wow.exe leaf; name reference-consensus)", and CMake
# would split those into extra list elements and corrupt every downstream match.
string(REGEX REPLACE "//[^\n]*" "" _header "${_header_raw}")
string(REGEX REPLACE "//[^\n]*" "" _cpp_src "${_cpp_raw}")
string(REGEX REPLACE "//[^\n]*" "" _inc_src "${_inc_raw}")

set(_registrations "${_cpp_src}${_inc_src}")

# ---------------------------------------------------------------------------
# Mutation arms. Each must change something; a mutation that silently matches
# nothing would make its WILL_FAIL arm pass for the wrong reason, so every arm
# verifies it actually altered the text and exits 0 (failing WILL_FAIL) if not.
# ---------------------------------------------------------------------------
if(DEFINED MUTATION)
    set(_before "${_header}${_registrations}")

    if(MUTATION STREQUAL "new_collision")
        # CMSG_AUTH_SESSION is registered; give a second CMSG its value.
        string(REGEX REPLACE "(CMSG_AUTH_SESSION[ \t]*=[ \t]*0x([0-9A-Fa-f]+)[ \t]*,)"
               "\\1\n    CMSG_FAKE_COLLIDER = 0x\\2," _header "${_header}")
    elseif(MUTATION STREQUAL "double_registration")
        # Register the other name on an existing latent collision, which is exactly
        # the 0x00E0 incident: two registered symbols on one value.
        set(_registrations "${_registrations}\nDefC(CMSG_COMMENTATOR_GET_MAP_INFO, \"x\", STATUS_NEVER, PROCESS_INPLACE, 0);")
    elseif(MUTATION STREQUAL "remove_client_guard")
        string(REPLACE "two client opcodes share one value" "removed" _cpp_src "${_cpp_src}")
    elseif(MUTATION STREQUAL "remove_server_guard")
        string(REPLACE "two server opcodes share one value" "removed" _cpp_src "${_cpp_src}")
    elseif(MUTATION STREQUAL "stale_baseline")
        # Resolve one known collision without updating the baseline below.
        # It must move the VALUE, not the name: renaming one of two symbols that share
        # a value leaves them still sharing it, so the collision -- and this arm --
        # survives. That version of this mutation passed, silently, until the arm was
        # checked. 0x1FFF is unused by any SMSG.
        string(REGEX REPLACE "(SMSG_SERVER_INFO_RESPONSE[ \t]*=[ \t]*)0x103A" "\\10x1FFF"
               _header "${_header}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()

    set(_after "${_header}${_registrations}${_cpp_src}")
    if(_before STREQUAL "${_after}" AND NOT MUTATION STREQUAL "remove_client_guard"
       AND NOT MUTATION STREQUAL "remove_server_guard")
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- the arm is dead, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()

# ---------------------------------------------------------------------------
# 1. The runtime guard in DefC/DefS must still be there.
# ---------------------------------------------------------------------------
foreach(_needle "two client opcodes share one value" "two server opcodes share one value")
    string(FIND "${_cpp_src}" "${_needle}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "Opcodes.cpp no longer asserts on colliding registrations (missing: ${_needle}).\n"
            "That assert is what turns a duplicate value into a startup failure instead of\n"
            "a silently displaced handler. Do not remove it.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 2. Collect every opcode constant, grouped by direction and value.
# ---------------------------------------------------------------------------
string(REGEX MATCHALL "[CS]?MSG_[A-Za-z0-9_]+[ \t]*=[ \t]*0x[0-9A-Fa-f]+" _decls "${_header}")

set(_seen_keys "")
set(_fatal "")
set(_latent "")

foreach(_decl IN LISTS _decls)
    string(REGEX MATCH "^([CS]?MSG_[A-Za-z0-9_]+)" _name "${_decl}")
    set(_name "${CMAKE_MATCH_1}")
    string(REGEX MATCH "0x([0-9A-Fa-f]+)$" _v "${_decl}")
    string(TOUPPER "${CMAKE_MATCH_1}" _value)

    if(_name MATCHES "^CMSG_")
        set(_dir "CMSG")
    elseif(_name MATCHES "^SMSG_")
        set(_dir "SMSG")
    else()
        set(_dir "MSG")
    endif()
    set(_key "${_dir}_0x${_value}")

    list(FIND _seen_keys "${_key}" _idx)
    if(_idx EQUAL -1)
        list(APPEND _seen_keys "${_key}")
        set(_names_${_key} "${_name}")
    else()
        set(_names_${_key} "${_names_${_key}} ${_name}")

        # How many of the colliding names are actually registered?
        set(_reg_count 0)
        foreach(_n IN LISTS _names_${_key})
        endforeach()
        string(REPLACE " " ";" _nlist "${_names_${_key}}")
        foreach(_n IN LISTS _nlist)
            if(_registrations MATCHES "Def[CS]\\([ \t]*${_n}[ \t]*,")
                math(EXPR _reg_count "${_reg_count}+1")
            endif()
        endforeach()

        if(_reg_count GREATER 1)
            set(_fatal "${_fatal}\n  ${_key}: ${_names_${_key}}  (${_reg_count} of them REGISTERED)")
        else()
            set(_latent "${_latent}\n  ${_key}: ${_names_${_key}}")
        endif()
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 3. Two registered symbols on one value is never acceptable.
# ---------------------------------------------------------------------------
if(NOT _fatal STREQUAL "")
    message(FATAL_ERROR
        "Two REGISTERED opcodes share one value, in one direction. The second\n"
        "registration displaces the first from the dispatch table and the displaced\n"
        "opcode stops being handled, with no error at the point of failure:${_fatal}\n\n"
        "This is the MSG_MOVE_WORLDPORT_ACK / CMSG_CHAR_ENUM incident. Resolve the\n"
        "value before registering either symbol.")
endif()

# ---------------------------------------------------------------------------
# 4. Latent collisions are baselined so the set can only shrink.
#
# Each of these has two names claiming one value with at most one registered, so
# at least one name is wrong. They are recorded rather than fixed here because
# choosing which name keeps the value needs binary evidence per opcode, and a
# wrong choice is the same incident in slow motion.
# ---------------------------------------------------------------------------
set(_expected_latent
    "CMSG_0x0026"      # CMSG_DESTROY_ITEM (registered) vs CMSG_COMMENTATOR_GET_MAP_INFO
    "CMSG_0x0360"      # CMSG_SELF_RES vs CMSG_HEARTH_AND_RESURRECT, neither registered
    "CMSG_0x0748"      # CMSG_GOSSIP_SELECT_OPTION (registered) vs CMSG_MOVE_SET_RUN_MODE
    "SMSG_0x103A"      # SMSG_AUTH_SRP6_RESPONSE vs SMSG_SERVER_INFO_RESPONSE
    "CMSG_0x1272"      # CMSG_CANCEL_AUTO_REPEAT_SPELL vs CMSG_UNSTABLE_PET
)

set(_found_latent "")
foreach(_key IN LISTS _seen_keys)
    string(FIND "${_latent}" "  ${_key}:" _at)
    if(NOT _at EQUAL -1)
        list(APPEND _found_latent "${_key}")
    endif()
endforeach()
list(SORT _found_latent)
list(SORT _expected_latent)

if(NOT _found_latent STREQUAL "${_expected_latent}")
    message(FATAL_ERROR
        "The set of latent opcode collisions changed.\n"
        "  expected: ${_expected_latent}\n"
        "  found   : ${_found_latent}\n\n"
        "A NEW entry means two symbols now claim one value -- resolve it rather than\n"
        "adding it here. A MISSING entry means one was resolved: delete it from\n"
        "_expected_latent in this file so the baseline keeps shrinking.")
endif()

list(LENGTH _decls _n_decls)
list(LENGTH _found_latent _n_latent)
message(STATUS "opcode collision guard: ${_n_decls} constants, 0 registered collisions, "
               "${_n_latent} latent (baselined)")
