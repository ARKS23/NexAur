# NexAur
本项目用于学习和研究游戏引擎开发, NexAur是长期开发和维护的个人小型游戏引擎项目。(有时间就开发 = =。。)

## 代码风格
- [代码风格文档](./docs/CodingGuideLines/CodingGuideLines.md)

## 进度
###  日志系统：v1版本全局日志系统开发完成(后续再升级)
![](./docs/architecture/images/log_1.png)

###  输入系统: v1版本轮询模式事件系统设计完成
- [输入系统文档](./docs/architecture/InputSystem/Input.md)

###  渲染模块: OpenGL后端实现的分支开发基本完成(后续换vulkan实现, 目前渲染模块很乱)
![](./docs/architecture/images/scenev2.png)

### 事件系统: v1版本总线事件派发结构编写完成
- [事件系统文档](./docs/architecture/EventSystem/EventSystem.md)

###  编辑器：开发中
- 视口面板，场景层级面板，实体属性面板正在开发中
![](./docs/architecture/images/editor_1.png)

## 技术栈
![C++](https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=flat-square&logo=c%2B%2B)
![CMake](https://img.shields.io/badge/Build-CMake-064F8C?style=flat-square&logo=cmake)

- **EnTT**: 性能的实体组件系统，实现数据驱动开发与内存连续性。
- **OpenGL4.5+**: 暂时使用OpenGL
- **ImGui docking**: 即时模式 GUI 库，用于构建引擎编辑器、调试面板与工具链
- **glm**: 数学库
- **spdlog**: 快速日志系统
- **Assimp**: 模型资产导入
- **stb_image**: 轻量级图像解码库，支持 PNG, JPG, TGA 等多种纹理格式。
- **glfw**: 轻量级的窗口管理与 OpenGL 上下文创建，处理键盘/鼠标输入。



