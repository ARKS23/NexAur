#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

#include "Core/Base.h"
#include "Function/Renderer/Vulkan/core/vulkan_owned_resources.h"
#include "Function/Renderer/Vulkan/vulkan_render_target.h"
#include "Function/Renderer/Vulkan/vulkan_resource_context.h"

namespace NexAur {
    class VulkanReflectionSurfaceTarget final {
    public:
        VulkanReflectionSurfaceTarget() = default;
        ~VulkanReflectionSurfaceTarget();

        VulkanReflectionSurfaceTarget(const VulkanReflectionSurfaceTarget&) = delete;
        VulkanReflectionSurfaceTarget& operator=(const VulkanReflectionSurfaceTarget&) = delete;

        bool init(
            const VulkanResourceContext& context,
            VkFormat reflection_surface_format,
            VkFormat fallback_specular_format,
            VkFormat motion_vector_format,
            uint32_t width,
            uint32_t height);
        bool resize(uint32_t width, uint32_t height);
        void shutdown();

        bool isReady() const { return m_ready; }
        VkExtent2D getExtent() const { return m_extent; }
        VkFormat getReflectionSurfaceFormat() const { return m_reflection_surface_format; }
        VkFormat getFallbackSpecularFormat() const { return m_fallback_specular_format; }
        VkFormat getMotionVectorFormat() const { return m_motion_vector_format; }
        VkSampler getSampler() const { return m_sampler.get(); }

        const VulkanImageViewState& getReflectionSurfaceImage() const {
            return m_reflection_surface_image.getView();
        }
        const VulkanImageViewState& getFallbackSpecularImage() const {
            return m_fallback_specular_image.getView();
        }
        const VulkanImageViewState& getMotionVectorImage() const {
            return m_motion_vector_image.getView();
        }

        void applyToRenderTarget(VulkanRenderTarget& target) const;

        void setReflectionSurfaceLayout(VkImageLayout layout) {
            m_reflection_surface_image.setLayout(layout);
        }
        void setFallbackSpecularLayout(VkImageLayout layout) {
            m_fallback_specular_image.setLayout(layout);
        }
        void setMotionVectorLayout(VkImageLayout layout) {
            m_motion_vector_image.setLayout(layout);
        }

    private:
        bool recreateImages(uint32_t width, uint32_t height);
        bool createImage(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            const char* debug_name,
            VulkanOwnedImage& image);
        bool createSampler();
        void cleanupImages();
        void cleanupSampler();

    private:
        const VulkanGpuAllocator* m_gpu_allocator = nullptr;
        VkDevice m_device = VK_NULL_HANDLE;
        VkFormat m_reflection_surface_format = VK_FORMAT_UNDEFINED;
        VkFormat m_fallback_specular_format = VK_FORMAT_UNDEFINED;
        VkFormat m_motion_vector_format = VK_FORMAT_UNDEFINED;
        VkExtent2D m_extent{};

        VulkanOwnedImage m_reflection_surface_image;
        VulkanOwnedImage m_fallback_specular_image;
        VulkanOwnedImage m_motion_vector_image;
        VulkanOwnedSampler m_sampler;
        bool m_ready = false;
    };
} // namespace NexAur
