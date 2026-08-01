# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# MopWireCodec.h/MopHandshake.h/MopFrameReader.h relocated to src/proto/ (Stage 2 CP2,
# ACE removal) -- they were already game-free and are proto's framing/handshake layer
# now. Add proto's include dir; the bare quote-includes in mop_framing_test.cpp are
# unchanged, they just resolve one directory further afield.
add_executable(mop_framing_test mop_framing_test.cpp)
target_include_directories(mop_framing_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/proto)
set_target_properties(mop_framing_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME mop_framing COMMAND mop_framing_test)

add_executable(script_action_fault_key_test script_action_fault_key_test.cpp)
target_include_directories(script_action_fault_key_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers)
set_target_properties(script_action_fault_key_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
add_test(NAME script_action_fault_key COMMAND script_action_fault_key_test)

# MopAuthSession.cpp/MopAuthProof.cpp/MopSocketDrain.h relocated to src/proto/ alongside
# MopWireCodec/MopFrameReader/MopHandshake (same move); MopAuthResponse.{h,cpp} stays
# here in Server/ because its .cpp includes Opcodes.h/SharedDefines.h. Compiling the two
# moved .cpp files directly (as before) rather than linking the proto library avoids a
# second definition of MopAuth::DecodeAuthSession/ComputeAuthProof turning up when this
# target also links 'game' (which links proto): the linker only pulls archive members to
# resolve symbols not yet defined, so once these two TUs define them directly nothing
# from libproto.a is extracted for them.
add_executable(mop_auth_test
    mop_auth_test.cpp
    ${CMAKE_SOURCE_DIR}/src/proto/MopAuthSession.cpp
    ${CMAKE_SOURCE_DIR}/src/proto/MopAuthProof.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MopAuthResponse.cpp)
target_include_directories(mop_auth_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/proto
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_auth_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# MopAuthResponse.cpp includes Opcodes.h, which (per its own comment, ":39") includes the FULL
# WorldSession.h so the opcode table's OpcodeHandler struct size matches across translation units.
# That chain pulls in Item.h/AuctionHouseMgr.h/LFGMgr.h/DBCStructure.h/Object.h/Eluna's LuaValue.h --
# i.e. most of the game module's own header surface. Adding those include dirs one at a time here
# (Object, then AuctionHouseBot, then vmap, then Eluna, ...) chases an unbounded list; linking the
# real 'game' target instead inherits its already-correct PUBLIC include set (and, when
# SCRIPT_LIB_ELUNA is on, Eluna's own include dirs) via CMake usage requirements, which is bounded
# and matches how Server/*.cpp already compiles inside the real game library. mop_auth_test itself
# calls nothing from 'game' -- only enum values and struct layouts are used -- so no game .obj
# beyond what the headers require gets pulled into the test binary.
target_link_libraries(mop_auth_test PRIVATE game)
add_test(NAME mop_auth COMMAND mop_auth_test)

add_executable(mop_charenum_test mop_charenum_test.cpp)
target_include_directories(mop_charenum_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_charenum_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: WorldPacket/Opcodes pull the game header surface; link 'game'.
target_link_libraries(mop_charenum_test PRIVATE game)
add_test(NAME mop_charenum COMMAND mop_charenum_test)

add_executable(mop_trainerbuyfailed_test mop_trainerbuyfailed_test.cpp)
target_include_directories(mop_trainerbuyfailed_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_trainerbuyfailed_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: WorldPacket/Opcodes pull the game header surface; link 'game'.
target_link_libraries(mop_trainerbuyfailed_test PRIVATE game)
add_test(NAME mop_trainerbuyfailed COMMAND mop_trainerbuyfailed_test)

add_executable(mop_creategating_test mop_creategating_test.cpp)
target_include_directories(mop_creategating_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_creategating_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: Common.h/SharedDefines.h pull the game header surface; link 'game'.
target_link_libraries(mop_creategating_test PRIVATE game)
add_test(NAME mop_creategating COMMAND mop_creategating_test)

add_executable(mop_updateobject_test
    mop_updateobject_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MopUpdateObject.cpp)
target_include_directories(mop_updateobject_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_updateobject_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
# Same rationale as mop_auth_test: WorldPacket/Opcodes pull the game header surface; link 'game'.
target_link_libraries(mop_updateobject_test PRIVATE game)
add_test(NAME mop_updateobject COMMAND mop_updateobject_test)

add_test(NAME mop_self_values_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_self_values_source_test.cmake)

foreach(mutation IN ITEMS inventory_only health_feed progression_feed visible_item_feed skill_feed buyback_feed questlog_feed packed_bytes_self_feed packed_bytes_observer_create)
    add_test(NAME mop_self_values_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_self_values_source_test.cmake)
    set_tests_properties(mop_self_values_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_initial_self_appearance_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_initial_self_appearance_source_test.cmake)

foreach(mutation IN ITEMS visible_item_feed combined_builder skill_feed questlog_feed values_block_seed)
    add_test(NAME mop_initial_self_appearance_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_initial_self_appearance_source_test.cmake)
    set_tests_properties(mop_initial_self_appearance_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_gameobject_projection_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gameobject_projection_source_test.cmake)

foreach(mutation IN ITEMS drop_bytes1 bytes1_index_drift bytes1_wrong_source short_rotation_loop stale_reserve)
    add_test(NAME mop_gameobject_projection_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gameobject_projection_source_test.cmake)
    set_tests_properties(mop_gameobject_projection_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_destructible_spawn_state_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_destructible_spawn_state_source_test.cmake)

foreach(mutation IN ITEMS restore_fallthrough drop_destructible_data)
    add_test(NAME mop_destructible_spawn_state_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_destructible_spawn_state_source_test.cmake)
    set_tests_properties(mop_destructible_spawn_state_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_calendar_reset_period_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_calendar_reset_period_source_test.cmake)

foreach(mutation IN ITEMS restore_subtraction clamp_lockout_away drop_period_source drop_overflow_clamp ignore_period_value)
    add_test(NAME mop_calendar_reset_period_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_calendar_reset_period_source_test.cmake)
    set_tests_properties(mop_calendar_reset_period_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_opcode_foundation_test mop_opcode_foundation_test.cpp)
target_include_directories(mop_opcode_foundation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_opcode_foundation_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_opcode_foundation_test PRIVATE game)
add_test(NAME mop_opcode_foundation COMMAND mop_opcode_foundation_test)

add_test(NAME mop_opcode_foundation_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_opcode_foundation_source_test.cmake)

add_test(NAME mop_opcode_reference_status_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_opcode_reference_status_source_test.cmake)

add_test(NAME mop_login_packet_order_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_login_packet_order_source_test.cmake)

# Each mutation restores one ordering the retail capture says never varies; the
# guard has to reject all three or it is not actually holding the order.
foreach(login_order_mutation account_data_after_verify drop_tutorials factions_before_world_states)
    add_test(NAME mop_login_packet_order_source_mutation_${login_order_mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${login_order_mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_login_packet_order_source_test.cmake)
    set_tests_properties(mop_login_packet_order_source_mutation_${login_order_mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_client_opcode_worklist_test mop_client_opcode_worklist_test.cpp)
target_include_directories(mop_client_opcode_worklist_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_client_opcode_worklist_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_client_opcode_worklist_test PRIVATE game)
add_test(NAME mop_client_opcode_worklist COMMAND mop_client_opcode_worklist_test)

add_test(NAME mop_char_response_codes_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_char_response_codes_source_test.cmake)

add_test(NAME mop_client_build_split_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_client_build_split_source_test.cmake)

foreach(build_split_mutation IN ITEMS
        merge_wire_into_data widen_wire_list narrow_data_list
        drop_wire_predicate gridmap_uses_wire_list
        realmbuilds_advertises_data_list banner_advertises_data_list)
    add_test(NAME mop_client_build_split_source_mutation_${build_split_mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${build_split_mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_client_build_split_source_test.cmake)
    set_tests_properties(mop_client_build_split_source_mutation_${build_split_mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

foreach(char_codes_mutation IN ITEMS
        revert_delete_success drop_create_trial drop_heirloom_code
        break_create_success_anchor break_name_in_use_anchor
        restore_duplicate_enum drop_charselect_timezone world_entry_timezone_before_motd
        battlepay_back_to_null drop_login_failed_registration drop_randomname_registration
        corrupt_namegen_format swap_randomname_args drop_namegen_null_guard
        namegen_index_locale_slot_zero)
    add_test(NAME mop_char_response_codes_source_mutation_${char_codes_mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${char_codes_mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_char_response_codes_source_test.cmake)
    set_tests_properties(mop_char_response_codes_source_mutation_${char_codes_mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_client_opcode_worklist_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_client_opcode_worklist_source_test.cmake)

foreach(mutation IN ITEMS
        logout_manual_registration logout_idle_registration logout_manual_value
        violence_registration violence_handler violence_value
        battlefield_status_registration battle_pay_registration battle_pay_duplicate_raw battle_pay_value)
    add_test(NAME mop_client_opcode_worklist_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_client_opcode_worklist_source_test.cmake)
    set_tests_properties(mop_client_opcode_worklist_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_compact_packets_test mop_compact_packets_test.cpp)
target_include_directories(mop_compact_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_compact_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_compact_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_compact_packets_test PRIVATE shared)
add_test(NAME mop_compact_packets COMMAND mop_compact_packets_test)

add_executable(mop_pet_set_action_capability_test
    mop_pet_set_action_capability_test.cpp)
target_include_directories(mop_pet_set_action_capability_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_pet_set_action_capability_test PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_pet_set_action_capability_test PRIVATE game)
add_test(NAME mop_pet_set_action_capability
    COMMAND mop_pet_set_action_capability_test)
if(WIN32)
    get_filename_component(MOP_PET_SET_ACTION_MYSQL_RUNTIME_DIR "${MySQL_LIBRARY}" DIRECTORY)
    set_tests_properties(mop_pet_set_action_capability PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MOP_PET_SET_ACTION_MYSQL_RUNTIME_DIR}")
endif()

add_test(NAME mop_pet_set_action_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_set_action_source_test.cmake)

foreach(mutation IN ITEMS
        reader_swap_scalars reader_mask_order reader_byte_order
        reader_drop_exact_length reader_drop_canonical_byte reader_drop_empty_guid
        reader_drop_tail reader_assign_outputs_early
        pet_drop_zero pet_drop_missing pet_drop_removed pet_admit_arbitrary pet_nonconst
        charm_drop_zero charm_drop_found charm_first_match_only charm_admit_arbitrary charm_nonconst
        handler_restore_raw_guid handler_restore_size_count handler_drop_position
        handler_lookup_before_active_guid handler_drop_active_guid handler_drop_reverse_owner
        handler_weaken_authority_conjunction handler_drop_actions_disabled handler_drop_charminfo
        handler_mask_requested_type handler_accept_clear handler_accept_state01
        handler_accept_reaction handler_accept_command handler_accept_arbitrary
        handler_drop_zero_spell handler_drop_spell_lookup handler_drop_owned_spell
        handler_drop_dbc_passive handler_drop_slot_spell handler_drop_slot_type
        handler_drop_pet_capability handler_drop_charm_capability handler_add_alive_gate
        handler_mirror_before_toggle handler_call_both_toggles handler_second_slot_write
        handler_add_rejection_log registration_wrong_status registration_wrong_processing
        registration_route_06f0 reference_keep_dormant)
    add_test(NAME mop_pet_set_action_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_set_action_source_test.cmake)
    set_tests_properties(mop_pet_set_action_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_pet_stop_attack_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_stop_attack_source_test.cmake)

foreach(mutation IN ITEMS
        stop_mask_order stop_byte_order stop_exact_length stop_canonical_byte
        stop_empty_guid stop_handler_route stop_restore_raw_guid
        stop_registration stop_reference stop_drop_owner
        stop_drop_slot_membership stop_bypass_slot_membership stop_weaken_conjunction stop_drop_alive
        stop_move_attack_before_owner stop_move_attack_before_membership stop_not_found_error
        stop_not_controlled_error stop_log_shape action_drop_owner
        action_drop_slot_membership action_bypass_slot_membership action_weaken_conjunction
        action_move_side_effect_before_membership action_not_found_error
        action_not_controlled_error action_unknown_command_error
        action_unknown_spell_error action_unknown_action_error
        action_drop_charminfo_error)
    add_test(NAME mop_pet_stop_attack_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_stop_attack_source_test.cmake)
    set_tests_properties(
        mop_pet_stop_attack_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_mail_take_item_security_test
    mop_mail_take_item_security_test.cpp)
target_include_directories(mop_mail_take_item_security_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_mail_take_item_security_test PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_mail_take_item_security_test PRIVATE shared)
add_test(NAME mop_mail_take_item_security
    COMMAND mop_mail_take_item_security_test)

add_test(NAME mop_mail_take_item_security_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_take_item_security_source_test.cmake)

foreach(mutation IN ITEMS
        drop_attachment_lookup lookup_after_global_item policy_after_can_store
        drop_receiver_check allow_cod ignore_remove_result
        restore_cod_send_mail restore_cod_modify_money
        cod_reply_item_zero cod_reply_count_one
        remove_before_can_store remove_in_failure_branch success_before_commit
        drop_template_drift_diagnostic log_cross_mail_reject
        log_cod_reject log_receiver_reject log_missing_item_reject)
    add_test(NAME mop_mail_take_item_security_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_take_item_security_source_test.cmake)
    set_tests_properties(mop_mail_take_item_security_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_mail_take_item_security_mutation_harness_target_drift
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -DMUTATION=allow_cod -DMUTATION_TARGET_DRIFT_PROBE=ON
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_take_item_security_source_test.cmake)
set_tests_properties(mop_mail_take_item_security_mutation_harness_target_drift
    PROPERTIES PASS_REGULAR_EXPRESSION
        "mutation target unavailable or wrong guard: allow_cod")

add_test(NAME mop_mail_take_item_security_mutation_harness_unknown_name
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -DMUTATION=unmapped_probe
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_take_item_security_source_test.cmake)
set_tests_properties(mop_mail_take_item_security_mutation_harness_unknown_name
    PROPERTIES PASS_REGULAR_EXPRESSION
        "mutation target unavailable or wrong guard: unmapped_probe")

add_test(NAME mop_guild_banker_activate_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_guild_banker_activate_source_test.cmake)

foreach(mutation IN ITEMS
        activate_mask_order activate_flag_position activate_byte_order
        activate_xor activate_exact_length activate_padding
        activate_canonical_byte activate_all_zero activate_handler_route
        activate_interaction activate_registration query_registration
        list_registration list_admission activate_refresh_side_effect
        reference_activate reference_query reference_list)
    add_test(NAME mop_guild_banker_activate_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_guild_banker_activate_source_test.cmake)
    set_tests_properties(mop_guild_banker_activate_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_mail_packets_test mop_mail_packets_test.cpp)
target_include_directories(mop_mail_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_mail_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_mail_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_mail_packets_test PRIVATE shared)
add_test(NAME mop_mail_packets COMMAND mop_mail_packets_test)

add_test(NAME mop_mail_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_packets_source_test.cmake)

foreach(mutation IN ITEMS builder_route registration allowlist reference
        missing_item utf8_bounds enchant_count legacy_limit mail_count item_cap
        player_guid_domain frame_limit)
    add_test(NAME mop_mail_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_packets_source_test.cmake)
    set_tests_properties(mop_mail_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_compact_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_compact_packets_source_test.cmake)

foreach(mutation IN ITEMS cancel_sender cancel_registration cancel_allowlist cancel_opcode
        cancel_reference cancel_old_route
        auto_repeat_mask auto_repeat_bytes auto_repeat_sender auto_repeat_registration
        auto_repeat_allowlist auto_repeat_opcode auto_repeat_reference
        read_item_alias read_item_sender
        attacker_sender attacker_registration attacker_allowlist attacker_opcode
        attacker_reference attacker_envelope
        party_kill_mask party_kill_bytes party_kill_sender party_kill_registration
        party_kill_allowlist party_kill_reference
        duel_complete_bit duel_countdown_width duel_sender duel_registration
        duel_allowlist duel_reference
        duel_request_mask duel_request_bytes duel_winner_bits duel_winner_realm_order
        duel_request_sender duel_winner_sender duel_pair_registration
        duel_pair_allowlist duel_pair_reference
        mirror_start_order mirror_pause_width mirror_sender mirror_registration
        mirror_allowlist mirror_reference
        rune_count_width rune_record_order rune_convert_order rune_sender
        rune_registration rune_allowlist rune_reference
        threat_count_width threat_update_mask threat_highest_mask threat_clear_bytes
        threat_remove_mask threat_sender threat_registration threat_allowlist threat_reference
        dismount_mask dismount_bytes dismount_sender dismount_registration
        dismount_allowlist dismount_reference
        pre_resurrect_mask pre_resurrect_bytes pre_resurrect_sender
        pre_resurrect_registration pre_resurrect_allowlist pre_resurrect_reference
        combo_mask combo_bytes combo_sender combo_registration combo_allowlist combo_reference
        totem_mask_order totem_byte_order totem_opcode_value totem_registration
        totem_reference totem_handler_route totem_drop_self_mover_guard
        totem_drop_slot_bound totem_increment_slot totem_accept_nonempty_mismatch
        totem_reject_empty_sentinel totem_drop_exact_tail
        totem_accept_noncanonical_zero_byte)
    add_test(NAME mop_compact_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_compact_packets_source_test.cmake)
    set_tests_properties(mop_compact_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_combat_log_packets_test mop_combat_log_packets_test.cpp)
target_include_directories(mop_combat_log_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_combat_log_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_combat_log_packets_test PRIVATE game)
add_test(NAME mop_combat_log_packets COMMAND mop_combat_log_packets_test)

add_executable(mop_aura_packets_test mop_aura_packets_test.cpp)
target_include_directories(mop_aura_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_aura_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_aura_packets_test PRIVATE game)
add_test(NAME mop_aura_packets COMMAND mop_aura_packets_test)

add_test(NAME mop_aura_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_aura_packets_source_test.cmake)
foreach(mutation IN ITEMS full_update_mode flag_translation registration whitelist)
    add_test(NAME mop_aura_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_aura_packets_source_test.cmake)
    set_tests_properties(mop_aura_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_combat_log_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_combat_log_packets_source_test.cmake)
foreach(mutation IN ITEMS instakill_builder instakill_sender instakill_registration instakill_gate instakill_reference
        energize_builder energize_sender energize_registration energize_gate energize_reference
        heal_builder heal_sender heal_registration heal_gate heal_reference
        damage_shield_builder damage_shield_sender damage_shield_registration damage_shield_gate damage_shield_reference
        spell_miss_builder spell_miss_sender spell_miss_registration spell_miss_gate spell_miss_reference
        spell_miss_count_width spell_miss_count_bound)
    add_test(NAME mop_combat_log_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_combat_log_packets_source_test.cmake)
    set_tests_properties(mop_combat_log_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_respec_packets_test mop_respec_packets_test.cpp)
target_include_directories(mop_respec_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_respec_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_respec_packets_test PRIVATE game)
add_test(NAME mop_respec_packets COMMAND mop_respec_packets_test)

add_test(NAME mop_respec_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_respec_packets_source_test.cmake)

add_executable(mop_party_stats_packets_test mop_party_stats_packets_test.cpp)
target_include_directories(mop_party_stats_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_party_stats_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_party_stats_packets_test PRIVATE game)
add_test(NAME mop_party_stats_packets COMMAND mop_party_stats_packets_test)

add_test(NAME mop_party_stats_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_party_stats_packets_source_test.cmake)

add_executable(mop_stable_packets_test mop_stable_packets_test.cpp)
target_include_directories(mop_stable_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_stable_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_stable_packets_test PRIVATE game)
add_test(NAME mop_stable_packets COMMAND mop_stable_packets_test)

add_test(NAME mop_stable_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_stable_packets_source_test.cmake)

add_executable(mop_ready_check_packets_test mop_ready_check_packets_test.cpp)
target_include_directories(mop_ready_check_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_ready_check_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_ready_check_packets_test PRIVATE game)
add_test(NAME mop_ready_check_packets COMMAND mop_ready_check_packets_test)

add_test(NAME mop_ready_check_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ready_check_source_test.cmake)

add_executable(mop_group_marker_packets_test mop_group_marker_packets_test.cpp)
target_include_directories(mop_group_marker_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_group_marker_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_group_marker_packets_test PRIVATE game)
add_test(NAME mop_group_marker_packets COMMAND mop_group_marker_packets_test)

add_test(NAME mop_group_marker_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_group_marker_source_test.cmake)

add_executable(mop_group_invite_response_packets_test
    mop_group_invite_response_packets_test.cpp)
target_include_directories(mop_group_invite_response_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_group_invite_response_packets_test PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_group_invite_response_packets_test PRIVATE game)
add_test(NAME mop_group_invite_response_packets
    COMMAND mop_group_invite_response_packets_test)
if(WIN32)
    get_filename_component(MOP_GROUP_INVITE_MYSQL_RUNTIME_DIR "${MySQL_LIBRARY}" DIRECTORY)
    set_tests_properties(mop_group_invite_response_packets PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MOP_GROUP_INVITE_MYSQL_RUNTIME_DIR}")
endif()

add_test(NAME mop_group_invite_response_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_group_invite_response_source_test.cmake)

foreach(mutation IN ITEMS
        group_invite_response_opcode_value
        group_invite_response_registration
        group_invite_response_reference
        group_invite_response_skip_marker
        group_invite_response_marker_value
        group_invite_response_swap_bits
        group_invite_response_drop_padding_guard
        group_invite_response_unconditional_roles
        group_invite_response_roles_endian
        group_invite_response_drop_exact_tail
        group_invite_response_side_effect_before_parse
        group_invite_response_branch_on_has_roles
        group_invite_response_decline_group_uaf
        group_invite_response_self_accept_after_remove
        group_invite_response_bot_logout_skip_invite_cleanup
        group_invite_response_unexpected_smsg_activation)
    add_test(NAME mop_group_invite_response_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_group_invite_response_source_test.cmake)
    set_tests_properties(
        mop_group_invite_response_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_auction_packets_test mop_auction_packets_test.cpp)
target_include_directories(mop_auction_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_auction_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_auction_packets_test PRIVATE game)
add_test(NAME mop_auction_packets COMMAND mop_auction_packets_test)

add_test(NAME mop_auction_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_auction_packets_source_test.cmake)

add_executable(mop_world_entry_packets_test mop_world_entry_packets_test.cpp)
target_include_directories(mop_world_entry_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_world_entry_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_world_entry_packets_test PRIVATE game)
add_test(NAME mop_world_entry_packets COMMAND mop_world_entry_packets_test)

add_test(NAME mop_world_entry_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_world_entry_packets_source_test.cmake)

foreach(mutation IN ITEMS time_sync_registration time_sync_parse_order)
    add_test(NAME mop_world_entry_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_world_entry_packets_source_test.cmake)
    set_tests_properties(mop_world_entry_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

# Two opcode symbols sharing one value in the same direction. DefC/DefS assert at
# runtime, but only for opcodes actually registered -- a collision between two
# unregistered names is invisible until someone registers the second, which is how
# MSG_MOVE_WORLDPORT_ACK on 0x00E0 (CMSG_CHAR_ENUM in 18414) hung every client.
add_test(NAME mop_opcode_collision_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_opcode_collision_source_test.cmake)

foreach(mutation IN ITEMS new_collision msg_hides_collision double_registration
                          unpadded_value symbolic_alias dual_registration_direction
                          third_collider invert_client_guard invert_server_guard
                          stale_baseline)
    add_test(NAME mop_opcode_collision_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_opcode_collision_source_test.cmake)
    set_tests_properties(mop_opcode_collision_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_addon_packets_test mop_addon_packets_test.cpp)
target_include_directories(mop_addon_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_addon_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_addon_packets_test PRIVATE game)
add_test(NAME mop_addon_packets COMMAND mop_addon_packets_test)

add_executable(mop_control_packets_test mop_control_packets_test.cpp)
target_include_directories(mop_control_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_control_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_control_packets_test PRIVATE game)
add_test(NAME mop_control_packets COMMAND mop_control_packets_test)

add_executable(mop_currency_packets_test mop_currency_packets_test.cpp)
target_include_directories(mop_currency_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_currency_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_currency_packets_test PRIVATE game)
add_test(NAME mop_currency_packets COMMAND mop_currency_packets_test)

add_test(NAME mop_currency_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_currency_packets_source_test.cmake)

foreach(mutation IN ITEMS weekly_registration weekly_admission weekly_body
        weekly_value weekly_reference)
    add_test(NAME mop_currency_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_currency_packets_source_test.cmake)
    set_tests_properties(mop_currency_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_initial_mover_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_initial_mover_source_test.cmake)

add_executable(mop_initial_packets_test mop_initial_packets_test.cpp)
target_include_directories(mop_initial_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_initial_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_initial_packets_test PRIVATE game)
add_test(NAME mop_initial_packets COMMAND mop_initial_packets_test)

add_test(NAME mop_initial_packets_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_initial_packets_source_test.cmake)

add_executable(mop_achievement_packets_test mop_achievement_packets_test.cpp)
target_include_directories(mop_achievement_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_achievement_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_achievement_packets_test PRIVATE game)
add_test(NAME mop_achievement_packets COMMAND mop_achievement_packets_test)

add_test(NAME mop_achievement_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_achievement_packets_source_test.cmake)

foreach(mutation IN ITEMS
        special_pvp_allow_self_kill special_pvp_allow_empty_requirements
        special_pvp_empty_query_inverted
        earned_mask_order earned_boolean earned_byte_order earned_scalar_order
        earned_producer_mapping earned_sender_opcode earned_broadcast_scope
        earned_chat_sender_name earned_legacy_body earned_opcode_value earned_registration
        earned_admission earned_reference
        criteria_update_mask_order criteria_update_byte_order
        criteria_update_scalar_order criteria_update_timer_order
        criteria_update_counter_width criteria_update_expiry_player_guid
        criteria_update_expiry_id
        criteria_update_expiry_counter criteria_update_expiry_flags
        criteria_update_expiry_packed_date criteria_update_expiry_timer1
        criteria_update_expiry_timer2 criteria_update_expiry_sender_opcode
        criteria_update_expiry_delivery criteria_update_expiry_callsite
        criteria_update_expiry_loading_gate criteria_update_generic_send
        criteria_update_generic_callsite_count
        deleted_body_second_word deleted_reset_producer deleted_incomplete_producer
        deleted_reset_loading_gate deleted_incomplete_loading_gate
        criteria_deleted_loading_gate criteria_deleted_reset_escape
        criteria_update_registration criteria_update_admission criteria_update_reference
        criteria_deleted_registration criteria_deleted_admission criteria_deleted_reference
        achievement_deleted_registration achievement_deleted_admission achievement_deleted_reference)
    add_test(NAME mop_achievement_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_achievement_packets_source_test.cmake)
    set_tests_properties(mop_achievement_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_ui_envelope_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ui_envelope_source_test.cmake)

foreach(mutation IN ITEMS pre_map_call after_map_call opcode_metadata)
    add_test(NAME mop_ui_envelope_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ui_envelope_source_test.cmake)
    set_tests_properties(mop_ui_envelope_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

foreach(mutation IN ITEMS builder_call opcode_metadata binder_reader
        binder_confirm_builder playerbound_builder binder_registration
        binder_allowlist tutorial_flag_registration tutorial_clear_registration
        tutorial_reset_registration tutorial_flag_reader
        forced_reactions_registration forced_reactions_handler)
    add_test(NAME mop_initial_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_initial_packets_source_test.cmake)
    set_tests_properties(mop_initial_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_creature_query_test mop_creature_query_test.cpp)
target_include_directories(mop_creature_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_creature_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_creature_query_test PRIVATE game)
add_test(NAME mop_creature_query COMMAND mop_creature_query_test)

add_test(NAME mop_creature_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_creature_query_source_test.cmake)

foreach(mutation IN ITEMS builder_call inbound_registration)
    add_test(NAME mop_creature_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_creature_query_source_test.cmake)
    set_tests_properties(mop_creature_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_gameobject_query_test mop_gameobject_query_test.cpp)
target_include_directories(mop_gameobject_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_gameobject_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_gameobject_query_test PRIVATE game)
add_test(NAME mop_gameobject_query COMMAND mop_gameobject_query_test)

add_test(NAME mop_gameobject_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gameobject_query_source_test.cmake)

foreach(mutation IN ITEMS quest_item_zero_skip request_parser_call response_builder_call inbound_registration)
    add_test(NAME mop_gameobject_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gameobject_query_source_test.cmake)
    set_tests_properties(mop_gameobject_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_corpse_query_test mop_corpse_query_test.cpp)
target_include_directories(mop_corpse_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_corpse_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_corpse_query_test PRIVATE game)
add_test(NAME mop_corpse_query COMMAND mop_corpse_query_test)

add_test(NAME mop_corpse_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_corpse_query_source_test.cmake)

foreach(mutation IN ITEMS query_builder map_reader map_builder
        inbound_registration outbound_registration allowlist)
    add_test(NAME mop_corpse_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_corpse_query_source_test.cmake)
    set_tests_properties(mop_corpse_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_quest_poi_query_test mop_quest_poi_query_test.cpp)
target_include_directories(mop_quest_poi_query_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/game/Server
    ${CMAKE_SOURCE_DIR}/src/game)
set_target_properties(mop_quest_poi_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_quest_poi_query_test PRIVATE game)
add_test(NAME mop_quest_poi_query COMMAND mop_quest_poi_query_test)

add_executable(mop_quest_npc_query_test mop_quest_npc_query_test.cpp)
target_include_directories(mop_quest_npc_query_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/game/Server
    ${CMAKE_SOURCE_DIR}/src/game)
set_target_properties(mop_quest_npc_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_quest_npc_query_test PRIVATE game)
add_test(NAME mop_quest_npc_query COMMAND mop_quest_npc_query_test)

add_test(NAME mop_quest_poi_query_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_poi_query_source_test.cmake)
foreach(mutation IN ITEMS request_parser response_builder inbound_registration outbound_registration allowlist floor_clamp)
    add_test(NAME mop_quest_poi_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_poi_query_source_test.cmake)
    set_tests_properties(mop_quest_poi_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_ping_field_order_source
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ping_field_order_source_test.cmake)
foreach(mutation IN ITEMS swapped_read echoed_latency)
    add_test(NAME mop_ping_field_order_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ping_field_order_source_test.cmake)
    set_tests_properties(mop_ping_field_order_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_name_query_test mop_name_query_test.cpp)
target_include_directories(mop_name_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_name_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_name_query_test PRIVATE game)
add_test(NAME mop_name_query COMMAND mop_name_query_test)

add_test(NAME mop_name_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_name_query_source_test.cmake)

add_executable(mop_realmquery_test mop_realmquery_test.cpp)
target_include_directories(mop_realmquery_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_realmquery_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_realmquery_test PRIVATE game)
add_test(NAME mop_realmquery COMMAND mop_realmquery_test)

foreach(mutation IN ITEMS request_parser_call response_builder_call inbound_registration
        early_result_delete tail_result_delete)
    add_test(NAME mop_name_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_name_query_source_test.cmake)
    set_tests_properties(mop_name_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_time_query_test mop_time_query_test.cpp)
target_include_directories(mop_time_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_time_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_time_query_test PRIVATE game)
add_test(NAME mop_time_query COMMAND mop_time_query_test)

add_test(NAME mop_time_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_time_query_source_test.cmake)

foreach(mutation IN ITEMS query_builder_call played_builder_call query_registration played_registration)
    add_test(NAME mop_time_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_time_query_source_test.cmake)
    set_tests_properties(mop_time_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_mail_query_test mop_mail_query_test.cpp)
target_include_directories(mop_mail_query_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_mail_query_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_mail_query_test PRIVATE game)
add_test(NAME mop_mail_query COMMAND mop_mail_query_test)

add_test(NAME mop_mail_query_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_query_source_test.cmake)

foreach(mutation IN ITEMS builder_call inbound_registration outbound_registration)
    add_test(NAME mop_mail_query_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mail_query_source_test.cmake)
    set_tests_properties(mop_mail_query_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_rated_bg_packets_test mop_rated_bg_packets_test.cpp)
target_include_directories(mop_rated_bg_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_rated_bg_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_rated_bg_packets_test PRIVATE game)
add_test(NAME mop_rated_bg_packets COMMAND mop_rated_bg_packets_test)

add_test(NAME mop_rated_bg_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_rated_bg_packets_source_test.cmake)

foreach(mutation IN ITEMS
        builder_call inbound_registration outbound_registration
        conquest_builder_call conquest_inbound_registration conquest_outbound_registration
        conquest_allowlist)
    add_test(NAME mop_rated_bg_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_rated_bg_packets_source_test.cmake)
    set_tests_properties(mop_rated_bg_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_calendar_packets_test mop_calendar_packets_test.cpp)
target_include_directories(mop_calendar_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_calendar_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_calendar_packets_test PRIVATE game)
add_test(NAME mop_calendar_packets COMMAND mop_calendar_packets_test)

add_test(NAME mop_calendar_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_calendar_packets_source_test.cmake)

foreach(mutation IN ITEMS initial_builder status_builder moderator_builder initial_registration status_registration moderator_registration pending_client_registration pending_server_registration pending_gate
        list_builder event_builder get_calendar_registration get_event_registration send_calendar_registration send_event_registration send_calendar_gate send_event_gate get_event_size_guard
        legacy_list_body legacy_event_body)
    add_test(NAME mop_calendar_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_calendar_packets_source_test.cmake)
    set_tests_properties(mop_calendar_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_party_update_packets_test mop_party_update_packets_test.cpp)
target_include_directories(mop_party_update_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_party_update_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_party_update_packets_test PRIVATE game)
add_test(NAME mop_party_update_packets COMMAND mop_party_update_packets_test)

add_executable(mop_guild_packets_test mop_guild_packets_test.cpp)
target_include_directories(mop_guild_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_guild_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_guild_packets_test PRIVATE game)
add_test(NAME mop_guild_packets COMMAND mop_guild_packets_test)

add_executable(mop_guild_bank_packets_test mop_guild_bank_packets_test.cpp)
target_include_directories(mop_guild_bank_packets_test PRIVATE
    ${CMAKE_SOURCE_DIR}/src/game/Object
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_guild_bank_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_guild_bank_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_guild_bank_packets_test PRIVATE shared)
add_test(NAME mop_guild_bank_packets COMMAND mop_guild_bank_packets_test)

add_test(NAME mop_guild_bank_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_guild_bank_packets_source_test.cmake)

foreach(mutation IN ITEMS builder_route query_boolean tab_coalesce money_recipient
        metadata_without_view zero_tab_metadata single_delta_bounds vector_delta_bounds
        zero_tab_full_update dynamic_flags_producer dynamic_flags_hold
        socket_order tab_lengths tab_data_order locked_zero modifier_contract
        client_limits activation_registry activation_admission activation_reference legacy_writer)
    add_test(NAME mop_guild_bank_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_guild_bank_packets_source_test.cmake)
    set_tests_properties(mop_guild_bank_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_chat_packets_test mop_chat_packets_test.cpp)
target_include_directories(mop_chat_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_chat_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_chat_packets_test PRIVATE game)
add_test(NAME mop_chat_packets COMMAND mop_chat_packets_test)

add_test(NAME mop_language_descriptors_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_language_descriptors_source_test.cmake)

add_test(NAME mop_core_banner_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_core_banner_source_test.cmake)

add_test(NAME mop_world_packet_logging_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_world_packet_logging_source_test.cmake)

add_test(NAME mop_default_language_skill_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_default_language_skill_source_test.cmake)
add_test(NAME mop_default_language_skill_source_mutation_repair
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -DMUTATION=repair
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_default_language_skill_source_test.cmake)
set_tests_properties(mop_default_language_skill_source_mutation_repair
    PROPERTIES WILL_FAIL TRUE)

add_test(NAME mop_chat_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_chat_packets_source_test.cmake)

foreach(mutation IN ITEMS builder registration allowlist emote_builder emote_reader emote_registration emote_allowlist emote_body cross_faction_denial text_bound receiver_identity addon_identity
        addon_count_width addon_length_width addon_softcap addon_unregister_registration addon_batch_registration addon_unregister_handler addon_batch_handler addon_group_filter addon_group_delivery addon_guild_filter addon_whisper_filter
        say_registration say_parser_wiring say_parser_layout say_language_spell say_reference_status inline_length_width realm_suffix_strip
        afk_registration afk_parser_wiring afk_parser_layout
        notice_length not_found_sender ambiguous_sender restricted_sender
        notice_registration notice_allowlist notice_reference
        legacy_wrong_faction legacy_gm_chat legacy_member_count)
    add_test(NAME mop_chat_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_chat_packets_source_test.cmake)
    set_tests_properties(mop_chat_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_channel_packets_test mop_channel_packets_test.cpp)
target_include_directories(mop_channel_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_channel_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_channel_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_channel_packets_test PRIVATE shared)
add_test(NAME mop_channel_packets COMMAND mop_channel_packets_test)

add_executable(mop_trade_packets_test mop_trade_packets_test.cpp)
target_include_directories(mop_trade_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_trade_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_trade_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_trade_packets_test PRIVATE shared)
add_test(NAME mop_trade_packets COMMAND mop_trade_packets_test)

add_test(NAME mop_trade_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_trade_packets_source_test.cmake)

foreach(mutation IN ITEMS status_values status_registration extended_registration
        status_gate extended_gate status_builder proposal_builder extended_builder
        count_width rollback_status over_money)
    add_test(NAME mop_trade_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_trade_packets_source_test.cmake)
    set_tests_properties(mop_trade_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_channel_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_channel_packets_source_test.cmake)

foreach(mutation IN ITEMS list_realm notify_guid_identity wrong_faction_tail
        you_joined_tail you_left_tail list_registration notify_registration
        list_gate notify_gate)
    add_test(NAME mop_channel_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_channel_packets_source_test.cmake)
    set_tests_properties(mop_channel_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_pet_unlearn_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_unlearn_legacy_source_test.cmake)

foreach(mutation IN ITEMS hidden_option legacy_sender legacy_handler legacy_opcodes)
    add_test(NAME mop_pet_unlearn_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_unlearn_legacy_source_test.cmake)
    set_tests_properties(mop_pet_unlearn_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_lfg_search_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_lfg_search_legacy_source_test.cmake)

foreach(mutation IN ITEMS legacy_sender legacy_handlers legacy_opcodes modern_update)
    add_test(NAME mop_lfg_search_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_lfg_search_legacy_source_test.cmake)
    set_tests_properties(mop_lfg_search_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_all_quests_completed_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_all_quests_completed_legacy_source_test.cmake)

foreach(mutation IN ITEMS legacy_handler legacy_opcode quest_backend)
    add_test(NAME mop_all_quests_completed_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_all_quests_completed_legacy_source_test.cmake)
    set_tests_properties(mop_all_quests_completed_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_mirror_image_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mirror_image_legacy_source_test.cmake)
foreach(mutation IN ITEMS legacy_handler legacy_opcode modern_request modern_split)
    add_test(NAME mop_mirror_image_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_mirror_image_legacy_source_test.cmake)
    set_tests_properties(mop_mirror_image_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_notification_packets_test mop_notification_packets_test.cpp)
target_include_directories(mop_notification_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_notification_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_notification_packets_test PRIVATE game)
add_test(NAME mop_notification_packets COMMAND mop_notification_packets_test)

add_test(NAME mop_notification_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_notification_packets_source_test.cmake)

foreach(mutation IN ITEMS length_width registration allowlist area_trigger_route admin_route
        compatibility_forward legacy_backend legacy_opcode opcode_value)
    add_test(NAME mop_notification_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_notification_packets_source_test.cmake)
    set_tests_properties(mop_notification_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_currency_loot_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_currency_loot_legacy_source_test.cmake)

foreach(mutation IN ITEMS currency_send legacy_selector legacy_opcode currency_accounting
        generic_backend premature_registration premature_allowlist)
    add_test(NAME mop_currency_loot_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_currency_loot_legacy_source_test.cmake)
    set_tests_properties(mop_currency_loot_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_lfg_packets_test mop_lfg_packets_test.cpp)
target_include_directories(mop_lfg_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_lfg_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_lfg_packets_test PRIVATE game)
add_test(NAME mop_lfg_packets COMMAND mop_lfg_packets_test)

add_test(NAME mop_lfg_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_lfg_packets_source_test.cmake)

foreach(mutation IN ITEMS builder_call outbound_registration opcode_value countdown
        status_builder_call status_registration status_opcode status_field_mapping status_reason_value
        lfr_join_registration lfr_leave_registration lfr_search_registration
        lfr_join_declaration lfr_leave_declaration lfr_join_definition lfr_leave_definition
        lfr_parser_calls lfr_builder_call lfr_dbc_lookup lfr_type_validation
        lfr_converted_packet lfr_join_opcode lfr_leave_opcode lfr_search_opcode
        lfr_bit_order queue_builder_call queue_registration queue_opcode
        queue_allowlist queue_bit_order queue_metadata)
    add_test(NAME mop_lfg_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_lfg_packets_source_test.cmake)
    set_tests_properties(mop_lfg_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_guild_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_guild_packets_source_test.cmake)

foreach(mutation IN ITEMS login_builder broadcast_builder outbound_registration opcode_value
        tabard_reader tabard_builder save_reader save_builder tabard_registration invite_realm_suffix
        save_registration tabard_allowlist design_bounds joined_builder presence_builder
        rank_builder leader_builder left_builder disband_builder event_registration
        event_allowlist event_opcode presence_calls removed_member_copy legacy_info legacy_event
        money_client_registration money_server_registration money_gate command_builder
        command_registration command_allowlist command_opcode invite_reader
        invite_registration invite_length_width invite_opcode
        tracking_count_bits_21 tracking_count_bits_24 tracking_ids_big_endian
        tracking_remove_count_limit tracking_allow_truncated tracking_allow_trailing
        tracking_wrong_opcode_value tracking_remove_defc tracking_wrong_handler
        tracking_wrong_reference_state tracking_add_persistence)
    add_test(NAME mop_guild_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_guild_packets_source_test.cmake)
    set_tests_properties(mop_guild_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_party_update_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_party_update_packets_source_test.cmake)

foreach(mutation IN ITEMS normal_builder removal_builder targeted_request inbound_registration outbound_registration)
    add_test(NAME mop_party_update_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_party_update_packets_source_test.cmake)
    set_tests_properties(mop_party_update_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_enter_world_gate_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_enter_world_gate_source_test.cmake)

add_test(NAME mop_staging_ui_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_staging_ui_packets_source_test.cmake)

foreach(mutation IN ITEMS
        bank_mask bank_bytes bank_sender mailbox_body server_message_body
        received_mail_body ticket_body calendar_scalar_order calendar_reset_field
        calendar_mask calendar_bytes calendar_instance_guid calendar_sender
        staging_gate staging_registration staging_reference)
    add_test(NAME mop_staging_ui_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_staging_ui_packets_source_test.cmake)
    set_tests_properties(mop_staging_ui_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

foreach(mutation IN ITEMS converted_packet transfer_registration
        update_gate update_registration update_bypass
        destroy_gate destroy_registration destroy_bypass)
    add_test(NAME mop_enter_world_gate_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_enter_world_gate_source_test.cmake)
    set_tests_properties(mop_enter_world_gate_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_transfer_abort_packets_test mop_transfer_abort_packets_test.cpp)
target_include_directories(mop_transfer_abort_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_transfer_abort_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_transfer_abort_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_transfer_abort_packets_test PRIVATE shared)
add_test(NAME mop_transfer_abort_packets COMMAND mop_transfer_abort_packets_test)

add_test(NAME mop_transfer_abort_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_transfer_abort_packets_source_test.cmake)

foreach(mutation IN ITEMS reason_mapping argument_bit reason_width registration gate sender_builder)
    add_test(NAME mop_transfer_abort_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_transfer_abort_packets_source_test.cmake)
    set_tests_properties(mop_transfer_abort_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_account_data_lifecycle_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_account_data_lifecycle_source_test.cmake)

foreach(mutation IN ITEMS gate exact_frame clear_body inflate_size ack_retirement sender_bypass)
    add_test(NAME mop_account_data_lifecycle_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_account_data_lifecycle_source_test.cmake)
    set_tests_properties(mop_account_data_lifecycle_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_gm_ticket_packets_test mop_gm_ticket_packets_test.cpp)
target_include_directories(mop_gm_ticket_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_gm_ticket_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_gm_ticket_packets_test PRIVATE game)
add_test(NAME mop_gm_ticket_packets COMMAND mop_gm_ticket_packets_test)

add_executable(mop_gossip_packets_test mop_gossip_packets_test.cpp)
target_include_directories(mop_gossip_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_gossip_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_gossip_packets_test PRIVATE game)
add_test(NAME mop_gossip_packets COMMAND mop_gossip_packets_test)

add_executable(mop_vendor_packets_test mop_vendor_packets_test.cpp)
target_include_directories(mop_vendor_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_vendor_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_vendor_packets_test PRIVATE game)
add_test(NAME mop_vendor_packets COMMAND mop_vendor_packets_test)

add_executable(mop_inventory_packets_test mop_inventory_packets_test.cpp)
target_include_directories(mop_inventory_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_inventory_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_inventory_packets_test PRIVATE game)
add_test(NAME mop_inventory_packets COMMAND mop_inventory_packets_test)

add_test(NAME mop_inventory_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_inventory_packets_source_test.cmake)

foreach(mutation IN ITEMS swap_inv_handler swap_handler autoequip_handler autostore_handler split_handler destroy_handler
        registration_swap_inv registration_swap registration_autoequip registration_autostore registration_split registration_destroy
        opcode_values legacy_reader failure_sender failure_registration failure_admission
        failure_opcode_value failure_legacy_sender item_push_sender item_push_registration
        item_push_admission item_push_opcode_value)
    add_test(NAME mop_inventory_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_inventory_packets_source_test.cmake)
    set_tests_properties(mop_inventory_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_loot_packets_test mop_loot_packets_test.cpp)
target_include_directories(mop_loot_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_loot_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_loot_packets_test PRIVATE game)
add_test(NAME mop_loot_packets COMMAND mop_loot_packets_test)

add_test(NAME mop_loot_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_loot_packets_source_test.cmake)

foreach(mutation IN ITEMS open_parser take_parser money_parser release_parser
        response_sender release_sender removed_sender money_sender situ_model
        registration_cmsg registration_smsg admission opcode_values unknown_name
        legacy_reader legacy_sender
        group_vote_parser group_start_sender group_update_sender
        group_winner_sender group_passed_sender group_need_vote
        group_item_model
        group_registration_cmsg group_registration_smsg group_admission)
    add_test(NAME mop_loot_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_loot_packets_source_test.cmake)
    set_tests_properties(mop_loot_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_gameobject_interaction_packets_test
    mop_gameobject_interaction_packets_test.cpp)
target_include_directories(mop_gameobject_interaction_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_gameobject_interaction_packets_test
    PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_gameobject_interaction_packets_test PRIVATE game)
add_test(NAME mop_gameobject_interaction_packets
    COMMAND mop_gameobject_interaction_packets_test)

add_test(NAME mop_gameobject_interaction_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gameobject_interaction_packets_source_test.cmake)

foreach(mutation IN ITEMS use_parser report_parser guid_type_guard
        custom_sender despawn_sender pagetext_sender
        client_registration report_registration server_registration
        sender_admission opcode_values legacy_reader
        page_query_parser page_query_builder page_query_client_registration
        page_query_server_registration page_query_admission
        page_query_reference_status page_query_duplicate_handler
        page_query_legacy_sender)
    add_test(NAME mop_gameobject_interaction_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gameobject_interaction_packets_source_test.cmake)
    set_tests_properties(
        mop_gameobject_interaction_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_world_quest_interaction_packets_test
    mop_world_quest_interaction_packets_test.cpp)
target_include_directories(mop_world_quest_interaction_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_world_quest_interaction_packets_test
    PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_world_quest_interaction_packets_test PRIVATE game)
add_test(NAME mop_world_quest_interaction_packets
    COMMAND mop_world_quest_interaction_packets_test)

add_test(NAME mop_world_quest_interaction_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_world_quest_interaction_packets_source_test.cmake)

foreach(mutation IN ITEMS area_parser area_leave no_corpse_builder
        quest_parser quest_builder client_registration server_registration
        sender_admission opcode_values
        npc_parser npc_mapping npc_loader
        npc_client_registration npc_server_registration npc_admission
        npc_legacy_sender npc_db_content npc_reference_status
        exploration_wire_order exploration_sender exploration_registration
        exploration_admission exploration_reference)
    add_test(NAME mop_world_quest_interaction_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_world_quest_interaction_packets_source_test.cmake)
    set_tests_properties(
        mop_world_quest_interaction_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_character_customize_packets_test mop_character_customize_packets_test.cpp)
target_include_directories(mop_character_customize_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_character_customize_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_character_customize_packets_test PRIVATE game)
add_test(NAME mop_character_customize_packets COMMAND mop_character_customize_packets_test)

add_test(NAME mop_character_customize_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_character_customize_packets_source_test.cmake)

foreach(mutation IN ITEMS request_reader success_builder error_builder client_registration
        server_registration whitelist client_value server_value)
    add_test(NAME mop_character_customize_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_character_customize_packets_source_test.cmake)
    set_tests_properties(mop_character_customize_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_battleground_timer_packets_test mop_battleground_timer_packets_test.cpp)
target_include_directories(mop_battleground_timer_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_battleground_timer_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_battleground_timer_packets_test PRIVATE game)
add_test(NAME mop_battleground_timer_packets COMMAND mop_battleground_timer_packets_test)

add_test(NAME mop_battleground_timer_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_battleground_timer_packets_source_test.cmake)

foreach(mutation IN ITEMS builder_order callsites request_reader client_registration
        server_registration whitelist client_value server_value)
    add_test(NAME mop_battleground_timer_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_battleground_timer_packets_source_test.cmake)
    set_tests_properties(mop_battleground_timer_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_vendor_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_vendor_packets_source_test.cmake)

foreach(mutation IN ITEMS request_reader response_builder client_registration server_registration whitelist
        sell_request_reader sell_response_builder sell_client_registration sell_server_registration sell_whitelist
        buy_request_reader buy_success_builder buy_failure_builder buy_client_registration
        buy_success_registration buy_failure_registration buy_success_whitelist buy_failure_whitelist
        buy_legacy_request buy_legacy_success buy_legacy_failure
        buyback_request_reader buyback_client_registration buyback_legacy_request)
    add_test(NAME mop_vendor_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_vendor_packets_source_test.cmake)
    set_tests_properties(mop_vendor_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_gossip_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gossip_packets_source_test.cmake)

foreach(mutation IN ITEMS request_reader response_builder
        select_reader select_validation source_binding source_clear poi_builder
        client_registration server_registration select_registration
        poi_registration whitelist poi_whitelist select_opcode poi_opcode)
    add_test(NAME mop_gossip_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gossip_packets_source_test.cmake)
    set_tests_properties(mop_gossip_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_gm_ticket_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gm_ticket_packets_source_test.cmake)

foreach(mutation IN ITEMS response_width callsite registration whitelist opcode_value
        get_client_registration get_server_registration system_client_registration
        system_server_registration get_whitelist system_whitelist
        case_client_registration case_server_registration case_whitelist
        create_parser_call create_registration create_message_width
        create_attachment_bounds vulnerable_response_registration
        vulnerable_response_whitelist vulnerable_response_builder
        vulnerable_response_foreign_builder)
    add_test(NAME mop_gm_ticket_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_gm_ticket_packets_source_test.cmake)
    set_tests_properties(mop_gm_ticket_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_movement_packets_test
    mop_movement_packets_test.cpp
    ${CMAKE_SOURCE_DIR}/src/game/movement/MovementInfo.cpp)
target_include_directories(mop_movement_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_movement_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_movement_packets_test PRIVATE game)
add_test(NAME mop_movement_packets COMMAND mop_movement_packets_test)

add_test(NAME mop_movement_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_movement_source_test.cmake)

foreach(mutation IN ITEMS root_then_clear_flags drop_back_speed_snare_calls speed_correction_is_noop back_speed_early_return drop_nonfinite_speed_guard nonfinite_guard_after_bookkeeping inbound_sequence movement_gap_sequence player_move_sequence allocation_bound
        force_swim_sequence force_swim_flags2_width flags2_width inbound_registration movement_gap_registration force_swim_registration
        force_swim_handler_order output_metadata heartbeat_framing mover_guid_validation
        spline_state_builder spline_state_registration monster_move_writer monster_move_registration
        monster_move_integration monster_move_gate teleport_ack_registration
        flight_sequence_order flight_flags2_width flight_set_fly_selector
        flight_registration_atomic flight_invent_stop_descend)
    add_test(NAME mop_movement_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_movement_source_test.cmake)
    set_tests_properties(mop_movement_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_taxi_status_packets_test mop_taxi_status_packets_test.cpp)
target_include_directories(mop_taxi_status_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_taxi_status_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_taxi_status_packets_test PRIVATE game)
add_test(NAME mop_taxi_status_packets COMMAND mop_taxi_status_packets_test)

add_test(NAME mop_taxi_status_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_taxi_status_packets_source_test.cmake)

foreach(mutation IN ITEMS
        request_mask_adj_0 request_mask_adj_1 request_mask_adj_2
        request_mask_adj_3 request_mask_adj_4 request_mask_adj_5 request_mask_adj_6
        request_byte_adj_0 request_byte_adj_1 request_byte_adj_2
        request_byte_adj_3 request_byte_adj_4 request_byte_adj_5 request_byte_adj_6
        reply_mask_adj_0 reply_mask_adj_1 reply_mask_adj_2
        reply_mask_adj_3 reply_mask_adj_4 reply_mask_adj_5
        reply_byte_adj_0 reply_byte_adj_1 reply_byte_adj_2
        reply_byte_adj_3 reply_byte_adj_4 reply_byte_adj_5 reply_byte_adj_6
        status_before_first_mask status_after_second_mask status_width
        request_xor request_bit_reset reply_xor unlearned_mapping request_opcode reply_opcode
        request_registration reply_registration reply_admission
        request_reference reply_reference handler_parser
        query_producer learn_producer spell_producer exact_tail authority_lookup)
    add_test(NAME mop_taxi_status_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_taxi_status_packets_source_test.cmake)
    set_tests_properties(mop_taxi_status_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_death_packets_test mop_death_packets_test.cpp)
target_include_directories(mop_death_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_death_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_death_packets_test PRIVATE game)
add_test(NAME mop_death_packets COMMAND mop_death_packets_test)

add_test(NAME mop_death_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_death_packets_source_test.cmake)

foreach(mutation IN ITEMS
        wire_order player_call misc_call registration whitelist opcode_value
        cemetery_wire cemetery_bound cemetery_query cemetery_serializer
        cemetery_registration cemetery_whitelist cemetery_opcode cemetery_team_filter
        durability_death_body durability_death_mirror_sender durability_death_unit_sender
        durability_death_registration durability_death_whitelist durability_death_reference
        reclaim_mask_order reclaim_byte_order reclaim_handler_parser
        resurrect_response_order resurrect_response_mask_order resurrect_response_byte_order
        resurrect_request_mask_order resurrect_request_byte_order resurrect_name_truncation
        resurrect_producer
        resurrect_producer_flags resurrect_producer_context resurrect_request_admission
        resurrect_accept_polarity reclaim_registration resurrect_registration
        return_graveyard_registration return_graveyard_corpse_guard
        nested_zone_parent_walk nested_zone_api_delegate death_recovery_reference
        spirit_confirm_mask_order spirit_confirm_byte_order spirit_confirm_raw_guid
        spirit_confirm_producer spirit_confirm_admission
        spirit_activate_mask_order spirit_activate_byte_order spirit_activate_raw_guid
        spirit_activate_malformed_guard spirit_activate_trailing_guard
        spirit_activate_handler_parser spirit_activate_player_state_guard spirit_activate_npc_flag
        spirit_activate_registration
        spirit_healer_opcode_values spirit_healer_reference)
    add_test(NAME mop_death_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_death_packets_source_test.cmake)
    set_tests_properties(mop_death_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_quest_share_packets_test mop_quest_share_packets_test.cpp)
target_include_directories(mop_quest_share_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_quest_share_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_quest_share_packets_test PRIVATE game)
add_test(NAME mop_quest_share_packets COMMAND mop_quest_share_packets_test)

add_executable(mop_quest_completed_packets_test mop_quest_completed_packets_test.cpp)
target_include_directories(mop_quest_completed_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_quest_completed_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_quest_completed_packets_test PRIVATE game)
add_test(NAME mop_quest_completed_packets COMMAND mop_quest_completed_packets_test)

add_executable(mop_quest_log_packets_test mop_quest_log_packets_test.cpp)
target_include_directories(mop_quest_log_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_quest_log_packets_test
    PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_quest_log_packets_test PRIVATE game)
add_test(NAME mop_quest_log_packets COMMAND mop_quest_log_packets_test)

add_executable(mop_questgiver_acquisition_packets_test
    mop_questgiver_acquisition_packets_test.cpp)
target_include_directories(mop_questgiver_acquisition_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_questgiver_acquisition_packets_test
    PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_questgiver_acquisition_packets_test PRIVATE game)
add_test(NAME mop_questgiver_acquisition_packets
    COMMAND mop_questgiver_acquisition_packets_test)

add_test(NAME mop_questgiver_acquisition_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_questgiver_acquisition_packets_source_test.cmake)

add_executable(mop_questgiver_turnin_packets_test
    mop_questgiver_turnin_packets_test.cpp)
target_include_directories(mop_questgiver_turnin_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_questgiver_turnin_packets_test
    PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_questgiver_turnin_packets_test PRIVATE game)
add_test(NAME mop_questgiver_turnin_packets
    COMMAND mop_questgiver_turnin_packets_test)

add_test(NAME mop_questgiver_turnin_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_questgiver_turnin_packets_source_test.cmake)

foreach(mutation IN ITEMS complete_parser request_reward_parser
        choose_reward_parser reward_item_resolution request_items_builder
        offer_reward_builder reward_summary_builder quest_update_builder
        complete_registration request_reward_registration
        choose_reward_registration response_registration response_admission
        reference_status)
    add_test(NAME mop_questgiver_turnin_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_questgiver_turnin_packets_source_test.cmake)
    set_tests_properties(
        mop_questgiver_turnin_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

foreach(mutation IN ITEMS details_emote_break offer_reward_emote_break details_emote_zero_skip hello_parser query_parser accept_parser
        quest_cache_query_parser quest_cache_response_builder
        quest_cache_absent_response query_interaction_gate
        list_builder details_builder
        details_auto_launch_zero gossip_complete_empty_body
        hello_registration accept_registration quest_cache_registration
        gossip_complete_registration
        list_admission quest_cache_admission details_admission
        gossip_complete_admission opcode_value reference_status
        quest_cache_reference_status gossip_complete_reference_status)
    add_test(NAME mop_questgiver_acquisition_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_questgiver_acquisition_packets_source_test.cmake)
    set_tests_properties(
        mop_questgiver_acquisition_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_quest_completed_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_completed_source_test.cmake)

add_test(NAME mop_quest_log_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_log_packets_source_test.cmake)

foreach(mutation IN ITEMS parser_declaration parser_implementation
        handler_parser early_return slot_range registration opcode_value
        disproved_alias)
    add_test(NAME mop_quest_log_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_log_packets_source_test.cmake)
    set_tests_properties(
        mop_quest_log_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_quest_share_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_share_packets_source_test.cmake)

foreach(mutation IN ITEMS confirm_builder result_builder result_reader sharer_validation result_range
        push_registration confirm_registration result_registration outbound_registration
        confirm_whitelist result_whitelist legacy_sender opcode_values result_values)
    add_test(NAME mop_quest_share_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_share_packets_source_test.cmake)
    set_tests_properties(mop_quest_share_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_arena_team_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_arena_team_legacy_source_test.cmake)

foreach(mutation IN ITEMS legacy_sender legacy_handler backend)
    add_test(NAME mop_arena_team_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_arena_team_legacy_source_test.cmake)
    set_tests_properties(mop_arena_team_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_petition_legacy_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_petition_legacy_source_test.cmake)

foreach(mutation IN ITEMS legacy_sender legacy_handler)
    add_test(NAME mop_petition_legacy_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_petition_legacy_source_test.cmake)
    set_tests_properties(mop_petition_legacy_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_spell_cast_packets_test mop_spell_cast_packets_test.cpp)
target_include_directories(mop_spell_cast_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_spell_cast_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_spell_cast_packets_test PRIVATE game)
add_test(NAME mop_spell_cast_packets COMMAND mop_spell_cast_packets_test)

add_executable(mop_use_item_packets_test mop_use_item_packets_test.cpp)
target_include_directories(mop_use_item_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_use_item_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_use_item_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_use_item_packets_test PRIVATE game shared)
add_test(NAME mop_use_item_packets COMMAND mop_use_item_packets_test)
add_test(NAME mop_use_item_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_use_item_packets_source_test.cmake)
foreach(mutation IN ITEMS use_item_opcode_value use_item_registration
        use_item_reference_state use_item_handler_reader_route use_item_legacy_prefix
        use_item_swap_slot_bag use_item_header_order use_item_target_mask_width
        use_item_cast_flags_width_order use_item_item_guid_mask_order
        use_item_item_guid_split use_item_target_guid_mask_order
        use_item_target_guid_byte_order use_item_item_target_guid_orders
        use_item_source_guid_and_xyz_order use_item_destination_guid_and_xyz_order
        use_item_research_count_width use_item_research_payload_order
        use_item_movement_force_width_and_bound use_item_transport_guid3_mask
        use_item_movement_byte_order use_item_tail_scalar_order
        use_item_target_string_bound use_item_exact_tail use_item_parse_before_binding
        use_item_target_initialization use_item_cast_item_arguments
        use_item_self_mover_guard use_item_item_guid_match use_item_glyph_bound)
    add_test(NAME ${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_use_item_packets_source_test.cmake)
    set_tests_properties(${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()
if(WIN32)
    get_filename_component(MOP_SPELL_CAST_MYSQL_RUNTIME_DIR "${MySQL_LIBRARY}" DIRECTORY)
    set_tests_properties(mop_spell_cast_packets mop_use_item_packets PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MOP_SPELL_CAST_MYSQL_RUNTIME_DIR}")
endif()

add_test(NAME mop_spell_cast_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_spell_cast_packets_source_test.cmake)

foreach(mutation IN ITEMS reader registration target_initializer target_mapping movement_count_width
        movement_count_bound movement_flag_order missile_guard elevation_guard full_consumption string_bound legacy_prefix
        cast_failed_sender cast_failed_registration pet_cast_failed_registration cast_failed_gate
        pet_cast_failed_gate cast_result_translation pet_presence_order debug_cast_failed_sender
        spell_start_sender spell_start_registration spell_start_gate spell_start_target_mask_width
        spell_start_caster_mask spell_start_string_bound
        spell_go_sender spell_go_registration spell_go_gate spell_go_miss_reason_width
        spell_go_hit_mask spell_go_string_bound spell_go_trajectory
        category_request_registration category_response_registration category_response_gate
        category_request_reader category_response_layout category_aura_aggregation
        cooldown_mask_order cooldown_presence_bit cooldown_count_width cooldown_byte_order
        cooldown_player_sender cooldown_item_sender cooldown_pet_sender cooldown_registration
        cooldown_gate cooldown_reference
        clear_cooldowns_mask_order clear_cooldowns_count_width clear_cooldowns_byte_order
        clear_cooldowns_player_sender clear_cooldowns_manager_sender clear_cooldowns_registration
        clear_cooldowns_gate clear_cooldowns_reference
        spellbook_wire_order spellbook_sender spellbook_registration spellbook_gate spellbook_reference
        pet_spellbook_builder pet_spellbook_sender pet_spellbook_registration pet_spellbook_gate
        pet_spellbook_reference
        cooldown_event_mask_order cooldown_event_byte_order cooldown_event_sender
        cooldown_event_registration cooldown_event_gate cooldown_event_reference
        item_cooldown_wire_order item_cooldown_sender item_cooldown_registration
        item_cooldown_gate item_cooldown_reference
        clear_target_mask_order clear_target_byte_order clear_target_sender
        clear_target_registration clear_target_gate clear_target_reference
        channel_start_mask_order channel_update_byte_order channel_sender
        channel_registration channel_gate channel_reference)
    add_test(NAME mop_spell_cast_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_spell_cast_packets_source_test.cmake)
    set_tests_properties(mop_spell_cast_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_battlefield_status_packets_test mop_battlefield_status_packets_test.cpp)
target_include_directories(mop_battlefield_status_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_battlefield_status_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_battlefield_status_packets_test PRIVATE game)
add_test(NAME mop_battlefield_status_packets COMMAND mop_battlefield_status_packets_test)
if(WIN32)
    get_filename_component(MOP_BATTLEFIELD_STATUS_MYSQL_RUNTIME_DIR "${MySQL_LIBRARY}" DIRECTORY)
    set_tests_properties(mop_battlefield_status_packets PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MOP_BATTLEFIELD_STATUS_MYSQL_RUNTIME_DIR}")
endif()
add_test(NAME mop_battlefield_status_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_battlefield_status_packets_source_test.cmake)

add_executable(mop_battle_pet_packets_test mop_battle_pet_packets_test.cpp)
target_include_directories(mop_battle_pet_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_battle_pet_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_battle_pet_packets_test PRIVATE game)
add_test(NAME mop_battle_pet_packets COMMAND mop_battle_pet_packets_test)

add_test(NAME mop_battle_pet_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_battle_pet_packets_source_test.cmake)

foreach(mutation IN ITEMS pet_count_width writable slot_count_width flush trap_level
        handler send client_registration server_registration whitelist client_value server_value)
    add_test(NAME mop_battle_pet_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_battle_pet_packets_source_test.cmake)
    set_tests_properties(mop_battle_pet_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_inspect_packets_test mop_inspect_packets_test.cpp)
target_include_directories(mop_inspect_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_inspect_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_inspect_packets_test PRIVATE game)
add_test(NAME mop_inspect_packets COMMAND mop_inspect_packets_test)
if(WIN32)
    get_filename_component(MOP_INSPECT_MYSQL_RUNTIME_DIR "${MySQL_LIBRARY}" DIRECTORY)
    set_tests_properties(mop_inspect_packets PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MOP_INSPECT_MYSQL_RUNTIME_DIR}")
endif()

add_test(NAME mop_inspect_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_inspect_packets_source_test.cmake)

foreach(mutation IN ITEMS request_reader response_builder client_registration
        server_registration whitelist request_value response_value
        request_reference response_reference)
    add_test(NAME mop_inspect_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_inspect_packets_source_test.cmake)
    set_tests_properties(mop_inspect_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_fishing_packets_test mop_fishing_packets_test.cpp)
target_include_directories(mop_fishing_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_fishing_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_fishing_packets_test PRIVATE shared)
add_test(NAME mop_fishing_packets COMMAND mop_fishing_packets_test)

add_test(NAME mop_fishing_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_fishing_packets_source_test.cmake)

foreach(mutation IN ITEMS escaped_sender not_hooked_senders escaped_registration
        not_hooked_registration escaped_allowlist not_hooked_allowlist
        escaped_opcode not_hooked_opcode)
    add_test(NAME mop_fishing_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_fishing_packets_source_test.cmake)
    set_tests_properties(mop_fishing_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_barber_shop_packets_test mop_barber_shop_packets_test.cpp)
target_include_directories(mop_barber_shop_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_barber_shop_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_barber_shop_packets_test PRIVATE shared)
add_test(NAME mop_barber_shop_packets COMMAND mop_barber_shop_packets_test)

add_test(NAME mop_barber_shop_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_barber_shop_packets_source_test.cmake)

foreach(mutation IN ITEMS sender registration allowlist opcode reference semantic_comment
        request_registration result_registration result_allowlist request_order request_size
        request_skin_lookup request_hair_lookup request_facial_lookup result_success)
    add_test(NAME mop_barber_shop_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_barber_shop_packets_source_test.cmake)
    set_tests_properties(mop_barber_shop_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_quest_failure_packets_test mop_quest_failure_packets_test.cpp)
target_include_directories(mop_quest_failure_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_quest_failure_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_quest_failure_packets_test PRIVATE shared)
add_test(NAME mop_quest_failure_packets COMMAND mop_quest_failure_packets_test)

add_test(NAME mop_quest_failure_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_failure_packets_source_test.cmake)

foreach(mutation IN ITEMS invalid_body failed_order log_full_body timer_body opcodes reference
        registration_SMSG_QUESTGIVER_QUEST_INVALID
        registration_SMSG_QUESTGIVER_QUEST_FAILED
        registration_SMSG_QUESTLOG_FULL
        registration_SMSG_QUESTUPDATE_FAILEDTIMER
        allowlist_SMSG_QUESTGIVER_QUEST_INVALID
        allowlist_SMSG_QUESTGIVER_QUEST_FAILED
        allowlist_SMSG_QUESTLOG_FULL
        allowlist_SMSG_QUESTUPDATE_FAILEDTIMER)
    add_test(NAME mop_quest_failure_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_failure_packets_source_test.cmake)
    set_tests_properties(mop_quest_failure_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_quest_progress_packets_test mop_quest_progress_packets_test.cpp)
target_include_directories(mop_quest_progress_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_quest_progress_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_quest_progress_packets_test PRIVATE game)
add_test(NAME mop_quest_progress_packets COMMAND mop_quest_progress_packets_test)

add_test(NAME mop_quest_progress_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_progress_packets_source_test.cmake)

foreach(mutation IN ITEMS field_order gameobject_type interact_type guid_mask guid_bytes sender
        kill_context gameobject_context talk_context registration allowlist opcode reference)
    add_test(NAME mop_quest_progress_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_quest_progress_packets_source_test.cmake)
    set_tests_properties(mop_quest_progress_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_player_progression_packets_test mop_player_progression_packets_test.cpp)
target_include_directories(mop_player_progression_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_player_progression_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_player_progression_packets_test PRIVATE game)
add_test(NAME mop_player_progression_packets COMMAND mop_player_progression_packets_test)

add_executable(mop_character_pane_packets_test mop_character_pane_packets_test.cpp)
target_include_directories(mop_character_pane_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_character_pane_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_character_pane_packets_test PRIVATE game)
add_test(NAME mop_character_pane_packets COMMAND mop_character_pane_packets_test)

add_test(NAME mop_character_pane_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_character_pane_packets_source_test.cmake)

foreach(mutation IN ITEMS
        standing_order standing_opcode standing_sender
        visible_body visible_opcode visible_sender
        currency_order currency_opcode currency_sender
        standing_registration visible_registration currency_registration
        standing_admission visible_admission currency_admission
        standing_reference visible_reference currency_reference
        title_opcode title_sender
        pvp_scalar_order pvp_mask pvp_bytes pvp_opcode pvp_sender
        inebriation_scalar_order inebriation_mask inebriation_byte3
        inebriation_bytes inebriation_opcode inebriation_sender
        title_earned_registration title_lost_registration pvp_registration inebriation_registration
        title_earned_admission title_lost_admission pvp_admission inebriation_admission
        title_earned_reference title_lost_reference pvp_reference inebriation_reference)
    add_test(NAME mop_character_pane_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_character_pane_packets_source_test.cmake)
    set_tests_properties(mop_character_pane_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_player_progression_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_player_progression_packets_source_test.cmake)

foreach(mutation IN ITEMS wire_order xp_wire_order sender xp_sender registration admission reference
        xp_registration xp_admission xp_reference)
    add_test(NAME mop_player_progression_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_player_progression_packets_source_test.cmake)
    set_tests_properties(mop_player_progression_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_world_state_packets_test mop_world_state_packets_test.cpp)
target_include_directories(mop_world_state_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_world_state_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_world_state_packets_test PRIVATE game)
add_test(NAME mop_world_state_packets COMMAND mop_world_state_packets_test)
if(WIN32)
    get_filename_component(MOP_WORLD_STATE_MYSQL_RUNTIME_DIR "${MySQL_LIBRARY}" DIRECTORY)
    set_tests_properties(mop_world_state_packets PROPERTIES
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${MOP_WORLD_STATE_MYSQL_RUNTIME_DIR}")
endif()

add_test(NAME mop_world_state_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_world_state_packets_source_test.cmake)

foreach(mutation IN ITEMS bit_order field_order player_sender battleground_sender
        registration allowlist opcode reference)
    add_test(NAME mop_world_state_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_world_state_packets_source_test.cmake)
    set_tests_properties(mop_world_state_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_ai_reaction_packets_test mop_ai_reaction_packets_test.cpp)
target_include_directories(mop_ai_reaction_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_ai_reaction_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_ai_reaction_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_ai_reaction_packets_test PRIVATE shared)
add_test(NAME mop_ai_reaction_packets COMMAND mop_ai_reaction_packets_test)

add_test(NAME mop_ai_reaction_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ai_reaction_packets_source_test.cmake)

foreach(mutation IN ITEMS mask_order byte_order reaction_position creature_sender pet_sender
        registration allowlist opcode reference)
    add_test(NAME mop_ai_reaction_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_ai_reaction_packets_source_test.cmake)
    set_tests_properties(mop_ai_reaction_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_power_update_packets_test mop_power_update_packets_test.cpp)
target_include_directories(mop_power_update_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_power_update_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_power_update_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_power_update_packets_test PRIVATE shared)
add_test(NAME mop_power_update_packets COMMAND mop_power_update_packets_test)

add_test(NAME mop_power_update_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_power_update_packets_source_test.cmake)

foreach(mutation IN ITEMS mask_order count_width byte_order primary_sender switch_sender
        switch_sender_gate reported_value druid_form_exit_guard registration allowlist reference)
    add_test(NAME mop_power_update_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_power_update_packets_source_test.cmake)
    set_tests_properties(mop_power_update_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_sound_packets_test mop_sound_packets_test.cpp)
target_include_directories(mop_sound_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_sound_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_sound_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_sound_packets_test PRIVATE shared)
add_test(NAME mop_sound_packets COMMAND mop_sound_packets_test)

add_test(NAME mop_sound_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_sound_packets_source_test.cmake)

foreach(mutation IN ITEMS direct_mask direct_bytes object_mask object_bytes
        worldobject_sound_sender map_sound_sender battleground_sound_sender
        object_sound_sender music_sender registration_sound registration_object
        registration_music allowlist_sound allowlist_object allowlist_music
        opcode_sound opcode_object opcode_music reference_sound reference_object
        reference_music)
    add_test(NAME mop_sound_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_sound_packets_source_test.cmake)
    set_tests_properties(mop_sound_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_pet_action_sound_packets_test mop_pet_action_sound_packets_test.cpp)
target_include_directories(mop_pet_action_sound_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_pet_action_sound_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_pet_action_sound_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_pet_action_sound_packets_test PRIVATE shared)
add_test(NAME mop_pet_action_sound_packets COMMAND mop_pet_action_sound_packets_test)

add_test(NAME mop_pet_action_sound_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_action_sound_packets_source_test.cmake)

foreach(mutation IN ITEMS mask_order byte_order action_position sender registration
        allowlist opcode reference)
    add_test(NAME mop_pet_action_sound_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_action_sound_packets_source_test.cmake)
    set_tests_properties(mop_pet_action_sound_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_pet_action_feedback_packets_test mop_pet_action_feedback_packets_test.cpp)
target_include_directories(mop_pet_action_feedback_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_pet_action_feedback_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_pet_action_feedback_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_pet_action_feedback_packets_test PRIVATE shared)
add_test(NAME mop_pet_action_feedback_packets COMMAND mop_pet_action_feedback_packets_test)

add_test(NAME mop_pet_action_feedback_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_action_feedback_packets_source_test.cmake)

foreach(mutation IN ITEMS presence_bit field_order optional_field sender registration
        allowlist opcode reference)
    add_test(NAME mop_pet_action_feedback_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_action_feedback_packets_source_test.cmake)
    set_tests_properties(mop_pet_action_feedback_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_pet_mode_packets_test mop_pet_mode_packets_test.cpp)
target_include_directories(mop_pet_mode_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_pet_mode_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_pet_mode_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_pet_mode_packets_test PRIVATE shared)
add_test(NAME mop_pet_mode_packets COMMAND mop_pet_mode_packets_test)

add_test(NAME mop_pet_mode_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_mode_packets_source_test.cmake)

foreach(mutation IN ITEMS mask_order byte_order mode_position set_sender apply_sender
        registration allowlist opcode reference)
    add_test(NAME mop_pet_mode_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_mode_packets_source_test.cmake)
    set_tests_properties(mop_pet_mode_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_pet_spells_packets_test mop_pet_spells_packets_test.cpp)
target_include_directories(mop_pet_spells_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    $<TARGET_PROPERTY:game,INTERFACE_INCLUDE_DIRECTORIES>)
set_target_properties(mop_pet_spells_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_compile_definitions(mop_pet_spells_packets_test PRIVATE
    $<TARGET_PROPERTY:game,INTERFACE_COMPILE_DEFINITIONS>)
target_link_libraries(mop_pet_spells_packets_test PRIVATE shared)
add_test(NAME mop_pet_spells_packets COMMAND mop_pet_spells_packets_test)

add_test(NAME mop_pet_spells_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_spells_packets_source_test.cmake)

foreach(mutation IN ITEMS spell_count_width cooldown_count_width
        cooldown_field_order player_sender other_senders clear_sender
        registration allowlist opcode reference)
    add_test(NAME mop_pet_spells_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_pet_spells_packets_source_test.cmake)
    set_tests_properties(mop_pet_spells_packets_source_mutation_${mutation} PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_far_sight_packets_test mop_far_sight_packets_test.cpp)
target_include_directories(mop_far_sight_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_far_sight_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_far_sight_packets_test PRIVATE shared)
add_test(NAME mop_far_sight_packets COMMAND mop_far_sight_packets_test)

add_executable(mop_showing_helm_packets_test mop_showing_helm_packets_test.cpp)
target_include_directories(mop_showing_helm_packets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared)
set_target_properties(mop_showing_helm_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_showing_helm_packets_test PRIVATE shared)
add_test(NAME mop_showing_helm_packets COMMAND mop_showing_helm_packets_test)

add_test(NAME mop_movement_pair_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_movement_pair_source_test.cmake)

foreach(mutation IN ITEMS drop_observer_half drop_mover_half observer_half_to_self unadmitted_spline drop_waterwalk_observer unadmitted_water_mover feather_mover_to_observers unadmitted_feather_mover constant_counter counter_does_not_advance speed_constant_counter builder_discards_counter counter_getter_not_advancer knockback_constant_counter collision_height_constant_counter drop_hover_observer unadmitted_hover_mover root_broadcasts_mover unadmitted_root_observer player_setter_skips_server_state gravity_sense_disagrees)
    add_test(NAME mop_movement_pair_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_movement_pair_source_test.cmake)
    set_tests_properties(mop_movement_pair_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_forced_movement_ack_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_forced_movement_ack_source_test.cmake)

foreach(mutation IN ITEMS
        root_prefix_order unroot_prefix_order root_bit_order unroot_bit_order
        root_guid_byte_order unroot_guid_byte_order root_transport_order unroot_transport_order
        root_fall_order unroot_fall_order root_flags_order unroot_flags_order flags2_13_width
        root_sequence_route unroot_sequence_route root_registration unroot_registration
        root_handler_parser unroot_handler_parser root_hostile_force_count unroot_hostile_force_count
        water_ack_prefix_order water_ack_bit_order water_ack_flags_order water_ack_flags2_width
        water_ack_guid_byte_order water_ack_transport_order water_ack_transport_position_orientation_order
        water_ack_fall_order water_ack_force_elements
        water_ack_sequence_route water_ack_handler_parser water_ack_mover_validation
        water_ack_forbid_state_mutation water_ack_registration water_pair_metadata water_reference_atomic
        active_mover_verify exact_tail_consumption movement_counter_storage no_relocation
        no_flag_assignment no_aura_mutation no_observer_broadcast allocation_before_resize
        paired_smsg_metadata reference_triplet_atomic)
    add_test(NAME mop_forced_movement_ack_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_forced_movement_ack_source_test.cmake)
    set_tests_properties(mop_forced_movement_ack_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_knockback_packets_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_knockback_packets_source_test.cmake)

foreach(mutation IN ITEMS direct_scalar_order direct_vertical_sign direct_mask_order direct_byte_order
        direct_builder_bypass direct_producer_mapping direct_route update_sequence
        update_drop_force_count update_drop_force_ids update_flags2_width registration_direct
        registration_ack registration_update admission_direct admission_update reference_atomic
        observer_route skip_mover_validation)
    add_test(NAME mop_knockback_packets_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_knockback_packets_source_test.cmake)
    set_tests_properties(mop_knockback_packets_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_test(NAME mop_showing_helm_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_showing_helm_source_test.cmake)

foreach(mutation IN ITEMS helm_assigns_bit cloak_assigns_bit helm_reads_uint8 helm_toggles cloak_uses_helm_bit)
    add_test(NAME mop_showing_helm_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/mop_showing_helm_source_test.cmake)
    set_tests_properties(mop_showing_helm_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()

add_executable(mop_realm_suffix_test mop_realm_suffix_test.cpp)
target_include_directories(mop_realm_suffix_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/shared/Utilities)
set_target_properties(mop_realm_suffix_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
target_link_libraries(mop_realm_suffix_test PRIVATE shared)
add_test(NAME mop_realm_suffix COMMAND mop_realm_suffix_test)
