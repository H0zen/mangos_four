/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 direct taxi-activation runtime failure coverage.
 */

#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "IClientLink.h"
#include "Player.h"
#include "WorldSession.h"
#include "movement/MoveSpline.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

DatabaseType WorldDatabase;
DatabaseType CharacterDatabase;
DatabaseType LoginDatabase;
uint32 realmID = 0;

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

class CapturingClientLink final : public proto::IClientLink
{
    public:
        void SendPacket(WorldPacket const& packet) override { packets.push_back(packet); }
        void Close() override { closed = true; }
        std::string const& GetRemoteAddress() const override { return address; }
        bool IsClosed() const override { return closed; }

        std::vector<WorldPacket> packets;

    private:
        std::string address = "taxi-activation-test";
        bool closed = false;
};

class TaxiTestPlayer final : public Player
{
    public:
        explicit TaxiTestPlayer(WorldSession* session) : Player(session) {}

        void InitializeForTest()
        {
            Object::_Create(1, 0, HIGHGUID_PLAYER);
            GetMotionMaster()->Initialize();
        }
};

static void test_same_map_route_checks_every_dbc_path_node()
{
    TaxiPathNodeEntry nodes[3] = {};
    TaxiPathNodeList path;
    path.resize(3);
    for (uint32 i = 0; i < 3; ++i)
    {
        nodes[i].PathID = 1;
        nodes[i].NodeIndex = i;
        nodes[i].ContinentID = 0;
        path.set(i, TaxiPathNodePtr(&nodes[i]));
    }

    CHECK(MopTaxiPackets::IsSameMapTaxiPath(path, 0));
    nodes[1].ContinentID = 1;
    CHECK(!MopTaxiPackets::IsSameMapTaxiPath(path, 0));
}

static void test_one_node_spline_is_a_failed_flight_and_is_fully_finalized()
{
    TaxiPathNodeEntry node = {};
    node.PathID = 1;
    node.NodeIndex = 0;
    node.ContinentID = 0;

    sTaxiPathNodesByPath.clear();
    sTaxiPathNodesByPath.resize(2);
    sTaxiPathNodesByPath[1].resize(1);
    sTaxiPathNodesByPath[1].set(0, TaxiPathNodePtr(&node));

    uint8 sessionKey[MopAuth::SESSION_KEY_LEN] = {};
    std::shared_ptr<CapturingClientLink> link = std::make_shared<CapturingClientLink>();
    WorldSession session(1, link, SEC_PLAYER, 4, 0, LOCALE_enUS, sessionKey);

    {
        TaxiTestPlayer player(&session);
        session.SetPlayer(&player);
        player.InitializeForTest();
        player.m_taxi.AddTaxiDestination(10);
        player.m_taxi.AddTaxiDestination(20);
        player.SetMoney(100);
        link->packets.clear();
        size_t const completedAchievementsBefore =
            player.GetAchievementMgr().GetCompletedAchievements().size();

        CHECK(!player.StartTaxiFlight(1234, 1, 10));
        CHECK(player.GetMoney() == 100);
        CHECK(player.GetAchievementMgr().GetCompletedAchievements().size() ==
            completedAchievementsBefore);
        CHECK(player.m_taxi.empty());
        CHECK(!player.IsMounted());
        CHECK(!player.IsTaxiFlying());
        CHECK(!player.HasFlag(UNIT_FIELD_FLAGS,
            UNIT_FLAG_DISABLE_MOVE | UNIT_FLAG_TAXI_FLIGHT));
        CHECK(player.GetMotionMaster()->GetCurrentMovementGeneratorType() !=
            FLIGHT_MOTION_TYPE);
        CHECK(player.movespline->Finalized());

        bool sawFailureReply = false;
        for (WorldPacket const& packet : link->packets)
        {
            CHECK(packet.GetOpcode() != SMSG_CRITERIA_UPDATE);
            CHECK(packet.GetOpcode() != SMSG_ACHIEVEMENT_EARNED);
            if (packet.GetOpcode() == SMSG_ACTIVATETAXIREPLY)
            {
                CHECK(packet.size() == 1);
                CHECK(packet.contents()[0] == 0x50);
                sawFailureReply = true;
            }
        }
        CHECK(sawFailureReply);

        session.SetPlayer(nullptr);
    }

    sTaxiPathNodesByPath.clear();
}

int main()
{
    test_same_map_route_checks_every_dbc_path_node();
    test_one_node_spline_is_a_failed_flight_and_is_fully_finalized();

    if (g_fail)
    {
        std::fprintf(stderr, "%d taxi-activation runtime check(s) failed\n", g_fail);
        return 1;
    }

    std::puts("MoP taxi-activation runtime checks passed");
    return 0;
}
