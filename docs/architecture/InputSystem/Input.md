# 输入系统
## 1.概述
### 1.1 模块定位
Input System 是 NexAur 引擎中负责 **输入采集**、**状态管理**与**语义映射** 的基础模块。

### 1.2 设计目标
- 平台解耦: 对外统一输入接口（目前只有Windows-GLFW）
- 语义化输入: 避免代码直接依赖平台keyCode
- 状态与事件分离: 支持**状态查询**也支持**事件驱动**

### 1.3 非目标
- 不负责事件的分发与订阅管理（由事件系统负责）

## 2. 设计
### 2.1 架构设计
```
[ OS / 硬件 ]
        ↓
[ PlatformModule / WindowSystem ]
        ↓
[ InputSystemGLFW -> InputState ]
        ↓
[ InputService / Event System ]
        ↓
[ Gameplay / UI / Editor ]
```

### 2.2 输入模型
#### 1.状态查询
用于持续行为判断
- 是否按下某个按键
- 当前鼠标位置
- 某个轴的当前值
#### 2 事件通知
用于一次性变化响应
- 当按键按下/抬起
- 鼠标滚轮滚动
- 窗口焦点变化

### 2.3 输入状态管理
输入系统维护下列状态数据,所有状态数据均以 **“当前帧快照”** 的形式存在，并在每帧开始或结束时更新。
- 键盘按键状态
- 鼠标按键状态
- 鼠标位置

### 2.4 输入语义映射
避免业务代码直接依赖平台keyCode
```aiignore
平台输入（如 GLFW_KEY_W）
        ↓
引擎语义输入（如 Key::MoveForward）
```

## 3. API与用法
### 3. 1 状态查询接口
用于在任意系统中查询当前输入状态：
- 查询按键是否按下
- 查询鼠标位置
- 查询轴值

这些接口**不依赖事件系统**，可直接调用。

## 4. 使用示例

新代码不要直接访问 `g_runtime_global_context.m_input_system`。模块顶层从 `ModuleRegistry` 获取 `InputService`，深层对象通过构造参数或 setter 接收它。

```C++
std::shared_ptr<InputService> input_service =
    context.registry.getService<InputService>();

// 深层对象只保存 InputService，不知道 GLFW 或全局上下文。
const InputState& input_state = input_service->getState();
auto [x, y] = input_state.getMousePosition();

// 判断是否按下W
if (input_state.isKeyPressed(KeyCode::W)) {
    // ...
}
```

## 5. 未来计划
- 在 `InputState` 之上增加 InputAction / ActionMap。
- 目前只支持 Windows/GLFW，后续可扩展跨平台实现。
