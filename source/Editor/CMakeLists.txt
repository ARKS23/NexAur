project(NexAurEditor)

# 引擎的源代码和头文件
file(GLOB_RECURSE editor_sources
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_executable(NexAurEditor ${editor_sources})

target_include_directories(NexAurEditor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 链接引擎库
target_link_libraries(NexAurEditor PRIVATE NexAurEngine)