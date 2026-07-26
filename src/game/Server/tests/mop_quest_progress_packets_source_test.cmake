file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerQuest.cpp" quest_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "field_order")
    string(REPLACE "out << credit.count << uint8(credit.type) << credit.questId;"
        "out << credit.questId << uint8(credit.type) << credit.count;"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "gameobject_type")
    string(REPLACE "GameObject = 2," "GameObject = 1,"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "interact_type")
    string(REPLACE "CreatureInteract = 3," "CreatureInteract = 4,"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "guid_mask")
    string(REPLACE "{ 0, 4, 2, 6, 1, 5, 7, 3 }"
        "{ 0, 4, 2, 6, 1, 5, 3, 7 }" player_header "${player_header}")
elseif(MUTATION STREQUAL "guid_bytes")
    string(REPLACE "{ 2, 7, 3, 0, 4, 5, 1, 6 }"
        "{ 2, 7, 3, 0, 4, 5, 6, 1 }" player_header "${player_header}")
elseif(MUTATION STREQUAL "sender")
    string(REPLACE "MopQuestPackets::BuildQuestProgressCredit(data, credit);"
        "/* removed quest-progress build */" quest_source "${quest_source}")
elseif(MUTATION STREQUAL "kill_context")
    string(REPLACE "MopQuestPackets::QuestProgressObjectiveType::CreatureKill);"
        "MopQuestPackets::QuestProgressObjectiveType::CreatureInteract);"
        quest_source "${quest_source}")
elseif(MUTATION STREQUAL "gameobject_context")
    string(REPLACE "MopQuestPackets::QuestProgressObjectiveType::GameObject"
        "MopQuestPackets::QuestProgressObjectiveType::CreatureKill"
        quest_source "${quest_source}")
elseif(MUTATION STREQUAL "talk_context")
    string(REPLACE "MopQuestPackets::QuestProgressObjectiveType::CreatureInteract);"
        "MopQuestPackets::QuestProgressObjectiveType::CreatureKill);"
        quest_source "${quest_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_QUESTUPDATE_ADD_KILL, \"SMSG_QUESTUPDATE_ADD_KILL\");"
        "/* removed SMSG_QUESTUPDATE_ADD_KILL registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_QUESTUPDATE_ADD_KILL:"
        "case REMOVED_SMSG_QUESTUPDATE_ADD_KILL:"
        session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode")
    string(REPLACE "SMSG_QUESTUPDATE_ADD_KILL                    = 0x1645,"
        "SMSG_QUESTUPDATE_ADD_KILL                    = 0x1644,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_QUESTUPDATE_ADD_KILL                      0x1645  ACTIVE"
        "SMSG_QUESTUPDATE_ADD_KILL                      0x1645  DORMANT"
        opcode_reference "${opcode_reference}")
endif()

function(require_once source token context)
    string(FIND "${source}" "${token}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "${context}: required token missing: ${token}")
    endif()
    math(EXPR next "${first} + 1")
    string(SUBSTRING "${source}" ${next} -1 tail)
    string(FIND "${tail}" "${token}" duplicate)
    if(NOT duplicate EQUAL -1)
        message(FATAL_ERROR "${context}: duplicate token: ${token}")
    endif()
endfunction()

require_once("${player_header}" "CreatureKill = 0," "creature-kill objective type")
require_once("${player_header}" "GameObject = 2," "GameObject objective type")
require_once("${player_header}" "CreatureInteract = 3," "creature-interact objective type")
require_once("${player_header}" "out << credit.count << uint8(credit.type) << credit.questId;"
    "18414 fixed-field order")
require_once("${player_header}" "out << credit.requiredCount << credit.objectId;"
    "18414 required-count/object-id order")
require_once("${player_header}" "{ 0, 4, 2, 6, 1, 5, 7, 3 }"
    "18414 credited-target GUID mask")
require_once("${player_header}" "{ 2, 7, 3, 0, 4, 5, 1, 6 }"
    "18414 credited-target GUID bytes")

require_once("${quest_source}" "MopQuestPackets::BuildQuestProgressCredit(data, credit);"
    "quest-progress sender")
require_once("${quest_source}" "MopQuestPackets::QuestProgressObjectiveType::CreatureKill);"
    "kill-credit type selection")
require_once("${quest_source}"
    "isCreature ? MopQuestPackets::QuestProgressObjectiveType::CreatureInteract :"
    "creature cast type selection")
require_once("${quest_source}"
    "MopQuestPackets::QuestProgressObjectiveType::GameObject);"
    "GameObject cast type selection")
require_once("${quest_source}" "MopQuestPackets::QuestProgressObjectiveType::CreatureInteract);"
    "talk-credit type selection")

require_once("${opcode_registry}"
    "DefS(SMSG_QUESTUPDATE_ADD_KILL, \"SMSG_QUESTUPDATE_ADD_KILL\");"
    "quest-progress registration")
require_once("${session_source}" "case SMSG_QUESTUPDATE_ADD_KILL:"
    "quest-progress converted-packet admission")
require_once("${opcode_header}"
    "SMSG_QUESTUPDATE_ADD_KILL                    = 0x1645,"
    "direct 18414 opcode")
require_once("${opcode_reference}"
    "SMSG_QUESTUPDATE_ADD_KILL                      0x1645  ACTIVE"
    "active direct-client reference")

string(FIND "${quest_source}" "| 0x80000000" legacy_high_bit)
if(NOT legacy_high_bit EQUAL -1)
    message(FATAL_ERROR "legacy GameObject high-bit encoding remains")
endif()
