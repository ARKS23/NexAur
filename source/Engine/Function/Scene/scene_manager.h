#pragma once
#include <memory>
#include <string>
#include "Core/Base.h"
#include "Function/Scene/scene_service.h"

namespace NexAur {
    class SceneV2;

    // 场景管理器: 负责场景生命周期和切换
    class NEXAUR_API SceneManager : public SceneService {
    public:
        SceneManager() = default;
        ~SceneManager() = default;

        // 生命周期方法
        void init();
        void shutdown();

        // 引擎每帧调用: 处理当前场景更新和延迟场景切换
        void tick(float delta_time);

        // 获取当前运行的场景
        std::shared_ptr<SceneV2> getActiveScene() const override { return m_active_scene; }

        // 创建新的场景并切换
        std::shared_ptr<SceneV2> createScene(const std::string& name) override;

        // 切换场景
        void setActiveScene(std::shared_ptr<SceneV2> new_scene) override;

    private:
        // 处理场景切换逻辑辅助函数
        void processSceneChange();

    private:
        std::shared_ptr<SceneV2> m_active_scene;
        std::shared_ptr<SceneV2> m_next_scene;  // 缓存下个要切换的场景，分帧切换
        bool m_is_transitioning = false;
    };
} // namespace NexAur
