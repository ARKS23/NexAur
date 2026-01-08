# 第三方库总控CMakeLists.txt

# GLAD
add_subdirectory(glad)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE) #以此为例，关闭不必要的文档编译，加快速度
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw)

# GLM
add_subdirectory(glm)

# SPDLOG
add_subdirectory(spdlog)

# EnTT
add_subdirectory(entt)