#pragma once

#include "Core/Base.h"
#include "Editor/Panels/editor_panel.h"

#include <string>

namespace NexAur {
    struct RendererDebugSnapshot;

    class NEXAUR_API RendererDebugPanel : public EditorPanel {
    public:
        explicit RendererDebugPanel(const std::string& name = "Renderer Debug")
            : EditorPanel(name) {}
        ~RendererDebugPanel() override = default;

        void onUIRender() override;

    private:
        void drawRendererSection(const RendererDebugSnapshot& snapshot);
        void drawFrameSection(const RendererDebugSnapshot& snapshot);
        void drawViewSection(const RendererDebugSnapshot& snapshot);
        void drawTargetsSection(const RendererDebugSnapshot& snapshot);
        void drawEffectsSection(const RendererDebugSnapshot& snapshot);
        void drawResourcesSection(const RendererDebugSnapshot& snapshot);
    };
} // namespace NexAur
