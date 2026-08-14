#pragma once

#include <memory>
#include <utility>

#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Function/Renderer/reflection_probe_capture_service.h"
#include "Function/Renderer/renderer_debug_service.h"
#include "Function/Renderer/renderer_service.h"
#include "Function/Renderer/viewport_renderer_service.h"

namespace NexAur {
    class AssetManager;
    class WindowService;
    struct RenderDataPacket;

    // Non-owning services required for the lifetime of the Vulkan renderer.
    struct VulkanRendererInitContext {
        WindowService* window_service = nullptr;
        AssetManager* asset_manager = nullptr;

        bool valid() const {
            return window_service != nullptr && asset_manager != nullptr;
        }
    };

    // Vulkan backend behind the renderer facade contracts.
    // Instance, device, and swapchain details remain private to Backend.
    class VulkanRendererSystem final :
        public RendererService,
        public ViewportRendererService,
        public ReflectionProbeCaptureService,
        public RendererDebugService {
    public:
        VulkanRendererSystem();
        ~VulkanRendererSystem() override;

        VulkanRendererSystem(const VulkanRendererSystem&) = delete;
        VulkanRendererSystem& operator=(const VulkanRendererSystem&) = delete;

        bool init(const VulkanRendererInitContext& context);
        void shutdown();

        RendererBackendType getBackendType() const override { return RendererBackendType::Vulkan; }
        void render(TimeStep ts, const RenderDataPacket& render_data) override;
        void setViewportSize(uint32_t width, uint32_t height) override;
        std::pair<uint32_t, uint32_t> getViewportSize() const override;
        ViewportOutput getViewportOutput() const override;
        ViewportPickResult pickViewport(const ViewportPickRequest& request) override;
        bool requestReflectionProbeCapture(const ReflectionProbeCaptureRequest& request) override;
        bool clearReflectionProbeCapture(int entity_id) override;
        ReflectionProbeCaptureState getReflectionProbeCaptureState(int entity_id) const override;
        ReflectionProbeCaptureQueueState getReflectionProbeCaptureQueueState() const override;
        void onUIContextInitialized() override;
        void beginUIFrame() override;
        void onUIContextShutdown() override;
        RendererDebugSnapshot getDebugSnapshot() const override;

        void onEvent(Event& event);

    private:
        struct Backend;
        std::unique_ptr<Backend> m_backend;
    };
} // namespace NexAur
