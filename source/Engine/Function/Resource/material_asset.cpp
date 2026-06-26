#include "pch.h"
#include "material_asset.h"

namespace NexAur {
    MaterialAsset::MaterialAsset(const MaterialImportData& import_data, AssetHandle base_color_texture)
        : m_debug_name(import_data.name.empty() ? "Material" : import_data.name)
        , m_base_color_factor(import_data.base_color_factor)
        , m_base_color_texture(base_color_texture)
        , m_metallic_factor(import_data.metallic_factor)
        , m_roughness_factor(import_data.roughness_factor)
        , m_alpha_mode(import_data.alpha_mode)
        , m_alpha_cutoff(import_data.alpha_cutoff) {}
} // namespace NexAur
