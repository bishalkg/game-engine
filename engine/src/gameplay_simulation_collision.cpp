#include "gameplay_simulation_internal.h"

#include <algorithm>

#include "engine/engine.h"

namespace game_engine {
namespace {

constexpr float kFlyingStoneFrameW = 160.0f;
constexpr float kFlyingStoneFrameH = 128.0f;
constexpr float kFlyingStoneDrawScale = 1.0f;
constexpr SDL_FRect kFlyingStoneColliderNorm{0.45f, 0.4375f, 0.1f, 0.125f};

struct FlyingStoneSpawn {
  std::size_t layerIndex = 0;
  uint32_t bossId = 0;
  glm::vec2 position{0.0f, 0.0f};
};

bool findObjectLayer(GameState& state, const GameObject& target, std::size_t& layerIndex) {
  for (std::size_t idx = 0; idx < state.layers.size(); ++idx) {
    for (const auto& obj : state.layers[idx]) {
      if (&obj == &target) {
        layerIndex = idx;
        return true;
      }
    }
  }
  return false;
}

GameObject makeFlyingStoneDrop(const GameState& state, const glm::vec2& position) {
  GameObject stone(kFlyingStoneFrameH, kFlyingStoneFrameW);
  stone.id = nextDynamicId(state);
  stone.objClass = ObjectClass::Material;
  stone.spriteType = SpriteType::FlyingStone;
  stone.dynamic = true;
  stone.grounded = true;
  stone.drawScale = kFlyingStoneDrawScale;
  stone.colliderNorm = kFlyingStoneColliderNorm;
  stone.applyScale();
  stone.position = position;
  stone.data.material = MaterialData(1, MaterialType::flyingStone);
  stone.currentAnimation = ANIM_IDLE;
  stone.presentationVariant = PresentationVariant::Idle;
  stone.animations.resize(ANIM_COLLECT + 1);
  stone.animations[ANIM_IDLE] = Animation(6, 1.0f);
  stone.animations[ANIM_COLLECT] = Animation(6, 0.25f);
  stone.spriteFrame = 1;
  return stone;
}

glm::vec2 flyingStonePositionForBoss(const GameObject& boss) {
  const float bossDrawW = boss.spritePixelW / boss.drawScale;
  const float bossDrawH = boss.spritePixelH / boss.drawScale;
  const float stoneDrawW = kFlyingStoneFrameW / kFlyingStoneDrawScale;
  const float stoneDrawH = kFlyingStoneFrameH / kFlyingStoneDrawScale;
  return glm::vec2{
    boss.position.x + bossDrawW * 0.5f - stoneDrawW * 0.5f,
    boss.position.y + bossDrawH * 0.5f - stoneDrawH * 0.5f,
  };
}

void recordBossDefeatedIfNeeded(
  SimulationEvents& events,
  GameObject& enemy,
  const DamageEnemyResult& result) {
  if (result.killed && enemy.data.enemy.isBoss) {
    recordBossDefeated(events, enemy);
  }
}

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

float horizontalDirectionAwayFromEnemy(const GameObject& player, const GameObject& enemy) {
  const SDL_FRect playerRect = collisionRect(player, enemy.objClass);
  const SDL_FRect enemyRect = collisionRect(enemy, player.objClass);
  const float playerCenterX = playerRect.x + playerRect.w * 0.5f;
  const float enemyCenterX = enemyRect.x + enemyRect.w * 0.5f;
  if (playerCenterX > enemyCenterX) {
    return 1.0f;
  }
  if (playerCenterX < enemyCenterX) {
    return -1.0f;
  }
  return player.direction >= 0.0f ? 1.0f : -1.0f;
}

bool isFallingOntoEnemy(
  const GameObject& player,
  const GameObject& enemy) {
  if (player.velocity.y <= 0.0f) {
    return false;
  }

  const SDL_FRect playerRect = collisionRect(player, enemy.objClass);
  const SDL_FRect enemyRect = collisionRect(enemy, player.objClass);
  const float playerFeet = playerRect.y + playerRect.h;
  const float playerLowerBodyTop = playerRect.y + playerRect.h * 0.6f;
  const float enemyUpperBodyBottom = enemyRect.y + enemyRect.h * 0.35f;
  const float horizontalOverlapLeft = std::max(playerRect.x, enemyRect.x);
  const float horizontalOverlapRight = std::min(playerRect.x + playerRect.w, enemyRect.x + enemyRect.w);
  return horizontalOverlapRight > horizontalOverlapLeft &&
         playerLowerBodyTop < enemyUpperBodyBottom &&
         playerFeet > enemyRect.y;
}

void applyPlayerEnemyHurtImpulse(GameObject& player, const GameObject& enemy) {
  if (player.objClass != ObjectClass::Player ||
      player.data.player.state == PlayerState::dead) {
    return;
  }

  constexpr float kSideKnockbackX = 180.0f;
  constexpr float kSideLiftY = -160.0f;
  constexpr float kAirPopKnockbackX = 120.0f;
  constexpr float kAirPopLiftY = -250.0f;

  const float awayX = horizontalDirectionAwayFromEnemy(player, enemy);
  if (isFallingOntoEnemy(player, enemy)) {
    player.velocity.x = awayX * kAirPopKnockbackX;
    player.velocity.y = kAirPopLiftY;
    player.grounded = false;
    return;
  }

  player.velocity.x = awayX * kSideKnockbackX;
  player.velocity.y = std::min(player.velocity.y, kSideLiftY);
  player.grounded = false;
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

bool isPlayerAttackPriorityState(const GameObject& player) {
  if (player.objClass != ObjectClass::Player) {
    return false;
  }

  return player.data.player.state == PlayerState::swingWeapon ||
         player.data.player.state == PlayerState::ultimate;
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
      if (obj.data.player.state == PlayerState::hurt ||
          obj.data.player.state == PlayerState::dead ||
          obj.data.player.state == PlayerState::ultimate) {
        return;
      }
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
        if (isPlayerInHurtRecovery(player)) {
          break;
        }
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
            recordBossDefeatedIfNeeded(events, other, result);
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
          recordBossDefeatedIfNeeded(events, other, result);
        } else {
          if (damagePlayer(player, 33) &&
              player.data.player.state != PlayerState::dead) {
            applyPlayerEnemyHurtImpulse(player, other);
          }
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
        if (other.data.material.type == MaterialType::flyingStone) {
          if (!isPlayerAttackPriorityState(player)) {
            break;
          }
          unlockFlyingStoneRewardForAllPlayers(state);
          recordFlyingStoneCollected(events, state.currentLevelId);
        } else {
          awardMaterialToPlayer(player, other.data.material);
        }
        clearDynamicCollider(other);
        other.data.material.state = MaterialState::collapsing;
        switch (other.data.material.type) {
          case MaterialType::coin:
            ++player.data.player.coinPickupCueCount;
            break;
          case MaterialType::gem:
            ++player.data.player.gemPickupCueCount;
            break;
          case MaterialType::flyingStone:
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
    recordBossDefeatedIfNeeded(events, other, result);
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
  const SDL_FRect& overlap,
  SimulationEvents& events) {
  switch (other.objClass) {
    case ObjectClass::Player:
      if (enemy.data.enemy.state == EnemyState::attack) {
        if (!isPlayerAttackPriorityState(other) &&
            !isPlayerInHurtRecovery(other) &&
            damagePlayer(other, 33) &&
            other.data.player.state != PlayerState::dead) {
          applyPlayerEnemyHurtImpulse(other, enemy);
        }
      }
      break;
    case ObjectClass::Level:
      if (other.data.level.isHazard) {
        const DamageEnemyResult result = damageEnemy(state, enemy, 50);
        recordBossDefeatedIfNeeded(events, enemy, result);
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
        applyEnemyGameplayCollision(state, subject, other, collision.overlap, events);
        break;
      case ObjectClass::Level:
      case ObjectClass::Portal:
      case ObjectClass::Background:
      case ObjectClass::Material:
        break;
    }
  }
}

void spawnFlyingStoneDrops(GameState& state, const SimulationEvents& events) {
  std::vector<FlyingStoneSpawn> spawns;
  for (const auto& event : events.bossDefeated) {
    if (!event.boss) {
      continue;
    }

    bool duplicate = false;
    for (const auto& spawn : spawns) {
      if (spawn.bossId == event.boss->id) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) {
      continue;
    }

    std::size_t layerIndex = 0;
    if (findObjectLayer(state, *event.boss, layerIndex)) {
      spawns.push_back(FlyingStoneSpawn{
        layerIndex,
        event.boss->id,
        flyingStonePositionForBoss(*event.boss),
      });
    }
  }

  for (const auto& spawn : spawns) {
    if (spawn.layerIndex < state.layers.size()) {
      state.layers[spawn.layerIndex].push_back(makeFlyingStoneDrop(state, spawn.position));
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
