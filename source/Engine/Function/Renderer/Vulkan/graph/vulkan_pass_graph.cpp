#include "pch.h"
#include "vulkan_pass_graph.h"

#include "Function/Renderer/Vulkan/graph/vulkan_graph_state_planner.h"

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
        m_pass.m_image_accesses.push_back({ image, usage, VulkanGraphAccessType::Read });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::writeImage(
        VulkanGraphImageHandle image,
        VulkanGraphImageUsage usage) {
        m_pass.m_image_accesses.push_back({ image, usage, VulkanGraphAccessType::Write });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::readWriteImage(
        VulkanGraphImageHandle image,
        VulkanGraphImageUsage usage) {
        m_pass.m_image_accesses.push_back({ image, usage, VulkanGraphAccessType::ReadWrite });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::readBuffer(
        VulkanGraphBufferHandle buffer,
        VulkanGraphBufferUsage usage) {
        m_pass.m_buffer_accesses.push_back({ buffer, usage, VulkanGraphAccessType::Read });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::writeBuffer(
        VulkanGraphBufferHandle buffer,
        VulkanGraphBufferUsage usage) {
        m_pass.m_buffer_accesses.push_back({ buffer, usage, VulkanGraphAccessType::Write });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::readWriteBuffer(
        VulkanGraphBufferHandle buffer,
        VulkanGraphBufferUsage usage) {
        m_pass.m_buffer_accesses.push_back({ buffer, usage, VulkanGraphAccessType::ReadWrite });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::readAccelerationStructure(
        VulkanGraphAccelerationStructureHandle acceleration_structure,
        VulkanGraphAccelerationStructureUsage usage) {
        m_pass.m_acceleration_structure_accesses.push_back({
            acceleration_structure,
            usage,
            VulkanGraphAccessType::Read
        });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::writeAccelerationStructure(
        VulkanGraphAccelerationStructureHandle acceleration_structure,
        VulkanGraphAccelerationStructureUsage usage) {
        m_pass.m_acceleration_structure_accesses.push_back({
            acceleration_structure,
            usage,
            VulkanGraphAccessType::Write
        });
        return *this;
    }

    VulkanGraphPassBuilder& VulkanGraphPassBuilder::readWriteAccelerationStructure(
        VulkanGraphAccelerationStructureHandle acceleration_structure,
        VulkanGraphAccelerationStructureUsage usage) {
        m_pass.m_acceleration_structure_accesses.push_back({
            acceleration_structure,
            usage,
            VulkanGraphAccessType::ReadWrite
        });
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
        resource.state = VulkanGraphStatePlanner::stateForImport(
            desc.initial_layout,
            desc.subresource_range,
            desc.external_acquire_stage);
        resource.desc = std::move(desc);

        VulkanGraphImageHandle handle;
        handle.index = static_cast<uint32_t>(m_images.size());
        m_images.push_back(std::move(resource));
        return handle;
    }

    VulkanGraphBufferHandle VulkanPassGraph::addBuffer(VulkanGraphBufferDesc desc) {
        if (!desc.valid()) {
            NX_CORE_ERROR("VulkanPassGraph received an invalid buffer resource: {}", desc.name);
            return {};
        }

        BufferResource resource;
        resource.state = VulkanGraphStatePlanner::stateForBufferImport(
            desc.initial_stage,
            desc.initial_access,
            desc.initial_access_type);
        resource.desc = std::move(desc);

        VulkanGraphBufferHandle handle;
        handle.index = static_cast<uint32_t>(m_buffers.size());
        m_buffers.push_back(std::move(resource));
        return handle;
    }

    VulkanGraphAccelerationStructureHandle VulkanPassGraph::addAccelerationStructure(
        VulkanGraphAccelerationStructureDesc desc) {
        if (!desc.valid()) {
            NX_CORE_ERROR(
                "VulkanPassGraph received an invalid acceleration structure resource: {}",
                desc.name);
            return {};
        }

        AccelerationStructureResource resource;
        resource.state = VulkanGraphStatePlanner::stateForAccelerationStructureImport(
            desc.initial_stage,
            desc.initial_access,
            desc.initial_access_type);
        resource.desc = std::move(desc);

        VulkanGraphAccelerationStructureHandle handle;
        handle.index = static_cast<uint32_t>(m_acceleration_structures.size());
        m_acceleration_structures.push_back(std::move(resource));
        return handle;
    }

    VulkanGraphPassBuilder VulkanPassGraph::addPass(std::string name) {
        m_passes.emplace_back(std::move(name));
        return VulkanGraphPassBuilder(m_passes.back());
    }

    void VulkanPassGraph::clear() {
        m_passes.clear();
        m_images.clear();
        m_buffers.clear();
        m_acceleration_structures.clear();
    }

    void VulkanPassGraph::commitImageLayouts() {
        for (ImageResource& image : m_images) {
            if (image.desc.commit_layout) {
                image.desc.commit_layout(image.state.layout);
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

    VulkanPassGraph::BufferResource* VulkanPassGraph::getBuffer(VulkanGraphBufferHandle handle) {
        if (!handle.valid() || handle.index >= m_buffers.size()) {
            return nullptr;
        }

        return &m_buffers[handle.index];
    }

    const VulkanPassGraph::BufferResource* VulkanPassGraph::getBuffer(
        VulkanGraphBufferHandle handle) const {
        if (!handle.valid() || handle.index >= m_buffers.size()) {
            return nullptr;
        }

        return &m_buffers[handle.index];
    }

    VulkanPassGraph::AccelerationStructureResource*
    VulkanPassGraph::getAccelerationStructure(
        VulkanGraphAccelerationStructureHandle handle) {
        if (!handle.valid() || handle.index >= m_acceleration_structures.size()) {
            return nullptr;
        }

        return &m_acceleration_structures[handle.index];
    }

    const VulkanPassGraph::AccelerationStructureResource*
    VulkanPassGraph::getAccelerationStructure(
        VulkanGraphAccelerationStructureHandle handle) const {
        if (!handle.valid() || handle.index >= m_acceleration_structures.size()) {
            return nullptr;
        }

        return &m_acceleration_structures[handle.index];
    }
} // namespace NexAur
