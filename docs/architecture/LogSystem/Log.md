# 日志系统

最后核对：2026-10-18。实现：[log_system.h](../../../source/Engine/Core/Log/log_system.h)、[log_system.cpp](../../../source/Engine/Core/Log/log_system.cpp)。

## 生命周期与存储

CoreModule 调用 `LogSystem::init()` 初始化名为 NEXAUR 的 spdlog logger。独立工具或测试若不走模块启动，需要自行初始化后再使用宏。

日志当前同步写入线程安全的 console sink 和 RecentLogSink。后者保留最近 512 条记录，使用互斥锁保护缓存；这不等于异步日志队列。

依赖通过 vcpkg 的 spdlog 包提供，不使用仓库内旧 external/spdlog 构建。

## API

```cpp
NexAur::LogSystem::init();
NX_CORE_INFO("Engine initialized.");
NX_CORE_WARN("Window resized to: {} x {}", width, height);
```

| 宏 | spdlog 级别 |
| --- | --- |
| `NX_CORE_TRACE` | trace |
| `NX_CORE_INFO` | info |
| `NX_CORE_WARN` | warn |
| `NX_CORE_ERROR` | error |
| `NX_CORE_FATAL` | critical |

NX_CORE_FATAL 只记录 critical 日志，本身不会终止进程。

## 编辑器 Console

[ConsolePanel](../../../source/Engine/Editor/Panels/console_panel.cpp)读取 `LogSystem::getRecentMessages()`，可通过 `clearRecentMessages()` 清空缓存。Console 已实现，不再属于未来 TODO。
