#include "pch.h"
#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"

#include <array>
#include <cstddef>

namespace NexAur {
    const char* VulkanDiagnosticsCollector::vkResultToString(VkResult result) {
        switch (result) {
        case VK_SUCCESS:
            return "VK_SUCCESS";
        case VK_NOT_READY:
            return "VK_NOT_READY";
        case VK_TIMEOUT:
            return "VK_TIMEOUT";
        case VK_EVENT_SET:
            return "VK_EVENT_SET";
        case VK_EVENT_RESET:
            return "VK_EVENT_RESET";
        case VK_INCOMPLETE:
            return "VK_INCOMPLETE";
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED:
            return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST:
            return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED:
            return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT:
            return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT:
            return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT:
            return "VK_ERROR_FEATURE_NOT_PRESENT";
        case VK_ERROR_INCOMPATIBLE_DRIVER:
            return "VK_ERROR_INCOMPATIBLE_DRIVER";
        case VK_ERROR_TOO_MANY_OBJECTS:
            return "VK_ERROR_TOO_MANY_OBJECTS";
        case VK_ERROR_FORMAT_NOT_SUPPORTED:
            return "VK_ERROR_FORMAT_NOT_SUPPORTED";
        case VK_ERROR_SURFACE_LOST_KHR:
            return "VK_ERROR_SURFACE_LOST_KHR";
        case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
            return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
        case VK_SUBOPTIMAL_KHR:
            return "VK_SUBOPTIMAL_KHR";
        case VK_ERROR_OUT_OF_DATE_KHR:
            return "VK_ERROR_OUT_OF_DATE_KHR";
        default:
            return "Unknown VkResult";
        }
    }

    std::string VulkanDiagnosticsCollector::vkFormatToString(VkFormat format) {
        switch (format) {
        case VK_FORMAT_UNDEFINED:
            return "Undefined";
        case VK_FORMAT_B8G8R8A8_SRGB:
            return "B8G8R8A8_SRGB";
        case VK_FORMAT_B8G8R8A8_UNORM:
            return "B8G8R8A8_UNORM";
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            return "R16G16B16A16_SFLOAT";
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            return "B10G11R11_UFLOAT_PACK32";
        case VK_FORMAT_R8_UNORM:
            return "R8_UNORM";
        case VK_FORMAT_R16_SFLOAT:
            return "R16_SFLOAT";
        case VK_FORMAT_R32_SINT:
            return "R32_SINT";
        case VK_FORMAT_D16_UNORM:
            return "D16_UNORM";
        case VK_FORMAT_D24_UNORM_S8_UINT:
            return "D24_UNORM_S8_UINT";
        case VK_FORMAT_D32_SFLOAT:
            return "D32_SFLOAT";
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            return "D32_SFLOAT_S8_UINT";
        default:
            return "VkFormat(" + std::to_string(static_cast<int>(format)) + ")";
        }
    }

    std::string VulkanDiagnosticsCollector::apiVersionToString(uint32_t api_version) {
        return std::to_string(VK_API_VERSION_MAJOR(api_version)) + "." +
               std::to_string(VK_API_VERSION_MINOR(api_version)) + "." +
               std::to_string(VK_API_VERSION_PATCH(api_version));
    }

    bool VulkanDiagnosticsCollector::checkVk(VkResult result, const char* operation) {
        if (result == VK_SUCCESS) {
            return true;
        }

        NX_CORE_ERROR(
            "{} failed: {} ({})",
            operation,
            vkResultToString(result),
            static_cast<int>(result));
        return false;
    }

    bool VulkanDiagnosticsCollector::requireFeature(VkBool32 supported, const char* feature_name) {
        if (supported == VK_TRUE) {
            return true;
        }

        NX_CORE_ERROR("Vulkan 1.3 feature is required but not supported: {}", feature_name);
        return false;
    }

    namespace {
        VkFormat findFormat(
            VkPhysicalDevice physical_device,
            const VkFormat* candidates,
            size_t candidate_count,
            VkFormatFeatureFlags required_features =
                VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) {

            for (size_t index = 0; index < candidate_count; ++index) {
                VkFormatProperties properties{};
                vkGetPhysicalDeviceFormatProperties(physical_device, candidates[index], &properties);
                if ((properties.optimalTilingFeatures & required_features) == required_features) {
                    return candidates[index];
                }
            }

            return VK_FORMAT_UNDEFINED;
        }
    } // namespace

    VkFormat VulkanDiagnosticsCollector::findHdrSceneColorFormat(VkPhysicalDevice physical_device) {
        constexpr std::array<VkFormat, 2> candidates{
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_FORMAT_B10G11R11_UFLOAT_PACK32
        };
        return findFormat(physical_device, candidates.data(), candidates.size());
    }

    VkFormat VulkanDiagnosticsCollector::findReflectionSurfaceFormat(VkPhysicalDevice physical_device) {
        constexpr std::array<VkFormat, 1> candidates{
            VK_FORMAT_R16G16B16A16_SFLOAT
        };
        return findFormat(
            physical_device,
            candidates.data(),
            candidates.size(),
            VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);
    }

    VkFormat VulkanDiagnosticsCollector::findMotionVectorFormat(VkPhysicalDevice physical_device) {
        constexpr std::array<VkFormat, 2> candidates{
            VK_FORMAT_R16G16_SFLOAT,
            VK_FORMAT_R32G32_SFLOAT
        };
        return findFormat(physical_device, candidates.data(), candidates.size());
    }

    VkFormat VulkanDiagnosticsCollector::findAoFormat(VkPhysicalDevice physical_device) {
        constexpr std::array<VkFormat, 2> candidates{
            VK_FORMAT_R8_UNORM,
            VK_FORMAT_R16_SFLOAT
        };
        return findFormat(physical_device, candidates.data(), candidates.size());
    }

    VkFormat VulkanDiagnosticsCollector::findSmaaMaskFormat(VkPhysicalDevice physical_device) {
        constexpr std::array<VkFormat, 3> candidates{
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_FORMAT_B8G8R8A8_UNORM,
            VK_FORMAT_R16G16B16A16_SFLOAT
        };
        return findFormat(physical_device, candidates.data(), candidates.size());
    }
} // namespace NexAur
