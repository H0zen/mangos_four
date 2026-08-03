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

# Geometry is header-only and sits below everything else: it knows nothing
# about WoW, so both the server and the offline baker link it.
add_subdirectory(shared/Geometry)
add_subdirectory(shared/terrain)

# Build the mangos shared library
add_subdirectory(shared)

# The 5.4.8 world protocol. Sits between shared and game: it links shared, and
# game links it back for the relocated Mop* codec/auth symbols WorldSocket.cpp
# still calls. See src/proto/CMakeLists.txt for the link-boundary rule.
add_subdirectory(proto)

add_subdirectory(genrev)

# Needs to link against mangos_world.lib
if(BUILD_MANGOSD)
    # Build the mangos game library
    add_subdirectory(game)
endif()

if(BUILD_MANGOSD)
    # Build the mangos world server
    add_subdirectory(mangosd)

    # Build the selected modules
    add_subdirectory(modules)
endif()

# Build the mangos realm authentication server
if(BUILD_REALMD)
    add_subdirectory(realmd)
endif()

# The client-data parsers are configured whenever the TESTS need them, not only for
# the tools: mangos_tests links extractor_client and extractor_data and nothing else
# of the baker. The baker executables themselves stay gated on BUILD_TOOLS, inside.
if(BUILD_TOOLS OR WITH_TESTS)
    add_subdirectory(tools)
endif()

if(WITH_TESTS)
    add_subdirectory(tests)
endif()

if (BUILD_MANGOSD OR BUILD_REALMD)
    if(WIN32)
        get_filename_component(MYSQL_LIB_DIR ${MySQL_LIBRARIES} DIRECTORY)
        get_filename_component(MYSQL_LIB_DLL ${MySQL_LIBRARIES} NAME)
        STRING(REPLACE ".lib" ".dll" MYSQL_LIB_DLL ${MYSQL_LIB_DLL})
        install(FILES ${MYSQL_LIB_DIR}/${MYSQL_LIB_DLL} DESTINATION ${BIN_DIR})
    endif()
endif()
