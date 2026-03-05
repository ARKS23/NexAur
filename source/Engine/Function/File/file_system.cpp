#include "pch.h"
#include "file_system.h"

namespace NexAur {
    FileSystem::FileSystem(const std::filesystem::path& root_dir) : m_engine_root(root_dir) { }

    std::filesystem::path FileSystem::getAssetPath(const std::string& relative_path) const {
        return m_engine_root / relative_path;
    }
} // namespace NexAur