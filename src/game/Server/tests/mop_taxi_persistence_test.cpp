/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 taxi-destination persistence fixtures.
 */

#include "PlayerTaxi.h"
#include "Database/DatabaseEnv.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static void SeedTaxi(PlayerTaxi& taxi)
{
    taxi.SetFlightMasterFactionTemplateId(91);
    taxi.AddTaxiDestination(1);
    taxi.AddTaxiDestination(2);
}

static void CheckCleared(PlayerTaxi& taxi)
{
    CHECK(taxi.empty());

    taxi.AddTaxiDestination(1);
    taxi.AddTaxiDestination(2);
    CHECK(taxi.SaveTaxiDestinationsToString() == "0 1 2 ");
    taxi.ClearTaxiDestinations();
}

class FixtureValidator final : public TaxiPersistence::Validator
{
    public:
        std::vector<std::pair<uint32, uint32>> paths;
        std::vector<std::pair<uint32, uint32>> checkedPaths;
        uint32 mountNode = 0;
        Team mountTeam = TEAM_NONE;
        bool throwOnPath = false;
        bool throwOnMount = false;

        bool HasTaxiPath(uint32 source, uint32 destination) override
        {
            if (throwOnPath)
            {
                throw std::runtime_error("path fixture failure");
            }

            checkedPaths.emplace_back(source, destination);
            return std::find(paths.begin(), paths.end(), std::make_pair(source, destination)) != paths.end();
        }

        bool HasTaxiMount(uint32 source, Team team) override
        {
            if (throwOnMount)
            {
                throw std::runtime_error("mount fixture failure");
            }

            return source == mountNode && team == mountTeam;
        }
};

static bool Load(PlayerTaxi& taxi, std::string const& value, Team team,
    FixtureValidator& validator)
{
    return TaxiPersistence::LoadTaxiDestinations(taxi, value, team, validator);
}

static void test_empty_is_a_clean_no_flight_state()
{
    for (std::string const& value : {std::string(), std::string(" \t\r\n ")})
    {
        PlayerTaxi taxi;
        SeedTaxi(taxi);
        CHECK(taxi.LoadTaxiDestinationsFromString(value, ALLIANCE));
        CheckCleared(taxi);
    }
}

static void test_malformed_input_does_not_escape_or_leave_state()
{
    for (std::string const& value : {
        std::string("91"),
        std::string("91 1"),
        std::string("not-a-number 1 2"),
        std::string("91 not-a-number 2"),
        std::string("-1 1 2"),
        std::string("91 -1 2"),
        std::string("4294967296 1 2"),
        std::string("91 4294967296 2"),
        std::string("91 1 2junk"),
        std::string("+91 1 2"),
        std::string("91 0 2"),
        std::string("91 1297 2")})
    {
        PlayerTaxi taxi;
        FixtureValidator validator;
        validator.paths.emplace_back(1, 2);
        validator.mountNode = 1;
        validator.mountTeam = ALLIANCE;
        SeedTaxi(taxi);

        bool result = true;
        bool threw = false;
        try
        {
            result = Load(taxi, value, ALLIANCE, validator);
        }
        catch (...)
        {
            threw = true;
        }

        CHECK(!threw);
        CHECK(!result);
        CheckCleared(taxi);
    }
}

static void test_public_loader_contains_parse_failures()
{
    for (std::string const& value : {std::string("not-a-number"), std::string("91")})
    {
        PlayerTaxi taxi;
        SeedTaxi(taxi);

        bool result = true;
        bool threw = false;
        try
        {
            result = taxi.LoadTaxiDestinationsFromString(value, ALLIANCE);
        }
        catch (...)
        {
            threw = true;
        }

        CHECK(!threw);
        CHECK(!result);
        CheckCleared(taxi);
    }
}

static void test_two_node_round_trip()
{
    PlayerTaxi taxi;
    FixtureValidator validator;
    validator.paths.emplace_back(1, 2);
    validator.mountNode = 1;
    validator.mountTeam = ALLIANCE;

    CHECK(Load(taxi, "91 1 2 ", ALLIANCE, validator));
    CHECK(taxi.GetTaxiSource() == 1);
    CHECK(taxi.GetTaxiDestination() == 2);
    CHECK(taxi.SaveTaxiDestinationsToString() == "91 1 2 ");
    std::vector<std::pair<uint32, uint32>> const expectedPaths = {{1, 2}};
    CHECK(validator.checkedPaths == expectedPaths);
}

static void test_multi_node_and_high_node_round_trip()
{
    PlayerTaxi taxi;
    FixtureValidator validator;
    validator.paths = {{1294, 1295}, {1295, 1296}};
    validator.mountNode = 1294;
    validator.mountTeam = HORDE;

    CHECK(Load(taxi, "4294967295\t1294  1295\r\n1296", HORDE, validator));
    CHECK(taxi.GetTaxiSource() == 1294);
    CHECK(taxi.GetTaxiDestination() == 1295);
    CHECK(taxi.SaveTaxiDestinationsToString() == "4294967295 1294 1295 1296 ");
    std::vector<std::pair<uint32, uint32>> const expectedPaths = {{1294, 1295}, {1295, 1296}};
    CHECK(validator.checkedPaths == expectedPaths);
}

static void test_absent_route_or_mount_clears_all_state()
{
    {
        PlayerTaxi taxi;
        FixtureValidator validator;
        validator.paths.emplace_back(1, 2);
        validator.mountNode = 1;
        validator.mountTeam = ALLIANCE;
        SeedTaxi(taxi);

        CHECK(!Load(taxi, "73 1 2 3", ALLIANCE, validator));
        CheckCleared(taxi);
    }

    {
        PlayerTaxi taxi;
        FixtureValidator validator;
        validator.paths.emplace_back(1, 2);
        validator.mountNode = 9;
        validator.mountTeam = ALLIANCE;
        SeedTaxi(taxi);

        CHECK(!Load(taxi, "73 1 2", ALLIANCE, validator));
        CheckCleared(taxi);
    }
}

static void test_validator_exceptions_do_not_escape_or_leave_state()
{
    for (bool throwOnMount : {false, true})
    {
        PlayerTaxi taxi;
        FixtureValidator validator;
        validator.paths.emplace_back(1, 2);
        validator.mountNode = 1;
        validator.mountTeam = ALLIANCE;
        validator.throwOnPath = !throwOnMount;
        validator.throwOnMount = throwOnMount;
        SeedTaxi(taxi);

        bool result = true;
        bool threw = false;
        try
        {
            result = Load(taxi, "73 1 2", ALLIANCE, validator);
        }
        catch (...)
        {
            threw = true;
        }

        CHECK(!threw);
        CHECK(!result);
        CheckCleared(taxi);
    }
}

int main()
{
    test_empty_is_a_clean_no_flight_state();
    test_malformed_input_does_not_escape_or_leave_state();
    test_public_loader_contains_parse_failures();
    test_two_node_round_trip();
    test_multi_node_and_high_node_round_trip();
    test_absent_route_or_mount_clears_all_state();
    test_validator_exceptions_do_not_escape_or_leave_state();

    if (g_fail)
    {
        std::fprintf(stderr, "%d taxi-persistence fixture(s) failed\n", g_fail);
        return 1;
    }

    std::puts("MoP taxi-persistence fixtures passed");
    return 0;
}
