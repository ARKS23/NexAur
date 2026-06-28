#pragma once

#include <string>

#include <glm/glm.hpp>

#include "Core/Base.h"
#include "Function/Resource/asset_handle.h"
#include "Function/Scene/entity.h"

namespace NexAur {
    class SceneV2;

    struct SpawnTransform {
        glm::vec3 position{ 0.0f };
        glm::vec3 rotation{ 0.0f };
        glm::vec3 scale{ 1.0f };
    };

    struct PlayerSpawnDesc {
        std::string name = "Player";
        SpawnTransform transform;
        float move_speed = 5.0f;
        int health = 3;
        float collider_radius = 0.5f;
        AssetHandle model_asset;
    };

    struct EnemySpawnDesc {
        std::string name = "Enemy";
        SpawnTransform transform;
        float move_speed = 2.0f;
        int health = 1;
        float collider_radius = 0.5f;
        AssetHandle model_asset;
    };

    struct CollectibleSpawnDesc {
        std::string name = "Collectible";
        SpawnTransform transform;
        int score = 1;
        float collider_radius = 0.35f;
        AssetHandle model_asset;
    };

    struct ProjectileSpawnDesc {
        std::string name = "Projectile";
        SpawnTransform transform;
        glm::vec3 velocity{ 0.0f, 0.0f, -12.0f };
        float lifetime_seconds = 3.0f;
        int damage = 1;
        float collider_radius = 0.2f;
        AssetHandle model_asset;
    };

    class NEXAUR_API PrefabFactory final {
    public:
        static Entity createPlayer(SceneV2& scene, const PlayerSpawnDesc& desc = {});
        static Entity createEnemy(SceneV2& scene, const EnemySpawnDesc& desc = {});
        static Entity createCollectible(SceneV2& scene, const CollectibleSpawnDesc& desc = {});
        static Entity createProjectile(SceneV2& scene, const ProjectileSpawnDesc& desc = {});
    };
} // namespace NexAur
