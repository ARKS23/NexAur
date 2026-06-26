#include "pch.h"
#include "vulkan_texture_resource.h"

#include "Function/Resource/texture_asset.h"

#include <cstring>

namespace NexAur {
    namespace {
        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        VkFormat toVulkanFormat(TextureColorSpace color_space) {
            return color_space == TextureColorSpace::SRGB
                ? VK_FORMAT_R8G8B8A8_SRGB
                : VK_FORMAT_R8G8B8A8_UNORM;
        }

        bool createBuffer(
            VmaAllocator allocator,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VmaMemoryUsage memory_usage,
            VmaAllocationCreateFlags allocation_flags,
            VkBuffer& buffer,
            VmaAllocation& allocation,
            VmaAllocationInfo* allocation_info_output = nullptr) {
            if (allocator == VK_NULL_HANDLE || size == 0) {
                return false;
            }

            VkBufferCreateInfo buffer_info{};
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_info.size = size;
            buffer_info.usage = usage;
            buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocation_info{};
            allocation_info.usage = memory_usage;
            allocation_info.flags = allocation_flags;

            VmaAllocationInfo created_allocation_info{};
            const VkResult result = vmaCreateBuffer(
                allocator,
                &buffer_info,
                &allocation_info,
                &buffer,
                &allocation,
                &created_allocation_info);
            if (result != VK_SUCCESS) {
                NX_CORE_ERROR("Failed to create Vulkan texture staging buffer: {}", static_cast<int>(result));
                buffer = VK_NULL_HANDLE;
                allocation = VK_NULL_HANDLE;
                return false;
            }

            if (allocation_info_output) {
                *allocation_info_output = created_allocation_info;
            }

            return true;
        }

        void destroyBuffer(VmaAllocator allocator, VkBuffer& buffer, VmaAllocation& allocation) {
            if (allocator != VK_NULL_HANDLE && buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(allocator, buffer, allocation);
            }

            buffer = VK_NULL_HANDLE;
            allocation = VK_NULL_HANDLE;
        }

        template<typename RecordCommands>
        bool submitUploadCommands(
            const VulkanResourceUploadContext& context,
            const char* operation,
            RecordCommands&& record_commands) {
            VkCommandBuffer command_buffer = VK_NULL_HANDLE;
            VkFence upload_fence = VK_NULL_HANDLE;

            auto cleanup = [&]() {
                if (upload_fence != VK_NULL_HANDLE) {
                    vkDestroyFence(context.device, upload_fence, nullptr);
                }
                if (command_buffer != VK_NULL_HANDLE) {
                    vkFreeCommandBuffers(context.device, context.command_pool, 1, &command_buffer);
                }
            };

            VkCommandBufferAllocateInfo allocate_info{};
            allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocate_info.commandPool = context.command_pool;
            allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocate_info.commandBufferCount = 1;
            if (!checkVk(vkAllocateCommandBuffers(context.device, &allocate_info, &command_buffer), "vkAllocateCommandBuffers(texture upload)")) {
                cleanup();
                return false;
            }

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!checkVk(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer(texture upload)")) {
                cleanup();
                return false;
            }

            record_commands(command_buffer);

            if (!checkVk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(texture upload)")) {
                cleanup();
                return false;
            }

            VkFenceCreateInfo fence_info{};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            if (!checkVk(vkCreateFence(context.device, &fence_info, nullptr, &upload_fence), "vkCreateFence(texture upload)")) {
                cleanup();
                return false;
            }

            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;
            if (!checkVk(vkQueueSubmit(context.graphics_queue, 1, &submit_info, upload_fence), operation)) {
                cleanup();
                return false;
            }

            if (!checkVk(vkWaitForFences(context.device, 1, &upload_fence, VK_TRUE, UINT64_MAX), "vkWaitForFences(texture upload)")) {
                cleanup();
                return false;
            }

            cleanup();
            return true;
        }

        void transitionImageLayout(
            VkCommandBuffer command_buffer,
            VkImage image,
            VkImageLayout old_layout,
            VkImageLayout new_layout) {
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout = old_layout;
            barrier.newLayout = new_layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;

            VkPipelineStageFlags source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkPipelineStageFlags destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

            if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
                new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                       new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }

            vkCmdPipelineBarrier(
                command_buffer,
                source_stage,
                destination_stage,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }

        void copyBufferToImage(
            VkCommandBuffer command_buffer,
            VkBuffer buffer,
            VkImage image,
            uint32_t width,
            uint32_t height) {
            VkBufferImageCopy copy_region{};
            copy_region.bufferOffset = 0;
            copy_region.bufferRowLength = 0;
            copy_region.bufferImageHeight = 0;
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel = 0;
            copy_region.imageSubresource.baseArrayLayer = 0;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageOffset = { 0, 0, 0 };
            copy_region.imageExtent = { width, height, 1 };

            vkCmdCopyBufferToImage(
                command_buffer,
                buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copy_region);
        }
    } // namespace

    VulkanTextureResource::~VulkanTextureResource() {
        reset();
    }

    bool VulkanTextureResource::create(const VulkanResourceUploadContext& context, const TextureAsset& texture) {
        reset();

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanTextureResource requires a valid upload context.");
            return false;
        }
        if (!texture.isLoaded() || texture.getFormat() != TexturePixelFormat::RGBA8) {
            NX_CORE_ERROR("VulkanTextureResource requires a loaded RGBA8 TextureAsset.");
            return false;
        }

        m_allocator = context.allocator;
        m_device = context.device;
        m_width = texture.getWidth();
        m_height = texture.getHeight();
        m_format = toVulkanFormat(texture.getColorSpace());

        VkBuffer staging_buffer = VK_NULL_HANDLE;
        VmaAllocation staging_allocation = VK_NULL_HANDLE;
        VmaAllocationInfo staging_allocation_info{};
        const VkDeviceSize image_size = static_cast<VkDeviceSize>(texture.getByteSize());

        if (!createBuffer(
                context.allocator,
                image_size,
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_AUTO,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                    VMA_ALLOCATION_CREATE_MAPPED_BIT,
                staging_buffer,
                staging_allocation,
                &staging_allocation_info)) {
            reset();
            return false;
        }

        if (!staging_allocation_info.pMappedData) {
            NX_CORE_ERROR("Failed to map Vulkan texture staging allocation.");
            destroyBuffer(context.allocator, staging_buffer, staging_allocation);
            reset();
            return false;
        }

        std::memcpy(staging_allocation_info.pMappedData, texture.getPixels().data(), static_cast<size_t>(image_size));
        vmaFlushAllocation(context.allocator, staging_allocation, 0, VK_WHOLE_SIZE);

        VkImageCreateInfo image_info{};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.extent.width = m_width;
        image_info.extent.height = m_height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.format = m_format;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocation_info{};
        allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

        if (!checkVk(vmaCreateImage(context.allocator, &image_info, &allocation_info, &m_image, &m_allocation, nullptr), "vmaCreateImage(texture)")) {
            destroyBuffer(context.allocator, staging_buffer, staging_allocation);
            reset();
            return false;
        }

        const bool uploaded = submitUploadCommands(
            context,
            "vkQueueSubmit(texture upload)",
            [&](VkCommandBuffer command_buffer) {
                transitionImageLayout(
                    command_buffer,
                    m_image,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                copyBufferToImage(command_buffer, staging_buffer, m_image, m_width, m_height);
                transitionImageLayout(
                    command_buffer,
                    m_image,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            });

        destroyBuffer(context.allocator, staging_buffer, staging_allocation);
        if (!uploaded) {
            reset();
            return false;
        }

        VkImageViewCreateInfo view_info{};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = m_image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = m_format;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        if (!checkVk(vkCreateImageView(context.device, &view_info, nullptr, &m_image_view), "vkCreateImageView(texture)")) {
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
        sampler_info.anisotropyEnable = VK_FALSE;
        sampler_info.maxAnisotropy = 1.0f;
        sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_WHITE;
        sampler_info.unnormalizedCoordinates = VK_FALSE;
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.minLod = 0.0f;
        sampler_info.maxLod = 0.0f;
        sampler_info.mipLodBias = 0.0f;
        if (!checkVk(vkCreateSampler(context.device, &sampler_info, nullptr, &m_sampler), "vkCreateSampler(texture)")) {
            reset();
            return false;
        }

        m_image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        return true;
    }

    void VulkanTextureResource::reset() {
        if (m_device != VK_NULL_HANDLE) {
            if (m_sampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_device, m_sampler, nullptr);
            }
            if (m_image_view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, m_image_view, nullptr);
            }
        }

        if (m_allocator != VK_NULL_HANDLE && m_image != VK_NULL_HANDLE) {
            vmaDestroyImage(m_allocator, m_image, m_allocation);
        }

        m_allocator = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_image = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
        m_image_view = VK_NULL_HANDLE;
        m_sampler = VK_NULL_HANDLE;
        m_image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        m_format = VK_FORMAT_UNDEFINED;
        m_width = 0;
        m_height = 0;
    }
} // namespace NexAur
