#include "gameplay_simulation_internal.h"

#include <algorithm>

#include "engine/engine.h"

namespace game_engine {

void collisionResponse(
  GameState& state,
  GameObject& objA,
  GameObject& objB,
  const SDL_FRect& rectC,
  const GameplaySimulationHooks& hooks) {
  const auto defaultResponse = [&]() {
    if (rectC.w < rectC.h) {
      if (objA.velocity.x > 0.0f) {
        objA.position.x -= rectC.w + 0.1f;
      } else if (objA.velocity.x < 0.0f) {
        objA.position.x += rectC.w + 0.1f;
      }
      objA.velocity.x = 0.0f;
    } else {
      if (objA.velocity.y > 0.0f) {
        objA.position.y -= rectC.h;
      } else if (objA.velocity.y < 0.0f) {
        objA.position.y += rectC.h;
      }
      objA.velocity.y = 0.0f;
    }
  };

  const auto blockHorizontalPassThrough = [&]() {
    if (objA.position.x <= objB.position.x) {
      objA.position.x -= rectC.w + 0.1f;
    } else {
      objA.position.x += rectC.w + 0.1f;
    }
    objA.velocity.x = 0.0f;
  };

  const auto isMovingTowardEnemySide = [&]() {
    if (objA.velocity.x > 0.0f && objA.position.x <= objB.position.x) {
      return true;
    }
    if (objA.velocity.x < 0.0f && objA.position.x >= objB.position.x) {
      return true;
    }
    return false;
  };

  const auto shouldBlockSwingPassThrough = [&]() {
    if (!(rectC.w < rectC.h && isMovingTowardEnemySide())) {
      return false;
    }

    const SDL_FRect rectA = collisionRect(objA, objB.objClass);
    const SDL_FRect rectB = collisionRect(objB, objA.objClass);
    const float enemyMiddleTop = rectB.y + rectB.h * 0.25f;
    const float enemyMiddleBottom = rectB.y + rectB.h * 0.75f;
    const bool overlapsEnemyMiddleBand =
      rectA.y < enemyMiddleBottom && (rectA.y + rectA.h) > enemyMiddleTop;

    return overlapsEnemyMiddleBand;
  };

  if (objA.objClass == ObjectClass::Player) {
    switch (objB.objClass) {
      case ObjectClass::Level:
        if (objB.data.level.isHazard) {
          objA.position.y -= rectC.h;
          damagePlayer(objA, 50);
        } else {
          defaultResponse();
        }
        break;
      case ObjectClass::Enemy:
        if (objB.data.enemy.state != EnemyState::dead) {
          if (objA.data.player.state == PlayerState::ultimate) {
            if (isUltimateDamageActive(objA)) {
              const DamageEnemyResult result = damageEnemy(
                state,
                objB,
                50,
                objA.id,
                objA.data.player.activeUltimateCastId,
                true,
                HitStopStrength::Heavy,
                objA.direction,
                enemyKnockbackMagnitude(EnemyImpactType::Ultimate));
              if (result.applied) {
                emitHitConfirmed(
                  hooks,
                  {objA.objClass, objA.id},
                  {objB.objClass, objB.id},
                  HitStopStrength::Heavy);
              }
            }
          } else if (objA.data.player.state == PlayerState::swingWeapon) {
            const DamageEnemyResult result = damageEnemy(
              state,
              objB,
              objA.data.player.meleeDamage,
              objA.id,
              0,
              false,
              HitStopStrength::Normal,
              objA.direction,
              enemyKnockbackMagnitude(EnemyImpactType::Melee));
            if (result.applied) {
              emitHitConfirmed(
                hooks,
                {objA.objClass, objA.id},
                {objB.objClass, objB.id},
                HitStopStrength::Normal);
            }
            if (shouldBlockSwingPassThrough()) {
              blockHorizontalPassThrough();
            }
          } else {
            objA.velocity = glm::vec2(50.0f, 0.0f) * -objA.direction;
          }
        }
        break;
      case ObjectClass::Portal:
        if (hooks.onPortalTriggered && !bossBlocksPortalTransition(state)) {
          hooks.onPortalTriggered(objB.data.portal.nextLevel);
        }
        break;
      case ObjectClass::Material:
        // player colliding with item

        if (objB.data.material.state == MaterialState::present) {
          awardMaterialToPlayer(objA, objB.data.material);
          clearDynamicCollider(objB);
          objB.data.material.state = MaterialState::collapsing;
          switch (objB.data.material.type) {
            case MaterialType::coin:
              ++objA.data.player.coinPickupCueCount;
              break;
            case MaterialType::gem:
              ++objA.data.player.gemPickupCueCount;
              break;
            default:
              break;
          }
        }
        break;
      case ObjectClass::Player:
      case ObjectClass::Background:
      case ObjectClass::Projectile:
        break;
    }
  } else if (objA.objClass == ObjectClass::Projectile) {
    if (objA.data.bullet.state != BulletState::moving) {
      return;
    }

    bool passthrough = false;
    switch (objB.objClass) {
      case ObjectClass::Enemy:
        if (objB.data.enemy.state != EnemyState::dead) {
          const DamageEnemyResult result = damageEnemy(
            state,
            objB,
            10,
            objA.data.bullet.ownerPlayerId,
            0,
            false,
            HitStopStrength::Normal,
            objA.direction,
            enemyKnockbackMagnitude(EnemyImpactType::Projectile));
          if (result.applied) {
            emitHitConfirmed(
              hooks,
              {objA.objClass, objA.id},
              {objB.objClass, objB.id},
              HitStopStrength::Normal);
          }
        } else {
          passthrough = true;
        }
        break;
      case ObjectClass::Player:
        passthrough = true;
        break;
      case ObjectClass::Level:
        break;
      case ObjectClass::Portal:
      case ObjectClass::Background:
      case ObjectClass::Projectile:
      case ObjectClass::Material:
        passthrough = true;
        break;
    }

    if (!passthrough) {
      defaultResponse();
      objA.velocity = glm::vec2(0.0f);
      objA.data.bullet.state = BulletState::colliding;
      setAnimationAndPresentation(objA, ANIM_RUN, PresentationVariant::ProjectileHit);
    }
  } else if (objA.objClass == ObjectClass::Enemy) {
    switch (objB.objClass) {
      case ObjectClass::Player:
        if (objA.data.enemy.state == EnemyState::attack) {
          damagePlayer(objB, 33);
        }
        break;
      case ObjectClass::Level:
        if (objB.data.level.isHazard) {
          objA.position.y -= rectC.h;
          damageEnemy(state, objA, 50);
        } else {
          defaultResponse();
        }
        break;
      case ObjectClass::Enemy:
        if (objB.data.enemy.state != EnemyState::dead) {
          objA.velocity = glm::vec2(50.0f, 0.0f) * -objA.direction;
        }
        break;
      case ObjectClass::Portal:
      case ObjectClass::Background:
      case ObjectClass::Projectile:
      case ObjectClass::Material:
        break;
    }
  }
}

void resolveObjectCollisions(
  GameState& state,
  GameObject& obj,
  const GameplaySimulationHooks& hooks) {
  bool foundGround = false;
  for (auto& layer : state.layers) {
    for (auto& objB : layer) {
      if (&obj == &objB || objB.collider.w == 0.0f || objB.collider.h == 0.0f) {
        continue;
      }

      const SDL_FRect rectA = collisionRect(obj, objB.objClass);
      const SDL_FRect rectB = collisionRect(objB, obj.objClass);
      SDL_FRect rectC{0.0f, 0.0f, 0.0f, 0.0f};
      if (!SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC)) {
        if (objB.objClass == ObjectClass::Level) {
          const SDL_FRect physicsCollider = physicsColliderFor(obj, objB.objClass);
          SDL_FRect sensor{
            .x = obj.position.x + physicsCollider.x,
            .y = obj.position.y + physicsCollider.y + physicsCollider.h,
            .w = physicsCollider.w,
            .h = 1.0f,
          };
          SDL_FRect dummy{0.0f, 0.0f, 0.0f, 0.0f};
          if (SDL_GetRectIntersectionFloat(&sensor, &rectB, &dummy)) {
            foundGround = true;
          }
        }
        continue;
      }

      collisionResponse(state, obj, objB, rectC, hooks);

      if (objB.objClass == ObjectClass::Level) {
        const SDL_FRect physicsCollider = physicsColliderFor(obj, objB.objClass);
        SDL_FRect sensor{
          .x = obj.position.x + physicsCollider.x,
          .y = obj.position.y + physicsCollider.y + physicsCollider.h,
          .w = physicsCollider.w,
          .h = 1.0f,
        };
        SDL_FRect dummy{0.0f, 0.0f, 0.0f, 0.0f};
        if (SDL_GetRectIntersectionFloat(&sensor, &rectB, &dummy)) {
          foundGround = true;
        }
      }
    }
  }

  if (obj.grounded != foundGround) {
    obj.grounded = foundGround;
    if (foundGround && obj.objClass == ObjectClass::Player && !obj.data.player.playLandingFrame) {
      obj.data.player.state = PlayerState::running;
      if (obj.data.player.jumpImpulseApplied) {
        obj.data.player.state = PlayerState::idle;
        obj.data.player.jumpImpulseApplied = false;
        obj.data.player.jumpWindupTimer.reset();
      }
    }
  }
}

void resolveBulletCollisions(
  GameState& state,
  GameObject& bullet,
  const GameplaySimulationHooks& hooks) {
  bool unusedGround = false;
  (void)unusedGround;
  if (bullet.data.bullet.state == BulletState::inactive) {
    return;
  }

  for (auto& layer : state.layers) {
    for (auto& objB : layer) {
      if (objB.collider.w == 0.0f || objB.collider.h == 0.0f) {
        continue;
      }
      const SDL_FRect rectA = worldRect(bullet);
      const SDL_FRect rectB = collisionRect(objB, bullet.objClass);
      SDL_FRect rectC{0.0f, 0.0f, 0.0f, 0.0f};
      if (SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC)) {
        collisionResponse(state, bullet, objB, rectC, hooks);
      }
    }
  }
}

void purgeCollectedMaterials(GameState& state) {
  for (auto& layer : state.layers) {
    layer.erase(
      std::remove_if(
        layer.begin(),
        layer.end(),
        [](const GameObject& obj) {
          return obj.objClass == ObjectClass::Material &&
                 obj.data.material.state == MaterialState::collected &&
                 obj.currentAnimation == -1;
        }),
      layer.end());
  }
}

void purgeFinishedDeadEnemies(GameState& state) {
  for (auto& layer : state.layers) {
    layer.erase(
      std::remove_if(
        layer.begin(),
        layer.end(),
        [](const GameObject& obj) {
          return obj.objClass == ObjectClass::Enemy &&
                 obj.data.enemy.state == EnemyState::dead &&
                 obj.currentAnimation == -1;
        }),
      layer.end());
  }
}

} // namespace game_engine
