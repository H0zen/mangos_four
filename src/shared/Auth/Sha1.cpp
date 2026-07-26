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

#include "Auth/Sha1.h"
#include "Auth/BigNumber.h"
#include <stdarg.h>
#include <cstring>
#include <openssl/provider.h>

Sha1Hash::Sha1Hash() : mC(EVP_MD_CTX_new())
{
    Initialize();
}

Sha1Hash::~Sha1Hash()
{
    EVP_MD_CTX_free(mC);
    mC = nullptr;
}

Sha1Hash::Sha1Hash(const Sha1Hash& other) : mC(EVP_MD_CTX_new())
{
    memcpy(mDigest, other.mDigest, SHA_DIGEST_LENGTH);

    // A copy of an already-finalized hash only ever has its digest read, so a
    // context that refuses to duplicate is not fatal -- leave it freshly
    // initialized rather than half-copied.
    if (!mC || !other.mC || EVP_MD_CTX_copy_ex(mC, other.mC) != 1)
    {
        Initialize();
    }
}

Sha1Hash& Sha1Hash::operator=(const Sha1Hash& other)
{
    if (this != &other)
    {
        memcpy(mDigest, other.mDigest, SHA_DIGEST_LENGTH);

        if (!mC || !other.mC || EVP_MD_CTX_copy_ex(mC, other.mC) != 1)
        {
            Initialize();
        }
    }
    return *this;
}

void Sha1Hash::UpdateData(const uint8* dta, int len)
{
    EVP_DigestUpdate(mC, dta, len);
}

void Sha1Hash::UpdateData(const std::string& str)
{
    UpdateData((uint8 const*)str.c_str(), str.length());
}

void Sha1Hash::UpdateBigNumbers(BigNumber* bn0, ...)
{
    va_list v;
    BigNumber* bn;

    va_start(v, bn0);
    bn = bn0;
    while (bn)
    {
        UpdateData(bn->AsByteArray(), bn->GetNumBytes());
        bn = va_arg(v, BigNumber*);
    }
    va_end(v);
}

void Sha1Hash::Initialize()
{
    // ARC4 (Auth/ARC4.h) owns a PER-OBJECT OpenSSLProviderManager that explicitly
    // loads/unloads the global "legacy"/"default" providers on every ARC4 construction
    // and destruction. OpenSSL 3.x permanently disables the default provider's
    // AUTO-activation for a library context the first time anything explicitly loads a
    // provider on it -- so once any AuthCrypt/ARC4 object has existed, "SHA1" is only
    // fetchable while an ARC4 object happens to be alive. Sha1Hash is used well outside
    // any AuthCrypt/ARC4 lifetime (e.g. MopAuthProof::ComputeAuthProof), so both
    // EVP_MD_fetch() and the legacy implicitly-fetched EVP_sha1() can find no provider
    // at all there and either return NULL or dereference a stale cached implementation
    // (reproduced: access violation at address 0). Load our own transient reference
    // when fetch fails: the returned EVP_MD keeps the provider alive via its own
    // refcount, so it is safe to unload ours again immediately afterwards.
    EVP_MD* md = EVP_MD_fetch(nullptr, "SHA1", nullptr);
    OSSL_PROVIDER* ownProvider = nullptr;
    if (!md)
    {
        ownProvider = OSSL_PROVIDER_load(nullptr, "default");
        md = EVP_MD_fetch(nullptr, "SHA1", nullptr);
    }

    if (md)
    {
        EVP_DigestInit_ex(mC, md, nullptr);
        EVP_MD_free(md);
    }

    if (ownProvider)
    {
        OSSL_PROVIDER_unload(ownProvider);
    }
}

void Sha1Hash::Finalize(void)
{
    unsigned int length = 0;
    EVP_DigestFinal_ex(mC, mDigest, &length);
}
