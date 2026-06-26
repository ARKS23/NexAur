#include "pch.h"
#include "vulkan_descriptor_layout_cache.h"

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

        VulkanDescriptorSetLayoutDesc materialDescriptorLayoutDesc() {
            VulkanDescriptorSetLayoutDesc desc;
            desc.bindings = {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
                { 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
                { 2, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            return desc;
        }

        VulkanDescriptorSetLayoutDesc frameGlobalDescriptorLayoutDesc() {
            VulkanDescriptorSetLayoutDesc desc;
            desc.bindings = {
                { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
                { 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            return desc;
        }
    } // namespace

    VulkanDescriptorLayoutCache::~VulkanDescriptorLayoutCache() {
        shutdown();
    }

    bool VulkanDescriptorLayoutCache::init(VkDevice device) {
        shutdown();

        if (device == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanDescriptorLayoutCache requires a valid device.");
            return false;
        }

        m_device = device;
        return true;
    }

    void VulkanDescriptorLayoutCache::shutdown() {
        if (m_device != VK_NULL_HANDLE) {
            for (auto& [_, layout] : m_layouts) {
                if (layout != VK_NULL_HANDLE) {
                    vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
                }
            }
        }

        m_layouts.clear();
        m_device = VK_NULL_HANDLE;
    }

    VkDescriptorSetLayout VulkanDescriptorLayoutCache::getOrCreateLayout(VulkanDescriptorSetLayoutDesc desc) {
        if (m_device == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanDescriptorLayoutCache is not initialized.");
            return VK_NULL_HANDLE;
        }

        desc.normalize();
        if (hasDuplicateBindings(desc)) {
            NX_CORE_ERROR("Descriptor set layout contains duplicate bindings.");
            return VK_NULL_HANDLE;
        }

        auto cached_it = m_layouts.find(desc);
        if (cached_it != m_layouts.end()) {
            return cached_it->second;
        }

        VkDescriptorSetLayout layout = createLayout(desc);
        if (layout == VK_NULL_HANDLE) {
            return VK_NULL_HANDLE;
        }

        m_layouts.emplace(std::move(desc), layout);
        return layout;
    }

    VkDescriptorSetLayout VulkanDescriptorLayoutCache::getBuiltinLayout(VulkanDescriptorSetLayoutId layout_id) {
        switch (layout_id) {
            case VulkanDescriptorSetLayoutId::FrameGlobal:
                return getOrCreateLayout(frameGlobalDescriptorLayoutDesc());
            case VulkanDescriptorSetLayoutId::Material:
                return getOrCreateLayout(materialDescriptorLayoutDesc());
            default:
                NX_CORE_ERROR("Unknown Vulkan descriptor set layout id.");
                return VK_NULL_HANDLE;
        }
    }

    VkDescriptorSetLayout VulkanDescriptorLayoutCache::createLayout(const VulkanDescriptorSetLayoutDesc& desc) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        bindings.reserve(desc.bindings.size());

        std::vector<VkDescriptorBindingFlags> binding_flags;
        binding_flags.reserve(desc.bindings.size());

        bool has_binding_flags = false;
        for (const VulkanDescriptorBindingDesc& binding_desc : desc.bindings) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = binding_desc.binding;
            binding.descriptorType = binding_desc.descriptor_type;
            binding.descriptorCount = binding_desc.descriptor_count;
            binding.stageFlags = binding_desc.stage_flags;
            bindings.push_back(binding);

            binding_flags.push_back(binding_desc.binding_flags);
            has_binding_flags = has_binding_flags || binding_desc.binding_flags != 0;
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_info{};
        binding_flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        binding_flags_info.bindingCount = static_cast<uint32_t>(binding_flags.size());
        binding_flags_info.pBindingFlags = binding_flags.data();

        VkDescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layout_info.pNext = has_binding_flags ? &binding_flags_info : nullptr;
        layout_info.flags = desc.flags;
        layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
        layout_info.pBindings = bindings.empty() ? nullptr : bindings.data();

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if (!checkVk(vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr, &layout), "vkCreateDescriptorSetLayout(cache)")) {
            return VK_NULL_HANDLE;
        }

        return layout;
    }

    bool VulkanDescriptorLayoutCache::hasDuplicateBindings(const VulkanDescriptorSetLayoutDesc& desc) const {
        for (size_t index = 1; index < desc.bindings.size(); ++index) {
            if (desc.bindings[index - 1].binding == desc.bindings[index].binding) {
                return true;
            }
        }

        return false;
    }
} // namespace NexAur
