#include "pch.h"
#include "material_asset.h"

namespace NexAur {
    MaterialAsset::MaterialAsset(
        const MaterialImportData& import_data,
        AssetHandle base_color_texture,
        AssetHandle normal_texture,
        AssetHandle metallic_texture,
        AssetHandle roughness_texture,
        AssetHandle metallic_roughness_texture,
        AssetHandle ao_texture,
        AssetHandle emissive_texture)
        : m_debug_name(import_data.name.empty() ? "Material" : import_data.name)
        , m_base_color_factor(import_data.base_color_factor)
        , m_base_color_texture(base_color_texture)
        , m_normal_texture(normal_texture)
        , m_metallic_texture(metallic_texture)
        , m_roughness_texture(roughness_texture)
        , m_metallic_roughness_texture(metallic_roughness_texture)
        , m_ao_texture(ao_texture)
        , m_emissive_texture(emissive_texture)
        , m_metallic_factor(import_data.metallic_factor)
        , m_roughness_factor(import_data.roughness_factor)
        , m_emissive_factor(import_data.emissive_factor)
        , m_normal_scale(import_data.normal_scale)
        , m_occlusion_strength(import_data.occlusion_strength)
        , m_metallic_roughness_mode(import_data.metallic_roughness_mode)
        , m_alpha_mode(import_data.alpha_mode)
        , m_alpha_cutoff(import_data.alpha_cutoff)
        , m_double_sided(import_data.double_sided) {}
} // namespace NexAur
