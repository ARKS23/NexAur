cmake_minimum_required(VERSION 3.10)
project(imgui)

# 一些需要的核心文件
set(IMGUI_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_demo.cpp # 保留demo，学习如何使用ImGui
    ImGuizmo.cpp
)

# GLFW + OpenGL3 后端
set(IMGUI_BACKENDS_SOURCES
    backends/imgui_impl_glfw.cpp
    backends/imgui_impl_opengl3.cpp
)

# 创建静态库
add_library(imgui STATIC ${IMGUI_SOURCES} ${IMGUI_BACKENDS_SOURCES})

# 包含头文件目录
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# 链接图形API依赖
target_link_libraries(imgui PRIVATE glfw glad)