# Keeps the data-file build list and the client-wire build list separate.
#
# The 5.4.8.18414 client tags its own MPQ content 18273 -- dbc/component.wow-<locale>.txt reads
# version="18273" and every extracted .map carries buildMagic 18273 -- so DBC/DB2 loading and
# GridMap must accept 18273 or the server cannot start against its own extraction.
#
# The CLIENT is a different question. Every opcode value, bit-packed body and handler here was
# recovered against 18414 specifically, and opcode numbers are reassigned between builds (0x023A
# is a 7-byte SMSG in 18414 but a 37-86 byte CMSG in 17359/17371/17399). Admitting a client of a
# different build would serve it a table it cannot parse.
#
# Both facts were true while a SINGLE list served both call sites, which meant an 18273 client was
# admitted to the world. That is what this gate prevents recurring: the obvious "cleanup" is to
# notice two near-identical lists and merge them, and merging them in either direction breaks
# something -- narrow the data list and the server will not boot, widen the wire list and it
# admits clients it cannot talk to.
#
# The wire value is corpus-confirmed, not assumed: CMSG_AUTH_SESSION offset 44 reads EE 47 = 18414
# in capture-000019 seq 2, and offset 44 is where MopAuthSession parses builtNumberClient.
#
# Run:
#   cmake -DSOURCE_ROOT=<repo> -P mop_client_build_split_source_test.cmake

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT must be set")
endif()

set(_shared  "${SOURCE_ROOT}/src/game/Server/SharedDefines.h")
set(_stores  "${SOURCE_ROOT}/src/game/Server/DBCStores.cpp")
set(_gateway "${SOURCE_ROOT}/src/game/Server/WorldGateway.cpp")
set(_gridmap "${SOURCE_ROOT}/src/game/WorldHandlers/GridMap.cpp")
foreach(_f "${_shared}" "${_stores}" "${_gateway}" "${_gridmap}")
    if(NOT EXISTS "${_f}")
        message(FATAL_ERROR "missing source: ${_f}")
    endif()
endforeach()

macro(strip_comments _var)
    string(REPLACE "\r\n" "\n" ${_var} "${${_var}}")
    string(REGEX REPLACE "/\\*([^*]|\\*+[^*/])*\\*+/" "" ${_var} "${${_var}}")
    string(REGEX REPLACE "//[^\n]*" "" ${_var} "${${_var}}")
endmacro()

file(READ "${_shared}" _shared_src)
strip_comments(_shared_src)
file(READ "${_stores}" _stores_src)
strip_comments(_stores_src)
file(READ "${_gateway}" _gateway_src)
strip_comments(_gateway_src)
file(READ "${_gridmap}" _gridmap_src)
strip_comments(_gridmap_src)

set(_blobs _shared_src _stores_src _gateway_src _gridmap_src)
if(DEFINED MUTATION)
    foreach(_b IN LISTS _blobs)
        set(_pre_${_b} "${${_b}}")
    endforeach()

    if(MUTATION STREQUAL "merge_wire_into_data")
        # The tempting cleanup: point client admission back at the data-file list.
        string(REPLACE "IsAcceptableClientWireBuild(request.fields.builtNumberClient)"
                       "IsAcceptableClientBuild(request.fields.builtNumberClient)"
                       _gateway_src "${_gateway_src}")
    elseif(MUTATION STREQUAL "widen_wire_list")
        string(REPLACE "EXPECTED_MANGOSD_WIRE_BUILD          {18414, 0}"
                       "EXPECTED_MANGOSD_WIRE_BUILD          {18273, 18414, 0}"
                       _shared_src "${_shared_src}")
    elseif(MUTATION STREQUAL "narrow_data_list")
        # Would stop the server booting against its own 18273-tagged extraction.
        string(REPLACE "EXPECTED_MANGOSD_CLIENT_BUILD        {18273, 18414, 0}"
                       "EXPECTED_MANGOSD_CLIENT_BUILD        {18414, 0}"
                       _shared_src "${_shared_src}")
    elseif(MUTATION STREQUAL "drop_wire_predicate")
        string(REPLACE "bool IsAcceptableClientWireBuild(uint32 build)"
                       "bool IsAcceptableClientWireBuild_removed(uint32 build)"
                       _stores_src "${_stores_src}")
    elseif(MUTATION STREQUAL "gridmap_uses_wire_list")
        # GridMap must keep the data list; the maps are 18273.
        string(REPLACE "IsAcceptableClientBuild(header.buildMagic)"
                       "IsAcceptableClientWireBuild(header.buildMagic)"
                       _gridmap_src "${_gridmap_src}")
    else()
        message(FATAL_ERROR "unknown MUTATION '${MUTATION}'")
    endif()

    set(_applied FALSE)
    foreach(_b IN LISTS _blobs)
        if(NOT "${${_b}}" STREQUAL "${_pre_${_b}}")
            set(_applied TRUE)
        endif()
    endforeach()
    if(NOT _applied)
        message(STATUS "MUTATION '${MUTATION}' rewrote nothing -- dead arm, exiting 0 so WILL_FAIL reports it")
        return()
    endif()
endif()

# ---------------------------------------------------------------------------
# 1. Two distinct lists must exist, with the right contents.
# ---------------------------------------------------------------------------
string(FIND "${_shared_src}" "EXPECTED_MANGOSD_CLIENT_BUILD        {18273, 18414, 0}" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The data-file build list is not {18273, 18414, 0}.\n\n"
        "The 18414 client tags its own content 18273: dbc/component.wow-<locale>.txt reads\n"
        "version=\"18273\" and every extracted .map carries buildMagic 18273. Narrowing this list\n"
        "to 18414 makes LoadDBCStores reject the DBCs and GridMap reject every map, so the server\n"
        "will not start against its own extraction.")
endif()

string(FIND "${_shared_src}" "EXPECTED_MANGOSD_WIRE_BUILD          {18414, 0}" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "The client-wire build list is not {18414, 0}.\n\n"
        "Every opcode value and packet body in this core is 18414-specific, and opcode numbers\n"
        "are reassigned between builds. Widening this list admits a client that cannot parse what\n"
        "we send it. A real 18414 client reports 18414: CMSG_AUTH_SESSION offset 44 reads EE 47\n"
        "in capture-000019 seq 2.")
endif()

# ---------------------------------------------------------------------------
# 2. Each call site must use the right one.
# ---------------------------------------------------------------------------
string(FIND "${_stores_src}" "bool IsAcceptableClientWireBuild(uint32 build)" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR "IsAcceptableClientWireBuild is gone; client admission has nothing narrow to call.")
endif()

string(FIND "${_gateway_src}" "IsAcceptableClientWireBuild(request.fields.builtNumberClient)" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "Client admission is not using the wire-build predicate.\n\n"
        "WorldGateway validates a CONNECTING CLIENT. Using the data-file list here admits an\n"
        "18273 client into an 18414 world -- it would be sent an opcode table it cannot parse.")
endif()

string(FIND "${_gridmap_src}" "IsAcceptableClientBuild(header.buildMagic)" _at)
if(_at EQUAL -1)
    message(FATAL_ERROR
        "GridMap is not using the data-file predicate.\n\n"
        "Map files carry buildMagic 18273. Checking them against the wire list rejects every map\n"
        "on disk.")
endif()

message(STATUS "client build split: data list {18273,18414}, wire list {18414}, "
               "gateway on wire predicate, GridMap on data predicate")
