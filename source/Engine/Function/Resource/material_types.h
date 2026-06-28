#pragma once

#include <string>

#include <glm/glm.hpp>

namespace NexAur {
    enum class MaterialAlphaMode {
        Opaque = 0,
        Mask,
        Blend
    };

    enum class MaterialMetallicRoughnessTextureMode {
        Separate = 0,
        PackedGltf = 1
    };

    struct MaterialImportData {
        std::string name;
        glm::vec4 base_color_factor{ 1.0f };
        std::string base_color_texture_path;
        std::string normal_texture_path;
        std::string metallic_texture_path;
        std::string roughness_texture_path;
        std::string metallic_roughness_texture_path;
        std::string ao_texture_path;
        std::string emissive_texture_path;
        float metallic_factor = 0.0f;
        float roughness_factor = 1.0f;
        glm::vec3 emissive_factor{ 0.0f };
        float normal_scale = 1.0f;
        float occlusion_strength = 1.0f;
        MaterialMetallicRoughnessTextureMode metallic_roughness_mode =
            MaterialMetallicRoughnessTextureMode::Separate;
        MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
        float alpha_cutoff = 0.5f;
    };
} // namespace NexAur
