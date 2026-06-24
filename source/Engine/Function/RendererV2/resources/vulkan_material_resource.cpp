#include "pch.h"
#include "vulkan_material_resource.h"

#include "Function/Resource/mesh.h"

namespace NexAur {
    void VulkanMaterialResource::create(const MaterialData& material_data) {
        m_debug_name = material_data.name;
        m_albedo_texture_path = material_data.albedo_path;
        m_normal_texture_path = material_data.normal_path;
        m_metallic_texture_path = material_data.metallic_path;
        m_roughness_texture_path = material_data.roughness_path;
        m_ao_texture_path = material_data.ao_path;
    }
} // namespace NexAur
