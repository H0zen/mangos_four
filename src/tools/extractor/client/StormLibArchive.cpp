#include <string>
#include <vector>
#include "StormLibArchive.hpp"
#include <cstring>

#include <StormLib.h>

#include <cstdio>
#include <unordered_set>
#include <utility>

namespace world::terrain
{
    namespace
    {
        std::string ExpandLocale(std::string s, const std::string& locale)
        {
            const std::string token = "{locale}";
            for (size_t at = s.find(token); at != std::string::npos;
                 at = s.find(token, at + locale.size()))
            {
                s.replace(at, token.size(), locale);
            }
            return s;
        }
    }

    StormLibArchive::~StormLibArchive()
    {
        for (void* h : m_handles)
        {
            if (h)
            {
                SFileCloseArchive(h);
            }
        }
    }

    bool StormLibArchive::AddArchive(const std::string& mpqPath)
    {
        HANDLE h = nullptr;
        if (!SFileOpenArchive(mpqPath.c_str(), 0, MPQ_OPEN_READ_ONLY, &h))
        {
            return false;
        }
        m_handles.push_back(h);
        return true;
    }

    int StormLibArchive::OpenClientData(const std::string& dataDir,
                                        const std::vector<std::string>& archives,
                                        const std::vector<std::string>& localeArchives,
                                        const std::string& locale)
    {
        int opened = 0;
        for (const std::string& rel : archives)
        {
            if (AddArchive(dataDir + "/" + ExpandLocale(rel, locale)))
            {
                ++opened;
            }
        }

        // No locale at all is a real client, not a mistake: a 1.12 install keeps every
        // archive, DBCs included, at the Data root and has no locale directory. Skip the
        // whole side rather than probing "Data//locale-.MPQ" a dozen times.
        if (locale.empty())
        {
            return opened;
        }

        const std::string localeDir = dataDir + "/" + locale;
        for (const std::string& rel : localeArchives)
        {
            if (AddArchive(localeDir + "/" + ExpandLocale(rel, locale)))
            {
                ++opened;
            }
        }
        return opened;
    }

    // The 5.4.8 archive set, and the update chain in ASCENDING BUILD ORDER: each is a
    // delta on top of the one before, so the update order is not cosmetic.
    //
    // Mists groups the bases by CONTENT TYPE, not by world region as 4.3.4 did: there is
    // no world/world2/art/alternate split any more, and expansion4 joins the three that
    // were already there. Measured on a real 5.4.8 install (build 18414).
    //
    // sound.MPQ is deliberately absent -- no geometry, and gigabytes to index. So are
    // itemtexture.MPQ and interface.MPQ for the same reason; texture.MPQ stays because
    // nothing else is certain to carry a model a WMO names.
    const std::vector<std::string>& ClientArchives548()
    {
        static const std::vector<std::string> archives = {
            "base-Win.MPQ",
            "misc.MPQ",
            "world.MPQ", "model.MPQ", "texture.MPQ",
            "expansion1.MPQ", "expansion2.MPQ", "expansion3.MPQ", "expansion4.MPQ",
        };
        return archives;
    }

    // Mists has ONE locale archive, not the per-expansion set 4.3.4 shipped. The speech
    // archives carry no DBC, WDT, ADT, WMO or M2 and are deliberately not opened.
    const std::vector<std::string>& ClientLocaleArchives548()
    {
        static const std::vector<std::string> archives = {
            "locale-{locale}.MPQ",
        };
        return archives;
    }

    // Eighteen builds, 16016 -> 18273. Enumerated rather than globbed so an archive the
    // chain does not name cannot silently change what a bake reads.
    const std::vector<std::string>& ClientUpdates548()
    {
        static const std::vector<std::string> updates = {
            "wow-update-base-16016.MPQ", "wow-update-base-16048.MPQ",
            "wow-update-base-16057.MPQ", "wow-update-base-16309.MPQ",
            "wow-update-base-16357.MPQ", "wow-update-base-16516.MPQ",
            "wow-update-base-16650.MPQ", "wow-update-base-16844.MPQ",
            "wow-update-base-16965.MPQ", "wow-update-base-17116.MPQ",
            "wow-update-base-17266.MPQ", "wow-update-base-17325.MPQ",
            "wow-update-base-17345.MPQ", "wow-update-base-17538.MPQ",
            "wow-update-base-17645.MPQ", "wow-update-base-17688.MPQ",
            "wow-update-base-17898.MPQ", "wow-update-base-18273.MPQ",
        };
        return updates;
    }

    const std::vector<std::string>& ClientLocaleUpdates548()
    {
        static const std::vector<std::string> updates = {
            "wow-update-{locale}-16016.MPQ", "wow-update-{locale}-16048.MPQ",
            "wow-update-{locale}-16057.MPQ", "wow-update-{locale}-16309.MPQ",
            "wow-update-{locale}-16357.MPQ", "wow-update-{locale}-16516.MPQ",
            "wow-update-{locale}-16650.MPQ", "wow-update-{locale}-16844.MPQ",
            "wow-update-{locale}-16965.MPQ", "wow-update-{locale}-17116.MPQ",
            "wow-update-{locale}-17266.MPQ", "wow-update-{locale}-17325.MPQ",
            "wow-update-{locale}-17345.MPQ", "wow-update-{locale}-17538.MPQ",
            "wow-update-{locale}-17645.MPQ", "wow-update-{locale}-17688.MPQ",
            "wow-update-{locale}-17898.MPQ", "wow-update-{locale}-18273.MPQ",
        };
        return updates;
    }

    int StormLibArchive::OpenPatchedClientData(
        const std::string& dataDir,
        const std::vector<std::string>& archives,
        const std::vector<std::string>& localeArchives,
        const std::vector<std::string>& updates,
        const std::vector<std::string>& localeUpdates,
        const std::string& locale)
    {
        const std::string localeDir = dataDir + "/" + locale;

        // The bases first, in priority order, remembering where each side starts so the
        // right updates can be attached to the right bases: wow-update-base-* patches the
        // non-locale archives and wow-update-<loc>-* the locale ones.
        const size_t baseBegin = m_handles.size();
        for (const std::string& rel : archives)
        {
            AddArchive(dataDir + "/" + ExpandLocale(rel, locale));
        }
        const size_t localeBegin = m_handles.size();
        if (!locale.empty())
        {
            for (const std::string& rel : localeArchives)
            {
                AddArchive(localeDir + "/" + ExpandLocale(rel, locale));
            }
        }
        const size_t localeEnd = m_handles.size();

        // ATTACH, so a patched file resolves. NULL prefix, never a forced one: StormLib's
        // FindPatchPrefix says WoW patches "mostly do not use prefix" and derives one only
        // when base\(patch_metadata) is present. Forcing "base" or the locale fails with
        // ERROR_CANT_FIND_PATCH_PREFIX.
        auto attach = [&](const std::vector<std::string>& names, const std::string& dir,
                          size_t from, size_t to)
        {
            for (size_t i = from; i < to; ++i)
            {
                for (const std::string& rel : names)
                {
                    const std::string path = dir + "/" + ExpandLocale(rel, locale);
                    SFileOpenPatchArchive(m_handles[i], path.c_str(), nullptr, 0);
                }
            }
        };

        attach(updates, dataDir, baseBegin, localeBegin);
        attach(localeUpdates, localeDir, localeBegin, localeEnd);

        // ...AND open them ordinarily, at the top of the priority chain, because an update
        // also carries WHOLE files that exist in no base at all -- a model added after
        // release is reachable no other way.
        //
        // Such a file is then patched INCREMENTALLY by the updates after it, and no base
        // holds it, so the base chain above cannot resolve it: each update must also be a
        // base for the ones that follow. Without this Read() rejects those deltas and
        // falls back to the OLDEST whole copy -- measured on 5.4.8, that served the 16516
        // beta of Deepwind Gorge, a flat plate, in place of the shipped battleground.
        // 1374 files over 29 maps, Siege of Orgrimmar and Throne of Thunder among them.
        auto openChain = [&](const std::vector<std::string>& names, const std::string& dir)
        {
            std::vector<std::pair<size_t, std::string>> opened;
            for (const std::string& rel : names)
            {
                const std::string path = dir + "/" + ExpandLocale(rel, locale);
                if (AddArchive(path))
                {
                    opened.emplace_back(m_handles.size() - 1, path);
                }
            }
            for (size_t i = 0; i < opened.size(); ++i)
            {
                for (size_t j = i + 1; j < opened.size(); ++j)
                {
                    SFileOpenPatchArchive(m_handles[opened[i].first],
                                          opened[j].second.c_str(), nullptr, 0);
                }
            }
        };

        openChain(updates, dataDir);
        if (!locale.empty())
        {
            openChain(localeUpdates, localeDir);
        }

        return int(m_handles.size());
    }

    /**
     * @brief Read the file, skipping any handle that answers with an incremental patch.
     *
     * A PTCH blob is not a file. The update archives sit high in the priority chain so
     * that whole files added after release are found, which means for a file that was
     * PATCHED rather than replaced they answer first, with the delta. Rejecting it and
     * carrying on lands on the base archive, which has the same updates attached and so
     * returns the file with the chain already applied.
     *
     * Measured, not assumed: on 4.3.4 every one of the 20 .dbc and 5 .db2 members of
     * wow-update-enUS-15595.MPQ is a PTCH, and Spell.dbc reads as a 136 KB delta here
     * against 17.3 MB and 73253 records through the chain. 5.4.8 uses the same PTCH
     * machinery over eighteen update builds.
     */
    bool StormLibArchive::Read(const std::string& path, std::vector<uint8_t>& out)
    {
        for (auto it = m_handles.rbegin(); it != m_handles.rend(); ++it)
        {
            HANDLE hFile = nullptr;
            if (!SFileOpenFileEx(*it, path.c_str(), 0, &hFile))
            {
                continue;
            }

            DWORD high = 0;
            const DWORD size = SFileGetFileSize(hFile, &high);
            if (size == SFILE_INVALID_SIZE)
            {
                SFileCloseFile(hFile);
                continue;
            }

            out.resize(size);
            DWORD got = 0;
            if (size > 0)
            {
                SFileReadFile(hFile, out.data(), size, &got, nullptr);
            }
            SFileCloseFile(hFile);
            if (got == size)
            {
                if (out.size() >= 4 && std::memcmp(out.data(), "PTCH", 4) == 0)
                {
                    out.clear();
                    continue;               // a delta, not the file -- keep looking
                }
                return true;
            }
            out.clear();
        }
        return false;
    }

    bool StormLibArchive::Contains(const std::string& path) const
    {
        for (auto it = m_handles.rbegin(); it != m_handles.rend(); ++it)
        {
            if (SFileHasFile(*it, const_cast<char*>(path.c_str())))
            {
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> StormLibArchive::FindFiles(const std::string& pattern) const
    {
        std::vector<std::string> result;
        if (m_handles.empty() || pattern.empty())
        {
            return result;
        }

        std::unordered_set<std::string> seen;
        for (auto it = m_handles.rbegin(); it != m_handles.rend(); ++it)
        {
            SFILE_FIND_DATA findData{};
            HANDLE hFind = SFileFindFirstFile(*it, pattern.c_str(), &findData, nullptr);
            if (!hFind)
            {
                continue;
            }
            do
            {
                std::string name(findData.cFileName);
                if (seen.insert(name).second)
                {
                    result.push_back(std::move(name));
                }
            } while (SFileFindNextFile(hFind, &findData));
            SFileFindClose(hFind);
        }
        return result;
    }
}
