# 全局总控CMakeLists.txt

cmake_minimum_required(VERSION 3.25)
project(NexAur VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(NEXAUR_USE_VCPKG_DEPS_DEFAULT OFF)
if(DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(NEXAUR_USE_VCPKG_DEPS_DEFAULT ON)
endif()

option(NEXAUR_USE_VCPKG_DEPS "Use vcpkg packages for third-party dependencies." ${NEXAUR_USE_VCPKG_DEPS_DEFAULT})
option(NEXAUR_BUILD_LEGACY_OPENGL "Build legacy OpenGL renderer." ON)
option(NEXAUR_BUILD_RENDERER_V2 "Build RendererV2 integration." OFF)

if(NEXAUR_BUILD_RENDERER_V2)
    message(WARNING "NEXAUR_BUILD_RENDERER_V2 is reserved for the later RendererV2 integration stage.")
endif()

if(NOT NEXAUR_BUILD_LEGACY_OPENGL)
    message(FATAL_ERROR "NEXAUR_BUILD_LEGACY_OPENGL=OFF is not supported until RendererV2 replaces the legacy OpenGL path.")
endif()

# 编译输出目录。默认保持旧路径；preset 可覆盖 NEXAUR_OUTPUT_ROOT 来隔离不同依赖来源的产物。
if(CMAKE_CONFIGURATION_TYPES OR NOT CMAKE_BUILD_TYPE)
    set(NEXAUR_OUTPUT_ROOT_DEFAULT "${CMAKE_SOURCE_DIR}/bin")
else()
    set(NEXAUR_OUTPUT_ROOT_DEFAULT "${CMAKE_SOURCE_DIR}/bin/${CMAKE_BUILD_TYPE}")
endif()
set(NEXAUR_OUTPUT_ROOT "${NEXAUR_OUTPUT_ROOT_DEFAULT}" CACHE PATH "Root directory for NexAur build outputs.")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${NEXAUR_OUTPUT_ROOT}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${NEXAUR_OUTPUT_ROOT}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${NEXAUR_OUTPUT_ROOT}")

# 项目路径宏
add_compile_definitions(ENGINE_ROOT_DIR="${CMAKE_SOURCE_DIR}")

# 子模块: 先编译依赖模块
if(NEXAUR_USE_VCPKG_DEPS)
    message(STATUS "NexAur: using vcpkg dependencies")
else()
    message(STATUS "NexAur: using vendored external dependencies")
    add_subdirectory(external)
endif()
add_subdirectory(source)

