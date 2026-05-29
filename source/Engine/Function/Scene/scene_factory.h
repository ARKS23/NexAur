#pragma once

#include <memory>

#include "Core/Base.h"

namespace NexAur {
    class SceneV2;

    class NEXAUR_API SceneFactory {
    public:
        // 创建编辑器启动时可直接预览的默认场景；普通 Scene 构造保持轻量、无编辑器依赖。
        static std::shared_ptr<SceneV2> createDefaultScene();
    };
} // namespace NexAur
