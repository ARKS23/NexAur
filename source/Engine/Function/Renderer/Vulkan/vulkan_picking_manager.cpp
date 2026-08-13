#include "pch.h"
#include "vulkan_picking_manager.h"

#include "Function/Renderer/Vulkan/diagnostics/vulkan_diagnostics_collector.h"
#include "Function/Renderer/Vulkan/graph/vulkan_pass_graph.h"

namespace NexAur {
    namespace {
        void transitionObjectIdToTransferSource(
            VkCommandBuffer command_buffer,
            VkImage image,
            VkImageLayout old_layout) {
            if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                return;
            }

            VkAccessFlags src_access = 0;
            VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
                src_access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            }

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = src_access;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.oldLayout = old_layout;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(
                command_buffer,
                src_stage,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }
    } // namespace

    VulkanPickingManager::~VulkanPickingManager() {
        shutdown();
    }

    bool VulkanPickingManager::init(
        const VulkanResourceContext& context,
        VkCommandPool command_pool,
        uint32_t width,
        uint32_t height) {
        shutdown();

        if (!context.valid() || command_pool == VK_NULL_HANDLE) {
            NX_CORE_ERROR("VulkanPickingManager requires a valid Vulkan context and command pool.");
            return false;
        }

        m_device = context.device;
        m_graphics_queue = context.graphics_queue;
        m_command_pool = command_pool;
        if (!m_target.init(context, width, height)) {
            shutdown();
            return false;
        }

        return true;
    }

    bool VulkanPickingManager::resize(uint32_t width, uint32_t height) {
        if (!m_target.resize(width, height)) {
            return false;
        }

        m_frame_ready = false;
        m_recorded_this_frame = false;
        return true;
    }

    void VulkanPickingManager::shutdown() {
        m_target.shutdown();
        m_device = VK_NULL_HANDLE;
        m_graphics_queue = VK_NULL_HANDLE;
        m_command_pool = VK_NULL_HANDLE;
        m_frame_ready = false;
        m_recorded_this_frame = false;
    }

    VulkanGraphImageHandle VulkanPickingManager::addObjectIdImage(VulkanPassGraph& graph) {
        if (!m_target.isReady()) {
            return {};
        }

        VulkanGraphImageDesc desc;
        desc.name = "PickingObjectId";
        desc.image = m_target.getObjectIdImage();
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        desc.initial_layout = m_target.getObjectIdLayout();
        desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setObjectIdLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    VulkanGraphImageHandle VulkanPickingManager::addDepthImage(VulkanPassGraph& graph) {
        if (!m_target.isReady()) {
            return {};
        }

        VulkanGraphImageDesc desc;
        desc.name = "PickingDepth";
        desc.image = m_target.getDepthImage();
        desc.subresource_range.aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
        desc.initial_layout = m_target.getDepthLayout();
        desc.commit_layout = [this](VkImageLayout layout) {
            m_target.setDepthLayout(layout);
        };
        return graph.addImage(std::move(desc));
    }

    void VulkanPickingManager::beginFrameRecording() {
        m_recorded_this_frame = false;
    }

    void VulkanPickingManager::markPassRecorded() {
        m_recorded_this_frame = true;
    }

    void VulkanPickingManager::onFrameSubmitted() {
        if (m_recorded_this_frame) {
            m_frame_ready = true;
        }
    }

    ViewportPickResult VulkanPickingManager::pickViewport(const ViewportPickRequest& request) {
        ViewportPickResult result;
        result.supported = true;

        if (!m_target.isReady() || !m_frame_ready) {
            return result;
        }

        const VkExtent2D extent = m_target.getExtent();
        if (request.x < 0 || request.y < 0 ||
            request.x >= static_cast<int>(extent.width) ||
            request.y >= static_cast<int>(extent.height)) {
            result.ready = true;
            return result;
        }

        int32_t entity_id = -1;
        if (!readPixel(
                static_cast<uint32_t>(request.x),
                static_cast<uint32_t>(request.y),
                entity_id)) {
            return result;
        }

        result.ready = true;
        result.entity_id = entity_id;
        return result;
    }

    RendererDebugPickingTargetStats VulkanPickingManager::buildDebugStats() const {
        RendererDebugPickingTargetStats stats;
        stats.ready = m_target.isReady();
        stats.frame_ready = m_frame_ready;
        if (!stats.ready) {
            return stats;
        }

        const VkExtent2D extent = m_target.getExtent();
        stats.width = extent.width;
        stats.height = extent.height;
        stats.object_id_format =
            VulkanDiagnosticsCollector::vkFormatToString(m_target.getObjectIdFormat());
        stats.depth_format =
            VulkanDiagnosticsCollector::vkFormatToString(m_target.getDepthFormat());
        return stats;
    }

    bool VulkanPickingManager::readPixel(uint32_t x, uint32_t y, int32_t& entity_id) {
        if (!m_target.isReady() ||
            m_target.getReadbackBuffer() == VK_NULL_HANDLE ||
            m_device == VK_NULL_HANDLE ||
            m_graphics_queue == VK_NULL_HANDLE ||
            m_command_pool == VK_NULL_HANDLE) {
            return false;
        }

        if (!VulkanDiagnosticsCollector::checkVk(
                vkDeviceWaitIdle(m_device),
                "vkDeviceWaitIdle(before picking readback)")) {
            return false;
        }

        VkCommandBuffer command_buffer = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocate_info.commandPool = m_command_pool;
        allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate_info.commandBufferCount = 1;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkAllocateCommandBuffers(m_device, &allocate_info, &command_buffer),
                "vkAllocateCommandBuffers(picking readback)")) {
            return false;
        }

        auto free_command_buffer = [&]() {
            if (command_buffer != VK_NULL_HANDLE) {
                vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
                command_buffer = VK_NULL_HANDLE;
            }
        };

        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (!VulkanDiagnosticsCollector::checkVk(
                vkBeginCommandBuffer(command_buffer, &begin_info),
                "vkBeginCommandBuffer(picking readback)")) {
            free_command_buffer();
            return false;
        }

        transitionObjectIdToTransferSource(
            command_buffer,
            m_target.getObjectIdImage(),
            m_target.getObjectIdLayout());
        m_target.setObjectIdLayout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkBufferImageCopy copy_region{};
        copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_region.imageSubresource.mipLevel = 0;
        copy_region.imageSubresource.baseArrayLayer = 0;
        copy_region.imageSubresource.layerCount = 1;
        copy_region.imageOffset = {
            static_cast<int32_t>(x),
            static_cast<int32_t>(y),
            0
        };
        copy_region.imageExtent = { 1, 1, 1 };

        vkCmdCopyImageToBuffer(
            command_buffer,
            m_target.getObjectIdImage(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            m_target.getReadbackBuffer(),
            1,
            &copy_region);

        if (!VulkanDiagnosticsCollector::checkVk(
                vkEndCommandBuffer(command_buffer),
                "vkEndCommandBuffer(picking readback)")) {
            free_command_buffer();
            return false;
        }

        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        const bool submitted = VulkanDiagnosticsCollector::checkVk(
            vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE),
            "vkQueueSubmit(picking readback)");
        const bool waited = submitted && VulkanDiagnosticsCollector::checkVk(
            vkQueueWaitIdle(m_graphics_queue),
            "vkQueueWaitIdle(picking readback)");
        free_command_buffer();
        if (!waited) {
            return false;
        }

        entity_id = m_target.readbackEntityId();
        return true;
    }
} // namespace NexAur
