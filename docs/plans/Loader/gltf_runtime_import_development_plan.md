# NexAur glTF / GLB Runtime Import Development Plan

日期：2026-06-29
分支：`vulkanRenderer`

## 1. 文档定位

本文档记录 NexAur 后续将 `tinygltf` 接入为 glTF 2.0 / GLB 主导入路径的开发计划。

目标不是简单替换一个第三方库，而是把模型导入链路整理成更清楚的资产管线：

```text
tinygltf / Assimp / future importers
  -> NexAur importer abstraction
  -> backend-neutral imported asset data
  -> AssetManager / CPU assets
  -> Renderer resource cache
```

后续原则：

- `tinygltf` 作为 runtime glTF 2.0 / GLB 主路径。
- Assimp 退到补充导入器、Editor/offline conversion tool 或 legacy fallback。
- Runtime / Renderer / Scene 不直接依赖 `tinygltf` 或 Assimp 类型。
- 导入结果必须进入 NexAur 自己的 `Model` / `MaterialAsset` / `TextureAsset` / scene data contract。

## 2. 背景和动机

当前模型导入主要依赖 Assimp。Assimp 的优势是格式覆盖广，但作为 runtime glTF 主路径有几个问题：

- 它会把 glTF 材质、贴图语义和节点结构转换成通用模型抽象，很多 glTF-specific 信息需要反查或猜测。
- PBR metallic-roughness、packed MR、normal scale、occlusion strength、emissive、alpha mode 等语义容易出现边界不清。
- tangent / bitangent / handedness 与 glTF spec 的对应关系不够直接。
- 对 DamagedHelmet 这类标准 glTF PBR 样例，调试时难以判断问题来自资产、导入器、材质、IBL 还是 shader。

`tinygltf` 更贴近 glTF 2.0 spec，适合作为 NexAur 的 glTF 主导入器。但它只是 parser，不是完整引擎资产管线。NexAur 仍然需要自己负责：

- Accessor / bufferView 读取策略。
- 材质默认值和颜色空间。
- texture decode / import / cache。
- tangent fallback。
- node transform 和 scene graph policy。
- extension 支持边界。
- smoke / regression 测试。

## 3. 总体设计原则

### 3.1 tinygltf 类型不向外泄漏

`tinygltf::Model`、`tinygltf::Accessor`、`tinygltf::Material` 等类型只允许出现在 glTF importer 内部。

推荐结构：

```text
source/Engine/Function/Resource/Import/
  asset_importer.h
  model_import_data.h
  gltf/
    tiny_gltf_importer.h
    tiny_gltf_importer.cpp
    tiny_gltf_accessor_reader.h
    tiny_gltf_accessor_reader.cpp
```

外部只看到 NexAur 自己的数据：

```text
ImportedModelData
ImportedMeshData
ImportedPrimitiveData
ImportedMaterialData
ImportedTextureData
ImportedSceneNode
```

第一版可以先桥接到现有 `Model` / `Mesh` / `MaterialImportData`，但接口设计上要避免未来支持 animation / skinning 时大改。

### 3.2 Runtime 主路径只做 glTF / GLB

runtime 默认只依赖 tinygltf glTF importer：

```text
AssetManager::importModelAsset()
  -> ModelImporterRegistry
  -> TinyGltfImporter for .gltf / .glb
```

Assimp 不再作为 glTF runtime 主路径。

Assimp 后续定位：

- FBX / OBJ / DAE 等非 glTF 格式的补充导入器。
- Editor import tool。
- Offline converter，例如 `.fbx -> .glb` 或 `.fbx -> NexAur internal asset`。
- Legacy fallback，必要时通过显式设置启用。

### 3.3 Import 和 GPU resource 创建分离

导入器只生成 CPU 侧资产数据，不创建 Vulkan resource。

```text
TinyGltfImporter
  -> CPU Model / Material / Texture identity
  -> AssetManager cache
  -> VulkanRenderResourceCache on demand
```

这样可以保持 Renderer backend-neutral，也方便后续做 asset cache、hot reload 和 offline cooking。

## 4. glTF v1 支持范围

第一阶段建议覆盖 glTF 2.0 PBR 静态模型主链路。

必须支持：

- `.gltf`。
- `.glb`。
- 外部 `.bin` buffer。
- embedded buffer / Data URI。
- bufferView image。
- 外部 PNG / JPEG texture。
- single scene / default scene。
- node transform。
- mesh 多 primitive。
- position / normal / texcoord0。
- tangent，如果 asset 提供。
- index buffer：`uint16` / `uint32` / `uint8`。
- material `pbrMetallicRoughness`。
- baseColor texture / factor。
- metallicRoughness texture / factors。
- normal texture / scale。
- occlusion texture / strength。
- emissive texture / factor。
- alpha mode / alpha cutoff。
- doubleSided。

第一阶段暂不要求：

- animation。
- skinning。
- morph target。
- multiple UV sets 的完整材质选择。
- vertex color。
- KTX2 / BasisU。
- Draco / Meshopt runtime decode。
- clearcoat / sheen / transmission / volume / anisotropy。
- punctual lights / cameras import。
- full scene prefab editing。

## 5. 数据契约

### 5.1 Geometry

导入器应把 glTF accessor 转成明确的 engine vertex stream。

第一版可以继续适配现有：

```cpp
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};
```

但 importer 内部建议使用更语义化的中间数据：

```text
ImportedPrimitiveData
  positions
  normals
  texcoords0
  tangents4       # glTF TANGENT vec4，w 是 handedness
  indices
  material_index
```

注意事项：

- Accessor 读取必须处理 `byteOffset`、bufferView `byteOffset`、`byteStride`。
- 支持 normalized attribute。
- 支持 sparse accessor，或者第一版明确检测后报错 / fallback。
- 不要假设 attribute buffer tightly packed。
- index type 要完整处理 `UNSIGNED_BYTE`、`UNSIGNED_SHORT`、`UNSIGNED_INT`。

### 5.2 Tangent Space

glTF `TANGENT` 是 `vec4`：

```text
xyz = tangent
w   = bitangent handedness
```

NexAur policy：

- 如果 glTF 提供 tangent，优先使用它。
- bitangent 用 `cross(normal, tangent.xyz) * tangent.w` 重建。
- 如果缺失 tangent，后续优先接入 `MikkTSpace` 生成。
- 第一版可以保留当前 CPU fallback，但只能作为 fallback，不应覆盖 glTF / Assimp 已生成 tangent。

这点对 DamagedHelmet、normal map 和金属高光非常关键。

阶段策略：

- PR-L1 只预留 tangent generation policy，不正式接入 MikkTSpace。
- PR-L2 开始导入 geometry 后，可以加入 `GenerateIfMissing` 的接口和测试样例。
- PR-L3 接入 normal map / material debug 后，最适合正式切换到 MikkTSpace，因为此时能用 normal debug view 和 DamagedHelmet 画面对比验证结果。
- 不建议在没有材质和 normal debug 的情况下提前替换 tangent 生成逻辑，否则很难判断 visual regression 的来源。

### 5.3 Material

glTF metallic-roughness 默认值：

```text
baseColorFactor = vec4(1, 1, 1, 1)
metallicFactor  = 1.0
roughnessFactor = 1.0
emissiveFactor  = vec3(0, 0, 0)
normalScale     = 1.0
occlusionStrength = 1.0
alphaMode = OPAQUE
alphaCutoff = 0.5
doubleSided = false
```

NexAur `MaterialImportData` 应按 glTF spec 填入，而不是依赖当前默认值猜测。

贴图通道约定：

```text
baseColorTexture          -> sRGB
emissiveTexture           -> sRGB
normalTexture             -> linear
metallicRoughnessTexture  -> linear, G = roughness, B = metallic
occlusionTexture          -> linear, R = AO
```

需要保留：

- texture index。
- sampler wrap / filter。
- texcoord set index。
- normal scale。
- occlusion strength。
- alpha mode / cutoff。
- doubleSided。

第一版如果暂不实现 sampler / texcoord set，可以明确使用默认 sampler，并在 TODO 中保留入口。

### 5.4 Texture

glTF texture 来源可能是：

- 外部文件。
- Data URI。
- GLB bufferView image。

因此 `TextureLoader` 需要新增内存图片解码入口：

```text
TextureLoader::load2DFromMemory(bytes, color_space, debug_name)
```

建议不要让 tinygltf 自己直接把 image decode 结果塞进 renderer。更清楚的做法是：

- tinygltf 负责解析 image URI / bufferView。
- NexAur 负责根据 texture slot 决定 color space。
- `TextureLoader` 负责 decode 外部文件或内存 bytes。
- `AssetManager` 负责注册 texture asset identity 和 CPU cache。

第一版也可以让 tinygltf decode image，但要立刻包进 `TextureAsset`，不要把 tinygltf image 类型传到外层。

## 6. Importer 架构建议

### 6.1 Importer Interface

```cpp
class IModelImporter {
public:
    virtual ~IModelImporter() = default;
    virtual bool canImport(const std::filesystem::path& path) const = 0;
    virtual ModelImportResult importModel(const ModelImportRequest& request) = 0;
};
```

建议结果对象显式区分成功、失败和 warning：

```text
ModelImportResult
  success
  model_data
  warnings
  errors
```

导入失败不要只写 log，要给调用方可测试的错误信息。

### 6.2 Importer Registry

```text
ModelImporterRegistry
  .gltf -> TinyGltfImporter
  .glb  -> TinyGltfImporter
  .fbx  -> AssimpImporter / OfflineImporter
  .obj  -> AssimpImporter / OfflineImporter
```

runtime 默认不需要加载 Assimp importer。Editor 或工具进程可以注册更多 importer。

### 6.3 AssetManager 接入

当前：

```text
AssetManager::importModelAsset(path)
  -> Model(path)
  -> Assimp importer inside Model
```

建议改为：

```text
AssetManager::importModelAsset(path)
  -> ModelImporterRegistry::import(path)
  -> Model::fromImportedData()
  -> register model / material / texture handles
```

`Model` 不应继续直接拥有“从路径加载”的导入细节。长期看，`Model` 应该是 CPU asset container，不是 importer。

## 7. PR 拆分建议

### PR-L1：Importer Abstraction + tinygltf Dependency

目标：

- 接入 tinygltf 依赖。
- 新增 importer interface / registry。
- 保持现有 Assimp 路径可用。
- 不改变当前渲染结果。
- 只建立 glTF 主导入路径的工程骨架，不在本 PR 内替换现有 runtime model load 行为。

建议工作：

- 在 third-party / external 或 vcpkg 路径中接入 tinygltf。
- 只在一个 `.cpp` 中启用 tinygltf implementation，避免多 TU 宏污染；不要在 `pch.h` 或公共头里打开 implementation 宏。
- 新增 `IModelImporter` / `ModelImporterRegistry`。
- 新增 `ModelImportRequest` / `ModelImportResult`，至少包含 success、warnings、errors 和可选 debug metadata。
- 新增 `TinyGltfImporter` metadata parse 实现：
  - 支持识别 `.gltf` / `.glb`。
  - 能调用 tinygltf 解析文件。
  - 能返回 scene / mesh / material / image / texture 数量。
  - 能收集 tinygltf warnings / errors。
- `AssetManager` 预留 importer registry 入口，但第一版可仍 fallback 到现有 `Model(path)`。
- 对 `.gltf` / `.glb` 的主路径切换加显式开关或内部 TODO，避免 PR-L1 悄悄改变运行结果。
- 预留 tangent generation policy：
  - `PreserveImportedTangents`。
  - `GenerateIfMissing`。
  - `None`。
  - PR-L1 只定义策略和接口，不实现 MikkTSpace。

暂时不做：

- 不读取 vertex / index accessor。
- 不转换 tinygltf geometry 到 `Model` / `Mesh`。
- 不导入 material / texture。
- 不实现 `TextureLoader::load2DFromMemory()`。
- 不替换 Assimp glTF runtime load path。
- 不正式接入 MikkTSpace。
- 不改变 DamagedHelmet 或 Sandbox 当前渲染结果。

验收：

- 构建通过。
- 未改变当前 Sandbox 渲染。
- `.gltf` / `.glb` 路径能被 registry 识别。
- DamagedHelmet glTF 能通过 tinygltf metadata parse smoke，返回 mesh / material / image 数量。
- tinygltf warning / error 能进入 `ModelImportResult`，调用方可以测试。
- Renderer / Scene / Runtime 代码不 include tinygltf 头文件。
- Assimp 路径仍可按现状运行。

实现状态：

- 已完成：`vcpkg.json` 已包含 `tinygltf`，Engine CMake 通过 `find_path(NEXAUR_TINYGLTF_INCLUDE_DIR tiny_gltf.h REQUIRED)` 接入头文件依赖。
- 已完成：新增 `Function/Resource/Import`，包含 `ModelImportRequest`、`ModelImportResult`、`ModelImportMetadata`、`IModelImporter` 和 `ModelImporterRegistry`。
- 已完成：新增 `TinyGltfImporter`，只在 `tiny_gltf_importer.cpp` 中启用 `TINYGLTF_IMPLEMENTATION`，公共头不泄漏 tinygltf 类型。
- 已完成：`TinyGltfImporter` 支持 `.gltf` / `.glb` metadata parse，返回 scene / node / mesh / primitive / material / texture / image / sampler / animation / skin 数量，并收集 warning / error。
- 已完成：`AssetManager` 初始化 runtime importer registry，并提供 `inspectModelImportMetadata()` 调试入口；当前 `importModelAsset()` 仍保持旧 Assimp 路径，不改变 Sandbox 渲染结果。
- 已完成：新增 `TinyGltfMetadataSmoke`，验证 DamagedHelmet 可通过 tinygltf metadata parse。
- 测试：`cmake --build --preset msvc-vcpkg-debug` 通过；`ctest --test-dir build\msvc-vcpkg -C Debug --output-on-failure` 13/13 通过。

### PR-L2：glTF Geometry / Accessor / GLB Baseline

目标：

- 用 tinygltf 导入最小静态 mesh。
- 支持 `.gltf` / `.glb`。
- 支持 positions / normals / texcoords / indices。

建议工作：

- 实现 accessor reader。
- 支持 bufferView stride / offsets。
- 支持 index type。
- 支持 single mesh / single primitive。
- 将 tinygltf 导入结果转换为现有 `Model` / `Mesh`。

验收：

- DamagedHelmet mesh 顶点数 / index 数正确。
- 简单 GLB cube 可导入并渲染。
- Assimp 路径仍可 fallback。

实现状态：

- 已完成：`ModelImportResult` 增加可选 CPU `Model`，`ModelImportMetadata` 增加 `vertex_count` / `index_count`，用于 geometry smoke 和后续调试。
- 已完成：`TinyGltfImporter` 在 `FullModel` 模式下读取 glTF / GLB geometry，并转换为现有 `Model` / `Mesh` / `Vertex`。
- 已完成：accessor reader 支持 bufferView `byteOffset`、accessor `byteOffset`、`byteStride` 和范围校验；当前明确拒绝 sparse accessor。
- 已完成：支持 `POSITION` / `NORMAL` / `TEXCOORD_0` / `TANGENT`；缺失 normal 时生成基础 normal，`GenerateIfMissing` 策略下生成基础 tangent。
- 已完成：index accessor 支持 `UNSIGNED_BYTE` / `UNSIGNED_SHORT` / `UNSIGNED_INT`；无 indices 时生成顺序索引。
- 已完成：`AssetManager::importModelAssetFromRegistry()` 提供显式 importer registry 导入入口，并使用独立 cache key，避免与当前 Assimp path identity 混用。
- 已完成：新增 `TinyGltfGeometrySmoke`，动态写入一个 interleaved vertex buffer + uint16 index 的最小 cube GLB，并验证 vertex / normal / uv / index 导入；DamagedHelmet 存在时额外验证 tinygltf geometry 计数。
- 边界：默认 `importModelAsset()` 仍保持现有 Assimp 路径，避免 PR-L3 完成前让 DamagedHelmet 默认材质/贴图退化。
- 测试：`cmake --build --preset msvc-vcpkg-debug` 通过；`ctest --test-dir build\msvc-vcpkg -C Debug --output-on-failure` 14/14 通过。

### PR-L3：glTF PBR Material / Texture Pipeline

目标：

- 用 tinygltf 正确导入 glTF metallic-roughness material。
- 支持外部图片、Data URI、bufferView image。
- 按 slot 注册 sRGB / linear。

建议工作：

- 扩展 `TextureLoader::load2DFromMemory()`。
- `MaterialImportData` 按 glTF spec 默认值填充。
- 支持 baseColor / normal / metallicRoughness / AO / emissive。
- 支持 alpha mode / cutoff / doubleSided。
- 支持 sampler wrap/filter 的最小映射，或者明确第一版 fallback。

验收：

- DamagedHelmet 的五张贴图全部通过 tinygltf 路径进入 `MaterialAsset`。
- Material debug view 中 baseColor / normal / metallic / roughness / AO / emissive 正确。
- packed MR 的 G/B 通道正确。
- baseColor / emissive 为 sRGB，其余为 linear。

实现状态：

- 已完成：`TextureLoader::load2DFromMemory()`，用于 Data URI / GLB bufferView image 的 PNG / JPEG 等内存图片解码。
- 已完成：`MaterialImportData` 增加可选 CPU texture asset 和 `double_sided`，保留现有 path 字段以兼容外部文件贴图。
- 已完成：`TinyGltfImporter` 导入 baseColor / metallicRoughness / normal / occlusion / emissive texture slot，并按 slot 固定 sRGB / linear。
- 已完成：`TinyGltfImporter` 导入 glTF PBR factors、normal scale、occlusion strength、emissive factor、alpha mode / cutoff、doubleSided。
- 已完成：`AssetManager::registerRuntimeTexture()`，让 embedded / Data URI texture 可进入现有 CPU texture cache 和 Vulkan texture resource 路径。
- 已完成：新增 `TinyGltfMaterialSmoke`，动态写入 embedded image GLB 和 Data URI glTF，验证 material fields、runtime texture 注册、color space 和 DamagedHelmet tinygltf 外部贴图路径。
- 边界：sampler wrap/filter 暂时仍使用当前 Vulkan 默认 sampler；texCoord set 仍按 `TEXCOORD_0` 处理，后续在 PR-L4 / material refinement 中展开。
- 测试：`cmake --build --preset msvc-vcpkg-debug` 通过；`ctest --test-dir build\msvc-vcpkg -C Debug --output-on-failure` 15/15 通过。

### PR-L4：Node Transform / Multi-Primitive / Scene Import

目标：

- 支持 glTF node hierarchy 的 transform。
- 支持 mesh 多 primitive。
- 支持 default scene。

建议工作：

- 导入 node TRS / matrix。
- 第一版可继续 bake node transform 到 mesh vertices。
- 或者输出 `ImportedSceneNode`，由 Scene 层生成实体。
- 多 primitive 保留各自 material index。

验收：

- 多 primitive glTF 能正确拆分 draw item / material。
- node transform 正确。
- 当前 `SceneTestClass::addModelEntity()` 行为保持稳定。

### PR-L5：DamagedHelmet Reference Regression

目标：

- 将 DamagedHelmet 作为 tinygltf 主路径的回归样例。
- 和 PR-R30.6 的 PBR / IBL reference alignment 联动。

建议工作：

- 新增 tinygltf import smoke。
- 检查 DamagedHelmet material paths / image names / factors / alpha mode。
- 检查 tangent 使用策略。
- 检查 material debug channels。
- 记录 reference preset 截图参数。

验收：

- tinygltf 路径下 DamagedHelmet 观感接近 externalRenderer 参考。
- Assimp fallback 和 tinygltf 主路径不会混用同一 asset identity 导致 cache 污染。
- CTest / Sandbox smoke 通过。

### PR-L6：Assimp Fallback / Offline Converter Boundary

目标：

- 明确 Assimp 不再是 runtime glTF 主路径。
- 保留非 glTF import 能力。
- 为 Editor/offline converter 做边界。

建议工作：

- 新增 `AssimpModelImporter`，只通过 importer registry 显式注册。
- runtime build 可选择不注册 Assimp。
- Editor/tool build 注册 Assimp importer。
- 文档明确 `.fbx/.obj/.dae` 的推荐路径是 offline convert。

验收：

- `.gltf/.glb` 默认走 tinygltf。
- 非 glTF 可以通过 Assimp fallback 或工具导入。
- Renderer / Scene / Runtime 不 include Assimp 头文件。

## 8. 测试样例矩阵

建议至少准备：

```text
DamagedHelmet.gltf
DamagedHelmet.glb
SimpleCube.gltf
SimpleCube.glb
EmbeddedTexture.glb
ExternalTexture.gltf
MultiplePrimitives.gltf
NodeTransform.gltf
AlphaMask.gltf
DoubleSided.gltf
```

每个样例覆盖：

- import success / warnings。
- vertex / index count。
- material factors。
- texture slot。
- texture color space。
- render smoke。

## 9. 风险和注意事项

- tinygltf 是 parser，不是完整 asset pipeline；不要把 importer 简化成“读完 tinygltf::Model 就结束”。
- GLB bufferView image 是必须支持的，否则很多真实资产会缺贴图。
- Accessor stride / sparse / normalized 是容易出 bug 的地方。
- Tangent space 会直接影响 normal map 和 PBR 高光，不能粗糙处理。
- glTF material 默认值要按 spec 明确填充，不能沿用非 glTF 的默认材质假设。
- texture cache key 要区分外部 path、GLB embedded image 和 Data URI，否则可能出现不同资产贴图串 cache。
- Assimp fallback 不应悄悄接管 glTF，否则 debug 时很难知道当前用了哪条路径。

## 10. 完成标准

这一条 Loader 阶段达到以下状态即可认为主线完成：

- `.gltf` / `.glb` 默认走 tinygltf。
- DamagedHelmet 通过 tinygltf 导入并正确渲染。
- glTF PBR material / texture / color space 链路稳定。
- GLB embedded texture 可用。
- node transform / multi primitive 可用。
- Assimp 从 runtime glTF 主路径移除，变为补充导入器或离线工具。
- Importer 不向 Renderer / Scene 泄漏 tinygltf / Assimp 类型。
- 构建、smoke / CTest 稳定通过。
