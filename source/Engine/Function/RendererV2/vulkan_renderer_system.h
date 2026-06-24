#pragma once

#include <memory>
#include <utility>

#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Function/Renderer/RHI/renderer_service.h"

namespace NexAur {
    class WindowService;
    struct RenderDataPacket;

    // RendererV2 的第一版 Vulkan 后端。公开接口只实现 RendererService，
    // Vulkan instance/device/swapchain 等细节隐藏在 Backend 中。
    class NEXAUR_API VulkanRendererSystem final : public RendererService {
    public:
        VulkanRendererSystem();
        ~VulkanRendererSystem() override;

        VulkanRendererSystem(const VulkanRendererSystem&) = delete;
        VulkanRendererSystem& operator=(const VulkanRendererSystem&) = delete;

        bool init(WindowService& window_service);
        void shutdown();

        RendererBackendType getBackendType() const override { return RendererBackendType::Vulkan; }
        void render(TimeStep ts, const RenderDataPacket& render_data) override;
        void setViewportSize(uint32_t width, uint32_t height) override;
        std::pair<uint32_t, uint32_t> getViewportSize() const override;
        ViewportOutput getViewportOutput() const override;
        ViewportPickResult pickViewport(const ViewportPickRequest& request) override;

        void onEvent(Event& event);

    private:
        struct Backend;
        std::unique_ptr<Backend> m_backend;
    };
} // namespace NexAur
