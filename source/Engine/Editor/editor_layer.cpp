#include "pch.h"
#include "editor_layer.h"

#include "Editor/editor_config.h"
#include "Editor/Panels/console_panel.h"
#include "Editor/Panels/placeholder_panel.h"
#include "Editor/Panels/project_panel.h"
#include "Editor/Panels/properties_panel.h"
#include "Editor/Panels/render_settings_panel.h"
#include "Editor/Panels/renderer_debug_panel.h"
#include "Editor/Panels/scene_hierarchy_panel.h"
#include "Editor/Panels/viewport_panel.h"
#include "Editor/Commands/editor_command.h"
#include "Editor/Style/editor_icons.h"
#include "Editor/Style/editor_style.h"
#include "Editor/Style/editor_theme.h"
#include "Editor/Widgets/editor_widgets.h"
#include "Function/Platform/platform_services.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Renderer/renderer_debug_service.h"
#include "Function/Renderer/renderer_service.h"
#include "Function/Renderer/data/render_context.h"
#include "Editor/Camera/editor_camera.h"
#include "Function/Scene/component.h"
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
        constexpr const char* kRenderSettingsPanelName = "Render Settings";
        constexpr const char* kRendererDebugPanelName = "Renderer Debug";
        constexpr const char* kProjectPanelName = "Project";
        constexpr const char* kConsolePanelName = "Console";
        constexpr const char* kTimelinePanelName = "Timeline";
        constexpr const char* kProfilerPanelName = "Profiler";

        constexpr const char* kCommandSaveScene = "scene.save";
        constexpr const char* kCommandLoadScene = "scene.load";
        constexpr const char* kCommandBakeReflectionProbes = "scene.reflection_probes.bake_all";
        constexpr const char* kCommandBakeDirtyReflectionProbes = "scene.reflection_probes.bake_dirty";
        constexpr const char* kCommandShowProject = "panel.project.show";
        constexpr const char* kCommandShowConsole = "panel.console.show";
        constexpr const char* kCommandShowRenderSettings = "panel.render_settings.show";
        constexpr const char* kCommandSaveLayout = "layout.save";
        constexpr const char* kCommandResetLayout = "layout.reset";
        constexpr const char* kCommandOpenAllPanels = "layout.open_all_panels";
        constexpr const char* kCommandPlay = "runtime.play";
        constexpr const char* kCommandPause = "runtime.pause";
        constexpr const char* kCommandStop = "runtime.stop";
        constexpr const char* kCommandToolSelect = "tool.select";
        constexpr const char* kCommandToolTranslate = "tool.translate";
        constexpr const char* kCommandToolRotate = "tool.rotate";
        constexpr const char* kCommandToolScale = "tool.scale";
        constexpr const char* kCommandToolLocal = "tool.space.local";
        constexpr const char* kCommandToolWorld = "tool.space.world";
        constexpr const char* kCommandToolToggleSpace = "tool.space.toggle";
        constexpr const char* kCommandToolSnap = "tool.snap.toggle";

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

        std::string buildCommandTooltip(const EditorCommand& command) {
            std::string tooltip = command.tooltip;
            if (!command.shortcut_text.empty()) {
                if (!tooltip.empty()) {
                    tooltip += "\n";
                }
                tooltip += command.shortcut_text;
            }
            return tooltip;
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
        m_panels.push_back(std::make_shared<RenderSettingsPanel>(kRenderSettingsPanelName));
        m_panels.push_back(std::make_shared<RendererDebugPanel>(kRendererDebugPanelName));
        m_panels.push_back(std::make_shared<ProjectPanel>(kProjectPanelName));
        m_panels.push_back(std::make_shared<ConsolePanel>(kConsolePanelName));
        m_panels.push_back(std::make_shared<PlaceholderPanel>(
            kTimelinePanelName,
            "Timeline editor is reserved for the animation tool pass."));
        m_panels.push_back(std::make_shared<PlaceholderPanel>(
            kProfilerPanelName,
            "Profiler view is reserved for the diagnostics tool pass."));

        registerEditorCommands();
        loadEditorConfig();

        NX_CORE_INFO("EditorLayer initialized.");
    }

    void EditorLayer::shutdown() {
        saveEditorConfig();
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
        syncReflectionProbeCaptureStates();
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
        processCommandShortcuts();
        drawMainToolbar();

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

    void EditorLayer::registerEditorCommands() {
        if (!m_context) {
            return;
        }

        if (!m_context->command_registry) {
            m_context->command_registry = std::make_shared<EditorCommandRegistry>();
        }

        EditorCommandRegistry& commands = *m_context->command_registry;

        commands.registerCommand({
            kCommandSaveScene,
            "Save Scene",
            "Save the active scene to the default editor scene file.",
            "Ctrl+S",
            ImGuiMod_Ctrl | ImGuiKey_S,
            [this]() { return m_context && m_context->active_scene && m_context->asset_manager; },
            {},
            [this]() { saveActiveScene(); }
        });

        commands.registerCommand({
            kCommandLoadScene,
            "Load Scene",
            "Load the default editor scene file.",
            "Ctrl+O",
            ImGuiMod_Ctrl | ImGuiKey_O,
            [this]() { return m_context && m_context->scene_service && m_context->asset_manager; },
            {},
            [this]() { loadScene(); }
        });

        commands.registerCommand({
            kCommandBakeReflectionProbes,
            "Bake Reflection Probes",
            "Queue every enabled reflection probe for runtime bake.",
            "",
            ImGuiKey_None,
            [this]() {
                return m_context &&
                       m_context->active_scene &&
                       m_context->renderer_service;
            },
            {},
            [this]() { bakeReflectionProbes(false); }
        });

        commands.registerCommand({
            kCommandBakeDirtyReflectionProbes,
            "Bake Dirty Reflection Probes",
            "Queue every dirty enabled reflection probe for runtime bake.",
            "",
            ImGuiKey_None,
            [this]() {
                return m_context &&
                       m_context->active_scene &&
                       m_context->renderer_service;
            },
            {},
            [this]() { bakeReflectionProbes(true); }
        });

        commands.registerCommand({
            kCommandShowProject,
            "Show Project",
            "Open the Project panel.",
            "",
            ImGuiKey_None,
            {},
            {},
            [this]() { openPanel(kProjectPanelName); }
        });

        commands.registerCommand({
            kCommandShowConsole,
            "Show Console",
            "Open the Console panel.",
            "",
            ImGuiKey_None,
            {},
            {},
            [this]() { openPanel(kConsolePanelName); }
        });

        commands.registerCommand({
            kCommandShowRenderSettings,
            "Show Render Settings",
            "Open the Render Settings panel.",
            "",
            ImGuiKey_None,
            {},
            {},
            [this]() { openPanel(kRenderSettingsPanelName); }
        });

        commands.registerCommand({
            kCommandSaveLayout,
            "Save Layout",
            "Save the current editor dock layout.",
            "",
            ImGuiKey_None,
            {},
            {},
            [this]() { saveEditorLayoutNow(); }
        });

        commands.registerCommand({
            kCommandResetLayout,
            "Reset Layout",
            "Restore the default editor dock layout.",
            "",
            ImGuiKey_None,
            {},
            {},
            [this]() {
                m_reset_dock_layout_requested = true;
                setAllPanelsOpen(true);
            }
        });

        commands.registerCommand({
            kCommandOpenAllPanels,
            "Open All Panels",
            "Open every registered editor panel.",
            "",
            ImGuiKey_None,
            {},
            {},
            [this]() { setAllPanelsOpen(true); }
        });

        commands.registerCommand({
            kCommandPlay,
            "Play",
            "Reserved for the runtime play-mode lifecycle pass.",
            "",
            ImGuiKey_None,
            {},
            {},
            []() { NX_CORE_INFO("Editor Play command is reserved for a later play-mode lifecycle pass."); }
        });

        commands.registerCommand({
            kCommandPause,
            "Pause",
            "Reserved for the runtime play-mode lifecycle pass.",
            "",
            ImGuiKey_None,
            {},
            {},
            []() { NX_CORE_INFO("Editor Pause command is reserved for a later play-mode lifecycle pass."); }
        });

        commands.registerCommand({
            kCommandStop,
            "Stop",
            "Reserved for the runtime play-mode lifecycle pass.",
            "",
            ImGuiKey_None,
            {},
            {},
            []() { NX_CORE_INFO("Editor Stop command is reserved for a later play-mode lifecycle pass."); }
        });

        commands.registerCommand({
            kCommandToolSelect,
            "Select",
            "Select entities without showing a transform gizmo.",
            "Q",
            ImGuiKey_Q,
            {},
            [this]() { return m_context && m_context->tool_state.operation == EditorToolOperation::Select; },
            [this]() { if (m_context) { m_context->tool_state.operation = EditorToolOperation::Select; } }
        });

        commands.registerCommand({
            kCommandToolTranslate,
            "Move",
            "Move the selected entity.",
            "W",
            ImGuiKey_W,
            {},
            [this]() { return m_context && m_context->tool_state.operation == EditorToolOperation::Translate; },
            [this]() { if (m_context) { m_context->tool_state.operation = EditorToolOperation::Translate; } }
        });

        commands.registerCommand({
            kCommandToolRotate,
            "Rotate",
            "Rotate the selected entity.",
            "E",
            ImGuiKey_E,
            {},
            [this]() { return m_context && m_context->tool_state.operation == EditorToolOperation::Rotate; },
            [this]() { if (m_context) { m_context->tool_state.operation = EditorToolOperation::Rotate; } }
        });

        commands.registerCommand({
            kCommandToolScale,
            "Scale",
            "Scale the selected entity.",
            "R",
            ImGuiKey_R,
            {},
            [this]() { return m_context && m_context->tool_state.operation == EditorToolOperation::Scale; },
            [this]() { if (m_context) { m_context->tool_state.operation = EditorToolOperation::Scale; } }
        });

        commands.registerCommand({
            kCommandToolLocal,
            "Local",
            "Use local transform space for gizmo operations.",
            "",
            ImGuiKey_None,
            {},
            [this]() { return m_context && m_context->tool_state.space == EditorToolSpace::Local; },
            [this]() { if (m_context) { m_context->tool_state.space = EditorToolSpace::Local; } }
        });

        commands.registerCommand({
            kCommandToolWorld,
            "World",
            "Use world transform space for gizmo operations.",
            "",
            ImGuiKey_None,
            {},
            [this]() { return m_context && m_context->tool_state.space == EditorToolSpace::World; },
            [this]() { if (m_context) { m_context->tool_state.space = EditorToolSpace::World; } }
        });

        commands.registerCommand({
            kCommandToolToggleSpace,
            "Toggle Transform Space",
            "Toggle the gizmo transform space between local and world.",
            "T",
            ImGuiKey_T,
            {},
            {},
            [this]() {
                if (!m_context) {
                    return;
                }
                m_context->tool_state.space =
                    m_context->tool_state.space == EditorToolSpace::World
                        ? EditorToolSpace::Local
                        : EditorToolSpace::World;
            }
        });

        commands.registerCommand({
            kCommandToolSnap,
            "Snap",
            "Toggle transform snapping.",
            "",
            ImGuiKey_None,
            {},
            [this]() { return m_context && m_context->tool_state.snap_enabled; },
            [this]() { if (m_context) { m_context->tool_state.snap_enabled = !m_context->tool_state.snap_enabled; } }
        });
    }

    void EditorLayer::drawMainMenuBar() {
        if (!ImGui::BeginMenuBar()) {
            return;
        }

        drawFileMenu();
        drawEditMenu();
        drawSceneMenu();
        drawProjectMenu();
        drawWindowMenu();
        drawHelpMenu();

        ImGui::EndMenuBar();
    }

    void EditorLayer::processCommandShortcuts() {
        if (m_context && m_context->command_registry) {
            m_context->command_registry->processShortcuts();
        }
    }

    void EditorLayer::drawMainToolbar() {
        const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();
        const float toolbar_height = ImGui::GetFrameHeight() + 8.0f;
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.colors.background_panel);
        if (!ImGui::BeginChild("##NexAurMainToolbar", ImVec2(0.0f, toolbar_height), false, flags)) {
            ImGui::EndChild();
            ImGui::PopStyleColor();
            return;
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 6.0f);

        drawToolbarCommand(kCommandSaveScene, EditorIcons::Save);
        ImGui::SameLine();
        drawToolbarCommand(kCommandLoadScene, EditorIcons::Load);

        drawToolbarSeparator();

        drawToolbarCommand(kCommandPlay, EditorIcons::Play);
        ImGui::SameLine();
        drawToolbarCommand(kCommandPause, EditorIcons::Pause);
        ImGui::SameLine();
        drawToolbarCommand(kCommandStop, EditorIcons::Stop);

        drawToolbarSeparator();

        drawToolbarCommand(kCommandToolSelect, EditorIcons::Select);
        ImGui::SameLine();
        drawToolbarCommand(kCommandToolTranslate, EditorIcons::Translate);
        ImGui::SameLine();
        drawToolbarCommand(kCommandToolRotate, EditorIcons::Rotate);
        ImGui::SameLine();
        drawToolbarCommand(kCommandToolScale, EditorIcons::Scale);

        drawToolbarSeparator();

        drawToolbarCommand(kCommandToolLocal);
        ImGui::SameLine();
        drawToolbarCommand(kCommandToolWorld);

        drawToolbarSeparator();

        drawToolbarCommand(kCommandToolSnap);

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    void EditorLayer::drawFileMenu() {
        if (!ImGui::BeginMenu("File")) {
            return;
        }

        drawCommandMenuItem(kCommandSaveScene);
        drawCommandMenuItem(kCommandLoadScene);

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

    void EditorLayer::drawSceneMenu() {
        if (!ImGui::BeginMenu("Scene")) {
            return;
        }

        drawCommandMenuItem(kCommandBakeReflectionProbes);
        drawCommandMenuItem(kCommandBakeDirtyReflectionProbes);

        ImGui::EndMenu();
    }

    void EditorLayer::drawProjectMenu() {
        if (!ImGui::BeginMenu("Project")) {
            return;
        }

        drawCommandMenuItem(kCommandShowProject);
        drawCommandMenuItem(kCommandShowConsole);
        drawCommandMenuItem(kCommandShowRenderSettings);

        ImGui::EndMenu();
    }

    void EditorLayer::drawWindowMenu() {
        if (!ImGui::BeginMenu("Window")) {
            return;
        }

        drawCommandMenuItem(kCommandSaveLayout);
        drawCommandMenuItem(kCommandResetLayout);
        drawCommandMenuItem(kCommandOpenAllPanels);

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

    void EditorLayer::drawCommandMenuItem(const std::string& command_id) {
        if (!m_context || !m_context->command_registry) {
            return;
        }

        const EditorCommand* command = m_context->command_registry->find(command_id);
        if (!command) {
            return;
        }

        const bool enabled = m_context->command_registry->isEnabled(command_id);
        const bool selected = m_context->command_registry->isSelected(command_id);
        const char* shortcut =
            command->shortcut_text.empty() ? nullptr : command->shortcut_text.c_str();

        if (ImGui::MenuItem(command->display_name.c_str(), shortcut, selected, enabled)) {
            m_context->command_registry->execute(command_id);
        }
    }

    void EditorLayer::drawToolbarCommand(const std::string& command_id, const char* label) {
        if (!m_context || !m_context->command_registry) {
            return;
        }

        const EditorCommand* command = m_context->command_registry->find(command_id);
        if (!command) {
            return;
        }

        const bool enabled = m_context->command_registry->isEnabled(command_id);
        const bool selected = m_context->command_registry->isSelected(command_id);
        const std::string tooltip = buildCommandTooltip(*command);
        const char* button_label = label && label[0] != '\0'
            ? label
            : command->display_name.c_str();

        ImGui::BeginDisabled(!enabled);
        if (EditorWidgets::toolbarButton(button_label, tooltip.c_str(), selected)) {
            m_context->command_registry->execute(command_id);
        }
        ImGui::EndDisabled();
    }

    void EditorLayer::drawToolbarSeparator() {
        ImGui::SameLine();

        const float height = ImGui::GetFrameHeight();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const EditorTheme& theme = EditorThemeTokens::getDefaultTheme();

        ImGui::GetWindowDrawList()->AddLine(
            ImVec2(cursor.x + 4.0f, cursor.y + 2.0f),
            ImVec2(cursor.x + 4.0f, cursor.y + height - 2.0f),
            ImGui::ColorConvertFloat4ToU32(theme.colors.border_subtle));

        ImGui::Dummy(ImVec2(10.0f, height));
        ImGui::SameLine();
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

    void EditorLayer::loadEditorConfig() {
        if (!m_context || !m_context->command_registry) {
            return;
        }

        m_editor_config = EditorConfigStore::loadOrCreateDefault(*m_context->command_registry);
        EditorThemeTokens::setActiveThemeVariant(m_editor_config.theme_variant);
        EditorConfigStore::applyShortcuts(*m_context->command_registry, m_editor_config);

        if (m_context->viewport_camera) {
            m_context->viewport_camera->setMoveSpeed(m_editor_config.viewport_camera_speed);
        }

        applyPanelVisibilityFromConfig();
        capturePanelVisibilityToConfig();
        EditorConfigStore::save(m_editor_config);

        m_editor_config_loaded = true;
    }

    void EditorLayer::saveEditorConfig() {
        if (!m_editor_config_loaded || !m_context || !m_context->command_registry) {
            return;
        }

        if (m_context->viewport_camera) {
            m_editor_config.viewport_camera_speed = m_context->viewport_camera->getMoveSpeed();
        }
        m_editor_config.theme_variant = std::string(EditorThemeTokens::getActiveThemeVariant());
        m_editor_config.shortcuts = EditorConfigStore::captureShortcuts(*m_context->command_registry);
        capturePanelVisibilityToConfig();
        EditorConfigStore::save(m_editor_config);

        if (m_editor_config.auto_save_layout) {
            saveEditorLayoutNow();
        }
    }

    void EditorLayer::applyPanelVisibilityFromConfig() {
        for (auto& panel : m_panels) {
            if (!panel) {
                continue;
            }

            const auto found = m_editor_config.panel_open.find(panel->getName());
            if (found == m_editor_config.panel_open.end()) {
                continue;
            }

            if (found->second) {
                panel->open();
            } else {
                panel->close();
            }
        }
    }

    void EditorLayer::capturePanelVisibilityToConfig() {
        for (const auto& panel : m_panels) {
            if (!panel) {
                continue;
            }

            m_editor_config.panel_open[panel->getName()] = panel->isOpen();
        }
    }

    void EditorLayer::saveEditorLayoutNow() const {
        if (!ImGui::GetCurrentContext()) {
            return;
        }

        const ImGuiIO& io = ImGui::GetIO();
        if (io.IniFilename && io.IniFilename[0] != '\0') {
            ImGui::SaveIniSettingsToDisk(io.IniFilename);
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
        ImGui::DockBuilderDockWindow(kRenderSettingsPanelName, dock_right);
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

    void EditorLayer::bakeReflectionProbes(bool dirty_only) {
        if (!m_context || !m_context->active_scene || !m_context->renderer_service) {
            NX_CORE_WARN("Bake Reflection Probes skipped: editor scene or renderer service is unavailable.");
            return;
        }

        uint32_t queued_count = 0;
        uint32_t skipped_count = 0;
        auto view = m_context->active_scene->getRegistry().view<ReflectionProbeComponent>();
        for (entt::entity entity : view) {
            ReflectionProbeComponent& probe = view.get<ReflectionProbeComponent>(entity);
            if (!probe.enabled || (dirty_only && !probe.capture_dirty)) {
                ++skipped_count;
                continue;
            }

            ReflectionProbeCaptureRequest request;
            request.entity_id = static_cast<int>(static_cast<uint32_t>(entity));
            request.resolution = std::clamp(probe.capture_resolution, 32u, 1024u);
            request.priority = probe.capture_priority;
            request.near_clip = std::max(0.001f, probe.capture_near_clip);
            request.far_clip = std::max(probe.capture_far_clip, request.near_clip + 0.001f);
            request.include_skybox = probe.capture_include_skybox;
            request.kind = ReflectionProbeCaptureKind::Bake;

            if (m_context->renderer_service->requestReflectionProbeCapture(request)) {
                probe.capture_dirty = false;
                ++queued_count;
            } else {
                ++skipped_count;
            }
        }

        NX_CORE_INFO(
            "Queued {} reflection probe bake request(s). Skipped {} probe(s).",
            queued_count,
            skipped_count);
    }

    void EditorLayer::syncReflectionProbeCaptureStates() {
        if (!m_context || !m_context->active_scene || !m_context->renderer_service) {
            return;
        }

        auto view = m_context->active_scene->getRegistry().view<ReflectionProbeComponent>();
        for (entt::entity entity : view) {
            ReflectionProbeComponent& probe = view.get<ReflectionProbeComponent>(entity);
            const ReflectionProbeCaptureState state =
                m_context->renderer_service->getReflectionProbeCaptureState(
                    static_cast<int>(static_cast<uint32_t>(entity)));

            if (state.status == ReflectionProbeCaptureStatus::Ready && state.runtime_resource_ready) {
                probe.capture_dirty = false;
                if (state.last_kind == ReflectionProbeCaptureKind::Bake && state.baked_asset) {
                    probe.baked_environment_asset = state.baked_asset;
                }
            } else if (state.status == ReflectionProbeCaptureStatus::Failed &&
                       state.last_kind == ReflectionProbeCaptureKind::Bake) {
                probe.capture_dirty = true;
            }
        }
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
