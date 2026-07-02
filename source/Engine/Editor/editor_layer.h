#pragma once
#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Core/Time/TimeStep.h"
#include "Function/Scene/entity.h"
#include "Editor/editor_context.h"
#include "Editor/editor_config.h"
#include "Editor/editor_services.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct ImGuiContext;

namespace NexAur {
    class SceneV2;
    class EditorPanel;
} // namespace NexAur


namespace NexAur {
    class NEXAUR_API EditorLayer : public EditorService, public SelectionService, public ViewportService {
    public:
        EditorLayer();
        explicit EditorLayer(std::shared_ptr<EditorContext> context);
        ~EditorLayer();

        void onUpdate(TimeStep delta_time);

        void onUIRender();

        void onEvent(Event& event) override;

        void setEnabled(bool enabled) override { m_is_enabled = enabled; }
        bool isEnabled() const override { return m_is_enabled; }
        void update(TimeStep delta_time) override { onUpdate(delta_time); }
        void renderUI() override { onUIRender(); }

        Entity getSelectedEntity() const override;
        void setSelectedEntity(Entity entity, const std::string& source) override;
        void clearSelection() override;
        const std::string& getSelectionSource() const override;

        bool isViewportFocused() const override;
        bool isViewportHovered() const override;

    private:
        void init();
        void shutdown();
        void ensureEditorStyleApplied();
        void registerEditorCommands();
        void beginDockSpace();
        void endDockSpace();
        void processCommandShortcuts();
        void drawMainMenuBar();
        void drawMainToolbar();
        void drawFileMenu();
        void drawEditMenu();
        void drawSceneMenu();
        void drawProjectMenu();
        void drawWindowMenu();
        void drawHelpMenu();
        void drawCommandMenuItem(const std::string& command_id);
        void drawToolbarCommand(const std::string& command_id, const char* label = nullptr);
        void drawToolbarSeparator();
        void drawStatusBar();
        std::shared_ptr<EditorPanel> findPanel(const std::string& name) const;
        void openPanel(const std::string& name);
        void setAllPanelsOpen(bool open);
        void loadEditorConfig();
        void saveEditorConfig();
        void applyPanelVisibilityFromConfig();
        void capturePanelVisibilityToConfig();
        void saveEditorLayoutNow() const;
        void ensureDefaultDockLayout(unsigned int dockspace_id, float width, float height);
        bool shouldBuildDefaultDockLayout(unsigned int dockspace_id) const;
        void saveActiveScene();
        void loadScene();
        void bakeReflectionProbes(bool dirty_only);
        void syncReflectionProbeCaptureStates();
        std::filesystem::path getDefaultScenePath() const;
        void syncPanelContext();
        void updateViewportCamera(TimeStep delta_time);
        bool shouldUpdateViewportCamera() const;
        bool isSceneViewMode() const;
        void syncViewportCameraToRenderPacket() const;
        
    private:
        std::vector<std::shared_ptr<EditorPanel>> m_panels; // TODO: 编辑器控制面板列表, 后续OnUIRender更新
        std::shared_ptr<EditorContext> m_context;    // 编辑器上下文，各个面板需要共享的数据
        bool m_is_enabled = true;
        ImGuiContext* m_styled_imgui_context = nullptr;
        EditorConfigData m_editor_config;
        bool m_editor_config_loaded = false;
        bool m_dock_layout_initialized = false;
        bool m_reset_dock_layout_requested = false;
    };
} // namespace NexAur
