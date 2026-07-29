# Pins the character-screen response codes and the character-select reply set.
#
# Character deletion removed the character from the database and then told the client it had
# failed. Our ResponseCodes enum was missing five enumerators added in 5.4.8, so every value
# below CHAR_CREATE_FORCE_LOGIN was shifted down: we answered SMSG_CHAR_DELETE with 71, and
# 71 in 18414 is CHAR_DELETE_IN_PROGRESS, not CHAR_DELETE_SUCCESS. The client never saw a
# terminal result. 34 of 104 values were wrong.
#
# Nothing pinned any of it, which is why it survived. The values are confirmed from two
# independent directions:
#
#   Wire. SMSG_CHAR_CREATE carries 0x2F (CHAR_CREATE_SUCCESS, 47) and 0x32
#   (CHAR_CREATE_NAME_IN_USE, 50) across the five observations in the 18414 corpus.
#
#   Binary. The client carries its own ordered response-name table, off_DC9890[109] (base
#   Wow.exe.c:80104, index 0 = RESPONSE_SUCCESS), where the array index IS the wire value.
#   Every one of our 109 enumerators matches that table by name at its own value, and the
#   table independently places CHAR_CREATE_SUCCESS at 47 and CHAR_CREATE_NAME_IN_USE at 50 --
#   agreeing with the wire anchors it never saw.
#
# That second source is what makes CHAR_DELETE_SUCCESS = 0x48 solid. There is no
# SMSG_CHAR_DELETE anywhere in the corpus, so it has no wire observation at all; it was
# originally pinned only because a live client accepted 72 and reported
# COP_DELETE_CHARACTER result=TRUE. The client's own table puts CHAR_DELETE_SUCCESS at 72
# directly, which is stronger than the behavioural test and independent of it.
#
# Run:
#   cmake -DSOURCE_ROOT=<repo> -P mop_char_response_codes_source_test.cmake

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(_shared  "${SOURCE_ROOT}/src/game/Server/SharedDefines.h")
set(_charh   "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp")
set(_misc    "${SOURCE_ROOT}/src/game/WorldHandlers/MiscHandler.cpp")
set(_opcodes "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp")
set(_dbcfmt  "${SOURCE_ROOT}/src/game/Server/DBCfmt.h")
foreach(_f "${_shared}" "${_charh}" "${_misc}" "${_opcodes}" "${_dbcfmt}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "missing source: ${_f}")
    endif()
endforeach()

# Comments are stripped so no assertion can be satisfied by prose that merely names the
# thing it checks for. Both forms: a block comment quoting an old value would otherwise
# keep a reverted enum passing.
macro(strip_comments _var)
    # CRLF first: the multi-line assertions below match on "\n", and on a CRLF checkout every
    # one of them would miss and report the code as absent when it is present.
    string(REPLACE "\r\n" "\n" ${_var} "${${_var}}")
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" ${_var} "${${_var}}")
    string(REGEX REPLACE "//[^\n]*" "" ${_var} "${${_var}}")
endmacro()

# Narrows a source blob to one function body, so an assertion about "this handler must not do X"
# cannot be satisfied or broken by an unrelated caller elsewhere in the file.
macro(extract_function _out _src _signature)
    string(FIND "${_src}" "${_signature}" _fn_start)
    if(_fn_start EQUAL -1)
        message(FATAL_ERROR "cannot locate function: ${_signature}")
    endif()
    string(SUBSTRING "${_src}" ${_fn_start} -1 ${_out})
    # The first closing brace at column 0 ends the function. Anchoring on that rather than on the
    # next signature means the window cannot silently swallow the following handler if that one
    # happens to have a return type this macro did not anticipate.
    string(FIND "${${_out}}" "\n}" _fn_end)
    if(_fn_end EQUAL -1)
        message(FATAL_ERROR "cannot find the closing brace of: ${_signature}")
    endif()
    string(SUBSTRING "${${_out}}" 0 ${_fn_end} ${_out})
endmacro()

file(READ "${_shared}" _shared_src)
strip_comments(_shared_src)
file(READ "${_charh}" _charh_src)
strip_comments(_charh_src)
file(READ "${_misc}" _misc_src)
strip_comments(_misc_src)
file(READ "${_opcodes}" _opcodes_src)
strip_comments(_opcodes_src)
file(READ "${_dbcfmt}" _dbcfmt_src)
strip_comments(_dbcfmt_src)

# ---------------------------------------------------------------------------
# Mutation arms. Each verifies it changed its target and exits 0 otherwise, so a
# dead arm surfaces as a WILL_FAIL failure rather than a silent pass.
# ---------------------------------------------------------------------------
set(_m_shared  "${_shared_src}")
set(_m_charh   "${_charh_src}")
set(_m_misc    "${_misc_src}")
set(_m_opcodes "${_opcodes_src}")
set(_m_dbcfmt  "${_dbcfmt_src}")
if(DEFINED MUTATION)
    if(MUTATION STREQUAL "revert_delete_success")
        # The exact defect: CHAR_DELETE_SUCCESS back to the pre-5.4.8 slot.
        string(REPLACE "CHAR_DELETE_SUCCESS                                    = 0x48,"
                       "CHAR_DELETE_SUCCESS                                    = 0x47," _m_shared "${_shared_src}")
    elseif(MUTATION STREQUAL "drop_create_trial")
        # Removing this one enumerator is what shifted everything below it.
        string(REPLACE "CHAR_CREATE_TRIAL                                      = 0x46," "" _m_shared "${_shared_src}")
    elseif(MUTATION STREQUAL "drop_heirloom_code")
        string(REPLACE "CHAR_DELETE_FAILED_HAS_HEIRLOOM_OR_MAIL                = 0x4D," "" _m_shared "${_shared_src}")
    elseif(MUTATION STREQUAL "break_create_success_anchor")
        # The wire-confirmed anchor. If this can move, the enum has no fixed point.
        string(REPLACE "CHAR_CREATE_SUCCESS                                    = 0x2F,"
                       "CHAR_CREATE_SUCCESS                                    = 0x30," _m_shared "${_shared_src}")
    elseif(MUTATION STREQUAL "break_name_in_use_anchor")
        string(REPLACE "CHAR_CREATE_NAME_IN_USE                                = 0x32,"
                       "CHAR_CREATE_NAME_IN_USE                                = 0x33," _m_shared "${_shared_src}")
    elseif(MUTATION STREQUAL "restore_duplicate_enum")
        string(REPLACE "    data << (uint8)CHAR_DELETE_SUCCESS;\n    SendPacket(&data);"
                       "    data << (uint8)CHAR_DELETE_SUCCESS;\n    SendPacket(&data);\n    SendCharacterEnum();" _m_charh "${_charh_src}")
    elseif(MUTATION STREQUAL "world_entry_timezone_before_motd")
        # Put the world-entry timezone back next to the time-speed bootstrap, where it sat before
        # the corpus showed retail emits it after the MOTD.
        string(REPLACE "        WorldPacket tz(SMSG_SET_TIME_ZONE_INFORMATION, 2 + 2 * 7);\n        MopWorldEntryPackets::BuildSetTimeZoneInformation(tz, \"Etc/UTC\");\n        SendPacket(&tz, true);"
                       "" _m_charh "${_charh_src}")
        string(REPLACE "        SendPacket(&lts, true);"
                       "        SendPacket(&lts, true);\n        WorldPacket tz(SMSG_SET_TIME_ZONE_INFORMATION, 2 + 2 * 7);\n        MopWorldEntryPackets::BuildSetTimeZoneInformation(tz, \"Etc/UTC\");\n        SendPacket(&tz, true);"
                       _m_charh "${_m_charh}")
    elseif(MUTATION STREQUAL "drop_charselect_timezone")
        string(REPLACE "MopWorldEntryPackets::BuildSetTimeZoneInformation(tz, \"Etc/UTC\");\n    SendPacket(&tz);" "" _m_misc "${_misc_src}")
    elseif(MUTATION STREQUAL "battlepay_back_to_null")
        string(REPLACE "&WorldSession::HandleBattlePayGetPurchaseListOpcode"
                       "&WorldSession::Handle_NULL" _m_opcodes "${_opcodes_src}")
    elseif(MUTATION STREQUAL "drop_login_failed_registration")
        string(REPLACE "DefS(SMSG_CHARACTER_LOGIN_FAILED, \"SMSG_CHARACTER_LOGIN_FAILED\");" "" _m_opcodes "${_opcodes_src}")
    elseif(MUTATION STREQUAL "drop_randomname_registration")
        string(REPLACE "DefC(CMSG_GENERATE_RANDOM_CHARACTER_NAME," "DefC(CMSG_UNUSED_GENERATE_RANDOM_CHARACTER_NAME," _m_opcodes "${_opcodes_src}")
    elseif(MUTATION STREQUAL "corrupt_namegen_format")
        # "nsii" is id, name, race, sex. Any other shape misreads all 12972 rows.
        string(REPLACE "NameGenEntryfmt[]=\"nsii\"" "NameGenEntryfmt[]=\"niii\"" _m_dbcfmt "${_dbcfmt_src}")
    elseif(MUTATION STREQUAL "swap_randomname_args")
        # The client sends sex then race. Swapping reads a Gnome female as race 1 sex 7.
        string(REPLACE "recvPacket >> sex;\n    recvPacket >> race;"
                       "recvPacket >> race;\n    recvPacket >> sex;" _m_charh "${_charh_src}")
    elseif(MUTATION STREQUAL "drop_namegen_null_guard")
        string(REPLACE "if (!name)" "if (false)" _m_charh "${_charh_src}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()
    if(_m_shared STREQUAL "${_shared_src}" AND _m_charh STREQUAL "${_charh_src}" AND
       _m_misc STREQUAL "${_misc_src}" AND _m_opcodes STREQUAL "${_opcodes_src}" AND
       _m_dbcfmt STREQUAL "${_dbcfmt_src}")
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()
set(_shared_src  "${_m_shared}")
set(_charh_src   "${_m_charh}")
set(_misc_src    "${_m_misc}")
set(_opcodes_src "${_m_opcodes}")
set(_dbcfmt_src  "${_m_dbcfmt}")

# ---------------------------------------------------------------------------
# 1. The two wire-confirmed anchors. Everything else in the enum is positioned
#    relative to these, so if either moves the whole table is unmoored.
# ---------------------------------------------------------------------------
foreach(_anchor
        "CHAR_CREATE_SUCCESS                                    = 0x2F,"
        "CHAR_CREATE_NAME_IN_USE                                = 0x32,")
    string(FIND "${_shared_src}" "${_anchor}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "A wire-confirmed response code moved:\n  ${_anchor}\n\n"
            "Retail SMSG_CHAR_CREATE carries 0x2F and 0x32 across the five observations in\n"
            "the 18414 corpus. These are the only two values in this enum confirmed against\n"
            "actual traffic; the rest are positioned relative to them.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 2. The five enumerators whose absence caused the drift.
# ---------------------------------------------------------------------------
foreach(_row
        "CHAR_CREATE_TRIAL                                      = 0x46,"
        "CHAR_DELETE_FAILED_HAS_HEIRLOOM_OR_MAIL                = 0x4D,"
        "CHAR_LOGIN_TEMPORARY_GM_LOCK                           = 0x59,"
        "CHAR_LOGIN_LOCKED_BY_CHARACTER_UPGRADE                 = 0x5A,"
        "CHAR_LOGIN_LOCKED_BY_REVOKED_CHARACTER_UPGRADE         = 0x5B,")
    string(FIND "${_shared_src}" "${_row}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "A 5.4.8 response code is missing again:\n  ${_row}\n\n"
            "Each of these was absent, and each absence shifted every value below it. Four of\n"
            "the five have UI strings in the client's GlueStrings.lua, which is how we know\n"
            "they are real 18414 codes rather than later additions.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 3. The value that actually broke deletion. Live-confirmed only - see header.
# ---------------------------------------------------------------------------
string(FIND "${_shared_src}" "CHAR_DELETE_SUCCESS                                    = 0x48," _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "CHAR_DELETE_SUCCESS is not 0x48 (72).\n\n"
        "At 0x47 (71) the client reads CHAR_DELETE_IN_PROGRESS, never sees a terminal\n"
        "result, and reports the delete as failed - while the character has in fact already\n"
        "been removed from the database. The client's own response-name table puts\n"
        "CHAR_DELETE_SUCCESS at index 72, and a live client accepted 72 and answered\n"
        "COP_DELETE_CHARACTER result=TRUE.")
endif()

# ---------------------------------------------------------------------------
# 4. Character-select reply set. Retail answers all three requests in this phase.
# ---------------------------------------------------------------------------
# Scoped to the delete handler: HandleCharEnumOpcode calls SendCharacterEnum legitimately, in
# answer to the client's own request. Only the unsolicited push after a delete is the defect.
extract_function(_delete_body "${_charh_src}" "void WorldSession::HandleCharDeleteOpcode")
string(FIND "${_delete_body}" "CHAR_DELETE_SUCCESS" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR "extract_function did not capture the delete handler body -- assertion below would be vacuous")
endif()
string(FIND "${_delete_body}" "SendCharacterEnum();" _at)
if(NOT _at EQUAL -1)
    message(FATAL_ERROR
        "The unsolicited SMSG_CHAR_ENUM after delete is back.\n\n"
        "It was added because the client 'never refreshed by itself' - but that was observed\n"
        "while this handler answered with the wrong success code, so the client never saw a\n"
        "terminal result to refresh on. With the code corrected the client sends its own\n"
        "CMSG_CHAR_ENUM, and the push became a duplicate 1.4 KB enumeration.")
endif()

string(FIND "${_misc_src}" "MopWorldEntryPackets::BuildSetTimeZoneInformation(tz, \"Etc/UTC\");" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The character-select SMSG_SET_TIME_ZONE_INFORMATION send is gone.\n\n"
        "Retail sends this packet twice per session. This is the account-data-phase occurrence,\n"
        "which sits immediately after SMSG_ACCOUNT_DATA_TIMES and before the character list:\n"
        "capture-000019 seq 22 -> 23 -> 24. The world-entry occurrence is a separate send in\n"
        "CharacterHandler and does not substitute for it.")
endif()

# The world-entry occurrence is ordered differently from the char-select one, and the difference
# is easy to 'fix' backwards -- a reviewer proposed moving it adjacent to SMSG_ACCOUNT_DATA_TIMES,
# which is the char-select position, not this one. Two captures put it after the MOTD:
#   capture-000019 seq 177 ACCOUNT_DATA_TIMES, 179 MOTD, 180 SET_TIME_ZONE_INFORMATION
#   capture-000013 seq 161 ACCOUNT_DATA_TIMES, 164 MOTD, 166 SET_TIME_ZONE_INFORMATION
# The packets in between differ between the two, so only the after-MOTD relation is asserted.
#
# The evidence is RECORDED SEQUENCE ORDER, not timestamps, and re-deriving it from timing will
# look like it disproves the claim. All three packets are one server write burst: in
# capture-000013 they share the 8000 ms bucket at 1000 ms capture resolution, and in
# capture-000019 -- which has 1 ms resolution -- they still share the same millisecond, 26255.
# So no timestamp can separate them in either capture; sequence is the only ordering available,
# and for a single server connection that is the order the packets arrived, hence were sent.
string(FIND "${_charh_src}" "data.Initialize(SMSG_MOTD," _motd_at)
string(FIND "${_charh_src}" "WorldPacket tz(SMSG_SET_TIME_ZONE_INFORMATION" _tz_at)
if(_motd_at EQUAL -1 OR _tz_at EQUAL -1)
    message(FATAL_ERROR
        "Cannot locate the world-entry MOTD and timezone sends -- the ordering assertion below\n"
        "would be vacuous. MOTD found at ${_motd_at}, timezone at ${_tz_at}.")
endif()
if(_tz_at LESS _motd_at)
    message(FATAL_ERROR
        "The world-entry SMSG_SET_TIME_ZONE_INFORMATION is emitted before the MOTD.\n\n"
        "Retail emits it after: capture-000019 (177 ACCOUNT_DATA_TIMES, 179 MOTD, 180 timezone)\n"
        "and capture-000013 (161, 164, 166). Note this is NOT the char-select ordering, where the\n"
        "timezone does immediately follow SMSG_ACCOUNT_DATA_TIMES -- the two phases differ, and\n"
        "moving this send next to the account-data packet makes it less retail-faithful, not more.")
endif()

foreach(_reg
        "DefC(CMSG_BATTLE_PAY_GET_PURCHASE_LIST, \"CMSG_BATTLE_PAY_GET_PURCHASE_LIST\", STATUS_AUTHED, PROCESS_INPLACE, &WorldSession::HandleBattlePayGetPurchaseListOpcode);"
        "DefS(SMSG_BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE, \"SMSG_BATTLE_PAY_GET_PURCHASE_LIST_RESPONSE\");"
        "DefS(SMSG_CHARACTER_LOGIN_FAILED, \"SMSG_CHARACTER_LOGIN_FAILED\");"
        "DefC(CMSG_GENERATE_RANDOM_CHARACTER_NAME,"
        "DefS(SMSG_RANDOMIZE_CHAR_NAME, \"SMSG_RANDOMIZE_CHAR_NAME\");")
    string(FIND "${_opcodes_src}" "${_reg}" _at)
    if(_at EQUAL -1)
        message(FATAL_ERROR
            "A character-screen registration is missing:\n  ${_reg}\n\n"
            "Battle pay: retail answers it 420 times across the corpus; on Handle_NULL the\n"
            "client's request goes unanswered. SMSG_CHARACTER_LOGIN_FAILED: naming it is a\n"
            "prerequisite for ever sending a CHAR_LOGIN_* code -- note nothing sends it yet,\n"
            "and this gate does not claim otherwise. Randomise name: the creation button\n"
            "round-trips to the server.")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 5. NameGen and the randomise request layout, both taken from the client binary.
# ---------------------------------------------------------------------------
string(FIND "${_dbcfmt_src}" "NameGenEntryfmt[]=\"nsii\"" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "NameGen.dbc format string changed. It is id, name, race, sex - 'nsii'. Any other\n"
        "shape misreads all 12972 rows and the randomise button returns nothing or garbage.")
endif()

extract_function(_random_body "${_charh_src}" "void WorldSession::HandleRandomizeCharNameOpcode")
string(FIND "${_random_body}" "GetRandomCharacterName" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR "extract_function did not capture the randomise handler body -- assertions below would be vacuous")
endif()

string(FIND "${_random_body}" "recvPacket >> sex;\n    recvPacket >> race;" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The randomise-name request is no longer read as sex then race.\n\n"
        "That order is from the client, not a reference server: RequestRandomName reads the\n"
        "char-create globals at +0x40 and +0x44; the packet constructor stores the +0x44 byte\n"
        "at object offset 0x20 and +0x40 at 0x21, and the body writer emits 0x20 first. The\n"
        "CreateCharacter path lays the same globals out as race, class, gender, fixing +0x40\n"
        "as race and +0x44 as sex. Swapping them reads a Gnome female as race 1 sex 7.")
endif()

string(FIND "${_random_body}" "if (!name)" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The NameGen null guard is gone.\n\n"
        "GetRandomCharacterName returns NULL for a race/sex the DBC does not cover. Without\n"
        "the guard the name is dereferenced unconditionally and the send crashes; with it we\n"
        "answer with the failure bit, which the client recovers from by generating a name\n"
        "locally.")
endif()

message(STATUS "char response codes guard: 2 wire-confirmed anchors, 5 restored enumerators, "
               "delete success at 0x48, no duplicate enum, char-select timezone present, "
               "5 registrations, NameGen 'nsii' + sex-then-race + null guard")
