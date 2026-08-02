/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS 5.4.8 taxi spline-completion lifecycle coverage.
 */

#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "IClientLink.h"
#include "Player.h"
#include "WaypointMovementGenerator.h"
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

class DiscardingClientLink final : public proto::IClientLink
{
    public:
        void SendPacket(WorldPacket const& packet) override
        {
            if (packet.GetOpcode() == SMSG_MONSTER_MOVE)
            {
                monsterMove = packet;
            }
        }
        void Close() override { closed = true; }
        std::string const& GetRemoteAddress() const override { return address; }
        bool IsClosed() const override { return closed; }

        uint32 GetMonsterMoveUncompressedPointCount() const
        {
            WorldPacket packet = monsterMove;
            if (packet.size() < 32)
            {
                return 0;
            }

            packet.rpos(28);
            packet.ResetBitReader();
            packet.ReadBit();
            packet.ReadBit();
            uint32 const type = packet.ReadBits(3);
            if (type == Movement::MonsterMoveFacingTarget)
            {
                packet.ReadBits(8);
            }
            packet.ReadBit();
            packet.ReadBit();
            packet.ReadBit();
            return packet.ReadBits(20);
        }

    private:
        std::string address = "taxi-completion-test";
        bool closed = false;
        WorldPacket monsterMove;
};

class TaxiCompletionTestPlayer final : public Player
{
    public:
        explicit TaxiCompletionTestPlayer(WorldSession* session) : Player(session) {}

        void InitializeForTest(uint32 lowGuid)
        {
            Object::_Create(lowGuid, 0, HIGHGUID_PLAYER);
            GetMotionMaster()->Initialize();
        }
};

static std::vector<uint8> CompletionFixture(uint32 splineId)
{
    std::vector<uint8> body = {
        0x6F, 0x71, 0x1C, 0x03, 0x19, 0x3B, 0x53, 0xC3,
        0x44, 0x12, 0x97, 0x45, 0x54, 0x01, 0xAB, 0x45,
        0xEB, 0x58, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00,
        0x00, 0xB1, 0x6C, 0x05, 0x5B, 0x04, 0x09, 0xF5,
        0x6D, 0x02
    };
    body[0] = uint8(splineId);
    body[1] = uint8(splineId >> 8);
    body[2] = uint8(splineId >> 16);
    body[3] = uint8(splineId >> 24);
    return body;
}

static WorldPacket MakeCompletionPacket(uint32 splineId)
{
    std::vector<uint8> const body = CompletionFixture(splineId);
    WorldPacket packet(CMSG_MOVE_SPLINE_DONE, body.size());
    packet.append(body.data(), body.size());
    return packet;
}

static void ConfigureSameMapPath(TaxiPathNodeEntry (&nodes)[3])
{
    sTaxiPathNodesByPath.clear();
    sTaxiPathNodesByPath.resize(2);
    sTaxiPathNodesByPath[1].resize(3);
    for (uint32 i = 0; i < 3; ++i)
    {
        nodes[i].PathID = 1;
        nodes[i].NodeIndex = i;
        nodes[i].ContinentID = 0;
        nodes[i].x = float(i * 32);
        sTaxiPathNodesByPath[1].set(i, TaxiPathNodePtr(&nodes[i]));
    }
    sTaxiPathSetBySource.clear();
    sTaxiPathSetBySource[10][20] = TaxiPathBySourceAndDestination(1, 0);
}

static void ConfigureSecondSameMapPath(TaxiPathNodeEntry (&nodes)[3])
{
    sTaxiPathNodesByPath.resize(3);
    sTaxiPathNodesByPath[2].resize(3);
    for (uint32 i = 0; i < 3; ++i)
    {
        nodes[i].PathID = 2;
        nodes[i].NodeIndex = i;
        nodes[i].ContinentID = 0;
        nodes[i].x = float((i + 2) * 32);
        sTaxiPathNodesByPath[2].set(i, TaxiPathNodePtr(&nodes[i]));
    }
    sTaxiPathSetBySource[20][30] = TaxiPathBySourceAndDestination(2, 0);
}

static void ConfigureCrossMapPath(TaxiPathNodeEntry (&nodes)[4])
{
    sTaxiPathNodesByPath.clear();
    sTaxiPathNodesByPath.resize(2);
    sTaxiPathNodesByPath[1].resize(4);
    for (uint32 i = 0; i < 4; ++i)
    {
        nodes[i].PathID = 1;
        nodes[i].NodeIndex = i;
        nodes[i].ContinentID = i < 2 ? 0 : 1;
        nodes[i].x = float(i * 32);
        sTaxiPathNodesByPath[1].set(i, TaxiPathNodePtr(&nodes[i]));
    }
    sTaxiPathSetBySource.clear();
    sTaxiPathSetBySource[10][20] = TaxiPathBySourceAndDestination(1, 0);
}

static void ClearTaxiTestStores()
{
    sTaxiPathSetBySource.clear();
    sTaxiPathNodesByPath.clear();
}

static void test_multihop_completion_after_finalization_cleans_failed_rollover()
{
    TaxiPathNodeEntry nodes[3] = {};
    ConfigureSameMapPath(nodes);

    uint8 sessionKey[MopAuth::SESSION_KEY_LEN] = {};
    std::shared_ptr<DiscardingClientLink> link = std::make_shared<DiscardingClientLink>();
    WorldSession session(1, link, SEC_PLAYER, 4, 0, LOCALE_enUS, sessionKey);

    {
        TaxiCompletionTestPlayer player(&session);
        session.SetPlayer(&player);
        player.InitializeForTest(0x055AB06D);
        player.m_taxi.AddTaxiDestination(10);
        player.m_taxi.AddTaxiDestination(20);
        player.m_taxi.AddTaxiDestination(30);

        CHECK(player.StartTaxiFlight(1234, 1, 0));
        CHECK(player.m_taxi.GetFlightLedger().GetPhase() == TaxiFlightPhase::InFlight);
        uint32 const splineId = player.movespline->GetId();

        player.movespline->updateState(player.movespline->Duration() + 1);
        player.GetMotionMaster()->UpdateMotion(1);
        CHECK(player.GetMotionMaster()->GetCurrentMovementGeneratorType() != FLIGHT_MOTION_TYPE);
        CHECK(player.m_taxi.GetTaxiSource() == 10);
        CHECK(player.m_taxi.GetTaxiDestination() == 20);
        CHECK(player.m_taxi.GetFlightLedger().GetPhase() == TaxiFlightPhase::AwaitingCompletion);

        WorldPacket packet = MakeCompletionPacket(splineId);
        session.HandleMoveSplineDoneOpcode(packet);
        CHECK(player.GetMotionMaster()->GetCurrentMovementGeneratorType() != FLIGHT_MOTION_TYPE);
        CHECK(player.m_taxi.empty());
        CHECK(player.m_taxi.GetFlightLedger().GetPhase() == TaxiFlightPhase::Finalized);

        session.SetPlayer(nullptr);
    }

    ClearTaxiTestStores();
}

static void test_multihop_same_map_route_uses_one_continuous_spline()
{
    TaxiPathNodeEntry firstLeg[3] = {};
    TaxiPathNodeEntry secondLeg[3] = {};
    ConfigureSameMapPath(firstLeg);
    ConfigureSecondSameMapPath(secondLeg);

    uint8 sessionKey[MopAuth::SESSION_KEY_LEN] = {};
    std::shared_ptr<DiscardingClientLink> link = std::make_shared<DiscardingClientLink>();
    WorldSession session(1, link, SEC_PLAYER, 4, 0, LOCALE_enUS, sessionKey);

    {
        TaxiCompletionTestPlayer player(&session);
        session.SetPlayer(&player);
        player.InitializeForTest(0x055AB06D);
        player.m_taxi.AddTaxiDestination(10);
        player.m_taxi.AddTaxiDestination(20);
        player.m_taxi.AddTaxiDestination(30);

        CHECK(player.StartTaxiFlight(1234, 1, 0));
        CHECK(player.GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE);
        FlightPathMovementGenerator* flight = static_cast<FlightPathMovementGenerator*>(
            player.GetMotionMaster()->top());
        CHECK(flight->GetPath().size() == 4);
        if (flight->GetPath().size() == 4)
        {
            CHECK(flight->GetPath()[0].PathID == 1);
            CHECK(flight->GetPath()[1].PathID == 1);
            CHECK(flight->GetPath()[2].PathID == 2);
            CHECK(flight->GetPath()[3].PathID == 2);
        }
        // The launch path includes a separate current-position control point.
        // Only the duplicated flight-master endpoint is removed at the join.
        CHECK(link->GetMonsterMoveUncompressedPointCount() == 4);
        CHECK(player.m_taxi.GetTaxiSource() == 10);
        CHECK(player.m_taxi.GetTaxiDestination() == 20);
        CHECK(player.m_taxi.GetFlightLedger().GetPhase() == TaxiFlightPhase::Inactive);

        player.movespline->updateState(player.movespline->Duration() + 1);
        player.GetMotionMaster()->UpdateMotion(1);
        CHECK(player.GetMotionMaster()->GetCurrentMovementGeneratorType() != FLIGHT_MOTION_TYPE);
        CHECK(player.m_taxi.empty());
        session.SetPlayer(nullptr);
    }

    ClearTaxiTestStores();
}

static void test_cross_map_completion_transfers_without_consuming_destination()
{
    TaxiPathNodeEntry nodes[4] = {};
    ConfigureCrossMapPath(nodes);
    MapEntry destinationMap = {};
    destinationMap.ID = 1;
    sMapStore.SetEntry(1, &destinationMap);

    uint8 sessionKey[MopAuth::SESSION_KEY_LEN] = {};
    std::shared_ptr<DiscardingClientLink> link = std::make_shared<DiscardingClientLink>();
    WorldSession session(1, link, SEC_PLAYER, 4, 0, LOCALE_enUS, sessionKey);

    {
        TaxiCompletionTestPlayer player(&session);
        session.SetPlayer(&player);
        player.InitializeForTest(0x055AB06D);
        player.m_taxi.AddTaxiDestination(10);
        player.m_taxi.AddTaxiDestination(20);

        CHECK(player.StartTaxiFlight(1234, 1, 0));
        CHECK(player.GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE);
        FlightPathMovementGenerator* flight = static_cast<FlightPathMovementGenerator*>(
            player.GetMotionMaster()->top());
        CHECK(flight->GetPathAtMapEnd() == 2);
        uint32 const splineId = player.movespline->GetId();
        player.movespline->updateState(player.movespline->Duration() + 1);
        CHECK(player.movespline->currentPathIdx() == 1);
        player.GetMotionMaster()->UpdateMotion(1);
        CHECK(flight->GetCurrentNode() == 1);

        WorldPacket packet = MakeCompletionPacket(splineId);
        session.HandleMoveSplineDoneOpcode(packet);

        CHECK(player.IsBeingTeleportedFar());
        CHECK(player.GetTeleportDest().mapid == 1);
        CHECK(flight->GetCurrentNode() == 2);
        CHECK(player.m_taxi.GetTaxiSource() == 10);
        CHECK(player.m_taxi.GetTaxiDestination() == 20);
        session.SetPlayer(nullptr);
    }

    sMapStore.Clear();
    ClearTaxiTestStores();
}

int main()
{
    test_multihop_completion_after_finalization_cleans_failed_rollover();
    test_multihop_same_map_route_uses_one_continuous_spline();
    test_cross_map_completion_transfers_without_consuming_destination();

    if (g_fail)
    {
        std::fprintf(stderr, "%d taxi-completion lifecycle check(s) failed\n", g_fail);
        return 1;
    }

    return 0;
}
