#include "pch.h"
#include "scene_hierarchy_panel.h"
#include "Function/Scene/component.h"

#include <imgui.h>

namespace NexAur {
    SceneHierarchyPanel::SceneHierarchyPanel(const std::string& name) : EditorPanel(name) {
        NX_CORE_INFO("SceneHierarchyPanel created.");
    }

    void SceneHierarchyPanel::onUpdate(TimeStep delta_time) {
        
    }

    void SceneHierarchyPanel::onUIRender() {
        std::shared_ptr<SceneV2> scene = m_context ? m_context->active_scene : nullptr;
        if (!scene) return;

        ImGui::Begin("Scene Hierarchy");

        // 遍历场景中的所有实体
        entt::registry& registry = scene->getRegistry();
        auto view = registry.view<TagComponent>();
        for (auto entity : view) {
            drawEntityNode(Entity(entity, scene.get()));
        }

        // 空白处点击检测: 在面板上没有点击到任何实体上
        const bool clicked_on_blank = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered() &&
            !ImGui::IsAnyItemActive();

        if (clicked_on_blank) {
            m_context->selected_entity = Entity(); // 取消选中
            m_context->selection_source = this->getName();
        }

        ImGui::End();
    }

    void SceneHierarchyPanel::drawEntityNode(Entity entity) {
        std::shared_ptr<SceneV2> scene = m_context ? m_context->active_scene : nullptr;
        std::string tag = scene->getRegistry().get<TagComponent>(entity).name;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth; // 整行高亮

        if (entity == m_context->selected_entity) { // 当前选中的实体刚好是现在这个
            flags |= ImGuiTreeNodeFlags_Selected;
        }

        // TODO: 层级关系
        // 强制把 entity 的 uint32_t 强转为 void* 作为 ImGui 的唯一 ID，防止名字重名导致 Bug
        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());

        // 处理鼠标左键点击事件
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
            m_context->selected_entity = entity;
            m_context->selection_source = this->getName();
        }

        // 处理鼠标右键点击事件: 删除
        bool is_delete_entity = false;
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Entity")) {
                is_delete_entity = true;
            }
            ImGui::EndPopup();
        }

        if (opened) ImGui::TreePop();

        // 删除延迟Entity处理
        if (is_delete_entity) {
            scene->destroyEntity(entity);
            if (m_context->selected_entity == entity) {
                m_context->selected_entity = Entity();
                m_context->selection_source = this->getName();
            }
        }
    }

    void SceneHierarchyPanel::onEvent(Event& event) {

    }
} // namespace NexAur
