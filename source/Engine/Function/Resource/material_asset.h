#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Resource/material_types.h"

namespace NexAur {
    class NEXAUR_API MaterialAsset {
    public:
        MaterialAsset() = default;
        MaterialAsset(const MaterialAsset&) = default;
        MaterialAsset& operator=(const MaterialAsset&) = default;
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
        float getEmissiveStrength() const { return m_emissive_strength; }
        float getNormalScale() const { return m_normal_scale; }
        float getOcclusionStrength() const { return m_occlusion_strength; }
        float getClearcoatFactor() const { return m_clearcoat_factor; }
        float getClearcoatRoughnessFactor() const { return m_clearcoat_roughness_factor; }
        float getTransmissionFactor() const { return m_transmission_factor; }
        MaterialMetallicRoughnessTextureMode getMetallicRoughnessMode() const { return m_metallic_roughness_mode; }
        MaterialAlphaMode getAlphaMode() const { return m_alpha_mode; }
        float getAlphaCutoff() const { return m_alpha_cutoff; }
        bool isDoubleSided() const { return m_double_sided; }
        uint64_t getGeneration() const { return m_generation; }

        void setDebugName(const std::string& value);
        void setBaseColorFactor(const glm::vec4& value);
        void setBaseColorTexture(AssetHandle handle);
        void setNormalTexture(AssetHandle handle);
        void setMetallicTexture(AssetHandle handle);
        void setRoughnessTexture(AssetHandle handle);
        void setMetallicRoughnessTexture(AssetHandle handle);
        void setAOTexture(AssetHandle handle);
        void setEmissiveTexture(AssetHandle handle);
        void setMetallicFactor(float value);
        void setRoughnessFactor(float value);
        void setEmissiveFactor(const glm::vec3& value);
        void setEmissiveStrength(float value);
        void setNormalScale(float value);
        void setOcclusionStrength(float value);
        void setClearcoatFactor(float value);
        void setClearcoatRoughnessFactor(float value);
        void setTransmissionFactor(float value);
        void setMetallicRoughnessMode(MaterialMetallicRoughnessTextureMode mode);
        void setAlphaMode(MaterialAlphaMode mode);
        void setAlphaCutoff(float value);
        void setDoubleSided(bool value);

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
        void markDirty();

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
        float m_emissive_strength = 1.0f;
        float m_normal_scale = 1.0f;
        float m_occlusion_strength = 1.0f;
        float m_clearcoat_factor = 0.0f;
        float m_clearcoat_roughness_factor = 0.0f;
        float m_transmission_factor = 0.0f;
        MaterialMetallicRoughnessTextureMode m_metallic_roughness_mode =
            MaterialMetallicRoughnessTextureMode::Separate;
        MaterialAlphaMode m_alpha_mode = MaterialAlphaMode::Opaque;
        float m_alpha_cutoff = 0.5f;
        bool m_double_sided = false;
        uint64_t m_generation = 0;
    };
} // namespace NexAur
