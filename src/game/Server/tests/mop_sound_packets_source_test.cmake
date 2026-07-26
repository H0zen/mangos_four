file(READ "${SOURCE_ROOT}/src/game/Object/Object.h" object_header)
file(READ "${SOURCE_ROOT}/src/game/Object/WorldObjectSummon.cpp" world_object_source)
file(READ "${SOURCE_ROOT}/src/game/WorldHandlers/Map.cpp" map_source)
file(READ "${SOURCE_ROOT}/src/game/BattleGround/BattleGroundMgr.cpp" battleground_source)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.cpp" opcode_registry)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes.h" opcode_header)
file(READ "${SOURCE_ROOT}/src/game/Server/Opcodes_reference.h" opcode_reference)
file(READ "${SOURCE_ROOT}/src/game/Server/WorldSession.cpp" session_source)

if(MUTATION STREQUAL "direct_mask")
    string(REPLACE "out.WriteGuidMask<2, 3, 7, 6, 0, 5, 4, 1>(sourceGuid);"
        "out.WriteGuidMask<3, 2, 7, 6, 0, 5, 4, 1>(sourceGuid);"
        object_header "${object_header}")
elseif(MUTATION STREQUAL "direct_bytes")
    string(REPLACE "out.WriteGuidBytes<3, 2, 4, 7, 5, 0, 6, 1>(sourceGuid);"
        "out.WriteGuidBytes<2, 3, 4, 7, 5, 0, 6, 1>(sourceGuid);"
        object_header "${object_header}")
elseif(MUTATION STREQUAL "object_mask")
    string(REPLACE "out.WriteGuidMask<7, 2, 4, 3>(targetGuid);"
        "out.WriteGuidMask<2, 7, 4, 3>(targetGuid);"
        object_header "${object_header}")
elseif(MUTATION STREQUAL "object_bytes")
    string(REPLACE "out.WriteGuidBytes<7, 5, 3, 1>(sourceGuid);"
        "out.WriteGuidBytes<5, 7, 3, 1>(sourceGuid);"
        object_header "${object_header}")
elseif(MUTATION STREQUAL "worldobject_sound_sender")
    string(REPLACE "MopSoundPackets::BuildPlaySound(data, sound_id, ObjectGuid());"
        "/* removed WorldObject direct-sound builder */"
        world_object_source "${world_object_source}")
elseif(MUTATION STREQUAL "map_sound_sender")
    string(REPLACE "MopSoundPackets::BuildPlaySound(data, soundId, ObjectGuid());"
        "/* removed map direct-sound builder */" map_source "${map_source}")
elseif(MUTATION STREQUAL "battleground_sound_sender")
    string(REPLACE "MopSoundPackets::BuildPlaySound(*data, soundid, ObjectGuid());"
        "/* removed battleground direct-sound builder */"
        battleground_source "${battleground_source}")
elseif(MUTATION STREQUAL "object_sound_sender")
    string(REPLACE "MopSoundPackets::BuildPlayObjectSound("
        "/* removed object-sound builder */ ("
        world_object_source "${world_object_source}")
elseif(MUTATION STREQUAL "music_sender")
    string(REPLACE "MopSoundPackets::BuildPlayMusic(data, sound_id);"
        "/* removed music builder */" world_object_source "${world_object_source}")
elseif(MUTATION STREQUAL "registration_sound")
    string(REPLACE "DefS(SMSG_PLAY_SOUND, \"SMSG_PLAY_SOUND\");"
        "/* removed SMSG_PLAY_SOUND registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "registration_object")
    string(REPLACE "DefS(SMSG_PLAY_OBJECT_SOUND, \"SMSG_PLAY_OBJECT_SOUND\");"
        "/* removed SMSG_PLAY_OBJECT_SOUND registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "registration_music")
    string(REPLACE "DefS(SMSG_PLAY_MUSIC, \"SMSG_PLAY_MUSIC\");"
        "/* removed SMSG_PLAY_MUSIC registration */" opcode_registry "${opcode_registry}")
elseif(MUTATION STREQUAL "allowlist_sound")
    string(REPLACE "case SMSG_PLAY_SOUND:"
        "case REMOVED_SMSG_PLAY_SOUND:" session_source "${session_source}")
elseif(MUTATION STREQUAL "allowlist_object")
    string(REPLACE "case SMSG_PLAY_OBJECT_SOUND:"
        "case REMOVED_SMSG_PLAY_OBJECT_SOUND:" session_source "${session_source}")
elseif(MUTATION STREQUAL "allowlist_music")
    string(REPLACE "case SMSG_PLAY_MUSIC:"
        "case REMOVED_SMSG_PLAY_MUSIC:" session_source "${session_source}")
elseif(MUTATION STREQUAL "opcode_sound")
    string(REPLACE "SMSG_PLAY_SOUND                              = 0x102A,"
        "SMSG_PLAY_SOUND                              = 0x102B,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "opcode_object")
    string(REPLACE "SMSG_PLAY_OBJECT_SOUND                       = 0x1443,"
        "SMSG_PLAY_OBJECT_SOUND                       = 0x1442,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "opcode_music")
    string(REPLACE "SMSG_PLAY_MUSIC                              = 0x0023,"
        "SMSG_PLAY_MUSIC                              = 0x0022,"
        opcode_header "${opcode_header}")
elseif(MUTATION STREQUAL "reference_sound")
    string(REPLACE "SMSG_PLAY_SOUND                                0x102A  ACTIVE"
        "SMSG_PLAY_SOUND                                0x102A  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "reference_object")
    string(REPLACE "SMSG_PLAY_OBJECT_SOUND                         0x1443  ACTIVE"
        "SMSG_PLAY_OBJECT_SOUND                         0x1443  DORMANT"
        opcode_reference "${opcode_reference}")
elseif(MUTATION STREQUAL "reference_music")
    string(REPLACE "SMSG_PLAY_MUSIC                                0x0023  ACTIVE"
        "SMSG_PLAY_MUSIC                                0x0023  DORMANT"
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

require_once("${object_header}"
    "inline void BuildPlaySound(WorldPacket& out, uint32 soundId,"
    "shared direct-sound builder")
require_once("${object_header}"
    "out.WriteGuidMask<2, 3, 7, 6, 0, 5, 4, 1>(sourceGuid);"
    "18414 direct-sound GUID mask")
require_once("${object_header}"
    "out.WriteGuidBytes<3, 2, 4, 7, 5, 0, 6, 1>(sourceGuid);"
    "18414 direct-sound GUID bytes")
require_once("${object_header}"
    "inline void BuildPlayObjectSound(WorldPacket& out, uint32 soundId,"
    "shared object-sound builder")
require_once("${object_header}"
    "out.WriteGuidMask<7, 2, 4, 3>(targetGuid);"
    "18414 object-sound GUID mask")
require_once("${object_header}"
    "out.WriteGuidBytes<7, 5, 3, 1>(sourceGuid);"
    "18414 object-sound GUID bytes")
require_once("${object_header}"
    "inline void BuildPlayMusic(WorldPacket& out, uint32 soundId)"
    "shared music builder")

require_once("${world_object_source}"
    "MopSoundPackets::BuildPlaySound(data, sound_id, ObjectGuid());"
    "WorldObject direct-sound sender")
require_once("${map_source}"
    "MopSoundPackets::BuildPlaySound(data, soundId, ObjectGuid());"
    "map direct-sound sender")
require_once("${battleground_source}"
    "MopSoundPackets::BuildPlaySound(*data, soundid, ObjectGuid());"
    "battleground direct-sound sender")
require_once("${world_object_source}"
    "MopSoundPackets::BuildPlayObjectSound("
    "WorldObject positional-sound sender")
require_once("${world_object_source}"
    "MopSoundPackets::BuildPlayMusic(data, sound_id);"
    "WorldObject music sender")

foreach(name IN ITEMS SMSG_PLAY_SOUND SMSG_PLAY_OBJECT_SOUND SMSG_PLAY_MUSIC)
    require_once("${opcode_registry}" "DefS(${name}, \"${name}\");"
        "${name} registration")
    require_once("${session_source}" "case ${name}:"
        "${name} converted-packet admission")
endforeach()

require_once("${opcode_header}"
    "SMSG_PLAY_SOUND                              = 0x102A,"
    "direct-sound 18414 opcode")
require_once("${opcode_header}"
    "SMSG_PLAY_OBJECT_SOUND                       = 0x1443,"
    "object-sound 18414 opcode")
require_once("${opcode_header}"
    "SMSG_PLAY_MUSIC                              = 0x0023,"
    "music 18414 opcode")
require_once("${opcode_reference}"
    "SMSG_PLAY_SOUND                                0x102A  ACTIVE"
    "active direct-sound reference")
require_once("${opcode_reference}"
    "SMSG_PLAY_OBJECT_SOUND                         0x1443  ACTIVE"
    "active object-sound reference")
require_once("${opcode_reference}"
    "SMSG_PLAY_MUSIC                                0x0023  ACTIVE"
    "active music reference")

foreach(legacy IN ITEMS
        "WorldPacket data(SMSG_PLAY_OBJECT_SOUND, 4 + 8);"
        "WorldPacket data(SMSG_PLAY_SOUND, 4);"
        "data->Initialize(SMSG_PLAY_SOUND, 4);")
    string(FIND "${world_object_source}${map_source}${battleground_source}"
        "${legacy}" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "legacy sound body remains: ${legacy}")
    endif()
endforeach()
