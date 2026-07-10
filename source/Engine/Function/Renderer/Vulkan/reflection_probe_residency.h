#pragma once

#include <cstdint>
#include <span>

namespace NexAur {
    struct ReflectionProbeResidencyCandidate {
        int entity_id = -1;
        uint64_t last_used_frame = 0;
        uint64_t generation = 0;
        bool resident = false;
        bool pinned = false;
        bool pending = false;
    };

    inline int selectReflectionProbeEvictionCandidate(
        std::span<const ReflectionProbeResidencyCandidate> candidates,
        int protected_entity_id) {
        const ReflectionProbeResidencyCandidate* selected = nullptr;
        for (const ReflectionProbeResidencyCandidate& candidate : candidates) {
            if (!candidate.resident ||
                candidate.pinned ||
                candidate.pending ||
                candidate.entity_id == protected_entity_id) {
                continue;
            }

            const bool older =
                !selected ||
                candidate.last_used_frame < selected->last_used_frame ||
                (candidate.last_used_frame == selected->last_used_frame &&
                 candidate.generation < selected->generation) ||
                (candidate.last_used_frame == selected->last_used_frame &&
                 candidate.generation == selected->generation &&
                 candidate.entity_id < selected->entity_id);
            if (older) {
                selected = &candidate;
            }
        }

        return selected ? selected->entity_id : -1;
    }
} // namespace NexAur
