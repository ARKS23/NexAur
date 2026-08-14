#pragma once

#include "Core/Base.h"
#include "Function/Renderer/renderer_service_types.h"

namespace NexAur {
    // Runtime reflection-probe capture contract used by editor bake workflows.
    class NEXAUR_API ReflectionProbeCaptureService {
    public:
        virtual ~ReflectionProbeCaptureService() = default;

        virtual bool requestReflectionProbeCapture(const ReflectionProbeCaptureRequest& request) = 0;
        virtual bool clearReflectionProbeCapture(int entity_id) = 0;
        virtual ReflectionProbeCaptureState getReflectionProbeCaptureState(int entity_id) const = 0;
        virtual ReflectionProbeCaptureQueueState getReflectionProbeCaptureQueueState() const = 0;
    };
} // namespace NexAur
