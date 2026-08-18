#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <vulkan/vulkan.h>

namespace NexAur {
    enum class VulkanRayQueryMode : uint8_t {
        Disabled = 0,
        Auto
    };

    struct VulkanRayTracingOptions {
        VulkanRayQueryMode ray_query_mode = VulkanRayQueryMode::Auto;
        bool force_disable_ray_query = false;
    };

    struct VulkanRayTracingDeviceSupport {
        bool query_succeeded = false;
        bool acceleration_structure_extension = false;
        bool ray_query_extension = false;
        bool deferred_host_operations_extension = false;
        bool ray_tracing_pipeline_extension = false;
        bool buffer_device_address_feature = false;
        bool acceleration_structure_feature = false;
        bool ray_query_feature = false;
        bool ray_tracing_pipeline_feature = false;
        bool runtime_descriptor_array_feature = false;
        bool shader_sampled_image_array_non_uniform_indexing_feature = false;
        bool shader_storage_buffer_array_non_uniform_indexing_feature = false;
        uint64_t min_scratch_alignment = 0;
        uint64_t max_geometry_count = 0;
        uint64_t max_instance_count = 0;
        uint32_t max_descriptor_set_sampled_images = 0;
        uint32_t max_per_stage_descriptor_sampled_images = 0;
        uint32_t max_descriptor_set_storage_buffers = 0;
        uint32_t max_per_stage_descriptor_storage_buffers = 0;
        std::string query_failure_reason;
    };

    struct VulkanRayTracingCapabilities {
        VulkanRayQueryMode ray_query_mode = VulkanRayQueryMode::Auto;
        bool acceleration_structure = false;
        bool ray_query = false;
        bool ray_tracing_pipeline = false;
        bool buffer_device_address = false;
        bool deferred_host_operations = false;
        bool ray_query_enabled = false;
        bool reflection_shading = false;
        uint32_t reflection_texture_capacity = 0;
        uint32_t reflection_geometry_descriptor_capacity = 0;
        uint64_t min_scratch_alignment = 0;
        uint64_t max_geometry_count = 0;
        uint64_t max_instance_count = 0;
        std::string unavailable_reason = "Ray Query capability has not been queried.";

        bool supportsRayQuery() const;
        bool supportsReflectionShading() const {
            return reflection_shading &&
                   reflection_texture_capacity >= 4 &&
                   reflection_geometry_descriptor_capacity >= 2;
        }
    };

    uint32_t chooseVulkanReflectionTextureCapacity(
        uint32_t max_descriptor_set_sampled_images,
        uint32_t max_per_stage_descriptor_sampled_images,
        uint32_t requested_capacity = 256);
    uint32_t chooseVulkanReflectionGeometryDescriptorCapacity(
        uint32_t max_descriptor_set_storage_buffers,
        uint32_t max_per_stage_descriptor_storage_buffers,
        uint32_t requested_capacity = 256);

    inline constexpr std::array<const char*, 3> kVulkanRayQueryDeviceExtensions{
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };

    VulkanRayTracingDeviceSupport queryVulkanRayTracingDeviceSupport(
        VkPhysicalDevice physical_device);

    VulkanRayTracingCapabilities negotiateVulkanRayTracingCapabilities(
        const VulkanRayTracingDeviceSupport& support,
        const VulkanRayTracingOptions& options = {});
} // namespace NexAur
