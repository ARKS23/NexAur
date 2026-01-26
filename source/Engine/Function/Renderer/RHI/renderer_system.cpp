#include "pch.h"
#include "renderer_system.h"
#include "renderer.h"

namespace NexAur {
    void RendererSystem::init() {
        // 初始化底层渲染器
        Renderer::init();
    }

    void RendererSystem::shutdown() {
        // 关闭底层渲染器
        Renderer::shutdown();
    }

    void RendererSystem::tick(TimeStep ts) {
        // 每帧更新渲染逻辑
    }

    void RendererSystem::onEvent(Event& e) {
        // 处理事件函数
    }

    void RendererSystem::onWindowResize(WindowResizeEvent& e) {
        Renderer::onWindowResize(e.getWidth(), e.getHeight());
    }
} // namespace NexAur
