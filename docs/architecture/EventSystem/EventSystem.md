# 事件系统

最后核对：2026-10-18。当前为同步立即分发和责任链模型，没有通用异步事件总线。

## 类型与分发器

[Event](../../../source/Engine/Core/Events/event.h)提供：

- `getEventType()`：具体类型。
- `getCategoryFlags()` / `isInCategory(...)`：分类。
- `getName()` / `toString()`：调试名称。
- `handled`：是否停止向后续模块传播。

EventDispatcher::dispatch 按类型调用 handler，并把 handler 的返回值写入 handled；dispatch 自身返回的是类型是否匹配，二者不可混淆。

## 调用链

```text
WindowSystem / GLFW callback
  -> WindowService event callback
  -> Engine::onEvent
       WindowClose / WindowResize
  -> ModuleManager::dispatchEvent
       按 ModuleStage 降序分发
       handled 后停止
```

Engine 消费 WindowClose 并停止运行；WindowResize 当前继续传递给模块。UI 优先于 Game、Editor、Renderer 和 Runtime 接收事件；同 stage 内按反向初始化顺序处理。

源码：[Engine 入口](../../../source/Engine/engine.cpp)、[ModuleManager](../../../source/Engine/Core/Module/module_manager.cpp)。

## UI 捕获规则

[UIModule](../../../source/Engine/Function/UI/ui_module.cpp)先通知 UISystem，再按以下规则拦截：

| 情况 | 行为 |
| --- | --- |
| ImGui wantsTextInput | 拦截键盘事件 |
| ImGui wantsCaptureKeyboard，且视口既未聚焦也未悬停 | 拦截键盘事件 |
| ImGui wantsCaptureMouse，且鼠标不在视口上 | 拦截鼠标事件 |
| 鼠标悬停视口 | 允许后续模块处理 picking、滚轮等交互 |

这些规则作用于事件链。InputAction 读取的轮询状态不会自动清空，详见[输入系统](../InputSystem/Input.md)。

## 当前范围

窗口、键盘、鼠标事件通过该链路派发。Trigger overlap 当前由 Gameplay/Physics 系统直接消费，不应据此推断已有异步物理事件队列或多播委托系统。
