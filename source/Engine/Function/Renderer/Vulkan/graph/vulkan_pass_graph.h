#pragma once

#include <functional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/graph/vulkan_graph_resource.h"

namespace NexAur {
    class VulkanGraphExecutor;

    class VulkanGraphPass {
    public:
        using ExecuteCallback = std::function<bool(VkCommandBuffer)>;

        explicit VulkanGraphPass(std::string name);

        const std::string& getName() const { return m_name; }
        const std::vector<VulkanGraphImageAccess>& getImageAccesses() const { return m_image_accesses; }
        bool execute(VkCommandBuffer command_buffer) const;

    private:
        friend class VulkanPassGraph;
        friend class VulkanGraphPassBuilder;

        std::string m_name;
        std::vector<VulkanGraphImageAccess> m_image_accesses;
        ExecuteCallback m_execute;
    };

    class VulkanGraphPassBuilder {
    public:
        explicit VulkanGraphPassBuilder(VulkanGraphPass& pass);

        VulkanGraphPassBuilder& readImage(VulkanGraphImageHandle image, VulkanGraphImageUsage usage);
        VulkanGraphPassBuilder& writeImage(VulkanGraphImageHandle image, VulkanGraphImageUsage usage);
        VulkanGraphPassBuilder& execute(VulkanGraphPass::ExecuteCallback callback);

    private:
        VulkanGraphPass& m_pass;
    };

    class NEXAUR_API VulkanPassGraph {
    public:
        VulkanGraphImageHandle addImage(VulkanGraphImageDesc desc);
        VulkanGraphPassBuilder addPass(std::string name);

        void clear();
        void commitImageLayouts();

    private:
        friend class VulkanGraphExecutor;

        struct ImageResource {
            VulkanGraphImageDesc desc;
            VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        };

        ImageResource* getImage(VulkanGraphImageHandle handle);
        const ImageResource* getImage(VulkanGraphImageHandle handle) const;

        std::vector<ImageResource> m_images;
        std::vector<VulkanGraphPass> m_passes;
    };
} // namespace NexAur
