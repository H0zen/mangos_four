file(READ "${SOURCE_ROOT}/src/game/Object/GameObject.cpp" go_source)

if(MUTATION STREQUAL "restore_fallthrough")
    string(REPLACE
        "            break;\n        case GAMEOBJECT_TYPE_TRANSPORT:"
        "        case GAMEOBJECT_TYPE_TRANSPORT:"
        go_source "${go_source}")
elseif(MUTATION STREQUAL "drop_destructible_data")
    string(REPLACE
        "SetUInt32Value(GAMEOBJECT_PARENTROTATION, m_goInfo->destructibleBuilding.destructibleData);"
        ""
        go_source "${go_source}")
endif()

string(FIND "${go_source}" "case GAMEOBJECT_TYPE_DESTRUCTIBLE_BUILDING:" destructible_start)
string(FIND "${go_source}" "case GAMEOBJECT_TYPE_TRANSPORT:" transport_start)
if(destructible_start EQUAL -1 OR transport_start EQUAL -1
   OR NOT destructible_start LESS transport_start)
    message(FATAL_ERROR
        "could not isolate the destructible arm of GameObject::Create")
endif()
math(EXPR arm_length "${transport_start} - ${destructible_start}")
string(SUBSTRING "${go_source}" ${destructible_start} ${arm_length} destructible_arm)

# GameObjectInfo is a union. transport.startOpen and
# destructibleBuilding.creditProxyCreature are both data word 1, so falling
# through into the transport arm makes a destructible's credit proxy decide
# whether it spawns GO_STATE_ACTIVE. The transport arm also writes getMSTime()
# into GAMEOBJECT_LEVEL, which no reader consumes but which the 18414
# projection sends at wire index 17.
# Counting breaks is not enough: "if (x) break;" would satisfy a count and
# still fall through. Require the arm to END in a break that is alone on its
# line, and forbid any conditional in the arm, so the break cannot be guarded
# by one. Scalar REGEX MATCH, not MATCHALL, so the ";" cannot split the result
# into extra list elements.
string(REGEX MATCH "[\r\n][ \t]*break;[ \t\r\n]*$" terminal_break "${destructible_arm}")
if(terminal_break STREQUAL "")
    message(FATAL_ERROR
        "the destructible arm must end in an unconditional break on its own "
        "line so it cannot fall into GAMEOBJECT_TYPE_TRANSPORT")
endif()

string(REGEX MATCHALL "[^a-zA-Z_]if[ \t]*\\(" arm_conditionals "${destructible_arm}")
list(LENGTH arm_conditionals conditional_count)
if(NOT conditional_count EQUAL 0)
    message(FATAL_ERROR
        "the destructible arm must stay unconditional; a branch here could "
        "guard the terminal break and reopen the fall-through")
endif()

string(FIND "${destructible_arm}" "ForceGameObjectHealth" health)
if(health EQUAL -1)
    message(FATAL_ERROR "the destructible arm must still initialise health")
endif()

string(FIND "${destructible_arm}"
    "m_goInfo->destructibleBuilding.destructibleData" destructible_data)
if(destructible_data EQUAL -1)
    message(FATAL_ERROR
        "GAMEOBJECT_PARENTROTATION must carry destructibleData; the client keys "
        "DestructibleModelData off exactly that slot")
endif()

# getMSTime() belongs to the transport arm alone. Its appearance in the
# destructible arm would mean the fall-through came back.
string(FIND "${destructible_arm}" "getMSTime" leaked_timestamp)
if(NOT leaked_timestamp EQUAL -1)
    message(FATAL_ERROR
        "the destructible arm must not reach the transport timestamp write")
endif()
