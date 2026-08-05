#include <memory>
#include <string>
#include <vector>
#include "MpqTileSource.hpp"

#include "AdtParser.hpp"

#include <cmath>

namespace world::terrain
{
    namespace
    {
        constexpr float DEG2RAD = 3.14159265358979323846f / 180.0f;
        constexpr float MID = MAP_CENTER * TILE_SIZE;

        Aabb WorldBoundsOf(const Aabb& local, const Transform& xf)
        {
            Aabb out;
            if (!local.valid())
            {
                return out;
            }
            for (int i = 0; i < 8; ++i)
            {
                const Vec3 c{(i & 1) ? local.hi.x : local.lo.x,
                             (i & 2) ? local.hi.y : local.lo.y,
                             (i & 4) ? local.hi.z : local.lo.z};
                out.expand(xf.localToWorld(c));
            }
            return out;
        }

        // Placement into the same world frame the terrain uses. The 180 degrees added to
        // the Z euler is the diag(-1,-1,1) axis flip, which is exactly a half-turn.
        // Built through the three-argument constructor, never by assigning to .scale:
        // that constructor exists to clamp a non-positive or non-finite scale, and MDDF
        // stores scale as uint16/1024, which is legitimately 0 for a malformed record.
        // worldToLocal divides by it, so the whole model quietly stops being hit by any
        // ray instead of failing.
        Transform PlacementTransform(const Placement& p)
        {
            return Transform({MID - p.pos.z, MID - p.pos.x, p.pos.y},
                             Mat3::fromEuler(p.rotDeg.z * DEG2RAD, p.rotDeg.x * DEG2RAD,
                                             (p.rotDeg.y + 180.0f) * DEG2RAD),
                             p.scale);
        }

        // A WDT global WMO's MODF is already in world coordinates, so no re-centring.
        Transform GlobalWmoTransform(const Placement& p)
        {
            return Transform({p.pos.z, p.pos.x, p.pos.y},
                             Mat3::fromEuler(p.rotDeg.z * DEG2RAD, p.rotDeg.x * DEG2RAD,
                                             (p.rotDeg.y + 180.0f) * DEG2RAD),
                             p.scale);
        }

        // MODD's quaternion is authored against the M2's RAW model space, but M2Parser
        // stores hull vertices Y-negated. The rotation acting on the STORED vertices is
        // therefore R(quat) * diag(1,-1,1). Skip that and every doodad comes out mirrored
        // about its own Y axis -- which still overlaps its bounding box, so it looks
        // plausible and quietly puts the collision in the wrong place.
        Transform WmoDoodadTransform(const Transform& wmoXf, const WmoDoodad& d)
        {
            Mat3 r = Mat3::fromQuat(d.quat[0], d.quat[1], d.quat[2], d.quat[3]);
            r.m[1] = -r.m[1];
            r.m[4] = -r.m[4];
            r.m[7] = -r.m[7];

            // The PRODUCT is what gets clamped: either factor alone can be fine while
            // the product underflows to zero.
            return Transform(wmoXf.localToWorld(d.pos), Mat3::mulm(wmoXf.rot, r),
                             wmoXf.scale * d.scale);
        }
    }

    // "Is there an _obj0 next to this root" cannot tell a pre-4.x monolithic tile from a
    // 5.4.8 install that is missing one -- and those need opposite answers, because the
    // second silently yields a map with correct ground and no buildings at all. Asked of
    // the MAP instead: if any tile it declares has an object half, the client writes them,
    // and a tile without one is incomplete. Stops at the first hit, so on split data it is
    // one lookup.
    bool MpqTileSource::MapUsesSplitAdt(uint32_t mapId)
    {
        auto cached = m_mapSplit.find(mapId);
        if (cached != m_mapSplit.end())
        {
            return cached->second;
        }

        bool split = false;
        if (const WdtData* wdt = Wdt(mapId))
        {
            for (int ty = 0; ty < 64 && !split; ++ty)
            {
                for (int tx = 0; tx < 64 && !split; ++tx)
                {
                    if (!wdt->HasAdt(tx, ty))
                    {
                        continue;
                    }
                    const std::string probe = AdtObjPath(mapId, tx, ty);
                    split = !probe.empty() && m_archive.Contains(probe);
                }
            }
        }
        m_mapSplit.emplace(mapId, split);
        return split;
    }

    std::string MpqTileSource::MapDirectory(uint32_t mapId) const
    {
        if (m_maps)
        {
            if (const std::string* dir = m_maps->Find(mapId))
            {
                return *dir;
            }
        }
        return std::string();
    }

    std::string MpqTileSource::AdtPath(uint32_t mapId, int tx, int ty) const
    {
        const std::string name = MapDirectory(mapId);
        if (name.empty())
        {
            return std::string();
        }
        return "World\\Maps\\" + name + "\\" + name + "_" + std::to_string(ty) + "_" +
               std::to_string(tx) + ".adt";
    }

    std::string MpqTileSource::AdtObjPath(uint32_t mapId, int tx, int ty) const
    {
        const std::string name = MapDirectory(mapId);
        if (name.empty())
        {
            return std::string();
        }
        return "World\\Maps\\" + name + "\\" + name + "_" + std::to_string(ty) + "_" +
               std::to_string(tx) + "_obj0.adt";
    }

    std::string MpqTileSource::WdtPath(uint32_t mapId) const
    {
        const std::string name = MapDirectory(mapId);
        if (name.empty())
        {
            return std::string();
        }
        return "World\\Maps\\" + name + "\\" + name + ".wdt";
    }

    const WdtData* MpqTileSource::Wdt(uint32_t mapId)
    {
        auto it = m_wdtCache.find(mapId);
        if (it != m_wdtCache.end())
        {
            return &it->second;
        }

        const std::string path = WdtPath(mapId);
        std::vector<uint8_t> bytes;
        WdtData wdt;
        if (path.empty() || !m_archive.Read(path, bytes) || !ParseWdt(bytes, wdt))
        {
            return nullptr;
        }
        return &m_wdtCache.emplace(mapId, std::move(wdt)).first->second;
    }

    void MpqTileSource::AttachWmoDoodads(const Placement& p, const std::string& wmoPath,
                                         const Transform& wmoXf, TerrainTile& tile)
    {
        const WmoRootData* root = m_wmo.Root(wmoPath);
        if (!root || root->sets.empty())
        {
            return;
        }

        // The placement names the one furnishing set that exists in the world; baking
        // every set would stack alternative furniture in the same room.
        const uint32_t setIdx = (p.doodadSet < root->sets.size()) ? p.doodadSet : 0u;
        const WmoDoodadSet& set = root->sets[setIdx];

        const uint64_t end = uint64_t(set.start) + set.count;
        for (uint64_t i = set.start; i < end && i < root->doodads.size(); ++i)
        {
            const WmoDoodad& d = root->doodads[size_t(i)];
            if (d.name.empty())
            {
                continue;
            }
            auto model = m_m2.Load(d.name);
            if (!model || model->Empty())
            {
                continue;
            }

            StaticInstance inst;
            inst.xf = WmoDoodadTransform(wmoXf, d);
            inst.model = model;
            inst.worldBounds = WorldBoundsOf(model->Bounds(), inst.xf);
            inst.adtId = p.nameSet;
            tile.instances.push_back(std::move(inst));
        }
    }

    std::shared_ptr<TerrainTile> MpqTileSource::LoadAdt(uint32_t mapId, int tx, int ty)
    {
        const std::string path = AdtPath(mapId, tx, ty);
        if (path.empty())
        {
            return nullptr;
        }

        std::vector<uint8_t> bytes;
        if (!m_archive.Read(path, bytes))
        {
            return nullptr;
        }

        AdtData adt;

        // 5.x resolves an MH2O instance's vertex format through LiquidObject ->
        // LiquidType -> LiquidMaterial. The parser stays free of the DBCs; the answer
        // is handed to it.
        LiquidVertexFormatFn lvfOf;
        if (m_liquidObjects)
        {
            const world::LiquidObjectStore* objects = m_liquidObjects;
            const world::LiquidTypeStore* types = m_liquidTypes;
            lvfOf = [objects, types](uint16_t objOrLvf, uint16_t liquidType,
                                     bool hasVertexData)
            {
                return objects->VertexFormat(objOrLvf, liquidType, hasVertexData, types);
            };
        }

        // CATACLYSM SPLIT THE TILE. The root keeps MH2O and the terrain MCNKs; the model
        // and WMO name lists and both placement arrays moved to _obj0.adt. Chosen by
        // whether that file is there rather than by a client-version switch: a 3.3.5a
        // tile has none and takes the monolithic path, so one reader serves both.
        //
        // Getting this wrong is quiet, which is the danger. Parse only the root on 4.x
        // and every map comes out with correct ground and NO BUILDINGS AT ALL -- no
        // WMOs, no doodads, no collision -- while every file parses cleanly.
        std::vector<uint8_t> objBytes;
        const std::string objPath = AdtObjPath(mapId, tx, ty);
        const bool split = !objPath.empty() && m_archive.Read(objPath, objBytes);

        if (split)
        {
            if (!ParseAdt(bytes, adt, AdtParts::Terrain, lvfOf))
            {
                return nullptr;
            }
            // The objects half decides EVERY placement on the tile. Dropping its result
            // gives back a tile with correct ground and no buildings -- the exact quiet
            // failure the comment above warns about -- which BakeMap then counts as a
            // success, because it only ever looks at hasTerrain.
            if (!ParseAdt(objBytes, adt, AdtParts::Objects, lvfOf))
            {
                return nullptr;
            }
        }
        else if (MapUsesSplitAdt(mapId))
        {
            // The map's other tiles have an object half and this one does not, so this
            // is an incomplete install, not a pre-Cataclysm client. Reading the root as
            // monolithic would find no MWMO/MMDX/MODF/MDDF and bake perfect ground with
            // nothing standing on it.
            return nullptr;
        }
        else if (!ParseAdt(bytes, adt, AdtParts::Both, lvfOf))
        {
            return nullptr;
        }

        if (!adt.hasTerrain)
        {
            return nullptr;
        }

        auto tile = std::make_shared<TerrainTile>();
        tile->tx = tx;
        tile->ty = ty;
        tile->hasTerrain = true;
        tile->v9 = std::move(adt.v9);
        tile->v8 = std::move(adt.v8);
        tile->holes = adt.holes;
        tile->areaIds = adt.areaIds;
        tile->hasLiquid = adt.hasLiquid;
        tile->liquidHeight = std::move(adt.liquidHeight);
        tile->liquidShow = std::move(adt.liquidShow);
        tile->liquidEntry = std::move(adt.liquidEntry);

        if (tile->hasLiquid)
        {
            const size_t cells = tile->liquidShow.size();
            tile->liquidKind.assign(cells, uint8_t(LiquidKind::None));
            tile->liquidDeep.assign(cells, 0);
            for (size_t i = 0; i < cells; ++i)
            {
                if (!tile->liquidShow[i])
                {
                    continue;
                }
                const LiquidKind kind =
                    world::ClassifyLiquid(tile->liquidEntry[i], m_liquidTypes);
                tile->liquidKind[i] = uint8_t(kind);
                // Dark water is the MCLQ per-cell bit (pre-WotLK tiles), or an
                // ocean cell whose MH2O chunk carries the "deep" attribute --
                // the rule the pre-rewrite extractor used. The former
                // ocean-without-light-map guess misflagged all of Vashj'ir.
                tile->liquidDeep[i] =
                    (adt.liquidDark[i] ||
                     (kind == LiquidKind::Ocean && adt.liquidDeepAttr[i]))
                        ? 1
                        : 0;
            }
        }

        if (!m_loadStatics)
        {
            return tile;
        }

        auto attach = [&](const Placement& p,
                          const std::shared_ptr<const ICollisionModel>& model)
        {
            if (!model || model->Empty())
            {
                return;
            }
            StaticInstance inst;
            inst.xf = PlacementTransform(p);
            inst.model = model;
            inst.worldBounds = WorldBoundsOf(model->Bounds(), inst.xf);
            inst.adtId = p.nameSet;
            tile->instances.push_back(std::move(inst));
        };

        for (const Placement& p : adt.wmoPlacements)
        {
            if (p.nameIndex >= adt.wmoNames.size())
            {
                continue;
            }
            const std::string& wmoPath = adt.wmoNames[p.nameIndex];
            attach(p, m_wmo.Load(wmoPath));
            AttachWmoDoodads(p, wmoPath, PlacementTransform(p), *tile);
        }

        for (const Placement& p : adt.m2Placements)
        {
            if (p.nameIndex < adt.m2Names.size())
            {
                attach(p, m_m2.Load(adt.m2Names[p.nameIndex]));
            }
        }

        return tile;
    }

    std::shared_ptr<TerrainTile> MpqTileSource::LoadGlobalWmo(uint32_t mapId)
    {
        auto cached = m_globalWmoCache.find(mapId);
        if (cached != m_globalWmoCache.end())
        {
            return cached->second;
        }

        const WdtData* wdt = Wdt(mapId);
        if (!wdt || !wdt->hasGlobalWmo || wdt->globalWmoName.empty() ||
            !wdt->globalWmoPlacement)
        {
            return nullptr;
        }

        auto model = m_wmo.Load(wdt->globalWmoName);
        if (!model || model->Empty())
        {
            return nullptr;
        }

        auto tile = std::make_shared<TerrainTile>();
        tile->isGlobalWmo = true;

        const Transform xf = GlobalWmoTransform(*wdt->globalWmoPlacement);
        StaticInstance inst;
        inst.xf = xf;
        inst.model = model;
        inst.worldBounds = WorldBoundsOf(model->Bounds(), inst.xf);
        inst.adtId = wdt->globalWmoPlacement->nameSet;
        tile->instances.push_back(std::move(inst));

        // A dungeon IS one big WMO, so all of its furniture is doodads.
        AttachWmoDoodads(*wdt->globalWmoPlacement, wdt->globalWmoName, xf, *tile);

        m_globalWmoCache[mapId] = tile;
        return tile;
    }

    std::shared_ptr<TerrainTile> MpqTileSource::Load(uint32_t mapId, int tx, int ty)
    {
        if (auto adtTile = LoadAdt(mapId, tx, ty))
        {
            return adtTile;
        }
        return LoadGlobalWmo(mapId);
    }
}
