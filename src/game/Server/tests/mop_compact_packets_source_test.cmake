file(READ "${SOURCE_ROOT}/src/game/Object/PlayerCombat.cpp" player_combat)
file(READ "${SOURCE_ROOT}/src/game/Object/UnitSpeed.cpp" unit_speed)
file(READ "${SOURCE_ROOT}/src/game/ChatCommands/PlayerStatsMods.cpp" player_stats_mods)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GroupHandler.cpp" group_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/InstanceData.cpp" instance_data)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerInstance.cpp" player_instance)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" character_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Object/UnitCombat.cpp" unit_combat)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CombatHandler.cpp" combat_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ItemHandler.cpp" item_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

if(MUTATION STREQUAL "cancel_sender")
    string(REPLACE
        "WorldPacket data(SMSG_CANCEL_COMBAT, 0);"
        "WorldPacket data(SMSG_CANCEL_COMBAT, 4);"
        player_combat "${player_combat}")
elseif(MUTATION STREQUAL "cancel_registration")
    string(REPLACE
        "DefS(SMSG_CANCEL_COMBAT, \"SMSG_CANCEL_COMBAT\");"
        "/* removed cancel-combat registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "cancel_allowlist")
    string(REPLACE
        "case SMSG_CANCEL_COMBAT:"
        "case 0xFFFF: /* removed cancel-combat allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "cancel_opcode")
    string(REPLACE
        "SMSG_CANCEL_COMBAT                           = 0x0E8B"
        "SMSG_CANCEL_COMBAT                           = 0x0E8A"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "cancel_reference")
    string(REPLACE
        "SMSG_CANCEL_COMBAT                             0x0E8B  ACTIVE"
        "SMSG_CANCEL_COMBAT                             0x0E8B  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "cancel_old_route")
    string(REPLACE
        "SMSG_UNKNOWN_0x0534                            0x0534  DOC"
        "SMSG_CANCEL_COMBAT                             0x0534  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "read_item_alias")
    string(APPEND opcode_header "\nSMSG_READ_ITEM_FAILED = 0x0E8B,\n")
elseif(MUTATION STREQUAL "read_item_sender")
    string(APPEND item_handler "\nWorldPacket stale(SMSG_READ_ITEM_FAILED, 8);\n")
elseif(MUTATION STREQUAL "attacker_sender")
    string(REPLACE
        "MopCompactPackets::BuildAttackerStateUpdate(data, update);"
        "/* removed attacker-state builder */"
        unit "${unit}")
elseif(MUTATION STREQUAL "attacker_registration")
    string(REPLACE
        "DefS(SMSG_ATTACKERSTATEUPDATE, \"SMSG_ATTACKERSTATEUPDATE\");"
        "/* removed attacker-state registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "attacker_allowlist")
    string(REPLACE
        "case SMSG_ATTACKERSTATEUPDATE:"
        "case 0xFFFF: /* removed attacker-state allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "attacker_opcode")
    string(REPLACE
        "SMSG_ATTACKERSTATEUPDATE                     = 0x06AA"
        "SMSG_ATTACKERSTATEUPDATE                     = 0x06AB"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "attacker_reference")
    string(REPLACE
        "SMSG_ATTACKERSTATEUPDATE                       0x06AA  ACTIVE"
        "SMSG_ATTACKERSTATEUPDATE                       0x06AA  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "attacker_envelope")
    string(REPLACE
        "out.WriteBit(false);"
        "out.WriteBit(true);"
        unit_header "${unit_header}")
endif()

if(player_combat MATCHES "WorldPacket[ \t]+data\\(SMSG_ATTACKSWING_NOTINRANGE")
    message(FATAL_ERROR "legacy construction SMSG_ATTACKSWING_NOTINRANGE remains in its sender")
endif()
if(player_combat MATCHES "WorldPacket[ \t]+data\\(SMSG_ATTACKSWING_BADFACING")
    message(FATAL_ERROR "legacy construction SMSG_ATTACKSWING_BADFACING remains in its sender")
endif()
if(player_combat MATCHES "WorldPacket[ \t]+data\\(SMSG_ATTACKSWING_DEADTARGET")
    message(FATAL_ERROR "legacy construction SMSG_ATTACKSWING_DEADTARGET remains in its sender")
endif()
if(player_combat MATCHES "WorldPacket[ \t]+data\\(SMSG_ATTACKSWING_CANT_ATTACK")
    message(FATAL_ERROR "legacy construction SMSG_ATTACKSWING_CANT_ATTACK remains in its sender")
endif()
if(player_stats_mods MATCHES "Initialize\\(SMSG_FORCE_SWIM_SPEED_CHANGE")
    message(FATAL_ERROR "legacy construction SMSG_FORCE_SWIM_SPEED_CHANGE remains in its sender")
endif()
if(group_handler MATCHES "WorldPacket[ \t]+data\\(MSG_RANDOM_ROLL")
    message(FATAL_ERROR "legacy construction MSG_RANDOM_ROLL remains in its sender")
endif()
if(instance_data MATCHES "WorldPacket[ \t]+data\\(SMSG_INSTANCE_ENCOUNTER")
    message(FATAL_ERROR "legacy construction SMSG_INSTANCE_ENCOUNTER remains in its sender")
endif()
if(player_instance MATCHES "WorldPacket[ \t]+data\\(MSG_SET_RAID_DIFFICULTY")
    message(FATAL_ERROR "legacy construction MSG_SET_RAID_DIFFICULTY remains in its sender")
endif()
if(player_instance MATCHES "WorldPacket[ \t]+data\\(MSG_SET_DUNGEON_DIFFICULTY")
    message(FATAL_ERROR "legacy construction MSG_SET_DUNGEON_DIFFICULTY remains in its sender")
endif()
if(opcode_header MATCHES "[\r\n][ \t]*MSG_SET_DUNGEON_DIFFICULTY[ \t]*=")
    message(FATAL_ERROR "legacy MSG_SET_DUNGEON_DIFFICULTY opcode remains active")
endif()
if(NOT player_instance MATCHES "MopCompactPackets::BuildSetDungeonDifficulty")
    message(FATAL_ERROR "dungeon-difficulty sender bypasses the shared 5.4.8 serializer")
endif()
if(character_handler MATCHES "//[ \t]*pCurrChar->SendDungeonDifficulty\\(false\\)")
    message(FATAL_ERROR "5.4.8 login dungeon-difficulty send remains suppressed")
endif()

foreach(source_text IN ITEMS "${unit_speed}" "${player_stats_mods}")
    if(NOT source_text MATCHES "MopCompactPackets::BuildMoveSetSwimSpeed")
        message(FATAL_ERROR "swim-speed sender bypasses the shared 5.4.8 serializer")
    endif()
endforeach()

foreach(server_name IN ITEMS
        SMSG_ATTACKSWING_ERROR
        SMSG_MOVE_SET_SWIM_SPEED
        SMSG_RANDOM_ROLL
        SMSG_UPDATE_INSTANCE_ENCOUNTER_UNIT
        SMSG_SET_RAID_DIFFICULTY
        SMSG_SET_DUNGEON_DIFFICULTY
        SMSG_ATTACKSTART
        SMSG_ATTACKSTOP)
    if(NOT opcode_registry MATCHES "DefS\\(${server_name},[ \t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
endforeach()

foreach(source_text IN ITEMS "${unit_combat}" "${combat_handler}")
    if(source_text MATCHES "GetPackGUID\\(\\)")
        message(FATAL_ERROR "legacy attack packet pack-GUID writer remains")
    endif()
endforeach()

if(NOT player_combat MATCHES "WorldPacket[ \t]+data\\(SMSG_CANCEL_COMBAT,[ \t]*0\\)")
    message(FATAL_ERROR "cancel-combat sender must retain the proven empty body")
endif()
if(NOT opcode_registry MATCHES "DefS\\(SMSG_CANCEL_COMBAT,[ \t]*\"SMSG_CANCEL_COMBAT\"\\)")
    message(FATAL_ERROR "SMSG_CANCEL_COMBAT is missing outbound opcode metadata")
endif()
if(NOT world_session MATCHES "case[ \t]+SMSG_CANCEL_COMBAT:")
    message(FATAL_ERROR "SMSG_CANCEL_COMBAT is missing from the converted-packet gate")
endif()
if(NOT opcode_header MATCHES "SMSG_CANCEL_COMBAT[ \t]*=[ \t]*0x0E8B")
    message(FATAL_ERROR "SMSG_CANCEL_COMBAT does not use the direct 18414 value 0x0E8B")
endif()
if(NOT opcode_reference MATCHES "SMSG_CANCEL_COMBAT[ \t]+0x0E8B[ \t]+ACTIVE")
    message(FATAL_ERROR "reference inventory does not record active 0x0E8B cancel combat")
endif()
if(NOT opcode_reference MATCHES "SMSG_UNKNOWN_0x0534[ \t]+0x0534[ \t]+DOC")
    message(FATAL_ERROR "reference inventory does not quarantine the guild-result-like 0x0534 route")
endif()
if(opcode_header MATCHES "SMSG_READ_ITEM_FAILED")
    message(FATAL_ERROR "disproved SMSG_READ_ITEM_FAILED identity remains active")
endif()
if(item_handler MATCHES "SMSG_READ_ITEM_FAILED")
    message(FATAL_ERROR "read-item handler can still emit the disproved 0x0E8B identity")
endif()

if(NOT unit MATCHES "MopCompactPackets::BuildAttackerStateUpdate")
    message(FATAL_ERROR "attacker-state sender bypasses the shared 5.4.8 serializer")
endif()
if(unit MATCHES "WorldPacket[ 	]+data\\(SMSG_ATTACKERSTATEUPDATE")
    message(FATAL_ERROR "legacy bare attacker-state packet construction remains")
endif()
if(NOT opcode_registry MATCHES "DefS\\(SMSG_ATTACKERSTATEUPDATE,[ 	]*\"SMSG_ATTACKERSTATEUPDATE\"\\)")
    message(FATAL_ERROR "SMSG_ATTACKERSTATEUPDATE is missing outbound opcode metadata")
endif()
if(NOT world_session MATCHES "case[ 	]+SMSG_ATTACKERSTATEUPDATE:")
    message(FATAL_ERROR "SMSG_ATTACKERSTATEUPDATE is missing from the converted-packet gate")
endif()
if(NOT opcode_header MATCHES "SMSG_ATTACKERSTATEUPDATE[ 	]*=[ 	]*0x06AA")
    message(FATAL_ERROR "SMSG_ATTACKERSTATEUPDATE does not use the direct 18414 value 0x06AA")
endif()
if(NOT opcode_reference MATCHES "SMSG_ATTACKERSTATEUPDATE[ 	]+0x06AA[ 	]+ACTIVE")
    message(FATAL_ERROR "reference inventory does not record active 0x06AA attacker-state update")
endif()
if(NOT unit_header MATCHES "BuildAttackerStateUpdate")
    message(FATAL_ERROR "5.4.8 attacker-state serializer is missing")
endif()
if(NOT unit_header MATCHES "out.WriteBit\\(false\\)")
    message(FATAL_ERROR "attacker-state outer envelope does not omit optional metadata")
endif()
