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
