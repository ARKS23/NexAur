project(Sandbox)

file(GLOB_RECURSE sandbox_sources 
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

# 生成可执行文件
add_executable(Sandbox ${sandbox_sources})

# 链接引擎
target_link_libraries(Sandbox PRIVATE NexAurEngine)

# 设置工作目录
set_target_properties(Sandbox PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")