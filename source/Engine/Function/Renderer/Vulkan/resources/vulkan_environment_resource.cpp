#include "pch.h"
#include "vulkan_environment_resource.h"

#include "Function/Renderer/Vulkan/descriptors/vulkan_descriptor_writer.h"
#include "Function/Resource/environment_map_asset.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <glm/geometric.hpp>
#include <glm/common.hpp>
#include <utility>
#include <vector>

namespace NexAur {
    namespace {
        constexpr VkFormat kEnvironmentFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
        constexpr uint32_t kEnvironmentCubeSize = 128;
        constexpr uint32_t kIrradianceCubeSize = 16;
        constexpr uint32_t kPrefilterCubeSize = 128;
        constexpr uint32_t kBrdfLutSize = 128;
        constexpr float kPi = 3.14159265359f;

        struct CubeMipPixels {
            uint32_t size = 0;
            std::vector<float> pixels;
        };

        bool checkVk(VkResult result, const char* operation) {
            if (result == VK_SUCCESS) {
                return true;
            }

            NX_CORE_ERROR("{} failed: {}", operation, static_cast<int>(result));
            return false;
        }

        uint32_t calculateMipCount(uint32_t size) {
            uint32_t mip_count = 1;
            while (size > 1) {
                size /= 2;
                ++mip_count;
            }
            return mip_count;
        }

        glm::vec3 sanitizeColor(const glm::vec3& color) {
            return glm::max(color, glm::vec3{ 0.0f });
        }

        glm::vec3 averageEnvironmentColor(const EnvironmentMapAsset& environment) {
            const std::vector<float>& pixels = environment.getPixels();
            if (pixels.empty()) {
                return glm::vec3{ 0.08f, 0.10f, 0.14f };
            }

            glm::dvec3 sum{ 0.0 };
            const size_t pixel_count = pixels.size() / 4u;
            for (size_t index = 0; index < pixel_count; ++index) {
                const size_t base = index * 4u;
                sum += glm::dvec3{
                    std::max(0.0f, pixels[base + 0]),
                    std::max(0.0f, pixels[base + 1]),
                    std::max(0.0f, pixels[base + 2])
                };
            }

            return glm::vec3(sum / static_cast<double>(std::max<size_t>(1, pixel_count)));
        }

        glm::vec3 cubeDirection(uint32_t face, float u, float v) {
            switch (face) {
                case 0: return glm::normalize(glm::vec3{ 1.0f, -v, -u });
                case 1: return glm::normalize(glm::vec3{ -1.0f, -v, u });
                case 2: return glm::normalize(glm::vec3{ u, 1.0f, v });
                case 3: return glm::normalize(glm::vec3{ u, -1.0f, -v });
                case 4: return glm::normalize(glm::vec3{ u, -v, 1.0f });
                default: return glm::normalize(glm::vec3{ -u, -v, -1.0f });
            }
        }

        glm::vec3 sampleEnvironment(const EnvironmentMapAsset& environment, const glm::vec3& direction) {
            const uint32_t width = environment.getWidth();
            const uint32_t height = environment.getHeight();
            const std::vector<float>& pixels = environment.getPixels();
            if (width == 0 || height == 0 || pixels.empty()) {
                return glm::vec3{ 0.0f };
            }

            const glm::vec3 n = glm::normalize(direction);
            float u = std::atan2(n.z, n.x) / (2.0f * kPi) + 0.5f;
            u = u - std::floor(u);
            const float v = std::acos(std::clamp(n.y, -1.0f, 1.0f)) / kPi;

            const float x = u * static_cast<float>(width - 1);
            const float y = std::clamp(v, 0.0f, 1.0f) * static_cast<float>(height - 1);
            const uint32_t x0 = static_cast<uint32_t>(std::floor(x)) % width;
            const uint32_t x1 = (x0 + 1) % width;
            const uint32_t y0 = static_cast<uint32_t>(std::clamp(
                static_cast<int>(std::floor(y)),
                0,
                static_cast<int>(height - 1)));
            const uint32_t y1 = std::min(y0 + 1, height - 1);
            const float fx = x - std::floor(x);
            const float fy = y - std::floor(y);

            auto read = [&](uint32_t px, uint32_t py) {
                const size_t base = (static_cast<size_t>(py) * width + px) * 4u;
                return glm::vec3{
                    std::max(0.0f, pixels[base + 0]),
                    std::max(0.0f, pixels[base + 1]),
                    std::max(0.0f, pixels[base + 2])
                };
            };

            const glm::vec3 top = glm::mix(read(x0, y0), read(x1, y0), fx);
            const glm::vec3 bottom = glm::mix(read(x0, y1), read(x1, y1), fx);
            return glm::mix(top, bottom, fy);
        }

        CubeMipPixels generateCubeMip(
            const EnvironmentMapAsset& environment,
            uint32_t size,
            const glm::vec3& average_color,
            float average_blend) {
            CubeMipPixels mip;
            mip.size = size;
            mip.pixels.resize(static_cast<size_t>(size) * size * 6u * 4u);

            for (uint32_t face = 0; face < 6; ++face) {
                for (uint32_t y = 0; y < size; ++y) {
                    for (uint32_t x = 0; x < size; ++x) {
                        const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
                        const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;
                        const glm::vec3 direction = cubeDirection(face, u, v);
                        const glm::vec3 sampled = sampleEnvironment(environment, direction);
                        const glm::vec3 color = glm::mix(sampled, average_color, std::clamp(average_blend, 0.0f, 1.0f));
                        const size_t base =
                            ((static_cast<size_t>(face) * size * size) +
                             (static_cast<size_t>(y) * size) +
                             x) *
                            4u;
                        mip.pixels[base + 0] = color.r;
                        mip.pixels[base + 1] = color.g;
                        mip.pixels[base + 2] = color.b;
                        mip.pixels[base + 3] = 1.0f;
                    }
                }
            }

            return mip;
        }

        std::vector<CubeMipPixels> generatePrefilterMips(
            const EnvironmentMapAsset& environment,
            uint32_t base_size,
            const glm::vec3& average_color) {
            const uint32_t mip_count = calculateMipCount(base_size);
            std::vector<CubeMipPixels> mips;
            mips.reserve(mip_count);

            for (uint32_t mip = 0; mip < mip_count; ++mip) {
                const uint32_t size = std::max(1u, base_size >> mip);
                const float roughness = mip_count > 1
                    ? static_cast<float>(mip) / static_cast<float>(mip_count - 1)
                    : 0.0f;
                const float average_blend = std::pow(roughness, 0.65f);
                mips.push_back(generateCubeMip(environment, size, average_color, average_blend));
            }

            return mips;
        }

        std::vector<float> generateBrdfLut(uint32_t size) {
            std::vector<float> pixels(static_cast<size_t>(size) * size * 4u);
            for (uint32_t y = 0; y < size; ++y) {
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
                for (uint32_t x = 0; x < size; ++x) {
                    const float ndotv = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                    const float fresnel_weight = std::pow(1.0f - ndotv, 5.0f);
                    const float energy = std::clamp(1.0f - roughness * 0.45f, 0.0f, 1.0f);
                    const size_t base = (static_cast<size_t>(y) * size + x) * 4u;
                    pixels[base + 0] = energy;
                    pixels[base + 1] = fresnel_weight * (1.0f - energy);
                    pixels[base + 2] = 0.0f;
                    pixels[base + 3] = 1.0f;
                }
            }

            return pixels;
        }

        std::vector<float> flattenCubeMips(const std::vector<CubeMipPixels>& mips) {
            size_t value_count = 0;
            for (const CubeMipPixels& mip : mips) {
                value_count += mip.pixels.size();
            }

            std::vector<float> flattened;
            flattened.reserve(value_count);
            for (const CubeMipPixels& mip : mips) {
                flattened.insert(flattened.end(), mip.pixels.begin(), mip.pixels.end());
            }
            return flattened;
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
            VkBufferCreateInfo buffer_info{};
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_info.size = size;
            buffer_info.usage = usage;
            buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocation_info{};
            allocation_info.usage = memory_usage;
            allocation_info.flags = allocation_flags;

            VmaAllocationInfo created_allocation_info{};
            if (!checkVk(
                    vmaCreateBuffer(
                        allocator,
                        &buffer_info,
                        &allocation_info,
                        &buffer,
                        &allocation,
                        &created_allocation_info),
                    "vmaCreateBuffer(environment staging)")) {
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
            if (!checkVk(vkAllocateCommandBuffers(context.device, &allocate_info, &command_buffer), "vkAllocateCommandBuffers(environment upload)")) {
                cleanup();
                return false;
            }

            VkCommandBufferBeginInfo begin_info{};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (!checkVk(vkBeginCommandBuffer(command_buffer, &begin_info), "vkBeginCommandBuffer(environment upload)")) {
                cleanup();
                return false;
            }

            record_commands(command_buffer);

            if (!checkVk(vkEndCommandBuffer(command_buffer), "vkEndCommandBuffer(environment upload)")) {
                cleanup();
                return false;
            }

            VkFenceCreateInfo fence_info{};
            fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            if (!checkVk(vkCreateFence(context.device, &fence_info, nullptr, &upload_fence), "vkCreateFence(environment upload)")) {
                cleanup();
                return false;
            }

            VkSubmitInfo submit_info{};
            submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &command_buffer;
            if (!checkVk(vkQueueSubmit(context.graphics_queue, 1, &submit_info, upload_fence), operation) ||
                !checkVk(vkWaitForFences(context.device, 1, &upload_fence, VK_TRUE, UINT64_MAX), "vkWaitForFences(environment upload)")) {
                cleanup();
                return false;
            }

            cleanup();
            return true;
        }

        void transitionImage(
            VkCommandBuffer command_buffer,
            VkImage image,
            uint32_t mip_count,
            uint32_t layer_count,
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
            barrier.subresourceRange.levelCount = mip_count;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = layer_count;

            VkPipelineStageFlags src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkPipelineStageFlags dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

            if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
                new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
                barrier.srcAccessMask = 0;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                       new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
                dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }

            vkCmdPipelineBarrier(
                command_buffer,
                src_stage,
                dst_stage,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
        }

        bool uploadCubeImage(
            const VulkanResourceUploadContext& context,
            const std::vector<CubeMipPixels>& mips,
            VulkanEnvironmentResource::ImageResource& image) {
            if (mips.empty()) {
                return false;
            }

            const std::vector<float> flattened = flattenCubeMips(mips);
            const VkDeviceSize upload_size = static_cast<VkDeviceSize>(flattened.size() * sizeof(float));

            VkBuffer staging_buffer = VK_NULL_HANDLE;
            VmaAllocation staging_allocation = VK_NULL_HANDLE;
            VmaAllocationInfo staging_info{};
            if (!createBuffer(
                    context.allocator,
                    upload_size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_AUTO,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT,
                    staging_buffer,
                    staging_allocation,
                    &staging_info)) {
                return false;
            }

            std::memcpy(staging_info.pMappedData, flattened.data(), static_cast<size_t>(upload_size));
            vmaFlushAllocation(context.allocator, staging_allocation, 0, VK_WHOLE_SIZE);

            VkImageCreateInfo image_info{};
            image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.extent.width = mips.front().size;
            image_info.extent.height = mips.front().size;
            image_info.extent.depth = 1;
            image_info.mipLevels = static_cast<uint32_t>(mips.size());
            image_info.arrayLayers = 6;
            image_info.format = kEnvironmentFormat;
            image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image_info.samples = VK_SAMPLE_COUNT_1_BIT;
            image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocation_info{};
            allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            if (!checkVk(
                    vmaCreateImage(
                        context.allocator,
                        &image_info,
                        &allocation_info,
                        &image.image,
                        &image.allocation,
                        nullptr),
                    "vmaCreateImage(environment cube)")) {
                destroyBuffer(context.allocator, staging_buffer, staging_allocation);
                return false;
            }

            std::vector<VkBufferImageCopy> copy_regions;
            copy_regions.reserve(mips.size() * 6u);
            VkDeviceSize offset = 0;
            for (uint32_t mip = 0; mip < static_cast<uint32_t>(mips.size()); ++mip) {
                const uint32_t size = mips[mip].size;
                const VkDeviceSize face_size =
                    static_cast<VkDeviceSize>(size) *
                    static_cast<VkDeviceSize>(size) *
                    4u *
                    sizeof(float);
                for (uint32_t face = 0; face < 6; ++face) {
                    VkBufferImageCopy region{};
                    region.bufferOffset = offset;
                    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    region.imageSubresource.mipLevel = mip;
                    region.imageSubresource.baseArrayLayer = face;
                    region.imageSubresource.layerCount = 1;
                    region.imageExtent = { size, size, 1 };
                    copy_regions.push_back(region);
                    offset += face_size;
                }
            }

            const bool uploaded = submitUploadCommands(
                context,
                "vkQueueSubmit(environment cube upload)",
                [&](VkCommandBuffer command_buffer) {
                    transitionImage(
                        command_buffer,
                        image.image,
                        static_cast<uint32_t>(mips.size()),
                        6,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    vkCmdCopyBufferToImage(
                        command_buffer,
                        staging_buffer,
                        image.image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        static_cast<uint32_t>(copy_regions.size()),
                        copy_regions.data());
                    transitionImage(
                        command_buffer,
                        image.image,
                        static_cast<uint32_t>(mips.size()),
                        6,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                });

            destroyBuffer(context.allocator, staging_buffer, staging_allocation);
            if (!uploaded) {
                return false;
            }

            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = image.image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            view_info.format = kEnvironmentFormat;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.levelCount = static_cast<uint32_t>(mips.size());
            view_info.subresourceRange.layerCount = 6;
            if (!checkVk(vkCreateImageView(context.device, &view_info, nullptr, &image.view), "vkCreateImageView(environment cube)")) {
                return false;
            }

            VkSamplerCreateInfo sampler_info{};
            sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            sampler_info.magFilter = VK_FILTER_LINEAR;
            sampler_info.minFilter = VK_FILTER_LINEAR;
            sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            sampler_info.minLod = 0.0f;
            sampler_info.maxLod = static_cast<float>(mips.size() - 1u);
            if (!checkVk(vkCreateSampler(context.device, &sampler_info, nullptr, &image.sampler), "vkCreateSampler(environment cube)")) {
                return false;
            }

            image.width = mips.front().size;
            image.height = mips.front().size;
            image.mip_count = static_cast<uint32_t>(mips.size());
            image.layer_count = 6;
            image.cube = true;
            return true;
        }

        bool uploadImage2D(
            const VulkanResourceUploadContext& context,
            uint32_t width,
            uint32_t height,
            const std::vector<float>& pixels,
            VulkanEnvironmentResource::ImageResource& image) {
            if (width == 0 || height == 0 || pixels.empty()) {
                return false;
            }

            const VkDeviceSize upload_size = static_cast<VkDeviceSize>(pixels.size() * sizeof(float));
            VkBuffer staging_buffer = VK_NULL_HANDLE;
            VmaAllocation staging_allocation = VK_NULL_HANDLE;
            VmaAllocationInfo staging_info{};
            if (!createBuffer(
                    context.allocator,
                    upload_size,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VMA_MEMORY_USAGE_AUTO,
                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                        VMA_ALLOCATION_CREATE_MAPPED_BIT,
                    staging_buffer,
                    staging_allocation,
                    &staging_info)) {
                return false;
            }

            std::memcpy(staging_info.pMappedData, pixels.data(), static_cast<size_t>(upload_size));
            vmaFlushAllocation(context.allocator, staging_allocation, 0, VK_WHOLE_SIZE);

            VkImageCreateInfo image_info{};
            image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            image_info.imageType = VK_IMAGE_TYPE_2D;
            image_info.extent.width = width;
            image_info.extent.height = height;
            image_info.extent.depth = 1;
            image_info.mipLevels = 1;
            image_info.arrayLayers = 1;
            image_info.format = kEnvironmentFormat;
            image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
            image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image_info.samples = VK_SAMPLE_COUNT_1_BIT;
            image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocation_info{};
            allocation_info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            if (!checkVk(
                    vmaCreateImage(
                        context.allocator,
                        &image_info,
                        &allocation_info,
                        &image.image,
                        &image.allocation,
                        nullptr),
                    "vmaCreateImage(environment 2D)")) {
                destroyBuffer(context.allocator, staging_buffer, staging_allocation);
                return false;
            }

            VkBufferImageCopy copy_region{};
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageExtent = { width, height, 1 };

            const bool uploaded = submitUploadCommands(
                context,
                "vkQueueSubmit(environment 2D upload)",
                [&](VkCommandBuffer command_buffer) {
                    transitionImage(
                        command_buffer,
                        image.image,
                        1,
                        1,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    vkCmdCopyBufferToImage(
                        command_buffer,
                        staging_buffer,
                        image.image,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &copy_region);
                    transitionImage(
                        command_buffer,
                        image.image,
                        1,
                        1,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                });

            destroyBuffer(context.allocator, staging_buffer, staging_allocation);
            if (!uploaded) {
                return false;
            }

            VkImageViewCreateInfo view_info{};
            view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view_info.image = image.image;
            view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view_info.format = kEnvironmentFormat;
            view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view_info.subresourceRange.levelCount = 1;
            view_info.subresourceRange.layerCount = 1;
            if (!checkVk(vkCreateImageView(context.device, &view_info, nullptr, &image.view), "vkCreateImageView(environment 2D)")) {
                return false;
            }

            image.width = width;
            image.height = height;
            image.mip_count = 1;
            image.layer_count = 1;
            image.cube = false;
            return true;
        }

        EnvironmentMapAsset createSolidEnvironment(const glm::vec3& color) {
            constexpr uint32_t width = 16;
            constexpr uint32_t height = 8;
            std::vector<float> pixels(static_cast<size_t>(width) * height * 4u);
            const glm::vec3 sanitized = sanitizeColor(color);
            for (uint32_t index = 0; index < width * height; ++index) {
                const size_t base = static_cast<size_t>(index) * 4u;
                pixels[base + 0] = sanitized.r;
                pixels[base + 1] = sanitized.g;
                pixels[base + 2] = sanitized.b;
                pixels[base + 3] = 1.0f;
            }
            return EnvironmentMapAsset(width, height, std::move(pixels), "FallbackEnvironment");
        }
    } // namespace

    VulkanEnvironmentResource::~VulkanEnvironmentResource() {
        reset();
    }

    bool VulkanEnvironmentResource::create(
        const VulkanEnvironmentResourceCreateContext& context,
        const EnvironmentMapAsset& environment_asset) {
        reset();

        if (!context.valid() || !environment_asset.isLoaded()) {
            NX_CORE_ERROR("VulkanEnvironmentResource requires a valid context and environment asset.");
            return false;
        }

        m_allocator = context.upload_context.allocator;
        m_device = context.upload_context.device;
        m_descriptor_allocator = context.descriptor_allocator;
        m_descriptor_set_layout = context.descriptor_set_layout;
        m_debug_name = environment_asset.getSourcePath().empty() ? "Environment" : environment_asset.getSourcePath();

        const glm::vec3 average_color = averageEnvironmentColor(environment_asset);
        std::vector<CubeMipPixels> environment_mips;
        environment_mips.push_back(generateCubeMip(environment_asset, kEnvironmentCubeSize, average_color, 0.0f));
        std::vector<CubeMipPixels> irradiance_mips;
        irradiance_mips.push_back(generateCubeMip(environment_asset, kIrradianceCubeSize, average_color, 0.92f));
        std::vector<CubeMipPixels> prefilter_mips =
            generatePrefilterMips(environment_asset, kPrefilterCubeSize, average_color);
        std::vector<float> brdf_lut = generateBrdfLut(kBrdfLutSize);

        if (!uploadCubeImage(context.upload_context, environment_mips, m_environment) ||
            !uploadCubeImage(context.upload_context, irradiance_mips, m_irradiance) ||
            !uploadCubeImage(context.upload_context, prefilter_mips, m_prefiltered) ||
            !uploadImage2D(context.upload_context, kBrdfLutSize, kBrdfLutSize, brdf_lut, m_brdf_lut) ||
            !updateDescriptorSet()) {
            reset();
            return false;
        }

        return true;
    }

    bool VulkanEnvironmentResource::createFallback(
        const VulkanEnvironmentResourceCreateContext& context,
        const glm::vec3& color) {
        EnvironmentMapAsset fallback = createSolidEnvironment(color);
        return create(context, fallback);
    }

    void VulkanEnvironmentResource::reset() {
        if (m_descriptor_allocator != nullptr && m_descriptor_allocation.valid()) {
            m_descriptor_allocator->free(m_descriptor_allocation);
        }

        destroyImage(m_brdf_lut);
        destroyImage(m_prefiltered);
        destroyImage(m_irradiance);
        destroyImage(m_environment);

        m_debug_name.clear();
        m_allocator = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_descriptor_allocator = nullptr;
        m_descriptor_allocation = {};
        m_descriptor_set_layout = VK_NULL_HANDLE;
        m_descriptor_set = VK_NULL_HANDLE;
    }

    void VulkanEnvironmentResource::destroyImage(ImageResource& image) {
        if (m_device != VK_NULL_HANDLE) {
            if (image.sampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_device, image.sampler, nullptr);
            }
            if (image.view != VK_NULL_HANDLE) {
                vkDestroyImageView(m_device, image.view, nullptr);
            }
        }

        if (m_allocator != VK_NULL_HANDLE && image.image != VK_NULL_HANDLE) {
            vmaDestroyImage(m_allocator, image.image, image.allocation);
        }

        image = {};
    }

    bool VulkanEnvironmentResource::updateDescriptorSet() {
        if (m_descriptor_allocator == nullptr || m_descriptor_set_layout == VK_NULL_HANDLE) {
            return false;
        }

        m_descriptor_allocation = m_descriptor_allocator->allocate(m_descriptor_set_layout);
        if (!m_descriptor_allocation.valid()) {
            NX_CORE_ERROR("Failed to allocate Vulkan environment descriptor set.");
            return false;
        }
        m_descriptor_set = m_descriptor_allocation.set;

        auto imageInfo = [](const ImageResource& image) {
            VkDescriptorImageInfo info{};
            info.imageView = image.view;
            info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            return info;
        };

        VkDescriptorImageInfo sampler_info{};
        sampler_info.sampler = m_environment.sampler;

        VulkanDescriptorWriter()
            .writeImage(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo(m_environment))
            .writeImage(1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo(m_irradiance))
            .writeImage(2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo(m_prefiltered))
            .writeImage(3, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, imageInfo(m_brdf_lut))
            .writeImage(4, VK_DESCRIPTOR_TYPE_SAMPLER, sampler_info)
            .update(m_device, m_descriptor_set);
        return true;
    }
} // namespace NexAur
