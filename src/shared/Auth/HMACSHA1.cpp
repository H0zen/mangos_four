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

#include "Auth/HMACSHA1.h"
#include "BigNumber.h"
#include <cstring>

#include <openssl/core_names.h>
#include <openssl/params.h>

HMACSHA1::HMACSHA1(uint32 len, const uint8 *seed) : m_ctx(nullptr), m_valid(false)
{
    memset(m_digest, 0, sizeof(m_digest));

    EVP_MAC* mac = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
    if (!mac)
    {
        return;
    }

    // The context takes its own reference on the algorithm.
    m_ctx = EVP_MAC_CTX_new(mac);
    EVP_MAC_free(mac);

    if (!m_ctx)
    {
        return;
    }

    // OSSL_PARAM_construct_utf8_string() wants a mutable buffer.
    char digest[] = "SHA1";
    OSSL_PARAM params[2];
    params[0] = OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digest, 0);
    params[1] = OSSL_PARAM_construct_end();

    if (EVP_MAC_init(m_ctx, seed, len, params) != 1)
    {
        EVP_MAC_CTX_free(m_ctx);
        m_ctx = nullptr;
        return;
    }

    m_valid = true;
}

HMACSHA1::~HMACSHA1()
{
    EVP_MAC_CTX_free(m_ctx);
    m_ctx = nullptr;
}

void HMACSHA1::UpdateBigNumber(BigNumber *bn)
{
    UpdateData(bn->AsByteArray(), bn->GetNumBytes());
}

void HMACSHA1::UpdateData(const uint8 *data, int length)
{
    if (!m_valid)
    {
        return;
    }

    if (EVP_MAC_update(m_ctx, data, size_t(length)) != 1)
    {
        m_valid = false;
        memset(m_digest, 0, sizeof(m_digest));
    }
}

void HMACSHA1::UpdateData(const std::string &str)
{
    UpdateData((uint8 const*)str.c_str(), str.length());
}

void HMACSHA1::Finalize()
{
    if (!m_valid)
    {
        return;
    }

    size_t length = 0;
    const int ok = EVP_MAC_final(m_ctx, (uint8*)m_digest, &length, sizeof(m_digest));
    // MANGOS_ASSERT is elided in RelWithDebInfo (/DNDEBUG), so a short digest would slip through it.
    // A real if is the only guard that actually fails closed here.
    if (ok != 1 || length != SHA_DIGEST_LENGTH)
    {
        m_valid = false;
        memset(m_digest, 0, sizeof(m_digest));
    }
}

uint8 *HMACSHA1::ComputeHash(BigNumber *bn)
{
    UpdateBigNumber(bn);
    Finalize();
    return (uint8*)m_digest;
}
