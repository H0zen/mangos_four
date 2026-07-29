/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * Independent byte fixtures for the 5.4.8.18414 generic chat packet.
 */

#include "Chat.h"
#include "Opcodes.h"
#include "WorldPacket.h"

#include <cstdio>
#include <cstring>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

static bool Equal(WorldPacket const& packet, std::vector<uint8> const& expected)
{
    return packet.size() == expected.size() &&
        std::memcmp(packet.contents(), expected.data(), expected.size()) == 0;
}

static void test_gm_system_message()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_SYSTEM;
    message.language = LANG_UNIVERSAL;
    message.chatTag = CHAT_TAG_GM;
    message.senderGuid = UI64LIT(0x0102030405060708);
    message.text = "GM";
    message.senderName = "Admin";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));

    CHECK(packet.GetOpcode() == SMSG_MESSAGECHAT);
    CHECK(Equal(packet, {
        0x00, 0x2A, 0xA0, 0x00, 0x40, 0x03, 0xFF, 0x80, 0x0B, 0x00, 0x00,
        0x05, 0x00, 0x06, 0x02, 0x09, 0x03, 0x07, 0x04,
        0x00,
        0x47, 0x4D,
        0x41, 0x64, 0x6D, 0x69, 0x6E
    }));
}

static void test_whisper_with_target_language_and_realms()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_WHISPER;
    message.language = Language(1);
    message.receiverGuid = UI64LIT(0x1122334455667788);
    message.realmId2 = 0xA1B2C3D4u;
    message.realmId1 = 0x10203040u;
    message.text = "Hi";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));
    CHECK(Equal(packet, {
        0x96, 0x00, 0x7F, 0x90, 0x08, 0x00, 0xA0, 0x00,
        0x07,
        0x67, 0x32, 0x54, 0x23, 0x10, 0x45, 0x76, 0x89,
        0x01,
        0xD4, 0xC3, 0xB2, 0xA1,
        0x48, 0x69,
        0x40, 0x30, 0x20, 0x10
    }));
}

static void test_achievement_message()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_ACHIEVEMENT;
    message.achievementId = 0x12345678u;
    message.text = "A";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));
    CHECK(Equal(packet, {
        0x97, 0x00, 0x00, 0x30, 0x00, 0x00, 0x70, 0x00,
        0x30, 0x78, 0x56, 0x34, 0x12, 0x41
    }));
}

static void test_group_message()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_PARTY;
    message.groupGuid = UI64LIT(0x0102030405060708);
    message.text = "P";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));
    CHECK(Equal(packet, {
        0x97, 0xFF, 0x00, 0x30, 0x08, 0x00, 0x70, 0x00,
        0x02, 0x06, 0x04, 0x05, 0x03, 0x09, 0x07, 0x02, 0x00, 0x50
    }));
}

static void test_guild_message()
{
    MopChatPackets::Message message;
    message.chatType = CHAT_MSG_GUILD;
    message.guildGuid = UI64LIT(0x0102030405060708);
    message.text = "G";

    WorldPacket packet;
    CHECK(MopChatPackets::BuildMessage(packet, message));
    CHECK(Equal(packet, {
        0x97, 0x00, 0x00, 0x30, 0x08, 0x00, 0x77, 0xF8,
        0x05, 0x02, 0x00, 0x04, 0x07, 0x03, 0x09, 0x06, 0x04, 0x47
    }));
}

static void test_length_boundaries()
{
    MopChatPackets::Message maximum;
    maximum.senderName.assign((size_t(1) << 11) - 1, 's');
    maximum.receiverName.assign((size_t(1) << 11) - 1, 'r');
    maximum.channelName.assign((size_t(1) << 7) - 1, 'c');
    maximum.addonPrefix.assign((size_t(1) << 5) - 1, 'p');
    maximum.text.assign((size_t(1) << 12) - 1, 'm');
    maximum.chatTag = (uint32(1) << 9) - 1;
    WorldPacket valid;
    CHECK(MopChatPackets::BuildMessage(valid, maximum));

    MopChatPackets::Message oversized;
    oversized.senderName.assign(size_t(1) << 11, 's');
    WorldPacket senderRejected;
    CHECK(!MopChatPackets::BuildMessage(senderRejected, oversized));
    CHECK(senderRejected.empty());

    oversized = MopChatPackets::Message{};
    oversized.receiverName.assign(size_t(1) << 11, 'r');
    WorldPacket receiverRejected;
    CHECK(!MopChatPackets::BuildMessage(receiverRejected, oversized));

    oversized = MopChatPackets::Message{};
    oversized.channelName.assign(size_t(1) << 7, 'c');
    WorldPacket channelRejected;
    CHECK(!MopChatPackets::BuildMessage(channelRejected, oversized));

    oversized = MopChatPackets::Message{};
    oversized.addonPrefix.assign(size_t(1) << 5, 'p');
    WorldPacket prefixRejected;
    CHECK(!MopChatPackets::BuildMessage(prefixRejected, oversized));

    oversized = MopChatPackets::Message{};
    oversized.text.assign(size_t(1) << 12, 'm');
    WorldPacket textRejected;
    CHECK(!MopChatPackets::BuildMessage(textRejected, oversized));

    oversized = MopChatPackets::Message{};
    oversized.chatTag = uint32(1) << 9;
    WorldPacket tagRejected;
    CHECK(!MopChatPackets::BuildMessage(tagRejected, oversized));
}

static void test_player_name_notices()
{
    WorldPacket notFound;
    CHECK(MopChatPackets::BuildPlayerNotFound(notFound, "Al"));
    CHECK(notFound.GetOpcode() == SMSG_CHAT_PLAYER_NOT_FOUND);
    CHECK(Equal(notFound, { 0x01, 0x00, 'A', 'l' }));

    WorldPacket ambiguous;
    CHECK(MopChatPackets::BuildPlayerAmbiguous(ambiguous, "Bob"));
    CHECK(ambiguous.GetOpcode() == SMSG_CHAT_PLAYER_AMBIGUOUS);
    CHECK(Equal(ambiguous, { 0x01, 0x80, 'B', 'o', 'b' }));

    WorldPacket maximum;
    CHECK(MopChatPackets::BuildPlayerNotFound(maximum,
        std::string((size_t(1) << 9) - 1, 'x')));
    CHECK(maximum.size() == (size_t(1) << 9) + 1);
    CHECK(maximum.contents()[0] == 0xFF);
    CHECK(maximum.contents()[1] == 0x80);

    WorldPacket oversized;
    CHECK(!MopChatPackets::BuildPlayerAmbiguous(oversized,
        std::string(size_t(1) << 9, 'x')));
    CHECK(oversized.empty());
}

static void test_chat_restricted_notice()
{
    WorldPacket trial;
    MopChatPackets::BuildChatRestrictedNotice(trial, 0);
    CHECK(trial.GetOpcode() == SMSG_CHAT_RESTRICTED);
    CHECK(Equal(trial, { 0x00 }));

    WorldPacket silenced;
    MopChatPackets::BuildChatRestrictedNotice(silenced, 3);
    CHECK(Equal(silenced, { 0x03 }));
}

static void test_opcode()
{
    CHECK(uint32(SMSG_MESSAGECHAT) == 0x1A9Au);
    CHECK(uint32(SMSG_MESSAGECHAT) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(CMSG_MESSAGECHAT_SAY) == 0x0A9Au);
    CHECK(uint32(CMSG_MESSAGECHAT_SAY) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(CMSG_MESSAGECHAT_AFK) == 0x0EABu);
    CHECK(uint32(CMSG_MESSAGECHAT_AFK) < uint32(OPCODE_TABLE_SIZE));
    CHECK(uint32(CMSG_UNREGISTER_ALL_ADDON_PREFIXES) == 0x029Fu);
    CHECK(uint32(CMSG_ADDON_REGISTERED_PREFIXES) == 0x040Eu);
    CHECK(uint32(SMSG_CHAT_PLAYER_NOT_FOUND) == 0x1082u);
    CHECK(uint32(SMSG_CHAT_PLAYER_AMBIGUOUS) == 0x061Au);
    CHECK(uint32(SMSG_CHAT_RESTRICTED) == 0x1A3Bu);
}

static void test_say_message_request()
{
    uint8 const body[] = {
        0x07, 0x00, 0x00, 0x00, // LANG_COMMON
        0x05,                   // 8-bit message length
        'H', 'e', 'l', 'l', 'o'
    };
    WorldPacket packet(CMSG_MESSAGECHAT_SAY, sizeof(body));
    packet.append(body, sizeof(body));

    uint32 language = 0;
    packet >> language;
    std::string message;
    CHECK(language == LANG_COMMON);
    CHECK(MopChatPackets::ReadSayMessageRequest(packet, message));
    CHECK(message == "Hello");
    CHECK(packet.rpos() == packet.size());

    uint8 const truncatedBody[] = { 0x05, 'H', 'i' };
    WorldPacket truncated(CMSG_MESSAGECHAT_SAY, sizeof(truncatedBody));
    truncated.append(truncatedBody, sizeof(truncatedBody));
    CHECK(!MopChatPackets::ReadSayMessageRequest(truncated, message));
    CHECK(message.empty());
    CHECK(truncated.rpos() == truncated.size());
}

/*
 * The non-say chat types read their length inline rather than through
 * ReadSayMessageRequest, and every one of them used a 9-bit width until this
 * was pinned. These are real 18414 guild bodies lifted byte-for-byte from the
 * retail corpus (catalogue 2BE10C89, build-filtered), and in all five the
 * length byte equals payloadLength - 5, which is only consistent with an
 * 8-bit, byte-aligned length.
 *
 * A 9-bit read consumes the length byte AND the first character byte, computes
 * roughly double the true length, and ReadString then clamps to the buffer end
 * -- so the message silently arrives missing its first character rather than
 * failing. Size evidence could never have caught it: a minimum body of 6 is
 * consistent with both widths.
 */
static void test_inline_message_length_is_eight_bits()
{
    struct Sample { uint8 length; char const* text; };
    Sample const samples[] = {
        { 3,  "cav" },
        { 6,  "kludne" },
        { 19, "1234567890123456789" },
    };

    for (Sample const& sample : samples)
    {
        WorldPacket packet(CMSG_MESSAGECHAT_GUILD, 5 + sample.length);
        packet << uint32(LANG_COMMON);
        packet << uint8(sample.length);
        packet.append(reinterpret_cast<uint8 const*>(sample.text), sample.length);

        uint32 language = 0;
        packet >> language;
        CHECK(language == LANG_COMMON);

        std::string message = packet.ReadString(packet.ReadBits(8));
        CHECK(message == sample.text);
        CHECK(packet.rpos() == packet.size());
    }

    // The exact bytes of a captured guild line, and what the 9-bit read did to it.
    uint8 const captured[] = { 0x07, 0x00, 0x00, 0x00, 0x03, 'c', 'a', 'v' };
    WorldPacket wrong(CMSG_MESSAGECHAT_GUILD, sizeof(captured));
    wrong.append(captured, sizeof(captured));
    uint32 lang = 0;
    wrong >> lang;
    CHECK(wrong.ReadString(wrong.ReadBits(9)) == "av");     // leading 'c' lost
}

static void test_afk_message_request()
{
    struct Fixture
    {
        std::vector<uint8> body;
        bool valid;
        char const* expected;
    };

    Fixture const fixtures[] = {
        { { 0x03, 'A', 'F', 'K' }, true, "AFK" },
        { { 0x00 }, true, "" },
        { { 0x03, 'A', 'F' }, false, "" },
        { { 0x01, 'A', 'X' }, false, "" }
    };

    for (Fixture const& fixture : fixtures)
    {
        WorldPacket packet(CMSG_MESSAGECHAT_AFK, fixture.body.size());
        packet.append(fixture.body.data(), fixture.body.size());
        std::string message = "not cleared";

        CHECK(MopChatPackets::ReadAfkMessageRequest(packet, message) == fixture.valid);
        CHECK(message == fixture.expected);
        CHECK(packet.rpos() == packet.size());
    }
}

static void test_addon_prefix_batch()
{
    uint8 const body[] = {
        0x00, 0x00, 0x02, // 24-bit prefix count
        0x19, 0x00,       // 5-bit lengths: 3, 4
        'A', 'B', 'C', 'W', 'X', 'Y', 'Z'
    };
    WorldPacket packet(CMSG_ADDON_REGISTERED_PREFIXES, sizeof(body));
    packet.append(body, sizeof(body));

    std::vector<std::string> prefixes;
    CHECK(MopChatPackets::ReadAddonPrefixBatch(packet, prefixes));
    CHECK(prefixes.size() == 2);
    CHECK(prefixes[0] == "ABC");
    CHECK(prefixes[1] == "WXYZ");
    CHECK(packet.rpos() == packet.size());
}

static void test_addon_prefix_soft_cap()
{
    WorldPacket oversized(CMSG_ADDON_REGISTERED_PREFIXES, 3);
    oversized.WriteBits(65u, 24);
    oversized.FlushBits();

    std::vector<std::string> prefixes;
    CHECK(!MopChatPackets::ReadAddonPrefixBatch(oversized, prefixes));
    CHECK(oversized.rpos() == oversized.size());

    WorldPacket full(CMSG_ADDON_REGISTERED_PREFIXES, 43);
    full.WriteBits(64u, 24);
    for (uint32 i = 0; i < 64; ++i)
        full.WriteBits(0u, 5);
    full.FlushBits();
    CHECK(MopChatPackets::ReadAddonPrefixBatch(full, prefixes));
    CHECK(prefixes.size() == 64);

    WorldPacket oneMore(CMSG_ADDON_REGISTERED_PREFIXES, 5);
    oneMore.WriteBits(1u, 24);
    oneMore.WriteBits(1u, 5);
    oneMore.FlushBits();
    oneMore.append("X", 1);
    CHECK(!MopChatPackets::ReadAddonPrefixBatch(oneMore, prefixes));
    CHECK(prefixes.empty());
}

static void test_text_emote_response()
{
    WorldPacket packet;
    MopChatPackets::BuildTextEmote(packet,
        UI64LIT(0x0002030005060008), UI64LIT(0x1100334400667700),
        0x11223344u, 0x55667788u);

    CHECK(packet.GetOpcode() == SMSG_TEXT_EMOTE);
    CHECK(Equal(packet, {
        0x7A, 0x57,
        0x67, 0x76, 0x10, 0x02, 0x07,
        0x44, 0x33, 0x22, 0x11,
        0x03, 0x04, 0x09, 0x32, 0x45,
        0x88, 0x77, 0x66, 0x55
    }));
}

static void test_text_emote_request()
{
    uint8 const body[] = {
        0x44, 0x33, 0x22, 0x11,
        0x88, 0x77, 0x66, 0x55,
        0x57, 0x32, 0x76, 0x45, 0x67, 0x10
    };
    WorldPacket packet(CMSG_TEXT_EMOTE, sizeof(body));
    packet.append(body, sizeof(body));

    MopChatPackets::TextEmoteRequest request =
        MopChatPackets::ReadTextEmoteRequest(packet);
    CHECK(request.textEmote == 0x11223344u);
    CHECK(request.emoteNumber == 0x55667788u);
    CHECK(request.targetGuid.GetRawValue() == UI64LIT(0x1100334400667700));
}

int main(int /*argc*/, char** /*argv*/)
{
    test_gm_system_message();
    test_whisper_with_target_language_and_realms();
    test_achievement_message();
    test_group_message();
    test_guild_message();
    test_length_boundaries();
    test_player_name_notices();
    test_chat_restricted_notice();
    test_opcode();
    test_say_message_request();
    test_inline_message_length_is_eight_bits();
    test_afk_message_request();
    test_addon_prefix_batch();
    test_addon_prefix_soft_cap();
    test_text_emote_response();
    test_text_emote_request();

    if (g_fail)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }

    std::puts("mop_chat_packets_test: PASS");
    return 0;
}
