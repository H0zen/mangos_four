file(READ "${SOURCE_ROOT}/src/game/Object/ObjectUpdate.cpp" update_source)

if(MUTATION STREQUAL "drop_bytes1")
    string(REPLACE
        "add(18, object.GetUInt32Value(GAMEOBJECT_BYTES_1));"
        ""
        update_source "${update_source}")
elseif(MUTATION STREQUAL "bytes1_index_drift")
    string(REPLACE
        "add(18, object.GetUInt32Value(GAMEOBJECT_BYTES_1));"
        "add(19, object.GetUInt32Value(GAMEOBJECT_BYTES_1));"
        update_source "${update_source}")
elseif(MUTATION STREQUAL "bytes1_wrong_source")
    string(REPLACE
        "add(18, object.GetUInt32Value(GAMEOBJECT_BYTES_1));"
        "add(18, object.GetUInt32Value(GAMEOBJECT_LEVEL));"
        update_source "${update_source}")
elseif(MUTATION STREQUAL "short_rotation_loop")
    string(REPLACE
        "for (uint16 i = 0; i < 4; ++i) add(uint16(12 + i), object.GetUInt32Value(GAMEOBJECT_PARENTROTATION + i));"
        "for (uint16 i = 0; i < 3; ++i) add(uint16(12 + i), object.GetUInt32Value(GAMEOBJECT_PARENTROTATION + i));"
        update_source "${update_source}")
elseif(MUTATION STREQUAL "stale_reserve")
    string(REPLACE
        "fields.reserve(19);"
        "fields.reserve(18);"
        update_source "${update_source}")
endif()

string(FIND "${update_source}" "void BuildMopGameObjectStaticFields" go_start)
string(FIND "${update_source}" "void BuildMopObserverPlayerStaticFields" next_start)
if(go_start EQUAL -1 OR next_start EQUAL -1 OR NOT go_start LESS next_start)
    message(FATAL_ERROR "could not isolate BuildMopGameObjectStaticFields")
endif()
math(EXPR go_length "${next_start} - ${go_start}")
string(SUBSTRING "${update_source}" ${go_start} ${go_length} go_body)

# The 18414 gameobject descriptor is OBJECT_END(8) + GAMEOBJECT_END(0x0C) with
# GAMEOBJECT_DYNAMIC lifted into the object block at wire 6, giving a contiguous
# wire range 0..18 and nothing else. The client indexes this array directly, so
# an omitted slot is not a smaller packet, it is a field the client reads as
# zero. Wire 18 is GAMEOBJECT_BYTES_1, whose byte 1 is the GAMEOBJECT_TYPE.
set(EXPECTED_LITERAL_INDICES "0,1,2,3,4,5,6,7,8,9,10,11,16,17,18")

string(REGEX MATCHALL "add\\([0-9]+," raw_adds "${go_body}")
set(literal_indices "")
foreach(one_add IN LISTS raw_adds)
    string(REGEX REPLACE "add\\(([0-9]+),.*" "\\1" one_index "${one_add}")
    list(APPEND literal_indices ${one_index})
endforeach()
string(REPLACE ";" "," actual_literal_indices "${literal_indices}")

if(NOT actual_literal_indices STREQUAL EXPECTED_LITERAL_INDICES)
    message(FATAL_ERROR
        "gameobject wire projection changed shape\n"
        "  expected literal indices: ${EXPECTED_LITERAL_INDICES}\n"
        "  actual literal indices:   ${actual_literal_indices}")
endif()

# 12..15 are emitted by a loop rather than four literals, so the literal list
# above legitimately skips them. Pin the loop so the gap stays covered.
string(FIND "${go_body}"
    "for (uint16 i = 0; i < 4; ++i) add(uint16(12 + i), object.GetUInt32Value(GAMEOBJECT_PARENTROTATION + i));"
    rotation_loop)
if(rotation_loop EQUAL -1)
    message(FATAL_ERROR
        "GAMEOBJECT_PARENTROTATION must fill wire indices 12..15 as four entries")
endif()

# Byte 1 of GAMEOBJECT_BYTES_1 is the GAMEOBJECT_TYPE. Without it every
# gameobject reads as type 0 on the client, and the WMO-backed types
# (11 TRANSPORT, 14 MAP_OBJECT, 15 MO_TRANSPORT, 33 DESTRUCTIBLE_BUILDING)
# lose the branch that keeps their model out of the M2 cache.
string(FIND "${go_body}"
    "add(18, object.GetUInt32Value(GAMEOBJECT_BYTES_1));" bytes1)
if(bytes1 EQUAL -1)
    message(FATAL_ERROR
        "wire index 18 must carry GAMEOBJECT_BYTES_1, the field holding GAMEOBJECT_TYPE")
endif()

# Both producers - the create block and the values block - build from the same
# projection, so a reserve left behind is a reliable sign one of them was
# updated and the other was not.
string(FIND "${update_source}" "fields.reserve(18);" stale_reserve)
if(NOT stale_reserve EQUAL -1)
    message(FATAL_ERROR
        "a gameobject field reserve still says 18; the projection emits 19")
endif()

# No trailing semicolon in the pattern: a match containing ";" would split into
# two CMake list elements and double the count.
string(REGEX MATCHALL "fields\\.reserve\\(19\\)" reserves "${update_source}")
list(LENGTH reserves reserve_count)
if(NOT reserve_count EQUAL 2)
    message(FATAL_ERROR
        "expected the create and values producers to reserve 19 gameobject fields, found ${reserve_count}")
endif()
