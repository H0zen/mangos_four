file(READ "${SOURCE_ROOT}/src/game/Object/Unit.h" unit_header)
file(READ "${SOURCE_ROOT}/src/game/Object/UnitPower.cpp" power_source)
file(READ "${SOURCE_ROOT}/src/game/Object/Unit.cpp" unit_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/SpellAuraShapeshift.cpp" shapeshift_source)

if(MUTATION STREQUAL "mask_order")
    string(REPLACE "out.WriteGuidMask<4, 6, 7, 5, 2, 3, 0, 1>(guid);"
        "out.WriteGuidMask<6, 4, 7, 5, 2, 3, 0, 1>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "count_width")
    string(REPLACE "out.WriteBits(uint32(1), 21);"
        "out.WriteBits(uint32(1), 22);" unit_header "${unit_header}")
elseif(MUTATION STREQUAL "byte_order")
    string(REPLACE "out.WriteGuidBytes<7, 0, 5, 3, 1, 2, 4>(guid);"
        "out.WriteGuidBytes<0, 7, 5, 3, 1, 2, 4>(guid);"
        unit_header "${unit_header}")
elseif(MUTATION STREQUAL "primary_sender")
    string(REPLACE "MopCompactPackets::BuildPowerUpdate(data, GetObjectGuid(), uint8(power), uint32(val));"
        "/* removed primary power-update sender */" power_source "${power_source}")
elseif(MUTATION STREQUAL "switch_sender")
    string(REPLACE "MopCompactPackets::BuildPowerUpdate(data, GetObjectGuid(), uint8(new_powertype), reportedValue);"
        "/* removed power-type switch sender */" unit_source "${unit_source}")
elseif(MUTATION STREQUAL "registration")
    string(REPLACE "DefS(SMSG_POWER_UPDATE, \"SMSG_POWER_UPDATE\");"
        "/* removed SMSG_POWER_UPDATE registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist")
    string(REPLACE "case SMSG_POWER_UPDATE:"
        "case REMOVED_SMSG_POWER_UPDATE:" session_source "${session_source}")
elseif(MUTATION STREQUAL "switch_sender_gate")
    string(REPLACE
        "        if (IsInWorld())\n        {\n            WorldPacket data;\n            MopCompactPackets::BuildPowerUpdate(data, GetObjectGuid(), uint8(new_powertype), reportedValue);"
        "        {\n            WorldPacket data;\n            MopCompactPackets::BuildPowerUpdate(data, GetObjectGuid(), uint8(new_powertype), reportedValue);"
        unit_source "${unit_source}")
elseif(MUTATION STREQUAL "reported_value")
    string(REPLACE "                : GetPower(new_powertype);"
        "                : curValue;" unit_source "${unit_source}")
elseif(MUTATION STREQUAL "druid_form_exit_guard")
    string(REPLACE
        "if (target->getClass() == CLASS_DRUID && target->GetPowerType() != POWER_MANA)"
        "if (target->getClass() == CLASS_DRUID)"
        shapeshift_source "${shapeshift_source}")
elseif(MUTATION STREQUAL "reference")
    string(REPLACE "SMSG_POWER_UPDATE                              0x109F  ACTIVE"
        "SMSG_POWER_UPDATE                              0x109F  DORMANT"
        opcode_reference "${opcode_reference}")
endif()

function(require_once source token context)
    string(FIND "${source}" "${token}" first)
    if(first EQUAL -1)
        message(FATAL_ERROR "${context}: required token missing: ${token}")
    endif()
    math(EXPR next "${first} + 1")
    string(SUBSTRING "${source}" ${next} -1 tail)
    string(FIND "${tail}" "${token}" duplicate)
    if(NOT duplicate EQUAL -1)
        message(FATAL_ERROR "${context}: duplicate token: ${token}")
    endif()
endfunction()

require_once("${unit_header}"
    "inline void BuildPowerUpdate(WorldPacket& out, ObjectGuid guid,"
    "shared power-update builder")
require_once("${unit_header}"
    "out.WriteGuidMask<4, 6, 7, 5, 2, 3, 0, 1>(guid);"
    "18414 power-update GUID mask")
require_once("${unit_header}"
    "out.WriteBits(uint32(1), 21);"
    "18414 power-update record count")
require_once("${unit_header}"
    "out.WriteGuidBytes<7, 0, 5, 3, 1, 2, 4>(guid);"
    "18414 power-update leading GUID bytes")
require_once("${unit_header}"
    "out << powerType;\n        out << value;\n        out.WriteGuidBytes<6>(guid);"
    "18414 power-update record and trailing GUID byte")
require_once("${power_source}"
    "MopCompactPackets::BuildPowerUpdate(data, GetObjectGuid(), uint8(power), uint32(val));"
    "normal power-change sender")
require_once("${unit_source}"
    "MopCompactPackets::BuildPowerUpdate(data, GetObjectGuid(), uint8(new_powertype), reportedValue);"
    "power-type switch sender")
# Ungated, this fires during Player::LoadFromDB -> InitStatsForLevel ->
# InitDataForForm, before the client knows the unit exists, and lands as the
# first SMSG after CMSG_PLAYER_LOGIN - the slot retail gives to
# SMSG_ACCOUNT_DATA_TIMES. Player::SendMessageToSet delivers the self copy
# regardless of IsInWorld(), so nothing else holds it back.
# POWER_MANA is deliberately left untouched by the block above, so curValue
# there is GetCreatePowers(POWER_MANA) - the base pool, not what the player
# holds. Reporting the field keeps both branches honest.
require_once("${unit_source}"
    "            (GetPowerIndex(new_powertype) == INVALID_POWER_INDEX)
                ? curValue
                : GetPower(new_powertype);"
    "power-type switch reports the real field value, with the legacy fallback for a class-unsupported power")
# Druid forms that already display mana must not re-announce an unchanged
# power type when they end.
require_once("${shapeshift_source}"
    "if (target->getClass() == CLASS_DRUID && target->GetPowerType() != POWER_MANA)"
    "druid form-exit power-type change guard")
require_once("${unit_source}"
    "        if (IsInWorld())\n        {\n            WorldPacket data;\n            MopCompactPackets::BuildPowerUpdate(data, GetObjectGuid(), uint8(new_powertype), reportedValue);"
    "power-type switch sender in-world gate")
require_once("${opcode_registry}"
    "DefS(SMSG_POWER_UPDATE, \"SMSG_POWER_UPDATE\");"
    "power-update registration")
require_once("${session_source}" "case SMSG_POWER_UPDATE:"
    "power-update converted-packet admission")
require_once("${opcode_reference}"
    "SMSG_POWER_UPDATE                              0x109F  ACTIVE"
    "active direct-client reference")

foreach(source IN ITEMS "${power_source}" "${unit_source}")
    if(source MATCHES "WorldPacket data\\(SMSG_POWER_UPDATE\\)")
        message(FATAL_ERROR "legacy power-update serializer remains")
    endif()
endforeach()
