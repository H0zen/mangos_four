#pragma once

// Whether an MH2O instance's vertex block starts with a float heightmap or is a run of
// uint8 depths: LiquidObject -> LiquidType -> LiquidMaterial -> LiquidVertexFormat.
// Read a depth block as floats and the bake gets water surfaces around 1e11, which is
// an ordinary float and passes every check downstream.
//
// Neither DBC is in the server's DBCfmt, so the format strings live with the code.

#include "MpqDbcLoader.hpp"
#include "LiquidTypeStore.hpp"

#include <cstdint>
#include <unordered_map>

namespace world
{
    /// A liquid_object_or_lvf at or above this is a LiquidObject.dbc id, not a format.
    constexpr uint16_t MH2O_FIRST_LIQUID_OBJECT = 42;

    /// ...except 42, which the client answers before the DB: `Liquid::
    /// RegisterLiquidObject` reads "LO = 42 || LT = 14 -> oceanLiquidObject".
    constexpr uint16_t LIQUID_OBJECT_OCEAN = 42;
    constexpr uint16_t LIQUID_TYPE_OCEAN = 2;
    constexpr uint16_t LIQUID_TYPE_WMO_OCEAN = 14;

    /// Depth-only: no heightmap, surface at 0.0 rather than at either height_level.
    constexpr uint8_t LVF_DEPTH_ONLY = 2;

    class LiquidObjectStore
    {
    public:
        // ID, flowDirection, flowSpeed, LiquidTypeID, fishable, reflection
        static constexpr const char* LiquidObjectFmt = "nffiii";
        // ID, LiquidVertexFormat, flags
        static constexpr const char* LiquidMaterialFmt = "nii";

        bool LoadFromDbc(world::terrain::IMpqArchive& archive)
        {
            m_objectLiquidType.clear();
            m_materialLvf.clear();

            DBCFileLoader objects;
            if (LoadDbcFromMpq(archive, "DBFilesClient\\LiquidObject.dbc",
                               LiquidObjectFmt, objects))
            {
                for (uint32_t r = 0; r < objects.GetNumRows(); ++r)
                {
                    DBCFileLoader::Record rec = objects.getRecord(r);
                    m_objectLiquidType[rec.getUInt(0)] = rec.getUInt(3);
                }
            }

            DBCFileLoader materials;
            if (LoadDbcFromMpq(archive, "DBFilesClient\\LiquidMaterial.dbc",
                               LiquidMaterialFmt, materials))
            {
                for (uint32_t r = 0; r < materials.GetNumRows(); ++r)
                {
                    DBCFileLoader::Record rec = materials.getRecord(r);
                    m_materialLvf[rec.getUInt(0)] = uint8_t(rec.getUInt(1));
                }
            }

            return !m_objectLiquidType.empty() && !m_materialLvf.empty();
        }

        /// LiquidType.dbc row a LiquidObject names, or 0 when unknown.
        uint32_t LiquidTypeOfObject(uint32_t objectId) const
        {
            auto it = m_objectLiquidType.find(objectId);
            return it != m_objectLiquidType.end() ? it->second : 0u;
        }

        /**
         * @brief The vertex format of one SMLiquidInstance.
         *
         * @param objOrLvf the instance's second field, verbatim
         * @param liquidType the instance's first field, a LiquidType.dbc row
         * @param hasVertexData whether offset_vertex_data was non-zero
         * @param types LiquidType.dbc, for the MaterialID hop
         */
        uint8_t VertexFormat(uint16_t objOrLvf, uint16_t liquidType, bool hasVertexData,
                             const LiquidTypeStore* types) const
        {
            if (!hasVertexData && liquidType != LIQUID_TYPE_OCEAN)
            {
                return LVF_DEPTH_ONLY;
            }

            if (objOrLvf < MH2O_FIRST_LIQUID_OBJECT)
            {
                return uint8_t(objOrLvf);
            }

            // Ahead of the DB on purpose: row 42's LiquidTypeID is 0, so the chain below
            // falls through to type 2 -> material 1 -> LVF 0 and reads uint8 depths as
            // floats. Measured on 5.4.8: 452957 (type 2, object 42) instances, and every
            // one of the 93536 with a vertex block stores 1 byte per corner.
            if (objOrLvf == LIQUID_OBJECT_OCEAN || liquidType == LIQUID_TYPE_WMO_OCEAN)
            {
                return LVF_DEPTH_ONLY;
            }

            uint32_t row = LiquidTypeOfObject(objOrLvf);
            if (!row)
            {
                row = liquidType;
            }

            uint32_t material = 0;
            if (types)
            {
                if (const LiquidTypeInfo* info = types->Find(row))
                {
                    material = info->materialId;
                }
            }

            auto lvf = m_materialLvf.find(material);
            if (lvf != m_materialLvf.end())
            {
                return lvf->second;
            }

            // No chain to follow. Ocean is depth-only in every version measured; every
            // other family begins with a heightmap.
            return liquidType == LIQUID_TYPE_OCEAN ? LVF_DEPTH_ONLY : uint8_t(0);
        }

        size_t ObjectCount() const { return m_objectLiquidType.size(); }
        size_t MaterialCount() const { return m_materialLvf.size(); }

    private:
        std::unordered_map<uint32_t, uint32_t> m_objectLiquidType;
        std::unordered_map<uint32_t, uint8_t> m_materialLvf;
    };
}
