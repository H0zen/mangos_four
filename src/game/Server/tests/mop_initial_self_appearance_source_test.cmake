file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Map.cpp" map_source)

if(MUTATION STREQUAL "visible_item_feed")
    string(REPLACE
        "MopUpdateObject::ObserverVisibleItemSourceStart + i"
        "MopUpdateObject::SelfInventorySourceStart + i"
        map_source "${map_source}")
elseif(MUTATION STREQUAL "combined_builder")
    string(REPLACE
        "MopUpdateObject::TranslateSelfPlayerFields"
        "MopUpdateObject::AppendSelfInventoryValuesBlock"
        map_source "${map_source}")
elseif(MUTATION STREQUAL "values_block_seed")
    string(REPLACE
        "MopUpdateObject::AppendSelfCreateBlock"
        "MopUpdateObject::AppendSelfPlayerValuesBlock"
        map_source "${map_source}")
elseif(MUTATION STREQUAL "questlog_feed")
    string(REPLACE
        "MopUpdateObject::SelfQuestLogSlotCount"
        "MopUpdateObject::SelfInventoryFieldCount"
        map_source "${map_source}")
elseif(MUTATION STREQUAL "skill_feed")
    string(REPLACE
        "MopUpdateObject::SelfSkillSourceStart + i"
        "MopUpdateObject::SelfInventorySourceStart + i"
        map_source "${map_source}")
endif()

string(FIND "${map_source}" "void Map::SendInitSelf(Player* player)" self_start)
string(FIND "${map_source}" "void Map::SendInitTransports(Player* player)" next_start)
if(self_start EQUAL -1 OR next_start EQUAL -1 OR NOT self_start LESS next_start)
    message(FATAL_ERROR "could not isolate Map::SendInitSelf")
endif()
math(EXPR self_length "${next_start} - ${self_start}")
string(SUBSTRING "${map_source}" ${self_start} ${self_length} self_body)

function(require_once token context)
    string(REGEX MATCHALL "${token}" matches "${self_body}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

require_once(
    "MopUpdateObject::ObserverVisibleItemSourceStart \\+ i"
    "initial self visible-item snapshot")
require_once(
    "MopUpdateObject::SelfInventorySourceStart \\+ i"
    "initial self inventory snapshot")
require_once(
    "MopUpdateObject::SelfSkillSourceStart \\+ i"
    "initial self skill snapshot")
require_once(
    "MopUpdateObject::SelfExploredSourceStart"
    "initial self explored-zone and rested-pool snapshot")
require_once(
    "PLAYER_FIELD_COINAGE"
    "initial self coinage/XP snapshot")
require_once(
    "MopUpdateObject::SelfQuestLogSourceStart"
    "initial self quest-log snapshot")
require_once(
    "MopUpdateObject::TranslateSelfPlayerFields"
    "combined initial self projection")
require_once(
    "MopUpdateObject::AppendSelfCreateBlock"
    "seed carried in the self create block")

string(FIND "${self_body}"
    "MopUpdateObject::AppendSelfInventoryValuesBlock" inventory_only)
if(NOT inventory_only EQUAL -1)
    message(FATAL_ERROR
        "initial self snapshot regressed to the inventory-only VALUES builder")
endif()

# The seeded fields must ride IN the create block, not arrive after it as a
# VALUES update. The client fires UI feedback for a value it sees CHANGE but
# not for one present in the create, so seeding via VALUES is what made login
# play the money sound, a spurious quest-accept sound and skill-up
# announcements. Retail carries these in the create; regressing to
# AppendSelfPlayerValuesBlock here would bring all three back.
string(FIND "${self_body}"
    "MopUpdateObject::AppendSelfPlayerValuesBlock" post_create_seed)
if(NOT post_create_seed EQUAL -1)
    message(FATAL_ERROR
        "initial self seed regressed to a post-create VALUES block")
endif()

# TranslateSelfPlayerFields asserts strictly ascending source indices. The
# quest-log range is 166..415, below visible items at 916, so its loop must be
# emitted first or the block is built out of order and trips that assert.
string(FIND "${self_body}" "MopUpdateObject::SelfQuestLogSlotCount" questlog_pos)
string(FIND "${self_body}" "MopUpdateObject::ObserverVisibleItemSourceStart + i" visible_pos)
if(questlog_pos EQUAL -1 OR visible_pos EQUAL -1)
    message(FATAL_ERROR "initial self snapshot is missing a required seed loop")
endif()
if(NOT questlog_pos LESS visible_pos)
    message(FATAL_ERROR
        "quest-log seed must precede the visible-item seed because AppendSelfPlayerValuesBlock requires ascending legacy indices")
endif()
