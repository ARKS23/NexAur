#include "pch.h"
#include "vulkan_frame_lighting_resource.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_layout_cache.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_types.h"
#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/frontend/vulkan_draw_list.h"
#include "Function/Renderer/Vulkan/resources/vulkan_environment_resource.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <glm/geometric.hpp>

namespace NexAur {
    namespace {
        constexpr uint32_t kMaxPointLights = 64;
        constexpr float kFallbackAmbientIntensity = 0.035f;

        struct VulkanGpuFrameGlobals {
            glm::mat4 view_projection{ 1.0f };
            std::array<glm::mat4, kMaxRenderShadowCascadeCount> shadow_light_view_projections{
                glm::mat4{ 1.0f },
                glm::mat4{ 1.0f },
                glm::mat4{ 1.0f },
                glm::mat4{ 1.0f }
            };
            glm::vec4 camera_position_environment_intensity{ 0.0f, 0.0f, 0.0f, 1.0f };
            glm::vec4 directional_direction_intensity{ -0.2f, -1.0f, -0.3f, 1.0f };
            glm::vec4 directional_color_point_count{ 1.0f, 1.0f, 1.0f, 0.0f };
            glm::vec4 ambient_color_intensity{ 1.0f, 1.0f, 1.0f, kFallbackAmbientIntensity };
            glm::vec4 shadow_params{ 0.0f, 0.65f, 0.002f, 1.0f };
            glm::vec4 shadow_quality_params{ 1.0f, 1.0f, 0.0f, 0.001f };
            glm::vec4 shadow_pcss_params{ 0.5f, 3.0f, 0.75f, 6.0f };
            glm::vec4 shadow_cascade_splits{ 0.0f, 0.0f, 0.0f, 0.0f };
            glm::vec4 shadow_cascade_params{ 0.0f, 1.0f, 0.0f, 0.0f };
            std::array<glm::vec4, kMaxRenderShadowCascadeCount> shadow_cascade_colors{
                glm::vec4{ 0.95f, 0.25f, 0.20f, 1.0f },
                glm::vec4{ 0.20f, 0.85f, 0.30f, 1.0f },
                glm::vec4{ 0.20f, 0.45f, 1.00f, 1.0f },
                glm::vec4{ 1.00f, 0.80f, 0.20f, 1.0f }
            };
            glm::vec4 ibl_debug_params{ 0.0f, 0.0f, 0.0f, 0.0f };
            glm::mat4 view_matrix{ 1.0f };
            std::array<glm::mat4, kMaxRenderPointShadowFaceCount> point_shadow_view_projections{};
            glm::vec4 point_shadow_params{ 0.0f, 512.0f, 0.015f, 0.02f };
            glm::vec4 point_shadow_quality_params{ 1.0f, 0.0f, 0.0f, 0.0f };
            glm::vec4 contact_shadow_params{ 0.0f, 0.35f, 0.45f, 0.08f };
            std::array<glm::mat4, kMaxRenderRectShadowLights> rect_shadow_view_projections{};
            glm::vec4 rect_shadow_params{ 0.0f, 1024.0f, 0.01f, 0.02f };
            glm::vec4 rect_shadow_quality_params{ 1.0f, 0.0f, 0.85f, 0.0f };
            glm::vec4 rect_light_params{ 1.0f, 0.0f, 0.0f, 0.0f };
            glm::vec4 rect_light_ltc_params{ 1.0f, 0.0f, 0.0f, 0.0f };
        };

        struct VulkanGpuPointLight {
            glm::vec4 position_intensity{ 0.0f };
            glm::vec4 color{ 1.0f };
            glm::vec4 attenuation{ 1.0f, 0.09f, 0.032f, 0.0f };
            glm::vec4 shadow{ -1.0f, 8.0f, 0.85f, 0.0f };
        };

        struct VulkanGpuRectLight {
            glm::vec4 position_intensity{ 0.0f };
            glm::vec4 right_width{ 1.0f, 0.0f, 0.0f, 1.0f };
            glm::vec4 up_height{ 0.0f, 0.0f, 1.0f, 1.0f };
            glm::vec4 normal_range{ 0.0f, -1.0f, 0.0f, 8.0f };
            glm::vec4 color_flags{ 1.0f, 1.0f, 1.0f, 0.0f };
            glm::vec4 shadow{ -1.0f, 0.85f, 0.0f, 0.0f };
        };

        static_assert(sizeof(VulkanGpuFrameGlobals) % 16 == 0, "Frame globals must stay 16-byte aligned.");
        static_assert(sizeof(VulkanGpuPointLight) % 16 == 0, "Point light data must stay 16-byte aligned.");
        static_assert(sizeof(VulkanGpuRectLight) % 16 == 0, "Rect light data must stay 16-byte aligned.");

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

        float sanitizeUnit(float value, float fallback) {
            if (!std::isfinite(value)) {
                return fallback;
            }
            return std::clamp(value, 0.0f, 1.0f);
        }

        float sanitizeMin(float value, float fallback, float minimum) {
            return std::isfinite(value) && value >= minimum ? value : fallback;
        }

        uint32_t sanitizeShadowFilterMode(RenderShadowFilterMode mode) {
            switch (mode) {
            case RenderShadowFilterMode::Hard:
            case RenderShadowFilterMode::PCF3x3:
            case RenderShadowFilterMode::PCF5x5:
            case RenderShadowFilterMode::PoissonPCF:
            case RenderShadowFilterMode::PCSS:
                return static_cast<uint32_t>(mode);
            default:
                return static_cast<uint32_t>(RenderShadowFilterMode::PCF3x3);
            }
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
            !createBuffer(sizeof(VulkanGpuRectLight) * kMaxRenderRectLights, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_rect_light_buffer) ||
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

        destroyBuffer(m_rect_light_buffer);
        destroyBuffer(m_point_light_buffer);
        destroyBuffer(m_frame_buffer);

        m_physical_device = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_descriptor_allocation = {};
        m_descriptor_set = VK_NULL_HANDLE;
        m_ready = false;
    }

    bool VulkanFrameLightingResource::update(
        const VulkanDrawList& draw_list,
        const RenderShadowCascadeFrame& shadow_frame,
        const RenderPointShadowFrame& point_shadow_frame,
        const RenderRectShadowFrame& rect_shadow_frame,
        float shadow_map_size,
        float point_shadow_map_size,
        float rect_shadow_map_size,
        const RenderSettings& render_settings) {
        if (!m_ready) {
            return false;
        }

        const uint32_t point_light_count = static_cast<uint32_t>(std::min<size_t>(draw_list.point_lights.size(), kMaxPointLights));
        const uint32_t rect_light_count = static_cast<uint32_t>(std::min<size_t>(draw_list.rect_lights.size(), kMaxRenderRectLights));

        VulkanGpuFrameGlobals frame_globals;
        frame_globals.view_projection = draw_list.view.view_projection_matrix;
        frame_globals.view_matrix = draw_list.view.view_matrix;
        for (uint32_t index = 0; index < kMaxRenderShadowCascadeCount; ++index) {
            frame_globals.shadow_light_view_projections[index] = shadow_frame.light_view_projections[index];
            frame_globals.shadow_cascade_splits[index] = shadow_frame.split_depths[index];
            frame_globals.shadow_cascade_colors[index] = shadow_frame.debug_colors[index];
        }
        for (uint32_t index = 0; index < kMaxRenderPointShadowFaceCount; ++index) {
            frame_globals.point_shadow_view_projections[index] = point_shadow_frame.light_view_projections[index];
        }
        for (uint32_t index = 0; index < kMaxRenderRectShadowLights; ++index) {
            frame_globals.rect_shadow_view_projections[index] = rect_shadow_frame.light_view_projections[index];
        }
        frame_globals.camera_position_environment_intensity = glm::vec4(
            draw_list.view.camera_position,
            draw_list.ibl_intensity);

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
            glm::max(draw_list.environment_color, glm::vec3{ 0.0f }),
            std::max(0.0f, draw_list.ibl_intensity) * kFallbackAmbientIntensity);
        const RenderShadowSettings& shadow_settings = render_settings.shadow;
        const bool shadow_enabled = draw_list.directional_light.cast_shadow && shadow_settings.enabled;
        const uint32_t cascade_count = std::clamp(
            shadow_frame.cascade_count,
            1u,
            kMaxRenderShadowCascadeCount);
        frame_globals.shadow_params = glm::vec4(
            shadow_enabled ? 1.0f : 0.0f,
            sanitizeUnit(shadow_settings.strength, draw_list.directional_light.shadow_strength),
            sanitizeMin(shadow_settings.constant_bias, draw_list.directional_light.shadow_bias, 0.0f),
            std::max(1.0f, shadow_map_size));
        frame_globals.shadow_quality_params = glm::vec4(
            static_cast<float>(sanitizeShadowFilterMode(shadow_settings.filter_mode)),
            sanitizeMin(shadow_settings.filter_radius, 1.0f, 0.0f),
            sanitizeMin(shadow_settings.normal_bias, 0.0f, 0.0f),
            sanitizeMin(shadow_settings.slope_bias, 0.001f, 0.0f));
        frame_globals.shadow_pcss_params = glm::vec4(
            sanitizeMin(shadow_settings.pcss_light_radius, 0.5f, 0.0f),
            sanitizeMin(shadow_settings.pcss_search_radius, 3.0f, 0.0f),
            sanitizeMin(shadow_settings.pcss_min_filter_radius, 0.75f, 0.0f),
            sanitizeMin(shadow_settings.pcss_max_filter_radius, 6.0f, 0.0f));
        frame_globals.shadow_cascade_params = glm::vec4(
            shadow_enabled && shadow_frame.cascades_enabled ? 1.0f : 0.0f,
            static_cast<float>(cascade_count),
            shadow_frame.debug_overlay ? 1.0f : 0.0f,
            0.0f);
        const uint32_t prefilter_mip_count =
            draw_list.environment && draw_list.environment->isReady()
                ? draw_list.environment->getPrefilterMipCount()
                : 1u;
        frame_globals.ibl_debug_params = glm::vec4(
            static_cast<float>(static_cast<uint32_t>(render_settings.ibl_debug.mode)),
            std::max(0.0f, render_settings.ibl_debug.prefilter_mip),
            static_cast<float>(prefilter_mip_count > 0u ? prefilter_mip_count - 1u : 0u),
            0.0f);
        const RenderPointShadowSettings& point_shadow_settings = render_settings.point_shadow;
        const bool point_shadow_enabled =
            point_shadow_settings.enabled &&
            point_shadow_frame.enabled &&
            point_shadow_frame.shadowed_light_count > 0;
        frame_globals.point_shadow_params = glm::vec4(
            point_shadow_enabled ? 1.0f : 0.0f,
            std::max(1.0f, point_shadow_map_size),
            sanitizeMin(point_shadow_settings.constant_bias, 0.015f, 0.0f),
            sanitizeMin(point_shadow_settings.normal_bias, 0.02f, 0.0f));
        frame_globals.point_shadow_quality_params = glm::vec4(
            sanitizeMin(point_shadow_settings.filter_radius, 1.0f, 0.0f),
            static_cast<float>(std::min(point_shadow_frame.shadowed_light_count, kMaxRenderPointShadowLights)),
            0.0f,
            0.0f);
        const RenderContactShadowSettings& contact_shadow_settings = render_settings.contact_shadow;
        frame_globals.contact_shadow_params = glm::vec4(
            contact_shadow_settings.enabled ? 1.0f : 0.0f,
            sanitizeUnit(contact_shadow_settings.intensity, 0.35f),
            sanitizeMin(contact_shadow_settings.max_distance, 0.45f, 0.0f),
            sanitizeMin(contact_shadow_settings.thickness, 0.08f, 0.0f));
        const RenderRectShadowSettings& rect_shadow_settings = render_settings.rect_shadow;
        const bool rect_shadow_enabled =
            rect_shadow_settings.enabled &&
            rect_shadow_frame.enabled &&
            rect_shadow_frame.shadowed_light_count > 0;
        frame_globals.rect_shadow_params = glm::vec4(
            rect_shadow_enabled ? 1.0f : 0.0f,
            std::max(1.0f, rect_shadow_map_size),
            sanitizeMin(rect_shadow_settings.constant_bias, 0.01f, 0.0f),
            sanitizeMin(rect_shadow_settings.normal_bias, 0.02f, 0.0f));
        frame_globals.rect_shadow_quality_params = glm::vec4(
            sanitizeMin(rect_shadow_settings.filter_radius, 1.0f, 0.0f),
            static_cast<float>(std::min(rect_shadow_frame.shadowed_light_count, kMaxRenderRectShadowLights)),
            sanitizeUnit(rect_shadow_settings.strength, 0.85f),
            0.0f);
        frame_globals.rect_light_params = glm::vec4(
            render_settings.rect_light.enabled ? 1.0f : 0.0f,
            static_cast<float>(rect_light_count),
            render_settings.rect_light.ltc_specular_enabled ? 1.0f : 0.0f,
            0.0f);
        frame_globals.rect_light_ltc_params = glm::vec4(
            sanitizeMin(render_settings.rect_light.specular_intensity_scale, 1.0f, 0.0f),
            render_settings.rect_light.debug_ltc_only ? 1.0f : 0.0f,
            0.0f,
            0.0f);

        std::array<VulkanGpuPointLight, kMaxPointLights> point_lights{};
        for (uint32_t index = 0; index < point_light_count; ++index) {
            const RenderFramePointLight& source = draw_list.point_lights[index];
            VulkanGpuPointLight& target = point_lights[index];
            target.position_intensity = glm::vec4(source.position, source.intensity);
            target.color = glm::vec4(source.color, 1.0f);
            target.attenuation = glm::vec4(source.constant, source.linear, source.quadratic, 0.0f);
            target.shadow = glm::vec4(
                static_cast<float>(source.shadow_slot),
                std::max(0.1f, source.shadow_range),
                sanitizeUnit(source.shadow_strength, 0.85f) *
                    sanitizeUnit(point_shadow_settings.strength, 0.85f),
                source.cast_shadow ? 1.0f : 0.0f);
        }

        std::array<VulkanGpuRectLight, kMaxRenderRectLights> rect_lights{};
        for (uint32_t index = 0; index < rect_light_count; ++index) {
            const RenderFrameRectLight& source = draw_list.rect_lights[index];
            VulkanGpuRectLight& target = rect_lights[index];
            target.position_intensity = glm::vec4(source.position, std::max(0.0f, source.intensity));
            target.right_width = glm::vec4(
                safeNormalize(source.right, glm::vec3{ 1.0f, 0.0f, 0.0f }),
                std::max(0.01f, source.size.x));
            target.up_height = glm::vec4(
                safeNormalize(source.up, glm::vec3{ 0.0f, 0.0f, 1.0f }),
                std::max(0.01f, source.size.y));
            target.normal_range = glm::vec4(
                safeNormalize(source.normal, glm::vec3{ 0.0f, -1.0f, 0.0f }),
                std::max(0.1f, source.range));
            target.color_flags = glm::vec4(glm::max(source.color, glm::vec3{ 0.0f }), source.two_sided ? 1.0f : 0.0f);
            target.shadow = glm::vec4(
                static_cast<float>(source.shadow_slot),
                sanitizeUnit(source.shadow_strength, 0.85f) *
                    sanitizeUnit(rect_shadow_settings.strength, 0.85f),
                source.cast_shadow ? 1.0f : 0.0f,
                0.0f);
        }

        return writeBuffer(m_frame_buffer, &frame_globals, sizeof(frame_globals)) &&
               writeBuffer(m_point_light_buffer, point_lights.data(), sizeof(point_lights)) &&
               writeBuffer(m_rect_light_buffer, rect_lights.data(), sizeof(rect_lights));
    }

    bool VulkanFrameLightingResource::updateShadowMap(VkImageView shadow_map_view, VkSampler shadow_sampler) {
        if (!m_ready || shadow_map_view == VK_NULL_HANDLE || shadow_sampler == VK_NULL_HANDLE) {
            return false;
        }

        VkDescriptorImageInfo shadow_map_info{};
        shadow_map_info.imageView = shadow_map_view;
        shadow_map_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo shadow_sampler_info{};
        shadow_sampler_info.sampler = shadow_sampler;

        VulkanDescriptorWriter()
            .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_map_info)
            .writeImage(3, VK_DESCRIPTOR_TYPE_SAMPLER, shadow_sampler_info)
            .update(m_device, m_descriptor_set);
        return true;
    }

    bool VulkanFrameLightingResource::updatePointShadowMap(VkImageView shadow_map_view, VkSampler shadow_sampler) {
        if (!m_ready || shadow_map_view == VK_NULL_HANDLE || shadow_sampler == VK_NULL_HANDLE) {
            return false;
        }

        VkDescriptorImageInfo shadow_map_info{};
        shadow_map_info.imageView = shadow_map_view;
        shadow_map_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo shadow_sampler_info{};
        shadow_sampler_info.sampler = shadow_sampler;

        VulkanDescriptorWriter()
            .writeImage(4, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_map_info)
            .writeImage(5, VK_DESCRIPTOR_TYPE_SAMPLER, shadow_sampler_info)
            .update(m_device, m_descriptor_set);
        return true;
    }

    bool VulkanFrameLightingResource::updateRectShadowMap(VkImageView shadow_map_view, VkSampler shadow_sampler) {
        if (!m_ready || shadow_map_view == VK_NULL_HANDLE || shadow_sampler == VK_NULL_HANDLE) {
            return false;
        }

        VkDescriptorImageInfo shadow_map_info{};
        shadow_map_info.imageView = shadow_map_view;
        shadow_map_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo shadow_sampler_info{};
        shadow_sampler_info.sampler = shadow_sampler;

        VulkanDescriptorWriter()
            .writeImage(7, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, shadow_map_info)
            .writeImage(8, VK_DESCRIPTOR_TYPE_SAMPLER, shadow_sampler_info)
            .update(m_device, m_descriptor_set);
        return true;
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

        VkDescriptorBufferInfo rect_light_buffer_info{};
        rect_light_buffer_info.buffer = m_rect_light_buffer.buffer;
        rect_light_buffer_info.offset = 0;
        rect_light_buffer_info.range = m_rect_light_buffer.size;

        VulkanDescriptorWriter()
            .writeBuffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame_buffer_info)
            .writeBuffer(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, point_light_buffer_info)
            .writeBuffer(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, rect_light_buffer_info)
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
