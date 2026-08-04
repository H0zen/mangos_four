/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Direct transaction result and ownership tests.
 */

#include "Database/QueryResult.h"
#include "Database/Database.h"
#include "Database/SqlOperations.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <deque>
#include <string>
#include <thread>
#include <vector>

namespace
{
int failures = 0;
int casesRun = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

class FakeConnection;

class TestDatabase final : public Database
{
    public:
        TestDatabase();
        ~TestDatabase() override;

        FakeConnection& Connection();
        void Queue(SqlOperation* operation);
        void EnableAsyncQueue();
        void FlushAsyncQueue();

    protected:
        SqlConnection* CreateConnection() override;
};

class FakeConnection final : public SqlConnection
{
    public:
        explicit FakeConnection(Database& database) : SqlConnection(database) {}
        ~FakeConnection() override { JoinContender(); }

        bool Initialize(char const*) override { return true; }
        QueryResult* Query(char const*) override { return NULL; }
        QueryNamedResult* QueryNamed(char const*) override { return NULL; }

        bool Execute(char const* sql) override
        {
            calls.push_back(std::string("STMT ") + sql);
            if (statementResults.empty())
            {
                return true;
            }

            bool const result = statementResults.front();
            statementResults.pop_front();
            return result;
        }

        bool BeginTransaction() override
        {
            calls.push_back("BEGIN");
            if (probeOuterLock)
            {
                contenderStarted = false;
                contenderAcquired = false;
                contender = std::thread([this]() {
                    contenderStarted = true;
                    SqlConnection::Lock guard(this);
                    contenderAcquired = true;
                });

                while (!contenderStarted)
                {
                    std::this_thread::yield();
                }

                for (int i = 0; i < 100 && !contenderAcquired; ++i)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                outerLockHeldAcrossBegin = !contenderAcquired;
            }
            return beginResult;
        }

        bool CommitTransaction() override
        {
            calls.push_back("COMMIT");
            return commitResult;
        }

        bool RollbackTransaction() override
        {
            calls.push_back("ROLLBACK");
            return true;
        }

        void JoinContender()
        {
            if (contender.joinable())
            {
                contender.join();
            }
        }

        bool beginResult = true;
        bool commitResult = true;
        bool probeOuterLock = false;
        bool outerLockHeldAcrossBegin = false;
        std::deque<bool> statementResults;
        std::vector<std::string> calls;

    private:
        std::atomic<bool> contenderStarted{false};
        std::atomic<bool> contenderAcquired{false};
        std::thread contender;
};

TestDatabase::TestDatabase()
{
    m_TransStorage = new DBTransHelperTSS();
    m_pAsyncConn = new FakeConnection(*this);
}

TestDatabase::~TestDatabase()
{
    FlushAsyncQueue();
    delete m_TransStorage;
    m_TransStorage = NULL;
}

FakeConnection& TestDatabase::Connection()
{
    return *static_cast<FakeConnection*>(m_pAsyncConn);
}

void TestDatabase::Queue(SqlOperation* operation)
{
    (*m_TransStorage)->get()->DelayExecute(operation);
}

void TestDatabase::EnableAsyncQueue()
{
    m_threadBody = new SqlDelayThread(this, m_pAsyncConn);
    AllowAsyncTransactions();
}

void TestDatabase::FlushAsyncQueue()
{
    delete m_threadBody;
    m_threadBody = NULL;
}

SqlConnection* TestDatabase::CreateConnection()
{
    return new FakeConnection(*this);
}

class CountingOperation final : public SqlOperation
{
    public:
        CountingOperation(int& executions, int& destructions)
            : executions(executions), destructions(destructions) {}

        ~CountingOperation() override { ++destructions; }

        // ExecuteLocked is the work; Execute is the non-virtual entry that takes the
        // connection's lock around it.
        bool ExecuteLocked(SqlConnection*) override
        {
            ++executions;
            return true;
        }

    private:
        int& executions;
        int& destructions;
};

void CheckCalls(FakeConnection const& connection,
    std::initializer_list<char const*> expected)
{
    std::vector<std::string> const expectedCalls(expected.begin(), expected.end());
    CHECK(connection.calls == expectedCalls);
}

// Break caught: inventing a direct commit when no transaction is pending.
void TestNoPendingTransaction()
{
    ++casesRun;
    TestDatabase database;
    CHECK(!database.CommitTransactionDirect());
    CHECK(database.Connection().calls.empty());
}

// Break caught: opening a SQL transaction for an empty pending transaction.
void TestEmptyTransactionFastPath()
{
    ++casesRun;
    TestDatabase database;
    CHECK(database.BeginTransaction());
    CHECK(database.CommitTransactionDirect());
    CHECK(database.Connection().calls.empty());
}

// Break caught: executing statements or finalization after BEGIN fails.
void TestBeginFailureStopsImmediately()
{
    ++casesRun;
    TestDatabase database;
    database.Connection().beginResult = false;
    CHECK(database.BeginTransaction());
    CHECK(database.Execute("one"));
    CHECK(!database.CommitTransactionDirect());
    CheckCalls(database.Connection(), {"BEGIN"});
}

// Break caught: continuing after a failed statement or rolling back other than once.
void TestStatementFailuresRollbackOnce()
{
    ++casesRun;
    {
        TestDatabase database;
        database.Connection().statementResults = {false};
        CHECK(database.BeginTransaction());
        CHECK(database.Execute("one"));
        CHECK(database.Execute("two"));
        CHECK(!database.CommitTransactionDirect());
        CheckCalls(database.Connection(), {"BEGIN", "STMT one", "ROLLBACK"});
    }
    {
        TestDatabase database;
        database.Connection().statementResults = {true, false};
        CHECK(database.BeginTransaction());
        CHECK(database.Execute("one"));
        CHECK(database.Execute("two"));
        CHECK(database.Execute("three"));
        CHECK(!database.CommitTransactionDirect());
        CheckCalls(database.Connection(),
            {"BEGIN", "STMT one", "STMT two", "ROLLBACK"});
    }
}

// Break caught: hiding a COMMIT failure or inventing rollback after COMMIT.
void TestCommitFailureIsReturned()
{
    ++casesRun;
    TestDatabase database;
    database.Connection().commitResult = false;
    CHECK(database.BeginTransaction());
    CHECK(database.Execute("one"));
    CHECK(!database.CommitTransactionDirect());
    CheckCalls(database.Connection(), {"BEGIN", "STMT one", "COMMIT"});
}

// Break caught: reordering successful statements or losing the COMMIT result.
void TestSuccessOrder()
{
    ++casesRun;
    TestDatabase database;
    CHECK(database.BeginTransaction());
    CHECK(database.Execute("one"));
    CHECK(database.Execute("two"));
    CHECK(database.CommitTransactionDirect());
    CheckCalls(database.Connection(), {"BEGIN", "STMT one", "STMT two", "COMMIT"});
}

// Break caught: retaining or executing direct transaction ownership more than once.
void TestDirectCommitConsumesOwnershipOnce()
{
    ++casesRun;
    int executions = 0;
    int destructions = 0;
    TestDatabase database;
    CHECK(database.BeginTransaction());
    database.Queue(new CountingOperation(executions, destructions));
    CHECK(database.CommitTransactionDirect());
    CHECK(executions == 1);
    CHECK(destructions == 1);
    CHECK(!database.CommitTransactionDirect());
    CHECK(executions == 1);
    CHECK(destructions == 1);
}

// Break caught: caller rollback touching SQL or leaking queued ownership.
void TestCallerRollbackCancelsPendingWork()
{
    ++casesRun;
    int executions = 0;
    int destructions = 0;
    TestDatabase database;
    CHECK(database.BeginTransaction());
    database.Queue(new CountingOperation(executions, destructions));
    CHECK(database.RollbackTransaction());
    CHECK(executions == 0);
    CHECK(destructions == 1);
    CHECK(database.Connection().calls.empty());
    CHECK(!database.CommitTransactionDirect());
}

// Break caught: narrowing connection serialization to individual statements.
void TestOuterLockCoversWholeSqlTransaction()
{
    ++casesRun;
    TestDatabase database;
    database.Connection().probeOuterLock = true;
    CHECK(database.BeginTransaction());
    CHECK(database.Execute("one"));
    CHECK(database.CommitTransactionDirect());
    database.Connection().JoinContender();
    CHECK(database.Connection().outerLockHeldAcrossBegin);
    CheckCalls(database.Connection(), {"BEGIN", "STMT one", "COMMIT"});
}

// Break caught: pre-async false-success or runtime async execution becoming direct.
void TestOrdinaryCommitModesRemainDistinct()
{
    ++casesRun;
    {
        TestDatabase database;
        database.Connection().beginResult = false;
        CHECK(database.BeginTransaction());
        CHECK(database.Execute("one"));
        CHECK(!database.CommitTransaction());
        CheckCalls(database.Connection(), {"BEGIN"});
        CHECK(!database.CommitTransactionDirect());
    }
    {
        TestDatabase database;
        database.EnableAsyncQueue();
        CHECK(database.BeginTransaction());
        CHECK(database.Execute("one"));
        CHECK(database.CommitTransaction());
        CHECK(database.Connection().calls.empty());
        CHECK(!database.CommitTransactionDirect());
        database.FlushAsyncQueue();
        CheckCalls(database.Connection(), {"BEGIN", "STMT one", "COMMIT"});
    }
}
}

int main()
{
    TestNoPendingTransaction();
    TestEmptyTransactionFastPath();
    TestBeginFailureStopsImmediately();
    TestStatementFailuresRollbackOnce();
    TestCommitFailureIsReturned();
    TestSuccessOrder();
    TestDirectCommitConsumesOwnershipOnce();
    TestCallerRollbackCancelsPendingWork();
    TestOuterLockCoversWholeSqlTransaction();
    TestOrdinaryCommitModesRemainDistinct();

    if (failures == 0)
    {
        std::printf("PASS: %d direct transaction cases\n", casesRun);
    }
    return failures == 0 ? 0 : 1;
}
