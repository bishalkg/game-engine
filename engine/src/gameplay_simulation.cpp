#include "engine/gameplay_simulation.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "engine/engine.h"

namespace game_engine {
namespace {

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

void damageEnemy(GameObject& enemy, int damage) {
  if (enemy.objClass != ObjectClass::Enemy || enemy.data.enemy.state == EnemyState::dead) {
    return;
  }
  if (enemy.data.enemy.state == EnemyState::hurt && !enemy.data.enemy.damageTimer.isTimedOut()) {
    return;
  }

  enemy.direction *= -1.0f;
  enemy.shouldFlash = true;
  enemy.flashTimer.reset();
  enemy.data.enemy.state = EnemyState::hurt;
  setAnimation(enemy, ANIM_HIT);
  enemy.data.enemy.healthPoints -= damage;
  enemy.data.enemy.damageTimer.reset();

  if (enemy.data.enemy.healthPoints <= 0) {
    enemy.data.enemy.healthPoints = 0;
    enemy.data.enemy.state = EnemyState::dead;
    setAnimation(enemy, ANIM_DIE);
    enemy.velocity = glm::vec2(0.0f);
  }
}

void damagePlayer(GameObject& player, int damage) {
  if (player.objClass != ObjectClass::Player || player.data.player.state == PlayerState::dead) {
    return;
  }
  if (player.data.player.state == PlayerState::hurt && !player.data.player.damageTimer.isTimedOut()) {
    return;
  }

  player.shouldFlash = true;
  player.flashTimer.reset();
  player.data.player.state = PlayerState::hurt;
  setAnimation(player, ANIM_HIT);
  player.data.player.healthPoints -= damage;
  player.data.player.damageTimer.reset();

  if (player.data.player.healthPoints <= 0) {
    player.data.player.healthPoints = 0;
    player.data.player.state = PlayerState::dead;
    setAnimation(player, ANIM_DIE);
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
  bullet.currentAnimation = ANIM_IDLE;
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
    player.meleePressedThisFrame = input.meleePressed;

    if (player.healthRecoveryTimer.isTimedOut()) {
      player.healthRecoveryTimer.reset();
      player.healthPoints = std::clamp(player.healthPoints + 1, 0, player.maxHealthPoints);
    }
    if (player.manaRecoveryTimer.isTimedOut()) {
      player.manaRecoveryTimer.reset();
      player.manaPoints = std::clamp(player.manaPoints + 1, 0, player.maxManaPoints);
    }

    if (input.leftHeld) {
      currDirection -= 1.0f;
    }
    if (input.rightHeld) {
      currDirection += 1.0f;
    }

    const bool hasSwingFollowup =
      static_cast<int>(obj.animations.size()) > ANIM_SWING_2 &&
      obj.animations[ANIM_SWING_2].getFrameCount() > 0;

    const auto resetSwingState = [&obj]() {
      obj.data.player.swingStage = PlayerSwingStage::None;
      obj.data.player.queuedFollowupSwing = false;
      obj.data.player.meleeDamage = 50;
    };

    const auto exitSwingState = [&]() {
      resetSwingState();
      obj.collider = baseFacing(obj);

      if (!obj.grounded) {
        obj.data.player.state = PlayerState::jumping;
        setAnimation(obj, ANIM_JUMP);
        return;
      }

      if (currDirection != 0.0f) {
        obj.data.player.state = PlayerState::running;
        setAnimation(obj, ANIM_RUN);
        return;
      }

      obj.data.player.state = PlayerState::idle;
      setAnimation(obj, ANIM_IDLE);
    };

    const auto startAttack1 = [&]() {
      resetSwingState();
      obj.data.player.state = PlayerState::swingWeapon;
      obj.data.player.swingStage = PlayerSwingStage::Attack1;
      setAnimation(obj, currDirection != 0.0f ? ANIM_RUN_ATTACK : ANIM_SWING);
      widenColliderForSwing(obj);
    };

    const auto handleAttacking = [&](int idleOrMoveAnim, int shootAnim, bool handleJump) {
      if (player.meleePressedThisFrame && player.state != PlayerState::swingWeapon) {
        startAttack1();
      } else if (input.fireHeld) {
        setAnimation(obj, shootAnim, false);

        if (player.weaponTimer.isTimedOut() && player.manaPoints > 10) {
          player.weaponTimer.reset();
          player.manaPoints = std::clamp(player.manaPoints - 2, 0, player.maxManaPoints);
          state.bullets.push_back(makeBulletFromPlayer(obj, state));
        }
      } else if (handleJump) {
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
      }
    };

    switch (player.state) {
      case PlayerState::idle: {
        obj.collider = baseFacing(obj);
        if (input.jumpPressed && obj.grounded) {
          player.state = PlayerState::jumping;
          player.jumpWindupTimer.reset();
          player.jumpImpulseApplied = false;
          setAnimation(obj, ANIM_JUMP);
          break;
        }
        if (currDirection != 0.0f) {
          player.state = PlayerState::running;
          setAnimation(obj, ANIM_RUN);
        } else if (obj.velocity.x != 0.0f) {
          const float factor = obj.velocity.x > 0.0f ? -1.5f : 1.5f;
          const float amount = factor * obj.acceleration.x * deltaTime;
          if (std::abs(obj.velocity.x) < std::abs(amount)) {
            obj.velocity.x = 0.0f;
          } else {
            obj.velocity.x += amount;
          }
        }

        handleAttacking(ANIM_IDLE, ANIM_SHOOT, false);
        break;
      }
      case PlayerState::running: {
        if (input.jumpPressed && obj.grounded) {
          player.state = PlayerState::jumping;
          player.jumpWindupTimer.reset();
          player.jumpImpulseApplied = false;
          setAnimation(obj, ANIM_JUMP);
          break;
        }
        if (currDirection == 0.0f) {
          player.state = PlayerState::idle;
        }

        if (obj.velocity.x * obj.direction < 0.0f && obj.grounded) {
          handleAttacking(ANIM_SLIDE, ANIM_SLIDE_SHOOT, false);
        } else {
          handleAttacking(ANIM_RUN, ANIM_RUN, false);
        }
        break;
      }
      case PlayerState::jumping: {
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

        handleAttacking(ANIM_JUMP, ANIM_JUMP, true);
        break;
      }
      case PlayerState::swingWeapon: {
        const bool attack1Done =
          (obj.currentAnimation == ANIM_RUN_ATTACK && obj.animations[ANIM_RUN_ATTACK].isDone()) ||
          (obj.currentAnimation == ANIM_SWING && obj.animations[ANIM_SWING].isDone());
        const bool attack2Done =
          obj.currentAnimation == ANIM_SWING_2 && obj.animations[ANIM_SWING_2].isDone();

        if (obj.currentAnimation == -1) {
          exitSwingState();
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
          obj.animations[obj.currentAnimation].reset();
          setAnimation(obj, ANIM_SWING_2);
          player.swingStage = PlayerSwingStage::Attack2;
          player.queuedFollowupSwing = false;
          player.meleeDamage = 75;
          widenColliderForSwing(obj);
        } else if (attack1Done) {
          obj.animations[obj.currentAnimation].reset();
          exitSwingState();
        } else if (attack2Done) {
          obj.animations[ANIM_SWING_2].reset();
          exitSwingState();
        }
        break;
      }
      case PlayerState::hurt: {
        resetSwingState();
        obj.collider = baseFacing(obj);
        if (player.damageTimer.step(deltaTime)) {
          player.state = PlayerState::idle;
          setAnimation(obj, ANIM_IDLE);
        }
        break;
      }
      case PlayerState::dead: {
        resetSwingState();
        obj.collider = baseFacing(obj);
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
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          obj.data.bullet.state = BulletState::inactive;
        }
        break;
      case BulletState::inactive:
        break;
    }
  } else if (obj.objClass == ObjectClass::Enemy) {
    auto& enemy = obj.data.enemy;

    enemy.attackTimer.step(deltaTime);

    switch (enemy.state) {
      case EnemyState::idle: {
        GameObject* target = findClosestLivingPlayer(state, obj);
        if (!target) {
          obj.acceleration = glm::vec2(0.0f);
          obj.velocity.x = 0.0f;
          setAnimation(obj, ANIM_IDLE, false);
          break;
        }

        const glm::vec2 distToPlayer = target->position - obj.position;
        if (glm::length(distToPlayer) < 100.0f) {
          currDirection = distToPlayer.x < 0.0f ? -1.0f : 1.0f;
          obj.acceleration = glm::vec2(30.0f, 0.0f);
          setAnimation(obj, ANIM_RUN, false);

          if (enemy.attackTimer.isTimedOut()) {
            enemy.state = EnemyState::attack;
            setAnimation(obj, ANIM_SWING);
            enemy.attackTimer.reset();
            widenColliderForSwing(obj);
          }
        } else {
          obj.acceleration = glm::vec2(0.0f);
          obj.velocity.x = 0.0f;
          setAnimation(obj, ANIM_IDLE, false);
        }
        break;
      }
      case EnemyState::attack:
        if (enemy.idleTimer.step(deltaTime)) {
          enemy.state = EnemyState::idle;
          setAnimation(obj, ANIM_IDLE);
          enemy.idleTimer.reset();
          obj.collider = baseFacing(obj);
        }
        break;
      case EnemyState::hurt:
        if (enemy.damageTimer.step(deltaTime)) {
          enemy.state = EnemyState::idle;
          setAnimation(obj, ANIM_IDLE);
          obj.collider = baseFacing(obj);
        }
        break;
      case EnemyState::dead:
        obj.velocity.x = 0.0f;
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
    obj.velocity.x = currDirection * obj.maxSpeedX;
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
          if (objA.data.player.state == PlayerState::swingWeapon) {
            damageEnemy(objB, objA.data.player.meleeDamage);
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
          damageEnemy(objB, 10);
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
      setAnimation(objA, ANIM_RUN);
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
          damageEnemy(objA, 50);
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

      const SDL_FRect rectA = worldRect(obj);
      const SDL_FRect rectB = worldRect(objB);
      SDL_FRect rectC{0.0f, 0.0f, 0.0f, 0.0f};
      if (!SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC)) {
        if (objB.objClass == ObjectClass::Level) {
          SDL_FRect sensor{
            .x = obj.position.x + obj.collider.x,
            .y = obj.position.y + obj.collider.y + obj.collider.h,
            .w = obj.collider.w,
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
        SDL_FRect sensor{
          .x = obj.position.x + obj.collider.x,
          .y = obj.position.y + obj.collider.y + obj.collider.h,
          .w = obj.collider.w,
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
      const SDL_FRect rectB = worldRect(objB);
      SDL_FRect rectC{0.0f, 0.0f, 0.0f, 0.0f};
      if (SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC)) {
        collisionResponse(state, bullet, objB, rectC, hooks);
      }
    }
  }
}

} // namespace

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
}

} // namespace game_engine
