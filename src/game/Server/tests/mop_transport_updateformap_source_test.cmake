cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Transports.cpp" transports_source)
set(original_transports_source "${transports_source}")

if(MUTATION STREQUAL "create_sequential_read")
    string(REPLACE
        "if (packet.read<uint16>(0) != itr->getSource()->GetMapId())"
        "if (packet.ReadUInt16() != itr->getSource()->GetMapId())"
        transports_source "${transports_source}")
elseif(MUTATION STREQUAL "out_sequential_read")
    string(REPLACE
        "if (out_packet.read<uint16>(0) != itr->getSource()->GetMapId())"
        "if (out_packet.ReadUInt16() != itr->getSource()->GetMapId())"
        transports_source "${transports_source}")
elseif(MUTATION STREQUAL "out_current_map")
    string(REPLACE
        "UpdateData transData(targetMap->GetId());\n        BuildOutOfRangeUpdateBlock(&transData);"
        "UpdateData transData(GetMapId());\n        BuildOutOfRangeUpdateBlock(&transData);"
        transports_source "${transports_source}")
elseif(MUTATION STREQUAL "mismatch_return")
    string(REPLACE
        "if (out_packet.read<uint16>(0) != itr->getSource()->GetMapId())\n                {\n                    continue;\n                }"
        "if (out_packet.read<uint16>(0) != itr->getSource()->GetMapId())\n                {\n                    return;\n                }"
        transports_source "${transports_source}")
elseif(DEFINED MUTATION AND NOT MUTATION STREQUAL "")
    message(FATAL_ERROR "unknown mutation: ${MUTATION}")
endif()

if(DEFINED MUTATION AND NOT MUTATION STREQUAL "" AND
        transports_source STREQUAL original_transports_source)
    message(FATAL_ERROR "mutation did not alter its intended source seam: ${MUTATION}")
endif()

string(REGEX MATCHALL "UpdateData[ \t]+transData[(]targetMap->GetId[(][)][)]" recipient_map_constructors
    "${transports_source}")
list(LENGTH recipient_map_constructors recipient_map_constructor_count)
if(NOT recipient_map_constructor_count EQUAL 2)
    message(FATAL_ERROR
        "Transport::UpdateForMap must build exactly two recipient-map UpdateData objects; found ${recipient_map_constructor_count}")
endif()

foreach(required IN ITEMS
        "packet.read<uint16>(0)"
        "out_packet.read<uint16>(0)")
    string(FIND "${transports_source}" "${required}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "missing non-mutating transport packet map check: ${required}")
    endif()
endforeach()

foreach(forbidden IN ITEMS
        "packet.ReadUInt16()"
        "out_packet.ReadUInt16()")
    string(FIND "${transports_source}" "${forbidden}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "stateful transport packet map check remains: ${forbidden}")
    endif()
endforeach()

string(FIND "${transports_source}"
    "if (packet.read<uint16>(0) != itr->getSource()->GetMapId())\n                {\n                    continue;\n                }"
    create_guard)
if(create_guard EQUAL -1)
    message(FATAL_ERROR "create-map mismatch must continue to the next observer")
endif()

string(FIND "${transports_source}"
    "if (out_packet.read<uint16>(0) != itr->getSource()->GetMapId())\n                {\n                    continue;\n                }"
    out_guard)
if(out_guard EQUAL -1)
    message(FATAL_ERROR "out-of-range map mismatch must continue to the next observer")
endif()
