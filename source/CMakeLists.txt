# 源码总控

add_subdirectory(Engine)
# 当前编辑器实现位于 Engine/Editor 中；旧的独立 Editor 目标为空壳，暂不参与构建。
add_subdirectory(Sandbox)
