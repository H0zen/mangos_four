# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# =============================================================================
# proto -- the 5.4.8 MoP world protocol
#
# Framing, the header cipher, the CMSG_AUTH_SESSION decode/proof, and the seam
# interfaces to the world (IWorldGateway/IClientLink). Talks to the world
# exclusively through IWorldGateway.
#
# proto does NOT link game, and does NOT link the database: if someone reaches
# for sWorld, sObjectMgr or LoginDatabase from in here, the build fails rather
# than quietly re-tangling the two layers.
#
# The Mop* files below are RELOCATED from src/game/Server/, not rewritten --
# they were already game-free (std-only, or shared-only via Common/Auth/
# Utilities headers). MopAuthResponse.{h,cpp} stays in src/game/Server/: its
# .cpp includes Opcodes.h and SharedDefines.h for the canonical
# SMSG_AUTH_RESPONSE serializer, which drags the full game header surface.
# proto reaches that serializer through IWorldGateway::BuildAuthErrorResponse()
# instead of linking it directly.
#
# Stage 2 CP3: proto::ClientConnection/Listener now carry live world traffic;
# WorldSocket/WorldSocketMgr are deleted. MopSocketDrain.h is gone too (hazard
# H4) -- its DrainState/ShouldCloseNow/InputStatus decisions encoded
# ACE_TP_Reactor's suspend/resume dispatch-window reasoning, which the net::
# engine (src/shared/net/) has no counterpart for; both its backends
# (ReactorServer, IocpServer) already guarantee an auth rejection's response
# reaches the wire before closing, verified from their own source. See
# ClientConnection::RejectAuth()'s comment for the resolution in full.
# =============================================================================

set(SRC_GRP_PROTO
    ClientConnection.cpp
    ClientConnection.h
    IClientLink.h
    IWorldGateway.h
    Listener.cpp
    Listener.h
    MopAuthProof.cpp
    MopAuthProof.h
    MopAuthSession.cpp
    MopAuthSession.h
    MopFrameReader.h
    MopHandshake.h
    MopWireCodec.h
)
source_group("proto" FILES ${SRC_GRP_PROTO})

add_library(proto STATIC ${SRC_GRP_PROTO})

target_include_directories(proto
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(proto
    PUBLIC
        shared
    PRIVATE
        mangos_openssl_strict
)
