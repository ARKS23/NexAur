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
        const std::vector<VulkanGraphBufferAccess>& getBufferAccesses() const { return m_buffer_accesses; }
        const std::vector<VulkanGraphAccelerationStructureAccess>&
        getAccelerationStructureAccesses() const {
            return m_acceleration_structure_accesses;
        }
        bool execute(VkCommandBuffer command_buffer) const;

    private:
        friend class VulkanPassGraph;
        friend class VulkanGraphPassBuilder;

        std::string m_name;
        std::vector<VulkanGraphImageAccess> m_image_accesses;
        std::vector<VulkanGraphBufferAccess> m_buffer_accesses;
        std::vector<VulkanGraphAccelerationStructureAccess> m_acceleration_structure_accesses;
        ExecuteCallback m_execute;
    };

    class VulkanGraphPassBuilder {
    public:
        explicit VulkanGraphPassBuilder(VulkanGraphPass& pass);

        VulkanGraphPassBuilder& readImage(VulkanGraphImageHandle image, VulkanGraphImageUsage usage);
        VulkanGraphPassBuilder& writeImage(VulkanGraphImageHandle image, VulkanGraphImageUsage usage);
        VulkanGraphPassBuilder& readWriteImage(VulkanGraphImageHandle image, VulkanGraphImageUsage usage);
        VulkanGraphPassBuilder& readBuffer(VulkanGraphBufferHandle buffer, VulkanGraphBufferUsage usage);
        VulkanGraphPassBuilder& writeBuffer(VulkanGraphBufferHandle buffer, VulkanGraphBufferUsage usage);
        VulkanGraphPassBuilder& readWriteBuffer(VulkanGraphBufferHandle buffer, VulkanGraphBufferUsage usage);
        VulkanGraphPassBuilder& readAccelerationStructure(
            VulkanGraphAccelerationStructureHandle acceleration_structure,
            VulkanGraphAccelerationStructureUsage usage);
        VulkanGraphPassBuilder& writeAccelerationStructure(
            VulkanGraphAccelerationStructureHandle acceleration_structure,
            VulkanGraphAccelerationStructureUsage usage);
        VulkanGraphPassBuilder& readWriteAccelerationStructure(
            VulkanGraphAccelerationStructureHandle acceleration_structure,
            VulkanGraphAccelerationStructureUsage usage);
        VulkanGraphPassBuilder& execute(VulkanGraphPass::ExecuteCallback callback);

    private:
        VulkanGraphPass& m_pass;
    };

    class VulkanPassGraph {
    public:
        VulkanGraphImageHandle addImage(VulkanGraphImageDesc desc);
        VulkanGraphBufferHandle addBuffer(VulkanGraphBufferDesc desc);
        VulkanGraphAccelerationStructureHandle addAccelerationStructure(
            VulkanGraphAccelerationStructureDesc desc);
        VulkanGraphPassBuilder addPass(std::string name);

        void clear();
        void commitImageLayouts();

    private:
        friend class VulkanGraphExecutor;

        struct ImageResource {
            VulkanGraphImageDesc desc;
            VulkanGraphImageState state;
        };

        struct BufferResource {
            VulkanGraphBufferDesc desc;
            VulkanGraphBufferState state;
        };

        struct AccelerationStructureResource {
            VulkanGraphAccelerationStructureDesc desc;
            VulkanGraphAccelerationStructureState state;
        };

        ImageResource* getImage(VulkanGraphImageHandle handle);
        const ImageResource* getImage(VulkanGraphImageHandle handle) const;
        BufferResource* getBuffer(VulkanGraphBufferHandle handle);
        const BufferResource* getBuffer(VulkanGraphBufferHandle handle) const;
        AccelerationStructureResource* getAccelerationStructure(
            VulkanGraphAccelerationStructureHandle handle);
        const AccelerationStructureResource* getAccelerationStructure(
            VulkanGraphAccelerationStructureHandle handle) const;

        std::vector<ImageResource> m_images;
        std::vector<BufferResource> m_buffers;
        std::vector<AccelerationStructureResource> m_acceleration_structures;
        std::vector<VulkanGraphPass> m_passes;
    };
} // namespace NexAur
