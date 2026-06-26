#pragma once

#include <cstdint>
#include <unordered_map>

#include <vulkan/vulkan.h>

#include "Core/Base.h"

namespace NexAur {
    enum class VulkanShaderProgramId : uint8_t {
        Forward = 0,
        ObjectId,
        Skybox,
        ShadowDepth,
        DebugDraw
    };

    struct VulkanShaderProgram {
        VkShaderModule vertex_module = VK_NULL_HANDLE;
        VkShaderModule fragment_module = VK_NULL_HANDLE;
        const char* vertex_entry = "VSMain";
        const char* fragment_entry = "PSMain";
        bool has_fragment_stage = true;

        bool valid() const {
            return vertex_module != VK_NULL_HANDLE &&
                   vertex_entry != nullptr &&
                   (!has_fragment_stage ||
                    (fragment_module != VK_NULL_HANDLE && fragment_entry != nullptr));
        }
    };

    class NEXAUR_API VulkanShaderLibrary {
    public:
        VulkanShaderLibrary() = default;
        ~VulkanShaderLibrary();

        VulkanShaderLibrary(const VulkanShaderLibrary&) = delete;
        VulkanShaderLibrary& operator=(const VulkanShaderLibrary&) = delete;

        bool init(VkDevice device);
        void shutdown();

        VulkanShaderProgram getProgram(VulkanShaderProgramId program_id);
        bool isInitialized() const { return m_device != VK_NULL_HANDLE; }

    private:
        VkShaderModule getOrCreateShaderModule(VulkanShaderProgramId program_id, VkShaderStageFlagBits stage);

    private:
        VkDevice m_device = VK_NULL_HANDLE;
        std::unordered_map<uint64_t, VkShaderModule> m_shader_modules;
    };
} // namespace NexAur
