# Locks the login send order against the retail 18414 sequence.
#
# The order is not a matter of taste. Eight real logins were extracted from the
# capture corpus and compared: 38 SMSG kinds appear in every one of them, 664
# ordered pairs hold in all eight, and only 39 pairs ever differ -- and those
# cluster on a handful of opcodes (SET_PCT_SPELL_MODIFIER, WEATHER, the
# PVP_SEASON/GUILD_QUERY_RANKS pair) which retail itself sent both ways, so the
# client demonstrably tolerates them.
#
# This guard asserts that the sends we DO implement appear in the source in the
# same relative order retail uses. It deliberately says nothing about the sends
# we do not implement yet; adding those is a content question, not an ordering
# one. What it prevents is silent drift back out of order.
#
# Retail spine positions are given per row so the intent survives a rename.

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(character_handler "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp")
set(player_source "${SOURCE_ROOT}/src/game/Object/Player.cpp")

foreach(required IN ITEMS "${character_handler}" "${player_source}")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Required source file is missing: ${required}")
    endif()
endforeach()

file(READ "${character_handler}" login_source)
file(READ "${player_source}" before_add_source)

if(DEFINED MUTATION)
    if(MUTATION STREQUAL "account_data_after_verify")
        # Restore the pre-fix order: account data sent after the world verify.
        string(REPLACE "SendAccountDataTimes(PER_CHARACTER_CACHE_MASK);\n    SendTutorialsData();"
            "SendTutorialsData();" login_source "${login_source}")
    elseif(MUTATION STREQUAL "drop_tutorials")
        string(REPLACE "SendTutorialsData();" "/* tutorials not sent */" login_source "${login_source}")
    elseif(MUTATION STREQUAL "factions_before_world_states")
        string(REPLACE "SendInitWorldStates(GetZoneId(), GetAreaId());"
            "m_reputationMgr.SendInitialReputations();\n    SendInitWorldStates(GetZoneId(), GetAreaId());"
            before_add_source "${before_add_source}")
    endif()
endif()

# ---------------------------------------------------------------- helpers ----
# Commented-out sends must not count. Player.cpp keeps a disabled
# //m_reputationMgr.SendInitialReputations() from an earlier arrangement, and
# matching that instead of the live call would lock in an order nothing sends.
function(strip_line_comments raw out_var)
    string(REGEX REPLACE "[ \t]*//[^\n]*" "" stripped "${raw}")
    set(${out_var} "${stripped}" PARENT_SCOPE)
endfunction()

function(require_ordered source_text label)
    set(previous_offset -1)
    set(previous_name "")
    foreach(entry IN LISTS ARGN)
        string(REPLACE "|" ";" parts "${entry}")
        list(GET parts 0 needle)
        list(GET parts 1 spine_pos)
        string(FIND "${source_text}" "${needle}" found)
        if(found EQUAL -1)
            message(FATAL_ERROR
                "${label}: retail spine position ${spine_pos} is not sent at all -- '${needle}' is absent")
        endif()
        if(found LESS previous_offset)
            message(FATAL_ERROR
                "${label}: '${needle}' (retail spine ${spine_pos}) is sent before '${previous_name}', "
                "inverting an ordering retail keeps in all eight captured logins")
        endif()
        set(previous_offset "${found}")
        set(previous_name "${needle}")
    endforeach()
endfunction()

# ---- CharacterHandler: the opening of the sequence ---------------------------
# Retail: ACCOUNT_DATA_TIMES(0), TUTORIAL_FLAGS(1), LOGIN_VERIFY_WORLD(2),
# FEATURE_SYSTEM_STATUS(4), MOTD(5). The ACCOUNT_DATA_TIMES / LOGIN_VERIFY_WORLD
# pair never inverts across the corpus, and both must precede the world verify.
strip_line_comments("${login_source}" login_source)
strip_line_comments("${before_add_source}" before_add_source)

require_ordered("${login_source}" "login open"
    "SendAccountDataTimes(PER_CHARACTER_CACHE_MASK)|0"
    "SendTutorialsData()|1"
    "SMSG_LOGIN_VERIFY_WORLD|2"
    "SMSG_FEATURE_SYSTEM_STATUS|4")

# ---- Player::SendInitialPacketsBeforeAddToMap: the middle -------------------
# Retail: SEND_UNLEARN_SPELLS(24), ACTION_BUTTONS(25), CORPSE_RECLAIM_DELAY(26),
# INIT_WORLD_STATES(27), INITIALIZE_FACTIONS(30), LOAD_EQUIPMENT_SET(35).
# Reputations following the world states is the ordering this guard exists for:
# it was inverted, and it was the only inversion left once account data and
# tutorials moved to the front.
require_ordered("${before_add_source}" "before-add block"
    "SMSG_SEND_UNLEARN_SPELLS|24"
    "SendInitialActionButtons()|25"
    "SendInitWorldStates(GetZoneId(), GetAreaId())|27"
    "m_reputationMgr.SendInitialReputations()|30"
    "SendEquipmentSetList()|35")

message(STATUS "login packet order matches the retail spine")
