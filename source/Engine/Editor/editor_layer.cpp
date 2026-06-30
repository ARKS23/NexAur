#include "pch.h"
#include "editor_layer.h"

#include "Editor/Panels/console_panel.h"
#include "Editor/Panels/placeholder_panel.h"
#include "Editor/Panels/project_panel.h"
#include "Editor/Panels/properties_panel.h"
#include "Editor/Panels/renderer_debug_panel.h"
#include "Editor/Panels/scene_hierarchy_panel.h"
#include "Editor/Panels/viewport_panel.h"
#include "Editor/Style/editor_style.h"
#include "Function/Platform/platform_services.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Renderer/renderer_debug_service.h"
#include "Function/Renderer/renderer_service.h"
#include "Function/Renderer/data/render_context.h"
#include "Editor/Camera/editor_camera.h"
#include "Function/Scene/scene_serializer.h"
#include "Function/Scene/scene_service.h"
#include "Function/UI/ui_system.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <sstream>
#include <utility>

namespace NexAur {
    namespace {
        constexpr const char* kSceneViewportPanelName = "Scene Viewport";
        constexpr const char* kSceneHierarchyPanelName = "Scene Hierarchy";
        constexpr const char* kInspectorPanelName = "Inspector";
        constexpr const char* kRendererDebugPanelName = "Renderer Debug";
        constexpr const char* kProjectPanelName = "Project";
        constexpr const char* kConsolePanelName = "Console";
        constexpr const char* kTimelinePanelName = "Timeline";
        constexpr const char* kProfilerPanelName = "Profiler";

        const char* viewportModeToText(EditorViewportViewMode mode) {
            switch (mode) {
            case EditorViewportViewMode::GameView:
                return "Game";
            case EditorViewportViewMode::SceneView:
            default:
                return "Scene";
            }
        }

        const char* rendererBackendToText(RendererBackendType backend) {
            switch (backend) {
            case RendererBackendType::Vulkan:
                return "Vulkan";
            case RendererBackendType::Unknown:
            default:
                return "Unknown";
            }
        }
    } // namespace

    EditorLayer::EditorLayer()
        : EditorLayer(std::make_shared<EditorContext>()) {}

    EditorLayer::EditorLayer(std::shared_ptr<EditorContext> context)
        : m_context(std::move(context)) {
        init();
        NX_CORE_INFO("EditorLayer created.");
    }

    EditorLayer::~EditorLayer() {
        shutdown();
        NX_CORE_INFO("EditorLayer destroyed.");
    }

    void EditorLayer::init() {
        if (!m_context) {
            m_context = std::make_shared<EditorContext>();
        }

        m_context->active_scene = m_context->scene_service
            ? m_context->scene_service->getActiveScene()
            : nullptr;
        m_context->selected_entity = Entity();
        m_context->selection_source = "None";

        if (!m_context->viewport_camera) {
            m_context->viewport_camera = std::make_shared<EditorCamera>(45.0f, 1920.0f / 1080.0f, 0.1f, 1000.0f);
        }
        m_context->viewport_camera->setInputService(m_context->input_service);

        m_panels.push_back(std::make_shared<ViewportPanel>(kSceneViewportPanelName));
        m_panels.push_back(std::make_shared<SceneHierarchyPanel>(kSceneHierarchyPanelName));
        m_panels.push_back(std::make_shared<PropertiesPanel>(kInspectorPanelName));
        m_panels.push_back(std::make_shared<RendererDebugPanel>(kRendererDebugPanelName));
        m_panels.push_back(std::make_shared<ProjectPanel>(kProjectPanelName));
        m_panels.push_back(std::make_shared<ConsolePanel>(kConsolePanelName));
        m_panels.push_back(std::make_shared<PlaceholderPanel>(
            kTimelinePanelName,
            "Timeline editor is reserved for the animation tool pass."));
        m_panels.push_back(std::make_shared<PlaceholderPanel>(
            kProfilerPanelName,
            "Profiler view is reserved for the diagnostics tool pass."));

        NX_CORE_INFO("EditorLayer initialized.");
    }

    void EditorLayer::shutdown() {
        m_panels.clear();
        NX_CORE_INFO("EditorLayer shutdown.");
    }

    Entity EditorLayer::getSelectedEntity() const {
        return m_context ? m_context->selected_entity : Entity();
    }

    void EditorLayer::setSelectedEntity(Entity entity, const std::string& source) {
        if (!m_context) {
            return;
        }

        m_context->selected_entity = entity;
        m_context->selection_source = source;
    }

    void EditorLayer::clearSelection() {
        if (!m_context) {
            return;
        }

        m_context->selected_entity = Entity();
        m_context->selection_source = "None";
    }

    const std::string& EditorLayer::getSelectionSource() const {
        static const std::string kNone = "None";
        return m_context ? m_context->selection_source : kNone;
    }

    bool EditorLayer::isViewportFocused() const {
        return m_context && m_context->viewport_focused;
    }

    bool EditorLayer::isViewportHovered() const {
        return m_context && m_context->viewport_hovered;
    }

    void EditorLayer::onUpdate(TimeStep delta_time) {
        if (!m_is_enabled) {
            return;
        }

        // 每帧先刷新 EditorContext，再让面板和相机读取最新服务状态。
        syncPanelContext();
        updateViewportCamera(delta_time);

        for (auto& panel : m_panels) {
            if (panel->isOpen()) {
                panel->onUpdate(delta_time);
            }
        }
    }

    void EditorLayer::onEvent(Event& event) {
        if (!m_is_enabled) {
            return;
        }

        for (auto& panel : m_panels) {
            if (panel->isOpen()) {
                panel->onEvent(event);
            }
            if (event.handled) {
                break;
            }
        }
    }

    void EditorLayer::onUIRender() {
        if (!m_is_enabled) {
            return;
        }

        ensureEditorStyleApplied();

        // DockSpace 是编辑器 UI 根窗口，所有 Panel 都挂在它里面。
        beginDockSpace();

        for (auto& panel : m_panels) {
            if (panel->isOpen()) {
                panel->onUIRender();
            }
        }

        syncViewportCameraToRenderPacket();
        endDockSpace();
    }

    void EditorLayer::beginDockSpace() {
        static bool dock_space_open = true;
        static bool opt_fullscreen = true;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen) {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("NexAur Editor DockSpace", &dock_space_open, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen) {
            ImGui::PopStyleVar(2);
        }

        drawMainMenuBar();

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable) {
            const ImGuiID dockspace_id = ImGui::GetID("NexAurDockSpace");
            ImVec2 dockspace_size = ImGui::GetContentRegionAvail();
            dockspace_size.y = std::max(0.0f, dockspace_size.y - ImGui::GetFrameHeightWithSpacing());

            ensureDefaultDockLayout(dockspace_id, dockspace_size.x, dockspace_size.y);
            ImGui::DockSpace(dockspace_id, dockspace_size, dockspace_flags);
        }

        drawStatusBar();
    }

    void EditorLayer::endDockSpace() {
        ImGui::End();
    }

    void EditorLayer::ensureEditorStyleApplied() {
        ImGuiContext* current_context = ImGui::GetCurrentContext();
        if (!current_context || current_context == m_styled_imgui_context) {
            return;
        }

        EditorStyle::applyTheme();
        EditorStyle::applyGizmoStyle();
        m_styled_imgui_context = current_context;
    }

    void EditorLayer::drawMainMenuBar() {
        if (!ImGui::BeginMenuBar()) {
            return;
        }

        drawFileMenu();
        drawEditMenu();
        drawProjectMenu();
        drawWindowMenu();
        drawHelpMenu();

        ImGui::EndMenuBar();
    }

    void EditorLayer::drawFileMenu() {
        if (!ImGui::BeginMenu("File")) {
            return;
        }

        const bool can_save =
            m_context && m_context->active_scene && m_context->asset_manager;
        const bool can_load =
            m_context && m_context->scene_service && m_context->asset_manager;

        if (ImGui::MenuItem("Save Scene", nullptr, false, can_save)) {
            saveActiveScene();
        }

        if (ImGui::MenuItem("Load Scene", nullptr, false, can_load)) {
            loadScene();
        }

        ImGui::EndMenu();
    }

    void EditorLayer::drawEditMenu() {
        if (!ImGui::BeginMenu("Edit")) {
            return;
        }

        ImGui::BeginDisabled();
        ImGui::MenuItem("Undo", "Ctrl+Z");
        ImGui::MenuItem("Redo", "Ctrl+Y");
        ImGui::EndDisabled();

        ImGui::EndMenu();
    }

    void EditorLayer::drawProjectMenu() {
        if (!ImGui::BeginMenu("Project")) {
            return;
        }

        if (ImGui::MenuItem("Show Project")) {
            openPanel(kProjectPanelName);
        }

        if (ImGui::MenuItem("Show Console")) {
            openPanel(kConsolePanelName);
        }

        ImGui::EndMenu();
    }

    void EditorLayer::drawWindowMenu() {
        if (!ImGui::BeginMenu("Window")) {
            return;
        }

        if (ImGui::MenuItem("Reset Layout")) {
            m_reset_dock_layout_requested = true;
            setAllPanelsOpen(true);
        }

        if (ImGui::MenuItem("Open All Panels")) {
            setAllPanelsOpen(true);
        }

        ImGui::Separator();

        for (const auto& panel : m_panels) {
            if (!panel) {
                continue;
            }

            bool open = panel->isOpen();
            if (ImGui::MenuItem(panel->getName().c_str(), nullptr, open)) {
                if (open) {
                    panel->close();
                } else {
                    panel->open();
                }
            }
        }

        ImGui::EndMenu();
    }

    void EditorLayer::drawHelpMenu() {
        if (!ImGui::BeginMenu("Help")) {
            return;
        }

        ImGui::TextUnformatted("NexAur");
        ImGui::TextDisabled("Editor shell");

        ImGui::EndMenu();
    }

    void EditorLayer::drawStatusBar() {
        ImGui::Separator();

        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        if (!ImGui::BeginChild("##NexAurStatusBar", ImVec2(0.0f, ImGui::GetFrameHeight()), false, flags)) {
            ImGui::EndChild();
            return;
        }

        const std::string scene_text =
            (m_context && m_context->active_scene)
                ? getDefaultScenePath().filename().string()
                : "No Scene";
        const char* view_mode_text =
            m_context ? viewportModeToText(m_context->viewport_view_mode) : "Scene";

        RendererBackendType backend = RendererBackendType::Unknown;
        double frame_ms = 0.0;
        if (m_context && m_context->renderer_debug_service) {
            const RendererDebugSnapshot snapshot =
                m_context->renderer_debug_service->getDebugSnapshot();
            backend = snapshot.backend.backend;
            frame_ms = snapshot.frame.engine_delta_ms;
        }

        const ImGuiIO& io = ImGui::GetIO();
        std::ostringstream left_text;
        left_text << "Scene: " << scene_text
            << "    View: " << view_mode_text
            << "    Renderer: " << rendererBackendToText(backend);

        std::ostringstream right_text;
        right_text << "FPS: " << static_cast<int>(io.Framerate + 0.5f);
        if (frame_ms > 0.0) {
            right_text.setf(std::ios::fixed);
            right_text.precision(2);
            right_text << "    Frame: " << frame_ms << " ms";
        }

        const std::string left = left_text.str();
        const std::string right = right_text.str();

        ImGui::TextUnformatted(left.c_str());

        const float right_width = ImGui::CalcTextSize(right.c_str()).x;
        const float right_x = ImGui::GetWindowContentRegionMax().x - right_width;
        if (right_x > ImGui::GetCursorPosX()) {
            ImGui::SameLine(right_x);
        } else {
            ImGui::SameLine();
        }
        ImGui::TextDisabled("%s", right.c_str());

        ImGui::EndChild();
    }

    std::shared_ptr<EditorPanel> EditorLayer::findPanel(const std::string& name) const {
        const auto it = std::find_if(m_panels.begin(), m_panels.end(), [&name](const auto& panel) {
            return panel && panel->getName() == name;
        });

        return it != m_panels.end() ? *it : nullptr;
    }

    void EditorLayer::openPanel(const std::string& name) {
        std::shared_ptr<EditorPanel> panel = findPanel(name);
        if (panel) {
            panel->open();
        }
    }

    void EditorLayer::setAllPanelsOpen(bool open) {
        for (auto& panel : m_panels) {
            if (!panel) {
                continue;
            }

            if (open) {
                panel->open();
            } else {
                panel->close();
            }
        }
    }

    void EditorLayer::ensureDefaultDockLayout(unsigned int dockspace_id, float width, float height) {
        if (dockspace_id == 0 || width <= 0.0f || height <= 0.0f) {
            return;
        }

        if (!m_reset_dock_layout_requested && m_dock_layout_initialized) {
            return;
        }

        if (!m_reset_dock_layout_requested && !shouldBuildDefaultDockLayout(dockspace_id)) {
            m_dock_layout_initialized = true;
            return;
        }

        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImVec2(width, height));

        ImGuiID dock_main = dockspace_id;
        ImGuiID dock_left = 0;
        ImGuiID dock_right = 0;
        ImGuiID dock_bottom = 0;

        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Left, 0.20f, &dock_left, &dock_main);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Right, 0.26f, &dock_right, &dock_main);
        ImGui::DockBuilderSplitNode(dock_main, ImGuiDir_Down, 0.24f, &dock_bottom, &dock_main);

        ImGui::DockBuilderDockWindow(kSceneViewportPanelName, dock_main);
        ImGui::DockBuilderDockWindow(kSceneHierarchyPanelName, dock_left);
        ImGui::DockBuilderDockWindow(kInspectorPanelName, dock_right);
        ImGui::DockBuilderDockWindow(kRendererDebugPanelName, dock_right);
        ImGui::DockBuilderDockWindow(kProjectPanelName, dock_bottom);
        ImGui::DockBuilderDockWindow(kConsolePanelName, dock_bottom);
        ImGui::DockBuilderDockWindow(kTimelinePanelName, dock_bottom);
        ImGui::DockBuilderDockWindow(kProfilerPanelName, dock_bottom);
        ImGui::DockBuilderFinish(dockspace_id);

        m_dock_layout_initialized = true;
        m_reset_dock_layout_requested = false;
    }

    bool EditorLayer::shouldBuildDefaultDockLayout(unsigned int dockspace_id) const {
        const ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
        if (!node) {
            return true;
        }

        return !node->ChildNodes[0] && !node->ChildNodes[1] && node->Windows.Size == 0;
    }

    void EditorLayer::saveActiveScene() {
        if (!m_context || !m_context->active_scene || !m_context->asset_manager) {
            NX_CORE_WARN("Save Scene skipped: editor scene context is incomplete.");
            return;
        }

        SceneSerializer serializer(*m_context->asset_manager);
        const std::filesystem::path scene_path = getDefaultScenePath();
        const SceneSerializationResult result = serializer.save(*m_context->active_scene, scene_path);
        if (result.success) {
            NX_CORE_INFO("{}. Entity count: {}.", result.message, result.entity_count);
        } else {
            NX_CORE_ERROR("{}", result.message);
        }
    }

    void EditorLayer::loadScene() {
        if (!m_context || !m_context->scene_service || !m_context->asset_manager) {
            NX_CORE_WARN("Load Scene skipped: editor scene context is incomplete.");
            return;
        }

        SceneSerializer serializer(*m_context->asset_manager);
        const std::filesystem::path scene_path = getDefaultScenePath();
        const SceneLoadResult result = serializer.load(scene_path);
        if (!result.success || !result.scene) {
            NX_CORE_ERROR("{}", result.message);
            return;
        }

        m_context->scene_service->setActiveScene(result.scene);
        m_context->active_scene = m_context->scene_service->getActiveScene();
        clearSelection();
        syncPanelContext();

        NX_CORE_INFO("{}. Entity count: {}.", result.message, result.entity_count);
    }

    std::filesystem::path EditorLayer::getDefaultScenePath() const {
        return std::filesystem::path("assets") / "scenes" / "editor_scene.nxscene";
    }

    void EditorLayer::syncPanelContext() {
        if (m_context && m_context->scene_service) {
            m_context->active_scene = m_context->scene_service->getActiveScene();
        }

        // Panel 不自己去找全局服务，只吃 EditorContext。
        for (auto& panel : m_panels) {
            panel->syncPanelContext(m_context);
        }
    }

    void EditorLayer::updateViewportCamera(TimeStep delta_time) {
        if (!m_context || !m_context->viewport_camera) {
            return;
        }
        if (!isSceneViewMode()) {
            return;
        }

        const bool text_input_captured =
            m_context->ui_service && m_context->ui_service->wantsTextInput();

        // 普通 ImGui 键盘捕获不阻止 viewport 相机；只有文本输入时才屏蔽 WASD。
        if (!text_input_captured && shouldUpdateViewportCamera()) {
            m_context->viewport_camera->onUpdate(delta_time);
        }

        syncViewportCameraToRenderPacket();
    }

    bool EditorLayer::shouldUpdateViewportCamera() const {
        if (!m_context) {
            return false;
        }

        if (m_context->viewport_focused || m_context->viewport_hovered) {
            return true;
        }

        if (!m_context->renderer_service) {
            return false;
        }

        const ViewportOutput output = m_context->renderer_service->getViewportOutput();
        return output.valid() && output.kind == ViewportOutputKind::ExternalSwapchain;
    }

    bool EditorLayer::isSceneViewMode() const {
        return m_context && m_context->viewport_view_mode == EditorViewportViewMode::SceneView;
    }

    void EditorLayer::syncViewportCameraToRenderPacket() const {
        if (!m_context || !m_context->viewport_camera || !m_context->render_context) {
            return;
        }
        if (!isSceneViewMode()) {
            return;
        }

        // SceneView 使用独立 EditorCamera；GameView 保留 Scene 提取出的 active CameraComponent。
        auto& camera_data = m_context->render_context->getWriteData().camera_data;
        const auto& viewport_camera = *m_context->viewport_camera;

        camera_data.view_matrix = viewport_camera.getView();
        camera_data.projection_matrix = viewport_camera.getProjection();
        camera_data.view_projection_matrix = viewport_camera.getViewProjection();
        camera_data.position = viewport_camera.getPosition();
        camera_data.near_clip = viewport_camera.getNearClip();
        camera_data.far_clip = viewport_camera.getFarClip();
    }
} // namespace NexAur
