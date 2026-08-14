#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanReflectionProbeCaptureTarget {
    public:
        static constexpr uint32_t kFaceCount = 6;

        VulkanReflectionProbeCaptureTarget() = default;
        ~VulkanReflectionProbeCaptureTarget();

        VulkanReflectionProbeCaptureTarget(const VulkanReflectionProbeCaptureTarget&) = delete;
        VulkanReflectionProbeCaptureTarget& operator=(const VulkanReflectionProbeCaptureTarget&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VkFormat color_format,
            VkFormat depth_format,
            uint32_t resolution);
        bool resize(uint32_t resolution);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        uint32_t getResolution() const { return m_extent.width; }
        VkFormat getColorFormat() const { return m_color_format; }
        VkFormat getDepthFormat() const { return m_depth_format; }
        VkImage getColorImage() const { return m_color_image.getImage(); }
        VkImage getDepthImage() const { return m_depth_image.getImage(); }
        VkBuffer getReadbackBuffer() const { return m_readback_buffer.get(); }
        VkDeviceSize getReadbackBufferSize() const { return m_readback_size; }
        VkImageLayout getColorLayout() const { return m_color_image.getLayout(); }
        VkImageLayout getDepthLayout() const { return m_depth_image.getLayout(); }
        void setColorLayout(VkImageLayout layout) { m_color_image.setLayout(layout); }
        void setDepthLayout(VkImageLayout layout) { m_depth_image.setLayout(layout); }

        VulkanRenderTarget getFaceRenderTarget(uint32_t face_index) const;
        bool recordCopyToReadback(VkCommandBuffer command_buffer) const;
        bool readColorPixels(std::vector<float>& pixels) const;

    private:
        bool recreateImages(uint32_t resolution);
        bool createColorImage(uint32_t resolution);
        bool createDepthImage(uint32_t resolution);
        bool createReadbackBuffer(uint32_t resolution);
        void cleanupImages();
        void cleanupReadbackBuffer();

        VkDeviceSize colorBytesPerTexel() const;
        bool decodeReadback(const void* data, std::vector<float>& pixels) const;

    private:
        VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_color_format = VK_FORMAT_UNDEFINED;
        VkFormat m_depth_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VulkanOwnedImage m_color_image;
        std::vector<VkImageView> m_color_face_views;

        VulkanOwnedImage m_depth_image;
        VkImageView m_depth_view = VK_NULL_HANDLE;

        VulkanOwnedBuffer m_readback_buffer;
        VkDeviceSize m_readback_size = 0;
        bool m_ready = false;
    };
} // namespace NexAur
