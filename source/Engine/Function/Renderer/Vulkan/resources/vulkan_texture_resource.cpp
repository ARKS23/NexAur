#include "pch.h"
#include "vulkan_texture_resource.h"

#include "Function/Resource/texture_asset.h"
#include "Function/Renderer/Vulkan/core/vulkan_gpu_allocator.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
        VkFormat toVulkanFormat(TextureColorSpace color_space) {
            return color_space == TextureColorSpace::SRGB ?
                VK_FORMAT_R8G8B8A8_SRGB :
                VK_FORMAT_R8G8B8A8_UNORM;
        }

        uint32_t calculateMipLevels(uint32_t width, uint32_t height) {
            const uint32_t largest_dimension = std::max(width, height);
            return largest_dimension > 0 ?
                static_cast<uint32_t>(std::floor(std::log2(largest_dimension))) + 1u :
                1u;
        }

        bool supportsLinearMipmapBlit(
            VkPhysicalDevice physical_device,
            VkFormat format) {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(
                physical_device,
                format,
                &properties);
            constexpr VkFormatFeatureFlags kRequired =
                VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                VK_FORMAT_FEATURE_BLIT_DST_BIT |
                VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            return (properties.optimalTilingFeatures & kRequired) == kRequired;
        }

        float resolveTextureMaxAnisotropy(VkPhysicalDevice physical_device) {
            VkPhysicalDeviceFeatures features{};
            vkGetPhysicalDeviceFeatures(physical_device, &features);
            if (features.samplerAnisotropy != VK_TRUE) {
                return 1.0f;
            }

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(physical_device, &properties);
            return std::clamp(
                properties.limits.maxSamplerAnisotropy,
                1.0f,
                8.0f);
        }

        void transitionImageLayout(
            VkCommandBuffer command_buffer,
            VkImage image,
            VkImageLayout old_layout,
            VkImageLayout new_layout,
            uint32_t base_mip_level,
            uint32_t level_count) {
            VkImageMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            barrier.srcAccessMask = VK_ACCESS_2_NONE;
            barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
            barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = base_mip_level;
            barrier.subresourceRange.levelCount = level_count;
            barrier.subresourceRange.layerCount = 1;

            if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
            } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL &&
                       new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                       new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
            }

            VkDependencyInfo dependency_info{};
            dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dependency_info.imageMemoryBarrierCount = 1;
            dependency_info.pImageMemoryBarriers = &barrier;
            vkCmdPipelineBarrier2(command_buffer, &dependency_info);
        }

        void generateMipmaps(
            VkCommandBuffer command_buffer,
            VkImage image,
            uint32_t width,
            uint32_t height,
            uint32_t mip_levels) {
            if (mip_levels <= 1) {
                transitionImageLayout(
                    command_buffer,
                    image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    0,
                    1);
                return;
            }

            int32_t mip_width = static_cast<int32_t>(width);
            int32_t mip_height = static_cast<int32_t>(height);
            for (uint32_t mip = 1; mip < mip_levels; ++mip) {
                transitionImageLayout(
                    command_buffer,
                    image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    mip - 1,
                    1);

                const int32_t next_width = std::max(mip_width / 2, 1);
                const int32_t next_height = std::max(mip_height / 2, 1);
                VkImageBlit blit{};
                blit.srcOffsets[1] = { mip_width, mip_height, 1 };
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel = mip - 1;
                blit.srcSubresource.layerCount = 1;
                blit.dstOffsets[1] = { next_width, next_height, 1 };
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel = mip;
                blit.dstSubresource.layerCount = 1;
                vkCmdBlitImage(
                    command_buffer,
                    image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1,
                    &blit,
                    VK_FILTER_LINEAR);

                transitionImageLayout(
                    command_buffer,
                    image,
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    mip - 1,
                    1);
                mip_width = next_width;
                mip_height = next_height;
            }

            transitionImageLayout(
                command_buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                mip_levels - 1,
                1);
        }

        void recordTextureUpload(
            VkCommandBuffer command_buffer,
            VkBuffer staging_buffer,
            VkDeviceSize staging_offset,
            VkImage image,
            uint32_t width,
            uint32_t height,
            uint32_t mip_levels) {
            transitionImageLayout(
                command_buffer,
                image,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                0,
                mip_levels);

            VkBufferImageCopy copy_region{};
            copy_region.bufferOffset = staging_offset;
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageExtent = { width, height, 1 };
            vkCmdCopyBufferToImage(
                command_buffer,
                staging_buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copy_region);
            generateMipmaps(
                command_buffer,
                image,
                width,
                height,
                mip_levels);
        }
    } // namespace

    VulkanTextureResource::~VulkanTextureResource() {
        reset();
    }

    bool VulkanTextureResource::create(
        const VulkanResourceUploadContext& context,
        const TextureAsset& texture) {
        reset();
        if (!context.valid() || !context.upload_manager->isInitialized()) {
            NX_CORE_ERROR("VulkanTextureResource requires a valid async upload context.");
            return false;
        }
        if (!texture.isLoaded() ||
            texture.getFormat() != TexturePixelFormat::RGBA8) {
            NX_CORE_ERROR("VulkanTextureResource requires a loaded RGBA8 TextureAsset.");
            return false;
        }

        m_width = texture.getWidth();
        m_height = texture.getHeight();
        m_format = toVulkanFormat(texture.getColorSpace());
        m_mip_levels = supportsLinearMipmapBlit(
            context.physical_device,
            m_format) ?
            calculateMipLevels(m_width, m_height) :
            1u;

        VulkanOwnedImageCreateInfo image_info;
        image_info.extent = { m_width, m_height, 1 };
        image_info.format = m_format;
        image_info.usage =
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT |
            (m_mip_levels > 1 ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
        image_info.mip_levels = m_mip_levels;
        image_info.debug_name = "Vulkan texture image";
        if (!m_image.create(*context.gpu_allocator, image_info)) {
            reset();
            return false;
        }

        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        const float max_anisotropy =
            resolveTextureMaxAnisotropy(context.physical_device);
        sampler_info.anisotropyEnable = max_anisotropy > 1.0f ?
            VK_TRUE :
            VK_FALSE;
        sampler_info.maxAnisotropy = max_anisotropy;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.maxLod = static_cast<float>(m_mip_levels - 1);
        if (!m_sampler.create(
                *context.gpu_allocator,
                sampler_info,
                "Vulkan texture sampler")) {
            reset();
            return false;
        }

        std::vector<uint8_t> upload_data(
            texture.getPixels().begin(),
            texture.getPixels().end());
        const VkImage image = m_image.getImage();
        const uint32_t width = m_width;
        const uint32_t height = m_height;
        const uint32_t mip_levels = m_mip_levels;
        m_upload_ticket = context.upload_manager->enqueue(
            std::move(upload_data),
            "Vulkan texture upload",
            [image, width, height, mip_levels](
                VkCommandBuffer command_buffer,
                VkBuffer staging_buffer,
                VkDeviceSize staging_offset) {
                recordTextureUpload(
                    command_buffer,
                    staging_buffer,
                    staging_offset,
                    image,
                    width,
                    height,
                    mip_levels);
            });
        if (m_upload_ticket.getStatus() == VulkanUploadStatus::Failed) {
            reset();
            return false;
        }

        m_image.setLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        return true;
    }

    void VulkanTextureResource::reset() {
        m_upload_ticket.cancel();
        m_upload_ticket = {};
        m_sampler.reset();
        m_image.reset();
        m_format = VK_FORMAT_UNDEFINED;
        m_width = 0;
        m_height = 0;
        m_mip_levels = 1;
    }
} // namespace NexAur
