#pragma once
#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Core/Time/TimeStep.h"
#include "Function/Scene/entity.h"
#include "Editor/editor_context.h"

#include <memory>
#include <vector>

namespace NexAur {
    class SceneV2;
    class EditorPanel;
} // namespace NexAur


namespace NexAur {
    class NEXAUR_API EditorLayer {
    public:
        EditorLayer();
        ~EditorLayer();

        void onUpdate(TimeStep delta_time);

        void onUIRender();

        void onEvent(Event& event);

    private:
        void init();
        void shutdown();
        void beginDockSpace();
        void endDockSpace();
        void synPanelContext();
        void updateViewportCamera(TimeStep delta_time);
        void syncViewportCameraToRenderPacket() const;
        
    private:
        std::vector<std::shared_ptr<EditorPanel>> m_panels; // TODO: 编辑器控制面板列表, 后续OnUIRender更新
        bool m_show_properties_panel = true; // 测试变量

        std::shared_ptr<EditorContext> m_context;    // 编辑器上下文，各个面板需要共享的数据
    };
} // namespace NexAur
