if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/Object/GuildBank.cpp" guild_bank)
file(READ "${SOURCE_ROOT}/src/game/Object/MopGuildBankPackets.h" builder)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/GuildHandler.cpp" guild_handler)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" registry)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" world_session)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" reference)

set(original_all "${guild_bank}${builder}${guild_handler}${registry}${world_session}${reference}")

if(MUTATION STREQUAL "builder_route")
    string(REPLACE "SendGuildBankList(session, list)"
        "SendLegacyGuildBankList(session, list)" guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "query_boolean")
    string(REPLACE "DisplayGuildBankContent(this, TabId, sendAllSlots)"
        "DisplayGuildBankContent(this, TabId, true)" guild_handler "${guild_handler}")
elseif(MUTATION STREQUAL "tab_coalesce")
    string(REPLACE "DisplayGuildBankContent(session, TabId, true, true)"
        "DisplayGuildBankContent(session, TabId, false, true)" guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "money_recipient")
    string(REPLACE
        "GetMemberSlotWithdrawRem(player->GetGUIDLow(), 0)"
        "0" guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "metadata_without_view")
    string(REPLACE
        "!canViewTab && !withTabInfo"
        "!canViewTab" guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "zero_tab_metadata")
    string(REPLACE
        "GetPurchasedTabs() == 0 && withTabInfo && TabId == 0"
        "false" guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "single_delta_bounds")
    string(REPLACE
        "TabId >= GetPurchasedTabs() || slot1 < 0 || slot1 >= GUILD_BANK_MAX_SLOTS"
        "TabId >= GetPurchasedTabs()" guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "vector_delta_bounds")
    string(REPLACE
        "slot.Slot >= GUILD_BANK_MAX_SLOTS"
        "false" guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "zero_tab_full_update")
    string(REPLACE
        "list.fullUpdate = sendAllSlots && !emptyMetadata;"
        "list.fullUpdate = sendAllSlots;" guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "dynamic_flags_producer")
    string(REPLACE
        "record.dynamicFlags = 0;"
        "record.dynamicFlags = pItem->GetUInt32Value(OBJECT_FIELD_DYNAMICFLAGS);"
        guild_bank "${guild_bank}")
elseif(MUTATION STREQUAL "dynamic_flags_hold")
    string(REPLACE
        "SMSG_GUILD_BANK_LIST                            0x0B79  DORMANT"
        "SMSG_GUILD_BANK_LIST                            0x0B79  ACTIVE"
        reference "${reference}")
elseif(MUTATION STREQUAL "socket_order")
    string(REPLACE "staged << socket.index << socket.enchantmentId"
        "staged << socket.enchantmentId << socket.index" builder "${builder}")
elseif(MUTATION STREQUAL "tab_lengths")
    string(REPLACE
        "staged.WriteBits(uint32(tab.icon.size()), 9);"
        "staged.WriteBits(uint32(tab.icon.size()), 8);" builder "${builder}")
elseif(MUTATION STREQUAL "tab_data_order")
    string(REPLACE
        "staged << tab.index;"
        "staged.append(tab.icon.data(), tab.icon.size());"
        builder "${builder}")
elseif(MUTATION STREQUAL "locked_zero")
    string(REPLACE "staged.WriteBit(false); // Parsed but unused by the 18414 client."
        "staged.WriteBit(true); // mutation" builder "${builder}")
elseif(MUTATION STREQUAL "modifier_contract")
    string(REPLACE
        "staged << uint32(4) << uint32(0); // Present, no persisted modifiers."
        "staged << uint32(0); // mutation" builder "${builder}")
elseif(MUTATION STREQUAL "client_limits")
    string(REPLACE "MAX_TAB_NAME_BYTES = 64" "MAX_TAB_NAME_BYTES = 127"
        builder "${builder}")
elseif(MUTATION STREQUAL "activation_registry")
    string(APPEND registry
        "\nDefC(CMSG_GUILD_BANK_QUERY_TAB, mutation);\nDefS(SMSG_GUILD_BANK_LIST, mutation);\n")
elseif(MUTATION STREQUAL "activation_admission")
    string(APPEND world_session "\ncase SMSG_GUILD_BANK_LIST:\n")
elseif(MUTATION STREQUAL "activation_reference")
    string(REPLACE
        "CMSG_GUILD_BANKER_ACTIVATE                     0x0372  DORMANT"
        "CMSG_GUILD_BANKER_ACTIVATE                     0x0372  ACTIVE"
        reference "${reference}")
elseif(MUTATION STREQUAL "legacy_writer")
    string(APPEND guild_bank "\ndata.WriteBits(itemCount, 20);\n")
endif()

set(mutated_all "${guild_bank}${builder}${guild_handler}${registry}${world_session}${reference}")
if(DEFINED MUTATION AND NOT MUTATION STREQUAL "" AND original_all STREQUAL mutated_all)
    message(FATAL_ERROR "mutation setup failed: ${MUTATION}")
endif()

set(ws "[ \t\r\n]")

function(mop_strip_cxx_comments in_text out_var)
    string(REGEX REPLACE "/[*][^*]*[*]+/" "" stripped "${in_text}")
    string(REGEX REPLACE "//[^
]*" "" stripped "${stripped}")
    set(${out_var} "${stripped}" PARENT_SCOPE)
endfunction()

mop_strip_cxx_comments("${guild_bank}" guild_bank_code)
mop_strip_cxx_comments("${builder}" builder_code)
mop_strip_cxx_comments("${registry}" registry_code)
mop_strip_cxx_comments("${world_session}" session_code)

if(NOT guild_bank MATCHES "[#]include${ws}*\"MopGuildBankPackets[.]h\"")
    message(FATAL_ERROR "guild-bank producer delegation guard: builder include missing")
endif()
if(NOT guild_bank_code MATCHES
        "SendGuildBankList${ws}*[(]${ws}*session${ws}*,${ws}*list${ws}*[)]")
    message(FATAL_ERROR "guild-bank producer delegation guard: direct response bypasses builder")
endif()
string(REGEX MATCHALL
    "SendGuildBankList${ws}*[(]${ws}*player->GetSession[(][)]${ws}*,${ws}*list${ws}*[)]"
    recipient_sends "${guild_bank_code}")
list(LENGTH recipient_sends recipient_send_count)
if(NOT recipient_send_count EQUAL 3)
    message(FATAL_ERROR "guild-bank producer delegation guard: recipient producers do not share builder")
endif()
if(guild_bank_code MATCHES "WriteBits${ws}*[(]${ws}*itemCount${ws}*,${ws}*20${ws}*[)]" OR
        guild_bank_code MATCHES "WriteBits${ws}*[(]${ws}*slots[.]size[(][)]${ws}*,${ws}*20${ws}*[)]" OR
        guild_bank_code MATCHES "WriteBits${ws}*[(]${ws}*GetPurchasedTabs[(][)]${ws}*,${ws}*22${ws}*[)]" OR
        guild_bank_code MATCHES "WriteBits${ws}*[(]${ws}*enchCount${ws}*,${ws}*23${ws}*[)]" OR
        guild_bank_code MATCHES "AppendDisplayGuildBankSlot${ws}*[(][^)]*ByteBuffer")
    message(FATAL_ERROR "guild-bank legacy-writer guard: inherited count widths or buffer writer remain")
endif()
if(NOT guild_handler MATCHES
        "DisplayGuildBankContent[(]this,${ws}*TabId,${ws}*sendAllSlots[)]")
    message(FATAL_ERROR "guild-bank query boolean guard: sendAllSlots is not honored")
endif()
if(NOT guild_bank_code MATCHES
        "DisplayGuildBankContent${ws}*[(]${ws}*session${ws}*,${ws}*TabId${ws}*,${ws}*true${ws}*,${ws}*true${ws}*[)]")
    message(FATAL_ERROR "guild-bank metadata coalesce guard: metadata is not paired with full same-tab content")
endif()
if(NOT guild_bank_code MATCHES
        "GetMemberSlotWithdrawRem${ws}*[(]${ws}*player->GetGUIDLow[(][)]${ws}*,${ws}*0${ws}*[)]")
    message(FATAL_ERROR "guild-bank recipient allowance guard: money update is not individualized")
endif()
if(NOT guild_bank_code MATCHES
        "if${ws}*[(]${ws}*!canViewTab${ws}*&&${ws}*!withTabInfo${ws}*[)]" OR
        NOT guild_bank_code MATCHES
        "if${ws}*[(]${ws}*sendAllSlots${ws}*&&${ws}*canViewTab${ws}*[)]")
    message(FATAL_ERROR
        "guild-bank metadata rights guard: members without tab rights lose metadata or receive items")
endif()
if(NOT guild_bank_code MATCHES
        "emptyMetadata${ws}*=${ws}*GetPurchasedTabs[(][)]${ws}*==${ws}*0${ws}*&&${ws}*withTabInfo${ws}*&&${ws}*TabId${ws}*==${ws}*0" OR
        NOT guild_bank_code MATCHES
        "withdrawRemaining${ws}*=${ws}*emptyMetadata${ws}*[?]${ws}*0${ws}*:")
    message(FATAL_ERROR
        "guild-bank zero-tab guard: login metadata is dropped or exposes a nonexistent allowance")
endif()
if(NOT guild_bank_code MATCHES
        "TabId${ws}*>=${ws}*GetPurchasedTabs[(][)]${ws}*[|][|]${ws}*slot1${ws}*<${ws}*0${ws}*[|][|]${ws}*slot1${ws}*>=${ws}*GUILD_BANK_MAX_SLOTS")
    message(FATAL_ERROR "guild-bank single-delta bounds guard: tab or slot1 is unchecked")
endif()
if(NOT guild_bank_code MATCHES
        "slot[.]Slot${ws}*>=${ws}*GUILD_BANK_MAX_SLOTS")
    message(FATAL_ERROR "guild-bank vector-delta bounds guard: a slot is unchecked")
endif()
if(NOT guild_bank_code MATCHES
        "list[.]fullUpdate${ws}*=${ws}*sendAllSlots${ws}*&&${ws}*!emptyMetadata")
    message(FATAL_ERROR
        "guild-bank zero-tab full-update guard: empty metadata is not observed false")
endif()
if(NOT guild_bank_code MATCHES "record[.]dynamicFlags${ws}*=${ws}*0${ws}*;")
    message(FATAL_ERROR
        "guild-bank dynamic-flags producer guard: unsupported field is not truthful zero")
endif()
if(NOT builder MATCHES "WriteBits[(]uint32[(]list[.]tabs[.]size[(][)][)],${ws}*21[)]" OR
        NOT builder MATCHES "WriteBits[(]uint32[(]list[.]items[.]size[(][)][)],${ws}*18[)]" OR
        NOT builder MATCHES "WriteBits[(]uint32[(]item[.]socketEnchants[.]size[(][)][)],${ws}*21[)]")
    message(FATAL_ERROR "guild-bank wire grammar guard: 21/18/21-bit counts drifted")
endif()
if(NOT builder MATCHES "staged${ws}*<<${ws}*socket[.]index${ws}*<<${ws}*socket[.]enchantmentId")
    message(FATAL_ERROR "guild-bank socket-order guard: pair is not index then enchantment")
endif()
if(NOT builder MATCHES
        "WriteBits[(]uint32[(]tab[.]icon[.]size[(][)][)],${ws}*9[)]" OR
        NOT builder MATCHES
        "WriteBits[(]uint32[(]tab[.]name[.]size[(][)][)],${ws}*7[)]")
    message(FATAL_ERROR "guild-bank tab-length guard: 9/7-bit split drifted")
endif()
if(NOT builder_code MATCHES
        "staged${ws}*<<${ws}*tab[.]index${ws}*;${ws}*staged[.]append[(]tab[.]icon[.]data[(][)],${ws}*tab[.]icon[.]size[(][)][)]${ws}*;${ws}*staged[.]append[(]tab[.]name[.]data[(][)],${ws}*tab[.]name[.]size[(][)][)]")
    message(FATAL_ERROR "guild-bank tab-data guard: index-icon-name order drifted")
endif()
if(NOT builder MATCHES
        "staged[.]WriteBit[(]false[)];${ws}*// Parsed but unused by the 18414 client[.]")
    message(FATAL_ERROR "guild-bank locked-bit guard: client-unused field is not constant zero")
endif()
if(NOT builder MATCHES
        "staged${ws}*<<${ws}*uint32[(]4[)]${ws}*<<${ws}*uint32[(]0[)];${ws}*// Present, no persisted modifiers[.]" OR
        NOT builder_code MATCHES
        "else${ws}*[{]${ws}*staged${ws}*<<${ws}*uint32[(]0[)];${ws}*[}]")
    message(FATAL_ERROR "guild-bank modifier guard: present mask0 or absent length0 drifted")
endif()
if(NOT builder MATCHES "MAX_TAB_COUNT${ws}*=${ws}*8" OR
        NOT builder MATCHES "MAX_ITEM_COUNT${ws}*=${ws}*98" OR
        NOT builder MATCHES "MAX_SOCKET_ENCHANT_COUNT${ws}*=${ws}*3" OR
        NOT builder MATCHES "MAX_TAB_NAME_BYTES${ws}*=${ws}*64" OR
        NOT builder MATCHES "MAX_TAB_ICON_BYTES${ws}*=${ws}*255" OR
        NOT builder MATCHES "MAX_POST_CRYPT_PAYLOAD_BYTES${ws}*=${ws}*0x7FFFF")
    message(FATAL_ERROR "guild-bank client-bounds guard: fixed client or frame limit drifted")
endif()
if(registry_code MATCHES "DefC${ws}*[(]${ws}*CMSG_GUILD_BANKER_ACTIVATE" OR
        registry_code MATCHES "DefC${ws}*[(]${ws}*CMSG_GUILD_BANK_QUERY_TAB" OR
        registry_code MATCHES "DefS${ws}*[(]${ws}*SMSG_GUILD_BANK_LIST")
    message(FATAL_ERROR "guild-bank activation HOLD guard: registry escaped before banker-activate proof")
endif()
if(session_code MATCHES "case${ws}+SMSG_GUILD_BANK_LIST${ws}*:")
    message(FATAL_ERROR "guild-bank admission HOLD guard: reply escaped the in-world gate")
endif()
if(NOT reference MATCHES "SMSG_GUILD_BANK_LIST${ws}+0x0B79${ws}+DORMANT" OR
        NOT reference MATCHES "CMSG_GUILD_BANKER_ACTIVATE${ws}+0x0372${ws}+DORMANT" OR
        NOT reference MATCHES "CMSG_GUILD_BANK_QUERY_TAB${ws}+0x1372${ws}+DORMANT")
    message(FATAL_ERROR "guild-bank reference HOLD guard: dormant triplet drifted")
endif()
