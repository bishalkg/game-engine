#include "engine/gameplay_simulation.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "engine/engine.h"

namespace game_engine {
namespace {

constexpr float kUltimateColliderPaddingFrac = 0.05f;
constexpr int kUltimateDamageWindowFrames = 9;

const NetGameInput& inputForPlayer(
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  uint32_t playerID) {
  static const NetGameInput kEmpty{};
  const auto it = playerInputs.find(playerID);
  return it == playerInputs.end() ? kEmpty : it->second;
}

bool hasAnimation(const GameObject& obj, int animIndex) {
  return animIndex >= 0 && animIndex < static_cast<int>(obj.animations.size()) &&
         obj.animations[animIndex].getFrameCount() > 0;
}

void setPresentation(GameObject& obj, PresentationVariant presentation) {
  obj.presentationVariant = presentation;
}

void setAnimation(GameObject& obj, int animIndex, bool reset = true) {
  if (!hasAnimation(obj, animIndex)) {
    obj.currentAnimation = -1;
    return;
  }

  const bool changed = obj.currentAnimation != animIndex;
  obj.currentAnimation = animIndex;
  if (changed || reset) {
    obj.animations[animIndex].reset();
  }
  obj.spriteFrame = obj.animations[animIndex].currentFrame() + 1;
}

void setAnimationAndPresentation(
  GameObject& obj,
  int animIndex,
  PresentationVariant presentation,
  bool reset = true) {
  setAnimation(obj, animIndex, reset);
  setPresentation(obj, presentation);
}

void syncSpriteFrame(GameObject& obj) {
  if (hasAnimation(obj, obj.currentAnimation)) {
    const int frameCount = obj.animations[obj.currentAnimation].getFrameCount();
    obj.spriteFrame =
      std::clamp(obj.animations[obj.currentAnimation].currentFrame() + 1, 1, std::max(frameCount, 1));
  }
}

void clearFlash(GameObject& obj, float deltaTime) {
  if (obj.shouldFlash && obj.flashTimer.step(deltaTime)) {
    obj.shouldFlash = false;
  }
}

SDL_FRect worldRect(const GameObject& obj) {
  return SDL_FRect{
    obj.position.x + obj.collider.x,
    obj.position.y + obj.collider.y,
    obj.collider.w,
    obj.collider.h,
  };
}

SDL_FRect baseFacing(const GameObject& obj) {
  SDL_FRect c = obj.baseCollider;
  if (obj.direction < 0.0f) {
    const float drawW = obj.spritePixelW / obj.drawScale;
    c.x = drawW - (c.x + c.w);
  }
  return c;
}

void widenColliderForSwing(GameObject& obj) {
  const float drawW = obj.spritePixelW / obj.drawScale;
  const float extra = 0.2f * drawW;

  SDL_FRect c = baseFacing(obj);
  c.w += extra;
  if (obj.direction < 0.0f) {
    c.x -= extra;
  }
  obj.collider = c;
}

void expandColliderForUltimate(GameObject& obj) {
  const float drawW = obj.spritePixelW / obj.drawScale;
  const float drawH = obj.spritePixelH / obj.drawScale;
  obj.collider = SDL_FRect{
    -kUltimateColliderPaddingFrac * drawW,
    -kUltimateColliderPaddingFrac * drawH,
    drawW * (1.0f + 2.0f * kUltimateColliderPaddingFrac),
    drawH * (1.0f + 2.0f * kUltimateColliderPaddingFrac),
  };
}

bool isUltimateDamageActive(const GameObject& obj) {
  if (obj.objClass != ObjectClass::Player ||
      obj.data.player.state != PlayerState::ultimate ||
      obj.currentAnimation != ANIM_ULTIMATE ||
      !hasAnimation(obj, ANIM_ULTIMATE)) {
    return false;
  }

  const Animation& ultimateAnim = obj.animations[ANIM_ULTIMATE];
  const int frameCount = ultimateAnim.getFrameCount();
  const int damageStartFrame = std::max(0, frameCount - kUltimateDamageWindowFrames);
  return ultimateAnim.currentFrame() >= damageStartFrame && !ultimateAnim.isDone();
}

SDL_FRect physicsColliderFor(const GameObject& obj, ObjectClass otherClass) {
  if (obj.objClass == ObjectClass::Player &&
      obj.data.player.state == PlayerState::ultimate &&
      otherClass != ObjectClass::Enemy) {
    return baseFacing(obj);
  }
  return obj.collider;
}

SDL_FRect collisionRect(const GameObject& obj, ObjectClass otherClass) {
  const SDL_FRect collider = physicsColliderFor(obj, otherClass);
  return SDL_FRect{
    obj.position.x + collider.x,
    obj.position.y + collider.y,
    collider.w,
    collider.h,
  };
}

GameObject* findPlayerById(GameState& state, uint32_t playerID) {
  if (state.playerLayer >= 0 && state.playerLayer < static_cast<int>(state.layers.size())) {
    for (auto& obj : state.layers[state.playerLayer]) {
      if (obj.objClass == ObjectClass::Player && obj.id == playerID) {
        return &obj;
      }
    }
  }

  for (auto& layer : state.layers) {
    for (auto& obj : layer) {
      if (obj.objClass == ObjectClass::Player && obj.id == playerID) {
        return &obj;
      }
    }
  }

  return nullptr;
}

GameObject* findClosestLivingPlayer(GameState& state, const GameObject& source) {
  if (state.playerLayer < 0 || state.playerLayer >= static_cast<int>(state.layers.size())) {
    return nullptr;
  }

  GameObject* closest = nullptr;
  float closestSq = std::numeric_limits<float>::max();
  for (auto& obj : state.layers[state.playerLayer]) {
    if (obj.objClass != ObjectClass::Player || obj.data.player.state == PlayerState::dead) {
      continue;
    }
    const glm::vec2 delta = obj.position - source.position;
    const float distSq = glm::dot(delta, delta);
    if (distSq < closestSq) {
      closestSq = distSq;
      closest = &obj;
    }
  }
  return closest;
}

uint32_t nextDynamicId(const GameState& state) {
  uint32_t nextID = 1;
  for (const auto& layer : state.layers) {
    for (const auto& obj : layer) {
      nextID = std::max(nextID, obj.id + 1);
    }
  }
  for (const auto& obj : state.bullets) {
    nextID = std::max(nextID, obj.id + 1);
  }
  return nextID;
}

void awardUltimateCharge(GameState& state, uint32_t playerID, int amount) {
  if (amount <= 0) {
    return;
  }

  if (GameObject* player = findPlayerById(state, playerID)) {
    player->data.player.ultimatePoints = std::clamp(
      player->data.player.ultimatePoints + amount,
      0,
      player->data.player.maxUltimatePoints);
  }
}

void emitHitConfirmed(
  const GameplaySimulationHooks& hooks,
  GameObjectKey attacker,
  GameObjectKey victim,
  HitStopStrength strength) {
  if (hooks.onHitConfirmed) {
    hooks.onHitConfirmed(attacker, victim, strength);
  }
}

void clearEnemyPendingKnockback(GameObject& enemy) {
  if (enemy.objClass != ObjectClass::Enemy) {
    return;
  }

  enemy.data.enemy.pendingKnockbackDirection = 0.0f;
  enemy.data.enemy.pendingKnockbackMagnitude = 0.0f;
  enemy.data.enemy.hasPendingKnockback = false;
}

void queueEnemyHitImpact(
  GameObject& enemy,
  HitStopStrength strength,
  float direction,
  float magnitude) {
  if (enemy.objClass != ObjectClass::Enemy) {
    return;
  }

  auto& enemyData = enemy.data.enemy;
  enemyData.hitStopRemainingSeconds =
    std::max(enemyData.hitStopRemainingSeconds, hitStopDurationSeconds(strength));
  enemyData.pendingKnockbackDirection = direction >= 0.0f ? 1.0f : -1.0f;
  enemyData.pendingKnockbackMagnitude = std::max(0.0f, magnitude);
  enemyData.hasPendingKnockback = enemyData.pendingKnockbackMagnitude > 0.0f;
  enemy.velocity = glm::vec2(0.0f);
  enemy.acceleration = glm::vec2(0.0f);
}

void clearDynamicCollider(GameObject& obj) {
  obj.collider = SDL_FRect{0.0f, 0.0f, 0.0f, 0.0f};
}

bool stepEnemyHitStop(GameObject& enemy, float deltaTime) {
  if (enemy.objClass != ObjectClass::Enemy) {
    return false;
  }

  auto& enemyData = enemy.data.enemy;
  if (enemyData.hitStopRemainingSeconds <= 0.0f) {
    return false;
  }

  enemyData.hitStopRemainingSeconds =
    std::max(0.0f, enemyData.hitStopRemainingSeconds - deltaTime);
  enemy.velocity = glm::vec2(0.0f);
  enemy.acceleration = glm::vec2(0.0f);

  if (enemyData.hitStopRemainingSeconds > 0.0f) {
    return true;
  }

  if (enemyData.hasPendingKnockback && enemyData.state != EnemyState::dead) {
    enemy.velocity.x =
      enemyData.pendingKnockbackDirection * enemyData.pendingKnockbackMagnitude;
  }
  clearEnemyPendingKnockback(enemy);
  return false;
}

struct DamageEnemyResult {
  bool applied = false;
  bool killed = false;
};

DamageEnemyResult damageEnemy(
  GameState& state,
  GameObject& enemy,
  int damage,
  uint32_t sourcePlayerId = 0,
  uint32_t sourceUltimateCastId = 0,
  bool ignoreHurtCooldown = false,
  HitStopStrength hitStopStrength = HitStopStrength::Normal,
  float knockbackDirection = 0.0f,
  float knockbackMagnitude = 0.0f) {
  DamageEnemyResult result{};
  if (enemy.objClass != ObjectClass::Enemy || enemy.data.enemy.state == EnemyState::dead) {
    return result;
  }
  if (sourceUltimateCastId != 0 &&
      enemy.data.enemy.lastUltimatePlayerId == sourcePlayerId &&
      enemy.data.enemy.lastUltimateCastId == sourceUltimateCastId) {
    return result;
  }
  if (!ignoreHurtCooldown &&
      enemy.data.enemy.state == EnemyState::hurt &&
      !enemy.data.enemy.damageTimer.isTimedOut()) {
    return result;
  }

  result.applied = true;
  if (sourceUltimateCastId != 0) {
    enemy.data.enemy.lastUltimatePlayerId = sourcePlayerId;
    enemy.data.enemy.lastUltimateCastId = sourceUltimateCastId;
  }
  enemy.shouldFlash = true;
  enemy.flashTimer.reset();
  enemy.data.enemy.state = EnemyState::hurt;
  setAnimationAndPresentation(enemy, ANIM_HIT, PresentationVariant::Hit);
  enemy.data.enemy.healthPoints -= damage;
  enemy.data.enemy.damageTimer.reset();
  queueEnemyHitImpact(enemy, hitStopStrength, knockbackDirection, knockbackMagnitude);

  if (enemy.data.enemy.healthPoints <= 0) {
    enemy.data.enemy.healthPoints = 0;
    enemy.data.enemy.state = EnemyState::dead;
    setAnimationAndPresentation(enemy, ANIM_DIE, PresentationVariant::Die);
    enemy.velocity = glm::vec2(0.0f);
    clearEnemyPendingKnockback(enemy);
    clearDynamicCollider(enemy);
    result.killed = true;
    if (sourcePlayerId != 0) {
      awardUltimateCharge(state, sourcePlayerId, 33);
    }
  }

  return result;
}

void damagePlayer(GameObject& player, int damage) {
  if (player.objClass != ObjectClass::Player ||
      player.data.player.state == PlayerState::dead ||
      player.data.player.state == PlayerState::ultimate) {
    return;
  }
  if (player.data.player.state == PlayerState::hurt && !player.data.player.damageTimer.isTimedOut()) {
    return;
  }

  player.shouldFlash = true;
  player.flashTimer.reset();
  player.data.player.state = PlayerState::hurt;
  setAnimationAndPresentation(player, ANIM_HIT, PresentationVariant::Hit);
  player.data.player.healthPoints -= damage;
  player.data.player.damageTimer.reset();

  if (player.data.player.healthPoints <= 0) {
    player.data.player.healthPoints = 0;
    player.data.player.state = PlayerState::dead;
    setAnimationAndPresentation(player, ANIM_DIE, PresentationVariant::Die);
    player.velocity = glm::vec2(0.0f);
  }
}

GameObject makeBulletFromPlayer(const GameObject& player, const GameState& state) {
  GameObject bullet(128, 128);
  bullet.id = nextDynamicId(state);
  bullet.objClass = ObjectClass::Projectile;
  bullet.spriteType = player.spriteType;
  bullet.drawScale = 2.0f;
  bullet.colliderNorm = {.x = 0.0f, .y = 0.40f, .w = 0.5f, .h = 0.1f};
  bullet.applyScale();
  bullet.data.bullet = BulletData();
  bullet.data.bullet.ownerPlayerId = player.id;
  bullet.currentAnimation = ANIM_IDLE;
  bullet.presentationVariant = PresentationVariant::ProjectileMoving;
  bullet.dynamic = true;
  bullet.direction = player.direction;
  bullet.maxSpeedX = 1000.0f;
  const int yJitter = 50;
  const float yVelocity = static_cast<float>(SDL_rand(yJitter)) - yJitter / 1.5f;
  bullet.velocity.x = 200.0f * player.direction + player.velocity.x * 0.2f;
  bullet.velocity.y = yVelocity;
  bullet.position = glm::vec2(
    player.position.x + (player.direction < 0.0f ? -20.0f : 20.0f),
    player.position.y + (player.spritePixelH / player.drawScale) / 8.0f);
  bullet.animations.resize(2);
  bullet.animations[ANIM_IDLE] = Animation(9, 1.0f);
  bullet.animations[ANIM_RUN] = Animation(4, 0.15f);
  bullet.spriteFrame = 1;
  return bullet;
}

void updateDynamicObject(
  GameState& state,
  GameObject& obj,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  const GameplaySimulationHooks& hooks,
  float deltaTime) {
  if (hasAnimation(obj, obj.currentAnimation)) {
    obj.animations[obj.currentAnimation].step(deltaTime);
    syncSpriteFrame(obj);
  }

  clearFlash(obj, deltaTime);

  if (obj.dynamic && !obj.grounded) {
    obj.velocity += Engine::GRAVITY * deltaTime;
  }

  float currDirection = 0.0f;

  if (obj.objClass == ObjectClass::Player) {
    auto& player = obj.data.player;
    const NetGameInput& input = inputForPlayer(playerInputs, obj.id);

    player.weaponTimer.step(deltaTime);
    player.healthRecoveryTimer.step(deltaTime);
    player.manaRecoveryTimer.step(deltaTime);
    player.ultimateRecoveryTimer.step(deltaTime);
    player.meleePressedThisFrame = input.meleePressed;
    player.ultimatePressedThisFrame = input.ultimatePressed;

    if (player.healthRecoveryTimer.isTimedOut()) {
      player.healthRecoveryTimer.reset();
      player.healthPoints = std::clamp(player.healthPoints + 1, 0, player.maxHealthPoints);
    }
    if (player.manaRecoveryTimer.isTimedOut()) {
      player.manaRecoveryTimer.reset();
      player.manaPoints = std::clamp(player.manaPoints + 1, 0, player.maxManaPoints);
    }
    if (player.ultimateRecoveryTimer.isTimedOut()) {
      player.ultimateRecoveryTimer.reset();
      player.ultimatePoints = std::clamp(player.ultimatePoints + 33, 0, player.maxUltimatePoints);
    }

    if (input.leftHeld) {
      currDirection -= 1.0f;
    }
    if (input.rightHeld) {
      currDirection += 1.0f;
    }
    const float desiredDirection = currDirection;

    const bool hasSwingFollowup =
      static_cast<int>(obj.animations.size()) > ANIM_SWING_2 &&
      obj.animations[ANIM_SWING_2].getFrameCount() > 0;
    const bool wantSwing = player.meleePressedThisFrame;
    const bool canSwing =
      player.state != PlayerState::swingWeapon && player.state != PlayerState::ultimate;
    const bool canStartUltimate =
      player.unlockedUltimateOne &&
      player.ultimatePressedThisFrame &&
      player.state != PlayerState::dead &&
      player.state != PlayerState::hurt &&
      player.state != PlayerState::ultimate &&
      player.ultimatePoints >= player.maxUltimatePoints &&
      hasAnimation(obj, ANIM_ULTIMATE);

    const auto resetSwingState = [&obj]() {
      obj.data.player.swingStage = PlayerSwingStage::None;
      obj.data.player.queuedFollowupSwing = false;
      obj.data.player.meleeDamage = 10;
    };

    const auto restoreDefaultPlayerState = [&]() {
      resetSwingState();
      player.activeUltimateCastId = 0;
      obj.collider = baseFacing(obj);

      if (!obj.grounded) {
        obj.data.player.state = PlayerState::jumping;
        setAnimationAndPresentation(obj, ANIM_JUMP, PresentationVariant::Jump);
        return;
      }

      if (desiredDirection != 0.0f) {
        obj.data.player.state = PlayerState::running;
        setAnimationAndPresentation(obj, ANIM_RUN, PresentationVariant::Run);
        return;
      }

      obj.data.player.state = PlayerState::idle;
      setAnimationAndPresentation(obj, ANIM_IDLE, PresentationVariant::Idle);
    };

    const auto startAttack1 = [&](int attackAnimIndex, PresentationVariant attackPresentation) {
      resetSwingState();
      obj.data.player.state = PlayerState::swingWeapon;
      obj.data.player.swingStage = PlayerSwingStage::Attack1;
      setAnimationAndPresentation(obj, attackAnimIndex, attackPresentation);
      widenColliderForSwing(obj);
    };

    const auto startUltimate = [&]() {
      resetSwingState();
      player.state = PlayerState::ultimate;
      player.ultimatePoints = 0;
      player.activeUltimateCastId = player.nextUltimateCastId++;
      obj.velocity.x = 0.0f;
      setAnimationAndPresentation(obj, ANIM_ULTIMATE, PresentationVariant::Ultimate);
      expandColliderForUltimate(obj);
    };

    const auto handleAttacking = [&](int idleOrMoveAnim,
                                     PresentationVariant idlePresentation,
                                     int shootAnim,
                                     PresentationVariant shootPresentation,
                                     int attackAnim,
                                     PresentationVariant attackPresentation,
                                     bool handleJump) {
      if (wantSwing && canSwing) {
        startAttack1(attackAnim, attackPresentation);
      } else if (input.fireHeld) {
        setAnimation(obj, shootAnim, false);
        setPresentation(obj, shootPresentation);

        if (player.weaponTimer.isTimedOut() && player.manaPoints > 10) {
          player.weaponTimer.reset();
          player.manaPoints = std::clamp(player.manaPoints - 2, 0, player.maxManaPoints);
          state.bullets.push_back(makeBulletFromPlayer(obj, state));
        }
      } else if (handleJump) {
        setPresentation(obj, idlePresentation);
        if (obj.currentAnimation != ANIM_JUMP && obj.currentAnimation != -1) {
          setAnimation(obj, ANIM_JUMP);
        }

        if (obj.currentAnimation == ANIM_JUMP &&
            obj.animations[ANIM_JUMP].isDone()) {
          obj.currentAnimation = -1;
        }
      } else {
        obj.animations[ANIM_SHOOT].reset();
        obj.animations[ANIM_SLIDE_SHOOT].reset();
        setAnimation(obj, idleOrMoveAnim, false);
        setPresentation(obj, idlePresentation);
      }
    };

    if (canStartUltimate) {
      startUltimate();
    }

    switch (player.state) {
      case PlayerState::idle: {
        obj.collider = baseFacing(obj);
        setPresentation(obj, PresentationVariant::Idle);
        if (input.jumpPressed && obj.grounded) {
          player.state = PlayerState::jumping;
          player.jumpWindupTimer.reset();
          player.jumpImpulseApplied = false;
          setAnimationAndPresentation(obj, ANIM_JUMP, PresentationVariant::Jump);
          break;
        }
        if (currDirection != 0.0f) {
          player.state = PlayerState::running;
          setAnimationAndPresentation(obj, ANIM_RUN, PresentationVariant::Run);
        } else if (obj.velocity.x != 0.0f) {
          const float factor = obj.velocity.x > 0.0f ? -1.5f : 1.5f;
          const float amount = factor * obj.acceleration.x * deltaTime;
          if (std::abs(obj.velocity.x) < std::abs(amount)) {
            obj.velocity.x = 0.0f;
          } else {
            obj.velocity.x += amount;
          }
        }

        if (wantSwing && canSwing) {
          handleAttacking(
            ANIM_RUN,
            PresentationVariant::Run,
            ANIM_RUN_ATTACK,
            PresentationVariant::RunAttack,
            ANIM_RUN_ATTACK,
            PresentationVariant::RunAttack,
            false);
        } else {
          handleAttacking(
            ANIM_IDLE,
            PresentationVariant::Idle,
            ANIM_SHOOT,
            PresentationVariant::Shoot,
            ANIM_SHOOT,
            PresentationVariant::Shoot,
            false);
        }
        break;
      }
      case PlayerState::running: {
        setPresentation(obj, PresentationVariant::Run);
        if (input.jumpPressed && obj.grounded) {
          player.state = PlayerState::jumping;
          player.jumpWindupTimer.reset();
          player.jumpImpulseApplied = false;
          setAnimationAndPresentation(obj, ANIM_JUMP, PresentationVariant::Jump);
          break;
        }
        if (currDirection == 0.0f) {
          player.state = PlayerState::idle;
        }

        if (obj.velocity.x * obj.direction < 0.0f && obj.grounded) {
          if (wantSwing && canSwing) {
            handleAttacking(
              ANIM_RUN,
              PresentationVariant::Run,
              ANIM_RUN_ATTACK,
              PresentationVariant::RunAttack,
              ANIM_RUN_ATTACK,
              PresentationVariant::RunAttack,
              false);
          } else {
            handleAttacking(
              ANIM_SLIDE,
              PresentationVariant::Slide,
              ANIM_SLIDE_SHOOT,
              PresentationVariant::SlideShoot,
              ANIM_SLIDE_SHOOT,
              PresentationVariant::SlideShoot,
              false);
          }
        } else {
          if (wantSwing && canSwing) {
            handleAttacking(
              ANIM_RUN,
              PresentationVariant::Run,
              ANIM_RUN_ATTACK,
              PresentationVariant::RunAttack,
              ANIM_RUN_ATTACK,
              PresentationVariant::RunAttack,
              false);
          } else {
            handleAttacking(
              ANIM_RUN,
              PresentationVariant::Run,
              ANIM_RUN,
              PresentationVariant::RunShoot,
              ANIM_RUN,
              PresentationVariant::RunAttack,
              false);
          }
        }
        break;
      }
      case PlayerState::jumping: {
        setPresentation(obj, PresentationVariant::Jump);
        if (!player.jumpImpulseApplied) {
          player.jumpWindupTimer.step(deltaTime);
          if (player.jumpWindupTimer.isTimedOut()) {
            obj.velocity.y += Engine::JUMP_FORCE;
            player.jumpImpulseApplied = true;
          }
        } else {
          const int frameCount = obj.animations[ANIM_JUMP].getFrameCount();
          if (!obj.grounded && obj.currentAnimation == ANIM_JUMP &&
              obj.animations[ANIM_JUMP].currentFrame() >= frameCount - 2) {
            obj.currentAnimation = -1;
            obj.spriteFrame = frameCount - 1;
            player.playLandingFrame = true;
          }

          if (obj.grounded) {
            if (player.playLandingFrame) {
              obj.currentAnimation = -1;
              obj.spriteFrame = frameCount;
              player.playLandingFrame = false;
              break;
            }
            obj.velocity.y = 0.0f;
            player.state = PlayerState::idle;
            obj.animations[ANIM_JUMP].reset();
          }
        }

        if (wantSwing && canSwing) {
          handleAttacking(
            ANIM_RUN,
            PresentationVariant::Run,
            ANIM_RUN_ATTACK,
            PresentationVariant::RunAttack,
            ANIM_RUN_ATTACK,
            PresentationVariant::RunAttack,
            false);
        } else {
          handleAttacking(
            ANIM_JUMP,
            PresentationVariant::Jump,
            ANIM_JUMP,
            PresentationVariant::JumpShoot,
            ANIM_JUMP,
            PresentationVariant::JumpShoot,
            true);
        }
        break;
      }
      case PlayerState::swingWeapon: {
        const bool isAttack1Anim =
          obj.currentAnimation == ANIM_RUN_ATTACK || obj.currentAnimation == ANIM_SWING;
        const bool isAttack2Anim = obj.currentAnimation == ANIM_SWING_2;
        const bool attack1Done =
          (obj.currentAnimation == ANIM_RUN_ATTACK && obj.animations[ANIM_RUN_ATTACK].isDone()) ||
          (obj.currentAnimation == ANIM_SWING && obj.animations[ANIM_SWING].isDone());
        const bool attack2Done =
          obj.currentAnimation == ANIM_SWING_2 && obj.animations[ANIM_SWING_2].isDone();

        if (obj.currentAnimation == -1 ||
            (player.swingStage == PlayerSwingStage::Attack1 && !isAttack1Anim) ||
            (player.swingStage == PlayerSwingStage::Attack2 && !isAttack2Anim)) {
          restoreDefaultPlayerState();
          break;
        }

        if (player.swingStage == PlayerSwingStage::Attack1 && hasSwingFollowup) {
          Animation& openerAnim = obj.animations[obj.currentAnimation];
          if (player.meleePressedThisFrame &&
              openerAnim.currentFrame() >= openerAnim.getFrameCount() / 2) {
            player.queuedFollowupSwing = true;
          }
        }

        if (attack1Done && player.queuedFollowupSwing && hasSwingFollowup) {
          if (obj.currentAnimation == ANIM_RUN_ATTACK) {
            obj.animations[ANIM_RUN_ATTACK].reset();
          } else if (obj.currentAnimation == ANIM_SWING) {
            obj.animations[ANIM_SWING].reset();
          }
          setAnimationAndPresentation(obj, ANIM_SWING_2, PresentationVariant::Swing2);
          player.swingStage = PlayerSwingStage::Attack2;
          player.queuedFollowupSwing = false;
          player.meleeDamage = 75;
          widenColliderForSwing(obj);
        } else if (attack1Done) {
          if (obj.currentAnimation == ANIM_RUN_ATTACK) {
            obj.animations[ANIM_RUN_ATTACK].reset();
          } else if (obj.currentAnimation == ANIM_SWING) {
            obj.animations[ANIM_SWING].reset();
          }
          restoreDefaultPlayerState();
        } else if (attack2Done) {
          obj.animations[ANIM_SWING_2].reset();
          restoreDefaultPlayerState();
        }
        break;
      }
      case PlayerState::ultimate: {
        currDirection = 0.0f;
        obj.velocity.x = 0.0f;
        expandColliderForUltimate(obj);
        setPresentation(obj, PresentationVariant::Ultimate);
        if (obj.currentAnimation == -1) {
          setAnimationAndPresentation(obj, ANIM_ULTIMATE, PresentationVariant::Ultimate);
          break;
        }
        if (obj.currentAnimation == ANIM_ULTIMATE &&
            obj.animations[ANIM_ULTIMATE].isDone()) {
          obj.animations[ANIM_ULTIMATE].reset();
          restoreDefaultPlayerState();
        }
        break;
      }
      case PlayerState::hurt: {
        resetSwingState();
        player.activeUltimateCastId = 0;
        obj.collider = baseFacing(obj);
        if (player.damageTimer.step(deltaTime)) {
          player.state = PlayerState::idle;
          setAnimationAndPresentation(obj, ANIM_IDLE, PresentationVariant::Idle);
        }
        break;
      }
      case PlayerState::dead: {
        resetSwingState();
        player.activeUltimateCastId = 0;
        obj.collider = baseFacing(obj);
        setPresentation(obj, PresentationVariant::Die);
        obj.velocity = glm::vec2(0.0f);
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          obj.currentAnimation = -1;
          obj.spriteFrame = 4;
          state.currentView = UIManager::GameView::GameOver;
        }
        break;
      }
    }
  } else if (obj.objClass == ObjectClass::Projectile) {
    obj.data.bullet.liveTimer.step(deltaTime);
    switch (obj.data.bullet.state) {
      case BulletState::moving: {
        setPresentation(obj, PresentationVariant::ProjectileMoving);
        const bool outsideViewport =
          hooks.cullProjectilesByViewport &&
          (obj.position.x - hooks.projectileViewport.x < 0.0f ||
           obj.position.x - hooks.projectileViewport.x > hooks.projectileViewport.w ||
           obj.position.y - hooks.projectileViewport.y < 0.0f ||
           obj.position.y - hooks.projectileViewport.y > hooks.projectileViewport.h);

        if (outsideViewport || obj.data.bullet.liveTimer.isTimedOut()) {
          obj.data.bullet.liveTimer.reset();
          obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
      case BulletState::colliding:
        setPresentation(obj, PresentationVariant::ProjectileHit);
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          obj.data.bullet.state = BulletState::inactive;
        }
        break;
      case BulletState::inactive:
        break;
    }
  } else if (obj.objClass == ObjectClass::Enemy) {
    auto& enemy = obj.data.enemy;
    const bool enemyFrozenByHitStop = stepEnemyHitStop(obj, deltaTime);

    switch (enemy.state) {
      case EnemyState::idle: {
        if (enemyFrozenByHitStop) {
          setAnimation(obj, ANIM_IDLE, false);
          setPresentation(obj, PresentationVariant::Idle);
          break;
        }

        GameObject* target = findClosestLivingPlayer(state, obj);
        if (!target) {
          obj.acceleration = glm::vec2(0.0f);
          obj.velocity.x = 0.0f;
          setAnimation(obj, ANIM_IDLE, false);
          setPresentation(obj, PresentationVariant::Idle);
          break;
        }

        const glm::vec2 distToPlayer = target->position - obj.position;
        if (glm::length(distToPlayer) < 100.0f) {
          currDirection = distToPlayer.x < 0.0f ? -1.0f : 1.0f;
          obj.acceleration = glm::vec2(30.0f, 0.0f);
          setAnimation(obj, ANIM_RUN, false);
          setPresentation(obj, PresentationVariant::Run);
        } else {
          obj.acceleration = glm::vec2(0.0f);
          obj.velocity.x = 0.0f;
          setAnimation(obj, ANIM_IDLE, false);
          setPresentation(obj, PresentationVariant::Idle);
        }
        break;
      }
      case EnemyState::attack:
        if (enemyFrozenByHitStop) {
          break;
        }
        if (enemy.idleTimer.step(deltaTime)) {
          enemy.state = EnemyState::idle;
          setAnimationAndPresentation(obj, ANIM_IDLE, PresentationVariant::Idle);
          enemy.idleTimer.reset();
          obj.collider = baseFacing(obj);
        }
        break;
      case EnemyState::hurt:
        if (enemyFrozenByHitStop) {
          break;
        }
        if (enemy.damageTimer.step(deltaTime)) {
          enemy.state = EnemyState::idle;
          setAnimationAndPresentation(obj, ANIM_IDLE, PresentationVariant::Idle);
          obj.collider = baseFacing(obj);
        }
        break;
      case EnemyState::dead:
        setPresentation(obj, PresentationVariant::Die);
        obj.velocity = glm::vec2(0.0f);
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          obj.currentAnimation = -1;
          obj.spriteFrame = 18;
        }
        break;
    }
  }

  if (currDirection != 0.0f && obj.direction != currDirection) {
    const float drawW = obj.spritePixelW / obj.drawScale;
    const float oldColliderX = obj.collider.x;
    const float newColliderX = drawW - obj.collider.x - obj.collider.w;
    obj.collider.x = newColliderX;
    obj.position.x -= (newColliderX - oldColliderX);
    obj.direction = currDirection;
  } else if (currDirection != 0.0f) {
    obj.direction = currDirection;
  }

  obj.velocity += currDirection * obj.acceleration * deltaTime;
  if (std::abs(obj.velocity.x) > obj.maxSpeedX) {
    obj.velocity.x = (obj.velocity.x < 0.0f ? -1.0f : 1.0f) * obj.maxSpeedX;
  }
  obj.position += obj.velocity * deltaTime;
}

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
                10,
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
              if (rectC.w < rectC.h) {
                blockHorizontalPassThrough();
              }
            }
          } else {
            objA.velocity = glm::vec2(50.0f, 0.0f) * -objA.direction;
          }
        }
        break;
      case ObjectClass::Portal:
        if (hooks.onPortalTriggered) {
          hooks.onPortalTriggered(objB.data.portal.nextLevel);
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

} // namespace

float hitStopDurationSeconds(HitStopStrength strength) {
  switch (strength) {
    case HitStopStrength::Heavy:
      return GameplayImpactTuning::heavyHitStopSeconds;
    case HitStopStrength::Normal:
    default:
      return GameplayImpactTuning::normalHitStopSeconds;
  }
}

uint16_t hitStopDurationMs(HitStopStrength strength) {
  return static_cast<uint16_t>(std::lround(hitStopDurationSeconds(strength) * 1000.0f));
}

float enemyKnockbackMagnitude(EnemyImpactType impactType) {
  switch (impactType) {
    case EnemyImpactType::Projectile:
      return GameplayImpactTuning::projectileEnemyKnockback;
    case EnemyImpactType::Ultimate:
      return GameplayImpactTuning::ultimateEnemyKnockback;
    case EnemyImpactType::Melee:
    default:
      return GameplayImpactTuning::meleeEnemyKnockback;
  }
}

void stepGameplaySimulation(
  GameState& state,
  const std::unordered_map<uint32_t, NetGameInput>& playerInputs,
  float deltaTime,
  const GameplaySimulationHooks& hooks) {
  for (auto& layer : state.layers) {
    for (auto& obj : layer) {
      if (obj.dynamic) {
        updateDynamicObject(state, obj, playerInputs, hooks, deltaTime);
      }
    }
  }

  for (auto& bullet : state.bullets) {
    updateDynamicObject(state, bullet, playerInputs, hooks, deltaTime);
  }

  for (auto& layer : state.layers) {
    for (auto& obj : layer) {
      if (obj.dynamic) {
        resolveObjectCollisions(state, obj, hooks);
      }
    }
  }

  for (auto& bullet : state.bullets) {
    resolveBulletCollisions(state, bullet, hooks);
  }

  state.bullets.erase(
    std::remove_if(
      state.bullets.begin(),
      state.bullets.end(),
      [](const GameObject& bullet) { return bullet.data.bullet.state == BulletState::inactive; }),
    state.bullets.end());

  purgeFinishedDeadEnemies(state);
}

} // namespace game_engine
