#include "gameplay_simulation_internal.h"

#include <algorithm>

#include "engine/engine.h"

namespace game_engine {
namespace {

void defaultSolidResponse(GameObject& obj, const SDL_FRect& overlap) {
  if (overlap.w < overlap.h) {
    if (obj.velocity.x > 0.0f) {
      obj.position.x -= overlap.w + 0.1f;
    } else if (obj.velocity.x < 0.0f) {
      obj.position.x += overlap.w + 0.1f;
    }
    obj.velocity.x = 0.0f;
  } else {
    if (obj.velocity.y > 0.0f) {
      obj.position.y -= overlap.h;
    } else if (obj.velocity.y < 0.0f) {
      obj.position.y += overlap.h;
    }
    obj.velocity.y = 0.0f;
  }
}

void blockHorizontalPassThrough(GameObject& obj, const GameObject& blocker, const SDL_FRect& overlap) {
  if (obj.position.x <= blocker.position.x) {
    obj.position.x -= overlap.w + 0.1f;
  } else {
    obj.position.x += overlap.w + 0.1f;
  }
  obj.velocity.x = 0.0f;
}

bool isMovingTowardEnemySide(const GameObject& obj, const GameObject& enemy) {
  if (obj.velocity.x > 0.0f && obj.position.x <= enemy.position.x) {
    return true;
  }
  if (obj.velocity.x < 0.0f && obj.position.x >= enemy.position.x) {
    return true;
  }
  return false;
}

bool shouldBlockSwingPassThrough(
  const GameObject& player,
  const GameObject& enemy,
  const SDL_FRect& overlap) {
  if (!(overlap.w < overlap.h && isMovingTowardEnemySide(player, enemy))) {
    return false;
  }

  const SDL_FRect rectA = collisionRect(player, enemy.objClass);
  const SDL_FRect rectB = collisionRect(enemy, player.objClass);
  const float enemyMiddleTop = rectB.y + rectB.h * 0.25f;
  const float enemyMiddleBottom = rectB.y + rectB.h * 0.75f;
  return rectA.y < enemyMiddleBottom && (rectA.y + rectA.h) > enemyMiddleTop;
}

bool projectilePassesThrough(const GameObject& projectile, const GameObject& other) {
  (void)projectile;
  switch (other.objClass) {
    case ObjectClass::Enemy:
      return other.data.enemy.state == EnemyState::dead;
    case ObjectClass::Player:
    case ObjectClass::Portal:
    case ObjectClass::Background:
    case ObjectClass::Projectile:
    case ObjectClass::Material:
      return true;
    case ObjectClass::Level:
      return false;
  }
  return true;
}

void refreshGroundedState(GameState& state, GameObject& obj) {
  bool foundGround = false;
  for (auto& layer : state.layers) {
    for (auto& objB : layer) {
      if (&obj == &objB || objB.objClass != ObjectClass::Level ||
          objB.collider.w == 0.0f || objB.collider.h == 0.0f) {
        continue;
      }

      const SDL_FRect rectB = collisionRect(objB, obj.objClass);
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

void applyPlayerGameplayCollision(
  GameState& state,
  GameObject& player,
  GameObject& other,
  const SDL_FRect& overlap,
  SimulationEvents& events) {
  switch (other.objClass) {
    case ObjectClass::Level:
      if (other.data.level.isHazard) {
        damagePlayer(player, 50);
      }
      break;
    case ObjectClass::Enemy:
      if (other.data.enemy.state != EnemyState::dead) {
        if (player.data.player.state == PlayerState::ultimate) {
          if (isUltimateDamageActive(player)) {
            const DamageEnemyResult result = damageEnemy(
              state,
              other,
              50,
              player.id,
              player.data.player.activeUltimateCastId,
              true,
              HitStopStrength::Heavy,
              player.direction,
              enemyKnockbackMagnitude(EnemyImpactType::Ultimate));
            if (result.applied) {
              recordHitConfirmed(
                events,
                {player.objClass, player.id},
                {other.objClass, other.id},
                HitStopStrength::Heavy);
            }
          }
        } else if (player.data.player.state == PlayerState::swingWeapon) {
          const DamageEnemyResult result = damageEnemy(
            state,
            other,
            player.data.player.meleeDamage,
            player.id,
            0,
            false,
            HitStopStrength::Normal,
            player.direction,
            enemyKnockbackMagnitude(EnemyImpactType::Melee));
          if (result.applied) {
            recordHitConfirmed(
              events,
              {player.objClass, player.id},
              {other.objClass, other.id},
              HitStopStrength::Normal);
          }
        } else {
          player.velocity = glm::vec2(50.0f, 0.0f) * -player.direction;
        }
      }
      break;
    case ObjectClass::Portal:
      if (!bossBlocksPortalTransition(state)) {
        recordPortalTriggered(events, other.data.portal.nextLevel);
      }
      break;
    case ObjectClass::Material:
      if (other.data.material.state == MaterialState::present) {
        awardMaterialToPlayer(player, other.data.material);
        clearDynamicCollider(other);
        other.data.material.state = MaterialState::collapsing;
        switch (other.data.material.type) {
          case MaterialType::coin:
            ++player.data.player.coinPickupCueCount;
            break;
          case MaterialType::gem:
            ++player.data.player.gemPickupCueCount;
            break;
          default:
            break;
        }
      }
      break;
    case ObjectClass::Player:
    case ObjectClass::Background:
    case ObjectClass::Projectile:
      (void)overlap;
      break;
  }
}

void applyProjectileGameplayCollision(
  GameState& state,
  GameObject& projectile,
  GameObject& other,
  const SDL_FRect& overlap,
  SimulationEvents& events) {
  if (projectile.data.bullet.state != BulletState::moving) {
    return;
  }

  const bool passthrough = projectilePassesThrough(projectile, other);
  if (other.objClass == ObjectClass::Enemy && other.data.enemy.state != EnemyState::dead) {
    const DamageEnemyResult result = damageEnemy(
      state,
      other,
      10,
      projectile.data.bullet.ownerPlayerId,
      0,
      false,
      HitStopStrength::Normal,
      projectile.direction,
      enemyKnockbackMagnitude(EnemyImpactType::Projectile));
    if (result.applied) {
      recordHitConfirmed(
        events,
        {projectile.objClass, projectile.id},
        {other.objClass, other.id},
        HitStopStrength::Normal);
    }
  }

  if (!passthrough) {
    defaultSolidResponse(projectile, overlap);
    projectile.velocity = glm::vec2(0.0f);
    projectile.data.bullet.state = BulletState::colliding;
    setAnimationAndPresentation(projectile, ANIM_RUN, PresentationVariant::ProjectileHit);
  }
}

void applyEnemyGameplayCollision(
  GameState& state,
  GameObject& enemy,
  GameObject& other,
  const SDL_FRect& overlap) {
  switch (other.objClass) {
    case ObjectClass::Player:
      if (enemy.data.enemy.state == EnemyState::attack) {
        damagePlayer(other, 33);
      }
      break;
    case ObjectClass::Level:
      if (other.data.level.isHazard) {
        damageEnemy(state, enemy, 50);
      }
      break;
    case ObjectClass::Enemy:
      if (other.data.enemy.state != EnemyState::dead) {
        enemy.velocity = glm::vec2(50.0f, 0.0f) * -enemy.direction;
      }
      break;
    case ObjectClass::Portal:
    case ObjectClass::Background:
    case ObjectClass::Projectile:
    case ObjectClass::Material:
      (void)overlap;
      break;
  }
}

} // namespace

std::vector<CollisionPair> detectCollisions(
  GameState& state,
  GameObject& subject,
  CollisionSubjectKind subjectKind) {
  std::vector<CollisionPair> collisions;
  if (subjectKind == CollisionSubjectKind::Bullet &&
      subject.data.bullet.state == BulletState::inactive) {
    return collisions;
  }

  for (auto& layer : state.layers) {
    for (auto& other : layer) {
      if (&subject == &other || other.collider.w == 0.0f || other.collider.h == 0.0f) {
        continue;
      }

      const SDL_FRect rectA =
        subjectKind == CollisionSubjectKind::Bullet
          ? worldRect(subject)
          : collisionRect(subject, other.objClass);
      const SDL_FRect rectB = collisionRect(other, subject.objClass);
      SDL_FRect overlap{0.0f, 0.0f, 0.0f, 0.0f};
      if (SDL_GetRectIntersectionFloat(&rectA, &rectB, &overlap)) {
        collisions.push_back(CollisionPair{&subject, &other, overlap, subjectKind});
      }
    }
  }

  return collisions;
}

void resolveSolidCollisions(
  GameState& state,
  GameObject& subject,
  const std::vector<CollisionPair>& collisions) {
  for (const auto& collision : collisions) {
    if (collision.subject != &subject || !collision.other) {
      continue;
    }

    GameObject& other = *collision.other;
    switch (subject.objClass) {
      case ObjectClass::Player:
        if (other.objClass == ObjectClass::Level) {
          if (other.data.level.isHazard) {
            subject.position.y -= collision.overlap.h;
          } else {
            defaultSolidResponse(subject, collision.overlap);
          }
        } else if (other.objClass == ObjectClass::Enemy &&
                   other.data.enemy.state != EnemyState::dead &&
                   subject.data.player.state == PlayerState::swingWeapon &&
                   shouldBlockSwingPassThrough(subject, other, collision.overlap)) {
          blockHorizontalPassThrough(subject, other, collision.overlap);
        }
        break;
      case ObjectClass::Enemy:
        if (other.objClass == ObjectClass::Level) {
          if (other.data.level.isHazard) {
            subject.position.y -= collision.overlap.h;
          } else {
            defaultSolidResponse(subject, collision.overlap);
          }
        }
        break;
      case ObjectClass::Projectile:
      case ObjectClass::Level:
      case ObjectClass::Portal:
      case ObjectClass::Background:
      case ObjectClass::Material:
        break;
    }
  }

  if (subject.objClass != ObjectClass::Projectile) {
    refreshGroundedState(state, subject);
  }
}

PhysicsStepResult physicsStep(
  GameState& state,
  const std::vector<MotionIntent>& objectMotion,
  const std::vector<MotionIntent>& bulletMotion,
  float deltaTime) {
  PhysicsStepResult result;

  for (const auto& intent : objectMotion) {
    if (intent.object) {
      integrateMotion(*intent.object, intent.direction, deltaTime);
    }
  }
  for (const auto& intent : bulletMotion) {
    if (intent.object) {
      integrateMotion(*intent.object, intent.direction, deltaTime);
    }
  }

  for (const auto& intent : objectMotion) {
    if (!intent.object) {
      continue;
    }
    std::vector<CollisionPair> collisions =
      detectCollisions(state, *intent.object, CollisionSubjectKind::DynamicObject);
    result.gameplayCollisions.insert(
      result.gameplayCollisions.end(),
      collisions.begin(),
      collisions.end());
    resolveSolidCollisions(state, *intent.object, collisions);
  }

  for (const auto& intent : bulletMotion) {
    if (!intent.object) {
      continue;
    }
    std::vector<CollisionPair> collisions =
      detectCollisions(state, *intent.object, CollisionSubjectKind::Bullet);
    result.gameplayCollisions.insert(
      result.gameplayCollisions.end(),
      collisions.begin(),
      collisions.end());
  }

  return result;
}

void applyGameplayCollisions(
  GameState& state,
  const PhysicsStepResult& physicsResult,
  SimulationEvents& events) {
  for (const auto& collision : physicsResult.gameplayCollisions) {
    if (!collision.subject || !collision.other) {
      continue;
    }

    GameObject& subject = *collision.subject;
    GameObject& other = *collision.other;
    switch (subject.objClass) {
      case ObjectClass::Player:
        applyPlayerGameplayCollision(state, subject, other, collision.overlap, events);
        break;
      case ObjectClass::Projectile:
        applyProjectileGameplayCollision(state, subject, other, collision.overlap, events);
        break;
      case ObjectClass::Enemy:
        applyEnemyGameplayCollision(state, subject, other, collision.overlap);
        break;
      case ObjectClass::Level:
      case ObjectClass::Portal:
      case ObjectClass::Background:
      case ObjectClass::Material:
        break;
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

void purgeDeadOrCollectedObjects(GameState& state) {
  state.bullets.erase(
    std::remove_if(
      state.bullets.begin(),
      state.bullets.end(),
      [](const GameObject& bullet) { return bullet.data.bullet.state == BulletState::inactive; }),
    state.bullets.end());

  purgeFinishedDeadEnemies(state);
  purgeCollectedMaterials(state);
}

} // namespace game_engine
