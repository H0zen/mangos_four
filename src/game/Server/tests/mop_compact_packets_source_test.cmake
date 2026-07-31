file(READ "${SOURCE_ROOT}/src/game/Object/PlayerCombat.cpp" player_combat)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerDuel.cpp" player_duel)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerMirror.cpp" player_mirror)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerCombo.cpp" player_combo)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerDeath.cpp" player_death)
file(READ "${SOURCE_ROOT}/src/game/Object/RuneMgr.cpp" rune_source)
file(READ "${SOURCE_ROOT}/src/game/Object/RuneMgr.h" rune_header)
file(READ "${SOURCE_ROOT}/src/game/Object/Player.h" player_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellEffectObjectCombat.cpp" spell_effect_object_combat)
file(READ "${SOURCE_ROOT}/src/game/Object/UnitSpeed.cpp" unit_speed)
file(READ "${SOURCE_ROOT}/src/game/ChatCommands/PlayerStatsMods.cpp" player_stats_mods)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GroupHandler.cpp" group_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/InstanceData.cpp" instance_data)
file(READ "${SOURCE_ROOT}/src/game/Object/PlayerInstance.cpp" player_instance)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CharacterHandler.cpp" character_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellHandler.cpp" spell_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MovementHandler.cpp" movement_handler)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MailHandler.cpp" mail_handler)
file(READ "${SOURCE_ROOT}/src/game/Object/UnitCombat.cpp" unit_combat)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/Object/UnitThreat.cpp" unit_threat)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/CombatHandler.cpp" combat_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/ItemHandler.cpp" item_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)

string(CONCAT original_totem_sources "${unit_header}" "${opcode_header}"
    "${opcode_registry}" "${opcode_reference}" "${spell_handler}")

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
elseif(MUTATION STREQUAL "auto_repeat_mask")
    string(REPLACE
        "out.WriteGuidMask<1, 3, 0, 4, 6, 7, 5, 2>(guid);"
        "out.WriteGuidMask<3, 1, 0, 4, 6, 7, 5, 2>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "auto_repeat_bytes")
    string(REPLACE
        "out.WriteGuidBytes<7, 6, 2, 5, 0, 4, 1, 3>(guid);"
        "out.WriteGuidBytes<6, 7, 2, 5, 0, 4, 1, 3>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "auto_repeat_sender")
    string(REPLACE
        "MopCompactPackets::BuildCancelAutoRepeat("
        "/* removed cancel-auto-repeat builder */ ("
        player_combat "${player_combat}")
elseif(MUTATION STREQUAL "auto_repeat_registration")
    string(REPLACE
        "DefS(SMSG_CANCEL_AUTO_REPEAT, \"SMSG_CANCEL_AUTO_REPEAT\");"
        "/* removed cancel-auto-repeat registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "auto_repeat_allowlist")
    string(REPLACE
        "case SMSG_CANCEL_AUTO_REPEAT:"
        "case 0xFFFF: /* removed cancel-auto-repeat allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "auto_repeat_opcode")
    string(REPLACE
        "SMSG_CANCEL_AUTO_REPEAT                      = 0x1E0F"
        "SMSG_CANCEL_AUTO_REPEAT                      = 0x1E0E"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "auto_repeat_reference")
    string(REPLACE
        "SMSG_CANCEL_AUTO_REPEAT                        0x1E0F  ACTIVE"
        "SMSG_CANCEL_AUTO_REPEAT                        0x1E0F  DORMANT"
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
elseif(MUTATION STREQUAL "party_kill_mask")
    string(REPLACE
        "out.WriteGuidMask<7, 2>(victim);"
        "out.WriteGuidMask<2, 7>(victim);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "party_kill_bytes")
    string(REPLACE
        "out.WriteGuidBytes<0, 5>(victim);"
        "out.WriteGuidBytes<5, 0>(victim);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "party_kill_sender")
    string(REPLACE
        "MopCompactPackets::BuildPartyKillLog(data,"
        "/* removed party-kill sender */ (data,"
        unit "${unit}")
elseif(MUTATION STREQUAL "party_kill_registration")
    string(REPLACE
        "DefS(SMSG_PARTYKILLLOG, \"SMSG_PARTYKILLLOG\");"
        "/* removed party-kill registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "party_kill_allowlist")
    string(REPLACE
        "case SMSG_PARTYKILLLOG:"
        "case 0xFFFF: /* removed party-kill allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "party_kill_reference")
    string(REPLACE
        "SMSG_PARTYKILLLOG                              0x048A  ACTIVE"
        "SMSG_PARTYKILLLOG                              0x048A  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "duel_complete_bit")
    string(REPLACE
        "out.WriteBit(completed);"
        "out << uint8(completed);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "duel_countdown_width")
    string(REPLACE
        "out << milliseconds;"
        "out << uint16(milliseconds);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "duel_sender")
    string(REPLACE
        "MopDuelPackets::BuildComplete(data, type != DUEL_INTERRUPTED);"
        "data.Initialize(SMSG_DUEL_COMPLETE, 1);"
        player_duel "${player_duel}")
elseif(MUTATION STREQUAL "duel_registration")
    string(REPLACE
        "DefS(SMSG_DUEL_COMPLETE, \"SMSG_DUEL_COMPLETE\");"
        "/* removed duel-complete registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "duel_allowlist")
    string(REPLACE
        "case SMSG_DUEL_COMPLETE:"
        "case 0xFFFF: /* removed duel-complete allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "duel_reference")
    string(REPLACE
        "SMSG_DUEL_COMPLETE                             0x1C0A  ACTIVE"
        "SMSG_DUEL_COMPLETE                             0x1C0A  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "duel_request_mask")
    string(REPLACE
        "out.WriteGuidMask<4, 2, 7>(initiator);"
        "out.WriteGuidMask<2, 4, 7>(initiator);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "duel_request_bytes")
    string(REPLACE
        "out.WriteGuidBytes<5, 3>(arbiter);"
        "out.WriteGuidBytes<3, 5>(arbiter);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "duel_winner_bits")
    string(REPLACE
        "out.WriteBits(uint32(winnerName.size()), 6);"
        "out.WriteBits(uint32(winnerName.size()), 5);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "duel_winner_realm_order")
    string(REPLACE
        "out << loserRealmAddress;"
        "out << winnerRealmAddress;"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "duel_request_sender")
    string(REPLACE
        "MopDuelPackets::BuildRequested("
        "/* removed duel-request sender */ ("
        spell_effect_object_combat "${spell_effect_object_combat}")
elseif(MUTATION STREQUAL "duel_winner_sender")
    string(REPLACE
        "MopDuelPackets::BuildWinner(data, type != DUEL_WON,"
        "/* removed duel-winner sender */ (data, type != DUEL_WON,"
        player_duel "${player_duel}")
elseif(MUTATION STREQUAL "duel_pair_registration")
    string(REPLACE
        "DefS(SMSG_DUEL_WINNER, \"SMSG_DUEL_WINNER\");"
        "/* removed duel-winner registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "duel_pair_allowlist")
    string(REPLACE
        "case SMSG_DUEL_REQUESTED:"
        "case 0xFFFF: /* removed duel-request allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "duel_pair_reference")
    string(REPLACE
        "SMSG_DUEL_WINNER                               0x10E1  ACTIVE"
        "SMSG_DUEL_WINNER                               0x10E1  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "mirror_start_order")
    string(REPLACE
        "out << maxValue << spellId << currentValue << uint32(regeneration) << type;"
        "out << type << maxValue << currentValue << uint32(regeneration) << spellId;"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "mirror_pause_width")
    string(REPLACE
        "out.WriteBit(paused);"
        "out << uint8(paused);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "mirror_sender")
    string(REPLACE
        "MopMirrorTimerPackets::BuildStart("
        "/* removed mirror-timer builder */ ("
        player_mirror "${player_mirror}")
elseif(MUTATION STREQUAL "mirror_registration")
    string(REPLACE
        "DefS(SMSG_START_MIRROR_TIMER, \"SMSG_START_MIRROR_TIMER\");"
        "/* removed mirror-timer registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "mirror_allowlist")
    string(REPLACE
        "case SMSG_START_MIRROR_TIMER:"
        "case 0xFFFF: /* removed mirror-timer allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "mirror_reference")
    string(REPLACE
        "SMSG_START_MIRROR_TIMER                        0x0E12  ACTIVE"
        "SMSG_START_MIRROR_TIMER                        0x0E12  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "rune_count_width")
    string(REPLACE
        "out.WriteBits(uint32(runes.size()), 23);"
        "out.WriteBits(uint32(runes.size()), 22);"
        rune_header "${rune_header}")
elseif(MUTATION STREQUAL "rune_record_order")
    string(REPLACE
        "out << rune.cooldownFraction << uint8(rune.type);"
        "out << uint8(rune.type) << rune.cooldownFraction;"
        rune_header "${rune_header}")
elseif(MUTATION STREQUAL "rune_convert_order")
    string(REPLACE
        "out << uint8(newType) << index;"
        "out << index << uint8(newType);"
        rune_header "${rune_header}")
elseif(MUTATION STREQUAL "rune_sender")
    string(REPLACE
        "MopRunePackets::BuildResync(data, runes);"
        "/* removed rune-resync builder */"
        rune_source "${rune_source}")
elseif(MUTATION STREQUAL "rune_registration")
    string(REPLACE
        "DefS(SMSG_RESYNC_RUNES, \"SMSG_RESYNC_RUNES\");"
        "/* removed rune registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "rune_allowlist")
    string(REPLACE
        "case SMSG_RESYNC_RUNES:"
        "case 0xFFFF: /* removed rune allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "rune_reference")
    string(REPLACE
        "SMSG_RESYNC_RUNES                              0x15E3  ACTIVE"
        "SMSG_RESYNC_RUNES                              0x15E3  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "threat_count_width")
    string(REPLACE
        "out.WriteBits(uint32(entries.size()), 21);"
        "out.WriteBits(uint32(entries.size()), 20);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "threat_update_mask")
    string(REPLACE
        "out.WriteGuidMask<5, 6, 1, 3, 7, 0, 4>(owner);"
        "out.WriteGuidMask<6, 5, 1, 3, 7, 0, 4>(owner);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "threat_highest_mask")
    string(REPLACE
        "out.WriteGuidMask<3, 0>(selected);"
        "out.WriteGuidMask<0, 3>(selected);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "threat_clear_bytes")
    string(REPLACE
        "out.WriteGuidBytes<7, 0, 4, 3, 2, 1, 6, 5>(owner);"
        "out.WriteGuidBytes<0, 7, 4, 3, 2, 1, 6, 5>(owner);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "threat_remove_mask")
    string(REPLACE
        "out.WriteGuidMask<0, 1, 5>(owner);"
        "out.WriteGuidMask<1, 0, 5>(owner);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "threat_sender")
    string(REPLACE
        "MopThreatPackets::BuildUpdate(data, GetObjectGuid(), entries);"
        "/* removed threat-update builder */"
        unit_threat "${unit_threat}")
elseif(MUTATION STREQUAL "threat_registration")
    string(REPLACE
        "DefS(SMSG_THREAT_UPDATE, \"SMSG_THREAT_UPDATE\");"
        "/* removed threat-update registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "threat_allowlist")
    string(REPLACE
        "case SMSG_THREAT_UPDATE:"
        "case 0xFFFF: /* removed threat-update allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "threat_reference")
    string(REPLACE
        "SMSG_THREAT_UPDATE                             0x0632  ACTIVE"
        "SMSG_THREAT_UPDATE                             0x0632  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "dismount_mask")
    string(REPLACE
        "out.WriteGuidMask<6, 3, 0, 7, 1, 2, 5, 4>(guid);"
        "out.WriteGuidMask<3, 6, 0, 7, 1, 2, 5, 4>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "dismount_bytes")
    string(REPLACE
        "out.WriteGuidBytes<3, 6, 7, 5, 1, 4, 2, 0>(guid);"
        "out.WriteGuidBytes<6, 3, 7, 5, 1, 4, 2, 0>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "dismount_sender")
    string(REPLACE
        "MopCompactPackets::BuildDismount(data, GetObjectGuid());"
        "/* removed dismount builder */"
        unit "${unit}")
elseif(MUTATION STREQUAL "dismount_registration")
    string(REPLACE
        "DefS(SMSG_DISMOUNT, \"SMSG_DISMOUNT\");"
        "/* removed dismount registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "dismount_allowlist")
    string(REPLACE
        "case SMSG_DISMOUNT:"
        "case 0xFFFF: /* removed dismount allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "dismount_reference")
    string(REPLACE
        "SMSG_DISMOUNT                                  0x0E3A  ACTIVE"
        "SMSG_DISMOUNT                                  0x0E3A  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "combo_mask")
    string(REPLACE
        "out.WriteGuidMask<0, 5, 6, 3, 7, 4, 1, 2>(target);"
        "out.WriteGuidMask<5, 0, 6, 3, 7, 4, 1, 2>(target);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "combo_bytes")
    string(REPLACE
        "out.WriteGuidBytes<5, 6, 4, 7, 3, 0>(target);"
        "out.WriteGuidBytes<6, 5, 4, 7, 3, 0>(target);"
        player_header "${player_header}")
elseif(MUTATION STREQUAL "combo_sender")
    string(REPLACE
        "MopComboPointPackets::BuildUpdate(data, combotarget->GetObjectGuid(), uint8(m_comboPoints));"
        "/* removed combo-point builder */"
        player_combo "${player_combo}")
elseif(MUTATION STREQUAL "combo_registration")
    string(REPLACE
        "DefS(SMSG_UPDATE_COMBO_POINTS, \"SMSG_UPDATE_COMBO_POINTS\");"
        "/* removed combo-point registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "combo_allowlist")
    string(REPLACE
        "case SMSG_UPDATE_COMBO_POINTS:"
        "case 0xFFFF: /* removed combo-point allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "combo_reference")
    string(REPLACE
        "SMSG_UPDATE_COMBO_POINTS                       0x082F  ACTIVE"
        "SMSG_UPDATE_COMBO_POINTS                       0x082F  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "pre_resurrect_mask")
    string(REPLACE
        "out.WriteGuidMask<1, 7, 5, 2, 6, 0, 3, 4>(guid);"
        "out.WriteGuidMask<7, 1, 5, 2, 6, 0, 3, 4>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "pre_resurrect_bytes")
    string(REPLACE
        "out.WriteGuidBytes<5, 1, 7, 0, 6, 4, 2, 3>(guid);"
        "out.WriteGuidBytes<1, 5, 7, 0, 6, 4, 2, 3>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "pre_resurrect_sender")
    string(REPLACE
        "MopCompactPackets::BuildPreResurrect(data, GetObjectGuid());"
        "/* removed pre-resurrect builder */"
        player_death "${player_death}")
elseif(MUTATION STREQUAL "pre_resurrect_registration")
    string(REPLACE
        "DefS(SMSG_PRE_RESURRECT, \"SMSG_PRE_RESURRECT\");"
        "/* removed pre-resurrect registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "pre_resurrect_allowlist")
    string(REPLACE
        "case SMSG_PRE_RESURRECT:"
        "case 0xFFFF: /* removed pre-resurrect allowlist */"
        world_session "${world_session}")
elseif(MUTATION STREQUAL "pre_resurrect_reference")
    string(REPLACE
        "SMSG_PRE_RESURRECT                             0x19C0  ACTIVE"
        "SMSG_PRE_RESURRECT                             0x19C0  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "totem_mask_order")
    string(REPLACE
        "uint8 const maskOrder[] = { 4, 2, 1, 3, 0, 6, 7, 5 };"
        "uint8 const maskOrder[] = { 2, 4, 1, 3, 0, 6, 7, 5 };"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "totem_byte_order")
    string(REPLACE
        "uint8 const byteOrder[] = { 6, 2, 4, 1, 5, 0, 3, 7 };"
        "uint8 const byteOrder[] = { 2, 6, 4, 1, 5, 0, 3, 7 };"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "totem_opcode_value")
    string(REPLACE
        "CMSG_TOTEM_DESTROYED                         = 0x1263"
        "CMSG_TOTEM_DESTROYED                         = 0x1262"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "totem_registration")
    string(REPLACE
        "DefC(CMSG_TOTEM_DESTROYED, \"CMSG_TOTEM_DESTROYED\", STATUS_LOGGEDIN, PROCESS_THREADUNSAFE, &WorldSession::HandleTotemDestroyed);"
        "/* removed CMSG_TOTEM_DESTROYED registration */"
        opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "totem_reference")
    string(REPLACE
        "CMSG_TOTEM_DESTROYED                           0x1263  ACTIVE"
        "CMSG_TOTEM_DESTROYED                           0x1263  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "totem_handler_route")
    string(REPLACE
        "MopCompactPackets::ReadTotemDestroyed(recvPacket, slotId)"
        "/* removed totem-destroy reader route */ ObjectGuid()"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "totem_drop_self_mover_guard")
    string(REPLACE
        "if (!_player->IsSelfMover())"
        "if (false) /* removed self-mover guard */"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "totem_drop_slot_bound")
    string(REPLACE
        "slotId < MAX_TOTEM_SLOT"
        "slotId <= MAX_TOTEM_SLOT"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "totem_increment_slot")
    string(REPLACE
        "ObjectGuid totemGuid = MopCompactPackets::ReadTotemDestroyed(recvPacket, slotId);"
        "ObjectGuid totemGuid = MopCompactPackets::ReadTotemDestroyed(recvPacket, slotId); ++slotId;"
        spell_handler "${spell_handler}")
elseif(MUTATION STREQUAL "totem_accept_nonempty_mismatch")
    string(REPLACE
        "return requestedGuid.IsEmpty() || requestedGuid == occupiedGuid;"
        "return true;"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "totem_reject_empty_sentinel")
    string(REPLACE
        "return requestedGuid.IsEmpty() || requestedGuid == occupiedGuid;"
        "return requestedGuid == occupiedGuid;"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "totem_drop_exact_tail")
    string(REPLACE
        "slotId < MAX_TOTEM_SLOT && in.rpos() == in.size() &&"
        "slotId < MAX_TOTEM_SLOT &&"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "totem_accept_noncanonical_zero_byte")
    string(REPLACE
        "if (in[index] == 0x01)"
        "if (false)"
        unit_header "${unit_header}")
endif()

if(MUTATION MATCHES "^totem_")
    string(CONCAT mutated_totem_sources "${unit_header}" "${opcode_header}"
        "${opcode_registry}" "${opcode_reference}" "${spell_handler}")
    if(mutated_totem_sources STREQUAL original_totem_sources)
        message(STATUS "MUTATION '${MUTATION}' changed nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
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
if(player_stats_mods MATCHES "(WorldPacket[ \t]+[A-Za-z_][A-Za-z0-9_]*[ \t]*\\([ \t]*|Initialize[ \t]*\\([ \t]*)SMSG_FORCE_[A-Z_]+_CHANGE")
    message(FATAL_ERROR "legacy SMSG_FORCE_*_CHANGE construction remains in the stat-mod commands")
endif()
if(NOT unit_speed MATCHES "MopCompactPackets::BuildMoveSetRunSpeed")
    message(FATAL_ERROR "run-speed sender bypasses the shared 5.4.8 serializer")
endif()

# Request/reply pairing. A request that changes state and then answers the client
# must not be registered while its reply is still dropped by the send gate: the
# transaction commits, the client hears nothing, and the retry is equally silent.
# CMSG_MAIL_TAKE_ITEM is the sharp case -- it removes the attachment and settles
# cash on delivery -- and it reached master once before being pulled back.
#
# Comments are stripped first. Without that, a commented-out DefS or gate case
# would appear to authorise a registration, and a commented-out DefC would hide
# one. Token gaps allow whitespace and newlines so that reformatting the call,
# or splitting it across lines, neither evades a rule nor satisfies one falsely.
function(mop_strip_cxx_comments in_text out_var)
    # Character classes, not backslash escapes. CMake's regex engine drops a
    # single backslash from a quoted argument, so "\*" reaches it as a bare
    # star and fails to compile; "[*]" says the same thing and survives.
    string(REGEX REPLACE "/[*][^*]*[*]+/" "" stripped "${in_text}")
    string(REGEX REPLACE "//[^
]*" "" stripped "${stripped}")
    set(${out_var} "${stripped}" PARENT_SCOPE)
endfunction()

# Any run of C++ whitespace between tokens, so reformatting or splitting a call
# across lines neither evades a rule nor satisfies one.
set(ws "[ \t\r\n]")

mop_strip_cxx_comments("${opcode_registry}" registry_code)
mop_strip_cxx_comments("${world_session}" session_code)
mop_strip_cxx_comments("${spell_handler}" spell_handler_code)

foreach(pairing IN ITEMS
        "CMSG_GET_MAIL_LIST|SMSG_MAIL_LIST_RESULT"
        "CMSG_MAIL_TAKE_ITEM|SMSG_SEND_MAIL_RESULT"
        "CMSG_GUILD_BANK_QUERY_TAB|SMSG_GUILD_BANK_LIST")
    string(REPLACE "|" ";" pairing_parts "${pairing}")
    list(GET pairing_parts 0 request_name)
    list(GET pairing_parts 1 reply_name)
    if(registry_code MATCHES "DefC${ws}*[(]${ws}*${request_name}${ws}*,")
        if(NOT registry_code MATCHES "DefS${ws}*[(]${ws}*${reply_name}${ws}*,")
            message(FATAL_ERROR
                "${request_name} is registered but ${reply_name} has no outbound metadata: "
                "the request would commit and the client would never hear the result")
        endif()
        if(NOT session_code MATCHES "case${ws}+${reply_name}${ws}*:")
            message(FATAL_ERROR
                "${request_name} is registered but ${reply_name} is not admitted to the "
                "in-world send gate, so its reply is dropped before transmission")
        endif()
    endif()
endforeach()

# A failed mail take must still name the item. The retail equip-error body
# carries a non-zero itemGuidLow with a zero itemCount, so replying with the
# default 0 tells the client an item could not be taken without saying which.
mop_strip_cxx_comments("${mail_handler}" mail_handler_code)
if(NOT mail_handler_code MATCHES "MAIL_ERR_EQUIP_ERROR${ws}*,${ws}*msg${ws}*,${ws}*itemId")
    message(FATAL_ERROR
        "the equip-error mail reply no longer passes the item id, so a failed take "
        "would not tell the client which item it was")
endif()

# CMSG_SET_ACTION_BUTTON is held: its body is proven but the handler's type
# allowlist is narrower than the client's, so two families would be dropped.
if(registry_code MATCHES "DefC${ws}*[(]${ws}*CMSG_SET_ACTION_BUTTON${ws}*,")
    message(FATAL_ERROR
        "CMSG_SET_ACTION_BUTTON is registered while its handler rejects the client's "
        "0x10 and 0x50 type families; recover those before promoting it")
endif()

# A registered speed acknowledgement must have a live arm in the shared handler.
# Registering one whose case is commented out parses the body and then falls to
# "Unknown move type opcode", so the forced-change bookkeeping never runs and the
# packet is silently discarded -- indistinguishable from not registering it.
#
# The list is the COMPLETE family, not just the ones registered today, so a future
# registration cannot escape the rule by being absent from it. The search is
# scoped to HandleForceSpeedChangeAckOpcodes rather than the whole file, so an
# unrelated case elsewhere cannot satisfy it.
mop_strip_cxx_comments("${movement_handler}" movement_handler_code)
string(FIND "${movement_handler_code}" "HandleForceSpeedChangeAckOpcodes" ack_handler_at)
if(ack_handler_at LESS 0)
    message(FATAL_ERROR "HandleForceSpeedChangeAckOpcodes not found; the arm guard cannot be scoped")
endif()
string(SUBSTRING "${movement_handler_code}" ${ack_handler_at} 4000 ack_handler_body)
foreach(ack IN ITEMS
        CMSG_FORCE_WALK_SPEED_CHANGE_ACK CMSG_FORCE_RUN_SPEED_CHANGE_ACK
        CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK CMSG_FORCE_SWIM_SPEED_CHANGE_ACK
        CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK CMSG_FORCE_TURN_RATE_CHANGE_ACK
        CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK
        CMSG_FORCE_PITCH_RATE_CHANGE_ACK)
    if(registry_code MATCHES "DefC${ws}*[(]${ws}*${ack}${ws}*,")
        if(NOT ack_handler_body MATCHES "case${ws}+${ack}${ws}*:")
            message(FATAL_ERROR
                "${ack} is registered but its arm in HandleForceSpeedChangeAckOpcodes is "
                "absent or commented out, so the acknowledgement would be parsed and dropped")
        endif()
    endif()
endforeach()

# CMSG_TOTEM_DESTROYED is active only as one coherent path: exact 18414 wire
# grammar, zero-based bounded slot, sender-owned lookup, dual GUID policy, and
# exact packet consumption. The official manual UI route sends an empty GUID;
# automatic removal sends a concrete GUID which must still match the occupant.
string(FIND "${unit_header}" "uint8 const maskOrder[] = { 4, 2, 1, 3, 0, 6, 7, 5 };" totem_mask_order)
if(totem_mask_order EQUAL -1)
    message(FATAL_ERROR "totem mask order no longer matches the 18414 writer")
endif()
string(FIND "${unit_header}" "uint8 const byteOrder[] = { 6, 2, 4, 1, 5, 0, 3, 7 };" totem_byte_order)
if(totem_byte_order EQUAL -1)
    message(FATAL_ERROR "totem byte order no longer matches the 18414 writer")
endif()
if(NOT opcode_header MATCHES "CMSG_TOTEM_DESTROYED${ws}*=${ws}*0x1263")
    message(FATAL_ERROR "totem opcode value is not the binary-proven 0x1263")
endif()
string(REGEX MATCHALL "DefC${ws}*[(]${ws}*CMSG_TOTEM_DESTROYED${ws}*,${ws}*\"CMSG_TOTEM_DESTROYED\"${ws}*,${ws}*STATUS_LOGGEDIN${ws}*,${ws}*PROCESS_THREADUNSAFE${ws}*,${ws}*&WorldSession::HandleTotemDestroyed${ws}*[)]" totem_registrations "${registry_code}")
list(LENGTH totem_registrations totem_registration_count)
if(NOT totem_registration_count EQUAL 1)
    message(FATAL_ERROR "totem registration must be exactly one logged-in world-thread handler row")
endif()
if(NOT opcode_reference MATCHES "CMSG_TOTEM_DESTROYED${ws}+0x1263${ws}+ACTIVE")
    message(FATAL_ERROR "totem reference inventory is not ACTIVE at 0x1263")
endif()
string(FIND "${spell_handler_code}" "void WorldSession::HandleTotemDestroyed" totem_handler_at)
string(FIND "${spell_handler_code}" "void WorldSession::HandleSelfResOpcode" totem_handler_end)
if(totem_handler_at EQUAL -1 OR totem_handler_end EQUAL -1 OR NOT totem_handler_at LESS totem_handler_end)
    message(FATAL_ERROR "totem handler route cannot be isolated")
endif()
math(EXPR totem_handler_length "${totem_handler_end} - ${totem_handler_at}")
string(SUBSTRING "${spell_handler_code}" ${totem_handler_at} ${totem_handler_length} totem_handler_body)
foreach(required IN ITEMS
        "MopCompactPackets::ReadTotemDestroyed(recvPacket, slotId)"
        "if (!_player->IsSelfMover())"
        "MopCompactPackets::IsTotemDestroyedRequestAdmissible(recvPacket, slotId)"
        "GetPlayer()->GetTotem(TotemSlot(slotId))"
        "MopCompactPackets::TotemDestroyedGuidMatches(totemGuid, totem->GetObjectGuid())"
        "totem->UnSummon()")
    string(FIND "${totem_handler_body}" "${required}" required_at)
    if(required_at EQUAL -1)
        message(FATAL_ERROR "totem handler policy is missing: ${required}")
    endif()
endforeach()
if(totem_handler_body MATCHES "[+][+]${ws}*slotId|slotId${ws}*[+][+]")
    message(FATAL_ERROR "totem handler increments the zero-based wire slot")
endif()
string(FIND "${unit_header}" "slotId < MAX_TOTEM_SLOT && in.rpos() == in.size() &&" totem_admission)
if(totem_admission EQUAL -1)
    message(FATAL_ERROR "totem admission must reject hostile slots and trailing bytes")
endif()
string(FIND "${unit_header}" "if (in[index] == 0x01)" totem_canonical_guid)
if(totem_canonical_guid EQUAL -1)
    message(FATAL_ERROR "totem admission must reject non-canonical present zero GUID bytes")
endif()
string(FIND "${unit_header}" "return requestedGuid.IsEmpty() || requestedGuid == occupiedGuid;" totem_guid_policy)
if(totem_guid_policy EQUAL -1)
    message(FATAL_ERROR "totem GUID policy must accept the empty sentinel and reject non-empty mismatch")
endif()

# MARK_AS_READ owes no reply and may stand alone, but exactly once: a duplicate
# registration reached the tree already, from a script that was not atomic.
string(REGEX MATCHALL "DefC${ws}*[(]${ws}*CMSG_MAIL_MARK_AS_READ${ws}*," mark_as_read_registrations "${registry_code}")
list(LENGTH mark_as_read_registrations mark_as_read_count)
if(NOT mark_as_read_count EQUAL 1)
    message(FATAL_ERROR "CMSG_MAIL_MARK_AS_READ must be registered exactly once, found ${mark_as_read_count}")
endif()
# Speed commands must delegate to Unit::SetSpeedRate rather than building packets.
# Hand-built sends miss three things at once: the direct body goes to the wrong
# audience, m_speed_rate is never updated, and m_forced_speed_changes is never
# bumped -- so HandleForceSpeedChangeAck sees an unexpected acknowledgement,
# compares it against a stale server speed and kicks the player for cheating.
if(player_stats_mods MATCHES "MopCompactPackets::Build(Spline)?MoveSet")
    message(FATAL_ERROR "a stat-mod command builds a speed packet by hand instead of using SetSpeedRate")
endif()
# Require the COMPLETE call, not merely a mention of SetSpeedRate. The argument
# is an absolute speed and SetSpeedRate takes a rate, so the per-type division is
# the load-bearing part: without it the command asks for fifteen times base. The
# two trailing trues select forced handling and force the send even when the rate
# is unchanged, which is what registers the forced change the ack is matched
# against. A bare SetSpeedRate(MOVE_RUN, speed, true, true) must NOT pass.
foreach(move_type IN ITEMS MOVE_RUN MOVE_SWIM)
    if(NOT player_stats_mods MATCHES
       "SetSpeedRate\\([ \t]*${move_type}[ \t]*,[ \t]*speed[ \t]*/[ \t]*baseMoveSpeed\\[${move_type}\\][ \t]*,[ \t]*true[ \t]*,[ \t]*true[ \t]*\\)")
        message(FATAL_ERROR "mount command must set ${move_type} via SetSpeedRate with the absolute-to-rate division and forced/ignoreChange")
    endif()
endforeach()
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

# Scoped to the shared speed path only. The stat-mod commands are required NOT
# to build these bodies at all -- see the SetSpeedRate delegation rule above --
# so demanding the builder appear there would reject the correct shape.
if(NOT unit_speed MATCHES "MopCompactPackets::BuildMoveSetSwimSpeed")
    message(FATAL_ERROR "swim-speed sender bypasses the shared 5.4.8 serializer")
endif()

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

if(NOT unit_header MATCHES "BuildCancelAutoRepeat")
    message(FATAL_ERROR "cancel-auto-repeat serializer is missing")
endif()
if(NOT unit_header MATCHES "WriteGuidMask<1, 3, 0, 4, 6, 7, 5, 2>\\(guid\\)")
    message(FATAL_ERROR "cancel-auto-repeat GUID mask order does not match reader sub_6FA553")
endif()
if(NOT unit_header MATCHES "WriteGuidBytes<7, 6, 2, 5, 0, 4, 1, 3>\\(guid\\)")
    message(FATAL_ERROR "cancel-auto-repeat GUID byte order does not match reader sub_6FA553")
endif()
if(NOT player_combat MATCHES "MopCompactPackets::BuildCancelAutoRepeat")
    message(FATAL_ERROR "cancel-auto-repeat sender bypasses the 18414 serializer")
endif()
if(player_combat MATCHES "SMSG_CANCEL_AUTO_REPEAT,[^\r\n]*GetPackGUID")
    message(FATAL_ERROR "legacy cancel-auto-repeat packed-GUID body remains")
endif()
if(NOT opcode_registry MATCHES "DefS\\(SMSG_CANCEL_AUTO_REPEAT,[ \t]*\"SMSG_CANCEL_AUTO_REPEAT\"\\)")
    message(FATAL_ERROR "SMSG_CANCEL_AUTO_REPEAT is missing outbound opcode metadata")
endif()
if(NOT world_session MATCHES "case[ \t]+SMSG_CANCEL_AUTO_REPEAT:")
    message(FATAL_ERROR "SMSG_CANCEL_AUTO_REPEAT is missing from the converted-packet gate")
endif()
if(NOT opcode_header MATCHES "SMSG_CANCEL_AUTO_REPEAT[ \t]*=[ \t]*0x1E0F")
    message(FATAL_ERROR "SMSG_CANCEL_AUTO_REPEAT does not use the direct 18414 value 0x1E0F")
endif()
if(NOT opcode_reference MATCHES "SMSG_CANCEL_AUTO_REPEAT[ \t]+0x1E0F[ \t]+ACTIVE")
    message(FATAL_ERROR "reference inventory does not record active 0x1E0F cancel auto-repeat")
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

if(NOT unit_header MATCHES "WriteGuidMask<7, 2>\\(victim\\)")
    message(FATAL_ERROR "party-kill GUID mask order does not match reader sub_6F2FE4")
endif()
if(NOT unit_header MATCHES "WriteGuidBytes<0, 5>\\(victim\\)")
    message(FATAL_ERROR "party-kill GUID byte order does not match reader sub_6F2FE4")
endif()
if(NOT unit MATCHES "MopCompactPackets::BuildPartyKillLog\\(data,")
    message(FATAL_ERROR "party-kill sender bypasses the 18414 serializer")
endif()
if(unit MATCHES "WorldPacket[ \\t]+data\\(SMSG_PARTYKILLLOG")
    message(FATAL_ERROR "legacy raw party-kill sender remains")
endif()
if(NOT opcode_registry MATCHES "DefS\\(SMSG_PARTYKILLLOG,[ \\t]*\"SMSG_PARTYKILLLOG\"\\)")
    message(FATAL_ERROR "SMSG_PARTYKILLLOG is missing outbound opcode metadata")
endif()
if(NOT world_session MATCHES "case[ \\t]+SMSG_PARTYKILLLOG:")
    message(FATAL_ERROR "SMSG_PARTYKILLLOG is missing from the converted-packet gate")
endif()
if(NOT opcode_reference MATCHES "SMSG_PARTYKILLLOG[ \\t]+0x048A[ \\t]+ACTIVE")
    message(FATAL_ERROR "reference inventory does not record active party-kill log")
endif()

if(NOT player_header MATCHES "namespace MopDuelPackets")
    message(FATAL_ERROR "duel-state serializers are missing from owning player code")
endif()
if(NOT player_header MATCHES "out.WriteBit\\(completed\\)")
    message(FATAL_ERROR "duel-complete body is not the one-bit 18414 layout")
endif()
if(NOT player_header MATCHES "out << milliseconds;")
    message(FATAL_ERROR "duel-countdown body is not the one-uint32 18414 layout")
endif()
foreach(builder IN ITEMS BuildOutOfBounds BuildInBounds BuildComplete BuildCountdown)
    if(NOT player_duel MATCHES "MopDuelPackets::${builder}")
        message(FATAL_ERROR "duel sender bypasses ${builder}")
    endif()
endforeach()
foreach(server_name IN ITEMS
        SMSG_DUEL_OUTOFBOUNDS SMSG_DUEL_INBOUNDS SMSG_DUEL_COMPLETE SMSG_DUEL_COUNTDOWN)
    if(NOT opcode_registry MATCHES "DefS\\(${server_name},[ \\t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
    if(NOT world_session MATCHES "case[ \\t]+${server_name}:")
        message(FATAL_ERROR "${server_name} is missing from the converted-packet gate")
    endif()
    if(NOT opcode_reference MATCHES "${server_name}[ \\t]+0x[0-9A-F]+[ \\t]+ACTIVE")
        message(FATAL_ERROR "reference inventory does not record active ${server_name}")
    endif()
endforeach()
if(player_duel MATCHES "WorldPacket[ \\t]+data\\(SMSG_DUEL_(OUTOFBOUNDS|INBOUNDS|COMPLETE|COUNTDOWN)")
    message(FATAL_ERROR "legacy raw duel-state packet construction remains")
endif()

if(NOT player_header MATCHES "out.WriteGuidMask<4, 2, 7>\\(initiator\\)")
    message(FATAL_ERROR "duel-request initiator mask order does not match reader sub_6CA34C")
endif()
if(NOT player_header MATCHES "out.WriteGuidBytes<5, 3>\\(arbiter\\)")
    message(FATAL_ERROR "duel-request arbiter byte order does not match reader sub_6CA34C")
endif()
if(NOT player_header MATCHES "out.WriteBits\\(uint32\\(winnerName.size\\(\\)\\), 6\\)")
    message(FATAL_ERROR "duel-winner name length is not six bits")
endif()
if(NOT player_header MATCHES "out << loserRealmAddress;")
    message(FATAL_ERROR "duel-winner crossed realm/name order does not match reader sub_6CFDCC")
endif()
if(NOT spell_effect_object_combat MATCHES "MopDuelPackets::BuildRequested")
    message(FATAL_ERROR "duel-request sender bypasses the 18414 serializer")
endif()
if(NOT player_duel MATCHES "MopDuelPackets::BuildWinner")
    message(FATAL_ERROR "duel-winner sender bypasses the 18414 serializer")
endif()
foreach(server_name IN ITEMS SMSG_DUEL_REQUESTED SMSG_DUEL_WINNER)
    if(NOT opcode_registry MATCHES "DefS\\(${server_name},[ \\t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
    if(NOT world_session MATCHES "case[ \\t]+${server_name}:")
        message(FATAL_ERROR "${server_name} is missing from the converted-packet gate")
    endif()
    if(NOT opcode_reference MATCHES "${server_name}[ \\t]+0x[0-9A-F]+[ \\t]+ACTIVE")
        message(FATAL_ERROR "reference inventory does not record active ${server_name}")
    endif()
endforeach()
if(spell_effect_object_combat MATCHES "WorldPacket[ \\t]+data\\(SMSG_DUEL_REQUESTED")
    message(FATAL_ERROR "legacy raw duel-request construction remains")
endif()
if(player_duel MATCHES "Initialize\\(SMSG_DUEL_WINNER")
    message(FATAL_ERROR "legacy guessed duel-winner construction remains")
endif()

if(NOT player_header MATCHES "namespace MopMirrorTimerPackets")
    message(FATAL_ERROR "mirror-timer serializers are missing from owning player code")
endif()
if(NOT player_header MATCHES
        "out << maxValue << spellId << currentValue << uint32\\(regeneration\\) << type;")
    message(FATAL_ERROR "start mirror-timer field order does not match reader sub_6F16F9")
endif()
if(NOT player_header MATCHES "out.WriteBit\\(paused\\)")
    message(FATAL_ERROR "start mirror-timer pause flag is not one bit")
endif()
foreach(builder IN ITEMS BuildStart BuildStop)
    if(NOT player_mirror MATCHES "MopMirrorTimerPackets::${builder}")
        message(FATAL_ERROR "mirror-timer sender bypasses ${builder}")
    endif()
endforeach()
if(player_mirror MATCHES "WorldPacket[ \\t]+data\\(SMSG_(START|STOP)_MIRROR_TIMER")
    message(FATAL_ERROR "legacy raw mirror-timer packet construction remains")
endif()
foreach(server_name IN ITEMS SMSG_START_MIRROR_TIMER SMSG_STOP_MIRROR_TIMER)
    if(NOT opcode_registry MATCHES "DefS\\(${server_name},[ \\t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
    if(NOT world_session MATCHES "case[ \\t]+${server_name}:")
        message(FATAL_ERROR "${server_name} is missing from the converted-packet gate")
    endif()
    if(NOT opcode_reference MATCHES "${server_name}[ \\t]+0x[0-9A-F]+[ \\t]+ACTIVE")
        message(FATAL_ERROR "reference inventory does not record active ${server_name}")
    endif()
endforeach()

if(NOT rune_header MATCHES "out.WriteBits\\(uint32\\(runes.size\\(\\)\\), 23\\)")
    message(FATAL_ERROR "rune-resync count is not the 23-bit 18414 layout")
endif()
if(NOT rune_header MATCHES "out << rune.cooldownFraction << uint8\\(rune.type\\);")
    message(FATAL_ERROR "rune-resync record order does not match reader sub_73299D")
endif()
if(NOT rune_header MATCHES "out << uint8\\(newType\\) << index;")
    message(FATAL_ERROR "rune-convert order does not match reader sub_6B9A69")
endif()
foreach(builder IN ITEMS BuildResync BuildAddPower BuildConvert)
    if(NOT rune_source MATCHES "MopRunePackets::${builder}")
        message(FATAL_ERROR "rune sender bypasses ${builder}")
    endif()
endforeach()
if(rune_source MATCHES "WorldPacket[ \\t]+data\\(SMSG_(RESYNC_RUNES|ADD_RUNE_POWER|CONVERT_RUNE)")
    message(FATAL_ERROR "legacy raw rune packet construction remains")
endif()
foreach(server_name IN ITEMS SMSG_RESYNC_RUNES SMSG_ADD_RUNE_POWER SMSG_CONVERT_RUNE)
    if(NOT opcode_registry MATCHES "DefS\\(${server_name},[ \\t]*\"${server_name}\"\\)")
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
    if(NOT world_session MATCHES "case[ \\t]+${server_name}:")
        message(FATAL_ERROR "${server_name} is missing from the converted-packet gate")
    endif()
    if(NOT opcode_reference MATCHES "${server_name}[ \\t]+0x[0-9A-F]+[ \\t]+ACTIVE")
        message(FATAL_ERROR "reference inventory does not record active ${server_name}")
    endif()
endforeach()

if(NOT unit_header MATCHES "namespace MopThreatPackets")
    message(FATAL_ERROR "threat serializers are missing from owning unit code")
endif()
string(FIND "${unit_header}" "out.WriteBits(uint32(entries.size()), 21);" threat_count_layout)
if(threat_count_layout EQUAL -1)
    message(FATAL_ERROR "threat-list count is not the 21-bit 18414 layout")
endif()
string(FIND "${unit_header}" "out.WriteGuidMask<5, 6, 1, 3, 7, 0, 4>(owner);" threat_update_layout)
if(threat_update_layout EQUAL -1)
    message(FATAL_ERROR "threat-update owner mask does not match reader sub_7344A4")
endif()
string(FIND "${unit_header}" "out.WriteGuidMask<3, 0>(selected);" threat_highest_layout)
if(threat_highest_layout EQUAL -1)
    message(FATAL_ERROR "highest-threat selected mask does not match reader sub_736527")
endif()
string(FIND "${unit_header}" "out.WriteGuidBytes<7, 0, 4, 3, 2, 1, 6, 5>(owner);" threat_clear_layout)
if(threat_clear_layout EQUAL -1)
    message(FATAL_ERROR "threat-clear byte order does not match reader sub_6F2392")
endif()
string(FIND "${unit_header}" "out.WriteGuidMask<0, 1, 5>(owner);" threat_remove_layout)
if(threat_remove_layout EQUAL -1)
    message(FATAL_ERROR "threat-remove owner mask does not match reader sub_6DBFD5")
endif()
foreach(builder IN ITEMS BuildUpdate BuildHighest BuildClear BuildRemove)
    if(NOT unit_threat MATCHES "MopThreatPackets::${builder}")
        message(FATAL_ERROR "threat sender bypasses ${builder}")
    endif()
endforeach()
foreach(server_name IN ITEMS
        SMSG_THREAT_UPDATE SMSG_HIGHEST_THREAT_UPDATE SMSG_THREAT_CLEAR SMSG_THREAT_REMOVE)
    string(FIND "${unit_threat}" "WorldPacket data(${server_name}" legacy_threat_sender)
    if(NOT legacy_threat_sender EQUAL -1)
        message(FATAL_ERROR "legacy raw ${server_name} construction remains")
    endif()
    string(FIND "${opcode_registry}" "DefS(${server_name}, \"${server_name}\");" threat_registration)
    if(threat_registration EQUAL -1)
        message(FATAL_ERROR "${server_name} is missing outbound opcode metadata")
    endif()
    string(FIND "${world_session}" "case ${server_name}:" threat_allowlist)
    if(threat_allowlist EQUAL -1)
        message(FATAL_ERROR "${server_name} is missing from the converted-packet gate")
    endif()
    string(REGEX MATCH "${server_name}[ \t]+0x[0-9A-F]+[ \t]+ACTIVE" threat_reference "${opcode_reference}")
    if(threat_reference STREQUAL "")
        message(FATAL_ERROR "reference inventory does not record active ${server_name}")
    endif()
endforeach()

string(FIND "${unit_header}" "out.WriteGuidMask<6, 3, 0, 7, 1, 2, 5, 4>(guid);" dismount_mask)
if(dismount_mask EQUAL -1)
    message(FATAL_ERROR "dismount GUID mask does not match reader sub_6D3AD4")
endif()
string(FIND "${unit_header}" "out.WriteGuidBytes<3, 6, 7, 5, 1, 4, 2, 0>(guid);" dismount_bytes)
if(dismount_bytes EQUAL -1)
    message(FATAL_ERROR "dismount GUID byte order does not match reader sub_6D3AD4")
endif()
string(FIND "${unit}" "MopCompactPackets::BuildDismount(data, GetObjectGuid());" dismount_sender)
if(dismount_sender EQUAL -1)
    message(FATAL_ERROR "dismount sender bypasses the 18414 serializer")
endif()
string(FIND "${unit}" "WorldPacket data(SMSG_DISMOUNT" legacy_dismount_sender)
if(NOT legacy_dismount_sender EQUAL -1)
    message(FATAL_ERROR "legacy raw dismount packet construction remains")
endif()
string(FIND "${opcode_registry}" "DefS(SMSG_DISMOUNT, \"SMSG_DISMOUNT\");" dismount_registration)
if(dismount_registration EQUAL -1)
    message(FATAL_ERROR "SMSG_DISMOUNT is missing outbound opcode metadata")
endif()
string(FIND "${world_session}" "case SMSG_DISMOUNT:" dismount_allowlist)
if(dismount_allowlist EQUAL -1)
    message(FATAL_ERROR "SMSG_DISMOUNT is missing from the converted-packet gate")
endif()
string(REGEX MATCH "SMSG_DISMOUNT[ \t]+0x0E3A[ \t]+ACTIVE" dismount_reference "${opcode_reference}")
if(dismount_reference STREQUAL "")
    message(FATAL_ERROR "reference inventory does not record active 0x0E3A dismount")
endif()

string(FIND "${player_header}" "out.WriteGuidMask<0, 5, 6, 3, 7, 4, 1, 2>(target);" combo_mask)
if(combo_mask EQUAL -1)
    message(FATAL_ERROR "combo-point target mask does not match reader sub_6E2BC4")
endif()
string(FIND "${player_header}" "out.WriteGuidBytes<5, 6, 4, 7, 3, 0>(target);" combo_bytes_first)
if(combo_bytes_first EQUAL -1)
    message(FATAL_ERROR "combo-point leading target bytes do not match reader sub_6E2BC4")
endif()
string(FIND "${player_header}" "out << points;" combo_value)
if(combo_value EQUAL -1)
    message(FATAL_ERROR "combo-point value is missing from the 18414 serializer")
endif()
string(FIND "${player_header}" "out.WriteGuidBytes<2, 1>(target);" combo_bytes_last)
if(combo_bytes_last EQUAL -1)
    message(FATAL_ERROR "combo-point trailing target bytes do not match reader sub_6E2BC4")
endif()
string(FIND "${player_combo}" "MopComboPointPackets::BuildUpdate(data, combotarget->GetObjectGuid(), uint8(m_comboPoints));" combo_sender)
if(combo_sender EQUAL -1)
    message(FATAL_ERROR "combo-point sender bypasses the 18414 serializer")
endif()
string(FIND "${player_combo}" "WorldPacket data(SMSG_UPDATE_COMBO_POINTS" legacy_combo_sender)
if(NOT legacy_combo_sender EQUAL -1)
    message(FATAL_ERROR "legacy raw combo-point packet construction remains")
endif()
string(FIND "${opcode_registry}" "DefS(SMSG_UPDATE_COMBO_POINTS, \"SMSG_UPDATE_COMBO_POINTS\");" combo_registration)
if(combo_registration EQUAL -1)
    message(FATAL_ERROR "SMSG_UPDATE_COMBO_POINTS is missing outbound opcode metadata")
endif()
string(FIND "${world_session}" "case SMSG_UPDATE_COMBO_POINTS:" combo_allowlist)
if(combo_allowlist EQUAL -1)
    message(FATAL_ERROR "SMSG_UPDATE_COMBO_POINTS is missing from the converted-packet gate")
endif()
string(REGEX MATCH "SMSG_UPDATE_COMBO_POINTS[ \t]+0x082F[ \t]+ACTIVE" combo_reference "${opcode_reference}")
if(combo_reference STREQUAL "")
    message(FATAL_ERROR "reference inventory does not record active 0x082F combo points")
endif()
string(FIND "${unit_header}" "out.WriteGuidMask<1, 7, 5, 2, 6, 0, 3, 4>(guid);" pre_resurrect_mask)
if(pre_resurrect_mask EQUAL -1)
    message(FATAL_ERROR "pre-resurrect GUID mask does not match readers sub_6E7875/sub_6D6EF4")
endif()
string(FIND "${unit_header}" "out.WriteGuidBytes<5, 1, 7, 0, 6, 4, 2, 3>(guid);" pre_resurrect_bytes)
if(pre_resurrect_bytes EQUAL -1)
    message(FATAL_ERROR "pre-resurrect GUID byte order does not match readers sub_6E7875/sub_6D6EF4")
endif()
string(FIND "${player_death}" "MopCompactPackets::BuildPreResurrect(data, GetObjectGuid());" pre_resurrect_sender)
if(pre_resurrect_sender EQUAL -1)
    message(FATAL_ERROR "pre-resurrect sender bypasses the 18414 serializer")
endif()
string(FIND "${player_death}" "WorldPacket data(SMSG_PRE_RESURRECT" legacy_pre_resurrect_sender)
if(NOT legacy_pre_resurrect_sender EQUAL -1)
    message(FATAL_ERROR "legacy raw pre-resurrect packet construction remains")
endif()
string(FIND "${opcode_registry}" "DefS(SMSG_PRE_RESURRECT, \"SMSG_PRE_RESURRECT\");" pre_resurrect_registration)
if(pre_resurrect_registration EQUAL -1)
    message(FATAL_ERROR "SMSG_PRE_RESURRECT is missing outbound opcode metadata")
endif()
string(FIND "${world_session}" "case SMSG_PRE_RESURRECT:" pre_resurrect_allowlist)
if(pre_resurrect_allowlist EQUAL -1)
    message(FATAL_ERROR "SMSG_PRE_RESURRECT is missing from the converted-packet gate")
endif()
string(REGEX MATCH "SMSG_PRE_RESURRECT[ 	]+0x19C0[ 	]+ACTIVE" pre_resurrect_reference "${opcode_reference}")
if(pre_resurrect_reference STREQUAL "")
    message(FATAL_ERROR "reference inventory does not record active 0x19C0 pre-resurrect")
endif()
