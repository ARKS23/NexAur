# NexAur Editor Material Editing Design Plan

日期：2026-07-03

## 1. 文档定位

本文档记录 NexAur Editor 中“选中物体后编辑材质”的设计和开发方案。

目标不是只在 Inspector 里加几个滑条，而是补齐从场景实体、材质资产、渲染前端到 Vulkan GPU material resource 的完整链路，让材质编辑成为稳定的编辑器能力，并服务后续 PBR、SSR、Reflection Probe、材质调试和资产工作流。

第一阶段重点：

- 选中实体后能看到模型的 material slots。
- 每个 slot 能显示模型默认材质，并允许绑定 override material。
- 能创建 per-object runtime material instance，编辑基础 PBR 参数并实时反映到 viewport。
- 场景保存 / 加载能保留 material override 关系。

后续阶段再扩展：

- `.nxmat` 持久化材质资产。
- Project 面板材质资源管理。
- 贴图槽拖拽、材质预览球、Undo / Redo 和多选编辑。

## 2. 当前基础与问题

当前 NexAur 已具备材质渲染基础：

- `MaterialImportData` 描述 glTF / 模型导入阶段的 PBR 材质参数和贴图路径。
- `MaterialAsset` 保存 base color、metallic、roughness、emissive、normal scale、AO strength、alpha mode、double sided 和 texture handles。
- `AssetManager::createMaterialFromImportData` 可以从导入数据创建 CPU `MaterialAsset`。
- `VulkanMaterialResource` 可以把 `MaterialAsset` 转成 uniform buffer + texture descriptor。
- `VulkanModelResource` 创建模型 GPU 资源时，会为模型内每个 mesh 创建默认 material resource。

但编辑器层缺少关键中间层：

- `MeshRendererComponent` 目前只有 `model_asset` 和 `is_transparent`，没有材质槽或材质 override。
- `RenderObjectData` / `RenderSceneFrameObject` 只传递 `model_asset + transform + entity_id`，没有 per-object material binding。
- `VulkanDrawListBuilder` 直接使用模型内置 materials，无法按实体覆盖材质。
- `MaterialAsset` 当前是只读值对象，没有 setter，也没有 `.nxmat` 文件格式。
- `PropertiesPanel` 只能显示 Model asset，不能编辑 submesh / material slot。

因此，材质编辑不能直接做成“改模型默认材质”。必须先引入对象级 material slot override。

## 3. 设计原则

### 3.1 默认材质只读，编辑前先实例化

模型导入材质属于 model asset 的默认数据。直接编辑它会带来两个问题：

- 多个实体引用同一个模型时，一个物体的修改会影响所有物体。
- glTF / OBJ 导入材质不一定有可写回的原始资产格式。

推荐规则：

- 模型默认材质在 Inspector 中只读展示。
- 用户要修改时，点击 `Make Instance`。
- `Make Instance` 从默认材质复制一份 runtime material，并绑定到当前实体的对应 slot。
- 后续 `Save As Material` 可以把 runtime material 写成 `.nxmat` 资产。

### 3.2 场景保存绑定关系，不保存 GPU 对象

场景只保存：

- model asset reference。
- material override asset references。
- 必要的 runtime-generated material identity，第一阶段可选。

场景不保存：

- Vulkan descriptor set。
- Vulkan buffer。
- texture image view。
- backend-specific resource 指针。

### 3.3 Editor Panel 不直接依赖 Vulkan

继续保持现有边界：

```text
PropertiesPanel
  -> EditorContext
  -> AssetManager / Scene / Renderer-neutral data
  -> RenderContext
  -> Renderer backend
```

禁止方向：

```text
PropertiesPanel -> VulkanMaterialResource
PropertiesPanel -> VkDescriptorSet
PropertiesPanel -> VulkanRenderResourceCache
```

### 3.4 先支持 PBR 参数，再支持复杂材质图

第一阶段只做 metallic-roughness PBR：

- Base Color Factor
- Metallic Factor
- Roughness Factor
- Emissive Factor
- Normal Scale
- AO Strength
- Alpha Mode / Alpha Cutoff
- Double Sided
- Texture slots

不在第一阶段引入节点材质、shader graph、layered material 或材质函数。

## 4. 数据模型

### 4.1 MeshRendererComponent 扩展

建议扩展 `MeshRendererComponent`：

```cpp
struct MeshRendererComponent {
    AssetHandle model_asset;
    std::vector<AssetHandle> material_overrides;
    bool is_transparent = false;
};
```

语义：

- `material_overrides[i]` 对应模型第 `i` 个 mesh / material slot。
- override handle 无效时，使用模型导入的默认材质。
- override handle 有效时，使用该 Material asset。
- 如果 override 数量少于 mesh 数量，缺失 slot 等价于无 override。

后续可扩展：

```cpp
struct MeshMaterialSlotOverride {
    AssetHandle material;
    bool enabled = true;
};
```

但第一版用 `std::vector<AssetHandle>` 更轻。

### 4.2 MaterialAsset 可编辑化

当前 `MaterialAsset` 是只读接口。要支持编辑，需要新增两层能力：

1. CPU 材质数据可复制 / 可修改。
2. 修改后能通知 renderer 重新创建或更新 GPU material resource。

推荐第一阶段新增 setter：

```cpp
void setBaseColorFactor(const glm::vec4& value);
void setMetallicFactor(float value);
void setRoughnessFactor(float value);
void setEmissiveFactor(const glm::vec3& value);
void setNormalScale(float value);
void setOcclusionStrength(float value);
void setAlphaMode(MaterialAlphaMode mode);
void setAlphaCutoff(float value);
void setDoubleSided(bool value);
void setBaseColorTexture(AssetHandle handle);
...
```

同时保留 `MaterialImportData` 作为导入格式，不把它当成运行期编辑状态。

### 4.3 Runtime Material Instance

第一阶段推荐引入 runtime material instance helper：

```cpp
AssetHandle AssetManager::createRuntimeMaterialInstance(
    const MaterialAsset& source,
    const std::string& debug_name);
```

行为：

- 复制 source 的参数和 texture handles。
- 注册为 `AssetType::Material`。
- `runtime_generated = true`。
- 返回新的 `AssetHandle`。

这可以支撑 `Make Instance`。

## 5. Scene 序列化

`MeshRendererComponent` 保存时增加：

```json
"MeshRenderer": {
  "model": { ... },
  "transparent": false,
  "material_overrides": [
    null,
    {
      "uuid": "...",
      "asset_type": "Material",
      "path": "assets/materials/gold.nxmat",
      "debug_name": "Gold"
    }
  ]
}
```

读取规则：

- 老场景没有 `material_overrides` 时保持兼容。
- override 数组元素为空或无效时回退模型默认材质。
- 如果 override 是 runtime-generated 且无 path，第一阶段可以注册 runtime identity 但不保证跨进程复原参数。
- `.nxmat` 完成后，Material override 应优先通过 path import。

需要扩展：

- `writeAssetReference` 支持 Material asset。
- `importAssetReference` 支持 `AssetType::Material`。
- `SceneSerializer` 写入 / 读取 `MeshRendererComponent::material_overrides`。

## 6. 渲染链路

### 6.1 RenderData / RenderSceneFrame

扩展：

```cpp
struct RenderObjectData {
    AssetHandle model_asset;
    std::vector<AssetHandle> material_overrides;
    glm::mat4 transform;
    int entity_id = -1;
};

struct RenderSceneFrameObject {
    AssetHandle model_asset;
    std::vector<AssetHandle> material_overrides;
    glm::mat4 transform;
    int entity_id = -1;
};
```

`RenderSceneFrameBuilder` 从 `MeshRendererComponent` 拷贝 overrides。

### 6.2 VulkanDrawListBuilder

当前 draw list builder 使用：

```cpp
draw_item.material = mesh_index < materials.size()
    ? &materials[mesh_index]
    : resource_cache.getFallbackMaterial();
```

改为：

```cpp
const AssetHandle override_material =
    mesh_index < object.material_overrides.size()
        ? object.material_overrides[mesh_index]
        : AssetHandle();

if (override_material) {
    draw_item.material =
        resource_cache.getOrCreateMaterial(override_material, asset_manager);
} else {
    draw_item.material = mesh_index < materials.size()
        ? &materials[mesh_index]
        : resource_cache.getFallbackMaterial();
}
```

sort key 建议加入 material id：

```cpp
buildSortKey(model_asset, mesh_index, resolved_material_asset)
```

这样可减少 descriptor binding 抖动。

### 6.3 VulkanRenderResourceCache

需要新增独立 material cache：

```cpp
VulkanMaterialResource* getOrCreateMaterial(
    AssetHandle material_asset,
    AssetManager& asset_manager);
```

行为：

- `AssetManager::loadMaterialCPU(material_asset)` 取得 CPU `MaterialAsset`。
- 用现有 `createMaterialResource` 创建 GPU material resource。
- cache key 为 material asset handle。
- material 修改后需要失效 / 重建对应 GPU resource。

第一阶段可以采用简单策略：

- 编辑器每次参数变更后，标记 material dirty。
- renderer resource cache 每帧检查 dirty generation，必要时重建。

更稳的后续策略：

```cpp
struct MaterialAsset {
    uint64_t generation = 0;
};
```

每次 setter 修改 generation，renderer cache 记录最后上传 generation。

## 7. Inspector UX 设计

### 7.1 Mesh Renderer 区域

选中带 `MeshRendererComponent` 的实体后，Inspector 显示：

```text
Mesh Renderer
  Model               [DamagedHelmet.glb]
  Transparent         [ ]

  Materials
    Slot 0: Default
      Default         DamagedHelmet.Default
      Override        None
      [Make Instance] [Use Default]

    Slot 1: Override
      Default         CornellBox.RedWall
      Override        RedWall.Instance
      Base Color      [ color picker ]
      Metallic        [ slider 0..1 ]
      Roughness       [ slider 0..1 ]
      Emissive        [ color/vector ]
      Normal Scale    [ slider ]
      AO Strength     [ slider ]
      Alpha Mode      [ Opaque / Mask / Blend ]
      Alpha Cutoff    [ slider ]
      Double Sided    [ checkbox ]
      Textures        [ foldout ]
```

### 7.2 操作按钮

- `Make Instance`
  - 从默认材质或当前 override 复制 runtime material。
  - 绑定到当前 slot。
  - 展开编辑区。

- `Use Default`
  - 清除该 slot override。
  - 回退模型默认材质。

- `Save As Material`
  - 后续 `.nxmat` 阶段提供。
  - 把 runtime material 保存为持久化 Material asset。

- `Locate Asset`
  - 后续 Project 面板完成后提供。

### 7.3 Asset Field

材质槽需要可编辑 asset field：

- 当前 `EditorPropertyDrawer::drawAssetField` 是只读展示。
- 需要新增 `drawAssetReferenceProperty` 或扩展现有 asset field：
  - 显示当前 asset。
  - 支持清除。
  - 支持路径输入 / Import。
  - 后续支持 Project 面板 drag-drop。

第一阶段可以先用：

```text
Override Path [input]
[Import Material] [Clear]
```

等 Project 面板完善后再替换为 drag-drop asset picker。

## 8. Material Asset 持久化

`.nxmat` 建议使用 JSON：

```json
{
  "format": "NexAurMaterial",
  "version": 1,
  "name": "SSR Mirror",
  "base_color_factor": [1.0, 1.0, 1.0, 1.0],
  "metallic_factor": 1.0,
  "roughness_factor": 0.05,
  "emissive_factor": [0.0, 0.0, 0.0],
  "normal_scale": 1.0,
  "occlusion_strength": 1.0,
  "alpha_mode": "Opaque",
  "alpha_cutoff": 0.5,
  "double_sided": false,
  "textures": {
    "base_color": "assets/textures/PBR/gold/albedo.png",
    "normal": "assets/textures/PBR/gold/normal.png",
    "metallic": "assets/textures/PBR/gold/metallic.png",
    "roughness": "assets/textures/PBR/gold/roughness.png",
    "metallic_roughness": "",
    "ao": "",
    "emissive": ""
  },
  "metallic_roughness_mode": "Separate"
}
```

`AssetManager` 增加：

```cpp
AssetHandle importMaterialAsset(const std::string& path);
bool saveMaterialAsset(AssetHandle handle, const std::string& path);
std::shared_ptr<MaterialAsset> loadMaterialCPU(AssetHandle handle);
```

纹理色彩空间规则：

- Base Color / Emissive：sRGB。
- Normal / Metallic / Roughness / MetallicRoughness / AO：Linear。

## 9. 和 SSR / Reflection Probe 的关系

材质编辑会直接改善当前渲染调试效率：

- SSR 测试时可以直接把选中平面调成 metallic 1.0 / roughness 0.02。
- `SSR Surface Mask` 可用于确认当前材质是否进入 SSR 链路。
- Reflection Probe 调试时可以快速切换高 roughness / 低 roughness 材质。
- 后续 material normal / roughness meta target 需要可靠的材质数据来源。

因此建议把材质编辑排在 Renderer quality promote 的后续辅助 PR 中，而不是作为纯 UI polish。

## 10. 非目标

第一阶段不做：

- Shader Graph。
- Node-based material editor。
- Layered Material。
- Subsurface / Clear Coat / Anisotropy。
- 多用户资产锁。
- 完整 Undo / Redo。
- Prefab / Variant 系统。
- 自动材质球缩略图烘焙。

这些能力可以在 material asset 和 Project 面板成熟后再规划。

## 11. PR 拆分建议

### PR-EP1：Editor Procedural Primitive Creation

目标：

- 在 Editor 菜单 / Scene Hierarchy 右键菜单中提供 `Create > 3D Object` 入口。
- 支持创建常用程序化简单形状：
  - Cube
  - Sphere
  - Plane
  - Cylinder
  - Cone
  - Capsule 可作为后续可选项。
- 创建后自动生成实体，并添加 `TagComponent`、`TransformComponent`、`MeshRendererComponent`。
- primitive mesh 作为 runtime procedural model asset 注册到 `AssetManager`，由现有 renderer resource cache 走普通 `model_asset` 上传路径。
- 新建物体默认放在当前 scene camera / editor camera 前方，或世界原点附近，并自动选中。
- 每个 primitive 使用合理默认材质；后续材质编辑 PR 完成后可直接 `Make Instance` 调材质。

设计边界：

- 第一版只生成静态 mesh，不做 CSG、参数化建模器或实时 editable mesh。
- 不把 primitive 直接硬编码进 Vulkan draw pass；仍走 `Model -> MeshRenderer -> RenderData -> ResourceCache` 的普通资产链路。
- runtime procedural model 第一阶段可以不导出为磁盘文件，但场景保存需要能记录它的 primitive 类型和基础参数，确保 reload 后可重建。

建议数据：

```cpp
enum class ProceduralPrimitiveType {
    Cube,
    Sphere,
    Plane,
    Cylinder,
    Cone
};

struct ProceduralPrimitiveComponent {
    ProceduralPrimitiveType type = ProceduralPrimitiveType::Cube;
    uint32_t segments = 32;
    uint32_t rings = 16;
};
```

创建流程：

```text
Editor Command: Create Primitive
  -> Scene creates entity
  -> add TransformComponent
  -> add ProceduralPrimitiveComponent
  -> build CPU Model / Mesh
  -> AssetManager registerRuntimeModel
  -> MeshRendererComponent.model_asset = runtime model handle
  -> select entity
```

验收：

- 能从菜单或层级面板创建 Cube / Sphere / Plane / Cylinder / Cone。
- 新建 primitive 立刻出现在 viewport 中，并能被选中、移动、旋转、缩放。
- 保存 / 加载场景后 primitive 能恢复。
- 新建 primitive 走普通 MeshRenderer 渲染路径，不引入 renderer 特判。
- Debug 构建通过，SceneSerializer smoke 覆盖 primitive component round-trip。

### PR-EM1：Material Slot Override Foundation

目标：

- `MeshRendererComponent` 增加 `material_overrides`。
- SceneSerializer 保存 / 读取 material overrides。
- RenderData / RenderSceneFrame 传递 overrides。
- Vulkan draw list 支持 override material handle。
- Resource cache 支持独立 material asset GPU resource。

验收：

- 默认材质不变。
- 给某个实体 slot 绑定 material override 后，只影响该实体。
- 场景保存 / 加载后 override 仍存在。
- Debug 构建通过，SceneSerializer smoke 覆盖 material override。

### PR-EM2：Runtime Material Instance Editing

目标：

- `MaterialAsset` 支持复制和基础 setter。
- `AssetManager` 支持 `createRuntimeMaterialInstance`。
- Inspector 增加 `Materials` foldout。
- 支持 `Make Instance` / `Use Default`。
- 支持编辑 base color、metallic、roughness、emissive、normal scale、AO strength。
- Renderer 能在参数变化后刷新 GPU material。

验收：

- 选中物体后能实时调材质颜色、金属度、粗糙度。
- 修改 runtime instance 不影响同模型的其他实体。
- SSR 测试墙可以通过 Inspector 调成镜面材质。

### PR-EM3：Texture Slot Editing

目标：

- Inspector 支持贴图槽显示、导入、清除。
- Base Color / Emissive 使用 sRGB，其余 PBR texture 使用 Linear。
- 支持 packed glTF metallic-roughness 和 separate metallic / roughness 的切换。

验收：

- 能给 runtime material 替换 albedo / normal / roughness 等贴图。
- glTF packed MR 材质显示和渲染正确。
- MaterialAsset smoke 覆盖 texture slot round-trip。

### PR-EM4：Persistent Material Asset `.nxmat`

目标：

- 新增 `.nxmat` JSON 格式。
- `AssetManager` 支持 import / load / save material asset。
- `Save As Material` 把 runtime material 保存成持久化资产。
- Scene material overrides 优先保存 `.nxmat` asset reference。

验收：

- 关闭 / 重启编辑器后，材质参数能从 `.nxmat` 复原。
- 多个实体引用同一个 `.nxmat` 时共享材质；`Make Instance` 可打破共享。

### PR-EM5：Material UX Polish

目标：

- Asset picker / drag-drop。
- 材质预览缩略图或小预览球。
- 多选编辑相同 slot。
- Undo / Redo 接入。
- Project 面板材质创建入口。

验收：

- 常见材质调试流程不需要手写代码。
- 用户能清楚区分 Default、Override、Runtime Instance、Persistent Material。

## 12. 推荐落地顺序

建议顺序：

```text
PR-EP1 Editor Procedural Primitive Creation
PR-EM1 Material Slot Override Foundation
PR-EM2 Runtime Material Instance Editing
PR-EM3 Texture Slot Editing
PR-EM4 Persistent Material Asset (.nxmat)
PR-EM5 Material UX Polish
```

如果要最快服务 SSR 调试，可以把 EM1 + EM2 作为最小闭环：

```text
选中镜面墙
  -> Materials / Slot 0
  -> Make Instance
  -> Metallic = 1.0
  -> Roughness = 0.02
  -> 观察 SSR Surface Mask / Hit Mask / Raw Reflection / Final Lit
```

这条路径能立刻减少为了测试材质而改 Sandbox 代码的需求。

## 13. 风险与注意事项

- 材质 override 需要处理 mesh 数量变化：模型重新导入后 slot 数可能改变，缺失 slot 必须安全回退默认材质。
- Runtime material 如果没有 `.nxmat` 持久化，场景跨进程复原能力有限；UI 需要明确标识 `Runtime Instance`。
- GPU material 更新不要每帧无条件重建，应通过 generation / dirty flag 控制。
- 透明材质不应只依赖 `MeshRendererComponent::is_transparent`，后续应从 material alpha mode 参与 opaque / transparent list 分流。
- 多实体共享同一个 material asset 时，编辑行为必须明确是“编辑共享资产”还是“创建实例”。
- 贴图导入必须正确处理 color space，否则 PBR 和 SSR 都会出现难以定位的亮度问题。

## 14. 测试建议

必要测试：

- `SceneSerializerSmoke`：MeshRenderer material overrides 保存 / 加载。
- `SceneSerializerSmoke`：Procedural primitive type / segment 参数保存 / 加载。
- `MaterialAssetSmoke`：runtime material copy、setter、texture handle、factor round-trip。
- `RenderSettingsSmoke` 不需要为材质编辑扩展，除非新增 debug setting。
- Sandbox 短启动 smoke：确认 editor / renderer 能加载 override material。

建议增加手动测试场景：

- 一个模型多实体共享默认材质。
- 其中一个实体 `Make Instance` 后改成红色高金属低粗糙度。
- 另一个实体保持默认材质不变。
- 保存场景，重载后 override 仍只影响目标实体。
- 在空场景中新建 Cube / Sphere / Cone，用 Transform gizmo 移动后保存并重载。
