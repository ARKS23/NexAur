#include "pch.h"
#include "vulkan_frame_lighting_resource.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <glm/geometric.hpp>

namespace NexAur {
    namespace {
        constexpr uint32_t kMaxPointLights = 64;
        constexpr float kFallbackAmbientIntensity = 0.035f;

        struct VulkanGpuFrameGlobals {
            glm::mat4 view_projection{ 1.0f };
            glm::vec4 camera_position_environment_intensity{ 0.0f, 0.0f, 0.0f, 1.0f };
            glm::vec4 directional_direction_intensity{ -0.2f, -1.0f, -0.3f, 1.0f };
            glm::vec4 directional_color_point_count{ 1.0f, 1.0f, 1.0f, 0.0f };
            glm::vec4 ambient_color_intensity{ 1.0f, 1.0f, 1.0f, kFallbackAmbientIntensity };
        };

        struct VulkanGpuPointLight {
            glm::vec4 position_intensity{ 0.0f };
            glm::vec4 color{ 1.0f };
            glm::vec4 attenuation{ 1.0f, 0.09f, 0.032f, 0.0f };
        };

        static_assert(sizeof(VulkanGpuFrameGlobals) % 16 == 0, "Frame globals must stay 16-byte aligned.");
        static_assert(sizeof(VulkanGpuPointLight) % 16 == 0, "Point light data must stay 16-byte aligned.");

        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
            return glm::dot(value, value) > 0.000001f ? glm::normalize(value) : fallback;
        }
    } // namespace

    VulkanFrameLightingResource::~VulkanFrameLightingResource() {
        shutdown();
    }

    bool VulkanFrameLightingResource::init(
        const VulkanResourceContext& context,
        VulkanDescriptorLayoutCache& descriptor_layout_cache,
        VulkanDescriptorAllocator& descriptor_allocator) {
        shutdown();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanFrameLightingResource requires a valid Vulkan context.");
            return false;
        }

        m_physical_device = context.physical_device;
        m_device = context.device;
        m_descriptor_allocator = &descriptor_allocator;

        VkDescriptorSetLayout layout = descriptor_layout_cache.getBuiltinLayout(VulkanDescriptorSetLayoutId::FrameGlobal);
        if (layout == VK_NULL_HANDLE ||
            !createBuffer(sizeof(VulkanGpuFrameGlobals), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_frame_buffer) ||
            !createBuffer(sizeof(VulkanGpuPointLight) * kMaxPointLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_point_light_buffer) ||
            !updateDescriptorSet(layout)) {
            shutdown();
            return false;
        }

        m_ready = true;
        return true;
    }

    void VulkanFrameLightingResource::shutdown() {
        if (m_descriptor_allocator != nullptr && m_descriptor_allocation.valid()) {
            m_descriptor_allocator->free(m_descriptor_allocation);
        }

        destroyBuffer(m_point_light_buffer);
        destroyBuffer(m_frame_buffer);

        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_descriptor_allocation = {};
        m_descriptor_set = VK_NULL_HANDLE;
        m_ready = false;
    }

    bool VulkanFrameLightingResource::update(const VulkanDrawList& draw_list) {
        if (!m_ready) {
            return false;
        }

        const uint32_t point_light_count = static_cast<uint32_t>(std::min<size_t>(draw_list.point_lights.size(), kMaxPointLights));

        VulkanGpuFrameGlobals frame_globals;
        frame_globals.view_projection = draw_list.view.view_projection_matrix;
        frame_globals.camera_position_environment_intensity = glm::vec4(
            draw_list.view.camera_position,
            draw_list.environment_intensity);

        const glm::vec3 light_direction = safeNormalize(
            draw_list.directional_light.direction,
            glm::normalize(glm::vec3{ -0.2f, -1.0f, -0.3f }));
        frame_globals.directional_direction_intensity = glm::vec4(
            light_direction,
            draw_list.directional_light.intensity);
        frame_globals.directional_color_point_count = glm::vec4(
            draw_list.directional_light.color,
            static_cast<float>(point_light_count));
        frame_globals.ambient_color_intensity = glm::vec4(
            glm::vec3{ 1.0f },
            std::max(0.0f, draw_list.environment_intensity) * kFallbackAmbientIntensity);

        std::array<VulkanGpuPointLight, kMaxPointLights> point_lights{};
        for (uint32_t index = 0; index < point_light_count; ++index) {
            const RenderFramePointLight& source = draw_list.point_lights[index];
            VulkanGpuPointLight& target = point_lights[index];
            target.position_intensity = glm::vec4(source.position, source.intensity);
            target.color = glm::vec4(source.color, 1.0f);
            target.attenuation = glm::vec4(source.constant, source.linear, source.quadratic, 0.0f);
        }

        return writeBuffer(m_frame_buffer, &frame_globals, sizeof(frame_globals)) &&
               writeBuffer(m_point_light_buffer, point_lights.data(), sizeof(point_lights));
    }

    bool VulkanFrameLightingResource::createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        Buffer& buffer) const {
        VkBufferCreateInfo buffer_info{};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (!checkVk(vkCreateBuffer(m_device, &buffer_info, nullptr, &buffer.buffer), "vkCreateBuffer(frame lighting)")) {
            return false;
        }

        VkMemoryRequirements memory_requirements{};
        vkGetBufferMemoryRequirements(m_device, buffer.buffer, &memory_requirements);

        const uint32_t memory_type = findMemoryType(
            memory_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memory_type == UINT32_MAX) {
            NX_CORE_ERROR("VulkanFrameLightingResource failed to find host-visible buffer memory.");
            return false;
        }

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = memory_requirements.size;
        allocate_info.memoryTypeIndex = memory_type;

        if (!checkVk(vkAllocateMemory(m_device, &allocate_info, nullptr, &buffer.memory), "vkAllocateMemory(frame lighting)") ||
            !checkVk(vkBindBufferMemory(m_device, buffer.buffer, buffer.memory, 0), "vkBindBufferMemory(frame lighting)")) {
            return false;
        }

        buffer.size = size;
        return true;
    }

    void VulkanFrameLightingResource::destroyBuffer(Buffer& buffer) {
        if (m_device != VK_NULL_HANDLE && buffer.buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(m_device, buffer.buffer, nullptr);
        }
        if (m_device != VK_NULL_HANDLE && buffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(m_device, buffer.memory, nullptr);
        }

        buffer = {};
    }

    bool VulkanFrameLightingResource::writeBuffer(const Buffer& buffer, const void* data, VkDeviceSize size) const {
        if (!buffer.valid() || !data || size > buffer.size) {
            return false;
        }

        void* mapped_data = nullptr;
        if (!checkVk(vkMapMemory(m_device, buffer.memory, 0, size, 0, &mapped_data), "vkMapMemory(frame lighting)")) {
            return false;
        }

        std::memcpy(mapped_data, data, static_cast<size_t>(size));
        vkUnmapMemory(m_device, buffer.memory);
        return true;
    }

    bool VulkanFrameLightingResource::updateDescriptorSet(VkDescriptorSetLayout layout) {
        if (m_descriptor_allocator == nullptr || layout == VK_NULL_HANDLE) {
            return false;
        }

        m_descriptor_allocation = m_descriptor_allocator->allocate(layout);
        if (!m_descriptor_allocation.valid()) {
            NX_CORE_ERROR("Failed to allocate Vulkan frame lighting descriptor set.");
            return false;
        }

        m_descriptor_set = m_descriptor_allocation.set;

        VkDescriptorBufferInfo frame_buffer_info{};
        frame_buffer_info.buffer = m_frame_buffer.buffer;
        frame_buffer_info.offset = 0;
        frame_buffer_info.range = m_frame_buffer.size;

        VkDescriptorBufferInfo point_light_buffer_info{};
        point_light_buffer_info.buffer = m_point_light_buffer.buffer;
        point_light_buffer_info.offset = 0;
        point_light_buffer_info.range = m_point_light_buffer.size;

        VulkanDescriptorWriter()
            .writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame_buffer_info)
            .writeBuffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, point_light_buffer_info)
            .update(m_device, m_descriptor_set);
        return true;
    }

    uint32_t VulkanFrameLightingResource::findMemoryType(
        uint32_t type_filter,
        VkMemoryPropertyFlags properties) const {
        VkPhysicalDeviceMemoryProperties memory_properties{};
        vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

        for (uint32_t index = 0; index < memory_properties.memoryTypeCount; ++index) {
            const bool type_matches = (type_filter & (1u << index)) != 0;
            const bool properties_match = (memory_properties.memoryTypes[index].propertyFlags & properties) == properties;
            if (type_matches && properties_match) {
                return index;
            }
        }

        return UINT32_MAX;
    }
} // namespace NexAur
