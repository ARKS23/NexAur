#pragma once
#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Core/Time/TimeStep.h"
#include "Editor/editor_context.h"

#include <memory>
#include <string>

namespace NexAur {
    class SceneV2;
} // namespace NexAur


namespace NexAur {
    class NEXAUR_API EditorPanel {
    public:
        EditorPanel(const std::string& name = "Panel") : m_name(name) {}
        virtual ~EditorPanel() = default;

        virtual void onUpdate(TimeStep delta_time) {};
        virtual void onUIRender() = 0;
        virtual void onEvent(Event& event) {};

        void syncPanelContext(const std::shared_ptr<EditorContext>& context) { m_context = context; }

        const std::string& getName() const { return m_name; }

        void open() { m_is_open = true; }
        void close() { m_is_open = false; }
        bool isOpen() const { return m_is_open; }
        bool& getOpenFlag () { return m_is_open; } // 直接暴露引用，方便ImGui绑定

    protected:
        std::string m_name;
        std::shared_ptr<EditorContext> m_context = nullptr;
        bool m_is_open = true;
    };
} // namespace NexAur
