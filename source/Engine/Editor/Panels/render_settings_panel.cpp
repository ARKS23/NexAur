#include "pch.h"
#include "render_settings_panel.h"

#include "Editor/Widgets/editor_widgets.h"
#include "Function/Renderer/data/render_context.h"

#include <algorithm>
#include <cstdint>
#include <imgui.h>

namespace NexAur {
    namespace {
        void setControlWidth() {
            ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x));
        }

        int toneMappingModeToIndex(RenderToneMappingMode mode) {
            switch (mode) {
            case RenderToneMappingMode::ACES:
                return 1;
            case RenderToneMappingMode::None:
            default:
                return 0;
            }
        }

        RenderToneMappingMode toneMappingModeFromIndex(int index) {
            return index == 1 ? RenderToneMappingMode::ACES : RenderToneMappingMode::None;
        }

        int iblDebugModeToIndex(RenderIblDebugMode mode) {
            return static_cast<int>(mode);
        }

        RenderIblDebugMode iblDebugModeFromIndex(int index) {
            switch (index) {
            case 1:
                return RenderIblDebugMode::DiffuseIbl;
            case 2:
                return RenderIblDebugMode::SpecularIbl;
            case 3:
                return RenderIblDebugMode::CombinedIbl;
            case 4:
                return RenderIblDebugMode::Normal;
            case 5:
                return RenderIblDebugMode::Metallic;
            case 6:
                return RenderIblDebugMode::Roughness;
            case 7:
                return RenderIblDebugMode::AmbientOcclusion;
            case 8:
                return RenderIblDebugMode::Emissive;
            case 9:
                return RenderIblDebugMode::Irradiance;
            case 10:
                return RenderIblDebugMode::PrefilteredEnvironment;
            case 11:
                return RenderIblDebugMode::BrdfLut;
            case 0:
            default:
                return RenderIblDebugMode::FinalLit;
            }
        }

        int shadowFilterModeToIndex(RenderShadowFilterMode mode) {
            switch (mode) {
            case RenderShadowFilterMode::PCF3x3:
                return 1;
            case RenderShadowFilterMode::PCF5x5:
                return 2;
            case RenderShadowFilterMode::PoissonPCF:
                return 3;
            case RenderShadowFilterMode::PCSS:
                return 4;
            case RenderShadowFilterMode::Hard:
            default:
                return 0;
            }
        }

        RenderShadowFilterMode shadowFilterModeFromIndex(int index) {
            switch (index) {
            case 1:
                return RenderShadowFilterMode::PCF3x3;
            case 2:
                return RenderShadowFilterMode::PCF5x5;
            case 3:
                return RenderShadowFilterMode::PoissonPCF;
            case 4:
                return RenderShadowFilterMode::PCSS;
            case 0:
            default:
                return RenderShadowFilterMode::Hard;
            }
        }

        int shadowMapResolutionToIndex(uint32_t resolution) {
            if (resolution >= 4096u) {
                return 2;
            }
            if (resolution >= 2048u) {
                return 1;
            }
            return 0;
        }

        uint32_t shadowMapResolutionFromIndex(int index) {
            switch (index) {
            case 1:
                return 2048u;
            case 2:
                return 4096u;
            case 0:
            default:
                return 1024u;
            }
        }

        int effectDebugViewToIndex(RenderEffectDebugView view) {
            return static_cast<int>(view);
        }

        RenderEffectDebugView effectDebugViewFromIndex(int index) {
            switch (index) {
            case 1:
                return RenderEffectDebugView::HdrSceneColor;
            case 2:
                return RenderEffectDebugView::BloomComposite;
            case 3:
                return RenderEffectDebugView::BloomDownsampleMip;
            case 4:
                return RenderEffectDebugView::BloomUpsampleMip;
            case 5:
                return RenderEffectDebugView::ShadowMap;
            case 6:
                return RenderEffectDebugView::ShadowCascades;
            case 7:
                return RenderEffectDebugView::SceneDepth;
            case 8:
                return RenderEffectDebugView::AoRaw;
            case 9:
                return RenderEffectDebugView::AoBlurred;
            case 0:
            default:
                return RenderEffectDebugView::FinalLit;
            }
        }
    } // namespace

    void RenderSettingsPanel::onUIRender() {
        bool& open_flag = getOpenFlag();
        if (!ImGui::Begin(getName().c_str(), &open_flag)) {
            ImGui::End();
            return;
        }

        if (!m_context || !m_context->render_context) {
            ImGui::TextDisabled("Render context is unavailable.");
            ImGui::End();
            return;
        }

        if (EditorWidgets::toolbarButton("Reset Render", "Restore runtime render settings to defaults.")) {
            m_context->render_context->setRenderSettings(RenderSettings());
        }
        ImGui::SameLine();
        if (EditorWidgets::toolbarButton("Reset Debug", "Restore debug visualization options to defaults.")) {
            m_context->render_context->setDebugVisualizationOptions(RenderDebugVisualizationOptions());
        }

        drawDebugVisualizationSection();

        RenderSettings settings = m_context->render_context->getRenderSettings();
        bool changed = false;

        drawEffectsDebugSection(settings, changed);
        drawPostProcessSection(settings, changed);
        drawAoSection(settings, changed);
        drawIblDebugSection(settings, changed);
        drawShadowSection(settings, changed);

        if (changed) {
            m_context->render_context->setRenderSettings(settings);
        }

        ImGui::End();
    }

    void RenderSettingsPanel::drawEffectsDebugSection(RenderSettings& settings, bool& changed) {
        if (!EditorWidgets::sectionHeader("Effects Debug")) {
            return;
        }

        EditorWidgets::propertyRow("View", [&]() {
            const char* items[] = {
                "Final Lit",
                "HDR Scene Color",
                "Bloom Composite",
                "Bloom Downsample Mip",
                "Bloom Upsample Mip",
                "Shadow Map",
                "Shadow Cascades",
                "Scene Depth",
                "AO Raw",
                "AO Blurred"
            };

            int index = effectDebugViewToIndex(settings.effects_debug.view);
            setControlWidth();
            if (ImGui::Combo("##EffectDebugView", &index, items, IM_ARRAYSIZE(items))) {
                settings.effects_debug.view = effectDebugViewFromIndex(index);
                changed = true;
            }
        });

        const bool mip_debug =
            settings.effects_debug.view == RenderEffectDebugView::BloomDownsampleMip ||
            settings.effects_debug.view == RenderEffectDebugView::BloomUpsampleMip;
        const bool shadow_debug =
            settings.effects_debug.view == RenderEffectDebugView::ShadowMap ||
            settings.effects_debug.view == RenderEffectDebugView::ShadowCascades;

        if (!mip_debug) {
            ImGui::BeginDisabled();
        }
        EditorWidgets::propertyRow("Bloom Mip", [&]() {
            int mip = static_cast<int>(settings.effects_debug.bloom_mip);
            setControlWidth();
            if (ImGui::SliderInt("##EffectDebugBloomMip", &mip, 0, 5)) {
                settings.effects_debug.bloom_mip = static_cast<uint32_t>(std::max(0, mip));
                changed = true;
            }
        });
        if (!mip_debug) {
            ImGui::EndDisabled();
        }

        if (!shadow_debug) {
            ImGui::BeginDisabled();
        }
        EditorWidgets::propertyRow("Shadow Cascade", [&]() {
            int cascade = static_cast<int>(settings.effects_debug.shadow_cascade);
            setControlWidth();
            if (ImGui::SliderInt("##EffectDebugShadowCascade", &cascade, 0, 3)) {
                settings.effects_debug.shadow_cascade = static_cast<uint32_t>(std::max(0, cascade));
                changed = true;
            }
        });
        if (!shadow_debug) {
            ImGui::EndDisabled();
        }

        if (settings.effects_debug.view == RenderEffectDebugView::BloomComposite ||
            settings.effects_debug.view == RenderEffectDebugView::BloomDownsampleMip ||
            settings.effects_debug.view == RenderEffectDebugView::BloomUpsampleMip) {
            ImGui::TextDisabled("Bloom debug can run the bloom graph without changing the Bloom toggle.");
        }
        if (settings.effects_debug.view == RenderEffectDebugView::AoRaw ||
            settings.effects_debug.view == RenderEffectDebugView::AoBlurred) {
            ImGui::TextDisabled("AO debug can run the AO graph without changing the AO toggle.");
        }
    }

    void RenderSettingsPanel::drawDebugVisualizationSection() {
        if (!EditorWidgets::sectionHeader("Viewport Debug")) {
            return;
        }

        RenderDebugVisualizationOptions options =
            m_context->render_context->getDebugVisualizationOptions();

        bool changed = false;
        EditorWidgets::propertyRow("Debug Draw", [&]() {
            changed |= ImGui::Checkbox("##DebugDraw", &options.enabled);
        });

        if (!options.enabled) {
            ImGui::BeginDisabled();
        }

        EditorWidgets::propertyRow("Camera Frustum", [&]() {
            changed |= ImGui::Checkbox("##CameraFrustum", &options.camera_frustum);
        });
        EditorWidgets::propertyRow("Light Gizmos", [&]() {
            changed |= ImGui::Checkbox("##LightGizmos", &options.light_gizmos);
        });

        if (!options.enabled) {
            ImGui::EndDisabled();
        }

        if (changed) {
            m_context->render_context->setDebugVisualizationOptions(options);
        }
    }

    void RenderSettingsPanel::drawPostProcessSection(RenderSettings& settings, bool& changed) {
        if (!EditorWidgets::sectionHeader("Post Processing")) {
            return;
        }

        EditorWidgets::propertyRow("Tone Mapping", [&]() {
            const char* items[] = { "None", "ACES" };
            int index = toneMappingModeToIndex(settings.post_process.tone_mapping_mode);
            setControlWidth();
            if (ImGui::Combo("##ToneMapping", &index, items, IM_ARRAYSIZE(items))) {
                settings.post_process.tone_mapping_mode = toneMappingModeFromIndex(index);
                changed = true;
            }
        });

        EditorWidgets::propertyRow("Exposure", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##Exposure", &settings.post_process.exposure, 0.0f, 4.0f, "%.2f");
        });

        EditorWidgets::propertyRow("Bloom", [&]() {
            changed |= ImGui::Checkbox("##BloomEnabled", &settings.post_process.bloom_enabled);
        });

        if (!settings.post_process.bloom_enabled) {
            ImGui::BeginDisabled();
        }

        EditorWidgets::propertyRow("Intensity", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##BloomIntensity", &settings.post_process.bloom_intensity, 0.0f, 1.0f, "%.3f");
        });
        EditorWidgets::propertyRow("Scatter", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##BloomScatter", &settings.post_process.bloom_scatter, 0.0f, 1.0f, "%.2f");
        });
        EditorWidgets::propertyRow("Radius", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##BloomRadius", &settings.post_process.bloom_radius, 0.25f, 2.5f, "%.2f");
        });

        if (!settings.post_process.bloom_enabled) {
            ImGui::EndDisabled();
        }
    }

    void RenderSettingsPanel::drawAoSection(RenderSettings& settings, bool& changed) {
        if (!EditorWidgets::sectionHeader("Ambient Occlusion")) {
            return;
        }

        EditorWidgets::propertyRow("Enabled", [&]() {
            changed |= ImGui::Checkbox("##AoEnabled", &settings.ao.enabled);
        });

        if (!settings.ao.enabled) {
            ImGui::BeginDisabled();
        }

        EditorWidgets::propertyRow("Radius", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##AoRadius", &settings.ao.radius, 0.05f, 5.0f, "%.2f");
        });
        EditorWidgets::propertyRow("Intensity", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##AoIntensity", &settings.ao.intensity, 0.0f, 2.0f, "%.2f");
        });
        EditorWidgets::propertyRow("Bias", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##AoBias", &settings.ao.bias, 0.0f, 0.2f, "%.3f");
        });
        EditorWidgets::propertyRow("Power", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##AoPower", &settings.ao.power, 0.25f, 4.0f, "%.2f");
        });
        EditorWidgets::propertyRow("Blur", [&]() {
            changed |= ImGui::Checkbox("##AoBlur", &settings.ao.blur_enabled);
        });
        EditorWidgets::propertyRow("Half Resolution", [&]() {
            changed |= ImGui::Checkbox("##AoHalfResolution", &settings.ao.half_resolution);
        });

        if (!settings.ao.enabled) {
            ImGui::EndDisabled();
        }
    }

    void RenderSettingsPanel::drawIblDebugSection(RenderSettings& settings, bool& changed) {
        if (!EditorWidgets::sectionHeader("IBL Debug", false)) {
            return;
        }

        EditorWidgets::propertyRow("Mode", [&]() {
            const char* items[] = {
                "Final Lit",
                "Diffuse IBL",
                "Specular IBL",
                "Combined IBL",
                "Normal",
                "Metallic",
                "Roughness",
                "Ambient Occlusion",
                "Emissive",
                "Irradiance",
                "Prefiltered Environment",
                "BRDF LUT"
            };

            int index = iblDebugModeToIndex(settings.ibl_debug.mode);
            setControlWidth();
            if (ImGui::Combo("##IblDebugMode", &index, items, IM_ARRAYSIZE(items))) {
                settings.ibl_debug.mode = iblDebugModeFromIndex(index);
                changed = true;
            }
        });

        EditorWidgets::propertyRow("Prefilter Mip", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##PrefilterMip", &settings.ibl_debug.prefilter_mip, 0.0f, 7.0f, "%.1f");
        });
    }

    void RenderSettingsPanel::drawShadowSection(RenderSettings& settings, bool& changed) {
        if (!EditorWidgets::sectionHeader("Shadow")) {
            return;
        }

        EditorWidgets::propertyRow("Enabled", [&]() {
            changed |= ImGui::Checkbox("##ShadowEnabled", &settings.shadow.enabled);
        });

        if (!settings.shadow.enabled) {
            ImGui::BeginDisabled();
        }

        EditorWidgets::propertyRow("Filter", [&]() {
            const char* items[] = { "Hard", "PCF 3x3", "PCF 5x5", "Poisson PCF", "PCSS" };
            int index = shadowFilterModeToIndex(settings.shadow.filter_mode);
            setControlWidth();
            if (ImGui::Combo("##ShadowFilter", &index, items, IM_ARRAYSIZE(items))) {
                settings.shadow.filter_mode = shadowFilterModeFromIndex(index);
                changed = true;
            }
        });
        EditorWidgets::propertyRow("Strength", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##ShadowStrength", &settings.shadow.strength, 0.0f, 1.0f, "%.2f");
        });
        EditorWidgets::propertyRow("Bias", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##ShadowBias", &settings.shadow.constant_bias, 0.0f, 0.02f, "%.5f");
        });
        EditorWidgets::propertyRow("Normal Bias", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##ShadowNormalBias", &settings.shadow.normal_bias, 0.0f, 0.2f, "%.4f");
        });
        EditorWidgets::propertyRow("Slope Bias", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##ShadowSlopeBias", &settings.shadow.slope_bias, 0.0f, 0.02f, "%.5f");
        });
        EditorWidgets::propertyRow("Filter Radius", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##ShadowFilterRadius", &settings.shadow.filter_radius, 0.25f, 3.0f, "%.2f");
        });

        const bool pcss_selected = settings.shadow.filter_mode == RenderShadowFilterMode::PCSS;
        if (!pcss_selected) {
            ImGui::BeginDisabled();
        }

        EditorWidgets::propertyRow("PCSS Light", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##PcssLightRadius", &settings.shadow.pcss_light_radius, 0.01f, 4.0f, "%.2f");
        });
        EditorWidgets::propertyRow("PCSS Search", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##PcssSearchRadius", &settings.shadow.pcss_search_radius, 0.5f, 12.0f, "%.2f");
        });
        EditorWidgets::propertyRow("PCSS Min", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##PcssMinRadius", &settings.shadow.pcss_min_filter_radius, 0.25f, 4.0f, "%.2f");
        });
        EditorWidgets::propertyRow("PCSS Max", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##PcssMaxRadius", &settings.shadow.pcss_max_filter_radius, 1.0f, 16.0f, "%.2f");
        });

        if (!pcss_selected) {
            ImGui::EndDisabled();
        }

        EditorWidgets::propertyRow("Distance", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##ShadowDistance", &settings.shadow.distance, 1.0f, 120.0f, "%.1f");
        });
        EditorWidgets::propertyRow("Map", [&]() {
            const char* items[] = { "1024", "2048", "4096" };
            int index = shadowMapResolutionToIndex(settings.shadow.map_resolution);
            setControlWidth();
            if (ImGui::Combo("##ShadowMap", &index, items, IM_ARRAYSIZE(items))) {
                settings.shadow.map_resolution = shadowMapResolutionFromIndex(index);
                changed = true;
            }
        });
        EditorWidgets::propertyRow("Stabilize", [&]() {
            changed |= ImGui::Checkbox("##ShadowStabilize", &settings.shadow.stabilize);
        });
        EditorWidgets::propertyRow("CSM", [&]() {
            changed |= ImGui::Checkbox("##CsmEnabled", &settings.shadow.cascades_enabled);
        });

        if (!settings.shadow.cascades_enabled) {
            ImGui::BeginDisabled();
        }

        EditorWidgets::propertyRow("Cascade Count", [&]() {
            int cascade_count = static_cast<int>(settings.shadow.cascade_count);
            setControlWidth();
            if (ImGui::SliderInt("##CascadeCount", &cascade_count, 1, 4)) {
                settings.shadow.cascade_count = static_cast<uint32_t>(cascade_count);
                changed = true;
            }
        });
        EditorWidgets::propertyRow("Split Lambda", [&]() {
            setControlWidth();
            changed |= ImGui::SliderFloat("##CascadeSplitLambda", &settings.shadow.cascade_split_lambda, 0.0f, 1.0f, "%.2f");
        });
        EditorWidgets::propertyRow("Cascade Overlay", [&]() {
            changed |= ImGui::Checkbox("##CascadeOverlay", &settings.shadow.cascade_debug_overlay);
        });

        if (!settings.shadow.cascades_enabled) {
            ImGui::EndDisabled();
        }

        if (!settings.shadow.enabled) {
            ImGui::EndDisabled();
        }
    }
} // namespace NexAur
