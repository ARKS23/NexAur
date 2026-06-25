#include "pch.h"
#include "vulkan_imgui_renderer.h"

#include <algorithm>

#include <imgui.h>
#include <imgui_impl_vulkan.h>

namespace NexAur {
    namespace {
        void checkVkResult(VkResult result) {
            if (result == VK_SUCCESS) {
                return;
            }

            NX_CORE_ERROR("ImGui Vulkan backend reported VkResult: {}", static_cast<int>(result));
        }
    } // namespace

    VulkanImGuiRenderer::~VulkanImGuiRenderer() {
        shutdown();
    }

    bool VulkanImGuiRenderer::init(const VulkanImGuiRendererContext& context) {
        if (m_initialized) {
            return true;
        }

        if (!context.valid()) {
            NX_CORE_ERROR("VulkanImGuiRenderer requires a valid Vulkan context.");
            return false;
        }
        if (!ImGui::GetCurrentContext()) {
            NX_CORE_ERROR("VulkanImGuiRenderer requires an active ImGui context.");
            return false;
        }

        m_swapchain_color_format = context.swapchain_color_format;
        m_min_image_count = std::max(2u, context.min_image_count);

        VkPipelineRenderingCreateInfo pipeline_rendering_info{};
        pipeline_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        pipeline_rendering_info.colorAttachmentCount = 1;
        pipeline_rendering_info.pColorAttachmentFormats = &m_swapchain_color_format;

        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.ApiVersion = context.api_version;
        init_info.Instance = context.instance;
        init_info.PhysicalDevice = context.physical_device;
        init_info.Device = context.device;
        init_info.QueueFamily = context.graphics_queue_family;
        init_info.Queue = context.graphics_queue;
        init_info.DescriptorPoolSize = 1024;
        init_info.MinImageCount = m_min_image_count;
        init_info.ImageCount = std::max(context.image_count, m_min_image_count);
        init_info.UseDynamicRendering = true;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering_info;
        init_info.PipelineInfoForViewports.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.PipelineInfoForViewports.PipelineRenderingCreateInfo = pipeline_rendering_info;
        init_info.CheckVkResultFn = checkVkResult;

        if (!ImGui_ImplVulkan_Init(&init_info)) {
            NX_CORE_ERROR("Failed to initialize ImGui Vulkan renderer backend.");
            return false;
        }

        m_initialized = true;
        NX_CORE_INFO("ImGui Vulkan renderer backend initialized.");
        return true;
    }

    void VulkanImGuiRenderer::shutdown() {
        if (!m_initialized) {
            return;
        }

        releaseViewportTexture();
        ImGui_ImplVulkan_Shutdown();
        m_swapchain_color_format = VK_FORMAT_UNDEFINED;
        m_min_image_count = 2;
        m_initialized = false;
    }

    void VulkanImGuiRenderer::beginFrame() {
        if (m_initialized) {
            ImGui_ImplVulkan_NewFrame();
        }
    }

    void VulkanImGuiRenderer::renderDrawData(VkCommandBuffer command_buffer) {
        if (!m_initialized || command_buffer == VK_NULL_HANDLE) {
            return;
        }

        ImDrawData* draw_data = ImGui::GetDrawData();
        if (!draw_data) {
            return;
        }

        ImGui_ImplVulkan_RenderDrawData(draw_data, command_buffer);
    }

    void VulkanImGuiRenderer::onSwapchainRecreated(uint32_t image_count) {
        if (!m_initialized) {
            return;
        }

        ImGui_ImplVulkan_SetMinImageCount(std::max(m_min_image_count, image_count));
    }

    bool VulkanImGuiRenderer::registerViewportTexture(VkImageView image_view, VkImageLayout image_layout) {
        if (!m_initialized || image_view == VK_NULL_HANDLE) {
            return false;
        }

        releaseViewportTexture();
        m_viewport_descriptor = ImGui_ImplVulkan_AddTexture(image_view, image_layout);
        return m_viewport_descriptor != VK_NULL_HANDLE;
    }

    void VulkanImGuiRenderer::releaseViewportTexture() {
        if (m_initialized && m_viewport_descriptor != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(m_viewport_descriptor);
        }
        m_viewport_descriptor = VK_NULL_HANDLE;
    }

    void* VulkanImGuiRenderer::getViewportTextureHandle() const {
        return reinterpret_cast<void*>(m_viewport_descriptor);
    }
} // namespace NexAur
