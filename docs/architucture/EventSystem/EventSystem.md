# 事件系统
## 概述
NexAur中的事件系统目前采用的是立即模式和责任链模式设计，参考**Hazel**引擎的设计，其主要目的是解耦底层的窗口回调与上层的应用逻辑，并提供一种可靠的机制来管理事件的拦截与消耗。

## 设计
### 1. 核心类结构：
所有事件均继承自**Event基类**，基类的方法如下
- `EventType getEventType()`: 获取事件类型（如 KeyPressed, MouseMoved）。

- `int getCategoryFlags()`: 获取分类（如 Input, Keyboard, MouseButton）。

- `bool handled`: 核心标志位，指示事件是否已被消耗。

- `string toString()`: 用于 Debug 日志输出。

- `string getName()`: 获取事件名字用于调试

- `bool isInCategory(EventCategory category)`: 判断当前事件是否舒徐某个分类

### 2. 事件分发器
- 事件分发器用于简化事件类型判断和回调函数执行
- 它负责把Event基类安全地转换为具体地子类，并调用对应的处理函数

### 3. 数据流向
编码完成后补充

## 用法
待编码完成后补充


## 未来计划
1. 未来加入音频处理和物理碰撞时补充延迟事件队列。
2. 加入类似UE的机制，在游戏玩法层引入多播委托机制。