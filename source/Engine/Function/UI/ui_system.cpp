#include "pch.h"
#include "ui_system.h"

#include "Function/Global/global_context.h"
#include "Function/Renderer/window_system.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

namespace NexAur {
    void UISystem::init() {
        // 设置ImGui上下文
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 开启键盘导航
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        
        ImGui::StyleColorsDark();

        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        GLFWwindow* window = g_runtime_global_context.m_window_system->getNativeWindow();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 450");

        NX_CORE_INFO("UI System initialized.");
    }

    void UISystem::shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        NX_CORE_INFO("UI System shut down.");
    }

    void UISystem::beginFrame() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    void UISystem::endFrameAndRender() {
        ImGuiIO& io = ImGui::GetIO();
        auto window_size = g_runtime_global_context.m_window_system->getWindowSize();
        io.DisplaySize = ImVec2((float)window_size[0], (float)window_size[1]);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // 多视口处理
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void UISystem::onEvent(Event& event) {
        // 处理全局派发事件
    }

    bool UISystem::isConsumingInput() const {
        return wantsCaptureMouse() || wantsCaptureKeyboard() || wantsTextInput();
    }

    bool UISystem::isConsumeingInput() const {
        return isConsumingInput();
    }

    bool UISystem::wantsCaptureMouse() const {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureMouse;
    }

    bool UISystem::wantsCaptureKeyboard() const {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantCaptureKeyboard;
    }

    bool UISystem::wantsTextInput() const {
        ImGuiIO& io = ImGui::GetIO();
        return io.WantTextInput;
    }
} // namespace NexAur
