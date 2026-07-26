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

#ifndef MANGOS_PROTO_IWORLDGATEWAY_H
#define MANGOS_PROTO_IWORLDGATEWAY_H

#include "IClientLink.h"
#include "MopAuthSession.h"

#include "Auth/MopAuthKey.h"
#include "Common/Common.h"

#include <cstring>
#include <memory>
#include <string>

namespace proto
{
    /**
     * @brief Opaque handle the world hands back for an attached session.
     *
     * proto never dereferences it; it only quotes it back on Deliver()/OnPing()/
     * Detach(). Zero means "no session".
     */
    typedef uint32 SessionId;

    const SessionId INVALID_SESSION_ID = 0;

    /**
     * @brief Wire AUTH_* verdict codes, as they go out in SMSG_AUTH_RESPONSE.
     *
     * Re-derived from this fork's own AUTH_* enum (src/game/Server/SharedDefines.h:
     * 3937 onward) -- NOT included from there, since proto does not link game. Only
     * the subset WorldSocket::HandleAuthSession actually emits today is reproduced;
     * add more as the game-side gateway needs them, re-checking the value against
     * SharedDefines.h each time (do not guess the next one in the sequence).
     */
    enum class AuthStatus : uint8
    {
        Ok              = 0x0C, // AUTH_OK
        Failed          = 0x0D, // AUTH_FAILED
        Reject          = 0x0E, // AUTH_REJECT
        BadServerProof  = 0x0F, // AUTH_BAD_SERVER_PROOF
        Unavailable     = 0x10, // AUTH_UNAVAILABLE
        SystemError     = 0x11, // AUTH_SYSTEM_ERROR
        VersionMismatch = 0x14, // AUTH_VERSION_MISMATCH
        UnknownAccount  = 0x15, // AUTH_UNKNOWN_ACCOUNT
        SessionExpired  = 0x17, // AUTH_SESSION_EXPIRED
        WaitQueue       = 0x1B, // AUTH_WAIT_QUEUE
        Banned          = 0x1C, // AUTH_BANNED
        Suspended       = 0x20  // AUTH_SUSPENDED
    };

    /**
     * @brief Everything the client claimed in CMSG_AUTH_SESSION, decoded but not
     *        yet trusted.
     *
     * Wraps MopAuth::AuthSessionFields (the bounded, bug-for-bug decoder already
     * moved into proto) rather than re-declaring its fields: proto and the
     * game-side gateway both need the exact same scattered-digest/account/addon
     * shape, and MopAuthSession.h is the one place that shape is allowed to live.
     */
    struct AuthRequest
    {
        MopAuth::AuthSessionFields fields; ///< decoded via MopAuth::DecodeAuthSession
        std::string peerAddress;           ///< remote IP, for IP locking and ban checks
    };

    /**
     * @brief World-side state carried from LookupAccount() to Attach().
     *
     * proto never looks inside this; it only holds it across the proof step and
     * hands it back. Keeps the account row -- security level, expansion, mute
     * time, locale -- from having to be fetched twice, without any of those
     * fields appearing in an interface that has no business knowing they exist.
     */
    struct AuthContext
    {
        virtual ~AuthContext() {}
    };

    /**
     * @brief What the world knows about an account, once it has looked it up.
     */
    struct AuthLookup
    {
        AuthStatus status; ///< Ok only if the account may proceed to the proof

        /// Canonical raw-40 session key (MopAuth::SESSION_KEY_LEN bytes); meaningful
        /// only when status == Ok. NOT a BigNumber/hex string -- see MopAuthKey.h for
        /// why: BigNumber round-trips drop leading zero bytes on ~1/256 of logins.
        uint8 sessionKey[MopAuth::SESSION_KEY_LEN];

        /// Opaque, world-owned. Returned to Attach() untouched.
        std::shared_ptr<AuthContext> context;

        AuthLookup() : status(AuthStatus::UnknownAccount)
        {
            memset(sessionKey, 0, sizeof(sessionKey));
        }
    };

    /**
     * @brief The one and only seam between the protocol layer and the world.
     *
     * Everything above this line is wire format and cryptography; everything below
     * it is policy and persistence. The split is deliberate and total:
     *
     *   - The world performs every database query, every ban and IP-lock check,
     *     every configuration decision, and owns the WorldSession object.
     *   - proto performs every byte-level operation, owns the framing and the
     *     header cipher, and verifies the SHA-1 proof -- for which it needs the
     *     session key, and nothing else, from the world.
     *
     * proto therefore links neither game nor the database, which is what makes the
     * boundary real rather than a naming convention.
     *
     * Threading: LookupAccount() and Attach() are called on a network thread, one
     * connection at a time. Deliver() may be called concurrently for distinct
     * sessions and must be safe against the world thread draining them.
     */
    class IWorldGateway
    {
        public:

            virtual ~IWorldGateway() {}

            /**
             * @brief Resolve an account and decide whether it may log in at all.
             *
             * This is where the account row, bans, IP lock, allowed build,
             * security level and the sessionkey ladder (spec 5.1) are checked --
             * all of it world and database business. Returning anything other
             * than AuthStatus::Ok makes proto send that code and drop the
             * connection.
             */
            virtual AuthLookup LookupAccount(const AuthRequest& request) = 0;

            /**
             * @brief Infallible crypt-activation callback, run by World::AddSession
             *        while its add-queue lock is held.
             *
             * This is the other half of hazard H3's publication transaction. proto
             * prepares the cipher (AuthCrypt::Prepare()) before calling Attach(), but
             * must not activate it (AuthCrypt::Activate(); connection state ->
             * AUTHED) until the moment the session is actually committed -- otherwise
             * a failure inside Attach() (a DB error, a bad addon blob) would leave an
             * ACTIVE crypt on a connection whose session was never published. Only
             * proto's ClientConnection can perform that activation (the crypt lives
             * there, not in the gateway), so the callback has to cross the seam in
             * the opposite direction from everything else in this interface: proto
             * hands it to Attach(), and the gateway's implementation is expected to
             * forward it verbatim into World::AddSession(session, commit, context) --
             * see WorldSocket::CommitAuthenticatedSession for the shape this replaces.
             *
             * The gateway does not need to (and must not) understand what @p commit
             * does; it is opaque, exactly like World::SessionPublishCommit already is.
             */
            using AuthCommit = void (*)(void*) noexcept;

            /**
             * @brief Build the world-side session for a client that proved itself.
             *
             * Called only after LookupAccount() returned Ok and the SHA-1 proof
             * verified (MopAuth::ComputeAuthProof / ProofEquals), so the account is
             * authentic by the time the world commits any state to it. Owns the
             * WorldSession allocation, the fallible loads (LoadGlobalAccountData,
             * LoadTutorialsData, ReadAddonsInfo) and the World::AddSession
             * publication transaction -- see WorldSocket::HandleAuthSession for the
             * ordering this must preserve: every fallible step here runs BEFORE
             * @p commit is ever reachable, so no failure path can observe an
             * activated crypt.
             *
             * @param link          Shared rather than raw so a session outliving its
             *                      socket sends into a disarmed link instead of
             *                      freed memory.
             * @param commit        Infallible; MUST be forwarded into
             *                      World::AddSession(session, commit, commitContext)
             *                      as-is, invoked only if and when that call
             *                      actually publishes the session. Never call it
             *                      directly, and never call it on any failure path.
             * @param commitContext Opaque argument @p commit must be invoked with.
             * @return A handle for later Deliver()/OnPing()/Detach(), or
             *         INVALID_SESSION_ID if the world declined to create the
             *         session after all (in which case @p commit was NOT invoked).
             */
            virtual SessionId Attach(const AuthRequest& request,
                                     const std::shared_ptr<IClientLink>& link,
                                     const std::shared_ptr<AuthContext>& context,
                                     AuthCommit commit, void* commitContext) = 0;

            /// Hand a decoded packet to an attached session. proto has already
            /// framed and decrypted it and knows nothing about what it means.
            virtual void Deliver(SessionId session, WorldPacket&& packet) = 0;

            /**
             * @brief A client ping arrived.
             *
             * proto counts how many pings in a row arrived faster than a real
             * client sends them (WorldSocket::HandlePing's overspeed-ping check),
             * because that is a property of the stream. The world decides what the
             * run is worth: the threshold is configuration
             * (CONFIG_UINT32_MAX_OVERSPEED_PINGS) and staff accounts are exempt,
             * which is session state.
             *
             * @param session     The session, or INVALID_SESSION_ID if the peer
             *                    pinged before authenticating.
             * @param fastPingRun Consecutive suspiciously fast pings; zero once
             *                    the client returns to a normal cadence.
             * @return false to drop the connection.
             */
            virtual bool OnPing(SessionId session, uint32 latency, uint32 fastPingRun) = 0;

            /// The connection is gone; release the session. The world may keep the
            /// session alive past this call to unwind game state.
            virtual void Detach(SessionId session) = 0;

            /**
             * @brief Render the canonical SMSG_AUTH_RESPONSE error variant.
             *
             * proto cannot call MopAuth::BuildAuthResponseError() itself --
             * MopAuthResponse.cpp includes Opcodes.h and SharedDefines.h, which
             * drag the full game header surface -- so the finished packet crosses
             * the seam instead of the status code crossing alone. The game-side
             * WorldGateway implementation is expected to be a one-line forward to
             * that serializer; this method exists so proto never has to duplicate
             * it.
             *
             * @param status Never AuthStatus::Ok -- callers only reach this on a
             *               rejection.
             * @param out    Receives the packet; cleared first.
             */
            virtual void BuildAuthErrorResponse(AuthStatus status, WorldPacket& out) = 0;

            /**
             * @brief Scripting hook: a pre-dispatch opcode arrived that Deliver()
             *        never sees.
             *
             * WorldSocket.cpp handled CMSG_KEEP_ALIVE (:1119-1128) and
             * CMSG_LOG_DISCONNECT (:1129-1137) itself rather than queuing them to
             * WorldSession, but still fired Eluna's fire-and-forget OnPacketReceive
             * notification for both -- the null WorldSession* on the (structurally
             * impossible pre-auth, but Eluna already null-checks it) KEEP_ALIVE case
             * included. One hook covers both opcodes: neither the veto-vs-notify
             * shape nor the argument list differs between them.
             *
             * @param packet  The packet (KEEP_ALIVE's payload is empty;
             *                LOG_DISCONNECT's uint32 reason has already been read).
             * @param session The session, or INVALID_SESSION_ID if the peer has not
             *                authenticated yet (only reachable for KEEP_ALIVE).
             */
            virtual void OnPacketReceived(WorldPacket& /*packet*/,
                                          SessionId /*session*/) {}
    };
}

#endif
