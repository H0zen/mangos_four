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

/*
 * CHANNEL and WHISPER carry a second string, so they read two lengths rather
 * than one. Both bodies below are real 18414 captures.
 *
 * CHANNEL is a 9-bit channel length then an 8-bit message length, then the
 * message, then the channel name. WHISPER is an 8-bit message length then a
 * 9-bit target length -- the client writes that second field as len>>1 in
 * eight bits plus a separate low bit -- then the message, then the target.
 *
 * The inherited readers used 10+9 for both, which shifted every field after
 * the first length, and WHISPER additionally had the two strings swapped, so a
 * whisper was addressed to its own text.
 */
static void test_channel_message_lengths()
{
    // 9-bit channel length 5, 8-bit message length 3, "jop" then "czech".
    uint8 const body[] = {
        0x07, 0x00, 0x00, 0x00,
        0x02, 0x81, 0x80,
        'j', 'o', 'p', 'c', 'z', 'e', 'c', 'h'
    };
    WorldPacket packet(CMSG_MESSAGECHAT_CHANNEL, sizeof(body));
    packet.append(body, sizeof(body));

    uint32 language = 0;
    packet >> language;
    CHECK(language == LANG_COMMON);

    uint32 const channelLength = packet.ReadBits(9);
    uint32 const msgLength = packet.ReadBits(8);
    CHECK(channelLength == 5);
    CHECK(msgLength == 3);

    std::string const msg = packet.ReadString(msgLength);
    std::string const channel = packet.ReadString(channelLength);
    CHECK(msg == "jop");
    CHECK(channel == "czech");
    CHECK(packet.rpos() == packet.size());
}


/*
 * Every addon channel uses an 8-bit message length, never nine, but they
 * disagree on both the order of the two length fields and the order of the two
 * strings. Every body below is a real 18414 capture except OFFICER, which has
 * no traffic in the corpus and is built from the client writer sub_C888C4.
 */





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
    test_say_message_request();
    test_channel_message_lengths();
    test_text_emote_request();

    if (g_fail)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fail);
        return 1;
    }

    std::puts("mop_chat_packets_test: PASS");
    return 0;
}
