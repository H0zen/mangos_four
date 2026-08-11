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
# The terrain and client-parser tests, ported from 03 alongside the engine they
# cover. They read synthetic byte buffers through MemoryArchive, so they need no
# client install and no database -- which is what lets them run in CI where the
# 5.4.8 corpus cannot.
#
# The 5.4.8-specific cases live in ClientParserTest: the MCVT sub-chunk scan, the
# 64-bit hole map, and LiquidObject 42. Those three were established by measuring
# a real client; these are what keep them from regressing afterwards.
#
# No test framework is vendored; see TestHarness.h.
# =============================================================================

set(SRC_GRP_TESTS
    main.cpp
    TestHarness.h
    ClientParserTest.cpp
    TerrainModelTest.cpp
    TileSerializerTest.cpp
    ModelMapTest.cpp
    NavBinningTest.cpp
    DynamicCollisionTest.cpp
    PlacementTest.cpp
    GeometryMathTest.cpp
    # Compiled in, not linked from `game`: game.lib pulls the whole server, down to
    # the database globals that only mangosd defines. These two know nothing of it.
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers/DynamicCollision.cpp
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers/GameObjectModel.cpp
)

source_group("tests" FILES ${SRC_GRP_TESTS})

add_executable(mangos_tests ${SRC_GRP_TESTS})

target_include_directories(mangos_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers
        ${CMAKE_SOURCE_DIR}/src/game/Server)

target_link_libraries(mangos_tests
    PRIVATE
        extractor_client
        extractor_data
        terrain
        geometry
        Threads::Threads
)

set_target_properties(mangos_tests PROPERTIES FOLDER "tests")

add_test(NAME mangos_tests COMMAND mangos_tests)

# The Placement refactor's guard: no GetPositionX/Y/Z, no GetDistance on the hierarchy,
# Where() instead. Source-level, because it needs no build -- see the file's header.
add_test(NAME spatial_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckSpatialBoundary.cmake)
