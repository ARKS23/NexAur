#include "pch.h"
#include "ui_system.h"

#include "Function/Platform/platform_services.h"
#include "Function/Renderer/RHI/renderer_service.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

namespace NexAur {
    void UISystem::init(std::shared_ptr<WindowService> window_service, std::shared_ptr<RendererService> renderer_service) {
        NX_CORE_ASSERT(window_service, "UISystem requires a valid WindowService.");
        m_window_service = std::move(window_service);
        m_renderer_service = renderer_service;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        const WindowGraphicsAPI graphics_api = m_window_service->getGraphicsAPI();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        if (graphics_api == WindowGraphicsAPI::OpenGL) {
            io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        }

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        GLFWwindow* window = static_cast<GLFWwindow*>(m_window_service->getNativeWindow());
        NX_CORE_ASSERT(window, "UISystem requires a valid native GLFW window.");

        if (graphics_api == WindowGraphicsAPI::OpenGL) {
            ImGui_ImplGlfw_InitForOpenGL(window, true);
            ImGui_ImplOpenGL3_Init("#version 450");
            m_backend = Backend::OpenGL;
        } else {
            ImGui_ImplGlfw_InitForVulkan(window, true);
            m_backend = Backend::GlfwOnly;
            NX_CORE_INFO("UI System initialized ImGui GLFW platform backend for Vulkan; RendererV2 owns the Vulkan ImGui renderer backend.");
        }

        if (std::shared_ptr<RendererService> renderer_service_locked = m_renderer_service.lock()) {
            renderer_service_locked->onUIContextInitialized();
        }

        m_context_initialized = true;
        NX_CORE_INFO("UI System initialized.");
    }

    void UISystem::shutdown() {
        if (m_context_initialized) {
            if (std::shared_ptr<RendererService> renderer_service = m_renderer_service.lock()) {
                renderer_service->onUIContextShutdown();
            }

            if (m_backend == Backend::OpenGL) {
                ImGui_ImplOpenGL3_Shutdown();
            }

            if (m_backend != Backend::None) {
                ImGui_ImplGlfw_Shutdown();
            }

            ImGui::DestroyContext();
        }

        m_context_initialized = false;
        m_backend = Backend::None;
        m_renderer_service.reset();
        m_window_service.reset();

        NX_CORE_INFO("UI System shut down.");
    }

    void UISystem::beginFrame() {
        if (!m_context_initialized) {
            return;
        }

        if (m_backend == Backend::OpenGL) {
            ImGui_ImplOpenGL3_NewFrame();
        } else if (std::shared_ptr<RendererService> renderer_service = m_renderer_service.lock()) {
            renderer_service->beginUIFrame();
        }

        if (m_backend != Backend::None) {
            ImGui_ImplGlfw_NewFrame();
        }

        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    void UISystem::finalizeFrame() {
        if (!m_context_initialized) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        auto [width, height] = m_window_service ? m_window_service->getSize() : std::pair<uint32_t, uint32_t>{ 0, 0 };
        io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));

        ImGui::Render();
    }

    void UISystem::renderBackend() {
        if (!m_context_initialized) {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        if (m_backend != Backend::OpenGL) {
            return;
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void UISystem::endFrameAndRender() {
        finalizeFrame();
        renderBackend();
    }

    void UISystem::onEvent(Event& event) {
        (void)event;
    }

    bool UISystem::isConsumingInput() const {
        return wantsCaptureMouse() || wantsCaptureKeyboard() || wantsTextInput();
    }

    bool UISystem::isConsumeingInput() const {
        return isConsumingInput();
    }

    bool UISystem::wantsCaptureMouse() const {
        if (!m_context_initialized || !ImGui::GetCurrentContext()) {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse;
    }

    bool UISystem::wantsCaptureKeyboard() const {
        if (!m_context_initialized || !ImGui::GetCurrentContext()) {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureKeyboard;
    }

    bool UISystem::wantsTextInput() const {
        if (!m_context_initialized || !ImGui::GetCurrentContext()) {
            return false;
        }

        ImGuiIO& io = ImGui::GetIO();
        return io.WantTextInput;
    }
} // namespace NexAur
