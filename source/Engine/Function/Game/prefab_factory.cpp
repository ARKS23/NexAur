#include "pch.h"
#include "prefab_factory.h"

#include "Function/Scene/collision_component.h"
#include "Function/Scene/component.h"
#include "Function/Scene/gameplay_component.h"
#include "Function/Scene/scene_v2.h"

namespace NexAur {
    namespace {
        void applyTransform(Entity entity, const SpawnTransform& transform_desc) {
            TransformComponent& transform = entity.getComponent<TransformComponent>();
            transform.translation = transform_desc.position;
            transform.rotation = transform_desc.rotation;
            transform.scale = transform_desc.scale;
        }

        void applyOptionalMesh(Entity entity, AssetHandle model_asset) {
            if (model_asset) {
                entity.addComponent<MeshRendererComponent>(model_asset);
            }
        }

        void applyHealth(Entity entity, int health_value) {
            HealthComponent& health = entity.addComponent<HealthComponent>();
            health.current = health_value;
            health.max = health_value;
        }

        void applySphereCollider(Entity entity, float radius) {
            SphereColliderComponent& collider = entity.addComponent<SphereColliderComponent>();
            collider.radius = radius;
            entity.addComponent<CollisionFilterComponent>();
        }
    } // namespace

    Entity PrefabFactory::createPlayer(SceneV2& scene, const PlayerSpawnDesc& desc) {
        Entity entity = scene.createEntity(desc.name);
        applyTransform(entity, desc.transform);

        PlayerComponent& player = entity.addComponent<PlayerComponent>();
        player.move_speed = desc.move_speed;
        entity.addComponent<VelocityComponent>();
        applyHealth(entity, desc.health);
        applySphereCollider(entity, desc.collider_radius);
        applyOptionalMesh(entity, desc.model_asset);

        return entity;
    }

    Entity PrefabFactory::createEnemy(SceneV2& scene, const EnemySpawnDesc& desc) {
        Entity entity = scene.createEntity(desc.name);
        applyTransform(entity, desc.transform);

        EnemyComponent& enemy = entity.addComponent<EnemyComponent>();
        enemy.move_speed = desc.move_speed;
        entity.addComponent<VelocityComponent>();
        applyHealth(entity, desc.health);
        applySphereCollider(entity, desc.collider_radius);
        applyOptionalMesh(entity, desc.model_asset);

        return entity;
    }

    Entity PrefabFactory::createCollectible(SceneV2& scene, const CollectibleSpawnDesc& desc) {
        Entity entity = scene.createEntity(desc.name);
        applyTransform(entity, desc.transform);

        CollectibleComponent& collectible = entity.addComponent<CollectibleComponent>();
        collectible.score = desc.score;
        applySphereCollider(entity, desc.collider_radius);
        entity.addComponent<TriggerComponent>();
        applyOptionalMesh(entity, desc.model_asset);

        return entity;
    }

    Entity PrefabFactory::createProjectile(SceneV2& scene, const ProjectileSpawnDesc& desc) {
        Entity entity = scene.createEntity(desc.name);
        applyTransform(entity, desc.transform);

        ProjectileComponent& projectile = entity.addComponent<ProjectileComponent>();
        projectile.damage = desc.damage;
        VelocityComponent& velocity = entity.addComponent<VelocityComponent>();
        velocity.velocity = desc.velocity;
        LifetimeComponent& lifetime = entity.addComponent<LifetimeComponent>();
        lifetime.seconds = desc.lifetime_seconds;
        applySphereCollider(entity, desc.collider_radius);
        entity.addComponent<TriggerComponent>();
        applyOptionalMesh(entity, desc.model_asset);

        return entity;
    }
} // namespace NexAur
