# Guards against two opcode symbols sharing one value in one direction.
#
# This is the check that was missing when MSG_MOVE_WORLDPORT_ACK was registered on
# its inherited 0x00E0, which is CMSG_CHAR_ENUM in 18414. The second registration
# displaced the first, every client hung on "Retrieving character list", and nothing
# reported why. DefC/DefS assert at runtime, but only for opcodes actually
# registered -- a collision between two unregistered names sits latent until someone
# registers the second, and then it is a live incident rather than a test failure.
#
# DIRECTION COMES FROM REGISTRATION, NOT FROM THE NAME PREFIX.
#
# That distinction is the whole test. MSG_ is a naming class, not a direction: MSG_
# symbols are registered into the same direction-specific tables as everything else,
# and MSG_MOVE_WORLDPORT_ACK itself goes in via DefC. An earlier version of this file
# gave MSG_ its own key space, which made MSG_/CMSG_ collisions invisible -- so it
# would NOT have caught the very incident it exists to prevent. Live examples it was
# hiding: MSG_MOVE_SET_WALK_SPEED_CHEAT vs CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK on
# 0x10D1, and MSG_DEV_SHOWLABEL vs the registered SMSG_CALENDAR_SEND_EVENT on 0x12AE.
#
# So each symbol is resolved to the direction(s) it can occupy:
#   DefC(name)        -> client table only
#   DefS(name)        -> server table only
#   unregistered CMSG_ -> client
#   unregistered SMSG_ -> server
#   unregistered MSG_  -> EITHER, so it is compared against both. Conservative on
#                         purpose: an unregistered MSG_ has no table assignment yet,
#                         and assuming one is how the last version went wrong.
#
# Cross-direction sharing is legitimate and common -- clientOpcodeTable and
# serverOpcodeTable are separate arrays. Only same-direction collisions are defects.
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

# ---------------------------------------------------------------------------
# Mutation arms. Every arm asserts it actually changed the text it targets and
# aborts with a PASS (exit 0) otherwise, so a dead arm shows up as a WILL_FAIL
# failure instead of quietly succeeding. That check compares like with like --
# an earlier version composed the before and after strings differently, so they
# always differed and the guard never fired.
# ---------------------------------------------------------------------------
set(_mutated_header "${_header}")
set(_mutated_cpp "${_cpp_src}")
set(_mutated_inc "${_inc_src}")

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "new_collision")
        # A second CMSG on a registered CMSG's value.
        string(REGEX REPLACE "(CMSG_AUTH_SESSION[ \t]*=[ \t]*0x([0-9A-Fa-f]+)[ \t]*,)"
               "\\1\n    CMSG_FAKE_COLLIDER = 0x\\2," _mutated_header "${_header}")
    elseif(MUTATION STREQUAL "msg_hides_collision")
        # The regression that produced this rewrite: an MSG_ symbol landing on a
        # registered CMSG's value must be caught, not filed under its own class.
        string(REGEX REPLACE "(CMSG_AUTH_SESSION[ \t]*=[ \t]*0x([0-9A-Fa-f]+)[ \t]*,)"
               "\\1\n    MSG_FAKE_COLLIDER = 0x\\2," _mutated_header "${_header}")
    elseif(MUTATION STREQUAL "double_registration")
        # Two REGISTERED symbols on one value: the 0x00E0 incident itself.
        set(_mutated_inc
            "${_inc_src}\nDefC(CMSG_COMMENTATOR_GET_MAP_INFO, \"x\", STATUS_NEVER, PROCESS_INPLACE, 0);")
    elseif(MUTATION STREQUAL "third_collider")
        # A new symbol joining an ALREADY BASELINED value. Comparing only
        # (direction,value) keys would miss this and let the baseline rot.
        string(REGEX REPLACE "(SMSG_AUTH_SRP6_RESPONSE[ \t]*=[ \t]*0x103A[ \t]*,)"
               "\\1\n    SMSG_THIRD_COLLIDER = 0x103A," _mutated_header "${_header}")
    elseif(MUTATION STREQUAL "invert_client_guard")
        # Neuter the runtime assert without touching its message. A version of this
        # gate that only searched for the message string passed this mutation.
        string(REPLACE "std::strcmp(name, clientOpcodeTable[v].name) == 0"
                       "std::strcmp(name, clientOpcodeTable[v].name) != 0"
               _mutated_cpp "${_cpp_src}")
    elseif(MUTATION STREQUAL "invert_server_guard")
        string(REPLACE "std::strcmp(name, serverOpcodeTable[v].name) == 0"
                       "std::strcmp(name, serverOpcodeTable[v].name) != 0"
               _mutated_cpp "${_cpp_src}")
    elseif(MUTATION STREQUAL "stale_baseline")
        # Resolve a baselined collision without updating the list. Must move the
        # VALUE: renaming one of two symbols that share a value leaves them still
        # sharing it, and that version of this arm passed.
        string(REGEX REPLACE "(SMSG_SERVER_INFO_RESPONSE[ \t]*=[ \t]*)0x103A" "\\10x1FFF"
               _mutated_header "${_header}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()

    if(_mutated_header STREQUAL "${_header}"
       AND _mutated_cpp STREQUAL "${_cpp_src}"
       AND _mutated_inc STREQUAL "${_inc_src}")
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()

set(_header "${_mutated_header}")
set(_cpp_src "${_mutated_cpp}")
set(_registrations "${_mutated_cpp}${_mutated_inc}")

# ---------------------------------------------------------------------------
# 1. The runtime guard must still compare the names, not merely mention them.
# ---------------------------------------------------------------------------
foreach(_needle
        "std::strcmp(name, clientOpcodeTable[v].name) == 0"
        "std::strcmp(name, serverOpcodeTable[v].name) == 0")
    string(FIND "${_cpp_src}" "${_needle}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "Opcodes.cpp no longer asserts that a re-registration uses the SAME name.\n"
            "  missing: ${_needle}\n"
            "That comparison is what turns a duplicate value into a startup failure\n"
            "instead of a silently displaced handler. Do not weaken or remove it.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 2. Resolve every symbol to the direction(s) it can occupy.
# ---------------------------------------------------------------------------
string(REGEX MATCHALL "DefC\\([ \t]*[A-Z][A-Za-z0-9_]*[ \t]*," _defc_raw "${_registrations}")
string(REGEX MATCHALL "DefS\\([ \t]*[A-Z][A-Za-z0-9_]*[ \t]*," _defs_raw "${_registrations}")
set(_defc "")
foreach(_m IN LISTS _defc_raw)
    string(REGEX MATCH "DefC\\([ \t]*([A-Z][A-Za-z0-9_]*)" _x "${_m}")
    list(APPEND _defc "${CMAKE_MATCH_1}")
endforeach()
set(_defs "")
foreach(_m IN LISTS _defs_raw)
    string(REGEX MATCH "DefS\\([ \t]*([A-Z][A-Za-z0-9_]*)" _x "${_m}")
    list(APPEND _defs "${CMAKE_MATCH_1}")
endforeach()

string(REGEX MATCHALL "[CS]?MSG_[A-Za-z0-9_]+[ \t]*=[ \t]*0x[0-9A-Fa-f]+" _decls "${_header}")

set(_keys "")
foreach(_decl IN LISTS _decls)
    string(REGEX MATCH "^([CS]?MSG_[A-Za-z0-9_]+)" _n "${_decl}")
    set(_name "${CMAKE_MATCH_1}")
    string(REGEX MATCH "0x([0-9A-Fa-f]+)$" _v "${_decl}")
    string(TOUPPER "${CMAKE_MATCH_1}" _value)

    list(FIND _defc "${_name}" _ic)
    list(FIND _defs "${_name}" _is)
    if(NOT _ic EQUAL -1)
        set(_dirs "CMSG")
    elseif(NOT _is EQUAL -1)
        set(_dirs "SMSG")
    elseif(_name MATCHES "^CMSG_")
        set(_dirs "CMSG")
    elseif(_name MATCHES "^SMSG_")
        set(_dirs "SMSG")
    else()
        set(_dirs "CMSG;SMSG")      # unregistered MSG_: could become either
    endif()

    foreach(_d IN LISTS _dirs)
        set(_k "${_d}:0x${_value}")
        list(FIND _keys "${_k}" _seen)
        if(_seen EQUAL -1)
            list(APPEND _keys "${_k}")
            set(_members_${_d}_${_value} "${_name}")
        else()
            list(APPEND _members_${_d}_${_value} "${_name}")
        endif()
    endforeach()
endforeach()

# ---------------------------------------------------------------------------
# 3. Build canonical signatures: direction:value:sorted-members. Signatures, not
#    bare keys, so that adding a third symbol to an already-baselined value or
#    swapping one member for another is detected.
# ---------------------------------------------------------------------------
set(_fatal "")
set(_found "")
foreach(_k IN LISTS _keys)
    string(REPLACE ":" ";" _parts "${_k}")
    list(GET _parts 0 _d)
    list(GET _parts 1 _hex)
    string(REPLACE "0x" "" _value "${_hex}")

    set(_mem "${_members_${_d}_${_value}}")
    list(LENGTH _mem _n)
    if(_n GREATER 1)
        list(SORT _mem)
        string(REPLACE ";" "," _memstr "${_mem}")

        set(_regcount 0)
        foreach(_name IN LISTS _mem)
            list(FIND _defc "${_name}" _a)
            list(FIND _defs "${_name}" _b)
            if(NOT _a EQUAL -1 OR NOT _b EQUAL -1)
                math(EXPR _regcount "${_regcount}+1")
            endif()
        endforeach()

        if(_regcount GREATER 1)
            set(_fatal "${_fatal}\n  ${_d} ${_hex}: ${_memstr}  (${_regcount} REGISTERED)")
        else()
            list(APPEND _found "${_d}:${_hex}:${_memstr}")
        endif()
    endif()
endforeach()

if(NOT _fatal STREQUAL "")
    message(FATAL_ERROR
        "Two REGISTERED opcodes share one value in one direction. The second\n"
        "registration displaces the first from the dispatch table and the displaced\n"
        "opcode stops being handled, with no error where it fails:${_fatal}\n\n"
        "This is the MSG_MOVE_WORLDPORT_ACK / CMSG_CHAR_ENUM incident. Resolve the\n"
        "value before registering either symbol.")
endif()

# ---------------------------------------------------------------------------
# 4. Latent collisions are baselined by full signature, so the set can only shrink.
#
# Each has two symbols claiming one value with at most one registered, so at least
# one name is wrong. They are recorded rather than fixed because choosing which name
# keeps the value needs binary evidence per opcode, and guessing is the same incident
# in slow motion.
# ---------------------------------------------------------------------------
set(_expected
    "CMSG:0x0026:CMSG_COMMENTATOR_GET_MAP_INFO,CMSG_DESTROY_ITEM"
    "CMSG:0x0360:CMSG_HEARTH_AND_RESURRECT,CMSG_SELF_RES"
    "CMSG:0x03C9:CMSG_ACTIVATETAXI,MSG_RAID_READY_CHECK_CONFIRM"
    "CMSG:0x0748:CMSG_GOSSIP_SELECT_OPTION,CMSG_MOVE_SET_RUN_MODE"
    "CMSG:0x10D1:CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK,MSG_MOVE_SET_WALK_SPEED_CHEAT"
    "CMSG:0x10D3:CMSG_TIME_SYNC_RESPONSE_DROPPED,MSG_MOVE_SET_SWIM_SPEED_CHEAT"
    "CMSG:0x1272:CMSG_CANCEL_AUTO_REPEAT_SPELL,CMSG_UNSTABLE_PET"
    "SMSG:0x103A:SMSG_AUTH_SRP6_RESPONSE,SMSG_SERVER_INFO_RESPONSE"
    "SMSG:0x12AE:MSG_DEV_SHOWLABEL,SMSG_CALENDAR_SEND_EVENT"
)

list(SORT _found)
list(SORT _expected)
if(NOT _found STREQUAL "${_expected}")
    set(_msg "")
    foreach(_s IN LISTS _found)
        list(FIND _expected "${_s}" _i)
        if(_i EQUAL -1)
            set(_msg "${_msg}\n  NEW      ${_s}")
        endif()
    endforeach()
    foreach(_s IN LISTS _expected)
        list(FIND _found "${_s}" _i)
        if(_i EQUAL -1)
            set(_msg "${_msg}\n  RESOLVED ${_s}")
        endif()
    endforeach()
    message(FATAL_ERROR
        "The set of latent opcode collisions changed:${_msg}\n\n"
        "NEW means two symbols now claim one value in one direction -- resolve it\n"
        "rather than adding it here. RESOLVED means one was fixed: delete it from\n"
        "_expected in this file so the baseline keeps shrinking.")
endif()

list(LENGTH _decls _nd)
list(LENGTH _found _nl)
message(STATUS "opcode collision guard: ${_nd} constants, 0 registered collisions, "
               "${_nl} latent (baselined by signature)")
