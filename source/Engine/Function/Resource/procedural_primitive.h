#pragma once

#include <cstdint>
#include <string_view>

namespace NexAur {
    enum class ProceduralPrimitiveType : uint8_t {
        Cube = 0,
        Sphere,
        Plane,
        Cylinder,
        Cone
    };

    inline const char* proceduralPrimitiveTypeToString(ProceduralPrimitiveType type) {
        switch (type) {
        case ProceduralPrimitiveType::Sphere:
            return "Sphere";
        case ProceduralPrimitiveType::Plane:
            return "Plane";
        case ProceduralPrimitiveType::Cylinder:
            return "Cylinder";
        case ProceduralPrimitiveType::Cone:
            return "Cone";
        case ProceduralPrimitiveType::Cube:
        default:
            return "Cube";
        }
    }

    inline ProceduralPrimitiveType proceduralPrimitiveTypeFromString(
        std::string_view value,
        ProceduralPrimitiveType fallback = ProceduralPrimitiveType::Cube) {
        if (value == "Cube") return ProceduralPrimitiveType::Cube;
        if (value == "Sphere") return ProceduralPrimitiveType::Sphere;
        if (value == "Plane") return ProceduralPrimitiveType::Plane;
        if (value == "Cylinder") return ProceduralPrimitiveType::Cylinder;
        if (value == "Cone") return ProceduralPrimitiveType::Cone;
        return fallback;
    }
} // namespace NexAur
