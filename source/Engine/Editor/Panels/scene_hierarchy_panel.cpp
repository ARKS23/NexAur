#include "pch.h"
#include "scene_hierarchy_panel.h"

#include "Editor/editor_services.h"
#include "Function/Scene/component.h"
#include "Function/Scene/scene_v2.h"

#include <imgui.h>

namespace NexAur {
    SceneHierarchyPanel::SceneHierarchyPanel(const std::string& name)
        : EditorPanel(name) {
        NX_CORE_INFO("SceneHierarchyPanel created.");
    }

    void SceneHierarchyPanel::onUpdate(TimeStep delta_time) {
        (void)delta_time;
    }

    void SceneHierarchyPanel::onUIRender() {
        bool& open_flag = getOpenFlag();
        if (!ImGui::Begin(getName().c_str(), &open_flag)) {
            ImGui::End();
            return;
        }

        std::shared_ptr<SceneV2> scene = m_context ? m_context->active_scene : nullptr;
        if (!scene) {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        entt::registry& registry = scene->getRegistry();
        auto view = registry.view<TagComponent>();
        for (auto entity : view) {
            drawEntityNode(Entity(entity, scene.get()));
        }

        const bool clicked_on_blank = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered() &&
            !ImGui::IsAnyItemActive();

        if (clicked_on_blank) {
            if (std::shared_ptr<SelectionService> selection_service = m_context->selection_service.lock()) {
                selection_service->clearSelection();
            } else {
                m_context->selected_entity = Entity();
                m_context->selection_source = getName();
            }
        }

        ImGui::End();
    }

    void SceneHierarchyPanel::drawEntityNode(Entity entity) {
        std::shared_ptr<SceneV2> scene = m_context ? m_context->active_scene : nullptr;
        if (!scene || !entity.hasComponent<TagComponent>()) {
            return;
        }

        std::string tag = entity.getComponent<TagComponent>().name;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

        if (m_context && entity == m_context->selected_entity) {
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uint64_t>(static_cast<uint32_t>(entity))), flags, "%s", tag.c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            if (std::shared_ptr<SelectionService> selection_service = m_context->selection_service.lock()) {
                selection_service->setSelectedEntity(entity, getName());
            } else {
                m_context->selected_entity = entity;
                m_context->selection_source = getName();
            }
        }

        bool delete_entity = false;
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Entity")) {
                delete_entity = true;
            }
            ImGui::EndPopup();
        }

        if (opened) {
            ImGui::TreePop();
        }

        if (delete_entity) {
            scene->destroyEntity(entity);
            if (m_context->selected_entity == entity) {
                if (std::shared_ptr<SelectionService> selection_service = m_context->selection_service.lock()) {
                    selection_service->clearSelection();
                } else {
                    m_context->selected_entity = Entity();
                    m_context->selection_source = getName();
                }
            }
        }
    }

    void SceneHierarchyPanel::onEvent(Event& event) {
        (void)event;
    }
} // namespace NexAur
