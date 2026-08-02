/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#include "MopAuthSession.h"
#include "MopAuthProof.h"
#include "MopAuthResponse.h"
#include "Utilities/ByteBuffer.h"
#include "Utilities/WorldPacket.h"
#include "Opcodes.h"
#include "SharedDefines.h"
#include "Auth/MopAuthKey.h"
#include "Auth/AuthCrypt.h"
#include "Auth/BigNumber.h"
#include <openssl/crypto.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

// The name-length block is ONE flag bit then an 11-bit length, both MSB-first (spec 3.4):
//   value = ((bits0 & 0x7F) << 4) | (bits1 >> 4)
// Vectors below are hand-encoded to that (see name_bits0()/name_bits1() further down).
static ByteBuffer make_legacy_body(uint32_t addonSize, uint32_t inflatedSize,
                                   uint8_t nameBits0, uint8_t nameBits1,
                                   char const* name, size_t nameBytes)
{
    ByteBuffer in;
    for (size_t i = 0; i < 52; ++i)
    {
        in << uint8_t(0);
    }
    in << addonSize;
    if (addonSize >= 4)
    {
        in << inflatedSize;                                       // blob's self-described size
        for (uint32_t i = 4; i < addonSize; ++i)
        {
            in << uint8_t(0);
        }
    }
    else
    {
        // A blob too small to carry a 4-byte header still has to be emitted at its stated length,
        // or the decoder would read the name bits as addon bytes and the vector would prove nothing.
        for (uint32_t i = 0; i < addonSize; ++i)
        {
            in << uint8_t(0);
        }
    }
    in << nameBits0 << nameBits1;
    if (nameBytes)
    {
        in.append(reinterpret_cast<uint8_t const*>(name), nameBytes);
    }
    return in;
}

static MopAuth::DecodeResult decode(ByteBuffer& in)
{
    MopAuth::AuthSessionFields out{};
    return MopAuth::DecodeAuthSession(in, out);
}

static void test_short_body_rejected()
{
    ByteBuffer in;
    in << uint32_t(0);
    MopAuth::AuthSessionFields out{};
    CHECK(MopAuth::DecodeAuthSession(in, out) == MopAuth::DecodeResult::ShortBody);
}

// Structure only: the legacy decode is bug-compatible, so field values are deliberately not asserted.
static void test_exact_valid_structure_accepted()
{
    ByteBuffer in = make_legacy_body(4, 1, 0x00, 0x10, "A", 1);   // flag=0, name length 1
    CHECK(decode(in) == MopAuth::DecodeResult::Ok);
}


// An addon blob too small to carry a 4-byte header is NOT malformed. The legacy inline parser did
//     addonsData.resize(m_addonSize);
//     recvPacket.read((uint8*)addonsData.contents(), m_addonSize);
// which for 0..3 is a no-op, never a rejection -- and the consumer agrees, since ReadAddonsInfo
// bails benignly via "if (data.rpos() + 4 > data.size()) { return; }". Each must decode to Ok with
// addonData at exactly the stated length, and must still go on to read the account name.

// Built by hand so the helper cannot accidentally supply the bytes this bound is meant to reject.

// The embedded inflated size must NOT decide authentication. The real consumer,
// WorldSession::ReadAddonsInfo, treats size 0 as "no addon info"
// and simply returns -- it does not reject the login. Auth must be equally tolerant.

// Same class: ReadAddonsInfo logs "addon info too big" and returns for size > 0xFFFFF. That is a
// skipped addon parse, not a failed authentication.


// The bound that DOES matter and must stay: the outer addonSize is attacker-controlled and drives
// a resize(), so it is still checked against the bytes actually present.

// Built by hand: the helper always emits the two name-length bytes.

// The name-length block is ONE flag bit then an 11-bit length, both MSB-first (spec 3.4):
//   value = ((bits0 & 0x7F) << 4) | (bits1 >> 4)
// Self-check against the gate2 capture: len=13, flag=0 -> bits0=0x00, bits1=0xD0 -> `00 D0`, which
// is byte-for-byte what the real client sent. Derived rather than hand-written: 2047 in particular
// is easy to mis-encode by hand.
static uint8_t name_bits0(uint32_t len, bool flag)
{
    return uint8_t((flag ? 0x80 : 0x00) | ((len >> 4) & 0x7F));
}

static uint8_t name_bits1(uint32_t len)
{
    return uint8_t((len & 0xF) << 4);
}

// The decoder applies NO cap to the name length, because the legacy path applied none:
//     account = recvPacket.ReadString(recvPacket.ReadBits(11));
// It took whatever the 11-bit field said and carried on to the build check and the account
// lookup. Every structurally well-formed length must therefore decode to Ok:
//   0     -> ReadString(0) returns "" and cannot throw (ByteBuffer.h:1005-1013); legacy then
//            reached IsAcceptableClientBuild and the account query, yielding a build mismatch or
//            AUTH_UNKNOWN_ACCOUNT -- NOT the AUTH_FAILED a decode rejection would short-circuit to.
//   17,32 -> ordinary ASCII usernames AccountMgr creates.
//   33+   -> a username of multi-byte code points. AccountMgr's own username limit is a CHARACTER
//            count, applied as utf8length(username) (AccountMgr.cpp:101), so a perfectly ordinary
//            account can encode to far more bytes than characters.
//   2047  -> the 11-bit maximum, the true worst case; ReadString self-bounds on size() regardless.
// Nothing downstream needs a cap: ReadString cannot read past the buffer whatever the count says,
// and TruncatedName below already rejects a length exceeding the bytes actually present.

// ReadString() truncates silently at end-of-buffer; the decoder must reject rather than shorten.

// Pins the PRODUCTION decoder's digest scatter to the binary-derived map in
// facts/FACTS_mop548_digest_permutation.md 2 (campaign research doc, not in this tree; serializer
// x86 sub_66F7E0 / x64 sub_1403DFD80, vtable off_140ED6F70 slot 1; digest base PROVEN via
// SHA1_Final's destination, not assumed).
//
// A bijection check is NOT sufficient -- a transposed table is still a bijection, which is exactly
// how an earlier design's test would have passed on a wrong map. This asserts ENTRY BY ENTRY.
//
// It works by marking each body offset with its own index, so the decoded digest[] reads back the
// offset each slot came from. That tests the shipped read sequence rather than a table copied
// alongside it -- there IS no table, and deliberately so: the permutation lives in the decoder as
// straight-line reads, so there is nothing to hand-copy and nothing to transpose.
static void test_digest_scatter_matches_binary()
{
    // digestIndex -> bodyOffset, written out independently from the facts doc's inverse map.
    static constexpr uint8_t kExpectDigestToBody[20] = {
        12, 50, 25, 10, 11, 41, 42, 47, 43, 26, 51, 17, 27, 48, 9, 49, 40, 46, 8, 22
    };

    // A 56-byte prefix where body[i] == i, so a digest byte reveals its own source offset.
    // Offsets 0..51 are all < 52, so every marker is unambiguous.
    ByteBuffer in;
    for (uint32_t i = 0; i < 52; ++i)
    {
        in << uint8_t(i);
    }
    in << uint32_t(4);                                     // addonSize at body 52..55
    in << uint32_t(1);                                     // the 4-byte addon blob
    in << name_bits0(1, false) << name_bits1(1);           // flag=0, name length 1
    in << uint8_t('A');

    MopAuth::AuthSessionFields out{};
    CHECK(MopAuth::DecodeAuthSession(in, out) == MopAuth::DecodeResult::Ok);

    for (uint8_t d = 0; d < 20; ++d)
    {
        CHECK(out.digest[d] == kExpectDigestToBody[d]);    // catches ANY transposition
    }

    // The capture anchors the permutation had no freedom to fit (facts 4). clientSeed is at body
    // 18..21, so with body[i] == i it reads back 0x15141312 little-endian.
    CHECK(out.clientSeed == 0x15141312u);
    CHECK(out.builtNumberClient == 0x2D2Cu);               // body 44..45
    CHECK(out.addonSize == 4u);                            // body 52..55

    // TRAP (facts 5): body 23 is the constant 1, NOT digest[20]. It must never appear in digest[].
    // With body[i] == i, marker 23 in any digest slot would mean an off-by-one past the digest.
    for (uint8_t d = 0; d < 20; ++d)
    {
        CHECK(out.digest[d] != 23);
    }
}

// Spec 3.4 / facts 4: body 420 is ONE flag bit (TC calls it "UseIPv6"; the OFFSET is confirmed, the
// NAME is inferred) followed by an 11-bit name length. The legacy path issued ReadBits(11) with NO
// leading ReadBit(), which silently reads the WRONG 11 bits.
//
// The capture settles it. gate2 carries strlen=13, flag=0, wire bytes `00 D0`:
//   ReadBits(11) alone      -> byte0[7:0] then byte1[7:5] -> 0b00000000110 = 6   WRONG
//   ReadBit() + ReadBits(11)-> flag=byte0[7]; byte0[6:0] then byte1[7:4] -> 0b00000001101 = 13  RIGHT
// This is not a style question: today's decoder mis-reads every real client's name length.
//
// (This does NOT reopen "11 vs 12 bits" -- both readings consume the same 12 bits. It is about
// which model the code states. A 12-bit read is equivalent ONLY while the flag is 0, and silently
// rejects a valid packet -- length >= 2048 -- the moment it is ever 1.)

// The flag must be READ, not skipped, and must not corrupt the length. With flag=1 the 11 bits are
// unchanged; a 12-bit read would yield 2048+13 and reject a perfectly valid packet.

// ---------------------------------------------------------------------------------------------
// MopSocketDrain.h / MopSock::* -- RETIRED (Stage 2 CP3, hazard H4).
//
// These pure decisions encoded ACE_TP_Reactor's suspend/resume dispatch-window reasoning
// (cancel_wakeup, a dispatch-status contract requiring a non-1 return to stop re-entry) so they
// could be unit-tested without a live reactor. Both net:: backends (src/shared/net/) replace the
// PROPERTY they guaranteed -- an auth rejection's response must reach the wire before the
// connection closes -- with their own flush-before-close teardown (ReactorServer's
// closeAfterDrain, IocpServer's closed()+out.empty() check in handleRecv/handleSend), verified by
// reading their source, not assumed. Neither backend has an ACE_TP_Reactor dispatch loop to race,
// so DrainState/ShouldCloseNow/InputStatus have no callers left and are deleted rather than kept
// as dead code pretending to guard a property the engine now guards on its own. See
// proto::ClientConnection::RejectAuth()'s comment for the resolution in full, and its onData()
// for the one piece that does NOT come for free (refusing to act on further input from an
// already-rejected peer while its response drains).
// ---------------------------------------------------------------------------------------------

static_assert(std::is_same<decltype(MopAuth::AuthSessionFields{}.builtNumberClient), uint16_t>::value,
              "auth build field must stay 16-bit");


// ---------------------------------------------------------------------------------------------
// MopAuth::SessionKeyFromHex -- the canonical account.sessionkey hex -> raw-40 K adapter
// (Auth/MopAuthKey.h / .cpp).
// ---------------------------------------------------------------------------------------------

// The pure primitive: copy little-endian bytes, pad the TAIL. A named function rather than an
// inline memcpy so the tail-vs-head rule has one place to live and one place to be tested.

// *** THE TEST THAT CATCHES THE DOUBLE REVERSAL. ***
//
// It drives the REAL adapter over the REAL chain, simulating realmd's own write path:
//     realmd:  vK[40] --SetBinary--> BigNumber --AsHexStr--> account.sessionkey
//     world:   hex --SessionKeyFromHex--> canonical raw-40 K
// and asserts the round trip returns vK BYTE FOR BYTE.
//
// This is the ONLY test that can see an endianness mismatch. Plan v2 fed AsByteArray() -- which is
// ALREADY little-endian (BigNumber.cpp:194 std::reverse) -- into a helper that reverses, producing
// a big-endian K, a deterministic proof failure, and a total auth outage. Every hand-crafted
// byte-array test still passed, because none of them ever called BigNumber. The defect lived in the
// SEAM between the helper's contract and the call site, so the test has to span the seam.

// ---------------------------------------------------------------------------------------------
// AuthCrypt -- the two-phase Prepare()/Activate() crypt and its MoP world seeds
// (Auth/AuthCrypt.h / .cpp, Auth/ARC4.*, Auth/HMACSHA1.*).
// ---------------------------------------------------------------------------------------------

// AuthCrypt known-answer test. Fixtures from the spec-derived oracle (tools/mop_stage2_fixtures.py),
// which asserts these vectors genuinely discriminate: direction (A != B), the 1024-byte drop
// (drop != no-drop), and short-K (40 bytes != 39). A test that cannot fail on those is decorative.
//
// Seeds are BINARY-CONFIRMED in both client binaries (spec 3.1):
//   A server-encrypt/client-decrypt 08F1959F47E5D2DBA13D778F3F3EE700  Wow-64 0x140F4CCA0 / Wow 0xDC6FD0
//   B server-decrypt/client-encrypt 40AAD392267143473A3108A6E7DC982A  Wow-64 0x140F4CCB0 / Wow 0xDC6FE0
static void test_authcrypt_kat()
{
    static const uint8 kK_FULL[40] = {
        0x03,0x0A,0x11,0x18,0x1F,0x26,0x2D,0x34,0x3B,0x42,0x49,0x50,0x57,0x5E,0x65,0x6C,
        0x73,0x7A,0x81,0x88,0x8F,0x96,0x9D,0xA4,0xAB,0xB2,0xB9,0xC0,0xC7,0xCE,0xD5,0xDC,
        0xE3,0xEA,0xF1,0xF8,0xFF,0x06,0x0D,0x14 };
    static const uint8 kKs_SeedA_serverEncrypt[4] = { 0xF6, 0x05, 0x69, 0xC6 };
    static const uint8 kKs_SeedB_serverDecrypt[4] = { 0x18, 0xEF, 0x76, 0x35 };

    AuthCrypt crypt;
    CHECK(!crypt.IsInitialized());
    CHECK(crypt.IsUsable());                       // RC4 selected + sized by the ctor
    CHECK(crypt.Prepare(kK_FULL));
    crypt.Activate();
    CHECK(crypt.IsInitialized());

    // Encrypting zeros yields the keystream verbatim (ARC4 is XOR), so this pins seed A's
    // direction AND the drop-1024 in one assertion. The return value is CHECKED: a silent false
    // would leave `send` as zeros, and an unchecked call would then "pass" nothing at all.
    uint8 send[4] = { 0, 0, 0, 0 };
    CHECK(crypt.EncryptSend(send, 4));
    for (int i = 0; i < 4; ++i)
    {
        CHECK(send[i] == kKs_SeedA_serverEncrypt[i]);
    }

    uint8 recv[4] = { 0, 0, 0, 0 };
    CHECK(crypt.DecryptRecv(recv, 4));
    for (int i = 0; i < 4; ++i)
    {
        CHECK(recv[i] == kKs_SeedB_serverDecrypt[i]);
    }
}

// THE STAGE 1 BLOCKER, closed. IsInitialized() used to be a RAN-TO-COMPLETION flag, not a SUCCEEDED
// flag: AuthCrypt set _initialized = true unconditionally while ARC4 discarded every EVP return
// value. Since IsInitialized() is the codec discriminator (WorldSocket::SendPacket's post-crypt
// branch on send / WorldSocket::DecryptHeaderHook on recv), a failed init would have framed
// PLAINTEXT headers as HDR_POSTCRYPT.

// *** THE COMMIT-REGION INVARIANT, AS A TEST. ***
// Between Prepare() and Activate() the ARC4 contexts are fully keyed -- but the crypt must still be
// INERT: IsInitialized() false, so Decrypt/Encrypt no-op AND the wire codec discriminator
// (WorldSocket::SendPacket's post-crypt branch / WorldSocket::DecryptHeaderHook) still frames
// PRE-crypt. That is what lets HandleAuthSession do every fallible thing -- allocate the session,
// load account data, inflate addons -- AFTER the crypt is prepared, with a throw still leaving the
// socket exactly as unauthenticated as it started.
//
// Stage 1's equivalent ordering rule was load-bearing and its own ledger said "NO TEST COVERS THIS".
// This is that test.

// FAIL-CLOSED: Activate() without a successful Prepare() must leave the crypt inert, never
// half-active. If this ever passes as "initialized", a failed Prepare would publish an unkeyed
// crypt and the codec would frame plaintext as post-crypt -- the blocker, by another route.

// A repeated Prepare while PREPARED is an invalid transition. It must poison the unpublished
// preparation so a caller that ignores the false return cannot Activate stale key material.

// Prepare after activation must be rejected before touching the live stream. The next four bytes
// must therefore be bytes 4..7 of the SAME keystream, not a restarted/re-keyed stream.

// An UNINITIALIZED crypt must REFUSE, never silently pass the data through. This is the property
// that makes the blocker unreachable: if the crypt cannot encrypt, the caller must find out rather
// than ship the buffer untouched.

// MopAuth::IsPlausibleSessionKeyHex edge cases -- the text half of the sessionkey rule
// (Auth/MopAuthKey.h). test_sessionkey_from_hex_roundtrip covers it THROUGH the adapter; this pins
// the predicate's own edges, where the "even and <=80 admits the empty string" bug lived.

// ---------------------------------------------------------------------------------------------
// MopAuth::ComputeAuthProof / ProofEquals -- the spec 3.2 five-chunk SHA1 proof digest, over the
// canonical raw-40 K, and its constant-time compare (Server/MopAuthProof.h / .cpp).
// ---------------------------------------------------------------------------------------------

// Spec 3.2's five chunks, in order:
//   account name (strlen, no NUL) | zero dword (4) | clientSeed (4 LE) | serverSeed (4 LE) | K (40 raw)
// Total = strlen(account) + 52. There is NO de-interleave step -- one SHA1_Final writes 20
// contiguous bytes; the forks' "digest[18]; digest[14]; ..." models the SERIALIZER's scatter, which
// is a different thing entirely and lives in the decoder.
//
// CIRCULARITY, STATED HONESTLY (spec 10.7): these expected digests are a FIXTURE derived from 3.2's
// field table by a standalone script (tools/mop_stage2_fixtures.py) written from the SPEC TEXT,
// not from this source. They are NOT an independent oracle: a mis-transcription of 3.2 itself would
// propagate into both and this test would pass. The only true oracle is a real capture, and it
// cannot serve -- verifying a digest needs that session's K, which is long gone. What this DOES
// catch is the implementation drifting from the spec, which is what it is for.
static void test_auth_proof_digest()
{
    static const uint8 kK_FULL[40] = {
        0x03,0x0A,0x11,0x18,0x1F,0x26,0x2D,0x34,0x3B,0x42,0x49,0x50,0x57,0x5E,0x65,0x6C,
        0x73,0x7A,0x81,0x88,0x8F,0x96,0x9D,0xA4,0xAB,0xB2,0xB9,0xC0,0xC7,0xCE,0xD5,0xDC,
        0xE3,0xEA,0xF1,0xF8,0xFF,0x06,0x0D,0x14 };
    static const uint8 kExpect_K_FULL[20] = {
        0xA7,0x90,0x8C,0xC2,0x5A,0xF4,0xA7,0x21,0x21,0x9A,
        0x7E,0x7F,0x27,0x36,0x82,0x65,0xF7,0x71,0x32,0xB3 };

    uint8 got[20] = { 0 };
    MopAuth::ComputeAuthProof("TESTACCOUNT", 0x11223344, 0x55667788, kK_FULL, got);
    for (int i = 0; i < 20; ++i)
    {
        CHECK(got[i] == kExpect_K_FULL[i]);
    }
}

// THE ~1/256 CASE. A 40-byte fixture has GetNumBytes() == 40 and takes the no-pad path, so it
// passes while the bug is fully present. This vector is the one that fails if K is ever hashed as
// 39 bytes (Sha1Hash::UpdateBigNumbers) or byte-rotated (BigNumber::AsByteArray(40)).


// ---------------------------------------------------------------------------------------------
// MopAuth::BuildAuthResponse{Accepted,Queued,Error} -- the ONE canonical SMSG_AUTH_RESPONSE
// serializer (Server/MopAuthResponse.h / .cpp). Full byte vectors from tools/mop_stage2_fixtures.py
// (spec/plan §S5), not spot-checks -- see the file header there for why a size/first-bit check
// gates nothing.
// ---------------------------------------------------------------------------------------------

// The five variants (spec 6.6a), from the client's own parser (sub_140A70980):
//   1. hasQueueInfo=0 REQUIRES hasAccountData=1, or AUTH_OK (12) is FORCED to AUTH_FAILED (13).
//   2. code 27 (AUTH_WAIT_QUEUE) is NEVER sent -- the client SYNTHESISES it from the queued bit.
//   3. With hasQueueInfo=1 the queue branch overwrites the forced 13, MASKING hasAccountData=0 on
//      updates so it only bites on the RELEASE. That is why SkyFire never caught it.
//
// [HYPOTHESIS] The BIT ORDER these vectors encode is SkyFire-derived and NOT confirmed; only the
// shape is corroborated by the client struct. If Gate 3b fails, the builder and
// tools/mop_stage2_fixtures.py are wrong TOGETHER and must be changed together -- keeping them
// independent is what makes that a two-place edit instead of a silent one.

// ACCEPTED: AUTH_OK, hasAccountData=1 + full block, queued=0. account/server expansion both MISTS.
static const uint8 kExpectResponse_Accepted[91] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x01, 0x00, 0x02,
    0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x03, 0x09, 0x01, 0x0A,
    0x01, 0x0B, 0x03, 0x16, 0x04, 0x18, 0x04, 0x19, 0x04, 0x1A, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03,
    0x00, 0x04, 0x00, 0x05, 0x02, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x04, 0x0A, 0x00, 0x0B,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C };

// INITIAL QUEUE: AUTH_OK, hasAccountData=1, queued=1, position 2. Exactly ACCEPTED + a u32.
static const uint8 kExpectResponse_InitialQueue[95] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7A, 0x02, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08,
    0x03, 0x09, 0x01, 0x0A, 0x01, 0x0B, 0x03, 0x16, 0x04, 0x18, 0x04, 0x19, 0x04, 0x1A, 0x00, 0x01,
    0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x02, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09,
    0x04, 0x0A, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C };

// QUEUE UPDATE: identical to INITIAL QUEUE but position 1 -- pins the position's OFFSET and its
// little-endian encoding, which a size-only check cannot.
static const uint8 kExpectResponse_QueueUpdate[95] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7A, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08,
    0x03, 0x09, 0x01, 0x0A, 0x01, 0x0B, 0x03, 0x16, 0x04, 0x18, 0x04, 0x19, 0x04, 0x1A, 0x00, 0x01,
    0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x02, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09,
    0x04, 0x0A, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C };

// ERROR: code != 12, so hasAccountData=0 is SAFE (it never enters the forcing branch). Two bytes:
// one flushed bit-byte, then the code. NOTE this is byte-identical to what Stage 1 already emits
// at the MopAuth::BuildAuthResponseError call site in WorldSocket::HandleAuthSession's error
// drain -- the ERROR path's wire output does NOT change in Stage 2, which is a useful negative
// result: a Gate 3b error-render regression cannot be blamed on these bytes.
static const uint8 kExpectResponse_Error[2] = { 0x00, 0x14 };

// ACCEPTED with the account restricted BELOW the realm cap -- the ONLY case where the two
// expansion fields differ. Pins that they occupy DISTINCT offsets; passing one value for both
// (as every legacy emitter did) makes this vector unreachable.
static const uint8 kExpectResponse_AcceptedSplitExpansion[91] = {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x58, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x00, 0x01, 0x00, 0x02,
    0x00, 0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06, 0x00, 0x07, 0x00, 0x08, 0x03, 0x09, 0x01, 0x0A,
    0x01, 0x0B, 0x03, 0x16, 0x04, 0x18, 0x04, 0x19, 0x04, 0x1A, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03,
    0x00, 0x04, 0x00, 0x05, 0x02, 0x06, 0x00, 0x07, 0x00, 0x08, 0x00, 0x09, 0x04, 0x0A, 0x00, 0x0B,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C };


// RELEASE is not a variant of its own: position 0 must produce ACCEPTED, byte for byte. A bare
// AUTH_OK -- what SendAuthWaitQue(0) sent before Stage 2 -- is PROVEN HARMFUL: with queued=0 there
// is no queue branch to overwrite the forced 13, so the release was the one packet where
// hasAccountData=0 actually bit.

// The two expansion fields must land at DISTINCT offsets. Exactly one byte may differ from
// ACCEPTED; if the builder collapsed them, either zero or two bytes would.

// The forcing rule is the whole reason this serializer exists; assert a CALLER cannot violate it.

// Code 27 must never reach the wire -- the client SYNTHESISES it from the queued bit, and an emitted
// 27 lands in the != 12 path and delivers failure.
//
// This asserts SERIALIZER ENFORCEMENT, not caller discipline. An earlier draft only ever called the
// builder with AUTH_OK and then declared "never emits 27" -- which proved what that test did, not
// what the serializer permits. The Accepted/Queued entry points cannot express 27 at all (they hard-
// code AUTH_OK), and Error REJECTS it. Both halves are checked.

// NOTE: linking 'shared' drags in ACE, whose OS_main.h rewrites main() to ace_main_i() and
// requires the (int, char**) signature. A no-argument main() therefore fails to link (LNK2019).
int main(int /*argc*/, char** /*argv*/)
{
    test_short_body_rejected();
    test_exact_valid_structure_accepted();
    test_digest_scatter_matches_binary();
    test_authcrypt_kat();
    test_auth_proof_digest();
    std::printf(g_fail ? "FAILED (%d)\n" : "OK\n", g_fail);
    return g_fail ? 1 : 0;
}
