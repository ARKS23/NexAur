# NexAur Editor UI Polish Plan

日期：2026-06-28

## 1. 文档定位

本文档记录 NexAur Editor UI 的后续整理和质感提升路线。

目标不是把编辑器做成复杂的商业软件外壳，而是在现有 ImGui / Docking / Vulkan viewport 基础上，逐步做出一种“简洁、紧凑、专业、有质感”的引擎编辑器体验。

参考方向：

- 中央 viewport 是第一视觉主体。
- 左侧提供 scene / project 浏览。
- 右侧提供 inspector / renderer settings。
- 底部提供 console / asset / timeline 等辅助面板。
- 顶部菜单和工具栏保持克制、清晰、图标化。
- 整体是深色、低噪声、信息密度高的工具界面。

本文档用于跟踪 Editor UI polish 阶段的任务拆分、边界和验收标准。

## 2. 当前基础

当前 NexAur Editor 已经具备不错的起点：

- Dear ImGui 已启用 docking。
- `EditorLayer` 已经创建主 DockSpace。
- `ViewportPanel` 已支持 Vulkan ImGui viewport texture。
- `ViewportPanel` 已支持 Scene / Game 视图切换。
- `ViewportPanel` 已接入 ImGuizmo transform gizmo。
- `SceneHierarchyPanel` 已提供场景实体列表和基础右键删除。
- `PropertiesPanel` 已能编辑常用组件。
- `RendererDebugPanel` 已能显示 renderer snapshot，并控制 debug visualization。
- Panel 通过 `EditorContext` 和 service 交互，没有直接访问 Vulkan backend。

这说明：当前不需要换 UI 技术栈。Dear ImGui 足够支撑这一阶段的编辑器体验。

需要补的是一层更完整的 Editor UI framework：

```text
Editor Shell
  -> Style / Icons / Fonts
  -> Dock Layout
  -> Menu / Toolbar / Status Bar
  -> Command System
  -> Property Drawers
  -> Asset / Console Panels
  -> Preferences / Render Settings
```

## 3. 风格目标

### 3.1 简洁但有质感

目标风格不是花哨，而是“安静、清楚、工具感强”。

推荐特征：

- 深色中性背景，避免大面积纯黑或高饱和颜色。
- 面板边界清晰但不厚重。
- 控件高度统一，文本和数值对齐。
- 图标按钮用于工具命令，文字按钮只用于明确动作。
- 常用操作靠近 viewport 顶部和工具栏。
- Inspector 使用紧凑的 label / value 布局。
- 选中态、hover 态、disabled 态有明确层级。
- 信息密度比 landing page 高，不做装饰性大卡片。

### 3.2 Viewport 优先

编辑器打开后的第一感受应该是“我正在编辑一个场景”，不是“我在看一堆面板”。

推荐默认布局：

```text
Top:
  Menu Bar
  Main Toolbar

Left:
  Scene Hierarchy / Explorer

Center:
  Scene Viewport / Game Viewport

Right:
  Inspector
  Renderer Effects / Preferences

Bottom:
  Project
  Console
  Timeline / Profiler placeholder

Bottom status:
  Scene name / mode / FPS / frame time
```

### 3.3 控件语言统一

后续 UI 控件尽量遵守：

- 工具：使用 icon button + tooltip。
- 模式切换：使用 segmented control 或 small tab。
- 布尔项：使用 checkbox / toggle。
- 数值：使用 drag / slider / input。
- 枚举：使用 combo。
- 颜色：使用 color edit。
- 资源引用：使用 asset field，后续支持 drag-drop。

## 4. 架构原则

### 4.1 继续使用 Dear ImGui

ImGui 对 NexAur 当前阶段是正确选择：

- 和已有 Vulkan viewport / ImGuizmo 集成成本低。
- 适合工具型、高迭代的编辑器 UI。
- Docking、菜单、表格、树、属性编辑都够用。
- 复杂控件可以后续按需引入 ImPlot、imgui-node-editor 等扩展。

暂时不建议切换 Qt / Web UI / Slate-like 自研框架。

### 4.2 Editor UI 不直接依赖 Vulkan backend

继续保持：

```text
Editor Panel
  -> EditorContext
  -> Service interface / backend-neutral data
```

禁止方向：

```text
Editor Panel -> Renderer/Vulkan/*
Editor Panel -> VkImage / VkDescriptorSet / Vulkan resource
```

渲染效果参数通过 backend-neutral `RenderSettings` 或 `RenderContext` options 写入；渲染状态通过 `RendererDebugSnapshot` 读取。

### 4.3 Theme / Icon / Font 集中管理

不要在每个 panel 里零散调 `ImGui::PushStyleColor()`。

建议新增一个集中入口：

```text
source/Engine/Editor/Style/
  editor_style.h
  editor_style.cpp
  editor_icons.h
```

职责：

- 设置 ImGui color palette。
- 设置 spacing / rounding / border / padding。
- 设置 ImGuizmo style。
- 后续加载 icon font。
- 提供少量通用 helper，例如 icon button、section header、property row。

### 4.4 Panel 只做 UI，不做业务调度

Panel 应该负责：

- 展示数据。
- 收集用户输入。
- 调用 service / command。

Panel 不应该负责：

- 保存场景文件的具体流程。
- 直接创建 renderer resource。
- 直接访问 global context。
- 执行复杂 gameplay / runtime 逻辑。

复杂命令应进入 `EditorCommand`。

### 4.5 Command System 先轻量

菜单、工具栏、快捷键、右键菜单不应该各自复制逻辑。

建议第一版：

```text
EditorCommand
EditorCommandRegistry
EditorShortcutMap
```

每个命令至少包含：

- command id。
- display name。
- optional icon。
- enabled predicate。
- execute callback。
- shortcut。

Undo / Redo 可以后续接入，不挤进第一版。

## 5. 推荐执行顺序

建议从 PR-E1 开始：

```text
PR-E1 Editor Style Foundation
PR-E2 Dockspace Layout + Shell Polish
PR-E3 Toolbar + Command System v1
PR-E4 Inspector Property Drawer v1
PR-E5 Scene Hierarchy / Explorer Polish
PR-E6 Project / Console Panels v1
PR-E7 Viewport Overlay + Gizmo Polish
PR-E8 Preferences / Render Settings Panel
PR-E9 Layout / Shortcut Persistence
```

如果想最快获得“像一个成熟编辑器”的观感，优先：

```text
E1 Style
E2 Dockspace Shell
E3 Toolbar
E4 Inspector
```

## 6. PR-E1：Editor Style Foundation

执行状态：已完成。

目标：

- 建立 NexAur Editor 统一主题。
- 统一 ImGui / ImGuizmo 的基础视觉风格。
- 为后续面板改造提供稳定 UI helper。
- 将零散 style 逻辑集中到 Editor 层，避免后续 panel 各自堆样式。
- 为 E2 Dockspace、E3 Toolbar、E4 Inspector 的视觉一致性打底。

当前观察：

- `EditorLayer::beginDockSpace()` 内有少量 dock root 必需的 `PushStyleVar()`，属于布局型 style，可以保留。
- `ViewportPanel::beginViewportWindow()` 使用 `WindowPadding = 0`，属于 viewport image 特殊需求，可以保留。
- `ViewportPanel::drawViewportModeToolbar()` 直接设置 `FramePadding`，后续可迁移到统一 toolbar / segmented control helper。
- `ViewportPanel::setGizmoStyle()` 直接设置 ImGuizmo style，建议在 E1 中迁到 `EditorStyle`。
- 当前没有集中 theme / icon / helper 入口，后续继续扩 panel 会越来越散。

设计原则：

- E1 只做视觉基础设施，不改变编辑器布局和业务流程。
- Editor Style 属于 Editor 层，不放进 Vulkan ImGui backend。
- `VulkanImGuiRenderer` 继续只负责 ImGui 渲染接入，不负责编辑器主题。
- 第一版保持低风险：先应用统一 theme，再小范围迁移明显重复的 style。
- 不为了“好看”牺牲工具信息密度，整体保持紧凑、克制、深色中性。

建议新增文件：

```text
source/Engine/Editor/Style/
  editor_style.h
  editor_style.cpp
  editor_icons.h
```

建议职责：

- `editor_style.h/.cpp`
  - `EditorStyle::applyTheme()`：设置 ImGui 全局颜色和尺寸。
  - `EditorStyle::applyGizmoStyle()`：统一 ImGuizmo 线宽、颜色和交互厚度。
  - `EditorStyle::drawSectionHeader()`：后续 Inspector / Debug 面板复用。
  - `EditorStyle::drawSubtleText()`：弱提示文字。
  - `EditorStyle::drawHelpTooltip()`：统一 tooltip 行为。
  - `EditorStyle::iconButton()`：先保留文本 fallback，后续接 icon font。
- `editor_icons.h`
  - 第一版可以只定义轻量 icon 常量或占位文本。
  - 后续接 FontAwesome / Codicon 时只改这里。

建议调用方式：

```text
EditorLayer
  -> ensureEditorStyleApplied()
  -> EditorStyle::applyTheme()

ViewportPanel
  -> EditorStyle::applyGizmoStyle()
```

说明：

- `EditorStyle::applyTheme()` 只需要在 ImGui context 创建后调用一次。
- 如果初始化时机不确定，可以在 `EditorLayer::onUIRender()` 开头用 bool guard 兜底调用一次。
- 不建议在每帧重复写完整 theme。
- 不建议从 `VulkanImGuiRenderer` 调用 `EditorStyle`，避免 Renderer backend 反向依赖 Editor。

主题方向：

```text
Background: 深灰，不使用纯黑
Panel: 比背景略亮，边界清楚
Header / Tab: 低饱和蓝灰
Accent: 少量冷色高亮，用于 selected / active
Text: 高对比但不过曝
Disabled: 明确变暗
Border: 细、低噪声
Rounding: 2 - 4
Frame padding: 紧凑但不拥挤
```

建议默认视觉参数：

```text
WindowRounding: 0 - 2
ChildRounding: 2
FrameRounding: 3
PopupRounding: 3
TabRounding: 3
ScrollbarRounding: 3
GrabRounding: 2
WindowBorderSize: 1
FrameBorderSize: 0
ChildBorderSize: 1
WindowPadding: 8, 8
FramePadding: 6, 3
ItemSpacing: 6, 4
ItemInnerSpacing: 4, 4
IndentSpacing: 14
```

建议工作：

1. 新增 `EditorStyle`。
   - 集中设置 ImGui color palette。
   - 集中设置 rounding、border、padding、spacing。
   - 集中设置 window / frame / tab / header / separator 等颜色。
   - 提供 `applyTheme()`，由 `EditorLayer` 在 Editor UI 渲染前应用。
2. 新增 style helper。
   - small icon button。
   - section header。
   - property label / value row。
   - disabled text / subtle text。
   - tooltip helper。
   - 先提供 API 和少量使用点，不强制一次性改完所有 panel。
3. ImGuizmo style 收口。
   - 把 `ViewportPanel::setGizmoStyle()` 中的样式迁出或统一调用。
   - 后续 gizmo 颜色、线宽统一由 `EditorStyle::applyGizmoStyle()` 控制。
4. 字体策略。
   - 第一版可继续用默认字体。
   - 后续接入 icon font，例如 FontAwesome / Codicon。
   - E1 可预留 `editor_icons.h`，但不强制引入第三方字体资源。
5. CMake 接入。
   - 将新增 Editor Style 文件加入 `source/Engine/CMakeLists.txt`。
   - 保持 Editor 模块内部依赖，不影响 Runtime / Renderer backend。
6. 最小迁移点。
   - `ViewportPanel::setGizmoStyle()` 迁移到 `EditorStyle`。
   - `ViewportPanel::drawViewportModeToolbar()` 可保留现状，或先使用 helper 做轻量 segmented button。
   - `RendererDebugPanel` / `PropertiesPanel` 暂不大改，避免 E1 膨胀成 E4。

建议拆分步骤：

1. PR-E1-A：Style 文件和 theme 接入。
   - 新增 `EditorStyle` 文件。
   - 在 `EditorLayer` 中调用一次 `EditorStyle::applyTheme()`。
   - 构建通过，打开编辑器能看到全局 theme 生效。
2. PR-E1-B：ImGuizmo style 收口。
   - 删除或弱化 `ViewportPanel::setGizmoStyle()` 的直接样式定义。
   - 改为调用 `EditorStyle::applyGizmoStyle()`。
3. PR-E1-C：基础 helper。
   - 增加 tooltip / subtle text / section header / icon button helper。
   - 在少量低风险位置试用。
4. PR-E1-D：文档和验收。
   - 更新执行日志。
   - 记录保留的局部 style 例外，例如 viewport `WindowPadding = 0`。

暂时不做：

- 不重写所有 panel。
- 不接入复杂 skin / theme editor。
- 不做运行时 UI framework。
- 不做完整 toolbar。
- 不做 command system。
- 不做 dockspace 默认布局重排。
- 不做 PropertiesPanel / Inspector 大重构。
- 不做完整 icon font 集成。
- 不把 Editor theme 放进 Vulkan backend。

验收：

- Editor 全局风格统一。
- Panel 不再到处临时设置重复 style。
- ImGuizmo 风格与 Editor 主题协调。
- `EditorStyle` 是 Editor 层统一入口，Renderer/Vulkan backend 不反向依赖 Editor。
- Viewport 特殊 padding 等局部 style 有明确理由，不被误清理。
- 默认布局和现有功能不倒退。
- 构建和 Sandbox smoke 通过。

实现记录：

- 新增 `source/Engine/Editor/Style/editor_style.h/.cpp`，集中设置 ImGui 深色主题、spacing、rounding、border 和常用 helper。
- 新增 `source/Engine/Editor/Style/editor_icons.h`，先提供 ASCII 文本 fallback，后续接入 icon font 时只收敛到这一层。
- `EditorLayer` 在 ImGui context 可用后通过 `ensureEditorStyleApplied()` 应用一次全局主题和 ImGuizmo 风格，不让 Vulkan ImGui backend 反向依赖 Editor。
- `ViewportPanel` 的 Scene/Game 切换改用 `EditorStyle::segmentedButton()`，并移除本地 `setGizmoStyle()`，统一调用 `EditorStyle::applyGizmoStyle()`。
- 保留 `EditorLayer::beginDockSpace()` 的 dock root style 和 `ViewportPanel::beginViewportWindow()` 的零 padding，因为它们属于布局/viewport 输出的局部必要样式。
- 验证：`cmake --build --preset msvc-vcpkg-debug`、`ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure`、Sandbox 隐藏窗口启动 smoke 均通过。

## 7. PR-E2：Dockspace Layout + Shell Polish

执行状态：计划中。

目标：

- 让主编辑器 shell 更接近成熟引擎编辑器。
- 固定默认布局，减少第一次打开时的随机感。

建议工作：

1. Dockspace 初始化布局。
   - 中央 `Scene Viewport`。
   - 左侧 `Scene Hierarchy` / 后续 `Explorer`。
   - 右侧 `Properties` / `Renderer Debug`。
   - 底部 `Project` / `Console` placeholder。
2. Menu bar 收口。
   - File / Edit / Project / Window / Help。
   - 当前 Save / Load Scene 放入 File。
   - 后续 panel visibility 放入 Window。
3. Status bar。
   - 当前 scene 名称。
   - SceneView / GameView。
   - FPS / frame time。
   - renderer backend。
4. Window 命名整理。
   - `Scene Viewport`、`Scene Hierarchy`、`Inspector`、`Renderer Debug` 等命名统一。

暂时不做：

- 不做完整 layout editor。
- 不做多 viewport / 多窗口 workspace。
- 不做复杂 workspace profile。

验收：

- 第一次打开 Editor 有稳定默认布局。
- 用户拖动 dock 后仍能正常使用。
- 可通过菜单打开 / 关闭常用面板。
- 构建和 Sandbox smoke 通过。

## 8. PR-E3：Toolbar + Command System v1

执行状态：计划中。

目标：

- 顶部工具栏具备基础编辑命令。
- 菜单、工具栏、快捷键共享同一套命令入口。

建议工作：

1. 新增轻量 command system。
   - `EditorCommand`
   - `EditorCommandRegistry`
   - `EditorShortcutMap`
2. 顶部 toolbar。
   - Save / Load。
   - Play / Pause / Stop placeholder。
   - Select / Move / Rotate / Scale。
   - Local / World。
   - Snap toggle。
3. Viewport tool state。
   - 将 gizmo operation / mode 从 `ViewportPanel` 私有状态逐步放入 editor tool state。
   - `ViewportPanel` 只消费当前 tool state。
4. Tooltip。
   - 每个图标按钮提供简短 tooltip。

暂时不做：

- 不做完整 Undo / Redo。
- 不做快捷键 rebinding UI。
- 不做 Play Mode 完整生命周期重构。

验收：

- Toolbar 可以控制 gizmo 模式。
- W / E / R / T 等快捷键仍工作。
- 菜单和 toolbar 不复制业务逻辑。
- 构建和 Sandbox smoke 通过。

## 9. PR-E4：Inspector Property Drawer v1

执行状态：计划中。

目标：

- 将 `PropertiesPanel` 从“手写组件 UI 堆叠”整理为更统一的 Inspector。
- 为后续组件扩展和 prefab / scene authoring 打基础。

建议工作：

1. 改名方向。
   - UI 标题建议从 `Properties Panel` 改为 `Inspector`。
   - 类名是否改为 `InspectorPanel` 可以单独评估，不强制在第一版做全局 rename。
2. Property drawer helper。
   - `drawVec3Property()`
   - `drawFloatProperty()`
   - `drawColorProperty()`
   - `drawAssetField()`
   - `drawComponentHeader()`
3. 组件分组。
   - Tag。
   - Transform。
   - Camera。
   - MeshRenderer。
   - Light。
   - Environment。
   - Gameplay / Physics / Audio 后续逐步补。
4. Add Component 菜单整理。
   - 按 Rendering / Camera / Light / Gameplay / Physics / Audio 分类。

暂时不做：

- 不做完整 C++ 反射系统。
- 不做复杂 prefab override UI。
- 不做多选编辑。

验收：

- Inspector 布局更紧凑、对齐更好。
- 新增组件 drawer 有固定模板。
- 现有组件编辑能力不倒退。
- 构建和 Sandbox smoke 通过。

## 10. PR-E5：Scene Hierarchy / Explorer Polish

执行状态：计划中。

目标：

- 改善 scene entity 浏览、搜索和常用创建操作。

建议工作：

1. 搜索框。
   - 按 entity name 过滤。
2. 右键菜单。
   - Create Empty。
   - Create Camera。
   - Create Light。
   - Duplicate。
   - Delete。
3. 选中态 polish。
   - 统一 selected / hover 颜色。
   - 空白点击清空选择保持稳定。
4. 图标。
   - entity / camera / light / mesh / audio 使用轻量 icon。

暂时不做：

- 不做完整父子层级拖拽。
- 不做 prefab nested hierarchy。
- 不做复杂 scene collection。

验收：

- Scene hierarchy 查找和常用创建更顺手。
- 选择状态和 viewport picking 继续同步。
- 构建和 Sandbox smoke 通过。

## 11. PR-E6：Project / Console Panels v1

执行状态：计划中。

目标：

- 补齐底部基础工具面板，让编辑器不再只有 scene / inspector。

建议工作：

1. Project panel。
   - 左侧 assets folder tree。
   - 右侧 folder contents。
   - 第一版显示文件名 / 类型即可。
2. Console panel。
   - 显示 engine log。
   - 支持 clear / filter。
   - 第一版可先接已有日志缓存或轻量 editor log sink。
3. 底部 tabs。
   - Project。
   - Console。
   - Timeline placeholder。
   - Profiler placeholder。

暂时不做：

- 不做资源缩略图生成。
- 不做拖拽导入完整链路。
- 不做 asset dependency graph。
- 不做完整 timeline / animation editor。

验收：

- Editor 底部有 Project / Console 基础面板。
- Console 能看到近期日志或明确 placeholder。
- Project 能浏览 assets 目录。
- 构建和 Sandbox smoke 通过。

## 12. PR-E7：Viewport Overlay + Gizmo Polish

执行状态：计划中。

目标：

- 让 viewport 更像主工作区，而不是普通 ImGui 图片面板。

建议工作：

1. Viewport overlay。
   - 左上角显示 view mode / resolution。
   - 右上角显示 gizmo mode / camera speed。
   - 底部显示 selected entity / FPS。
2. Viewport toolbar。
   - Scene / Game segmented control。
   - Shaded / Wireframe / Debug view placeholder。
   - Debug draw toggle。
3. Gizmo polish。
   - 统一颜色和尺寸。
   - 明确 Local / World。
   - 操作时避免 picking 抢输入。
4. Camera navigation hint。
   - 不用大段教学文字。
   - 只在状态区或 tooltip 中简短提示。

暂时不做：

- 不做自研 transform gizmo 替代 ImGuizmo。
- 不做复杂 editor icon rendering。
- 不做多 viewport。

验收：

- Viewport 信息层不遮挡主要画面。
- Gizmo 操作更清晰。
- Scene / Game 切换入口更自然。
- 构建和 Sandbox smoke 通过。

## 13. PR-E8：Preferences / Render Settings Panel

执行状态：计划中。

目标：

- 为后续 HDR、ACES、Bloom、Shadow、IBL 等渲染效果准备干净的 Editor UI 入口。
- 避免把可写效果参数混进只读 debug snapshot。

建议工作：

1. Preferences panel。
   - Editor preferences。
   - Viewport preferences。
   - Renderer effects。
2. RenderSettings 数据入口。
   - exposure。
   - tone mapping enabled / mode。
   - bloom enabled / intensity / radius。
   - shadow enabled / filter mode / bias。
   - debug view mode。
3. 和 `RendererDebugPanel` 分工。
   - `RendererDebugPanel`：只读状态、target、resource、frame stats。
   - `Preferences` / `Renderer Effects`：可写参数。
4. 保存策略。
   - 第一版可以仅运行时保存。
   - 后续接入 editor config 文件。

暂时不做：

- 不做完整 project settings asset。
- 不做 per-scene render profile。
- 不做 GPU profiler。

验收：

- 可写 render effect 参数不进入 `RendererDebugSnapshot`。
- Editor / Scene / Runtime 不依赖 Vulkan backend。
- 后续 PR-R26 之后的 HDR / ACES / Bloom 有 UI 接入口。
- 构建和 Sandbox smoke 通过。

## 14. PR-E9：Layout / Shortcut Persistence

执行状态：计划中。

目标：

- 让编辑器布局和常用快捷键具备基础持久化。

建议工作：

1. ImGui ini 路径固定。
   - 避免运行目录变化导致布局丢失。
2. Layout reset。
   - Window 菜单中提供 Reset Layout。
3. Editor config。
   - 保存 theme variant。
   - 保存 camera speed。
   - 保存 toolbar / panel visibility。
4. Shortcut persistence 预留。
   - 第一版可以只保存默认快捷键。
   - 后续提供 rebinding UI。

暂时不做：

- 不做云同步。
- 不做多用户 profile。
- 不做复杂 workspace system。

验收：

- 重启后布局稳定。
- 可以一键恢复默认布局。
- 快捷键和基础 editor setting 有清晰存储位置。
- 构建和 Sandbox smoke 通过。

## 15. 不建议现在做的事情

当前阶段暂缓：

- 不切换 Qt / Web UI。
- 不做完整 runtime UI framework。
- 不做材质节点编辑器。
- 不做动画时间轴编辑器完整版本。
- 不做复杂 asset thumbnail pipeline。
- 不做多窗口 workspace。
- 不做完整 Undo / Redo 历史系统。
- 不做脚本编辑器。

这些都合理，但应该等 Editor shell、Inspector、Asset / Console、Render Settings 稳定后再进入专项。

## 16. 完成标准

Editor UI polish 阶段达到以下状态即可认为完成：

- Editor 有稳定默认 dock layout。
- 主题统一，视觉干净、紧凑、有工具质感。
- 顶部菜单、工具栏、状态栏形成完整 shell。
- Toolbar / menu / shortcut 使用统一 command 入口。
- Inspector 具备统一 property drawer 风格。
- Scene Hierarchy 有搜索和常用右键操作。
- Project / Console 面板可用。
- Viewport overlay 和 gizmo 体验更清晰。
- Render Settings / Preferences 有清楚入口。
- Editor 不直接依赖 Vulkan backend。
- 构建、CTest 和 Sandbox smoke 稳定通过。

## 17. 执行日志

后续每个 Editor UI polish PR 完成后，在本节同步状态。

| PR | 状态 | 摘要 |
| --- | --- | --- |
| PR-E1 Editor Style Foundation | 已完成 | 新增 EditorStyle / editor_icons，统一 ImGui 主题与 ImGuizmo 风格，并迁移 Viewport 的 Scene/Game segmented button。 |
| PR-E2 Dockspace Layout + Shell Polish | 计划中 | 固定默认 dock layout，补 menu / status bar。 |
| PR-E3 Toolbar + Command System v1 | 计划中 | 建立 toolbar 和轻量 command registry。 |
| PR-E4 Inspector Property Drawer v1 | 计划中 | 整理 Properties/Inspector 的组件编辑 UI。 |
| PR-E5 Scene Hierarchy / Explorer Polish | 计划中 | 搜索、右键创建、图标和选中态 polish。 |
| PR-E6 Project / Console Panels v1 | 计划中 | 补齐底部 Project / Console 基础面板。 |
| PR-E7 Viewport Overlay + Gizmo Polish | 计划中 | 加强 viewport overlay、toolbar 和 gizmo 体验。 |
| PR-E8 Preferences / Render Settings Panel | 计划中 | 为渲染效果参数提供可写 UI 入口。 |
| PR-E9 Layout / Shortcut Persistence | 计划中 | 固定布局、快捷键和 editor config 持久化。 |
