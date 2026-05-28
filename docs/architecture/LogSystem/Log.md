# Log System
## 概述
日志模块用于在开发和调试阶段输出结构化的运行时信息。本模块基于日志库**spdlog**构建，目前是单线程程序，未使用异步日志。

## 设计
- 日志系统采用静态**单例模式**封装，确保在应用程序的任何位置都能直接访问，无需传递上下文。
- 使用宏进行静态函数封装封装: 如`NX_CORE_INFO`比直接用`NexAur::Log::GetLogger()->info`更方便.


## API与用法
- 必须在使用前进行**初始化**，否则直接使用会崩溃
```C++
NexAur::Log::Init();
```

| 级别   |         API             | 说明                    |
|--------|-------------------------|--------------------------|
| Trace  | NX_CORE_TRACE(...)      | 极其琐碎的追踪信息       |
| Info   | NX_CORE_INFO(...)       | 常规状态信息             |
| Warn   | NX_CORE_WARN(...)       | 警告，程序可继续运行     |
| Error  | NX_CORE_ERROR(...)      | 错误，可能导致功能异常   |
| Fatal  | NX_CORE_FATAL(...)      | 致命错误，程序即将崩溃   |


## 依赖
- **spdlog**： external/spdlog

## 使用示例
```C++
// 1. 基础字符串
NX_CORE_INFO("Engine Initialized successfully.");

// 2. 带参数格式化 
int width = 1920;
int height = 1080;
NX_CORE_WARN("Window resized to: {0} x {1}", width, height);

// 3. 打印自定义对象 (需要重载 operator<<)
glm::vec3 pos = {1.0f, 2.0f, 3.0f};
NX_CORE_INFO("Player Position: {0}, {1}, {2}", pos.x, pos.y, pos.z);
```

## 未来计划
- 编辑器中实现一个Log Console面板，实时显示日志
- 随着项目多线程化后修改为异步日志
