# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(sql_transaction_direct_result_test
    sql_transaction_direct_result_test.cpp)
target_link_libraries(sql_transaction_direct_result_test PRIVATE shared)
set_target_properties(sql_transaction_direct_result_test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)
add_test(NAME sql_transaction_direct_result
    COMMAND sql_transaction_direct_result_test)

if(WIN32)
    get_filename_component(SQL_TRANSACTION_MYSQL_RUNTIME_DIR
        "${MySQL_LIBRARY}" DIRECTORY)
    set_tests_properties(sql_transaction_direct_result PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${SQL_TRANSACTION_MYSQL_RUNTIME_DIR}")
endif()

add_test(NAME sql_transaction_direct_result_source
    COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/sql_transaction_direct_result_source_test.cmake)

foreach(mutation IN ITEMS
        ignore_begin_result
        execute_after_begin_failure
        return_direct_true
        drop_direct_delete
        drop_direct_detach
        narrow_outer_lock
        make_async_commit_direct
        drop_missing_connection_guard
        drop_missing_transaction_guard)
    add_test(NAME sql_transaction_direct_result_source_mutation_${mutation}
        COMMAND ${CMAKE_COMMAND} -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
            -DMUTATION=${mutation}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/sql_transaction_direct_result_source_test.cmake)
    set_tests_properties(
        sql_transaction_direct_result_source_mutation_${mutation}
        PROPERTIES WILL_FAIL TRUE)
endforeach()
