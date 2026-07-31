file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Spell.h" spell_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Spell.cpp" spell_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellPackets.cpp" spell_packets)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellHandler.cpp" spell_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellEffectTail.cpp" spell_effect_tail)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/ChatCommands/DebugCommands.cpp" debug_commands)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.cpp" player_source)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerItemStorage.cpp" item_storage_source)
file(READ "${SOURCE_ROOT}/src/game/Object/PetSpells.cpp" pet_spells_source)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerSpell.cpp" player_spell_source)
file(READ "${SOURCE_ROOT}/src/game/Object/SpellCooldownMgr.cpp" spell_cooldown_mgr_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

if(MUTATION STREQUAL "reader")
    string(REPLACE
        "MopSpellPackets::ReadCastSpellRequest(recvPacket, request)"
        "false /* removed 18414 cast reader */"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE
        "DefC(CMSG_CAST_SPELL, \"CMSG_CAST_SPELL\""
        "DefC(0xFFFF, \"removed CMSG_CAST_SPELL\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "target_initializer")
    string(REPLACE
        "InitializeForCastRequest("
        "InitializeForRemovedCastRequest("
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "target_mapping")
    string(REPLACE
        "TARGET_FLAG_UNIT | TARGET_FLAG_UNK2"
        "TARGET_FLAG_UNIT | TARGET_FLAG_UNK2 | TARGET_FLAG_UNIT_UNK"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "movement_count_width")
    string(REPLACE
        "movement.forceCount = in.ReadBits(22);"
        "movement.forceCount = in.ReadBits(16); /* damaged movement count */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "movement_count_bound")
    string(REPLACE
        "movement.forceCount > (in.size() - in.rpos()) / sizeof(uint32)"
        "false /* removed movement-count bound */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "movement_flag_order")
    string(REPLACE
        "movement.hasMovementFlags = !in.ReadBit();\n        movement.hasTimestamp = !in.ReadBit();\n        movement.hasUnknownUInt32 = !in.ReadBit();\n        if (movement.hasMovementFlags)\n            in.ReadBits(30);"
        "movement.hasMovementFlags = !in.ReadBit();\n        if (movement.hasMovementFlags)\n            in.ReadBits(30); /* moved before presence bits */\n        movement.hasTimestamp = !in.ReadBit();\n        movement.hasUnknownUInt32 = !in.ReadBit();"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "missile_guard")
    string(REPLACE
        "if (hasMissileSpeed)\n            in >> missileSpeed;"
        "if (hasElevation)\n            in >> missileSpeed; /* swapped missile guard */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "elevation_guard")
    string(REPLACE
        "if (hasElevation)\n            in >> elevation;"
        "if (hasMissileSpeed)\n            in >> elevation; /* swapped elevation guard */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "full_consumption")
    string(REPLACE
        "if (in.rpos() != in.size())"
        "if (false /* removed full-consumption gate */)"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "string_bound")
    string(REPLACE
        "targetStringLength > in.size() - in.rpos()"
        "false /* removed target-string bound */"
        spell_source "${spell_source}")
elseif(MUTATION STREQUAL "legacy_prefix")
    string(REPLACE
        "if (!MopSpellPackets::ReadCastSpellRequest(recvPacket, request))"
        "uint8 cast_count; uint32 spellId, glyphIndex; uint8 cast_flags;\n    recvPacket >> cast_count;\n    recvPacket >> spellId >> glyphIndex;\n    recvPacket >> cast_flags;\n    if (!MopSpellPackets::ReadCastSpellRequest(recvPacket, request))"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "cast_failed_sender")
    string(REPLACE
        "MopSpellPackets::BuildCastFailed(data, spellInfo->ID, reportedResult,"
        "/* removed 18414 cast-failure builder */"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "cast_failed_registration")
    string(REPLACE
        "DefS(SMSG_CAST_FAILED, \"SMSG_CAST_FAILED\");"
        "/* removed SMSG_CAST_FAILED registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "pet_cast_failed_registration")
    string(REPLACE
        "DefS(SMSG_PET_CAST_FAILED, \"SMSG_PET_CAST_FAILED\");"
        "/* removed SMSG_PET_CAST_FAILED registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "cast_failed_gate")
    string(REPLACE
        "case SMSG_CAST_FAILED:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "pet_cast_failed_gate")
    string(REPLACE
        "case SMSG_PET_CAST_FAILED:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "cast_result_translation")
    string(REPLACE
        "if (result <= 118)"
        "if (result <= 117)"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "pet_presence_order")
    string(REPLACE
        "out.WriteBit(!arguments.hasArg18);\n        out.WriteBit(!arguments.hasArg10);"
        "out.WriteBit(!arguments.hasArg10);\n        out.WriteBit(!arguments.hasArg18);"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "debug_cast_failed_sender")
    string(REPLACE
        "MopSpellPackets::BuildCastFailed(data, 133, SpellCastResult(failnum), 0, false, arguments);"
        "WorldPacket data(SMSG_CAST_FAILED, 5);"
        debug_commands "${debug_commands}")
elseif(MUTATION STREQUAL "spell_start_sender")
    string(REPLACE
        "MopSpellPackets::BuildSpellStart(data, spell)"
        "false /* removed 18414 spell-start builder */"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_start_registration")
    string(REPLACE
        "DefS(SMSG_SPELL_START, \"SMSG_SPELL_START\");"
        "/* removed SMSG_SPELL_START registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "spell_start_gate")
    string(REPLACE
        "case SMSG_SPELL_START:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "spell_start_target_mask_width")
    string(REPLACE
        "out.WriteBits(spell.targetMask, 20);"
        "out.WriteBits(spell.targetMask, 21);"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_start_caster_mask")
    string(REPLACE
        "out.WriteGuidMask<2, 6>(spell.casterUnitGuid);"
        "out.WriteGuidMask<6, 2>(spell.casterUnitGuid);"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_start_string_bound")
    string(REPLACE
        "spell.targetString.size() > 0x7F"
        "spell.targetString.size() > 0xFF"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_go_sender")
    string(REPLACE
        "MopSpellPackets::BuildSpellGo(data, spell)"
        "false /* removed 18414 spell-go builder */"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_go_registration")
    string(REPLACE
        "DefS(SMSG_SPELL_GO, \"SMSG_SPELL_GO\");"
        "/* removed SMSG_SPELL_GO registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "spell_go_gate")
    string(REPLACE
        "case SMSG_SPELL_GO:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "spell_go_miss_reason_width")
    string(REPLACE
        "out.WriteBits(miss.reason, 4);"
        "out.WriteBits(miss.reason, 5);"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_go_hit_mask")
    string(REPLACE
        "out.WriteGuidMask<2, 7, 1, 6, 4, 5, 0, 3>(hit);"
        "out.WriteGuidMask<7, 2, 1, 6, 4, 5, 0, 3>(hit);"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_go_string_bound")
    string(REPLACE
        "if ((spell.hasTargetString && spell.targetString.size() > 0x7F)"
        "if ((spell.hasTargetString && spell.targetString.size() > 0xFF)"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "spell_go_trajectory")
    string(REPLACE
        "if (m_targets.GetSpeed() > 0.0f)"
        "if (false /* removed trajectory flag */)"
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "category_request_registration")
    string(REPLACE
        "DefC(CMSG_REQUEST_CATEGORY_COOLDOWNS, \"CMSG_REQUEST_CATEGORY_COOLDOWNS\""
        "DefC(0xFFFF, \"removed CMSG_REQUEST_CATEGORY_COOLDOWNS\""
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "category_response_registration")
    string(REPLACE
        "DefS(SMSG_CATEGORY_COOLDOWN, \"SMSG_CATEGORY_COOLDOWN\");"
        "/* removed SMSG_CATEGORY_COOLDOWN registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "category_response_gate")
    string(REPLACE
        "case SMSG_CATEGORY_COOLDOWN:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "category_request_reader")
    string(REPLACE
        "MopSpellPackets::ReadCategoryCooldownRequest(recvPacket)"
        "false /* removed empty-request validation */"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "category_response_layout")
    string(REPLACE
        "out.WriteBits(uint32(records.size()), 21);"
        "out.WriteBits(uint32(records.size()), 20);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "category_aura_aggregation")
    string(REPLACE
        "GetAurasByType(SPELL_AURA_MOD_SPELL_CATEGORY_COOLDOWN)"
        "GetAurasByType(SPELL_AURA_DUMMY)"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "cooldown_mask_order")
    string(REPLACE
        "out.WriteGuidMask<0, 6>(ownerGuid);"
        "out.WriteGuidMask<6, 0>(ownerGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "cooldown_presence_bit")
    string(REPLACE
        "out.WriteBit(flags == 0);"
        "out.WriteBit(flags != 0);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "cooldown_count_width")
    string(REPLACE
        "out.WriteBits(uint32(cooldowns.size()), 21);"
        "out.WriteBits(uint32(cooldowns.size()), 20);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "cooldown_byte_order")
    string(REPLACE
        "out.WriteGuidBytes<5, 3, 7>(ownerGuid);"
        "out.WriteGuidBytes<3, 5, 7>(ownerGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "cooldown_player_sender")
    string(REPLACE
        "MopSpellPackets::BuildSpellCooldown(data, GetObjectGuid(), 0, cooldowns);"
        "/* removed player cooldown sender */"
        player_source "${player_source}")
elseif(MUTATION STREQUAL "cooldown_item_sender")
    string(REPLACE
        "MopSpellPackets::BuildSpellCooldown(data, GetObjectGuid(), 1, cooldowns);"
        "/* removed item cooldown sender */"
        item_storage_source "${item_storage_source}")
elseif(MUTATION STREQUAL "cooldown_pet_sender")
    string(REPLACE
        "MopSpellPackets::BuildSpellCooldown(data, GetObjectGuid(), 0, cooldowns);"
        "/* removed pet cooldown sender */"
        pet_spells_source "${pet_spells_source}")
elseif(MUTATION STREQUAL "cooldown_registration")
    string(REPLACE
        "DefS(SMSG_SPELL_COOLDOWN, \"SMSG_SPELL_COOLDOWN\");"
        "/* removed SMSG_SPELL_COOLDOWN registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "cooldown_gate")
    string(REPLACE
        "case SMSG_SPELL_COOLDOWN:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "cooldown_reference")
    string(REPLACE
        "SMSG_SPELL_COOLDOWN                            0x0452  ACTIVE"
        "SMSG_SPELL_COOLDOWN                            0x0452  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "clear_cooldowns_mask_order")
    string(REPLACE
        "out.WriteGuidMask<5, 6, 7, 3, 2>(ownerGuid);"
        "out.WriteGuidMask<6, 5, 7, 3, 2>(ownerGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "clear_cooldowns_count_width")
    string(REPLACE
        "out.WriteBits(uint32(spellIds.size()), 22);"
        "out.WriteBits(uint32(spellIds.size()), 24);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "clear_cooldowns_byte_order")
    string(REPLACE
        "out.WriteGuidBytes<0, 1, 7, 4, 3, 5, 6>(ownerGuid);"
        "out.WriteGuidBytes<1, 0, 7, 4, 3, 5, 6>(ownerGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "clear_cooldowns_player_sender")
    string(REPLACE
        "MopSpellPackets::BuildClearCooldowns(data, target->GetObjectGuid(), {spell_id});"
        "/* removed single cooldown-clear sender */"
        player_source "${player_source}")
elseif(MUTATION STREQUAL "clear_cooldowns_manager_sender")
    string(REPLACE
        "MopSpellPackets::BuildClearCooldowns(data, m_owner->GetObjectGuid(), spellIds);"
        "/* removed all-cooldowns-clear sender */"
        spell_cooldown_mgr_source "${spell_cooldown_mgr_source}")
elseif(MUTATION STREQUAL "clear_cooldowns_registration")
    string(REPLACE
        "DefS(SMSG_CLEAR_COOLDOWNS, \"SMSG_CLEAR_COOLDOWNS\");"
        "/* removed SMSG_CLEAR_COOLDOWNS registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "clear_cooldowns_gate")
    string(REPLACE
        "case SMSG_CLEAR_COOLDOWNS:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "clear_cooldowns_reference")
    string(REPLACE
        "SMSG_CLEAR_COOLDOWNS                           0x1458  ACTIVE"
        "SMSG_CLEAR_COOLDOWNS                           0x1458  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "spellbook_wire_order")
    string(REPLACE
        "out << newSpellId << oldSpellId;"
        "out << oldSpellId << newSpellId;"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "spellbook_sender")
    string(REPLACE
        "MopSpellPackets::BuildLearnedSpell(data, spell_id, false);"
        "/* removed learned-spell sender */"
        player_spell_source "${player_spell_source}")
elseif(MUTATION STREQUAL "spellbook_registration")
    string(REPLACE
        "DefS(SMSG_LEARNED_SPELL, \"SMSG_LEARNED_SPELL\");"
        "/* removed learned-spell registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "spellbook_gate")
    string(REPLACE
        "case SMSG_LEARNED_SPELL:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "spellbook_reference")
    string(REPLACE
        "SMSG_LEARNED_SPELL                             0x129A  ACTIVE"
        "SMSG_LEARNED_SPELL                             0x129A  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "pet_spellbook_builder")
    string(REPLACE
        "inline void BuildPetLearnedSpell(WorldPacket& out, uint32 spellId)"
        "inline void RemovedPetLearnedSpell(WorldPacket& out, uint32 spellId)"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "pet_spellbook_sender")
    string(REPLACE
        "MopSpellPackets::BuildPetLearnedSpell(data, spell_id);"
        "/* removed pet learned-spell sender */"
        pet_spells_source "${pet_spells_source}")
elseif(MUTATION STREQUAL "pet_spellbook_registration")
    string(REPLACE
        "DefS(SMSG_PET_LEARNED_SPELL, \"SMSG_PET_LEARNED_SPELL\");"
        "/* removed pet learned-spell registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "pet_spellbook_gate")
    string(REPLACE
        "case SMSG_PET_LEARNED_SPELL:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "pet_spellbook_reference")
    string(REPLACE
        "SMSG_PET_LEARNED_SPELL                         0x0282  ACTIVE"
        "SMSG_PET_LEARNED_SPELL                         0x0282  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "cooldown_event_mask_order")
    string(REPLACE
        "out.WriteGuidMask<4, 7, 1, 5, 6, 0, 2, 3>(ownerGuid);"
        "out.WriteGuidMask<7, 4, 1, 5, 6, 0, 2, 3>(ownerGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "cooldown_event_byte_order")
    string(REPLACE
        "out.WriteGuidBytes<5, 7>(ownerGuid);"
        "out.WriteGuidBytes<7, 5>(ownerGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "cooldown_event_sender")
    string(REPLACE
        "MopSpellPackets::BuildCooldownEvent(data, m_owner->GetObjectGuid(), spellInfo->ID);"
        "/* removed cooldown-event sender */"
        spell_cooldown_mgr_source "${spell_cooldown_mgr_source}")
elseif(MUTATION STREQUAL "cooldown_event_registration")
    string(REPLACE
        "DefS(SMSG_COOLDOWN_EVENT, \"SMSG_COOLDOWN_EVENT\");"
        "/* removed SMSG_COOLDOWN_EVENT registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "cooldown_event_gate")
    string(REPLACE
        "case SMSG_COOLDOWN_EVENT:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "cooldown_event_reference")
    string(REPLACE
        "SMSG_COOLDOWN_EVENT                            0x1163  ACTIVE"
        "SMSG_COOLDOWN_EVENT                            0x1163  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "item_cooldown_wire_order")
    string(REPLACE
        "out << itemGuid << spellId;"
        "out << spellId << itemGuid;"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "item_cooldown_sender")
    string(REPLACE
        "MopSpellPackets::BuildItemCooldown(data, pItem->GetObjectGuid(), spellData.SpellId);"
        "/* removed item cooldown sender */"
        player_source "${player_source}")
elseif(MUTATION STREQUAL "item_cooldown_registration")
    string(REPLACE
        "DefS(SMSG_ITEM_COOLDOWN, \"SMSG_ITEM_COOLDOWN\");"
        "/* removed SMSG_ITEM_COOLDOWN registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "item_cooldown_gate")
    string(REPLACE
        "case SMSG_ITEM_COOLDOWN:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "item_cooldown_reference")
    string(REPLACE
        "SMSG_ITEM_COOLDOWN                             0x1904  ACTIVE"
        "SMSG_ITEM_COOLDOWN                             0x1904  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "clear_target_mask_order")
    string(REPLACE
        "out.WriteGuidMask<6, 2, 0, 4, 7, 1, 3, 5>(targetGuid);"
        "out.WriteGuidMask<2, 6, 0, 4, 7, 1, 3, 5>(targetGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "clear_target_byte_order")
    string(REPLACE
        "out.WriteGuidBytes<4, 0, 3, 5, 2, 7, 6, 1>(targetGuid);"
        "out.WriteGuidBytes<0, 4, 3, 5, 2, 7, 6, 1>(targetGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "clear_target_sender")
    string(REPLACE
        "MopSpellPackets::BuildClearTarget(data, unitTarget->GetObjectGuid());"
        "/* removed clear-target sender */"
        spell_effect_tail "${spell_effect_tail}")
elseif(MUTATION STREQUAL "clear_target_registration")
    string(REPLACE
        "DefS(SMSG_CLEAR_TARGET, \"SMSG_CLEAR_TARGET\");"
        "/* removed SMSG_CLEAR_TARGET registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "clear_target_gate")
    string(REPLACE
        "case SMSG_CLEAR_TARGET:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "clear_target_reference")
    string(REPLACE
        "SMSG_CLEAR_TARGET                              0x1061  ACTIVE"
        "SMSG_CLEAR_TARGET                              0x1061  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "channel_start_mask_order")
    string(REPLACE
        "out.WriteGuidMask<7, 5, 4, 1>(casterGuid);"
        "out.WriteGuidMask<5, 7, 4, 1>(casterGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "channel_update_byte_order")
    string(REPLACE
        "out.WriteGuidBytes<4, 7, 1, 2, 6, 5>(casterGuid);"
        "out.WriteGuidBytes<7, 4, 1, 2, 6, 5>(casterGuid);"
        spell_header "${spell_header}")
elseif(MUTATION STREQUAL "channel_sender")
    string(REPLACE
        "MopSpellPackets::BuildChannelStart("
        "/* removed channel-start builder */ ("
        spell_packets "${spell_packets}")
elseif(MUTATION STREQUAL "channel_registration")
    string(REPLACE
        "DefS(SMSG_CHANNEL_START, \"SMSG_CHANNEL_START\");"
        "/* removed channel-start registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "channel_gate")
    string(REPLACE
        "case SMSG_CHANNEL_START:"
        "case SMSG_UNKNOWN_0:"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "channel_reference")
    string(REPLACE
        "SMSG_CHANNEL_START                             0x10F9  ACTIVE"
        "SMSG_CHANNEL_START                             0x10F9  DORMANT"
        opcode_reference "${opcode_reference}")
endif()

string(FIND "${spell_handler}" "void WorldSession::HandleCastSpellOpcode" cast_start)
string(FIND "${spell_handler}" "void WorldSession::HandleCancelCastOpcode" cast_end)
if(cast_start EQUAL -1 OR cast_end EQUAL -1 OR cast_end LESS_EQUAL cast_start)
    message(FATAL_ERROR "could not isolate HandleCastSpellOpcode")
endif()
math(EXPR cast_length "${cast_end} - ${cast_start}")
string(SUBSTRING "${spell_handler}" ${cast_start} ${cast_length} cast_handler)

string(FIND "${spell_source}" "void ReadCastSpellMovementBits" cast_movement_bits_begin)
string(FIND "${spell_source}" "void ReadCastSpellMovementBytes" cast_movement_bits_end)
if(cast_movement_bits_begin EQUAL -1 OR cast_movement_bits_end EQUAL -1
        OR cast_movement_bits_end LESS_EQUAL cast_movement_bits_begin)
    message(FATAL_ERROR "could not isolate ReadCastSpellMovementBits")
endif()
math(EXPR cast_movement_bits_length "${cast_movement_bits_end} - ${cast_movement_bits_begin}")
string(SUBSTRING "${spell_source}" ${cast_movement_bits_begin} ${cast_movement_bits_length}
    cast_movement_bits)

string(FIND "${spell_source}" "bool MopSpellPackets::ReadCastSpellRequest" cast_request_begin)
if(cast_request_begin EQUAL -1 OR cast_request_begin LESS_EQUAL cast_movement_bits_end)
    message(FATAL_ERROR "could not isolate ReadCastSpellMovementBytes")
endif()
math(EXPR cast_movement_bytes_length "${cast_request_begin} - ${cast_movement_bits_end}")
string(SUBSTRING "${spell_source}" ${cast_movement_bits_end} ${cast_movement_bytes_length}
    cast_movement_bytes)

string(FIND "${spell_source}" "namespace\n{\n    struct UseItemMovement" use_item_helpers_begin)
if(use_item_helpers_begin EQUAL -1 OR use_item_helpers_begin LESS_EQUAL cast_request_begin)
    message(FATAL_ERROR "could not isolate ReadCastSpellRequest")
endif()
math(EXPR cast_request_length "${use_item_helpers_begin} - ${cast_request_begin}")
string(SUBSTRING "${spell_source}" ${cast_request_begin} ${cast_request_length} cast_request)

string(FIND "${spell_packets}" "void Spell::SendSpellStart()" spell_start_begin)
string(FIND "${spell_packets}" "void Spell::SendSpellGo()" spell_start_end)
if(spell_start_begin EQUAL -1 OR spell_start_end EQUAL -1 OR spell_start_end LESS_EQUAL spell_start_begin)
    message(FATAL_ERROR "could not isolate SendSpellStart")
endif()
math(EXPR spell_start_length "${spell_start_end} - ${spell_start_begin}")
string(SUBSTRING "${spell_packets}" ${spell_start_begin} ${spell_start_length} spell_start_sender_body)

string(FIND "${spell_packets}" "bool MopSpellPackets::BuildSpellStart" spell_start_builder_begin)
string(FIND "${spell_packets}" "bool MopSpellPackets::BuildSpellGo" spell_start_builder_end)
if(spell_start_builder_begin EQUAL -1 OR spell_start_builder_end EQUAL -1
        OR spell_start_builder_end LESS_EQUAL spell_start_builder_begin)
    message(FATAL_ERROR "could not isolate BuildSpellStart")
endif()
math(EXPR spell_start_builder_length "${spell_start_builder_end} - ${spell_start_builder_begin}")
string(SUBSTRING "${spell_packets}" ${spell_start_builder_begin} ${spell_start_builder_length} spell_start_builder)

string(FIND "${spell_packets}" "bool MopSpellPackets::BuildSpellGo" spell_go_builder_begin)
string(FIND "${spell_packets}" "void Spell::SendCastResult" spell_go_builder_end)
if(spell_go_builder_begin EQUAL -1 OR spell_go_builder_end EQUAL -1
        OR spell_go_builder_end LESS_EQUAL spell_go_builder_begin)
    message(FATAL_ERROR "could not isolate BuildSpellGo")
endif()
math(EXPR spell_go_builder_length "${spell_go_builder_end} - ${spell_go_builder_begin}")
string(SUBSTRING "${spell_packets}" ${spell_go_builder_begin} ${spell_go_builder_length} spell_go_builder)

string(FIND "${spell_packets}" "void Spell::SendSpellGo()" spell_go_sender_begin)
string(FIND "${spell_packets}" "void Spell::WriteAmmoToPacket" spell_go_sender_end)
if(spell_go_sender_begin EQUAL -1 OR spell_go_sender_end EQUAL -1
        OR spell_go_sender_end LESS_EQUAL spell_go_sender_begin)
    message(FATAL_ERROR "could not isolate SendSpellGo")
endif()
math(EXPR spell_go_sender_length "${spell_go_sender_end} - ${spell_go_sender_begin}")
string(SUBSTRING "${spell_packets}" ${spell_go_sender_begin} ${spell_go_sender_length} spell_go_sender_body)

function(require_once source token context)
    set(remaining "${source}")
    set(count 0)
    while(TRUE)
        string(FIND "${remaining}" "${token}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR count "${count} + 1")
        string(LENGTH "${token}" token_length)
        math(EXPR next_position "${position} + ${token_length}")
        string(SUBSTRING "${remaining}" ${next_position} -1 remaining)
    endwhile()
    if(NOT count EQUAL 1)
        message(FATAL_ERROR "${context}: expected one active occurrence, found ${count}")
    endif()
endfunction()

function(forbid source token context)
    string(FIND "${source}" "${token}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "${context}: forbidden token remains: ${token}")
    endif()
endfunction()

require_once("${opcode_header}"
    "CMSG_CAST_SPELL                              = 0x0206"
    "CMSG_CAST_SPELL opcode value")
require_once("${opcode_registry}"
    "DefC(CMSG_CAST_SPELL, \"CMSG_CAST_SPELL\""
    "CMSG_CAST_SPELL registration")
require_once("${cast_handler}"
    "MopSpellPackets::ReadCastSpellRequest(recvPacket, request)"
    "18414 cast reader wiring")
require_once("${cast_movement_bits}"
    "movement.forceCount = in.ReadBits(22);"
    "22-bit embedded movement count")
require_once("${cast_movement_bytes}"
    "movement.forceCount > (in.size() - in.rpos()) / sizeof(uint32)"
    "embedded movement allocation bound")
require_once("${cast_request}"
    "if (hasMissileSpeed)\n            in >> missileSpeed;"
    "binary-proven missile-speed guard")
require_once("${cast_request}"
    "if (hasElevation)\n            in >> elevation;"
    "binary-proven elevation guard")
require_once("${cast_request}"
    "if (in.rpos() != in.size())"
    "full request consumption gate")
require_once("${spell_source}"
    "InitializeForCastRequest("
    "owning target initializer")
require_once("${spell_source}"
    "request.targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_UNK2)"
    "unit target mapping")
require_once("${spell_source}"
    "request.targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_GAMEOBJECT_ITEM)"
    "game-object target mapping")
require_once("${spell_source}"
    "request.targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE)"
    "corpse target mapping")
require_once("${spell_source}"
    "request.targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM)"
    "item target mapping")
require_once("${cast_request}"
    "targetStringLength > in.size() - in.rpos()"
    "target-string allocation bound")
require_once("${opcode_header}"
    "SMSG_CAST_FAILED                             = 0x143A"
    "SMSG_CAST_FAILED opcode value")
require_once("${opcode_header}"
    "SMSG_PET_CAST_FAILED                         = 0x149B"
    "SMSG_PET_CAST_FAILED opcode value")
require_once("${opcode_registry}"
    "DefS(SMSG_CAST_FAILED, \"SMSG_CAST_FAILED\");"
    "SMSG_CAST_FAILED registration")
require_once("${opcode_registry}"
    "DefS(SMSG_PET_CAST_FAILED, \"SMSG_PET_CAST_FAILED\");"
    "SMSG_PET_CAST_FAILED registration")
require_once("${world_session}"
    "case SMSG_CAST_FAILED:"
    "SMSG_CAST_FAILED suppression gate")
require_once("${world_session}"
    "case SMSG_PET_CAST_FAILED:"
    "SMSG_PET_CAST_FAILED suppression gate")
require_once("${spell_packets}"
    "MopSpellPackets::BuildCastFailed(data, spellInfo->ID, reportedResult,"
    "18414 cast-failure sender wiring")
require_once("${spell_packets}"
    "if (result <= 118)"
    "18414 cast-result enum translation")
require_once("${spell_packets}"
    "out.WriteBit(!arguments.hasArg18);\n        out.WriteBit(!arguments.hasArg10);"
    "pet cast-failure presence-bit order")
require_once("${debug_commands}"
    "MopSpellPackets::BuildCastFailed(data, 133, SpellCastResult(failnum), 0, false, arguments);"
    "debug cast-failure sender wiring")
forbid("${debug_commands}"
    "WorldPacket data(SMSG_CAST_FAILED"
    "legacy debug cast-failure body")
require_once("${opcode_registry}"
    "DefS(SMSG_SPELL_START, \"SMSG_SPELL_START\");"
    "SMSG_SPELL_START registration")
require_once("${world_session}"
    "case SMSG_SPELL_START:"
    "SMSG_SPELL_START suppression gate")
require_once("${spell_packets}"
    "MopSpellPackets::BuildSpellStart(data, spell)"
    "18414 spell-start sender wiring")
require_once("${spell_start_builder}"
    "out.WriteBits(spell.targetMask, 20);"
    "spell-start 20-bit target mask")
require_once("${spell_start_builder}"
    "out.WriteGuidMask<2, 6>(spell.casterUnitGuid);"
    "spell-start caster-unit mask order")
require_once("${spell_start_builder}"
    "spell.targetString.size() > 0x7F"
    "spell-start target-string bound")
forbid("${spell_start_sender_body}"
    "data << m_targets;"
    "legacy spell-start target serializer")
require_once("${opcode_registry}"
    "DefS(SMSG_SPELL_GO, \"SMSG_SPELL_GO\");"
    "SMSG_SPELL_GO registration")
require_once("${world_session}"
    "case SMSG_SPELL_GO:"
    "SMSG_SPELL_GO suppression gate")
require_once("${spell_go_sender_body}"
    "MopSpellPackets::BuildSpellGo(data, spell)"
    "18414 spell-go sender wiring")
require_once("${spell_go_sender_body}"
    "if (m_targets.GetSpeed() > 0.0f)"
    "spell-go trajectory presence")
require_once("${spell_go_builder}"
    "out.WriteBits(miss.reason, 4);"
    "spell-go 4-bit miss reason")
require_once("${spell_go_builder}"
    "out.WriteGuidMask<2, 7, 1, 6, 4, 5, 0, 3>(hit);"
    "spell-go hit GUID mask order")
require_once("${spell_go_builder}"
    "if ((spell.hasTargetString && spell.targetString.size() > 0x7F)"
    "spell-go target-string bound")
require_once("${spell_go_builder}"
    "out.WriteBits(spell.targetMask, 20);"
    "spell-go 20-bit target mask")
forbid("${spell_go_sender_body}"
    "data << m_targets;"
    "legacy spell-go target serializer")
forbid("${spell_go_sender_body}"
    "WriteSpellGoTargets"
    "legacy spell-go hit/miss serializer")
require_once("${opcode_header}"
    "CMSG_REQUEST_CATEGORY_COOLDOWNS               = 0x1203"
    "CMSG_REQUEST_CATEGORY_COOLDOWNS opcode value")
require_once("${opcode_header}"
    "SMSG_CATEGORY_COOLDOWN                        = 0x01DB"
    "SMSG_CATEGORY_COOLDOWN opcode value")
require_once("${opcode_registry}"
    "DefC(CMSG_REQUEST_CATEGORY_COOLDOWNS, \"CMSG_REQUEST_CATEGORY_COOLDOWNS\""
    "category-cooldown request registration")
require_once("${opcode_registry}"
    "DefS(SMSG_CATEGORY_COOLDOWN, \"SMSG_CATEGORY_COOLDOWN\");"
    "category-cooldown response registration")
require_once("${world_session}"
    "case SMSG_CATEGORY_COOLDOWN:"
    "category-cooldown sender admission")
require_once("${spell_handler}"
    "void WorldSession::HandleRequestCategoryCooldowns(WorldPacket& recvPacket)"
    "category-cooldown gameplay handler")
require_once("${spell_handler}"
    "MopSpellPackets::ReadCategoryCooldownRequest(recvPacket)"
    "empty category-cooldown request validation")
require_once("${spell_handler}"
    "GetAurasByType(SPELL_AURA_MOD_SPELL_CATEGORY_COOLDOWN)"
    "category-cooldown aura aggregation")
require_once("${spell_handler}"
    "MopSpellPackets::BuildCategoryCooldown(data, records);"
    "18414 category-cooldown response sender")
require_once("${spell_header}"
    "out.WriteBits(uint32(records.size()), 21);"
    "category-cooldown 21-bit record count")
require_once("${spell_header}"
    "out << record.cooldownModifier << record.category;"
    "category-cooldown record wire order")
require_once("${spell_header}"
    "inline void BuildSpellCooldown(WorldPacket& out, ObjectGuid ownerGuid,"
    "18414 spell-cooldown builder")
require_once("${spell_header}"
    "out.WriteGuidMask<0, 6>(ownerGuid);"
    "spell-cooldown leading GUID mask")
require_once("${spell_header}"
    "out.WriteBit(flags == 0);"
    "spell-cooldown inverse flags-presence bit")
require_once("${spell_header}"
    "out.WriteGuidMask<7, 3, 1, 5>(ownerGuid);"
    "spell-cooldown middle GUID mask")
require_once("${spell_header}"
    "out.WriteBits(uint32(cooldowns.size()), 21);"
    "spell-cooldown 21-bit record count")
require_once("${spell_header}"
    "out.WriteGuidMask<2, 4>(ownerGuid);"
    "spell-cooldown trailing GUID mask")
require_once("${spell_header}"
    "out.WriteGuidBytes<5, 3, 7>(ownerGuid);"
    "spell-cooldown leading GUID bytes")
require_once("${spell_header}"
    "out.WriteGuidBytes<4, 1, 0, 2, 6>(ownerGuid);"
    "spell-cooldown trailing GUID bytes")
require_once("${player_source}"
    "MopSpellPackets::BuildSpellCooldown(data, GetObjectGuid(), 0, cooldowns);"
    "player school-lockout cooldown sender")
require_once("${item_storage_source}"
    "MopSpellPackets::BuildSpellCooldown(data, GetObjectGuid(), 1, cooldowns);"
    "weapon-switch cooldown sender")
require_once("${pet_spells_source}"
    "MopSpellPackets::BuildSpellCooldown(data, GetObjectGuid(), 0, cooldowns);"
    "pet cooldown sender")
require_once("${opcode_registry}"
    "DefS(SMSG_SPELL_COOLDOWN, \"SMSG_SPELL_COOLDOWN\");"
    "spell-cooldown registration")
require_once("${world_session}"
    "case SMSG_SPELL_COOLDOWN:"
    "spell-cooldown sender admission")
require_once("${opcode_reference}"
    "SMSG_SPELL_COOLDOWN                            0x0452  ACTIVE"
    "active direct-client reference")
foreach(source IN ITEMS "${player_source}" "${item_storage_source}" "${pet_spells_source}")
    forbid("${source}" "WorldPacket data(SMSG_SPELL_COOLDOWN"
        "legacy spell-cooldown body")
endforeach()
require_once("${spell_header}"
    "inline void BuildClearCooldowns(WorldPacket& out, ObjectGuid ownerGuid,"
    "18414 clear-cooldowns builder")
require_once("${spell_header}"
    "out.WriteGuidMask<5, 6, 7, 3, 2>(ownerGuid);"
    "clear-cooldowns leading GUID mask")
require_once("${spell_header}"
    "out.WriteBits(uint32(spellIds.size()), 22);"
    "clear-cooldowns 22-bit count")
require_once("${spell_header}"
    "out.WriteGuidMask<1, 0, 4>(ownerGuid);"
    "clear-cooldowns trailing GUID mask")
require_once("${spell_header}"
    "out.WriteGuidBytes<0, 1, 7, 4, 3, 5, 6>(ownerGuid);"
    "clear-cooldowns leading GUID bytes")
require_once("${spell_header}"
    "out.WriteGuidBytes<2>(ownerGuid);"
    "clear-cooldowns trailing GUID byte")
require_once("${player_source}"
    "MopSpellPackets::BuildClearCooldowns(data, target->GetObjectGuid(), {spell_id});"
    "single cooldown-clear sender")
require_once("${spell_cooldown_mgr_source}"
    "MopSpellPackets::BuildClearCooldowns(data, m_owner->GetObjectGuid(), spellIds);"
    "all-cooldowns-clear sender")
require_once("${opcode_registry}"
    "DefS(SMSG_CLEAR_COOLDOWNS, \"SMSG_CLEAR_COOLDOWNS\");"
    "clear-cooldowns registration")
require_once("${world_session}"
    "case SMSG_CLEAR_COOLDOWNS:"
    "clear-cooldowns sender admission")
require_once("${opcode_reference}"
    "SMSG_CLEAR_COOLDOWNS                           0x1458  ACTIVE"
    "active clear-cooldowns reference")
foreach(source IN ITEMS "${player_source}" "${spell_cooldown_mgr_source}")
    forbid("${source}" "WorldPacket data(SMSG_CLEAR_COOLDOWNS"
        "legacy clear-cooldowns body")
endforeach()
require_once("${spell_header}"
    "inline void BuildCooldownEvent(WorldPacket& out, ObjectGuid ownerGuid,"
    "18414 cooldown-event builder")
require_once("${spell_header}"
    "out.WriteGuidMask<4, 7, 1, 5, 6, 0, 2, 3>(ownerGuid);"
    "cooldown-event GUID mask order")
require_once("${spell_header}"
    "out.WriteGuidBytes<5, 7>(ownerGuid);"
    "cooldown-event leading GUID bytes")
require_once("${spell_header}"
    "out.WriteGuidBytes<3, 1, 2, 4, 6, 0>(ownerGuid);"
    "cooldown-event trailing GUID bytes")
require_once("${spell_cooldown_mgr_source}"
    "MopSpellPackets::BuildCooldownEvent(data, m_owner->GetObjectGuid(), spellInfo->ID);"
    "cooldown-event sender")
require_once("${opcode_registry}"
    "DefS(SMSG_COOLDOWN_EVENT, \"SMSG_COOLDOWN_EVENT\");"
    "cooldown-event registration")
require_once("${world_session}"
    "case SMSG_COOLDOWN_EVENT:"
    "cooldown-event send admission")
require_once("${opcode_reference}"
    "SMSG_COOLDOWN_EVENT                            0x1163  ACTIVE"
    "active cooldown-event reference")
forbid("${spell_cooldown_mgr_source}" "WorldPacket data(SMSG_COOLDOWN_EVENT"
    "legacy cooldown-event body")
require_once("${spell_header}"
    "inline void BuildItemCooldown(WorldPacket& out, ObjectGuid itemGuid,"
    "18414 item-cooldown builder")
require_once("${spell_header}"
    "out << itemGuid << spellId;"
    "item-cooldown wire field order")
require_once("${player_source}"
    "MopSpellPackets::BuildItemCooldown(data, pItem->GetObjectGuid(), spellData.SpellId);"
    "item-cooldown sender")
require_once("${opcode_registry}"
    "DefS(SMSG_ITEM_COOLDOWN, \"SMSG_ITEM_COOLDOWN\");"
    "item-cooldown registration")
require_once("${world_session}"
    "case SMSG_ITEM_COOLDOWN:"
    "item-cooldown send admission")
require_once("${opcode_reference}"
    "SMSG_ITEM_COOLDOWN                             0x1904  ACTIVE"
    "active direct-client item-cooldown reference")
forbid("${player_source}" "WorldPacket data(SMSG_ITEM_COOLDOWN"
    "legacy item-cooldown body")
require_once("${spell_header}"
    "out.WriteGuidMask<6, 2, 0, 4, 7, 1, 3, 5>(targetGuid);"
    "clear-target GUID mask order")
require_once("${spell_header}"
    "out.WriteGuidBytes<4, 0, 3, 5, 2, 7, 6, 1>(targetGuid);"
    "clear-target GUID byte order")
require_once("${spell_effect_tail}"
    "MopSpellPackets::BuildClearTarget(data, unitTarget->GetObjectGuid());"
    "clear-target sender")
require_once("${opcode_registry}"
    "DefS(SMSG_CLEAR_TARGET, \"SMSG_CLEAR_TARGET\");"
    "clear-target registration")
require_once("${world_session}"
    "case SMSG_CLEAR_TARGET:"
    "clear-target send admission")
require_once("${opcode_reference}"
    "SMSG_CLEAR_TARGET                              0x1061  ACTIVE"
    "active direct-client clear-target reference")
forbid("${spell_effect_tail}" "WorldPacket data(SMSG_CLEAR_TARGET"
    "legacy clear-target body")
require_once("${spell_header}"
    "out.WriteGuidMask<7, 5, 4, 1>(casterGuid);"
    "channel-start leading caster mask")
require_once("${spell_header}"
    "out.WriteGuidBytes<6, 7, 3, 1, 0>(casterGuid);"
    "channel-start leading caster bytes")
require_once("${spell_header}"
    "out.WriteGuidMask<0, 3, 4, 1, 5, 2, 6, 7>(casterGuid);"
    "channel-update caster mask")
require_once("${spell_header}"
    "out.WriteGuidBytes<4, 7, 1, 2, 6, 5>(casterGuid);"
    "channel-update leading caster bytes")
foreach(builder IN ITEMS BuildChannelStart BuildChannelUpdate)
    require_once("${spell_packets}"
        "MopSpellPackets::${builder}("
        "${builder} owning sender")
endforeach()
foreach(opcode IN ITEMS SMSG_CHANNEL_START SMSG_CHANNEL_UPDATE)
    require_once("${opcode_registry}"
        "DefS(${opcode}, \"${opcode}\");"
        "${opcode} registration")
    require_once("${world_session}"
        "case ${opcode}:"
        "${opcode} send admission")
    forbid("${spell_packets}" "WorldPacket data(${opcode}"
        "legacy ${opcode} body")
endforeach()
require_once("${opcode_reference}"
    "SMSG_CHANNEL_START                             0x10F9  ACTIVE"
    "active channel-start reference")
require_once("${opcode_reference}"
    "SMSG_CHANNEL_UPDATE                            0x11D9  ACTIVE"
    "active channel-update reference")
require_once("${spell_header}"
    "inline void BuildLearnedSpell(WorldPacket& out, uint32 spellId,"
    "18414 learned-spell builder")
require_once("${spell_header}"
    "inline void BuildRemovedSpell(WorldPacket& out, uint32 spellId)"
    "18414 removed-spell builder")
require_once("${spell_header}"
    "inline void BuildSupersededSpell(WorldPacket& out, uint32 oldSpellId,"
    "18414 superseded-spell builder")
require_once("${spell_header}"
    "out.WriteBit(suppressMessaging);"
    "learned-spell suppress-messaging bit")
require_once("${spell_header}"
    "out << newSpellId << oldSpellId;"
    "superseded-spell new/old wire order")
require_once("${player_spell_source}"
    "MopSpellPackets::BuildLearnedSpell(data, spell_id, false);"
    "player learned-spell sender")
foreach(opcode IN ITEMS SMSG_LEARNED_SPELL SMSG_REMOVED_SPELL SMSG_SUPERCEDED_SPELL)
    require_once("${opcode_registry}"
        "DefS(${opcode}, \"${opcode}\");"
        "${opcode} registration")
    require_once("${world_session}"
        "case ${opcode}:"
        "${opcode} send admission")
endforeach()

require_once("${spell_header}"
    "inline void BuildPetLearnedSpell(WorldPacket& out, uint32 spellId)"
    "18414 pet learned-spell builder")
require_once("${spell_header}"
    "inline void BuildPetRemovedSpell(WorldPacket& out, uint32 spellId)"
    "18414 pet removed-spell builder")
require_once("${pet_spells_source}"
    "MopSpellPackets::BuildPetLearnedSpell(data, spell_id);"
    "pet learned-spell sender")
require_once("${pet_spells_source}"
    "MopSpellPackets::BuildPetRemovedSpell(data, spell_id);"
    "pet removed-spell sender")
foreach(opcode IN ITEMS SMSG_PET_LEARNED_SPELL SMSG_PET_REMOVED_SPELL)
    require_once("${opcode_registry}"
        "DefS(${opcode}, \"${opcode}\");"
        "${opcode} registration")
    require_once("${world_session}"
        "case ${opcode}:"
        "${opcode} send admission")
endforeach()
require_once("${opcode_reference}"
    "SMSG_PET_LEARNED_SPELL                         0x0282  ACTIVE"
    "active pet learned-spell reference")
require_once("${opcode_reference}"
    "SMSG_PET_REMOVED_SPELL                         0x1CAE  ACTIVE"
    "active pet removed-spell reference")
foreach(opcode IN ITEMS SMSG_PET_LEARNED_SPELL SMSG_PET_REMOVED_SPELL)
    forbid("${pet_spells_source}" "WorldPacket data(${opcode}"
        "legacy ${opcode} body")
endforeach()
require_once("${opcode_reference}"
    "SMSG_LEARNED_SPELL                             0x129A  ACTIVE"
    "active learned-spell reference")
require_once("${opcode_reference}"
    "SMSG_REMOVED_SPELL                             0x14C3  ACTIVE"
    "active removed-spell reference")
require_once("${opcode_reference}"
    "SMSG_SUPERCEDED_SPELL                          0x1943  ACTIVE"
    "active superseded-spell reference")
foreach(opcode IN ITEMS SMSG_LEARNED_SPELL SMSG_REMOVED_SPELL SMSG_SUPERCEDED_SPELL)
    forbid("${player_spell_source}" "WorldPacket data(${opcode}"
        "legacy ${opcode} body")
endforeach()

string(FIND "${cast_handler}" "MopSpellPackets::ReadCastSpellRequest(recvPacket, request)" reader_position)
string(FIND "${cast_handler}" "sSpellStore.LookupEntry(spellId)" lookup_position)
string(FIND "${cast_handler}" "targets.InitializeForCastRequest(mover, request)" initializer_position)
string(FIND "${cast_handler}" "sSpellMgr.SelectAuraRankForLevel" rank_position)
if(reader_position EQUAL -1 OR lookup_position EQUAL -1 OR initializer_position EQUAL -1 OR rank_position EQUAL -1
        OR NOT reader_position LESS lookup_position
        OR NOT lookup_position LESS initializer_position
        OR NOT initializer_position LESS rank_position)
    message(FATAL_ERROR "cast parser/validation/target-resolution order drifted")
endif()

string(FIND "${cast_movement_bits}" "movement.hasMovementFlags = !in.ReadBit();" movement_flags_presence)
string(FIND "${cast_movement_bits}" "movement.hasTimestamp = !in.ReadBit();" movement_timestamp_presence)
string(FIND "${cast_movement_bits}" "movement.hasUnknownUInt32 = !in.ReadBit();" movement_uint32_presence)
string(FIND "${cast_movement_bits}" "in.ReadBits(30);" movement_flags_payload)
if(movement_flags_presence EQUAL -1 OR movement_timestamp_presence EQUAL -1
        OR movement_uint32_presence EQUAL -1 OR movement_flags_payload EQUAL -1
        OR NOT movement_flags_presence LESS movement_timestamp_presence
        OR NOT movement_timestamp_presence LESS movement_uint32_presence
        OR NOT movement_uint32_presence LESS movement_flags_payload)
    message(FATAL_ERROR "embedded movement flags/presence-bit order drifted")
endif()

forbid("${cast_handler}"
    "recvPacket >> cast_count;"
    "legacy byte-aligned cast-count prefix")
forbid("${cast_handler}"
    "recvPacket >> spellId >> glyphIndex;"
    "legacy byte-aligned spell/glyph prefix")
forbid("${cast_handler}"
    "targets.ReadAdditionalData(recvPacket, cast_flags);"
    "legacy additional-data parser")
