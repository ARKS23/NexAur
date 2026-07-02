#pragma once
#include "Core/Base.h"
#include "editor_panel.h"

#include <array>
#include <string>

namespace NexAur {
    class SceneHierarchyPanel : public EditorPanel {
    public:
        SceneHierarchyPanel(const std::string& name);
        ~SceneHierarchyPanel() override = default;

        virtual void onUpdate(TimeStep delta_time) override;
        virtual void onUIRender() override;
        virtual void onEvent(Event& event) override;

    private:
        void drawToolbar();
        void drawEntityNode(Entity entity);
        void drawBlankContextMenu(const std::shared_ptr<SceneV2>& scene);
        void drawCreateEntityMenu(const std::shared_ptr<SceneV2>& scene);
        bool matchesSearchFilter(const std::string& name) const;
        const char* getEntityIcon(Entity entity) const;
        void selectEntity(Entity entity);
        void clearSelection();
        Entity createEmptyEntity(const std::shared_ptr<SceneV2>& scene, const std::string& name);
        Entity createCameraEntity(const std::shared_ptr<SceneV2>& scene);
        Entity createDirectionalLightEntity(const std::shared_ptr<SceneV2>& scene);
        Entity createPointLightEntity(const std::shared_ptr<SceneV2>& scene);
        Entity createRectLightEntity(const std::shared_ptr<SceneV2>& scene);
        Entity duplicateEntity(const std::shared_ptr<SceneV2>& scene, Entity source);
        void deleteEntity(const std::shared_ptr<SceneV2>& scene, Entity entity);

        std::array<char, 128> m_search_buffer{};
    };
} // namespace NexAur
