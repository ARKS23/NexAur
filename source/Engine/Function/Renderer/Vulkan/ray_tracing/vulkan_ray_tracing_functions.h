#pragma once

#include <vulkan/vulkan.h>

namespace NexAur {
    struct VulkanRayTracingDeviceFunctions {
        PFN_vkCreateAccelerationStructureKHR create_acceleration_structure = nullptr;
        PFN_vkDestroyAccelerationStructureKHR destroy_acceleration_structure = nullptr;
        PFN_vkGetAccelerationStructureBuildSizesKHR get_acceleration_structure_build_sizes = nullptr;
        PFN_vkCmdBuildAccelerationStructuresKHR cmd_build_acceleration_structures = nullptr;
        PFN_vkCmdCopyAccelerationStructureKHR cmd_copy_acceleration_structure = nullptr;
        PFN_vkCmdWriteAccelerationStructuresPropertiesKHR
            cmd_write_acceleration_structures_properties = nullptr;
        PFN_vkGetAccelerationStructureDeviceAddressKHR get_acceleration_structure_device_address = nullptr;
        PFN_vkSetDebugUtilsObjectNameEXT set_debug_utils_object_name = nullptr;

        bool load(VkDevice device);
        void reset();
        bool valid() const;
        bool supportsCompaction() const;
    };
} // namespace NexAur
