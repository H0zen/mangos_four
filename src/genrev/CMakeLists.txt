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

# `cmake -P` runs with no project cache, so nothing configured here reaches the
# script unless it is passed explicitly. GIT_EXECUTABLE is the important one:
# without it GenRevision.cmake skips revision discovery entirely and falls back to
# "unknown"/"Archived" with empty submodule fields. That was survivable only while
# the script wrote somewhere nothing compiles; now that it writes the header the
# compiler reads, those values would be stamped into every build.
#
# CMAKE_HOST_SYSTEM_VERSION is in the same category and easy to miss: script mode
# leaves it empty, and revision_data.h.in substitutes it into CMAKE_HOST_SYSTEM,
# so without it the build-time run degrades "Windows 10.0.26200" to "Windows ".
# CMAKE_VERSION and CMAKE_HOST_SYSTEM_NAME are the only other substituted values
# not computed inside the script, and cmake -P does set both.
#
# VERBATIM matters here because two of these are paths that routinely contain
# spaces - a default Windows Git lives under "C:/Program Files/Git" - and without
# it the escaping is generator-specific and can arrive as literal data.
set(REVISION_DATA_HEADER "${CMAKE_BINARY_DIR}/src/shared/revision_data.h")

add_custom_target(revision_data.h ALL
  COMMAND "${CMAKE_COMMAND}"
    -DBUILDDIR=${CMAKE_BINARY_DIR}
    -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
    -DWITHOUT_GIT=${WITHOUT_GIT}
    -DSCRIPT_LIB_ELUNA=${SCRIPT_LIB_ELUNA}
    -DSCRIPT_LIB_SD3=${SCRIPT_LIB_SD3}
    -DCMAKE_HOST_SYSTEM_VERSION=${CMAKE_HOST_SYSTEM_VERSION}
    -P "${CMAKE_SOURCE_DIR}/cmake/GenRevision.cmake" "${CMAKE_BINARY_DIR}"
  BYPRODUCTS "${REVISION_DATA_HEADER}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Generating revision_data.h"
  VERBATIM
)

# Declaring the header a byproduct is what tells the generator this command is the
# thing that produces it, rather than leaving it an untracked file that happens to
# appear. Ninja in particular needs that edge: without it the header is unknown to
# the graph, and the ordering edge below only sequences the command, it does not
# mark anything dirty.
set_source_files_properties("${REVISION_DATA_HEADER}" PROPERTIES GENERATED TRUE)

# Without an explicit edge this is just another ALL target, so the build tool is
# free to compile the consumers before - or alongside - the command that rewrites
# the header, letting one invocation finish carrying the previous revision. shared
# is the root of the link graph here (game, mangosd and the modules all reach it),
# so ordering shared after the generator orders every consumer with it. `shared` is
# already defined at this point: src/CMakeLists.txt adds it before genrev.
if(TARGET shared)
  add_dependencies(shared revision_data.h)
endif()
