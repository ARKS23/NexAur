#pragma once

#include "Core/Base.h"

namespace NexAur {
    enum class EditorToolOperation {
        Select,
        Translate,
        Rotate,
        Scale
    };

    enum class EditorToolSpace {
        Local,
        World
    };

    struct NEXAUR_API EditorToolState {
        EditorToolOperation operation = EditorToolOperation::Translate;
        EditorToolSpace space = EditorToolSpace::World;
        bool snap_enabled = false;
        bool gizmo_visible = true;
        float translate_snap = 0.5f;
        float rotate_snap = 15.0f;
        float scale_snap = 0.25f;
    };
} // namespace NexAur
