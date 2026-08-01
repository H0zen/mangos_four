/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "Chat.h"
#include "Database/DatabaseEnv.h"

#include <cstdio>
#include <cstring>
#include <limits>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

namespace
{
int failures = 0;

class TestChatHandler : public ChatHandler
{
public:
    TestChatHandler() : ChatHandler() {}
    using ChatHandler::ExtractUInt64;
};

#define CHECK(expr) do { if (!(expr)) { \
    std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    ++failures; \
} } while (0)

void CheckSuccess(char* input, uint64 expected, char const* expectedTail)
{
    TestChatHandler handler;
    char* args = input;
    uint64 value = 17;

    CHECK(handler.ExtractUInt64(&args, value));
    CHECK(value == expected);
    CHECK(std::strcmp(args, expectedTail) == 0);
}

void CheckFailure(char* input)
{
    TestChatHandler handler;
    char* args = input;
    char* const originalArgs = args;
    uint64 value = 17;

    CHECK(!handler.ExtractUInt64(&args, value));
    CHECK(args == originalArgs);
    CHECK(value == 17);
}
}

int main()
{
    char aboveUInt32[] = "4294967296";
    CheckSuccess(aboveUInt32, UI64LIT(4294967296), "");

    char maximum[] = "18446744073709551615";
    CheckSuccess(maximum, std::numeric_limits<uint64>::max(), "");

    char withTail[] = "9007199254740991  remainder";
    CheckSuccess(withTail, UI64LIT(9007199254740991), "remainder");

    char overflow[] = "18446744073709551616";
    CheckFailure(overflow);

    char negative[] = "-1";
    CheckFailure(negative);

    char spacedNegative[] = "  -1";
    CheckFailure(spacedNegative);

    char invalidTail[] = "42copper";
    CheckFailure(invalidTail);

    if (failures)
    {
        std::fprintf(stderr, "mop_chat_uint64_extract_test: %d failure(s)\n", failures);
        return 1;
    }

    std::puts("mop_chat_uint64_extract_test: all checks passed");
    return 0;
}
