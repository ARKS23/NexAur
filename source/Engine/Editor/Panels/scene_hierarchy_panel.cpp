#include "pch.h"
#include "scene_hierarchy_panel.h"

#include "Editor/editor_services.h"
#include "Editor/Style/editor_icons.h"
#include "Editor/Widgets/editor_widgets.h"
#include "Function/Scene/component.h"
#include "Function/Scene/scene_v2.h"

#include <algorithm>
#include <cctype>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>

namespace NexAur {
    namespace {
        std::string toLowerCopy(std::string value) {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }
    } // namespace

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

        drawToolbar();
        ImGui::Separator();

        entt::registry& registry = scene->getRegistry();
        auto view = registry.view<TagComponent>();
        for (auto entity : view) {
            Entity wrapped_entity(entity, scene.get());
            const std::string& name = wrapped_entity.getComponent<TagComponent>().name;
            if (matchesSearchFilter(name)) {
                drawEntityNode(wrapped_entity);
            }
        }

        const bool clicked_on_blank = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !ImGui::IsAnyItemHovered() &&
            !ImGui::IsAnyItemActive();

        if (clicked_on_blank) {
            clearSelection();
        }

        drawBlankContextMenu(scene);

        ImGui::End();
    }

    void SceneHierarchyPanel::drawToolbar() {
        const float width = ImGui::GetContentRegionAvail().x;
        EditorWidgets::searchBox(
            "##SceneHierarchySearch",
            m_search_buffer.data(),
            m_search_buffer.size(),
            "Search entities",
            width);
    }

    void SceneHierarchyPanel::drawEntityNode(Entity entity) {
        std::shared_ptr<SceneV2> scene = m_context ? m_context->active_scene : nullptr;
        if (!scene || !entity.hasComponent<TagComponent>()) {
            return;
        }

        const std::string& tag = entity.getComponent<TagComponent>().name;
        const std::string label = std::string(getEntityIcon(entity)) + "  " + tag;
        const bool selected = m_context && entity == m_context->selected_entity;

        ImGui::PushID(static_cast<int>(static_cast<uint32_t>(entity)));
        if (ImGui::Selectable(label.c_str(), selected)) {
            selectEntity(entity);
        }

        bool delete_entity = false;
        bool duplicate_entity = false;
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Duplicate")) {
                duplicate_entity = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Entity")) {
                delete_entity = true;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();

        if (duplicate_entity) {
            Entity duplicated = duplicateEntity(scene, entity);
            if (duplicated) {
                selectEntity(duplicated);
            }
        }

        if (delete_entity) {
            deleteEntity(scene, entity);
        }
    }

    void SceneHierarchyPanel::drawBlankContextMenu(const std::shared_ptr<SceneV2>& scene) {
        if (!scene) {
            return;
        }

        if (!ImGui::BeginPopupContextWindow(
            "##SceneHierarchyBlankContext",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            return;
        }

        drawCreateEntityMenu(scene);
        ImGui::EndPopup();
    }

    void SceneHierarchyPanel::drawCreateEntityMenu(const std::shared_ptr<SceneV2>& scene) {
        if (!scene || !ImGui::BeginMenu("Create")) {
            return;
        }

        if (ImGui::MenuItem("Empty Entity")) {
            selectEntity(createEmptyEntity(scene, "Empty Entity"));
        }

        if (ImGui::MenuItem("Camera")) {
            selectEntity(createCameraEntity(scene));
        }

        if (ImGui::BeginMenu("Light")) {
            if (ImGui::MenuItem("Directional Light")) {
                selectEntity(createDirectionalLightEntity(scene));
            }
            if (ImGui::MenuItem("Point Light")) {
                selectEntity(createPointLightEntity(scene));
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }

    bool SceneHierarchyPanel::matchesSearchFilter(const std::string& name) const {
        if (m_search_buffer[0] == '\0') {
            return true;
        }

        const std::string filter = toLowerCopy(m_search_buffer.data());
        const std::string lower_name = toLowerCopy(name);
        return lower_name.find(filter) != std::string::npos;
    }

    const char* SceneHierarchyPanel::getEntityIcon(Entity entity) const {
        if (entity.hasComponent<CameraComponent>() || entity.hasComponent<ActiveCameraComponent>()) {
            return EditorIcons::Camera;
        }
        if (entity.hasComponent<DirectionalLightComponent>() || entity.hasComponent<PointLightComponent>()) {
            return EditorIcons::Light;
        }
        if (entity.hasComponent<MeshRendererComponent>()) {
            return EditorIcons::Mesh;
        }
        return EditorIcons::Entity;
    }

    void SceneHierarchyPanel::selectEntity(Entity entity) {
        if (!m_context) {
            return;
        }

        if (std::shared_ptr<SelectionService> selection_service = m_context->selection_service.lock()) {
            selection_service->setSelectedEntity(entity, getName());
        } else {
            m_context->selected_entity = entity;
            m_context->selection_source = getName();
        }
    }

    void SceneHierarchyPanel::clearSelection() {
        if (!m_context) {
            return;
        }

        if (std::shared_ptr<SelectionService> selection_service = m_context->selection_service.lock()) {
            selection_service->clearSelection();
        } else {
            m_context->selected_entity = Entity();
            m_context->selection_source = getName();
        }
    }

    Entity SceneHierarchyPanel::createEmptyEntity(const std::shared_ptr<SceneV2>& scene, const std::string& name) {
        return scene ? scene->createEntity(name) : Entity();
    }

    Entity SceneHierarchyPanel::createCameraEntity(const std::shared_ptr<SceneV2>& scene) {
        Entity entity = createEmptyEntity(scene, "Camera");
        if (!entity) {
            return entity;
        }

        auto& camera = entity.addComponent<CameraComponent>();
        camera.position = { 0.0f, 0.0f, 5.0f };
        camera.nearClip = 0.1f;
        camera.farClip = 1000.0f;
        camera.viewMatrix = glm::lookAt(camera.position, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        camera.projectionMatrix = glm::perspective(glm::radians(45.0f), 16.0f / 9.0f, camera.nearClip, camera.farClip);
        camera.viewProjectionMatrix = camera.projectionMatrix * camera.viewMatrix;
        return entity;
    }

    Entity SceneHierarchyPanel::createDirectionalLightEntity(const std::shared_ptr<SceneV2>& scene) {
        Entity entity = createEmptyEntity(scene, "DirectionalLight");
        if (entity) {
            entity.addComponent<DirectionalLightComponent>();
        }
        return entity;
    }

    Entity SceneHierarchyPanel::createPointLightEntity(const std::shared_ptr<SceneV2>& scene) {
        Entity entity = createEmptyEntity(scene, "PointLight");
        if (entity) {
            auto& light = entity.addComponent<PointLightComponent>();
            light.intensity = 1.0f;
            entity.getComponent<TransformComponent>().translation = glm::vec3(-5.0f, 0.0f, 0.0f);
        }
        return entity;
    }

    Entity SceneHierarchyPanel::duplicateEntity(const std::shared_ptr<SceneV2>& scene, Entity source) {
        if (!scene || !source || !source.hasComponent<TagComponent>()) {
            return Entity();
        }

        Entity duplicate = scene->createEntity(source.getComponent<TagComponent>().name + " Copy");
        if (source.hasComponent<TransformComponent>()) {
            duplicate.getComponent<TransformComponent>() = source.getComponent<TransformComponent>();
        }

        if (source.hasComponent<CameraComponent>()) {
            duplicate.addComponent<CameraComponent>(source.getComponent<CameraComponent>());
        }
        if (source.hasComponent<ActiveCameraComponent>()) {
            duplicate.addComponent<ActiveCameraComponent>(source.getComponent<ActiveCameraComponent>());
        }
        if (source.hasComponent<MeshRendererComponent>()) {
            duplicate.addComponent<MeshRendererComponent>(source.getComponent<MeshRendererComponent>());
        }
        if (source.hasComponent<DirectionalLightComponent>()) {
            duplicate.addComponent<DirectionalLightComponent>(source.getComponent<DirectionalLightComponent>());
        }
        if (source.hasComponent<PointLightComponent>()) {
            duplicate.addComponent<PointLightComponent>(source.getComponent<PointLightComponent>());
        }
        if (source.hasComponent<EnvironmentComponent>()) {
            duplicate.addComponent<EnvironmentComponent>(source.getComponent<EnvironmentComponent>());
        }

        return duplicate;
    }

    void SceneHierarchyPanel::deleteEntity(const std::shared_ptr<SceneV2>& scene, Entity entity) {
        if (!scene || !entity) {
            return;
        }

        const bool was_selected = m_context && m_context->selected_entity == entity;
        scene->destroyEntity(entity);
        if (was_selected) {
            clearSelection();
        }
    }

    void SceneHierarchyPanel::onEvent(Event& event) {
        (void)event;
    }
} // namespace NexAur
