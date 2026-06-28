#include "pch.h"
#include "vulkan_shader_library.h"

#ifdef NX_PLATFORM_WINDOWS
    #include <Windows.h>
#endif

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        std::filesystem::path executableDirectory() {
#ifdef NX_PLATFORM_WINDOWS
            std::array<char, MAX_PATH> buffer{};
            const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length > 0 && length < buffer.size()) {
                return std::filesystem::path(buffer.data()).parent_path();
            }
#endif
            return std::filesystem::current_path();
        }

        std::filesystem::path shaderDirectory() {
            return executableDirectory() / "shaders" / "Renderer" / "Vulkan";
        }

        const char* shaderFileName(VulkanShaderProgramId program_id, VkShaderStageFlagBits stage) {
            switch (program_id) {
                case VulkanShaderProgramId::Forward:
                    return stage == VK_SHADER_STAGE_VERTEX_BIT ? "vulkan_forward.vert.spv" : "vulkan_forward.frag.spv";
                case VulkanShaderProgramId::ObjectId:
                    return stage == VK_SHADER_STAGE_VERTEX_BIT ? "vulkan_object_id.vert.spv" : "vulkan_object_id.frag.spv";
                case VulkanShaderProgramId::Skybox:
                    return stage == VK_SHADER_STAGE_VERTEX_BIT ? "vulkan_skybox.vert.spv" : "vulkan_skybox.frag.spv";
                case VulkanShaderProgramId::ShadowDepth:
                    return stage == VK_SHADER_STAGE_VERTEX_BIT ? "vulkan_shadow_depth.vert.spv" : nullptr;
                case VulkanShaderProgramId::DebugDraw:
                    return stage == VK_SHADER_STAGE_VERTEX_BIT ? "vulkan_debug_draw.vert.spv" : "vulkan_debug_draw.frag.spv";
                case VulkanShaderProgramId::PostProcess:
                    return stage == VK_SHADER_STAGE_VERTEX_BIT ? "vulkan_post_process.vert.spv" : "vulkan_post_process.frag.spv";
                default:
                    return nullptr;
            }
        }

        bool hasFragmentStage(VulkanShaderProgramId program_id) {
            return program_id != VulkanShaderProgramId::ShadowDepth;
        }

        uint64_t shaderModuleKey(VulkanShaderProgramId program_id, VkShaderStageFlagBits stage) {
            return (static_cast<uint64_t>(program_id) << 32u) | static_cast<uint64_t>(stage);
        }

        std::vector<char> readBinaryFile(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::ate | std::ios::binary);
            if (!file.is_open()) {
                NX_CORE_ERROR("Failed to open Vulkan shader: {}", path.string());
                return {};
            }

            const std::streamsize file_size = file.tellg();
            if (file_size <= 0 || file_size % 4 != 0) {
                NX_CORE_ERROR("Invalid Vulkan shader bytecode size: {}", path.string());
                return {};
            }

            std::vector<char> buffer(static_cast<size_t>(file_size));
            file.seekg(0);
            file.read(buffer.data(), file_size);
            return buffer;
        }
    } // namespace

    VulkanShaderLibrary::~VulkanShaderLibrary() {
        shutdown();
    }

    bool VulkanShaderLibrary::init(VkDevice device) {
        shutdown();

        if (device == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanShaderLibrary requires a valid device.");
            return false;
        }

        m_device = device;
        return true;
    }

    void VulkanShaderLibrary::shutdown() {
        if (m_device != VK_NULL_HANDLE) {
            for (auto& [_, shader_module] : m_shader_modules) {
                if (shader_module != VK_NULL_HANDLE) {
                    vkDestroyShaderModule(m_device, shader_module, nullptr);
                }
            }
        }

        m_shader_modules.clear();
        m_device = VK_NULL_HANDLE;
    }

    VulkanShaderProgram VulkanShaderLibrary::getProgram(VulkanShaderProgramId program_id) {
        VulkanShaderProgram program;
        program.has_fragment_stage = hasFragmentStage(program_id);
        program.vertex_module = getOrCreateShaderModule(program_id, VK_SHADER_STAGE_VERTEX_BIT);
        if (program.has_fragment_stage) {
            program.fragment_module = getOrCreateShaderModule(program_id, VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        return program;
    }

    VkShaderModule VulkanShaderLibrary::getOrCreateShaderModule(VulkanShaderProgramId program_id, VkShaderStageFlagBits stage) {
        if (m_device == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanShaderLibrary is not initialized.");
            return VK_NULL_HANDLE;
        }

        const uint64_t key = shaderModuleKey(program_id, stage);
        auto cached_it = m_shader_modules.find(key);
        if (cached_it != m_shader_modules.end()) {
            return cached_it->second;
        }

        const char* file_name = shaderFileName(program_id, stage);
        if (!file_name) {
            NX_CORE_ERROR("Unknown Vulkan shader program id.");
            return VK_NULL_HANDLE;
        }

        const std::vector<char> bytecode = readBinaryFile(shaderDirectory() / file_name);
        if (bytecode.empty()) {
            return VK_NULL_HANDLE;
        }

        VkShaderModuleCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.codeSize = bytecode.size();
        create_info.pCode = reinterpret_cast<const uint32_t*>(bytecode.data());

        VkShaderModule shader_module = VK_NULL_HANDLE;
        if (!checkVk(vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module), "vkCreateShaderModule(shader library)")) {
            return VK_NULL_HANDLE;
        }

        m_shader_modules.emplace(key, shader_module);
        return shader_module;
    }
} // namespace NexAur
