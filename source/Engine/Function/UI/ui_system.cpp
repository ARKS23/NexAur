#include "pch.h"
#include "ui_system.h"

#include "Function/Platform/platform_services.h"
#include "Function/Renderer/renderer_service.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include <imgui_impl_glfw.h>

#include <filesystem>

#ifndef ENGINE_ROOT_DIR
#define ENGINE_ROOT_DIR "."
#endif

namespace NexAur {
    namespace {
        std::filesystem::path getDefaultImguiIniPath() {
            return std::filesystem::path(ENGINE_ROOT_DIR) / "saved" / "editor" / "imgui.ini";
        }
    } // namespace

    void UISystem::init(std::shared_ptr<WindowService> window_service, std::shared_ptr<RendererService> renderer_service) {
        NX_CORE_ASSERT(window_service, "UISystem requires a valid WindowService.");
        m_window_service = std::move(window_service);
        m_renderer_service = renderer_service;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        const std::filesystem::path imgui_ini_path = getDefaultImguiIniPath();
        std::error_code directory_error;
        std::filesystem::create_directories(imgui_ini_path.parent_path(), directory_error);
        if (directory_error) {
            NX_CORE_WARN("Failed to create ImGui ini directory: {}", directory_error.message());
        }
        m_imgui_ini_path = imgui_ini_path.string();
        io.IniFilename = m_imgui_ini_path.c_str();

        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        GLFWwindow* window = static_cast<GLFWwindow*>(m_window_service->getNativeWindow());
        NX_CORE_ASSERT(window, "UISystem requires a valid native GLFW window.");

        ImGui_ImplGlfw_InitForVulkan(window, true);
        m_backend = Backend::GlfwOnly;
        NX_CORE_INFO("UI System initialized ImGui GLFW platform backend; VulkanRendererSystem owns the ImGui renderer backend.");

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

            if (m_backend != Backend::None) {
                ImGui_ImplGlfw_Shutdown();
            }

            ImGui::DestroyContext();
        }

        m_context_initialized = false;
        m_backend = Backend::None;
        m_renderer_service.reset();
        m_window_service.reset();
        m_imgui_ini_path.clear();

        NX_CORE_INFO("UI System shut down.");
    }

    void UISystem::beginFrame() {
        if (!m_context_initialized) {
            return;
        }

        if (std::shared_ptr<RendererService> renderer_service = m_renderer_service.lock()) {
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
