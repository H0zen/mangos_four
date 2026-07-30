/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 home-realm suffix stripping.
 *
 * The 18414 client appends its space-free realm name to any character name it
 * did not get typed by hand -- a whisper reply, a chat link, a who-list click,
 * a guild roster invite. Live capture on build 18414, one session:
 *
 *     ... 'heyGregory'            target 'Gregory'
 *     ... 'heyHumanwarrior-Four'  target 'Humanwarrior-Four'
 *
 * Both name the same character. StripRealmSuffix is the pure half of the fix,
 * split out from StripHomeRealmSuffix so it can be exercised without a login
 * database.
 */

#include "Util.h"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static std::string Stripped(char const* name, char const* realm)
{
    std::string s(name);
    StripRealmSuffix(s, realm);
    return s;
}

static void test_home_realm_suffix_is_removed()
{
    CHECK(Stripped("Humanwarrior-Four", "Four") == "Humanwarrior");
    CHECK(Stripped("Gregory-Four", "Four") == "Gregory");
}

/// The client's realm name casing is not guaranteed to match `realmlist`.
static void test_match_is_case_insensitive()
{
    CHECK(Stripped("Gregory-four", "Four") == "Gregory");
    CHECK(Stripped("Gregory-FOUR", "Four") == "Gregory");
    CHECK(Stripped("Gregory-Four", "four") == "Gregory");
}

/// A bare name is the hand-typed form and must pass through untouched.
static void test_bare_name_is_untouched()
{
    CHECK(Stripped("Gregory", "Four") == "Gregory");
    CHECK(Stripped("Four", "Four") == "Four");
}

/// A foreign realm keeps its suffix deliberately. This build has no
/// cross-realm support, so the lookup should fail rather than quietly match a
/// local character who happens to share the name.
static void test_foreign_realm_suffix_is_kept()
{
    CHECK(Stripped("Gregory-Other", "Four") == "Gregory-Other");
    CHECK(Stripped("Gregory-Fourteen", "Four") == "Gregory-Fourteen");
}

/// Degenerate inputs must not produce an empty name or truncate wrongly.
static void test_degenerate_inputs()
{
    CHECK(Stripped("-Four", "Four") == "-Four");            // suffix only: not a name
    CHECK(Stripped("", "Four") == "");
    CHECK(Stripped("Gregory-Four", "") == "Gregory-Four");  // no realm known: leave alone
    CHECK(Stripped("Gregory Four", "Four") == "Gregory Four"); // separator must be '-'
}

int main(int /*argc*/, char** /*argv*/)
{
    test_home_realm_suffix_is_removed();
    test_match_is_case_insensitive();
    test_bare_name_is_untouched();
    test_foreign_realm_suffix_is_kept();
    test_degenerate_inputs();

    if (g_fail)
    {
        std::fprintf(stderr, "%d check(s) failed\n", g_fail);
        return 1;
    }

    std::printf("mop_realm_suffix: all checks passed\n");
    return 0;
}
