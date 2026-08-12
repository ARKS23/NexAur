#pragma once

#include <cstdint>
#include <string>

#include <VkBootstrap.h>
#include <vulkan/vulkan.h>

#include "Core/Log/log_system.h"

namespace NexAur {
    class VulkanDiagnosticsCollector final {
    public:
        static const char* vkResultToString(VkResult result);
        static std::string vkFormatToString(VkFormat format);
        static std::string apiVersionToString(uint32_t api_version);

        static bool checkVk(VkResult result, const char* operation);

        template<typename T>
        static void logVkbFailure(const char* operation, const vkb::Result<T>& result) {
            if (result) {
                return;
            }

            NX_CORE_ERROR(
                "{} failed: {} ({})",
                operation,
                result.error().message(),
                vkResultToString(result.vk_result()));
            for (const std::string& reason : result.detailed_failure_reasons()) {
                NX_CORE_ERROR("{} detail: {}", operation, reason);
            }
        }

        static bool requireFeature(VkBool32 supported, const char* feature_name);

        static VkFormat findHdrSceneColorFormat(VkPhysicalDevice physical_device);
        static VkFormat findAoFormat(VkPhysicalDevice physical_device);
        static VkFormat findSmaaMaskFormat(VkPhysicalDevice physical_device);
    };
} // namespace NexAur
