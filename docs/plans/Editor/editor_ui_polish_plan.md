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
PR-E1.5 Editor Theme Calibration / Visual Baseline
PR-E2 Dockspace Layout + Shell Polish
PR-E2.5 Editor Visual System v2 / Engine-like Chrome
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
E1.5 Theme Calibration
E2 Dockspace Shell
E2.5 Visual System v2
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

## 6.5. PR-E1.5：Editor Theme Calibration / Visual Baseline

执行状态：已完成。

背景：

- PR-E1 已经建立 `EditorStyle` 统一入口，但当前 sandbox 打开后的实际观感仍然偏灰、偏亮、偏默认 ImGui。
- 参考目标图的核心不是“更多装饰”，而是深色、紧凑、层级清晰、低噪声的工具界面。
- 如果直接进入 PR-E2 做 dockspace / shell，容易把布局、菜单、状态栏和主题调色混在一个 PR 里，后续 review 和回退都会变重。
- 因此在 PR-E2 前插入一个小 PR，先把视觉基线调准，再让 E2 在稳定 theme 上做布局。

目标：

- 让当前 Editor UI 的颜色、对比度和控件质感更接近目标风格。
- 保持 E1 的集中 style 架构，只调整 `EditorStyle`，不让 panel 继续零散堆样式。
- 让 Window、Dock、TitleBar、Tab、Header、Frame、Slider、Combo、Scrollbar 的层级更清楚。
- 降低大面积蓝色 header / selected fill 的存在感，蓝色只作为 active / hover / selected 的少量强调。
- 形成后续 E2 / E3 / E4 可直接复用的视觉 token。

当前差距：

- Panel 背景和 dock 背景偏亮，整体看起来像一层灰色遮罩，而不是成熟工具 shell。
- Header / TitleBar / Tab / Frame 的层级不够分明，信息密度有了，但质感不够稳。
- 控件填充色偏亮，slider / combo / checkbox 在 debug 面板里显得噪声较多。
- Viewport 内容偏亮属于 renderer / exposure 方向，不能在 Editor UI PR 里混着修；E1.5 只处理 UI shell 本身。
- 当前左侧仍是 `Renderer Debug`，底部缺 Project / Console，右侧 Inspector 也还没 polish，这些属于 E2 / E4 / E6，不放进 E1.5。

建议工作：

1. 重新校准 `EditorStyle::applyTheme()`。
   - 深化 `WindowBg`、`ChildBg`、`MenuBarBg`、`TitleBg`、`DockingEmptyBg`。
   - 保持非纯黑，避免过重，但整体比当前更接近深色工具 UI。
   - 让 Border / Separator 更细、更低噪声。
2. 收敛 active / hover 色。
   - `Header`、`HeaderHovered`、`HeaderActive` 使用低饱和蓝灰。
   - `Button`、`FrameBg`、`Tab` 以深灰为主，只在交互态轻微提亮。
   - `CheckMark`、`SliderGrab`、`NavHighlight` 保留冷色 accent。
3. 优化控件视觉密度。
   - 调整 `FramePadding`、`ItemSpacing`、`ScrollbarSize`、`GrabMinSize`。
   - 保持紧凑但不压缩到难读。
   - 避免每个 panel 自己改 padding。
4. 校准 dock / tab / title chrome。
   - Menu bar、Dockspace root、Tab active / unfocused active 有明确层级。
   - Dock preview 颜色克制，不抢 viewport。
5. 轻量检查 `ViewportPanel` segmented button。
   - Scene / Game 选中态更像 segmented control，而不是普通按钮。
   - 不在本 PR 做完整 viewport toolbar。
6. 更新文档和截图验收记录。
   - 记录 E1.5 只处理视觉基线，不处理布局和 renderer exposure。

暂时不做：

- 不做 PR-E2 的默认 dock layout。
- 不新增顶部 toolbar、底部 Project / Console、状态栏。
- 不重构 `RendererDebugPanel` / `PropertiesPanel` 的字段布局。
- 不接入 icon font。
- 不加载新字体。
- 不修改 renderer exposure、tone mapping、skybox 或 bloom。
- 不把 theme 逻辑放进 Vulkan ImGui backend。

代码组织：

```text
source/Engine/Editor/Style/
  editor_style.h
  editor_style.cpp
  editor_icons.h
```

- 主要改动集中在 `editor_style.cpp` 内部 palette / metrics。
- 如有必要，可在 `editor_style.cpp` 内用局部 `Palette` / `Metrics` 常量整理颜色和尺寸。
- 公共 API 尽量不新增，避免小 PR 变成 UI framework 扩张。
- `EditorLayer` 继续只调用 `EditorStyle::applyTheme()` / `EditorStyle::applyGizmoStyle()`。
- `ViewportPanel` 继续只消费 `EditorStyle::segmentedButton()`，不直接写视觉 token。

验收：

- 打开 sandbox 后，Editor UI 不再偏灰白，整体更接近深色、低噪声、专业工具感。
- Panel / TitleBar / Tab / Header / Frame 的层级肉眼清楚。
- Debug panel 虽然布局仍旧，但控件噪声明显降低。
- Scene / Game segmented button 选中态明确。
- 现有 dock 布局和 editor 功能不倒退。
- 构建、CTest 和 Sandbox smoke 通过。

实现记录：

- 重新校准 `EditorStyle::applyTheme()` 的深色 palette，压低 `WindowBg`、`ChildBg`、`MenuBarBg`、`TitleBg`、`DockingEmptyBg` 的亮度。
- 将 `FrameBg`、`Button`、`Header`、`Tab` 调整为深灰 / 低饱和蓝灰，减少大面积蓝色块。
- 增加 `FrameBorderSize` 并降低 `Border` / `Separator` 亮度，让控件层级更清楚但不变吵。
- 调整 `WindowPadding`、`FramePadding`、`ItemSpacing`、`ScrollbarSize`、`GrabMinSize`，让 debug 面板的控件密度更紧凑。
- 优化 `EditorStyle::segmentedButton()` 的 active 态，Scene / Game 切换更像 segmented control。
- 未改 dock layout、toolbar、Project / Console、Inspector 字段布局、字体和 renderer exposure。
- 验证：`cmake --build --preset msvc-vcpkg-debug`、`ctest --test-dir build/msvc-vcpkg -C Debug --output-on-failure`、Sandbox 隐藏窗口启动 smoke 均通过。

## 7. PR-E2：Dockspace Layout + Shell Polish

执行状态：已完成。

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

完成记录：

- `EditorLayer` 增加稳定默认 DockBuilder layout：左侧 Scene Hierarchy，中间 Scene Viewport，右侧 Inspector / Renderer Debug，底部 Project / Console。
- 主菜单收口到 File / Edit / Project / Window / Help，并提供 panel 显隐与 Reset Layout。
- 底部状态栏显示当前 scene、视图模式、renderer backend、FPS / frame time。
- 新增 `ProjectPanel` 与 `ConsolePanel` 基础外壳，为后续资源浏览、日志 sink 接入、底部工具区 polish 留出干净扩展点。
- 统一窗口标题为 `Scene Viewport`、`Scene Hierarchy`、`Inspector`、`Renderer Debug`、`Project`、`Console`。

## 7.5. PR-E2.5：Editor Visual System v2 / Engine-like Chrome

执行状态：已完成。

目标：

- 建立统一的编辑器视觉系统，让当前 ImGui 编辑器从默认灰色工具风升级为现代深色引擎编辑器风格。
- 统一深色 graphite / charcoal 主题，降低默认 ImGui 灰色面板的廉价感。
- 强化主视口、左侧 Hierarchy、右侧 Inspector、底部 Console / Content Browser 的视觉层级。
- 统一按钮、tab、toolbar、panel header、separator、input field、slider、checkbox 等控件风格。
- 为后续编辑器 panel 提供统一封装，减少每个 panel 裸写 ImGui 样式。

建议颜色 token：

```text
BackgroundMain       #0F1419
BackgroundPanel      #151B22
BackgroundPanelLight #1B232C
Header               #202A34
BorderSubtle         #2A3440
TextPrimary          #D7DEE8
TextSecondary        #9AA7B5
AccentBlue           cyan / blue，用于 selected、active tab、按钮高亮
AccentOrange         orange / yellow，用于 warning、selection outline、重要提示
ErrorRed             error 日志
SuccessGreen         ok 状态
```

建议工作：

1. 主题 token 化。
   - 在 `EditorStyle` 中建立 `EditorThemeColors` / `EditorThemeMetrics` 一类的集中定义。
   - `applyTheme()` 只消费 token，不散落硬编码颜色。
   - 统一 window、child、popup、menu、tab、header、table、input、slider、checkbox、separator、scrollbar、dock preview 的颜色和层级。
2. 字体和 icon 入口。
   - 预留 `EditorFonts` / `EditorTheme` 字体加载入口。
   - 默认 UI 字体优先使用项目内可用清晰无衬线字体；仓库没有字体资源时保留接口，不强行引入字体文件。
   - Console / code 区域预留等宽字体入口。
   - 图标建议支持 FontAwesome / Material Icons / Codicon 等 Icon Font，但接入前必须检查 license。
   - 字体路径不能硬编码系统路径，应来自 EditorConfig 或 assets 下的项目路径配置。
   - 图标字体不可用时，UI 必须 fallback 到纯文本按钮。
3. Editor UI 组件封装。
   - 当前已有 `EditorStyle::iconButton()`、`segmentedButton()`、`drawSectionHeader()`、`drawSubtleText()`、`drawHelpTooltip()`，但还不足以支撑后续 panel。
   - 建议新增或扩展 `EditorWidgets` / `EditorStyle`：
     - `drawToolbarButton()`
     - `drawSearchBox()`
     - `drawPanelHeader()`
     - `drawPropertyRow()`
     - `drawAssetField()`
     - `drawStatusPill()`
     - `drawConsoleLine()`
     - `drawTableSelectableRow()`
   - Panel 内尽量调用统一 helper，避免直接堆 `PushStyleColor()` / `PushStyleVar()`。
4. Engine-like chrome。
   - 顶部 menu / toolbar / dock tab / status bar 的背景层级统一。
   - Viewport 增加更明确的边框和工作区层级。
   - Inspector、Hierarchy、Project、Console 使用一致的 header、search、row hover、selected 态。
   - Rounding 保持克制，不做大圆角卡片；工具型 UI 以 0-4px 小圆角为主。
5. 视觉回归基线。
   - 在文档中记录当前目标截图方向和颜色 token。
   - Sandbox 启动后目视确认不再是默认灰色 ImGui 风。

具体开发计划：

1. 文件结构整理。
   - 保留 `source/Engine/Editor/Style/editor_style.h/.cpp` 作为主题入口。
   - 新增 `source/Engine/Editor/Style/editor_theme.h/.cpp`，集中定义颜色、尺寸、rounding、spacing token。
   - 新增 `source/Engine/Editor/Style/editor_fonts.h/.cpp`，预留字体 / icon font 加载入口。
   - 新增 `source/Engine/Editor/Widgets/editor_widgets.h/.cpp`，放通用控件封装。
   - `editor_icons.h` 继续作为 icon 名称 / fallback 文本入口，后续如果接 icon font，只替换常量内容或查询接口。
   - 新文件统一加入 `source/Engine/CMakeLists.txt` 的 editor file list。
2. Theme token v2。
   - 定义 `EditorThemeColors`：
     - `background_main`
     - `background_panel`
     - `background_panel_light`
     - `header`
     - `header_hovered`
     - `header_active`
     - `border_subtle`
     - `text_primary`
     - `text_secondary`
     - `accent_blue`
     - `accent_cyan`
     - `accent_orange`
     - `error_red`
     - `success_green`
   - 定义 `EditorThemeMetrics`：
     - `window_padding`
     - `frame_padding`
     - `item_spacing`
     - `cell_padding`
     - `indent_spacing`
     - `scrollbar_size`
     - `window_rounding`
     - `panel_rounding`
     - `frame_rounding`
     - `tab_rounding`
     - `border_size`
   - `EditorStyle::applyTheme()` 改为从 token 写入 ImGuiStyle。
   - 保留 `EditorStyle::applyGizmoStyle()`，但颜色也尽量引用 theme token。
3. 字体 / Icon Font 预留。
   - `EditorFonts` 第一版只提供接口和 fallback，不强制引入字体资源：
     - `loadFonts(ImGuiIO&, const EditorFontConfig&)`
     - `hasIconFont()`
     - `getDefaultFont()`
     - `getMonoFont()`
   - `EditorFontConfig` 支持项目内路径，例如：
     - `ui_font_path`
     - `mono_font_path`
     - `icon_font_path`
     - `ui_font_size`
     - `mono_font_size`
   - 字体路径来源先预留为 EditorConfig / assets 路径，不读取系统字体路径。
   - 如仓库没有字体资源，本 PR 只保留默认 ImGui 字体和文本 icon fallback。
   - 文档中记录 FontAwesome / Material Icons / Codicon 等候选 icon font 需要 license review。
4. EditorWidgets v1。
   - 第一版封装只做“足够复用、低侵入”的控件：
     - `toolbarButton(label, tooltip, selected)`
     - `searchBox(id, buffer, size, hint)`
     - `panelHeader(title)`
     - `sectionHeader(title, default_open)`
     - `propertyRow(label, draw_value_callback)`
     - `statusPill(label, color)`
     - `consoleLine(level, logger, message)`
     - `tableSelectableRow(label, selected)`
   - 不在第一版做复杂模板反射，不把 Inspector 全量重写进 E2.5。
   - Helper 负责局部 style push/pop，调用者不直接散落颜色。
5. 迁移现有 panel 的低风险部分。
   - `ViewportPanel`：
     - Scene / Game segmented control 继续走统一 helper。
     - Viewport window 保留零 padding，但边框 / active 层级跟随 theme。
   - `ProjectPanel`：
     - 搜索框改用 `EditorWidgets::searchBox()`。
     - 表格 row hover / selected 风格跟随 theme。
   - `ConsolePanel`：
     - 日志行改用 `EditorWidgets::consoleLine()`。
     - error / warning / info / trace 颜色引用 theme token。
   - `RendererDebugPanel`：
     - section header 改用统一 helper。
   - `PropertiesPanel`：
     - 只迁移 section header / subtle text，不在本 PR 做完整 property drawer。
6. Shell chrome 校准。
   - Dockspace root、menu bar、status bar 背景统一成 graphite / charcoal 层级。
   - Dock tab active / hovered / unfocused active 明确区分。
   - Border 使用 `BorderSubtle`，减少亮灰线条。
   - Window title、child panel、table header、input field、slider、checkbox 的默认灰感统一压暗。
   - Accent blue / cyan 只用于 active / selected / interaction，不大面积铺色。
   - Accent orange 只用于 warning / selection outline / important hint。
7. 验证和回归。
   - 构建：`cmake --build build\msvc-vcpkg --config Debug`。
   - 自动测试：`ctest --test-dir build\msvc-vcpkg -C Debug --output-on-failure`。
   - Sandbox smoke：启动 `bin\msvc-vcpkg\Debug\Sandbox.exe`，确认不提前退出。
   - 目视验收：
     - 主背景不再是默认灰。
     - 左中右底面板层级清晰。
     - Console / Project 不再像原始 ImGui demo table。
     - Viewport 是第一视觉主体。
     - 默认字体路径缺失时仍能正常显示。

开发边界：

- E2.5 只做视觉系统和轻量迁移，不把所有 panel 全面重写。
- 如果需要引入字体文件或 icon font 文件，必须单独确认 license 和资源放置路径。
- 不为了视觉效果引入新的 UI 技术栈。
- 不让 Editor panel 直接依赖 Vulkan backend。

暂时不做：

- 不在本 PR 完整实现 Toolbar 命令系统，留给 PR-E3。
- 不重写 Inspector property drawer，留给 PR-E4。
- 不做完整 Content Browser 缩略图，留给后续 Project / Asset pass。
- 不强行引入第三方字体或图标字体文件；如需引入，单独确认 license。
- 不硬编码 Windows / macOS / Linux 系统字体路径。

验收：

- Editor 主题接近 graphite / charcoal，主背景、panel、header、tab、input 层级清晰。
- Accent blue / cyan、warning orange、error red、success green 有统一 token 和使用入口。
- 常用控件视觉更紧凑精致，默认 ImGui 灰色感明显降低。
- 至少一部分现有 panel 改为使用统一 helper，后续 panel 有明确封装路径。
- 字体 / icon 加载有集中设计，不在 panel 内零散加载或硬编码系统路径。
- 构建和 Sandbox smoke 通过。

完成记录：

- 新增 `EditorTheme` / `EditorThemeTokens`，集中定义 graphite / charcoal 颜色 token 和 spacing / rounding / border metrics。
- `EditorStyle::applyTheme()` 改为消费 theme token，统一 window、child、popup、menu、tab、header、table、input、slider、checkbox、separator、scrollbar、dock preview 等颜色。
- 新增 `EditorFonts` 作为字体 / icon font 集中入口；第一版只保留项目路径加载和 fallback，不硬编码系统字体，不强行引入字体资源。
- 新增 `EditorWidgets`，提供 toolbar button、search box、panel header、section header、property row、status pill、console line、table selectable row 等轻量 helper。
- 迁移 `ProjectPanel`、`ConsolePanel`、`RendererDebugPanel`、`PropertiesPanel` 的低风险控件到统一 helper。
- `ConsolePanel` 的日志等级颜色改为引用 theme token。
- 本 PR 只做视觉系统和低风险迁移，不做 Toolbar command system、不重写 Inspector property drawer。

## 8. PR-E3：Toolbar + Command System v1

执行状态：已完成。

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

完成记录：

- 新增 `EditorCommand` / `EditorCommandRegistry`，支持 command id、显示名、tooltip、快捷键、enabled / selected predicate 和 execute callback。
- `EditorLayer` 统一注册 Save / Load、Project / Console 面板打开、Reset Layout / Open All Panels、Play / Pause / Stop placeholder、Select / Move / Rotate / Scale、Local / World、Snap 等命令。
- 顶部主工具栏接入统一 command registry，工具栏按钮和菜单项不再复制 Save / Load 等业务逻辑。
- `Ctrl+S` / `Ctrl+O`、`Q` / `W` / `E` / `R` / `T` 走统一快捷键分发；文本输入时快捷键自动让路。
- 新增 `EditorToolState`，`ViewportPanel` 不再保存私有 gizmo operation / mode / snap 状态，而是消费共享 tool state。
- Play / Pause / Stop 在本 PR 仍为占位命令，只写日志，不提前引入完整 Play Mode 生命周期。

## 9. PR-E4：Inspector Property Drawer v1

执行状态：已完成。

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

完成记录：

- 新增 `EditorPropertyDrawer`，集中封装 component header、string / float / int / bool / vec3 / color / asset field / read-only text 等 Inspector 字段绘制。
- `PropertiesPanel` 迁移为消费统一 drawer，Tag、Transform、Camera、Active Camera、MeshRenderer、Directional Light、Point Light、Environment 具备统一 label/value 对齐。
- `Add Component` 菜单按 Rendering / Camera / Light 分组，并为 Gameplay / Physics / Audio 预留后续分类入口。
- Asset 字段 v1 只做只读显示和类型展示，不做 asset picker、drag-drop 或 Content Browser 联动。
- 本 PR 不引入 C++ 反射系统、不做多选编辑、不做 prefab override UI。

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

执行状态：已完成。

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

完成记录：

- `LogSystem` 增加 recent log ring buffer sink，Console 面板可以读取近期 engine log。
- `ConsolePanel` 支持 Clear、Filter、Auto Scroll，并按日志等级显示不同颜色。
- `ProjectPanel` 增加搜索、隐藏文件开关、上一级/回 assets 操作，并显示 Name / Type / Size。
- 底部工具区补齐 `Timeline` / `Profiler` placeholder tab，但不提前实现 timeline / profiler 业务。

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
| PR-E1.5 Editor Theme Calibration / Visual Baseline | 已完成 | 校准深色主题、控件层级和 segmented button active 态，为 dockspace shell 打稳定视觉底。 |
| PR-E2 Dockspace Layout + Shell Polish | 已完成 | 固定默认 dock layout，补齐 File / Edit / Project / Window / Help、状态栏、Project / Console 底部面板外壳。 |
| PR-E2.5 Editor Visual System v2 / Engine-like Chrome | 已完成 | 新增 EditorTheme / EditorFonts / EditorWidgets，重构 theme token，并迁移 Project / Console / RendererDebug / Properties 的低风险控件。 |
| PR-E3 Toolbar + Command System v1 | 已完成 | 建立 toolbar 和轻量 command registry，菜单、工具栏、快捷键共享命令入口，Viewport gizmo 消费统一 tool state。 |
| PR-E4 Inspector Property Drawer v1 | 已完成 | 新增 EditorPropertyDrawer，统一 Inspector 字段绘制，并补齐基础组件 drawer 与 Add Component 分类菜单。 |
| PR-E5 Scene Hierarchy / Explorer Polish | 计划中 | 搜索、右键创建、图标和选中态 polish。 |
| PR-E6 Project / Console Panels v1 | 已完成 | Project 支持 assets 浏览、搜索、类型/大小信息；Console 接入 recent log sink，支持 clear / filter / auto-scroll，并补 Timeline / Profiler placeholder。 |
| PR-E7 Viewport Overlay + Gizmo Polish | 计划中 | 加强 viewport overlay、toolbar 和 gizmo 体验。 |
| PR-E8 Preferences / Render Settings Panel | 计划中 | 为渲染效果参数提供可写 UI 入口。 |
| PR-E9 Layout / Shortcut Persistence | 计划中 | 固定布局、快捷键和 editor config 持久化。 |
