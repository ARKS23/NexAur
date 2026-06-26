#pragma once

#include <string>

#include <glm/glm.hpp>

namespace NexAur {
    enum class MaterialAlphaMode {
        Opaque = 0,
        Mask,
        Blend
    };

    struct MaterialImportData {
        std::string name;
        glm::vec4 base_color_factor{ 1.0f };
        std::string base_color_texture_path;
        std::string normal_texture_path;
        std::string metallic_texture_path;
        std::string roughness_texture_path;
        std::string ao_texture_path;
        float metallic_factor = 0.0f;
        float roughness_factor = 1.0f;
        MaterialAlphaMode alpha_mode = MaterialAlphaMode::Opaque;
        float alpha_cutoff = 0.5f;
    };
} // namespace NexAur
