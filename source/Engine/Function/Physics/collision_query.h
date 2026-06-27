#pragma once

#include "Core/Base.h"
#include "Function/Physics/physics_types.h"

namespace NexAur::CollisionQuery {
    NEXAUR_API bool passesFilter(const WorldCollider& lhs, const WorldCollider& rhs);
    NEXAUR_API bool overlaps(const WorldCollider& lhs, const WorldCollider& rhs);
} // namespace NexAur::CollisionQuery
