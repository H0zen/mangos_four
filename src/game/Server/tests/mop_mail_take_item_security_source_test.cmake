if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MailHandler.cpp" mail_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/MailTakeItemPolicy.h" policy_source)

string(FIND "${mail_source}" "void WorldSession::HandleMailTakeItem" handler_start)
string(FIND "${mail_source}" "void WorldSession::HandleMailTakeMoney" handler_end)
if(handler_start EQUAL -1 OR handler_end EQUAL -1 OR
   NOT handler_start LESS handler_end)
    message(FATAL_ERROR "missing mail-take-item handler implementation")
endif()
math(EXPR handler_length "${handler_end} - ${handler_start}")
string(SUBSTRING "${mail_source}" ${handler_start} ${handler_length} handler_source)

function(require_once content needle description)
    string(FIND "${content}" "${needle}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "missing ${description}")
    endif()
    math(EXPR next "${first} + 1")
    string(SUBSTRING "${content}" ${next} -1 tail)
    string(FIND "${tail}" "${needle}" second)
    if(NOT second EQUAL -1)
        message(FATAL_ERROR "duplicate ${description}")
    endif()
endfunction()

function(require_count content needle expected description)
    set(remainder "${content}")
    set(actual 0)
    while(TRUE)
        string(FIND "${remainder}" "${needle}" position)
        if(position EQUAL -1)
            break()
        endif()
        math(EXPR actual "${actual} + 1")
        math(EXPR next "${position} + 1")
        string(SUBSTRING "${remainder}" ${next} -1 remainder)
    endwhile()
    if(NOT actual EQUAL expected)
        message(FATAL_ERROR "invalid ${description}: expected ${expected}, got ${actual}")
    endif()
endfunction()

function(require_order content first_needle second_needle description)
    string(FIND "${content}" "${first_needle}" first_position)
    string(FIND "${content}" "${second_needle}" second_position)
    if(first_position EQUAL -1 OR second_position EQUAL -1 OR
       NOT first_position LESS second_position)
        message(FATAL_ERROR "invalid ${description}")
    endif()
endfunction()

function(forbid content needle description)
    string(FIND "${content}" "${needle}" position)
    if(NOT position EQUAL -1)
        message(FATAL_ERROR "forbidden ${description}")
    endif()
endfunction()

function(mutate_once variable needle replacement description)
    set(value "${${variable}}")
    require_once("${value}" "${needle}" "mutation target for ${description}")
    string(REPLACE "${needle}" "${replacement}" value "${value}")
    set(${variable} "${value}" PARENT_SCOPE)
endfunction()

if(MUTATION STREQUAL "drop_attachment_lookup")
    mutate_once(handler_source
        "MailTakeItemPolicy::FindAttachment(*m, itemId)"
        "NULL /* dropped attachment lookup */"
        "drop_attachment_lookup")
elseif(MUTATION STREQUAL "lookup_after_global_item")
    mutate_once(handler_source
        "MailItemInfo const* const attachment =\n        MailTakeItemPolicy::FindAttachment(*m, itemId);\n    Item* it = pl->GetMItem(itemId);"
        "Item* it = pl->GetMItem(itemId);\n    MailItemInfo const* const attachment =\n        MailTakeItemPolicy::FindAttachment(*m, itemId);"
        "lookup_after_global_item")
elseif(MUTATION STREQUAL "policy_after_can_store")
    mutate_once(handler_source
        "MailTakeItemPolicy::Evaluate"
        "CanStoreItem /* policy_after_can_store */ MailTakeItemPolicy::Evaluate"
        "policy_after_can_store")
elseif(MUTATION STREQUAL "drop_receiver_check")
    mutate_once(policy_source
        "mail.receiverGuid != playerGuid ||"
        "false ||"
        "drop_receiver_check")
elseif(MUTATION STREQUAL "allow_cod")
    mutate_once(policy_source
        "mail.COD != 0)"
        "false)"
        "allow_cod")
elseif(MUTATION STREQUAL "ignore_remove_result")
    mutate_once(handler_source
        "if (!m->RemoveItem(itemId))"
        "m->RemoveItem(itemId); if (false)"
        "ignore_remove_result")
elseif(MUTATION STREQUAL "restore_cod_send_mail")
    mutate_once(handler_source
        "m->state = MAIL_STATE_CHANGED;"
        "SendMailTo; m->state = MAIL_STATE_CHANGED;"
        "restore_cod_send_mail")
elseif(MUTATION STREQUAL "restore_cod_modify_money")
    mutate_once(handler_source
        "m->state = MAIL_STATE_CHANGED;"
        "ModifyMoney; m->state = MAIL_STATE_CHANGED;"
        "restore_cod_modify_money")
elseif(MUTATION STREQUAL "cod_reply_item_zero")
    mutate_once(handler_source
        "MAIL_ERR_INTERNAL_ERROR, 0, itemId, 0);\n        return;\n    }\n\n    ItemPosCountVec dest;"
        "MAIL_ERR_INTERNAL_ERROR, 0, 0, 0);\n        return;\n    }\n\n    ItemPosCountVec dest;"
        "cod_reply_item_zero")
elseif(MUTATION STREQUAL "cod_reply_count_one")
    mutate_once(handler_source
        "MAIL_ERR_INTERNAL_ERROR, 0, itemId, 0);\n        return;\n    }\n\n    ItemPosCountVec dest;"
        "MAIL_ERR_INTERNAL_ERROR, 0, itemId, 1);\n        return;\n    }\n\n    ItemPosCountVec dest;"
        "cod_reply_count_one")
elseif(MUTATION STREQUAL "remove_before_can_store")
    mutate_once(handler_source
        "ItemPosCountVec dest;"
        "m->RemoveItem(itemId);\n\n    ItemPosCountVec dest;"
        "remove_before_can_store")
elseif(MUTATION STREQUAL "success_before_commit")
    mutate_once(handler_source
        "CharacterDatabase.CommitTransaction();\n\n        pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_OK, 0, itemId, count);"
        "pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_OK, 0, itemId, count);\n\n        CharacterDatabase.CommitTransaction();"
        "success_before_commit")
elseif(DEFINED MUTATION)
    message(FATAL_ERROR "unknown mutation: ${MUTATION}")
endif()

require_once("${mail_source}" "#include \"MailTakeItemPolicy.h\""
    "mail-take-item policy include")
require_once("${handler_source}" "MailTakeItemPolicy::FindAttachment(*m, itemId)"
    "selected-mail attachment lookup")
require_once("${handler_source}" "pl->GetMItem(itemId)"
    "player-local item lookup")
require_order("${handler_source}"
    "MailTakeItemPolicy::FindAttachment(*m, itemId)"
    "pl->GetMItem(itemId)"
    "attachment lookup before player-local item lookup")

require_once("${handler_source}" "MailTakeItemPolicy::ResolvedItem const resolved = {"
    "resolved item view")
require_once("${handler_source}" "MailTakeItemPolicy::Evaluate"
    "mail-take-item policy evaluation")
require_order("${handler_source}"
    "MailTakeItemPolicy::ResolvedItem const resolved = {"
    "MailTakeItemPolicy::Evaluate"
    "resolved item view before policy evaluation")
require_order("${handler_source}"
    "MailTakeItemPolicy::Evaluate"
    "CanStoreItem"
    "policy evaluation before inventory admission")

require_once("${policy_source}" "mail.receiverGuid != playerGuid ||"
    "receiver authority check")
require_once("${policy_source}" "mail.COD != 0)"
    "zero-COD requirement")
require_count("${handler_source}"
    "MAIL_ERR_INTERNAL_ERROR, 0, itemId, 0);"
    2 "fail-closed item reply tuples")

require_once("${handler_source}" "if (!m->RemoveItem(itemId))"
    "checked selected-mail attachment removal")
require_order("${handler_source}"
    "if (!m->RemoveItem(itemId))"
    "removedItems.push_back"
    "checked attachment removal before removed-item recording")

forbid("${handler_source}" "SendMailTo" "COD payment mail")
forbid("${handler_source}" "ModifyMoney" "COD money deduction")

foreach(mutation_needle IN ITEMS
        "m->RemoveItem(itemId)"
        "pl->RemoveMItem"
        "pl->MoveItemToInventory"
        "CharacterDatabase.BeginTransaction")
    require_order("${handler_source}" "CanStoreItem" "${mutation_needle}"
        "inventory admission before ${mutation_needle}")
endforeach()

require_once("${handler_source}"
    "uint32 count = it->GetCount();"
    "saved pre-move stack count")
require_order("${handler_source}"
    "uint32 count = it->GetCount();"
    "pl->MoveItemToInventory"
    "stack count saved before inventory move")
require_once("${handler_source}"
    "pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_OK, 0, itemId, count);"
    "non-COD success tuple")
require_order("${handler_source}"
    "CharacterDatabase.CommitTransaction();"
    "pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_OK, 0, itemId, count);"
    "commit before success reply")
require_once("${handler_source}"
    "pl->SendMailResult(mailId, MAIL_ITEM_TAKEN, MAIL_ERR_EQUIP_ERROR, msg, itemId, 0);"
    "inventory failure tuple")
