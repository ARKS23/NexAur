#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_scene_resource.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_acceleration_structure.h"

namespace NexAur {
    VulkanRayTracingSceneResource::~VulkanRayTracingSceneResource() {
        shutdown();
    }

    bool VulkanRayTracingSceneResource::init(
        VkDevice device,
        VulkanDescriptorAllocator& descriptor_allocator,
        VkDescriptorSetLayout descriptor_set_layout) {
        shutdown();
        if (device == VK_NULL_HANDLE || descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        m_device = device;
        m_descriptor_allocator = &descriptor_allocator;
        m_descriptor_set_layout = descriptor_set_layout;
        m_descriptor_allocation = descriptor_allocator.allocate(descriptor_set_layout);
        if (!m_descriptor_allocation.valid()) {
            shutdown();
            return false;
        }

        return true;
    }

    void VulkanRayTracingSceneResource::shutdown() {
        if (m_descriptor_allocator != nullptr && m_descriptor_allocation.valid()) {
            m_descriptor_allocator->free(m_descriptor_allocation);
        }

        m_descriptor_allocation = {};
        m_acceleration_structure = VK_NULL_HANDLE;
        m_descriptor_set_layout = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_device = VK_NULL_HANDLE;
    }

    bool VulkanRayTracingSceneResource::update(
        const VulkanAccelerationStructure* acceleration_structure) {
        m_acceleration_structure = VK_NULL_HANDLE;
        if (!isInitialized() ||
            acceleration_structure == nullptr ||
            !acceleration_structure->isReady() ||
            (acceleration_structure->getType() !=
                 VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR &&
             acceleration_structure->getType() !=
                 VK_ACCELERATION_STRUCTURE_TYPE_GENERIC_KHR)) {
            return false;
        }

        VulkanDescriptorWriter writer;
        writer.writeAccelerationStructure(
            0,
            acceleration_structure->get());
        writer.update(m_device, m_descriptor_allocation.set);
        m_acceleration_structure = acceleration_structure->get();
        return true;
    }
} // namespace NexAur
