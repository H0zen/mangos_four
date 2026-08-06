#pragma once

// One 5.4.8 .adt tile, parsed into the grids the height engine wants. The V9/V8
// layout and the tile-local indexing match the reference map-extractor exactly, so
// TerrainTile::TerrainHeight indexes them correctly: the FIRST index is derived from
// the MCNK IndexY field and is what a world-X query resolves against.
//
// Liquid is reported as raw LiquidType.dbc row ids. Classifying a row into
// water/ocean/magma/slime needs the DBC and is deliberately not done here.

#include "ChunkReaders.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace world::terrain
{
    constexpr int ADT_CHUNKS = 16;
    constexpr int ADT_CELLS_PER_CHUNK = 8;
    constexpr int ADT_GRID = ADT_CHUNKS * ADT_CELLS_PER_CHUNK;  // 128
    constexpr int ADT_V9 = ADT_GRID + 1;                        // 129

    /// MCNK header flag: holes are the 5.3+ 64-bit per-cell map at 0x14, where the
    /// earlier expansions kept ofsHeight.
    constexpr uint32_t MCNK_HIGH_RES_HOLES = 1u << 16;

    struct AdtData
    {
        bool hasTerrain = false;
        std::vector<float> v9;
        std::vector<float> v8;
        /// One bit per height cell, 8x8 per chunk -- the 5.3+ high-resolution form.
        /// A chunk that carries only the 16-bit map is widened into it on read.
        std::array<uint64_t, ADT_CHUNKS * ADT_CHUNKS> holes{};
        std::array<uint16_t, ADT_CHUNKS * ADT_CHUNKS> areaIds{};

        bool hasLiquid = false;
        bool hasMh2o = false;
        std::vector<float> liquidHeight;
        std::vector<uint8_t> liquidShow;
        std::vector<uint16_t> liquidEntry;
        std::vector<uint8_t> liquidDark;     ///< MCLQ per-cell dark-water bit
        std::vector<uint8_t> liquidDeepAttr; ///< MH2O deep attribute, per cell

        std::vector<std::string> wmoNames;
        std::vector<std::string> m2Names;
        std::vector<Placement> wmoPlacements;
        std::vector<Placement> m2Placements;
    };

    /**
     * @brief Which half of a tile a file is expected to supply.
     *
     * Up to 3.3.5a a tile is one file and the answer is Both. Cataclysm splits it, and
     * Mists keeps the split: the root holds MH2O and the terrain MCNKs, while _obj0.adt
     * carries the name lists and the MDDF/MODF placements -- AND its own 256 MCNKs,
     * which hold per-chunk doodad references and report area id 0 with no height data.
     * Reading those as terrain wipes what the root supplied, so each file is told what
     * it is for.
     *
     * _obj1.adt, new in 5.0.1, is deliberately never opened: measured on
     * Northrend_30_16 its MWMO/MWID/MODF are byte-identical to _obj0's while its MDDF
     * holds a fraction of the doodads, so it adds no collidable body the root set lacks
     * and reading it would only duplicate placements.
     */
    enum class AdtParts
    {
        Both,       ///< one monolithic tile (up to 3.3.5a)
        Terrain,    ///< the root file of a split tile: MCNK heights, holes, areas, MH2O
        Objects,    ///< _obj0.adt: MMDX/MMID/MWMO/MWID and the MDDF/MODF placements
    };

    /**
     * @brief Vertex format of one MH2O instance: (objOrLvf, liquidType, hasVertexData).
     *
     * From 5.x an instance's second field is a LiquidObject.dbc id once it reaches 42,
     * and resolving it needs three DBCs. This layer must stay free of them -- that is
     * what makes it testable against a byte buffer with no client install -- so the
     * answer is passed in. Without one, the fallback below is used.
     */
    using LiquidVertexFormatFn =
        std::function<uint8_t(uint16_t objOrLvf, uint16_t liquidType, bool hasVertexData)>;

    // Returns false only on a structurally broken file; a valid-but-empty tile yields
    // hasTerrain = false with no placements. Accumulates into `out`, so a split tile is
    // two calls on one AdtData.
    bool ParseAdt(const uint8_t* data, size_t size, AdtData& out,
                  AdtParts parts = AdtParts::Both,
                  const LiquidVertexFormatFn& lvfOf = nullptr);

    inline bool ParseAdt(const std::vector<uint8_t>& bytes, AdtData& out,
                         AdtParts parts = AdtParts::Both,
                         const LiquidVertexFormatFn& lvfOf = nullptr)
    {
        return ParseAdt(bytes.data(), bytes.size(), out, parts, lvfOf);
    }
}
