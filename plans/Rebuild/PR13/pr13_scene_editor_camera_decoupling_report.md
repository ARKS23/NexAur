# PR13 Scene / EditorCamera 解耦执行报告

日期：2026-05-29

## 本次目标

PR13 的目标是把编辑器观察相机从运行时 Scene 中拆出来：Scene 只保存可序列化、可运行的 CameraComponent，EditorCamera 则成为编辑器视口自己的交互状态。这样后续做场景序列化、运行时相机控制器、Game View / Scene View 分离时，不会被当前“编辑器相机每帧覆盖场景相机”的临时代码卡住。

## 已完成内容

### 1. SceneV2 不再拥有 EditorCamera

已移除：

```cpp
std::shared_ptr<EditorCamera> m_editor_camera;
std::shared_ptr<EditorCamera> getEditorCamera();
```

`SceneV2::tick` 现在不再调用 `EditorCamera::onUpdate`，也不再把编辑器相机矩阵写回 `CameraComponent`。这保证了场景里的 CameraComponent 可以独立作为游戏相机数据存在。

### 2. 新增 SceneFactory

新增文件：

```text
source/Engine/Function/Scene/scene_factory.h
source/Engine/Function/Scene/scene_factory.cpp
```

`SceneFactory::createDefaultScene()` 负责创建默认场景内容：

- `MainCamera`
- `Environment`
- `DirectionalLight`
- `PointLight`

`SceneV2` 构造函数因此保持轻量，不再混入默认资源、默认光照和编辑器相机创建逻辑。

### 3. SceneManager 使用默认场景工厂

`SceneManager::init()` 改为：

```cpp
setActiveScene(SceneFactory::createDefaultScene());
processSceneChange();
```

`SceneManager::createScene()` 现在创建空白场景。需要模板内容时，应显式走 `SceneFactory`，避免所有 Scene 构造都带上默认测试对象。

### 4. EditorContext 持有视口相机

`EditorContext` 新增：

```cpp
std::shared_ptr<EditorCamera> viewport_camera;
bool viewport_focused = false;
bool viewport_hovered = false;
```

这个相机只属于编辑器视口，不属于运行时 Scene。`ViewportPanel` 会同步 ImGui 视口聚焦/悬停状态，并用该相机设置 gizmo 矩阵与视口尺寸。

### 5. EditorLayer 同步视口相机到渲染包

编辑器帧更新时：

```text
SceneV2::extractSceneData()
  -> 写入场景 CameraComponent
EditorLayer::updateViewportCamera()
  -> 更新 EditorCamera
  -> 覆盖 RenderDataPacket.camera_data
EditorLayer::onUIRender()
  -> ViewportPanel 同步视口尺寸
  -> 交换渲染包前再次同步相机矩阵
RenderContext::swapBuffers()
  -> RendererSystem 使用编辑器视口相机渲染 Scene View
```

重点是只覆盖渲染包，不回写 Scene 组件。这样 Editor viewport 仍能移动观察，但游戏相机数据不会被编辑器相机污染。

## 当前架构状态

现在相机职责分为两条链路：

```text
运行时相机：
Scene CameraComponent
  -> SceneV2::extractSceneData
  -> RenderDataPacket.camera_data

编辑器视口相机：
EditorContext::viewport_camera
  -> EditorLayer::syncViewportCameraToRenderPacket
  -> RenderDataPacket.camera_data
```

在 Editor 模式下，Scene View 使用编辑器相机观察；在非 Editor 模式或未来 Game View 中，可以直接使用 Scene 的 CameraComponent。

## 后续建议

- 下一步可以给 `CameraComponent` 增加更明确的投影参数，例如 FOV、Near/Far、Aspect，并由组件自身或 CameraSystem 生成矩阵。
- 后续可以拆出 `EditorViewportController`，把视口相机输入、聚焦状态和 render packet override 集中管理，避免 `EditorLayer` 继续变胖。
- Game View / Scene View 分离时，建议保留两份相机选择策略：Game View 使用 Scene active camera，Scene View 使用 EditorContext viewport camera。

## 验证结果

已通过：

```text
cmake -S . -B build
cmake --build build --config Debug --target NexAurEngine
cmake --build build --config Debug --target Sandbox
```

构建仍有项目既有的 C4251 DLL 接口警告，没有新增构建错误。
