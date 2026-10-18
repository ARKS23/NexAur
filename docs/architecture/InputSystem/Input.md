# 输入系统

最后核对：2026-10-18。输入包含轮询状态和同步事件两条路径；事件分发见[事件系统](../EventSystem/EventSystem.md)。

## 状态与动作数据流

```text
GLFW key / mouse state
  -> PlatformModule::tick
  -> InputSystemGLFW::update / InputState
  -> InputService
  -> InputActionModule::tick / InputActionSystem
  -> InputActionService
  -> GameModule
```

InputSystemGLFW 每帧刷新已登记的键盘键、鼠标按钮和鼠标位置。InputActionSystem 对照上一帧状态计算 pressed/released，并生成一维/二维轴。Editor 工具仍可直接使用 InputService。

源码：[平台输入实现](../../../source/Engine/Function/Input/input_system_glfw.cpp)、[动作映射](../../../source/Engine/Function/Input/input_action_system.cpp)。

## 当前默认绑定

| 语义 | 输入 |
| --- | --- |
| Move | WASD 二维轴 |
| Jump | Space |
| Fire | 鼠标左键 |
| Pause | Escape |
| Interact | E |

InputActionService 提供 `isHeld`、`wasPressed`、`wasReleased`、`getAxis1D` 和 `getAxis2D`。动作有绑定并不代表所有玩法行为都已经实现。

## 使用方式

模块顶层获取服务，深层系统通过构造参数或函数参数接收依赖：

```cpp
auto input_actions = context.registry.getRequiredService<InputActionService>();
glm::vec2 move = input_actions->getAxis2D(DefaultInputActions::Move);
if (input_actions->wasPressed(DefaultInputActions::Jump)) {
    // 由玩法系统决定如何处理跳跃。
}
```

编辑器状态查询：

```cpp
const InputState& state = input_service->getState();
auto [x, y] = state.getMousePosition();
const bool forward = state.isKeyPressed(KeyCode::W);
```

## 与 UI 捕获的边界

UIModule 可以把同步输入事件标为 handled；EditorCamera 还有自身的 focus/hover 和 UI capture 检查。

当前 InputActionSystem 直接读取 InputService 快照，尚无统一的 Editor/Runtime action context 或 ImGui capture 过滤层。因此“事件被 UI 消费”不等于“轮询动作自动被屏蔽”。涉及玩法输入隔离时需分别检查这两条路径。

当前实现后端为 GLFW，仓库构建预设面向 Windows x64。可编辑 binding UI、action map 持久化和手柄映射仍属于后续扩展。验证入口为 `NexAur.InputActionSmoke`。
