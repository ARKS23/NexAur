#include "pch.h"
#include "properties_panel.h"

#include "Editor/Widgets/editor_property_drawer.h"
#include "Editor/Widgets/editor_widgets.h"
#include "Function/Resource/asset_metadata.h"
#include "Function/Resource/asset_manager.h"
#include "Function/Resource/material_asset.h"
#include "Function/Resource/model.h"
#include "Function/Renderer/renderer_service.h"
#include "Function/Scene/component.h"
#include "Function/Scene/procedural_primitive_entity.h"

#include <imgui.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstring>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

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

        void copyStringToBuffer(const std::string& value, char* buffer, size_t buffer_size) {
            if (!buffer || buffer_size == 0) {
                return;
            }

            const size_t copy_size = std::min(value.size(), buffer_size - 1);
            std::memcpy(buffer, value.data(), copy_size);
            buffer[copy_size] = '\0';
        }

        const char* reflectionProbeCaptureStatusText(ReflectionProbeCaptureStatus status) {
            switch (status) {
            case ReflectionProbeCaptureStatus::Idle:
                return "Idle";
            case ReflectionProbeCaptureStatus::Pending:
                return "Pending";
            case ReflectionProbeCaptureStatus::Capturing:
                return "Capturing";
            case ReflectionProbeCaptureStatus::Ready:
                return "Fresh";
            case ReflectionProbeCaptureStatus::Failed:
                return "Failed";
            default:
                return "Unknown";
            }
        }

        std::string assetDisplayName(AssetHandle handle, const AssetManager* asset_manager) {
            if (!handle) {
                return "None";
            }

            if (asset_manager) {
                if (const AssetMetadata* metadata = asset_manager->getMetadata(handle)) {
                    if (!metadata->debug_name.empty()) {
                        return metadata->debug_name;
                    }
                    if (!metadata->path.empty()) {
                        return metadata->path;
                    }
                }
            }

            std::ostringstream stream;
            stream << "Asset " << static_cast<uint64_t>(handle.id);
            return stream.str();
        }

        void trimTrailingInvalidOverrides(std::vector<AssetHandle>& overrides) {
            while (!overrides.empty() && !overrides.back()) {
                overrides.pop_back();
            }
        }

        AssetHandle materialOverrideAt(const MeshRendererComponent& mesh_renderer, size_t slot_index) {
            return slot_index < mesh_renderer.material_overrides.size()
                ? mesh_renderer.material_overrides[slot_index]
                : AssetHandle();
        }

        void setMaterialOverride(
            MeshRendererComponent& mesh_renderer,
            size_t slot_index,
            AssetHandle material_handle) {
            if (mesh_renderer.material_overrides.size() <= slot_index) {
                mesh_renderer.material_overrides.resize(slot_index + 1u);
            }
            mesh_renderer.material_overrides[slot_index] = material_handle;
            trimTrailingInvalidOverrides(mesh_renderer.material_overrides);
        }

        std::string defaultMaterialName(const MaterialImportData& material, size_t slot_index) {
            if (!material.name.empty()) {
                return material.name;
            }
            return "Default Slot " + std::to_string(slot_index);
        }

        std::string runtimeMaterialName(Entity entity, size_t slot_index, const MaterialImportData& source) {
            std::string entity_name = "Entity";
            if (entity && entity.hasComponent<TagComponent>()) {
                entity_name = entity.getComponent<TagComponent>().name;
            }

            return entity_name + "." + defaultMaterialName(source, slot_index) + ".Instance";
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
        drawProceduralPrimitiveComponent(selected);
        drawDirectionalLightComponent(selected);
        drawPointLightComponent(selected);
        drawRectLightComponent(selected);
        drawEnvironmentComponent(selected);
        drawReflectionProbeComponent(selected);

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
            drawAddComponentItem<ReflectionProbeComponent>(entity, "Reflection Probe");
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

        const bool reflection_probe = entity.hasComponent<ReflectionProbeComponent>();
        if (EditorPropertyDrawer::drawVec3Property(
                "Translation",
                tc.translation,
                0.05f,
                kPosMin,
                kPosMax,
                "%.3f",
                kFlags) &&
            reflection_probe) {
            entity.getComponent<ReflectionProbeComponent>().capture_dirty = true;
        }

        if (reflection_probe) {
            ImGui::BeginDisabled();
        }
        glm::vec3 rotation_degrees = glm::degrees(tc.rotation);
        if (EditorPropertyDrawer::drawVec3Property("Rotation", rotation_degrees, 0.5f, kRotMin, kRotMax, "%.1f", kFlags)) {
            tc.rotation = glm::radians(rotation_degrees);
        }

        if (EditorPropertyDrawer::drawVec3Property("Scale", tc.scale, 0.05f, kScaleMin, kScaleMax, "%.3f", kFlags)) {
            tc.scale = glm::max(tc.scale, glm::vec3(kScaleMin)); // 确保缩放不为负数或过小
        }

        if (reflection_probe) {
            ImGui::EndDisabled();
        }

        EditorWidgets::propertyRow("Actions", [&]() {
            if (ImGui::Button("Reset Transform")) {
                tc.translation = glm::vec3(0.0f);
                tc.rotation = glm::vec3(0.0f);
                tc.scale = glm::vec3(1.0f);
                if (reflection_probe) {
                    entity.getComponent<ReflectionProbeComponent>().capture_dirty = true;
                }
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
        drawMaterialSlots(entity, mesh_renderer);
    }

    void PropertiesPanel::drawMaterialSlots(Entity entity, MeshRendererComponent& mesh_renderer) {
        AssetManager* asset_manager =
            m_context && m_context->asset_manager ? m_context->asset_manager.get() : nullptr;
        if (!asset_manager || !mesh_renderer.model_asset) {
            return;
        }

        std::shared_ptr<Model> model = asset_manager->loadModelCPU(mesh_renderer.model_asset);
        if (!model || !model->isLoaded()) {
            return;
        }

        if (!EditorWidgets::sectionHeader("Materials", true)) {
            return;
        }

        const std::vector<Mesh>& meshes = model->getMeshes();
        for (size_t slot_index = 0; slot_index < meshes.size(); ++slot_index) {
            drawMaterialSlot(entity, mesh_renderer, *model, slot_index);
        }
    }

    void PropertiesPanel::drawMaterialSlot(
        Entity entity,
        MeshRendererComponent& mesh_renderer,
        const Model& model,
        size_t slot_index) {
        AssetManager* asset_manager =
            m_context && m_context->asset_manager ? m_context->asset_manager.get() : nullptr;
        if (!asset_manager || slot_index >= model.getMeshes().size()) {
            return;
        }

        const MaterialImportData& default_material =
            model.getMeshes()[slot_index].getMaterialImportData();
        const AssetHandle override_material = materialOverrideAt(mesh_renderer, slot_index);

        const std::string slot_label = "Slot " + std::to_string(slot_index);
        ImGui::PushID(static_cast<int>(slot_index));
        if (ImGui::TreeNodeEx(slot_label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth)) {
            const std::string default_name = defaultMaterialName(default_material, slot_index);
            EditorWidgets::propertyRow("Default", [&]() {
                ImGui::TextDisabled("%s", default_name.c_str());
            });

            const std::string override_name = assetDisplayName(override_material, asset_manager);
            EditorWidgets::propertyRow("Override", [&]() {
                ImGui::TextDisabled("%s", override_name.c_str());
            });

            EditorWidgets::propertyRow("Actions", [&]() {
                if (ImGui::Button("Make Instance")) {
                    std::shared_ptr<MaterialAsset> source_material =
                        override_material ? asset_manager->loadMaterialCPU(override_material) : nullptr;
                    if (!source_material) {
                        source_material = asset_manager->createMaterialFromImportData(default_material);
                    }

                    if (source_material) {
                        const AssetHandle material_instance =
                            asset_manager->createRuntimeMaterialInstance(
                                *source_material,
                                runtimeMaterialName(entity, slot_index, default_material));
                        if (material_instance) {
                            setMaterialOverride(mesh_renderer, slot_index, material_instance);
                        }
                    }
                }

                ImGui::SameLine();
                ImGui::BeginDisabled(!override_material);
                if (ImGui::Button("Use Default")) {
                    setMaterialOverride(mesh_renderer, slot_index, AssetHandle());
                }
                ImGui::EndDisabled();
            });

            const AssetHandle editable_material = materialOverrideAt(mesh_renderer, slot_index);
            if (editable_material) {
                std::shared_ptr<MaterialAsset> material =
                    asset_manager->loadMaterialCPU(editable_material);
                if (material) {
                    glm::vec4 base_color = material->getBaseColorFactor();
                    if (EditorPropertyDrawer::drawColor4Property("Base Color", base_color)) {
                        material->setBaseColorFactor(base_color);
                    }

                    float metallic = material->getMetallicFactor();
                    if (EditorPropertyDrawer::drawFloatProperty(
                            "Metallic",
                            metallic,
                            0.01f,
                            0.0f,
                            1.0f,
                            "%.3f",
                            ImGuiSliderFlags_AlwaysClamp)) {
                        material->setMetallicFactor(metallic);
                    }

                    float roughness = material->getRoughnessFactor();
                    if (EditorPropertyDrawer::drawFloatProperty(
                            "Roughness",
                            roughness,
                            0.01f,
                            0.0f,
                            1.0f,
                            "%.3f",
                            ImGuiSliderFlags_AlwaysClamp)) {
                        material->setRoughnessFactor(roughness);
                    }

                    glm::vec3 emissive = material->getEmissiveFactor();
                    if (EditorPropertyDrawer::drawColor3Property("Emissive", emissive)) {
                        material->setEmissiveFactor(emissive);
                    }

                    float normal_scale = material->getNormalScale();
                    if (EditorPropertyDrawer::drawFloatProperty(
                            "Normal Scale",
                            normal_scale,
                            0.01f,
                            0.0f,
                            4.0f,
                            "%.3f",
                            ImGuiSliderFlags_AlwaysClamp)) {
                        material->setNormalScale(normal_scale);
                    }

                    float ao_strength = material->getOcclusionStrength();
                    if (EditorPropertyDrawer::drawFloatProperty(
                            "AO Strength",
                            ao_strength,
                            0.01f,
                            0.0f,
                            1.0f,
                            "%.3f",
                            ImGuiSliderFlags_AlwaysClamp)) {
                        material->setOcclusionStrength(ao_strength);
                    }
                }
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void PropertiesPanel::drawProceduralPrimitiveComponent(Entity entity) {
        if (!entity.hasComponent<ProceduralPrimitiveComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Procedural Primitive")) return;

        ProceduralPrimitiveComponent& primitive = entity.getComponent<ProceduralPrimitiveComponent>();
        bool changed = false;

        EditorWidgets::propertyRow("Type", [&]() {
            const char* items[] = { "Cube", "Sphere", "Plane", "Cylinder", "Cone" };
            constexpr int kMaxPrimitiveTypeIndex = static_cast<int>(ProceduralPrimitiveType::Cone);
            int index = std::clamp(static_cast<int>(primitive.type), 0, kMaxPrimitiveTypeIndex);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("##ProceduralPrimitiveType", &index, items, IM_ARRAYSIZE(items))) {
                primitive.type = static_cast<ProceduralPrimitiveType>(index);
                changed = true;
            }
        });

        int segments = static_cast<int>(primitive.segments);
        if (EditorPropertyDrawer::drawIntProperty(
                "Segments",
                segments,
                1,
                3,
                128,
                ImGuiSliderFlags_AlwaysClamp)) {
            primitive.segments = static_cast<uint32_t>(std::clamp(segments, 3, 128));
            changed = true;
        }

        int rings = static_cast<int>(primitive.rings);
        if (EditorPropertyDrawer::drawIntProperty(
                "Rings",
                rings,
                1,
                2,
                128,
                ImGuiSliderFlags_AlwaysClamp)) {
            primitive.rings = static_cast<uint32_t>(std::clamp(rings, 2, 128));
            changed = true;
        }

        EditorWidgets::propertyRow("Actions", [&]() {
            if (ImGui::Button("Rebuild")) {
                changed = true;
            }
        });

        if (changed && m_context && m_context->asset_manager) {
            refreshProceduralPrimitiveMesh(entity, *m_context->asset_manager);
        }
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

    void PropertiesPanel::drawReflectionProbeComponent(Entity entity) {
        if (!entity.hasComponent<ReflectionProbeComponent>()) return;
        if (!EditorPropertyDrawer::drawComponentHeader("Reflection Probe")) return;

        ReflectionProbeComponent& probe = entity.getComponent<ReflectionProbeComponent>();
        const TransformComponent* transform = entity.hasComponent<TransformComponent>() ?
            &entity.getComponent<TransformComponent>() :
            nullptr;
        AssetManager* asset_manager =
            m_context && m_context->asset_manager ? m_context->asset_manager.get() : nullptr;
        constexpr ImGuiSliderFlags kFlags = ImGuiSliderFlags_AlwaysClamp;

        EditorPropertyDrawer::drawAssetField("Environment", probe.environment_asset, asset_manager);

        static std::array<char, 260> s_environment_path_buffer{};
        static uint32_t s_last_entity_id = 0;
        static uint64_t s_last_asset_id = 0;
        const uint32_t entity_id = static_cast<uint32_t>(entity);
        const uint64_t asset_id = static_cast<uint64_t>(probe.environment_asset.id);
        if (s_last_entity_id != entity_id || s_last_asset_id != asset_id) {
            s_environment_path_buffer.fill('\0');
            if (asset_manager && probe.environment_asset) {
                copyStringToBuffer(
                    asset_manager->getPath(probe.environment_asset),
                    s_environment_path_buffer.data(),
                    s_environment_path_buffer.size());
            }
            s_last_entity_id = entity_id;
            s_last_asset_id = asset_id;
        }

        EditorWidgets::propertyRow("Environment Path", [&]() {
            ImGui::SetNextItemWidth(220.0f);
            ImGui::InputText(
                "##ReflectionProbeEnvironmentPath",
                s_environment_path_buffer.data(),
                s_environment_path_buffer.size());
        });

        EditorWidgets::propertyRow("Actions", [&]() {
            const bool can_import = asset_manager != nullptr && s_environment_path_buffer[0] != '\0';
            ImGui::BeginDisabled(!can_import);
            if (ImGui::Button("Import")) {
                AssetHandle imported =
                    asset_manager->importEnvironmentMapAsset(s_environment_path_buffer.data());
                if (imported) {
                    probe.environment_asset = imported;
                    probe.capture_dirty = true;
                    s_last_asset_id = static_cast<uint64_t>(probe.environment_asset.id);
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Scene Env")) {
                AssetHandle scene_environment;
                if (m_context && m_context->active_scene) {
                    auto view = m_context->active_scene->getRegistry().view<EnvironmentComponent>();
                    for (auto env_entity : view) {
                        scene_environment = view.get<EnvironmentComponent>(env_entity).getEnvironmentHandle();
                        break;
                    }
                }

                if (scene_environment) {
                    probe.environment_asset = scene_environment;
                    probe.capture_dirty = true;
                    if (asset_manager) {
                        copyStringToBuffer(
                            asset_manager->getPath(probe.environment_asset),
                            s_environment_path_buffer.data(),
                            s_environment_path_buffer.size());
                    }
                    s_last_asset_id = static_cast<uint64_t>(probe.environment_asset.id);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                probe.environment_asset = AssetHandle();
                s_environment_path_buffer.fill('\0');
                s_last_asset_id = 0;
                probe.capture_dirty = true;
            }
        });

        EditorPropertyDrawer::drawBoolProperty("Enabled", probe.enabled);
        EditorPropertyDrawer::drawBoolProperty("Box Projection", probe.box_projection);
        if (EditorPropertyDrawer::drawVec3Property("Box Extents", probe.box_extents, 0.05f, 0.05f, 100.0f, "%.2f", kFlags)) {
            probe.box_extents = glm::max(probe.box_extents, glm::vec3{ 0.05f });
        }
        EditorPropertyDrawer::drawFloatProperty("Intensity", probe.intensity, 0.02f, 0.0f, 10.0f, "%.2f", kFlags);
        EditorPropertyDrawer::drawBoolProperty("Diffuse Enabled", probe.diffuse_enabled);
        EditorPropertyDrawer::drawFloatProperty(
            "Diffuse Intensity",
            probe.diffuse_intensity,
            0.02f,
            0.0f,
            10.0f,
            "%.2f",
            kFlags);
        EditorPropertyDrawer::drawFloatProperty("Blend Distance", probe.blend_distance, 0.02f, 0.0f, 20.0f, "%.2f", kFlags);

        int capture_resolution = static_cast<int>(probe.capture_resolution);
        if (EditorPropertyDrawer::drawIntProperty(
                "Capture Resolution",
                capture_resolution,
                16,
                32,
                1024,
                kFlags)) {
            probe.capture_resolution = static_cast<uint32_t>(std::clamp(capture_resolution, 32, 1024));
            probe.capture_dirty = true;
        }
        int capture_priority = static_cast<int>(probe.capture_priority);
        if (EditorPropertyDrawer::drawIntProperty(
                "Capture Priority",
                capture_priority,
                1,
                0,
                100,
                kFlags)) {
            probe.capture_priority = static_cast<uint32_t>(std::clamp(capture_priority, 0, 100));
        }
        if (EditorPropertyDrawer::drawFloatProperty(
                "Capture Near",
                probe.capture_near_clip,
                0.01f,
                0.001f,
                10.0f,
                "%.3f",
                kFlags)) {
            probe.capture_near_clip = std::max(0.001f, probe.capture_near_clip);
            probe.capture_far_clip = std::max(probe.capture_far_clip, probe.capture_near_clip + 0.001f);
            probe.capture_dirty = true;
        }
        if (EditorPropertyDrawer::drawFloatProperty(
                "Capture Far",
                probe.capture_far_clip,
                0.25f,
                1.0f,
                500.0f,
                "%.2f",
                kFlags)) {
            probe.capture_far_clip = std::max(probe.capture_far_clip, probe.capture_near_clip + 0.001f);
            probe.capture_dirty = true;
        }
        if (EditorPropertyDrawer::drawBoolProperty("Capture Skybox", probe.capture_include_skybox)) {
            probe.capture_dirty = true;
        }
        EditorPropertyDrawer::drawReadOnlyText(
            "Capture Freshness",
            probe.capture_dirty ? "Dirty" : "Fresh");

        const int probe_entity_id = static_cast<int>(static_cast<uint32_t>(entity));
        const ReflectionProbeCaptureState capture_state =
            m_context && m_context->renderer_service ?
                m_context->renderer_service->getReflectionProbeCaptureState(probe_entity_id) :
                ReflectionProbeCaptureState{};
        EditorPropertyDrawer::drawReadOnlyText(
            "Capture Status",
            std::string(reflectionProbeCaptureStatusText(capture_state.status)) + " - " + capture_state.message);
        std::string baked_asset_text = "None";
        if (probe.baked_environment_asset) {
            const AssetMetadata* metadata =
                asset_manager ? asset_manager->getMetadata(probe.baked_environment_asset) : nullptr;
            baked_asset_text = metadata && !metadata->debug_name.empty() ?
                metadata->debug_name :
                std::to_string(static_cast<uint64_t>(probe.baked_environment_asset.id));
        }
        EditorPropertyDrawer::drawReadOnlyText("Baked Asset", baked_asset_text);

        EditorWidgets::propertyRow("Capture", [&]() {
            auto request_capture = [&](ReflectionProbeCaptureKind kind) {
                if (!m_context || !m_context->renderer_service) {
                    return;
                }

                ReflectionProbeCaptureRequest request;
                request.entity_id = probe_entity_id;
                request.resolution = std::clamp(probe.capture_resolution, 32u, 1024u);
                request.priority = probe.capture_priority;
                request.near_clip = std::max(0.001f, probe.capture_near_clip);
                request.far_clip = std::max(probe.capture_far_clip, request.near_clip + 0.001f);
                request.include_skybox = probe.capture_include_skybox;
                request.kind = kind;
                request.input_hash = transform ? probe.computeCaptureInputHash(*transform) : 0;
                probe.capture_dirty = true;
                m_context->renderer_service->requestReflectionProbeCapture(request);
            };

            const bool can_capture =
                probe.enabled && transform && m_context && m_context->renderer_service;
            ImGui::BeginDisabled(!can_capture);
            if (ImGui::Button("Capture Probe")) {
                request_capture(ReflectionProbeCaptureKind::Capture);
            }
            ImGui::SameLine();
            if (ImGui::Button("Bake Probe")) {
                request_capture(ReflectionProbeCaptureKind::Bake);
            }
            ImGui::SameLine();
            const bool can_clear =
                m_context &&
                m_context->renderer_service &&
                (capture_state.runtime_resource_ready || probe.baked_environment_asset);
            ImGui::BeginDisabled(!can_clear);
            if (ImGui::Button("Clear Baked Data")) {
                const bool cleared_runtime =
                    m_context->renderer_service->clearReflectionProbeCapture(probe_entity_id);
                if (cleared_runtime || probe.baked_environment_asset) {
                    probe.baked_environment_asset = AssetHandle();
                    probe.capture_dirty = true;
                }
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();
        });
    }
} // namespace NexAur
