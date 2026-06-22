#pragma once
#include <filesystem>
#include <string>

namespace NexAur {
    class NEXAUR_API FileSystem {
    public:
        FileSystem(const std::filesystem::path& root_dir);
        ~FileSystem() = default;

        // 获取资源的绝对物理路径
        std::filesystem::path getAssetPath(const std::string& relative_path) const;

        // 获取当前的引擎根目录
        std::filesystem::path getEngineRoot() const { return m_engine_root; }

    private:
        std::filesystem::path m_engine_root; // 引擎的物理根目录
    };
} // namespace NexAur

// 资源路径解析是很多底层模块都会用到的轻量能力，不应该要求 include 全局上下文。
#ifndef ENGINE_ROOT_DIR
#define ENGINE_ROOT_DIR "."
#endif

#ifndef NX_ASSET
#if defined(NDEBUG) || defined(NX_DIST)
    #define NX_ASSET(relative_path) (std::string(relative_path))
#else
    #define NX_ASSET(relative_path) ((std::filesystem::path(ENGINE_ROOT_DIR) / (relative_path)).string())
#endif
#endif
