#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_capabilities.h"

#include <cstring>
#include <algorithm>
#include <vector>

namespace NexAur {
    namespace {
        bool hasExtension(
            const std::vector<VkExtensionProperties>& extensions,
            const char* extension_name) {
            for (const VkExtensionProperties& extension : extensions) {
                if (std::strcmp(extension.extensionName, extension_name) == 0) {
                    return true;
                }
            }
            return false;
        }

        std::string missingRayQueryRequirement(
            const VulkanRayTracingDeviceSupport& support) {
            if (!support.acceleration_structure_extension) {
                return "Missing VK_KHR_acceleration_structure.";
            }
            if (!support.ray_query_extension) {
                return "Missing VK_KHR_ray_query.";
            }
            if (!support.deferred_host_operations_extension) {
                return "Missing VK_KHR_deferred_host_operations.";
            }
            if (!support.buffer_device_address_feature) {
                return "bufferDeviceAddress feature is unavailable.";
            }
            if (!support.acceleration_structure_feature) {
                return "accelerationStructure feature is unavailable.";
            }
            if (!support.ray_query_feature) {
                return "rayQuery feature is unavailable.";
            }
            return {};
        }
    } // namespace

    bool VulkanRayTracingCapabilities::supportsRayQuery() const {
        return acceleration_structure && ray_query && buffer_device_address &&
               deferred_host_operations;
    }

    uint32_t chooseVulkanReflectionTextureCapacity(
        uint32_t max_descriptor_set_sampled_images,
        uint32_t max_per_stage_descriptor_sampled_images,
        uint32_t requested_capacity) {
        constexpr uint32_t kFallbackTextureSlotCount = 4;
        if (requested_capacity < kFallbackTextureSlotCount) {
            return 0;
        }

        const uint32_t available = std::min(
            max_descriptor_set_sampled_images,
            max_per_stage_descriptor_sampled_images);
        if (available < kFallbackTextureSlotCount) {
            return 0;
        }
        return std::min(available, requested_capacity);
    }

    uint32_t chooseVulkanReflectionGeometryDescriptorCapacity(
        uint32_t max_descriptor_set_storage_buffers,
        uint32_t max_per_stage_descriptor_storage_buffers,
        uint32_t requested_capacity) {
        constexpr uint32_t kTableStorageBufferCount = 3;
        constexpr uint32_t kGeometryBufferArrayCount = 2;
        constexpr uint32_t kMinimumCapacity = 2;
        const uint32_t available = std::min(
            max_descriptor_set_storage_buffers,
            max_per_stage_descriptor_storage_buffers);
        if (available <= kTableStorageBufferCount || requested_capacity < kMinimumCapacity) {
            return 0;
        }

        const uint32_t array_capacity =
            (available - kTableStorageBufferCount) / kGeometryBufferArrayCount;
        return array_capacity >= kMinimumCapacity ?
            std::min(array_capacity, requested_capacity) :
            0;
    }

    VulkanRayTracingDeviceSupport queryVulkanRayTracingDeviceSupport(
        VkPhysicalDevice physical_device) {
        VulkanRayTracingDeviceSupport support;
        if (physical_device == VK_NULL_HANDLE) {
            support.query_failure_reason = "Cannot query Ray Query capability for a null physical device.";
            return support;
        }

        uint32_t extension_count = 0;
        VkResult extension_result = vkEnumerateDeviceExtensionProperties(
            physical_device,
            nullptr,
            &extension_count,
            nullptr);
        if (extension_result != VK_SUCCESS) {
            support.query_failure_reason =
                "Failed to enumerate Vulkan device extensions (" +
                std::to_string(static_cast<int>(extension_result)) + ").";
            return support;
        }

        std::vector<VkExtensionProperties> extensions(extension_count);
        extension_result = vkEnumerateDeviceExtensionProperties(
            physical_device,
            nullptr,
            &extension_count,
            extensions.data());
        if (extension_result != VK_SUCCESS) {
            support.query_failure_reason =
                "Failed to read Vulkan device extensions (" +
                std::to_string(static_cast<int>(extension_result)) + ").";
            return support;
        }
        extensions.resize(extension_count);

        support.acceleration_structure_extension = hasExtension(
            extensions,
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        support.ray_query_extension = hasExtension(
            extensions,
            VK_KHR_RAY_QUERY_EXTENSION_NAME);
        support.deferred_host_operations_extension = hasExtension(
            extensions,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        support.ray_tracing_pipeline_extension = hasExtension(
            extensions,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

        VkPhysicalDeviceBufferDeviceAddressFeatures buffer_device_address_features{};
        buffer_device_address_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

        VkPhysicalDeviceAccelerationStructureFeaturesKHR acceleration_structure_features{};
        acceleration_structure_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

        VkPhysicalDeviceRayQueryFeaturesKHR ray_query_features{};
        ray_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

        VkPhysicalDeviceRayTracingPipelineFeaturesKHR ray_tracing_pipeline_features{};
        ray_tracing_pipeline_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

        VkPhysicalDeviceDescriptorIndexingFeatures descriptor_indexing_features{};
        descriptor_indexing_features.sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

        VkPhysicalDeviceFeatures2 features{};
        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &buffer_device_address_features;
        void** chain_tail = &buffer_device_address_features.pNext;
        if (support.acceleration_structure_extension) {
            *chain_tail = &acceleration_structure_features;
            chain_tail = &acceleration_structure_features.pNext;
        }
        if (support.ray_query_extension) {
            *chain_tail = &ray_query_features;
            chain_tail = &ray_query_features.pNext;
        }
        if (support.ray_tracing_pipeline_extension) {
            *chain_tail = &ray_tracing_pipeline_features;
            chain_tail = &ray_tracing_pipeline_features.pNext;
        }
        *chain_tail = &descriptor_indexing_features;
        vkGetPhysicalDeviceFeatures2(physical_device, &features);

        support.buffer_device_address_feature =
            buffer_device_address_features.bufferDeviceAddress == VK_TRUE;
        support.acceleration_structure_feature =
            acceleration_structure_features.accelerationStructure == VK_TRUE;
        support.ray_query_feature = ray_query_features.rayQuery == VK_TRUE;
        support.ray_tracing_pipeline_feature =
            ray_tracing_pipeline_features.rayTracingPipeline == VK_TRUE;
        support.runtime_descriptor_array_feature =
            descriptor_indexing_features.runtimeDescriptorArray == VK_TRUE;
        support.shader_sampled_image_array_non_uniform_indexing_feature =
            descriptor_indexing_features.shaderSampledImageArrayNonUniformIndexing == VK_TRUE;
        support.shader_storage_buffer_array_non_uniform_indexing_feature =
            descriptor_indexing_features.shaderStorageBufferArrayNonUniformIndexing == VK_TRUE;

        if (support.acceleration_structure_extension) {
            VkPhysicalDeviceAccelerationStructurePropertiesKHR acceleration_structure_properties{};
            acceleration_structure_properties.sType =
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

            VkPhysicalDeviceProperties2 properties{};
            properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
            properties.pNext = &acceleration_structure_properties;
            vkGetPhysicalDeviceProperties2(physical_device, &properties);

            support.min_scratch_alignment =
                acceleration_structure_properties.minAccelerationStructureScratchOffsetAlignment;
            support.max_geometry_count = acceleration_structure_properties.maxGeometryCount;
            support.max_instance_count = acceleration_structure_properties.maxInstanceCount;
        }

        VkPhysicalDeviceProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        vkGetPhysicalDeviceProperties2(physical_device, &properties);
        support.max_descriptor_set_sampled_images =
            properties.properties.limits.maxDescriptorSetSampledImages;
        support.max_per_stage_descriptor_sampled_images =
            properties.properties.limits.maxPerStageDescriptorSampledImages;
        support.max_descriptor_set_storage_buffers =
            properties.properties.limits.maxDescriptorSetStorageBuffers;
        support.max_per_stage_descriptor_storage_buffers =
            properties.properties.limits.maxPerStageDescriptorStorageBuffers;

        support.query_succeeded = true;
        return support;
    }

    VulkanRayTracingCapabilities negotiateVulkanRayTracingCapabilities(
        const VulkanRayTracingDeviceSupport& support,
        const VulkanRayTracingOptions& options) {
        VulkanRayTracingCapabilities capabilities;
        capabilities.ray_query_mode = options.ray_query_mode;
        capabilities.unavailable_reason.clear();

        if (!support.query_succeeded) {
            capabilities.unavailable_reason = support.query_failure_reason.empty() ?
                "Ray Query capability discovery failed." : support.query_failure_reason;
            return capabilities;
        }

        capabilities.acceleration_structure =
            support.acceleration_structure_extension &&
            support.acceleration_structure_feature;
        capabilities.ray_query = support.ray_query_extension && support.ray_query_feature;
        capabilities.buffer_device_address = support.buffer_device_address_feature;
        capabilities.deferred_host_operations = support.deferred_host_operations_extension;
        capabilities.ray_tracing_pipeline =
            support.ray_tracing_pipeline_extension &&
            support.ray_tracing_pipeline_feature &&
            capabilities.acceleration_structure &&
            capabilities.buffer_device_address &&
            capabilities.deferred_host_operations;
        capabilities.reflection_texture_capacity =
            chooseVulkanReflectionTextureCapacity(
                support.max_descriptor_set_sampled_images,
                support.max_per_stage_descriptor_sampled_images);
        capabilities.reflection_geometry_descriptor_capacity =
            chooseVulkanReflectionGeometryDescriptorCapacity(
                support.max_descriptor_set_storage_buffers,
                support.max_per_stage_descriptor_storage_buffers);
        capabilities.reflection_shading =
            capabilities.reflection_texture_capacity > 0 &&
            capabilities.reflection_geometry_descriptor_capacity > 0 &&
            support.runtime_descriptor_array_feature &&
            support.shader_sampled_image_array_non_uniform_indexing_feature &&
            support.shader_storage_buffer_array_non_uniform_indexing_feature;
        capabilities.min_scratch_alignment = support.min_scratch_alignment;
        capabilities.max_geometry_count = support.max_geometry_count;
        capabilities.max_instance_count = support.max_instance_count;

        if (options.force_disable_ray_query) {
            capabilities.unavailable_reason = "Ray Query force-disabled for testing.";
            return capabilities;
        }
        if (options.ray_query_mode == VulkanRayQueryMode::Disabled) {
            capabilities.unavailable_reason = "Ray Query disabled by renderer configuration.";
            return capabilities;
        }

        capabilities.unavailable_reason = missingRayQueryRequirement(support);
        capabilities.ray_query_enabled = capabilities.unavailable_reason.empty();
        return capabilities;
    }
} // namespace NexAur
