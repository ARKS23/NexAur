#include "pch.h"
#include "model.h"

#include <utility>

namespace NexAur {
    Model::Model(std::vector<Mesh> meshes, std::string debug_name)
        : m_meshes(std::move(meshes)), m_directory(std::move(debug_name)), m_is_loaded(!m_meshes.empty()) {}
} // namespace NexAur
