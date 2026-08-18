#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_ray_tracing_functions.h"

namespace NexAur {
    bool VulkanRayTracingDeviceFunctions::load(VkDevice device) {
        reset();
        if (device == VK_NULL_HANDLE) {
            return false;
        }

        create_acceleration_structure = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
        destroy_acceleration_structure = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
        get_acceleration_structure_build_sizes =
            reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
                vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
        cmd_build_acceleration_structures =
            reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
                vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
        cmd_copy_acceleration_structure =
            reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
                vkGetDeviceProcAddr(device, "vkCmdCopyAccelerationStructureKHR"));
        cmd_write_acceleration_structures_properties =
            reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(
                vkGetDeviceProcAddr(
                    device,
                    "vkCmdWriteAccelerationStructuresPropertiesKHR"));
        get_acceleration_structure_device_address =
            reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
                vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
        set_debug_utils_object_name = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
            vkGetDeviceProcAddr(device, "vkSetDebugUtilsObjectNameEXT"));
        if (!valid()) {
            reset();
            return false;
        }
        return true;
    }

    void VulkanRayTracingDeviceFunctions::reset() {
        create_acceleration_structure = nullptr;
        destroy_acceleration_structure = nullptr;
        get_acceleration_structure_build_sizes = nullptr;
        cmd_build_acceleration_structures = nullptr;
        cmd_copy_acceleration_structure = nullptr;
        cmd_write_acceleration_structures_properties = nullptr;
        get_acceleration_structure_device_address = nullptr;
        set_debug_utils_object_name = nullptr;
    }

    bool VulkanRayTracingDeviceFunctions::valid() const {
        return create_acceleration_structure != nullptr &&
               destroy_acceleration_structure != nullptr &&
               get_acceleration_structure_build_sizes != nullptr &&
               cmd_build_acceleration_structures != nullptr &&
               get_acceleration_structure_device_address != nullptr;
    }

    bool VulkanRayTracingDeviceFunctions::supportsCompaction() const {
        return cmd_copy_acceleration_structure != nullptr &&
               cmd_write_acceleration_structures_properties != nullptr;
    }
} // namespace NexAur
