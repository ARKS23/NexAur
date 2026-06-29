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

        void writeCubePixel(CubeMipPixels& mip, uint32_t face, uint32_t x, uint32_t y, const glm::vec3& color) {
            const size_t base =
                ((static_cast<size_t>(face) * mip.size * mip.size) +
                 (static_cast<size_t>(y) * mip.size) +
                 x) *
                4u;
            mip.pixels[base + 0] = color.r;
            mip.pixels[base + 1] = color.g;
            mip.pixels[base + 2] = color.b;
            mip.pixels[base + 3] = 1.0f;
        }

        void buildBasis(const glm::vec3& normal, glm::vec3& tangent, glm::vec3& bitangent) {
            const glm::vec3 helper = std::abs(normal.y) < 0.999f
                ? glm::vec3{ 0.0f, 1.0f, 0.0f }
                : glm::vec3{ 0.0f, 0.0f, 1.0f };
            tangent = glm::normalize(glm::cross(helper, normal));
            bitangent = glm::normalize(glm::cross(normal, tangent));
        }

        float radicalInverseVdc(uint32_t bits) {
            bits = (bits << 16u) | (bits >> 16u);
            bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
            bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
            bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
            bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
            return static_cast<float>(bits) * 2.3283064365386963e-10f;
        }

        glm::vec2 hammersley(uint32_t index, uint32_t count) {
            return glm::vec2{
                static_cast<float>(index) / std::max(static_cast<float>(count), 1.0f),
                radicalInverseVdc(index)
            };
        }

        glm::vec3 importanceSampleGGX(const glm::vec2& xi, float roughness, const glm::vec3& normal) {
            const float alpha = std::max(roughness * roughness, 0.001f);
            const float alpha2 = alpha * alpha;
            const float phi = 2.0f * kPi * xi.x;
            const float cos_theta = std::sqrt(
                (1.0f - xi.y) /
                std::max(1.0f + (alpha2 - 1.0f) * xi.y, 0.0001f));
            const float sin_theta = std::sqrt(std::max(1.0f - cos_theta * cos_theta, 0.0f));

            const glm::vec3 half_vector_tangent{
                std::cos(phi) * sin_theta,
                std::sin(phi) * sin_theta,
                cos_theta
            };

            glm::vec3 tangent;
            glm::vec3 bitangent;
            buildBasis(normal, tangent, bitangent);
            return glm::normalize(
                tangent * half_vector_tangent.x +
                bitangent * half_vector_tangent.y +
                normal * half_vector_tangent.z);
        }

        float geometrySchlickGGXIbl(float n_dot_v, float roughness) {
            const float k = (roughness * roughness) * 0.5f;
            return n_dot_v / std::max(n_dot_v * (1.0f - k) + k, 0.0001f);
        }

        float geometrySmithIbl(float n_dot_v, float n_dot_l, float roughness) {
            return geometrySchlickGGXIbl(n_dot_v, roughness) *
                   geometrySchlickGGXIbl(n_dot_l, roughness);
        }

        glm::vec2 integrateBrdf(float n_dot_v, float roughness, uint32_t sample_count) {
            const glm::vec3 normal{ 0.0f, 0.0f, 1.0f };
            const glm::vec3 view_direction{
                std::sqrt(std::max(1.0f - n_dot_v * n_dot_v, 0.0f)),
                0.0f,
                n_dot_v
            };

            float scale = 0.0f;
            float bias = 0.0f;
            for (uint32_t sample_index = 0; sample_index < sample_count; ++sample_index) {
                const glm::vec2 xi = hammersley(sample_index, sample_count);
                const glm::vec3 half_vector = importanceSampleGGX(xi, roughness, normal);
                glm::vec3 light_direction =
                    2.0f * glm::dot(view_direction, half_vector) * half_vector - view_direction;
                if (glm::dot(light_direction, light_direction) <= 0.000001f) {
                    continue;
                }
                light_direction = glm::normalize(light_direction);

                const float n_dot_l = std::clamp(light_direction.z, 0.0f, 1.0f);
                const float n_dot_h = std::clamp(half_vector.z, 0.0f, 1.0f);
                const float v_dot_h = std::clamp(glm::dot(view_direction, half_vector), 0.0f, 1.0f);
                if (n_dot_l <= 0.0f) {
                    continue;
                }

                const float geometry = geometrySmithIbl(n_dot_v, n_dot_l, roughness);
                const float geometry_visibility =
                    geometry * v_dot_h / std::max(n_dot_h * n_dot_v, 0.0001f);
                const float fresnel = std::pow(1.0f - v_dot_h, 5.0f);
                scale += (1.0f - fresnel) * geometry_visibility;
                bias += fresnel * geometry_visibility;
            }

            const float inv_sample_count = 1.0f / std::max(static_cast<float>(sample_count), 1.0f);
            return glm::vec2{ scale * inv_sample_count, bias * inv_sample_count };
        }

        CubeMipPixels generateEnvironmentCubeMip(const EnvironmentMapAsset& environment, uint32_t size) {
            CubeMipPixels mip;
            mip.size = size;
            mip.pixels.resize(static_cast<size_t>(size) * size * 6u * 4u);

            for (uint32_t face = 0; face < 6; ++face) {
                for (uint32_t y = 0; y < size; ++y) {
                    for (uint32_t x = 0; x < size; ++x) {
                        const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
                        const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;
                        const glm::vec3 direction = cubeDirection(face, u, v);
                        writeCubePixel(mip, face, x, y, sampleEnvironment(environment, direction));
                    }
                }
            }

            return mip;
        }

        CubeMipPixels generateIrradianceMip(const EnvironmentMapAsset& environment, uint32_t size) {
            constexpr float kSampleDelta = 0.35f;
            CubeMipPixels mip;
            mip.size = size;
            mip.pixels.resize(static_cast<size_t>(size) * size * 6u * 4u);

            for (uint32_t face = 0; face < 6; ++face) {
                for (uint32_t y = 0; y < size; ++y) {
                    for (uint32_t x = 0; x < size; ++x) {
                        const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
                        const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;
                        const glm::vec3 normal = cubeDirection(face, u, v);

                        glm::vec3 right;
                        glm::vec3 up;
                        buildBasis(normal, right, up);

                        glm::vec3 irradiance{ 0.0f };
                        float sample_count = 0.0f;
                        for (float phi = 0.0f; phi < 2.0f * kPi; phi += kSampleDelta) {
                            for (float theta = 0.0f; theta < 0.5f * kPi; theta += kSampleDelta) {
                                const float sin_theta = std::sin(theta);
                                const float cos_theta = std::cos(theta);
                                const glm::vec3 tangent_sample{
                                    sin_theta * std::cos(phi),
                                    sin_theta * std::sin(phi),
                                    cos_theta
                                };
                                const glm::vec3 sample_direction = glm::normalize(
                                    tangent_sample.x * right +
                                    tangent_sample.y * up +
                                    tangent_sample.z * normal);
                                irradiance += sampleEnvironment(environment, sample_direction) * cos_theta * sin_theta;
                                sample_count += 1.0f;
                            }
                        }

                        const glm::vec3 color = sample_count > 0.0f
                            ? kPi * irradiance / sample_count
                            : sampleEnvironment(environment, normal);
                        writeCubePixel(mip, face, x, y, color);
                    }
                }
            }

            return mip;
        }

        glm::vec3 prefilterEnvironment(
            const EnvironmentMapAsset& environment,
            const glm::vec3& normal,
            float roughness) {
            constexpr uint32_t kSampleCount = 32;
            if (roughness <= 0.001f) {
                return sampleEnvironment(environment, normal);
            }

            const glm::vec3 view_direction = normal;
            glm::vec3 prefiltered{ 0.0f };
            float total_weight = 0.0f;

            for (uint32_t sample_index = 0; sample_index < kSampleCount; ++sample_index) {
                const glm::vec2 xi = hammersley(sample_index, kSampleCount);
                const glm::vec3 half_vector = importanceSampleGGX(xi, roughness, normal);
                glm::vec3 light_direction =
                    2.0f * glm::dot(view_direction, half_vector) * half_vector - view_direction;
                if (glm::dot(light_direction, light_direction) <= 0.000001f) {
                    continue;
                }
                light_direction = glm::normalize(light_direction);

                const float n_dot_l = std::max(glm::dot(normal, light_direction), 0.0f);
                if (n_dot_l <= 0.0f) {
                    continue;
                }

                prefiltered += sampleEnvironment(environment, light_direction) * n_dot_l;
                total_weight += n_dot_l;
            }

            return total_weight > 0.0f ? prefiltered / total_weight : sampleEnvironment(environment, normal);
        }

        std::vector<CubeMipPixels> generatePrefilterMips(const EnvironmentMapAsset& environment, uint32_t base_size) {
            const uint32_t mip_count = calculateMipCount(base_size);
            std::vector<CubeMipPixels> mips;
            mips.reserve(mip_count);

            for (uint32_t mip = 0; mip < mip_count; ++mip) {
                const uint32_t size = std::max(1u, base_size >> mip);
                const float roughness = mip_count > 1
                    ? static_cast<float>(mip) / static_cast<float>(mip_count - 1)
                    : 0.0f;
                if (mip == 0) {
                    mips.push_back(generateEnvironmentCubeMip(environment, size));
                    continue;
                }

                CubeMipPixels prefiltered_mip;
                prefiltered_mip.size = size;
                prefiltered_mip.pixels.resize(static_cast<size_t>(size) * size * 6u * 4u);
                for (uint32_t face = 0; face < 6; ++face) {
                    for (uint32_t y = 0; y < size; ++y) {
                        for (uint32_t x = 0; x < size; ++x) {
                            const float u = (2.0f * (static_cast<float>(x) + 0.5f) / static_cast<float>(size)) - 1.0f;
                            const float v = (2.0f * (static_cast<float>(y) + 0.5f) / static_cast<float>(size)) - 1.0f;
                            const glm::vec3 direction = cubeDirection(face, u, v);
                            writeCubePixel(
                                prefiltered_mip,
                                face,
                                x,
                                y,
                                prefilterEnvironment(environment, direction, roughness));
                        }
                    }
                }
                mips.push_back(std::move(prefiltered_mip));
            }

            return mips;
        }

        std::vector<float> generateBrdfLut(uint32_t size) {
            constexpr uint32_t kSampleCount = 128;
            static uint32_t cached_size = 0;
            static std::vector<float> cached_pixels;
            if (cached_size == size && !cached_pixels.empty()) {
                return cached_pixels;
            }

            std::vector<float> pixels(static_cast<size_t>(size) * size * 4u);
            for (uint32_t y = 0; y < size; ++y) {
                const float roughness = (static_cast<float>(y) + 0.5f) / static_cast<float>(size);
                for (uint32_t x = 0; x < size; ++x) {
                    const float ndotv = (static_cast<float>(x) + 0.5f) / static_cast<float>(size);
                    const glm::vec2 brdf = integrateBrdf(
                        std::clamp(ndotv, 0.001f, 1.0f),
                        std::clamp(roughness, 0.0f, 1.0f),
                        kSampleCount);
                    const size_t base = (static_cast<size_t>(y) * size + x) * 4u;
                    pixels[base + 0] = brdf.x;
                    pixels[base + 1] = brdf.y;
                    pixels[base + 2] = 0.0f;
                    pixels[base + 3] = 1.0f;
                }
            }

            cached_size = size;
            cached_pixels = pixels;
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

        std::vector<CubeMipPixels> environment_mips;
        environment_mips.push_back(generateEnvironmentCubeMip(environment_asset, kEnvironmentCubeSize));
        std::vector<CubeMipPixels> irradiance_mips;
        irradiance_mips.push_back(generateIrradianceMip(environment_asset, kIrradianceCubeSize));
        std::vector<CubeMipPixels> prefilter_mips =
            generatePrefilterMips(environment_asset, kPrefilterCubeSize);
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
