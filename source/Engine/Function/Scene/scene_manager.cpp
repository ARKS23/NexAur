#include "pch.h"
#include "scene_manager.h"
#include "scene_factory.h"
#include "scene_v2.h"

namespace NexAur {
    void SceneManager::init() {
        setActiveScene(SceneFactory::createDefaultScene());
    }

    void SceneManager::shutdown() {
        m_active_scene.reset();
        m_next_scene.reset();
    }

    void SceneManager::tick(float delta_time) {
        // 处理当前场景tick
        if (m_active_scene) {
            m_active_scene->tick(delta_time);
        }

        // 保留 pending 检查，方便后续重新引入异步/事件驱动切换。
        if (m_is_transitioning) {
            processSceneChange();
        }
    }

    std::shared_ptr<SceneV2> SceneManager::createScene(const std::string& name) {
        auto new_scene = std::make_shared<SceneV2>();
        // 普通新场景保持空白；需要模板内容时由 SceneFactory 显式创建。
        (void)name;

        setActiveScene(new_scene);
        return new_scene;
    }

    void SceneManager::setActiveScene(std::shared_ptr<SceneV2> new_scene) {
        m_next_scene = new_scene;
        m_is_transitioning = true;
        processSceneChange();
    }

    void SceneManager::processSceneChange() {
        if (!m_is_transitioning) return;

        // TODO: 派发场景卸载/加载事件

        // 切换场景
        m_active_scene = m_next_scene;

        m_next_scene = nullptr;
        m_is_transitioning = false;
    }
} // namespace NexAur
