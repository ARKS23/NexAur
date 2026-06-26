#include "pch.h"
#include "vulkan_pass_graph.h"

#include <utility>

namespace NexAur {
    VulkanGraphPass::VulkanGraphPass(std::string name)
        : m_name(std::move(name)) {}

    bool VulkanGraphPass::execute(VkCommandBuffer command_buffer) const {
        if (!m_execute) {
            return true;
        }

        return m_execute(command_buffer);
    }

    VulkanGraphPassBuilder::VulkanGraphPassBuilder(VulkanGraphPass& pass)
        : m_pass(pass) {}

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::readImage(
        VulkanGraphImageHandle image,
        VulkanGraphImageUsage usage) {
        m_pass.m_image_accesses.push_back({ image, usage });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::writeImage(
        VulkanGraphImageHandle image,
        VulkanGraphImageUsage usage) {
        m_pass.m_image_accesses.push_back({ image, usage });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::execute(VulkanGraphPass::ExecuteCallback callback) {
        m_pass.m_execute = std::move(callback);
        return *this;
    }

    VulkanGraphImageHandle VulkanPassGraph::addImage(VulkanGraphImageDesc desc) {
        if (!desc.valid()) {
            NX_CORE_ERROR("VulkanPassGraph received an invalid image resource: {}", desc.name);
            return {};
        }

        ImageResource resource;
        resource.current_layout = desc.initial_layout;
        resource.desc = std::move(desc);

        VulkanGraphImageHandle handle;
        handle.index = static_cast<uint32_t>(m_images.size());
        m_images.push_back(std::move(resource));
        return handle;
    }

    VulkanGraphPassBuilder VulkanPassGraph::addPass(std::string name) {
        m_passes.emplace_back(std::move(name));
        return VulkanGraphPassBuilder(m_passes.back());
    }

    void VulkanPassGraph::clear() {
        m_passes.clear();
        m_images.clear();
    }

    void VulkanPassGraph::commitImageLayouts() {
        for (ImageResource& image : m_images) {
            if (image.desc.commit_layout) {
                image.desc.commit_layout(image.current_layout);
            }
        }
    }

    VulkanPassGraph::ImageResource* VulkanPassGraph::getImage(VulkanGraphImageHandle handle) {
        if (!handle.valid() || handle.index >= m_images.size()) {
            return nullptr;
        }

        return &m_images[handle.index];
    }

    const VulkanPassGraph::ImageResource* VulkanPassGraph::getImage(VulkanGraphImageHandle handle) const {
        if (!handle.valid() || handle.index >= m_images.size()) {
            return nullptr;
        }

        return &m_images[handle.index];
    }
} // namespace NexAur
