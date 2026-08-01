if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/shared/Database/SqlOperations.cpp" sql_source)
file(READ "${SOURCE_ROOT}/src/shared/Database/Database.cpp" database_source)

function(isolate source start_token end_token output context)
    string(FIND "${source}" "${start_token}" start)
    string(FIND "${source}" "${end_token}" end)
    if(start EQUAL -1 OR end LESS_EQUAL start)
        message(FATAL_ERROR "${context} seam guard: function not found")
    endif()
    math(EXPR length "${end} - ${start}")
    string(SUBSTRING "${source}" ${start} ${length} isolated)
    set(${output} "${isolated}" PARENT_SCOPE)
endfunction()

macro(mutate variable old new context)
    set(before "${${variable}}")
    string(REPLACE "${old}" "${new}" ${variable} "${${variable}}")
    if("${${variable}}" STREQUAL "${before}")
        message(FATAL_ERROR "${context} mutation setup guard: target not found")
    endif()
endmacro()

function(require_once source pattern context)
    string(REGEX MATCHALL "${pattern}" matches "${source}")
    list(LENGTH matches count)
    if(NOT count EQUAL 1)
        message(FATAL_ERROR
            "${context} guard: expected exactly one match, found ${count}")
    endif()
endfunction()

function(require_text source token context)
    string(FIND "${source}" "${token}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "${context} guard: required text not found")
    endif()
endfunction()

function(require_text_once source token context)
    string(FIND "${source}" "${token}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "${context} guard: required text not found")
    endif()
    string(LENGTH "${token}" token_length)
    math(EXPR remainder_start "${first} + ${token_length}")
    string(SUBSTRING "${source}" ${remainder_start} -1 remainder)
    string(FIND "${remainder}" "${token}" second)
    if(NOT second EQUAL -1)
        message(FATAL_ERROR "${context} guard: required text appears more than once")
    endif()
endfunction()

isolate("${sql_source}" "bool SqlTransaction::Execute(SqlConnection* conn)"
    "SqlPreparedRequest::SqlPreparedRequest" sql_transaction
    "SqlTransaction::Execute")
isolate("${database_source}" "bool Database::CommitTransaction()"
    "bool Database::CommitTransactionDirect()" ordinary_commit
    "Database::CommitTransaction")
isolate("${database_source}" "bool Database::CommitTransactionDirect()"
    "bool Database::RollbackTransaction()" direct_commit
    "Database::CommitTransactionDirect")

if(FORCE_TARGET_DRIFT AND MUTATION STREQUAL "ignore_begin_result")
    mutate(sql_transaction "if (!conn->BeginTransaction())"
        "if (begin_result_guard_drifted)" "target_drift_self_check_seed")
endif()

if(DEFINED MUTATION AND NOT MUTATION STREQUAL "")
    if(MUTATION STREQUAL "ignore_begin_result")
        if(NOT FORCE_TARGET_DRIFT)
            mutate(sql_transaction "if (!conn->BeginTransaction())" "if (false)"
                "ignore_begin_result")
        endif()
    elseif(MUTATION STREQUAL "execute_after_begin_failure")
        mutate(sql_transaction
            "if (!conn->BeginTransaction())\n    {\n        return false;\n    }"
            "if (!conn->BeginTransaction())\n    {\n        // continue after failed BEGIN\n    }"
            "execute_after_begin_failure")
    elseif(MUTATION STREQUAL "return_direct_true")
        mutate(direct_commit "return result;" "return true;"
            "return_direct_true")
    elseif(MUTATION STREQUAL "drop_direct_delete")
        mutate(direct_commit "    delete pTrans;\n" ""
            "drop_direct_delete")
    elseif(MUTATION STREQUAL "drop_direct_detach")
        mutate(direct_commit
            "SqlTransaction* pTrans = (*m_TransStorage)->detach();"
            "SqlTransaction* pTrans = (*m_TransStorage)->get();"
            "drop_direct_detach")
    elseif(MUTATION STREQUAL "narrow_outer_lock")
        mutate(sql_transaction "    LOCK_DB_CONN(conn);\n\n" ""
            "narrow_outer_lock")
    elseif(MUTATION STREQUAL "make_async_commit_direct")
        mutate(ordinary_commit
            "m_threadBody->Delay((*m_TransStorage)->detach());"
            "return CommitTransactionDirect();"
            "make_async_commit_direct")
    elseif(MUTATION STREQUAL "drop_missing_connection_guard")
        mutate(direct_commit
            "if (!m_pAsyncConn)\n    {\n        return false;\n    }"
            "if (false)\n    {\n        return false;\n    }"
            "drop_missing_connection_guard")
    elseif(MUTATION STREQUAL "drop_missing_transaction_guard")
        mutate(direct_commit
            "if (!(*m_TransStorage)->get())\n    {\n        return false;\n    }"
            "if (false)\n    {\n        return false;\n    }"
            "drop_missing_transaction_guard")
    else()
        message(FATAL_ERROR "unknown mutation: ${MUTATION}")
    endif()
endif()

set(ws "[ \t\r\n]")
set(begin_guard_context "BEGIN failure must return immediately")
if(MUTATION STREQUAL "ignore_begin_result" OR
        MUTATION STREQUAL "execute_after_begin_failure")
    set(begin_guard_context "${MUTATION}")
endif()

require_text("${sql_transaction}"
    "if (m_queue.empty())\n    {\n        return true;\n    }"
    "empty transaction fast path")
require_once("${sql_transaction}" "LOCK_DB_CONN[(]conn[)]"
    "narrow_outer_lock")
require_text_once("${sql_transaction}"
    "if (!conn->BeginTransaction())\n    {\n        return false;\n    }"
    "${begin_guard_context}")
require_text_once("${sql_transaction}" "pStmt->Execute(conn)"
    "statement execution")
require_text_once("${sql_transaction}" "conn->RollbackTransaction()"
    "statement failure rollback")
require_text_once("${sql_transaction}" "return conn->CommitTransaction();"
    "COMMIT result propagation")

string(FIND "${sql_transaction}" "LOCK_DB_CONN(conn);" lock_pos)
string(FIND "${sql_transaction}" "if (!conn->BeginTransaction())" begin_pos)
string(FIND "${sql_transaction}" "pStmt->Execute(conn)" execute_pos)
string(FIND "${sql_transaction}" "conn->RollbackTransaction()" rollback_pos)
string(FIND "${sql_transaction}" "return conn->CommitTransaction();" commit_pos)
if(lock_pos EQUAL -1 OR begin_pos LESS_EQUAL lock_pos OR
        execute_pos LESS_EQUAL begin_pos OR rollback_pos LESS_EQUAL execute_pos OR
        commit_pos LESS_EQUAL rollback_pos)
    message(FATAL_ERROR "transaction order and outer lock guard failed")
endif()

require_text_once("${direct_commit}"
    "if (!m_pAsyncConn)\n    {\n        return false;\n    }"
    "drop_missing_connection_guard")
require_text_once("${direct_commit}"
    "if (!(*m_TransStorage)->get())\n    {\n        return false;\n    }"
    "drop_missing_transaction_guard")
require_text_once("${direct_commit}"
    "SqlTransaction* pTrans = (*m_TransStorage)->detach();"
    "drop_direct_detach")
require_text_once("${direct_commit}"
    "bool const result = pTrans->Execute(m_pAsyncConn);"
    "direct Execute result capture")
require_text_once("${direct_commit}" "delete pTrans;"
    "drop_direct_delete")
require_text_once("${direct_commit}" "return result;"
    "return_direct_true")

string(FIND "${direct_commit}"
    "SqlTransaction* pTrans = (*m_TransStorage)->detach();" detach_pos)
string(FIND "${direct_commit}"
    "bool const result = pTrans->Execute(m_pAsyncConn);" execute_direct_pos)
string(FIND "${direct_commit}" "delete pTrans;" delete_pos)
string(FIND "${direct_commit}" "return result;" return_pos)
if(detach_pos EQUAL -1 OR execute_direct_pos LESS_EQUAL detach_pos OR
        delete_pos LESS_EQUAL execute_direct_pos OR return_pos LESS_EQUAL delete_pos)
    message(FATAL_ERROR "direct detach, execute, delete, return order guard failed")
endif()

require_text_once("${ordinary_commit}"
    "if (!m_bAllowAsyncTransactions)\n    {\n        return CommitTransactionDirect();\n    }"
    "pre-async direct result propagation")
require_text_once("${ordinary_commit}"
    "m_threadBody->Delay((*m_TransStorage)->detach());\n    return true;"
    "make_async_commit_direct")
