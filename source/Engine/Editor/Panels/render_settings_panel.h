#pragma once

#include "Core/Base.h"
#include "Editor/Panels/editor_panel.h"

#include <string>

namespace NexAur {
    struct RenderSettings;

    class NEXAUR_API RenderSettingsPanel : public EditorPanel {
    public:
        explicit RenderSettingsPanel(const std::string& name = "Render Settings")
            : EditorPanel(name) {}
        ~RenderSettingsPanel() override = default;

        void onUIRender() override;

    private:
        void drawDebugVisualizationSection();
        void drawLightingPresetSection(RenderSettings& settings, bool& changed);
        void drawEffectsDebugSection(RenderSettings& settings, bool& changed);
        void drawPostProcessSection(RenderSettings& settings, bool& changed);
        void drawAoSection(RenderSettings& settings, bool& changed);
        void drawIblDebugSection(RenderSettings& settings, bool& changed);
        void drawShadowSection(RenderSettings& settings, bool& changed);
        void drawPointShadowSection(RenderSettings& settings, bool& changed);
        void drawContactShadowSection(RenderSettings& settings, bool& changed);
    };
} // namespace NexAur
