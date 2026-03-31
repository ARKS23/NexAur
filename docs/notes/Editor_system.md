# 编辑器开发记录文档
## EditorLayer：完成
职责：控制子面板的渲染，事件分发，作为总调度。

## ViewportPanel：完成
职责：显示渲染画面，点击物体交互。

## SceneHierarchyPanel
职责：遍历当前 Scene 中的 EnTT registry，把所有拥有 TagComponent（名字）的实体用树状列表（ImGui::TreeNode）画出来。
交互：点击某个节点时，将该 Entity ID 写入 EditorContext。
- SceneHierarchyPanel 职责边界
```
只负责展示实体树。
只负责把用户点击映射为 selected_entity 变化。
不负责 Gizmo，不负责属性编辑，不负责渲染视口逻辑。
场景数据来源仅通过 active_scene，从 scene_v2.h 的 registry 读取，显示名字通过 component.h 的 TagComponent。
```

## PropertiesPanel: 开发中
- 职责：从 EditorContext 中读取当前选中的 Entity。如果非空，就用 registry.try_get<T>() 挨个检查它身上挂了什么组件。
- UI 生成：如果有 TransformComponent，就画出位置/旋转/缩放的拖拽条；如果有 MeshComponent，就显示当前使用的模型路径等。

## ContentBrowserPanel
- 职责：遍历本地电脑目录列出资产文件