#pragma once

#include <string>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Resource/material_types.h"

namespace NexAur {
    class NEXAUR_API MaterialAsset {
    public:
        MaterialAsset() = default;
        MaterialAsset(
            const MaterialImportData& import_data,
            AssetHandle base_color_texture = AssetHandle(),
            AssetHandle normal_texture = AssetHandle(),
            AssetHandle metallic_texture = AssetHandle(),
            AssetHandle roughness_texture = AssetHandle(),
            AssetHandle metallic_roughness_texture = AssetHandle(),
            AssetHandle ao_texture = AssetHandle(),
            AssetHandle emissive_texture = AssetHandle());

        const std::string& getDebugName() const { return m_debug_name; }
        const glm::vec4& getBaseColorFactor() const { return m_base_color_factor; }
        AssetHandle getBaseColorTexture() const { return m_base_color_texture; }
        AssetHandle getNormalTexture() const { return m_normal_texture; }
        AssetHandle getMetallicTexture() const { return m_metallic_texture; }
        AssetHandle getRoughnessTexture() const { return m_roughness_texture; }
        AssetHandle getMetallicRoughnessTexture() const { return m_metallic_roughness_texture; }
        AssetHandle getAOTexture() const { return m_ao_texture; }
        AssetHandle getEmissiveTexture() const { return m_emissive_texture; }
        float getMetallicFactor() const { return m_metallic_factor; }
        float getRoughnessFactor() const { return m_roughness_factor; }
        const glm::vec3& getEmissiveFactor() const { return m_emissive_factor; }
        float getNormalScale() const { return m_normal_scale; }
        float getOcclusionStrength() const { return m_occlusion_strength; }
        MaterialMetallicRoughnessTextureMode getMetallicRoughnessMode() const { return m_metallic_roughness_mode; }
        MaterialAlphaMode getAlphaMode() const { return m_alpha_mode; }
        float getAlphaCutoff() const { return m_alpha_cutoff; }
        bool isDoubleSided() const { return m_double_sided; }

        bool hasBaseColorTexture() const { return m_base_color_texture.isValid(); }
        bool hasNormalTexture() const { return m_normal_texture.isValid(); }
        bool hasMetallicTexture() const { return m_metallic_texture.isValid(); }
        bool hasRoughnessTexture() const { return m_roughness_texture.isValid(); }
        bool hasMetallicRoughnessTexture() const { return m_metallic_roughness_texture.isValid(); }
        bool hasAOTexture() const { return m_ao_texture.isValid(); }
        bool hasEmissiveTexture() const { return m_emissive_texture.isValid(); }
        bool usesPackedMetallicRoughness() const {
            return m_metallic_roughness_mode == MaterialMetallicRoughnessTextureMode::PackedGltf &&
                   hasMetallicRoughnessTexture();
        }
        bool isTransparent() const { return m_alpha_mode == MaterialAlphaMode::Blend; }

    private:
        std::string m_debug_name = "Material";
        glm::vec4 m_base_color_factor{ 1.0f };
        AssetHandle m_base_color_texture;
        AssetHandle m_normal_texture;
        AssetHandle m_metallic_texture;
        AssetHandle m_roughness_texture;
        AssetHandle m_metallic_roughness_texture;
        AssetHandle m_ao_texture;
        AssetHandle m_emissive_texture;
        float m_metallic_factor = 0.0f;
        float m_roughness_factor = 1.0f;
        glm::vec3 m_emissive_factor{ 0.0f };
        float m_normal_scale = 1.0f;
        float m_occlusion_strength = 1.0f;
        MaterialMetallicRoughnessTextureMode m_metallic_roughness_mode =
            MaterialMetallicRoughnessTextureMode::Separate;
        MaterialAlphaMode m_alpha_mode = MaterialAlphaMode::Opaque;
        float m_alpha_cutoff = 0.5f;
        bool m_double_sided = false;
    };
} // namespace NexAur
