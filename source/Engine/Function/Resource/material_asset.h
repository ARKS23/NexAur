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
        MaterialAsset(const MaterialImportData& import_data, AssetHandle base_color_texture);

        const std::string& getDebugName() const { return m_debug_name; }
        const glm::vec4& getBaseColorFactor() const { return m_base_color_factor; }
        AssetHandle getBaseColorTexture() const { return m_base_color_texture; }
        float getMetallicFactor() const { return m_metallic_factor; }
        float getRoughnessFactor() const { return m_roughness_factor; }
        MaterialAlphaMode getAlphaMode() const { return m_alpha_mode; }
        float getAlphaCutoff() const { return m_alpha_cutoff; }

        bool hasBaseColorTexture() const { return m_base_color_texture.isValid(); }
        bool isTransparent() const { return m_alpha_mode == MaterialAlphaMode::Blend; }

    private:
        std::string m_debug_name = "Material";
        glm::vec4 m_base_color_factor{ 1.0f };
        AssetHandle m_base_color_texture;
        float m_metallic_factor = 0.0f;
        float m_roughness_factor = 1.0f;
        MaterialAlphaMode m_alpha_mode = MaterialAlphaMode::Opaque;
        float m_alpha_cutoff = 0.5f;
    };
} // namespace NexAur
