# NexAur 资产清单

本清单记录当前会被主线或 Sandbox 使用的资产边界。新增资源时优先更新这里，避免旧 demo 资源重新混入运行时路径。

## Runtime 必需

这些资产由 Engine/Renderer 主线直接加载，缺失会影响当前场景渲染：

- `assets/shaders/pbr/`
- `assets/shaders/shadow/`
- `assets/shaders/skybox/`
- `assets/textures/HDR/warm_restaurant_8k.hdr`

## Sandbox 必需

这些资产用于 Sandbox 的 PBR 材质测试：

- `assets/textures/PBR/gold/`
- `assets/textures/PBR/rusted_iron/`
- `assets/textures/PBR/grass/`
- `assets/textures/PBR/plastic/`
- `assets/textures/PBR/wall/`

## Sandbox 可选

- `assets/models/DamagedHelmet/`

`assets/models/` 当前在 `.gitignore` 中保持忽略，避免把较大的本地样例模型和版权不明资产直接纳入仓库。Sandbox 会在模型缺失时自动跳过该样例；如果需要测试模型加载，请把 `DamagedHelmet.gltf` 及其同目录贴图放回：

```text
assets/models/DamagedHelmet/DamagedHelmet.gltf
```

## 已清理的 legacy/demo 资产

以下资源只服务旧 `Scene` demo 或未被主线引用，已从仓库中移除：

- `assets/shaders/container.*`
- `assets/shaders/phong/`
- `assets/shaders/floor/`
- `assets/shaders/lightCube/`
- `assets/textures/container/`
- `assets/textures/wood/`
- `assets/textures/skybox/`
- `assets/textures/HDR/bryanston_park_sunrise_4k.hdr`
- `assets/textures/HDR/qwantani_dusk_2_puresky_8k.hdr`

## 维护规则

- Runtime 代码不要直接依赖 `assets/models/` 下的忽略文件，除非调用方能优雅处理缺失。
- HDR 环境图默认只保留一个主线样例；额外 HDR 建议放在本地素材库或单独下载包。
- 如果后续需要恢复旧 demo，可新建明确的 sample/legacy 包，不要把它混回 Engine 主线资产目录。
