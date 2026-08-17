#include "pch.h"
#include "Function/Renderer/Vulkan/ray_tracing/vulkan_acceleration_structure.h"

#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"
#include "Function/Renderer/Vulkan/core/vulkan_retirement_queue.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace NexAur {
    namespace {
        template<typename Handle>
        uint64_t handleToUint64(Handle handle) {
            if constexpr (std::is_pointer_v<Handle>) {
                return reinterpret_cast<uint64_t>(handle);
            } else {
                return static_cast<uint64_t>(handle);
            }
        }

        struct RetiredAccelerationStructure {
            VkDevice device = VK_NULL_HANDLE;
            PFN_vkDestroyAccelerationStructureKHR destroy_function = nullptr;
            VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
            VulkanOwnedBuffer backing_buffer;

            RetiredAccelerationStructure(
                VkDevice retired_device,
                PFN_vkDestroyAccelerationStructureKHR retired_destroy_function,
                VkAccelerationStructureKHR retired_handle,
                VulkanOwnedBuffer&& retired_backing_buffer)
                : device(retired_device),
                  destroy_function(retired_destroy_function),
                  handle(retired_handle),
                  backing_buffer(std::move(retired_backing_buffer)) {}

            RetiredAccelerationStructure(const RetiredAccelerationStructure&) = delete;
            RetiredAccelerationStructure& operator=(const RetiredAccelerationStructure&) = delete;

            RetiredAccelerationStructure(RetiredAccelerationStructure&& other) noexcept
                : device(other.device),
                  destroy_function(other.destroy_function),
                  handle(other.handle),
                  backing_buffer(std::move(other.backing_buffer)) {
                other.device = VK_NULL_HANDLE;
                other.destroy_function = nullptr;
                other.handle = VK_NULL_HANDLE;
            }

            RetiredAccelerationStructure& operator=(RetiredAccelerationStructure&&) = delete;

            ~RetiredAccelerationStructure() {
                if (device != VK_NULL_HANDLE &&
                    destroy_function != nullptr &&
                    handle != VK_NULL_HANDLE) {
                    destroy_function(device, handle, nullptr);
                }
                backing_buffer.reset();
            }
        };

        void setAccelerationStructureDebugName(
            VkDevice device,
            const VulkanRayTracingDeviceFunctions& functions,
            VkAccelerationStructureKHR handle,
            const std::string& debug_name) {
            if (device == VK_NULL_HANDLE ||
                functions.set_debug_utils_object_name == nullptr ||
                handle == VK_NULL_HANDLE ||
                debug_name.empty()) {
                return;
            }

            VkDebugUtilsObjectNameInfoEXT name_info{};
            name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
            name_info.objectType = VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR;
            name_info.objectHandle = handleToUint64(handle);
            name_info.pObjectName = debug_name.c_str();
            const VkResult result = functions.set_debug_utils_object_name(device, &name_info);
            if (result != VK_SUCCESS) {
                NX_CORE_WARN(
                    "Failed to name Vulkan acceleration structure '{}': {} ({}).",
                    debug_name,
                    VulkanDiagnosticsCollector::vkResultToString(result),
                    static_cast<int>(result));
            }
        }
    } // namespace

    VkDeviceSize alignVulkanAccelerationStructureScratchSize(
        VkDeviceSize size,
        VkDeviceSize alignment) {
        if (size == 0 ||
            alignment == 0 ||
            (alignment & (alignment - 1)) != 0 ||
            size > std::numeric_limits<VkDeviceSize>::max() - (alignment - 1)) {
            return 0;
        }
        return (size + alignment - 1) & ~(alignment - 1);
    }

    bool queryVulkanAccelerationStructureBuildSizes(
        VkDevice device,
        const VulkanRayTracingDeviceFunctions& functions,
        VkAccelerationStructureBuildTypeKHR build_type,
        const VkAccelerationStructureBuildGeometryInfoKHR& build_info,
        std::span<const uint32_t> max_primitive_counts,
        VulkanAccelerationStructureBuildSizes& sizes) {
        sizes = {};
        const bool has_geometry_data =
            (build_info.pGeometries != nullptr) !=
            (build_info.ppGeometries != nullptr);
        if (device == VK_NULL_HANDLE ||
            !functions.valid() ||
            build_info.sType !=
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR ||
            build_info.geometryCount == 0 ||
            build_info.geometryCount != max_primitive_counts.size() ||
            !has_geometry_data) {
            NX_CORE_ERROR("Invalid acceleration structure build-size query.");
            return false;
        }

        VkAccelerationStructureBuildSizesInfoKHR size_info{};
        size_info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        functions.get_acceleration_structure_build_sizes(
            device,
            build_type,
            &build_info,
            max_primitive_counts.data(),
            &size_info);

        sizes.acceleration_structure_size = size_info.accelerationStructureSize;
        sizes.build_scratch_size = size_info.buildScratchSize;
        sizes.update_scratch_size = size_info.updateScratchSize;
        if (!sizes.valid()) {
            NX_CORE_ERROR(
                "Acceleration structure build-size query returned invalid sizes: AS {}, build scratch {}, update scratch {}.",
                sizes.acceleration_structure_size,
                sizes.build_scratch_size,
                sizes.update_scratch_size);
            sizes = {};
            return false;
        }
        return true;
    }

    VulkanAccelerationStructureScratchBuffer::VulkanAccelerationStructureScratchBuffer(
        VulkanAccelerationStructureScratchBuffer&& other) noexcept {
        moveFrom(std::move(other));
    }

    VulkanAccelerationStructureScratchBuffer&
    VulkanAccelerationStructureScratchBuffer::operator=(
        VulkanAccelerationStructureScratchBuffer&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(std::move(other));
        }
        return *this;
    }

    bool VulkanAccelerationStructureScratchBuffer::ensureCapacity(
        const VulkanGpuAllocator& allocator,
        VkDeviceSize required_size,
        VkDeviceSize alignment,
        const char* debug_name) {
        const VkDeviceSize aligned_size =
            alignVulkanAccelerationStructureScratchSize(required_size, alignment);
        if (!allocator.isInitialized() ||
            !allocator.isBufferDeviceAddressEnabled() ||
            aligned_size == 0) {
            NX_CORE_ERROR("Invalid acceleration structure scratch buffer request.");
            return false;
        }

        if (isReady() &&
            m_capacity >= aligned_size &&
            getDeviceAddress() % alignment == 0) {
            return true;
        }

        VulkanOwnedBufferCreateInfo create_info;
        create_info.size = aligned_size;
        create_info.usage =
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        create_info.memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        create_info.minimum_alignment = alignment;
        create_info.debug_name = debug_name != nullptr && debug_name[0] != '\0' ?
            debug_name : "Vulkan acceleration structure scratch buffer";

        VulkanOwnedBuffer buffer;
        if (!buffer.create(allocator, create_info)) {
            return false;
        }

        m_buffer = std::move(buffer);
        m_capacity = aligned_size;
        m_alignment = alignment;
        return true;
    }

    void VulkanAccelerationStructureScratchBuffer::reset() {
        m_buffer.reset();
        m_capacity = 0;
        m_alignment = 1;
    }

    void VulkanAccelerationStructureScratchBuffer::moveFrom(
        VulkanAccelerationStructureScratchBuffer&& other) noexcept {
        m_buffer = std::move(other.m_buffer);
        m_capacity = other.m_capacity;
        m_alignment = other.m_alignment;

        other.m_capacity = 0;
        other.m_alignment = 1;
    }

    VulkanAccelerationStructure::~VulkanAccelerationStructure() {
        reset();
    }

    VulkanAccelerationStructure::VulkanAccelerationStructure(
        VulkanAccelerationStructure&& other) noexcept {
        moveFrom(std::move(other));
    }

    VulkanAccelerationStructure& VulkanAccelerationStructure::operator=(
        VulkanAccelerationStructure&& other) noexcept {
        if (this != &other) {
            reset();
            moveFrom(std::move(other));
        }
        return *this;
    }

    bool VulkanAccelerationStructure::create(
        const VulkanGpuAllocator& allocator,
        const VulkanRayTracingDeviceFunctions& functions,
        const VulkanAccelerationStructureCreateInfo& create_info) {
        reset();
        if (!allocator.isInitialized() ||
            !allocator.isBufferDeviceAddressEnabled() ||
            !functions.valid() ||
            !create_info.valid()) {
            NX_CORE_ERROR("Invalid Vulkan acceleration structure create request.");
            return false;
        }

        VulkanOwnedBufferCreateInfo backing_info;
        backing_info.size = create_info.size;
        backing_info.usage =
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        backing_info.memory_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        backing_info.debug_name = create_info.debug_name.empty() ?
            "Vulkan acceleration structure backing buffer" :
            create_info.debug_name + " backing buffer";

        VulkanOwnedBuffer backing_buffer;
        if (!backing_buffer.create(allocator, backing_info)) {
            return false;
        }

        VkAccelerationStructureCreateInfoKHR acceleration_structure_info{};
        acceleration_structure_info.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        acceleration_structure_info.buffer = backing_buffer.get();
        acceleration_structure_info.size = create_info.size;
        acceleration_structure_info.type = create_info.type;

        VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
        if (!VulkanDiagnosticsCollector::checkVk(
                functions.create_acceleration_structure(
                    allocator.getDevice(),
                    &acceleration_structure_info,
                    nullptr,
                    &handle),
                "vkCreateAccelerationStructureKHR")) {
            backing_buffer.reset();
            return false;
        }

        setAccelerationStructureDebugName(
            allocator.getDevice(),
            functions,
            handle,
            create_info.debug_name);

        VkAccelerationStructureDeviceAddressInfoKHR address_info{};
        address_info.sType =
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        address_info.accelerationStructure = handle;
        const VkDeviceAddress device_address =
            functions.get_acceleration_structure_device_address(
                allocator.getDevice(),
                &address_info);
        if (device_address == 0) {
            NX_CORE_ERROR("vkGetAccelerationStructureDeviceAddressKHR returned zero.");
            functions.destroy_acceleration_structure(
                allocator.getDevice(),
                handle,
                nullptr);
            backing_buffer.reset();
            return false;
        }

        m_functions = functions;
        m_retirement_queue = allocator.getRetirementQueue();
        m_device = allocator.getDevice();
        m_backing_buffer = std::move(backing_buffer);
        m_handle = handle;
        m_type = create_info.type;
        m_device_address = device_address;
        m_size = create_info.size;
        m_debug_name = create_info.debug_name.empty() ?
            "Vulkan acceleration structure" : create_info.debug_name;
        return true;
    }

    bool VulkanAccelerationStructure::recordBuild(
        VkCommandBuffer command_buffer,
        VkAccelerationStructureBuildGeometryInfoKHR build_info,
        std::span<const VkAccelerationStructureBuildRangeInfoKHR> build_ranges,
        VkDeviceAddress scratch_address) const {
        const bool has_geometry_data =
            (build_info.pGeometries != nullptr) !=
            (build_info.ppGeometries != nullptr);
        if (!isReady() ||
            command_buffer == VK_NULL_HANDLE ||
            scratch_address == 0 ||
            build_info.sType !=
                VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR ||
            build_info.type != m_type ||
            build_info.geometryCount == 0 ||
            build_info.geometryCount != build_ranges.size() ||
            !has_geometry_data ||
            (build_info.mode == VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR &&
             build_info.srcAccelerationStructure == VK_NULL_HANDLE)) {
            NX_CORE_ERROR("Invalid acceleration structure build command for '{}'.", m_debug_name);
            return false;
        }

        build_info.dstAccelerationStructure = m_handle;
        build_info.scratchData.deviceAddress = scratch_address;
        const VkAccelerationStructureBuildRangeInfoKHR* range_info = build_ranges.data();
        m_functions.cmd_build_acceleration_structures(
            command_buffer,
            1,
            &build_info,
            &range_info);
        return true;
    }

    void VulkanAccelerationStructure::reset() {
        if (m_handle != VK_NULL_HANDLE) {
            RetiredAccelerationStructure retired(
                m_device,
                m_functions.destroy_acceleration_structure,
                m_handle,
                std::move(m_backing_buffer));
            if (m_retirement_queue != nullptr) {
                m_retirement_queue->retire(std::move(retired));
            }
        } else {
            m_backing_buffer.reset();
        }

        m_functions.reset();
        m_retirement_queue = nullptr;
        m_device = VK_NULL_HANDLE;
        m_handle = VK_NULL_HANDLE;
        m_type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
        m_device_address = 0;
        m_size = 0;
        m_debug_name.clear();
    }

    void VulkanAccelerationStructure::moveFrom(
        VulkanAccelerationStructure&& other) noexcept {
        m_functions = other.m_functions;
        m_retirement_queue = other.m_retirement_queue;
        m_device = other.m_device;
        m_backing_buffer = std::move(other.m_backing_buffer);
        m_handle = other.m_handle;
        m_type = other.m_type;
        m_device_address = other.m_device_address;
        m_size = other.m_size;
        m_debug_name = std::move(other.m_debug_name);

        other.m_functions.reset();
        other.m_retirement_queue = nullptr;
        other.m_device = VK_NULL_HANDLE;
        other.m_handle = VK_NULL_HANDLE;
        other.m_type = VK_ACCELERATION_STRUCTURE_TYPE_MAX_ENUM_KHR;
        other.m_device_address = 0;
        other.m_size = 0;
        other.m_debug_name.clear();
    }
} // namespace NexAur
