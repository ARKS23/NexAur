#pragma once

#include <string>

#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Core/Time/TimeStep.h"
#include "Function/Scene/entity.h"

namespace NexAur {
    // EditorModule 对 Engine 暴露的顶层控制接口。
    class NEXAUR_API EditorService {
    public:
        virtual ~EditorService() = default;

        virtual void setEnabled(bool enabled) = 0;
        virtual bool isEnabled() const = 0;
        virtual void update(TimeStep delta_time) = 0;
        virtual void renderUI() = 0;
        virtual void onEvent(Event& event) = 0;
    };

    // 编辑器选择状态的唯一写入口，避免面板之间直接互相改状态。
    class NEXAUR_API SelectionService {
    public:
        virtual ~SelectionService() = default;

        virtual Entity getSelectedEntity() const = 0;
        virtual void setSelectedEntity(Entity entity, const std::string& source) = 0;
        virtual void clearSelection() = 0;
        virtual const std::string& getSelectionSource() const = 0;
    };

    // UI 捕获策略需要知道鼠标是否在视口上，因此把视口状态抽成服务。
    class NEXAUR_API ViewportService {
    public:
        virtual ~ViewportService() = default;

        virtual bool isViewportFocused() const = 0;
        virtual bool isViewportHovered() const = 0;
    };
} // namespace NexAur
