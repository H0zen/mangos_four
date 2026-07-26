/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_WORLDGATEWAY
#define MANGOS_H_WORLDGATEWAY

#include "IWorldGateway.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class WorldSession;

/**
 * @brief The world's side of the protocol seam.
 *
 * Everything the old WorldSocket::HandleAuthSession used to reach for -- the
 * login database, the IP-lock and ban checks, the allowed client build, the
 * security floor, and the WorldSession object itself -- lives here, on the far
 * side of an interface proto knows nothing about.
 *
 * proto refers to a session only by an opaque SessionId. That indirection is
 * not ceremony: it means a connection can never be handed a WorldSession
 * pointer it might outlive, and the registry below is the single place where
 * the mapping is resolved under a lock.
 */
class WorldGateway : public proto::IWorldGateway
{
    public:

        WorldGateway();
        ~WorldGateway() override;

        // --- proto::IWorldGateway -----------------------------------------

        proto::AuthLookup LookupAccount(const proto::AuthRequest& request) override;

        proto::SessionId Attach(const proto::AuthRequest& request,
                                const std::shared_ptr<proto::IClientLink>& link,
                                const std::shared_ptr<proto::AuthContext>& context,
                                proto::IWorldGateway::AuthCommit commit,
                                void* commitContext) override;

        void Deliver(proto::SessionId session, WorldPacket&& packet) override;

        bool OnPing(proto::SessionId session, uint32 latency,
                   uint32 fastPingRun) override;

        void Detach(proto::SessionId session) override;

        void BuildAuthErrorResponse(proto::AuthStatus status, WorldPacket& out) override;

        void OnPacketReceived(WorldPacket& packet, proto::SessionId session) override;

    private:

        /// Resolve a handle to a live session, or NULL. Caller must hold m_lock.
        WorldSession* Find(proto::SessionId session) const;

        mutable std::mutex m_lock;

        /// Handles are drawn from a counter rather than reusing account ids, so a
        /// stale handle from a torn-down connection can never resolve onto the
        /// session of a player who has since logged back in.
        proto::SessionId m_nextId;

        std::unordered_map<proto::SessionId, WorldSession*> m_sessions;
};

#endif
