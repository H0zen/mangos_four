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

# The unified baker. One tool reads the client and writes everything mangosd
# needs -- the DBC set, the fused terrain + collision tiles, the game-object
# models and the navmesh -- replacing the map-extractor / vmap-extractor /
# Movemap-Generator trio and its shell scripts.
add_subdirectory(extractor)

# Scores those tiles against the world database's own ground-only creature spawns.
# A clean parse is not correct geometry; this is what says whether it is.
if(BUILD_TOOLS)
    add_subdirectory(height-check)
endif()
