#include "pch.h"
#include "material_asset.h"

#include <algorithm>
#include <cmath>

namespace NexAur {
    namespace {
        float sanitizeUnit(float value, float fallback) {
            return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
        }

        float sanitizeNonNegative(float value, float fallback) {
            return std::isfinite(value) ? std::max(0.0f, value) : fallback;
        }

        glm::vec3 sanitizeNonNegative(const glm::vec3& value, const glm::vec3& fallback) {
            if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
                return fallback;
            }
            return glm::max(value, glm::vec3{ 0.0f });
        }

        glm::vec4 sanitizeColor(const glm::vec4& value, const glm::vec4& fallback) {
            if (!std::isfinite(value.x) ||
                !std::isfinite(value.y) ||
                !std::isfinite(value.z) ||
                !std::isfinite(value.w)) {
                return fallback;
            }
            return glm::clamp(value, glm::vec4{ 0.0f }, glm::vec4{ 1.0f });
        }

        MaterialAlphaMode sanitizeAlphaMode(MaterialAlphaMode mode) {
            switch (mode) {
            case MaterialAlphaMode::Opaque:
            case MaterialAlphaMode::Mask:
            case MaterialAlphaMode::Blend:
                return mode;
            default:
                return MaterialAlphaMode::Opaque;
            }
        }
    } // namespace

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

    void MaterialAsset::setDebugName(const std::string& value) {
        m_debug_name = value.empty() ? "Material" : value;
    }

    void MaterialAsset::setBaseColorFactor(const glm::vec4& value) {
        m_base_color_factor = sanitizeColor(value, m_base_color_factor);
        markDirty();
    }

    void MaterialAsset::setBaseColorTexture(AssetHandle handle) {
        m_base_color_texture = handle;
        markDirty();
    }

    void MaterialAsset::setNormalTexture(AssetHandle handle) {
        m_normal_texture = handle;
        markDirty();
    }

    void MaterialAsset::setMetallicTexture(AssetHandle handle) {
        m_metallic_texture = handle;
        markDirty();
    }

    void MaterialAsset::setRoughnessTexture(AssetHandle handle) {
        m_roughness_texture = handle;
        markDirty();
    }

    void MaterialAsset::setMetallicRoughnessTexture(AssetHandle handle) {
        m_metallic_roughness_texture = handle;
        markDirty();
    }

    void MaterialAsset::setAOTexture(AssetHandle handle) {
        m_ao_texture = handle;
        markDirty();
    }

    void MaterialAsset::setEmissiveTexture(AssetHandle handle) {
        m_emissive_texture = handle;
        markDirty();
    }

    void MaterialAsset::setMetallicFactor(float value) {
        m_metallic_factor = sanitizeUnit(value, m_metallic_factor);
        markDirty();
    }

    void MaterialAsset::setRoughnessFactor(float value) {
        m_roughness_factor = sanitizeUnit(value, m_roughness_factor);
        markDirty();
    }

    void MaterialAsset::setEmissiveFactor(const glm::vec3& value) {
        m_emissive_factor = sanitizeNonNegative(value, m_emissive_factor);
        markDirty();
    }

    void MaterialAsset::setNormalScale(float value) {
        m_normal_scale = sanitizeNonNegative(value, m_normal_scale);
        markDirty();
    }

    void MaterialAsset::setOcclusionStrength(float value) {
        m_occlusion_strength = sanitizeUnit(value, m_occlusion_strength);
        markDirty();
    }

    void MaterialAsset::setMetallicRoughnessMode(MaterialMetallicRoughnessTextureMode mode) {
        m_metallic_roughness_mode = mode;
        markDirty();
    }

    void MaterialAsset::setAlphaMode(MaterialAlphaMode mode) {
        m_alpha_mode = sanitizeAlphaMode(mode);
        markDirty();
    }

    void MaterialAsset::setAlphaCutoff(float value) {
        m_alpha_cutoff = sanitizeUnit(value, m_alpha_cutoff);
        markDirty();
    }

    void MaterialAsset::setDoubleSided(bool value) {
        m_double_sided = value;
        markDirty();
    }

    void MaterialAsset::markDirty() {
        ++m_generation;
        if (m_generation == 0) {
            ++m_generation;
        }
    }
} // namespace NexAur
