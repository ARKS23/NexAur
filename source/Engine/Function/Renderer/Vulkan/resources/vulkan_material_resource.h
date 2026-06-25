#pragma once

#include <string>

#include "Core/Base.h"

namespace NexAur {
    struct MaterialData;

    class NEXAUR_API VulkanMaterialResource {
    public:
        void create(const MaterialData& material_data);

        const std::string& getDebugName() const { return m_debug_name; }
        const std::string& getAlbedoTexturePath() const { return m_albedo_texture_path; }
        const std::string& getNormalTexturePath() const { return m_normal_texture_path; }
        const std::string& getMetallicTexturePath() const { return m_metallic_texture_path; }
        const std::string& getRoughnessTexturePath() const { return m_roughness_texture_path; }
        const std::string& getAoTexturePath() const { return m_ao_texture_path; }

    private:
        std::string m_debug_name;
        std::string m_albedo_texture_path;
        std::string m_normal_texture_path;
        std::string m_metallic_texture_path;
        std::string m_roughness_texture_path;
        std::string m_ao_texture_path;
    };
} // namespace NexAur
