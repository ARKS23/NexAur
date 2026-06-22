#pragma once

#include <string>

#include "Core/Base.h"
#include "Core/Events/event.h"
#include "Core/Time/TimeStep.h"
#include "Function/Scene/entity.h"

namespace NexAur {
    class NEXAUR_API EditorService {
    public:
        virtual ~EditorService() = default;

        virtual void setEnabled(bool enabled) = 0;
        virtual bool isEnabled() const = 0;
        virtual void update(TimeStep delta_time) = 0;
        virtual void renderUI() = 0;
        virtual void onEvent(Event& event) = 0;
    };

    class NEXAUR_API SelectionService {
    public:
        virtual ~SelectionService() = default;

        virtual Entity getSelectedEntity() const = 0;
        virtual void setSelectedEntity(Entity entity, const std::string& source) = 0;
        virtual void clearSelection() = 0;
        virtual const std::string& getSelectionSource() const = 0;
    };

    class NEXAUR_API ViewportService {
    public:
        virtual ~ViewportService() = default;

        virtual bool isViewportFocused() const = 0;
        virtual bool isViewportHovered() const = 0;
    };
} // namespace NexAur
