# NexAur架构

## 架构专题

- [模块系统架构说明](ModuleSystem/ModuleSystem.md)：说明 Phase1 已落地的模块系统、服务边界、帧循环、事件路由，以及如何新增模块。

## 初步架构图
架构图待补充。

## 启动调用链

1. `main` 创建 `Engine`。
2. `Engine::startEngine()` 调用 `RunTimeGlobalContext::startSystems()`。
3. `RunTimeGlobalContext` 创建 `ModuleManager`，注册内置模块工厂。
4. `ModuleManager` 根据模块依赖初始化 Core、FileSystem、Platform、Resource、RenderContext、Renderer、Runtime、UI、Editor。
5. `Engine` 绑定 `WindowService` 事件回调并进入主循环。
6. 结束时 `ModuleManager` 按初始化顺序的反向关闭模块。

`RunTimeGlobalContext` 现在主要是组合根和旧代码兼容桥；深层系统应优先使用 Service 或显式参数。

### TickOneFrame

当前一帧的主顺序：

1. `calculateFPS()`。
2. `ModuleManager::tickModules()`：当前主要由 `PlatformModule` 刷新 `InputState`。
3. `Engine::logicalTick()`：active scene 更新并提取 `RenderDataPacket`。
4. `ModuleManager::postUpdateModules()`：EditorCamera 等编辑器逻辑覆盖 viewport 相机数据。
5. `UIService::beginFrame()`。
6. `ModuleManager::renderUIModules()`：Editor panel 提交 ImGui。
7. `RenderContext::swapBuffers()`：逻辑写入数据切换为 Renderer 读取数据。
8. `RendererService::render()`。
9. `UIService::endFrameAndRender()`。
10. `WindowService::present()` / `pollEvents()`。
