#include "pch.h"
#include "properties_panel.h"

#include "Editor/Widgets/editor_property_drawer.h"
#include "Editor/Widgets/editor_widgets.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Scene/component.h"

#include <imgui.h>

#include <utility>

namespace NexAur {
    namespace {
        template<typename Component, typename... Args>
        void drawAddComponentItem(Entity entity, const char* label, Args&&... args) {
            const bool already_has_component = entity.hasComponent<Component>();
            ImGui::BeginDisabled(already_has_component);
            if (ImGui::MenuItem(label, nullptr, already_has_component, !already_has_component)) {
                entity.addComponent<Component>(std::forward<Args>(args)...);
            }
            ImGui::EndDisabled();
        }
    } // namespace

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

        drawCameraComponent(selected);
        drawActiveCameraComponent(selected);
        drawMeshRendererComponent(selected);
        drawDirectionalLightComponent(selected);
        drawPointLightComponent(selected);
        drawRectLightComponent(selected);
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
        EditorWidgets::panelHeader(entity_name.c_str());

        if (m_context && !m_context->selection_source.empty()) {
            ImGui::TextDisabled("Selected Source: %s", m_context->selection_source.c_str());
        }

    }

    void PropertiesPanel::drawAddComponentMenu(Entity entity) {
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("##AddComponentMenu");
        }

        if (!ImGui::BeginPopup("##AddComponentMenu")) {
            return;
        }

        if (ImGui::BeginMenu("Rendering")) {
            drawAddComponentItem<MeshRendererComponent>(entity, "Mesh Renderer");
            drawAddComponentItem<EnvironmentComponent>(entity, "Environment");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Camera")) {
            drawAddComponentItem<CameraComponent>(entity, "Camera");
            drawAddComponentItem<ActiveCameraComponent>(entity, "Active Camera Marker");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Light")) {
            drawAddComponentItem<DirectionalLightComponent>(entity, "Directional Light");
            drawAddComponentItem<PointLightComponent>(entity, "Point Light");
            drawAddComponentItem<RectLightComponent>(entity, "Rect Light");
            ImGui::EndMenu();
        }

        ImGui::BeginDisabled();
        if (ImGui::BeginMenu("Gameplay")) {
            ImGui::MenuItem("Gameplay drawers are planned for a later pass.");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Physics")) {
            ImGui::MenuItem("Physics drawers are planned for a later pass.");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Audio")) {
            ImGui::MenuItem("Audio drawers are planned for a later pass.");
            ImGui::EndMenu();
        }
        ImGui::EndDisabled();

        ImGui::EndPopup();
    }

    void PropertiesPanel::drawTagComponent(Entity entity) {
        if (!entity.hasComponent<TagComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Tag")) return;

        TagComponent& tag = entity.getComponent<TagComponent>();
        EditorPropertyDrawer::drawStringProperty("Name", tag.name);
    }

    void PropertiesPanel::drawTransformComponent(Entity entity) {
        if (!entity.hasComponent<TransformComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Transform")) return;

        TransformComponent& tc = entity.getComponent<TransformComponent>();

        constexpr float kPosMin = -1000.0f;
        constexpr float kPosMax =  1000.0f;
        constexpr float kRotMin =  -180.0f;
        constexpr float kRotMax =   180.0f;
        constexpr float kScaleMin =   0.01f;
        constexpr float kScaleMax = 100.0f;
        constexpr ImGuiSliderFlags kFlags = ImGuiSliderFlags_AlwaysClamp;

        EditorPropertyDrawer::drawVec3Property("Translation", tc.translation, 0.05f, kPosMin, kPosMax, "%.3f", kFlags);

        glm::vec3 rotation_degrees = glm::degrees(tc.rotation);
        if (EditorPropertyDrawer::drawVec3Property("Rotation", rotation_degrees, 0.5f, kRotMin, kRotMax, "%.1f", kFlags)) {
            tc.rotation = glm::radians(rotation_degrees);
        }

        if (EditorPropertyDrawer::drawVec3Property("Scale", tc.scale, 0.05f, kScaleMin, kScaleMax, "%.3f", kFlags)) {
            tc.scale = glm::max(tc.scale, glm::vec3(kScaleMin)); // 确保缩放不为负数或过小
        }

        EditorWidgets::propertyRow("Actions", [&]() {
            if (ImGui::Button("Reset Transform")) {
                tc.translation = glm::vec3(0.0f);
                tc.rotation = glm::vec3(0.0f);
                tc.scale = glm::vec3(1.0f);
            }
        });
    }

    void PropertiesPanel::drawCameraComponent(Entity entity) {
        if (!entity.hasComponent<CameraComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Camera")) return;

        CameraComponent& camera = entity.getComponent<CameraComponent>();
        constexpr ImGuiSliderFlags kFlags = ImGuiSliderFlags_AlwaysClamp;

        EditorPropertyDrawer::drawVec3Property("Position", camera.position, 0.05f, -1000.0f, 1000.0f, "%.3f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Near Clip", camera.nearClip, 0.01f, 0.001f, 1000.0f, "%.3f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Far Clip", camera.farClip, 1.0f, 0.01f, 100000.0f, "%.1f", kFlags);
    }

    void PropertiesPanel::drawActiveCameraComponent(Entity entity) {
        if (!entity.hasComponent<ActiveCameraComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Active Camera")) return;

        ActiveCameraComponent& active_camera = entity.getComponent<ActiveCameraComponent>();
        EditorPropertyDrawer::drawBoolProperty("Enabled", active_camera.enabled);
    }

    void PropertiesPanel::drawMeshRendererComponent(Entity entity) {
        if (!entity.hasComponent<MeshRendererComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Mesh Renderer")) return;

        MeshRendererComponent& mesh_renderer = entity.getComponent<MeshRendererComponent>();
        EditorPropertyDrawer::drawAssetField(
            "Model",
            mesh_renderer.model_asset,
            m_context && m_context->asset_manager ? m_context->asset_manager.get() : nullptr);
        EditorPropertyDrawer::drawBoolProperty("Transparent", mesh_renderer.is_transparent);
    }

    void PropertiesPanel::drawDirectionalLightComponent(Entity entity) {
        if (!entity.hasComponent<DirectionalLightComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Directional Light")) return;

        DirectionalLightComponent& light = entity.getComponent<DirectionalLightComponent>();
        constexpr ImGuiSliderFlags kFlags = ImGuiSliderFlags_AlwaysClamp;

        EditorPropertyDrawer::drawVec3Property("Direction", light.direction, 0.02f, -1.0f, 1.0f, "%.3f", kFlags);
        EditorPropertyDrawer::drawColor3Property("Color", light.color);
        EditorPropertyDrawer::drawFloatProperty("Intensity", light.intensity, 0.05f, 0.0f, 100.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawBoolProperty("Cast Shadow", light.cast_shadow);
        EditorPropertyDrawer::drawFloatProperty("Shadow Strength", light.shadow_strength, 0.01f, 0.0f, 1.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Shadow Bias", light.shadow_bias, 0.0001f, 0.0f, 0.1f, "%.5f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Shadow Distance", light.shadow_distance, 0.5f, 0.0f, 1000.0f, "%.1f", kFlags);
    }

    void PropertiesPanel::drawPointLightComponent(Entity entity) {
        if (!entity.hasComponent<PointLightComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Point Light")) return;

        PointLightComponent& light = entity.getComponent<PointLightComponent>();
        constexpr ImGuiSliderFlags kFlags = ImGuiSliderFlags_AlwaysClamp;

        EditorPropertyDrawer::drawColor3Property("Color", light.color);
        EditorPropertyDrawer::drawFloatProperty("Intensity", light.intensity, 0.05f, 0.0f, 100.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Constant", light.constant, 0.01f, 0.0f, 10.0f, "%.3f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Linear", light.linear, 0.001f, 0.0f, 10.0f, "%.4f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Quadratic", light.quadratic, 0.001f, 0.0f, 10.0f, "%.4f", kFlags);
        EditorPropertyDrawer::drawBoolProperty("Cast Shadow", light.cast_shadow);
        EditorPropertyDrawer::drawFloatProperty("Shadow Range", light.shadow_range, 0.1f, 0.1f, 100.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Shadow Strength", light.shadow_strength, 0.01f, 0.0f, 1.0f, "%.2f", kFlags);
    }

    void PropertiesPanel::drawRectLightComponent(Entity entity) {
        if (!entity.hasComponent<RectLightComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Rect Light")) return;

        RectLightComponent& light = entity.getComponent<RectLightComponent>();
        constexpr ImGuiSliderFlags kFlags = ImGuiSliderFlags_AlwaysClamp;

        EditorPropertyDrawer::drawColor3Property("Color", light.color);
        EditorPropertyDrawer::drawFloatProperty("Intensity", light.intensity, 0.05f, 0.0f, 200.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawVec2Property("Size", light.size, 0.05f, 0.01f, 100.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Range", light.range, 0.1f, 0.1f, 100.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawBoolProperty("Two Sided", light.two_sided);
        EditorPropertyDrawer::drawBoolProperty("Cast Shadow", light.cast_shadow);
        EditorPropertyDrawer::drawFloatProperty("Shadow Strength", light.shadow_strength, 0.01f, 0.0f, 1.0f, "%.2f", kFlags);
    }

    void PropertiesPanel::drawEnvironmentComponent(Entity entity) {
        if (!entity.hasComponent<EnvironmentComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Environment")) return;

        EnvironmentComponent& environment = entity.getComponent<EnvironmentComponent>();
        constexpr ImGuiSliderFlags kFlags = ImGuiSliderFlags_AlwaysClamp;

        EditorPropertyDrawer::drawAssetField(
            "Environment",
            environment.environment_asset,
            m_context && m_context->asset_manager ? m_context->asset_manager.get() : nullptr);
        EditorPropertyDrawer::drawColor3Property("Background", environment.background_color);
        EditorPropertyDrawer::drawFloatProperty("Intensity", environment.intensity, 0.02f, 0.0f, 10.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("Skybox Intensity", environment.skybox_intensity, 0.02f, 0.0f, 10.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawFloatProperty("IBL Intensity", environment.ibl_intensity, 0.02f, 0.0f, 10.0f, "%.2f", kFlags);
    }
} // namespace NexAur
