#include "pch.h"
#include "properties_panel.h"
#include "Function/Scene/component.h"

#include <algorithm>
#include <array>

#include <Imgui.h>

namespace NexAur {
    void PropertiesPanel::onUpdate(TimeStep delta_time) {
        
    }

    void PropertiesPanel::onUIRender() {
        bool& open_flag = getOpenFlag();
        if (!ImGui::Begin(getName().c_str(), &open_flag)) {
            ImGui::End();
            return;
        }

        if (!m_context || !m_context->active_scene) {
            ImGui::TextDisabled("No active scene.");
            ImGui::End();
            return;
        }

        Entity selected = m_context->selected_entity;
        entt::registry& registry = m_context->active_scene->getRegistry();

        if (!selected || !registry.valid(selected)) {
            m_context->selected_entity = Entity(); // 确保无效实体被重置
            ImGui::TextDisabled("No entity selected.");
            ImGui::End();
            return;
        }

        drawEntityHeader(selected);
        drawAddComponentMenu(selected);
        drawTagComponent(selected);
        drawTransformComponent(selected);

        // 后续实现
        drawCameraComponent(selected);
        drawMeshRendererComponent(selected);
        drawDirectionalLightComponent(selected);
        drawEnvironmentComponent(selected);

        ImGui::End();
    }

    void PropertiesPanel::onEvent(Event& event) {
        
    }

    void PropertiesPanel::drawEntityHeader(Entity entity) {
        std::string entity_name = "Unnamed Entity";
        if (entity.hasComponent<TagComponent>()) {
            entity_name = entity.getComponent<TagComponent>().name;
        }
        ImGui::Text("%s", entity_name.c_str());

        if (m_context && !m_context->selection_source.empty()) {
            ImGui::TextDisabled("Selected Source: %s", m_context->selection_source.c_str());
        }

        ImGui::Separator(); // 水平分割线
    }

    void PropertiesPanel::drawAddComponentMenu(Entity entity) {
        // TODO
    }

    void PropertiesPanel::drawTagComponent(Entity entity) {
        if (!entity.hasComponent<TagComponent>()) return;
        if (!ImGui::CollapsingHeader("Tag", ImGuiTreeNodeFlags_DefaultOpen)) return;

        TagComponent& tag = entity.getComponent<TagComponent>();
        std::array<char, 256> buffer{};
        const std::size_t copy_size = std::min(tag.name.size(), buffer.size() - 1);
        std::copy_n(tag.name.data(), copy_size, buffer.data());

        if (ImGui::InputText("Name", buffer.data(), buffer.size())) {
            tag.name = std::string(buffer.data());
        }
    }

    void PropertiesPanel::drawTransformComponent(Entity entity) {
        if (!entity.hasComponent<TransformComponent>()) return;
        if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;

        TransformComponent& tc = entity.getComponent<TransformComponent>();

        constexpr float kPosMin = -1000.0f;
        constexpr float kPosMax =  1000.0f;
        constexpr float kRotMin =  -180.0f;
        constexpr float kRotMax =   180.0f;
        constexpr float kScaleMin =   0.01f;
        constexpr float kScaleMax = 100.0f;
        constexpr ImGuiSliderFlags kFlags = ImGuiSliderFlags_AlwaysClamp;

        ImGui::DragFloat3("Translation", glm::value_ptr(tc.translation), 0.05f, kPosMin, kPosMax, "%.3f", kFlags);

        glm::vec3 rotation_degrees = glm::degrees(tc.rotation);
        if (ImGui::DragFloat3("Rotation", glm::value_ptr(rotation_degrees), 0.5f, kRotMin, kRotMax, "%.1f", kFlags)) {
            tc.rotation = glm::radians(rotation_degrees);
        }

        if (ImGui::DragFloat3("Scale", glm::value_ptr(tc.scale), 0.05f, kScaleMin, kScaleMax, "%.3f", kFlags)) {
            tc.scale = glm::max(tc.scale, glm::vec3(kScaleMin)); // 确保缩放不为负数或过小
        }

        if (ImGui::Button("Reset Transform")) {
            tc.translation = glm::vec3(0.0f);
            tc.rotation = glm::vec3(0.0f);
            tc.scale = glm::vec3(1.0f);
        }
    }

    void PropertiesPanel::drawCameraComponent(Entity entity) {
        
    }

    void PropertiesPanel::drawMeshRendererComponent(Entity entity) {
        
    }

    void PropertiesPanel::drawDirectionalLightComponent(Entity entity) {
        
    }

    void PropertiesPanel::drawEnvironmentComponent(Entity entity) {
        
    }
} // namespace NexAur
