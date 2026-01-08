# Engine MakeLists
project(NexAurEngine)

# 动态库和静态库选项
option(NEXAUR_BUILD_SHARED "Build engine as shared library" ON)

file(GLOB_RECURSE engine_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# 创建对应的库类型
if (NEXAUR_BUILD_SHARED)
    add_library(NexAurEngine SHARED ${engine_sources})
    target_compile_definitions(NexAurEngine PUBLIC NEXAUR_DYNAMIC_LINK) # 定义宏,Base中会用到
    target_compile_definitions(NexAurEngine PRIVATE NX_BUILD_DLL)
    message(STATUS "NexAur Engine: Building SHARED Library (DLL)")
else()
    add_library(NexAurEngine STATIC ${engine_sources})
    target_compile_definitions(NexAurEngine PUBLIC NEXAUR_STATIC_LINK)
    message(STATUS "NexAur Engine: Building STATIC Library (LIB)")
endif()

# 预编译头
target_precompile_headers(NexAurEngine PRIVATE pch.h)

# 设置头文件路径
target_include_directories(NexAurEngine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/external/entt/src"
    "${CMAKE_SOURCE_DIR}/external/glm"
    "${CMAKE_SOURCE_DIR}/external/spdlog/include"
    "${CMAKE_SOURCE_DIR}/external/glfw/include"
    "${CMAKE_SOURCE_DIR}/external/glad/include"
)

# 链接第三方库
target_link_libraries(NexAurEngine PUBLIC
    EnTT
    glad
    glfw
    glm
    spdlog
)