#include "game/default_systems.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "engine/engine.h"

namespace {

using game_engine::EntityResources;

struct SimContext {
  game_engine::Engine& engine;
  game_engine::GameState& gameState;
  game_engine::Resources& resources;
  game_engine::SDLState& sdlState;
};

static void updateGameObject(SimContext& ctx, GameObject& obj, float deltaTime);
static void handleCollision(SimContext& ctx, GameObject& a, GameObject& b, float deltaTime);
static void collisionResponse(
  SimContext& ctx,
  const SDL_FRect& rectA,
  const SDL_FRect& rectB,
  const SDL_FRect& rectC,
  GameObject& objA,
  GameObject& objB,
  float deltaTime);


static void updateAllObjects(SimContext& ctx, float deltaTime) {

  if (ctx.gameState.evaluateGameOver()) {
    ctx.engine.setGameOverSoundtrack();
  }

  // if singleplayer let is pass through normal logic
  for (auto &layer : ctx.gameState.layers) {
    for (GameObject &obj : layer) { // for each obj in layer
      // optimization to avoid n*m comparisions
      if (obj.dynamic) {
        updateGameObject(ctx, obj, deltaTime);
      }
      // if (obj.type == ObjectType::player) {
      //   bool left  = state.keys ? state.keys[SDL_SCANCODE_LEFT]  : false;
      //   bool right = state.keys ? state.keys[SDL_SCANCODE_RIGHT] : false;
      //   SDL_Log("pos=(%.2f,%.2f) vel=(%.2f,%.2f) left=%d right=%d", obj.position.x, obj.position.y, obj.velocity.x, obj.velocity.y, int(left), int(right));
      // }
    }
  }

  // update bullet physics
  for (GameObject &bullet : ctx.gameState.bullets) {
    updateGameObject(ctx, bullet, deltaTime);
  }


  // if multiplayer, we need to use the latest GameSnapshot to update the objects
  if (ctx.engine.isClientMode() || ctx.engine.isHostMode()) {
    // m_gameClient will have the latest snapshot so use a getter to get the data



  }

}

static void updateMapViewport(SimContext& ctx, GameObject& player) {
  if (!ctx.resources.m_currLevel || !ctx.resources.m_currLevel->map) return;

  int mapWpx = ctx.resources.m_currLevel->map->mapWidth * ctx.resources.m_currLevel->map->tileWidth;
  int mapHpx = ctx.resources.m_currLevel->map->mapHeight * ctx.resources.m_currLevel->map->tileHeight;

  ctx.gameState.mapViewport.x = std::clamp(
      (player.position.x + player.spritePixelW * 0.5f) - ctx.gameState.mapViewport.w * 0.5f,
      0.0f,
      std::max(0.0f, float(mapWpx - ctx.gameState.mapViewport.w)));

  ctx.gameState.mapViewport.y = std::clamp(
      (player.position.y + player.spritePixelH * 0.5f) - ctx.gameState.mapViewport.h * 0.5f,
      0.0f,
      std::max(0.0f, float(mapHpx - ctx.gameState.mapViewport.h)));
}

static void updateGameplayState(SimContext& ctx, float deltaTime, GameObject& player, const UIManager::UIActions& actions) {
if (ctx.gameState.currentView == UIManager::GameView::LevelLoading) return;


  if (ctx.gameState.currentView == UIManager::GameView::Playing ||
      ctx.gameState.currentView == UIManager::GameView::PauseMenu
  ) {
      ctx.engine.setBackgroundSoundtrack(); // TODO we will want to set background track per level

      // update & draw game world to sdl.renderer here (before ImGui::Render)
      if (!actions.blockGameplayUpdates) {
        updateAllObjects(ctx, deltaTime);
        updateMapViewport(ctx, player);
      }


      // debugging
      if (ctx.gameState.debugMode) {
        SDL_SetRenderDrawColor(ctx.sdlState.renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(
            ctx.sdlState.renderer,
            5,
            5,
            std::format("State: {}  Direction: {} B: {}, G: {}, Px: {}, Py:{}, VPx: {}", static_cast<int>(player.data.player.state), player.direction, ctx.gameState.bullets.size(), player.grounded, player.position.x, player.position.y, ctx.gameState.mapViewport.x).c_str());
      }
    }

}

static void updateGameObject(SimContext& ctx, GameObject &obj, float deltaTime) {

  EntityResources entityRes = ctx.resources.m_currLevel->texCharacterMap[obj.spriteType];

  if (obj.currentAnimation != -1) {
    obj.animations[obj.currentAnimation].step(deltaTime);
  }

  // gravity applied globally; downward y force when not grounded
  if (obj.dynamic && !obj.grounded) {
    // increase downward velocity = acc*deltaTime every frame
    obj.velocity += game_engine::Engine::GRAVITY * deltaTime;
  }

  // const auto widenColliderForSwing = [&](GameObject& o, float currDirection) {
  //   if (obj.objClass == ObjectClass::Player && obj.data.player.state == PlayerState::swingWeapon) {

  //     // const float drawW = o.spritePixelW / o.drawScale;
  //     // const float extra  = 0.2f * drawW;

  //     // SDL_FRect c = baseFacing(o);
  //     // c.w += extra;
  //     // if (currDirection < 0) c.x -= extra; // extend to the left
  //     // o.collider = c;

  //     const float drawW = o.spritePixelW / o.drawScale;
  //     const float extra  = 0.2f * drawW;           // how much to extend the sword
  //     const auto& base   = o.baseCollider;

  //     o.collider = base;
  //     o.collider.w = base.w + extra;
  //     if (currDirection < 0) {
  //         // facing left: shift left so the leading edge moves outward
  //         o.collider.x = base.x + extra;
  //     } else {
  //         // facing right: leave x as base; extension goes to the right
  //         o.collider.x = base.x;
  //     }
  //   }
  // };
  const auto baseFacing = [&](const GameObject& o) {
      SDL_FRect c = o.baseCollider;
      if (o.direction < 0) {
          float drawW = o.spritePixelW / o.drawScale;
          c.x = drawW - (c.x + c.w); // mirror the base for left
      }
      return c;
  };

  const auto widenColliderForSwing = [&](GameObject& o) {
      const float drawW = o.spritePixelW / o.drawScale;
      const float extra = 0.2f * drawW;

      SDL_FRect c = baseFacing(o);
      c.w += extra;
      if (o.direction < 0) c.x -= extra; // extend left
      o.collider = c;
  };

  float currDirection = 0;
  if (obj.objClass == ObjectClass::Player) {

    // update direction
    if (ctx.sdlState.keys[SDL_SCANCODE_LEFT]) {
      currDirection += -1;
    }
    if (ctx.sdlState.keys[SDL_SCANCODE_RIGHT]) {
      currDirection += 1;
    }

    Timer &weaponTimer = obj.data.player.weaponTimer;
    weaponTimer.step(deltaTime);

    obj.data.player.healthRecoveryTimer.step(deltaTime);
    obj.data.player.manaRecoveryTimer.step(deltaTime);

    if (obj.data.player.healthRecoveryTimer.isTimedOut()) {
      obj.data.player.healthRecoveryTimer.reset();
      obj.data.player.healthPoints = std::clamp(obj.data.player.healthPoints + 1, 0,
      obj.data.player.maxHealthPoints);
    }
    if (obj.data.player.manaRecoveryTimer.isTimedOut()) {
      obj.data.player.manaRecoveryTimer.reset();
      obj.data.player.manaPoints = std::clamp(obj.data.player.manaPoints + 1, 0,
      obj.data.player.maxManaPoints);
    }

    const auto handleAttacking = [&obj, &entityRes, &weaponTimer, &currDirection, deltaTime, widenColliderForSwing, &ctx](
      SDL_Texture *tex, SDL_Texture *attackTex, int animIndex, int attackAnimIndex, bool handleJump){


        if (ctx.sdlState.keys[SDL_SCANCODE_S] && obj.data.player.state != PlayerState::swingWeapon) {

          obj.texture = attackTex;
          obj.currentAnimation = attackAnimIndex;
          obj.animations[attackAnimIndex].reset();
          obj.data.player.state = PlayerState::swingWeapon;
          widenColliderForSwing(obj);
          MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioSword1);

        } else if (ctx.sdlState.keys[SDL_SCANCODE_A]) {

          obj.texture = attackTex;
          obj.currentAnimation = attackAnimIndex;

          if (obj.animations[attackAnimIndex].currentFrame() == 4) {
            // obj.animations[shootAnimIndex].freezeAtFrame();
            // obj.currentAnimation = -1;   // use spriteFrame path
            // obj.spriteFrame      = 4;

          } else {
            obj.currentAnimation = attackAnimIndex;
          }


          ctx.resources.whooshCooldown.step(deltaTime); // whooshCooldown should have same length as bullet weaponTimer
          // When you shoot (no loops, no track index needed):

          if (weaponTimer.isTimedOut() && obj.data.player.manaPoints > 10) {

            weaponTimer.reset();

            if (ctx.resources.whooshCooldown.isTimedOut()) {
              ctx.resources.whooshCooldown.reset();
              MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioShoot);
            }
            obj.data.player.manaPoints = std::clamp(obj.data.player.manaPoints -10, 0,
            obj.data.player.maxManaPoints);
            // create bullets
            GameObject bullet(128, 128);
            bullet.drawScale = 2.0f;
            bullet.colliderNorm = { .x=0.0, .y=0.40, .w=0.5, .h=0.1 };
            bullet.applyScale();

            bullet.data.bullet = BulletData();
            bullet.objClass = ObjectClass::Projectile;
            bullet.direction = obj.direction;
            bullet.texture = ctx.resources.texBullet;
            bullet.currentAnimation = ctx.resources.ANIM_BULLET_MOVING;
            const int yJitter = 50;
            const float yVelocity = SDL_rand(yJitter) - yJitter / 1.5f;

            const float baseSpeed = 200.0f;
            const float inherit   = obj.velocity.x * 0.2f; // optional
            bullet.velocity.x = baseSpeed * obj.direction + inherit;
            bullet.velocity.y = yVelocity;

            bullet.maxSpeedX = 1000.0f;
            bullet.animations = ctx.resources.bulletAnims;

            // adjust depending on direction faced; lerp
            const float left = -20;
            const float right = 20;
            const float t = (obj.direction + 1) / 1.0f; // 0 or 1 taking into account neg sign
            const float xOffset = left + right * t;
            bullet.position = glm::vec2(
              obj.position.x + xOffset,
              obj.position.y + (obj.spritePixelH/bullet.drawScale) / 8.0
            );

            bool foundInactive = false;
            for (int i = 0; i < ctx.gameState.bullets.size() && !foundInactive; i++) {
              if (ctx.gameState.bullets[i].data.bullet.state == BulletState::inactive) {
                foundInactive = true;
                ctx.gameState.bullets[i] = bullet;
              }
            }

            // only add new if no inactive found
            if (!foundInactive) {
              ctx.gameState.bullets.push_back(bullet); // push bullets so we can draw them
            }
          }


        } else if (handleJump) {
          if (obj.currentAnimation != ctx.resources.ANIM_JUMP &&
              obj.currentAnimation != -1) {
              obj.currentAnimation = ctx.resources.ANIM_JUMP;
              obj.animations[ctx.resources.ANIM_JUMP].reset();
              obj.texture = entityRes.texJump;
          }

          if (obj.currentAnimation == ctx.resources.ANIM_JUMP &&
              obj.animations[ctx.resources.ANIM_JUMP].isDone()) {
              obj.currentAnimation = -1;              // mark as finished so it won’t restart
          }

        } else {
          obj.animations[ctx.resources.ANIM_SHOOT].reset();
          obj.animations[ctx.resources.ANIM_SLIDE_SHOOT].reset();
          // obj.animations[shootAnimIndex].unfreezeAnim();
          // and then we need to set freezeAtFrame() when .currentFrame() == 4
          // and unfreezeAnim when SDL_SCANCODE_A is no longer being pressed and set obj.currentAnim accordingly
          obj.texture = tex;
          obj.currentAnimation = animIndex;
      }
    };

    const bool wantSwing = ctx.sdlState.keys[SDL_SCANCODE_S];
    const bool canSwing  = (obj.data.player.state != PlayerState::swingWeapon);
    // update animation state
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        obj.collider = baseFacing(obj);
        if (currDirection != 0) {
          obj.data.player.state = PlayerState::running;
          obj.texture = entityRes.texRun;
          obj.currentAnimation = ctx.resources.ANIM_RUN;
        } else {
          // decelerate faster than we speed up
          if (obj.velocity.x) {
            const float factor = obj.velocity.x > 0 ? -1.5f : 1.5f;
            float amount = factor * obj.acceleration.x * deltaTime;
            if (std::abs(obj.velocity.x) < std::abs(amount)) {
              obj.velocity.x = 0;
            } else {
              obj.velocity.x += amount;
            }
          }
        }

        if (wantSwing && canSwing) {
            handleAttacking(entityRes.texRun, entityRes.texRunAttack, ctx.resources.ANIM_RUN, ctx.resources.ANIM_RUN_ATTACK, false);
        } else {
            handleAttacking(entityRes.texIdle, entityRes.texShoot, ctx.resources.ANIM_IDLE, ctx.resources.ANIM_SHOOT, false);
        }

        break;
      }
      case PlayerState::hurt: {
        if (obj.data.player.damageTimer.step(deltaTime)) {
          obj.data.player.state = PlayerState::idle;
          obj.texture = entityRes.texIdle;
          obj.currentAnimation = ctx.resources.ANIM_IDLE;
        }
        break;
      }
      case PlayerState::dead:
      {
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          obj.currentAnimation = -1; // prevent animation from looping after death
          obj.spriteFrame = 4;

          ctx.gameState.currentView = UIManager::GameView::GameOver;
          ctx.engine.setGameOverSoundtrack();
        }
        break;
      }
      case PlayerState::running:
      {
        if (currDirection == 0) {
          obj.data.player.state = PlayerState::idle;
        }

        ctx.resources.stepAudioCooldown.step(deltaTime);
        if (ctx.resources.stepAudioCooldown.isTimedOut()) {
          ctx.resources.stepAudioCooldown.reset();
          MIX_PlayAudio(ctx.resources.mixer, ctx.resources.m_currLevel->audioStep);
        }

        // move in opposite dir of velocity, sliding
        if (obj.velocity.x * obj.direction < 0 && obj.grounded) {
          handleAttacking(entityRes.texSlide, entityRes.texSlideShoot, ctx.resources.ANIM_SLIDE, ctx.resources.ANIM_SLIDE_SHOOT, false);
        } else {
          if (wantSwing && canSwing) {
            handleAttacking(entityRes.texRun, entityRes.texRunAttack, ctx.resources.ANIM_RUN, ctx.resources.ANIM_RUN_ATTACK, false);
          } else {
            handleAttacking(entityRes.texRun, entityRes.texRunShoot, ctx.resources.ANIM_RUN, ctx.resources.ANIM_RUN, false);
          }
        }

        break;
      }
      case PlayerState::jumping:
      {

        if (!obj.data.player.jumpImpulseApplied) {
          obj.data.player.jumpWindupTimer.step(deltaTime);
          if (obj.data.player.jumpWindupTimer.isTimedOut()) {
              obj.velocity.y += game_engine::Engine::JUMP_FORCE;   // upward impulse
              obj.data.player.jumpImpulseApplied = true;
              MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioJump);
          }
        } else {
          int n = obj.animations[ctx.resources.ANIM_JUMP].getFrameCount(); // e.g. 6 frames -> indices 0..5

          // Airborne: hold second‑to‑last frame once reached
          if (!obj.grounded && obj.currentAnimation == ctx.resources.ANIM_JUMP) {
              if (obj.animations[ctx.resources.ANIM_JUMP].currentFrame() >= n - 2) {
                  obj.currentAnimation = -1;           // stop timered anim
                  obj.spriteFrame = (n - 2) + 1;       // freeze on frame n-2 (1‑based)
                  obj.data.player.playLandingFrame = true;
              }
          }

          if (obj.grounded) {
              if (obj.data.player.playLandingFrame) {
                  obj.currentAnimation = -1;           // show landing frame once
                  obj.spriteFrame = (n - 1) + 1;       // last frame (1‑based)
                  obj.data.player.playLandingFrame = false;
                  break;                               // render this frame; state stays jumping this tick
              } else {
                  obj.velocity.y = 0;
                  obj.data.player.state = PlayerState::idle; // or running
                  obj.animations[ctx.resources.ANIM_JUMP].reset();
              }
          }
        }

        if (wantSwing && canSwing) {
          handleAttacking(entityRes.texRun, entityRes.texRunAttack, ctx.resources.ANIM_RUN, ctx.resources.ANIM_RUN_ATTACK, false);
        } else {
          handleAttacking(entityRes.texJump, entityRes.texRunShoot, ctx.resources.ANIM_JUMP, ctx.resources.ANIM_JUMP, true);
        }
        break;
      }
      case PlayerState::swingWeapon: { // handle swinging weapon like handleShooting
        // sets to idle immediately in next loop even when currentAnimation=10
        if (obj.currentAnimation == ctx.resources.ANIM_RUN_ATTACK &&
            obj.animations[ctx.resources.ANIM_RUN_ATTACK].isDone()) {

            obj.collider = baseFacing(obj);

            obj.data.player.state = PlayerState::idle;
            obj.texture = entityRes.texIdle;
            obj.currentAnimation = ctx.resources.ANIM_IDLE;
            obj.animations[ctx.resources.ANIM_RUN_ATTACK].reset();
            obj.animations[ctx.resources.ANIM_IDLE].reset();
        } else if (obj.currentAnimation == ctx.resources.ANIM_SWING &&
                  obj.animations[ctx.resources.ANIM_SWING].isDone()) {
            obj.data.player.state = PlayerState::idle;
            obj.collider = baseFacing(obj);
            obj.texture = entityRes.texIdle;
            obj.currentAnimation = ctx.resources.ANIM_IDLE;
            obj.animations[ctx.resources.ANIM_SWING].reset();
            obj.animations[ctx.resources.ANIM_IDLE].reset();
        }

        break;
      }
    }
  } else if (obj.objClass == ObjectClass::Projectile) {

    obj.data.bullet.liveTimer.step(deltaTime);
    switch (obj.data.bullet.state) {
      case BulletState::moving:
      {
        if (
          obj.position.x - ctx.gameState.mapViewport.x < 0 ||
          obj.position.x - ctx.gameState.mapViewport.x > ctx.sdlState.logW ||
          obj.position.y - ctx.gameState.mapViewport.y < 0 ||
          obj.position.y - ctx.gameState.mapViewport.y > ctx.sdlState.logH ||
          obj.data.bullet.liveTimer.isTimedOut()
        ) {
            obj.data.bullet.liveTimer.reset();
            // obj.position.x = 0; obj.position.y = 0;
            obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
      case BulletState::colliding:
      {
        if (obj.animations[obj.currentAnimation].isDone()) {
          obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
    }
  } else if (obj.objClass == ObjectClass::Enemy) {

    EntityResources entityRes = ctx.resources.m_currLevel->texCharacterMap[obj.spriteType];
    switch (obj.data.enemy.state) {
      case EnemyState::idle:
      {
        // obj.collider = baseFacing(obj);
        glm::vec2 distToPlayer = ctx.engine.getPlayer().position - obj.position;
        if (glm::length(distToPlayer) < 100) {
          // face the enemy towards the player
          currDirection = 1;
          if (distToPlayer.x < 0) {
            currDirection = -1;
          };
          obj.acceleration = glm::vec2(30, 0);
          obj.texture = entityRes.texRun;


          // step the attack timer here
          // if the timer is done switch state to attack
          if (obj.data.enemy.attackTimer.step(deltaTime)) {
            obj.data.enemy.state = EnemyState::attack;
            obj.texture = entityRes.texAttack;
            obj.currentAnimation = ctx.resources.ANIM_SWING;
            obj.data.enemy.attackTimer.reset();
            widenColliderForSwing(obj);
          }


        } else {
          // stop them from moving when too far away
          obj.acceleration = glm::vec2(0);
          obj.velocity.x = 0;
          obj.texture = entityRes.texIdle;
        }

        break;
      }
      case EnemyState::attack:
      {
        if (obj.data.enemy.idleTimer.step(deltaTime)) {
          obj.data.enemy.state = EnemyState::idle;
          obj.texture = entityRes.texIdle;
          obj.currentAnimation = ctx.resources.ANIM_IDLE;
          obj.data.enemy.idleTimer.reset();
          obj.collider = baseFacing(obj);  // revert to normal collider
        }
      }
      case EnemyState::hurt:
      {
        if (obj.data.enemy.damageTimer.step(deltaTime)) {
          obj.data.enemy.state = EnemyState::idle;
          obj.texture = entityRes.texIdle;
          obj.currentAnimation = ctx.resources.ANIM_IDLE;
          obj.collider = baseFacing(obj);
        }
        break;
      }
      case EnemyState::dead:
      {
        obj.velocity.x = 0;
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          // stop animations for dead enemy
          obj.currentAnimation = -1;
          obj.spriteFrame = 18; // TODO this is because enemy has 18 frames
        }
        break;
      }
    }
  }

  if (currDirection && obj.direction != currDirection) {
    // Direction changed - mirror the collision box offset to match flipped character
    // and adjust position to keep collision box at same world position
    float drawW = obj.spritePixelW / obj.drawScale;
    float oldColliderX = obj.collider.x;
    float newColliderX = drawW - obj.collider.x - obj.collider.w;
    obj.collider.x = newColliderX;
    float positionShift = newColliderX - oldColliderX;
    obj.position.x -= positionShift; // Keep world position constant

    // If this is the player, adjust camera to prevent screen jump
    // if (obj.objClass == ObjectClass::Player) {
    //   // ctx.gameState.mapViewport.x -= positionShift/2;
    // }

    obj.direction = currDirection;
  } else if (currDirection) {
    obj.direction = currDirection;
  }
  // update velocity based on currDirection (which way we're facing),
  // acceleration and deltaTime
  obj.velocity += currDirection * obj.acceleration * deltaTime;
  if (std::abs(obj.velocity.x) > obj.maxSpeedX) { // cap the max velocity
    obj.velocity.x = currDirection * obj.maxSpeedX;
  }
  // update position based on velocity
  obj.position += obj.velocity * deltaTime;

  // handle collision detection
  bool foundGround = false;
  for (auto &layer : ctx.gameState.layers) {
    for (GameObject &objB: layer){
      // if (obj.type == ObjectType::enemy) {
      //   std::cout << "found Ground" << foundGround << std::endl;
      // }
      if (&obj != &objB && objB.collider.h != 0 && objB.collider.w != 0) {
        handleCollision(ctx, obj, objB, deltaTime);

        // update ground sensor only when landing on level tiles
        if (objB.objClass == ObjectClass::Level) {
          SDL_FRect sensor{
            .x = obj.position.x + obj.collider.x,
            .y = obj.position.y + obj.collider.y + obj.collider.h,
            .w = obj.collider.w, .h = 1
          };

          SDL_FRect rectB{
            .x = objB.position.x + objB.collider.x,
            .y = objB.position.y + objB.collider.y,
            .w = objB.collider.w, .h = objB.collider.h
          };

          SDL_FRect dummyRectC{0};

          if (SDL_GetRectIntersectionFloat(&sensor, &rectB, &dummyRectC)) {
            foundGround = true;
          }
        }

      }
    }
  }

  if (obj.grounded != foundGround) {
    // switching grounded state
    obj.grounded = foundGround;
    if (foundGround && obj.objClass == ObjectClass::Player && !obj.data.player.playLandingFrame) {
        obj.data.player.state = PlayerState::running;

        if (obj.grounded && obj.data.player.jumpImpulseApplied) {
          obj.data.player.state = PlayerState::idle;
          obj.data.player.jumpImpulseApplied = false;
          obj.data.player.jumpWindupTimer.reset();
        }
      }

  }
}

/*
 collisionResponse will dictates what you want to happen given a collision has been detected
 The defaultResponse handles vertical and horizontal collisions by preventing sprites from overlapping due to collision
*/
static void collisionResponse(SimContext& ctx, const SDL_FRect &rectA, const SDL_FRect &rectB, const SDL_FRect &rectC, GameObject &objA, GameObject &objB, float deltaTime) {

  // logRectEvery(rectC, 100000);
  const auto defaultResponse = [&]() {
    if (rectC.w < rectC.h) {
      // horizontal collision
      if (objA.velocity.x > 0) {
        // traveling to right, colliding object must be to the right, so sub .w; need extra 0.1 to escape collision for next frame
        objA.position.x -= rectC.w+0.1;
      } else if (objA.velocity.x < 0) {
        objA.position.x += rectC.w+0.1;
      }
      objA.velocity.x = 0; // reset velocity to 0 so object stops
    } else {
      //vertical collison
      if (objA.velocity.y > 0) {
        objA.position.y -= rectC.h; // down
      } else if (objA.velocity.y < 0) {
        objA.position.y += rectC.h; // up
      }
      objA.velocity.y = 0;
    }
  };

  if (objA.objClass == ObjectClass::Player) {
    switch (objB.objClass) {
      case ObjectClass::Level:
      {
        if (objB.data.level.isHazard) {
            EntityResources entityRes = ctx.resources.m_currLevel->texCharacterMap.at(objA.spriteType);
            PlayerData &d = objA.data.player;
            if (d.state != PlayerState::dead) {
              // objA.direction = -1 * objA.direction;
              objA.position.y -= rectC.h; // up
              objA.shouldFlash = true;
              objA.flashTimer.reset();
              objA.texture = entityRes.texHit;
              objA.currentAnimation = ctx.resources.ANIM_HIT;
              d.state = PlayerState::hurt;

              // damage and flag dead
              if (d.damageTimer.isTimedOut()) {
                d.healthPoints -= 50.0;
                d.damageTimer.reset();
              }
              if (d.healthPoints <= 0) {
                d.state = PlayerState::dead;
                objA.texture = entityRes.texDie;
                objA.currentAnimation = ctx.resources.ANIM_DIE;
                MIX_PlayTrack(ctx.resources.enemyDieTrack, 0);
                objA.velocity = glm::vec2(0);
                // ctx.gameState.currentView = GameView::GameOver;
              }
              MIX_PlayTrack(ctx.resources.boneImpactHitTrack, 0);
            }

        } else {
          defaultResponse();
        }
        break;
      }
      case ObjectClass::Enemy:
      {
        if (objB.data.enemy.state != EnemyState::dead) {


          if (objA.data.player.state == PlayerState::swingWeapon) {
            // when swinging weapon and colliding, reduce enemy health
            EntityResources entityRes = ctx.resources.m_currLevel->texCharacterMap.at(objB.spriteType);
            EnemyData &d = objB.data.enemy;

            // function damageObject(state, damageAmount, entityRes, dieTrack, hitTrack)
            if (d.state != EnemyState::dead) {
              objB.direction = -1 * objA.direction;
              objB.shouldFlash = true;
              objB.flashTimer.reset();
              objB.texture = entityRes.texHit;
              objB.currentAnimation = ctx.resources.ANIM_HIT;
              d.state = EnemyState::hurt;

              if (d.damageTimer.isTimedOut()) {
                d.healthPoints -= 50;
                d.damageTimer.reset();
              }

              if (d.healthPoints <= 0) {
                d.state = EnemyState::dead;
                objB.texture = entityRes.texDie;
                objB.currentAnimation = ctx.resources.ANIM_DIE;
                MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioEnemyDie);
              }
              // MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioBoneImpact);
              MIX_PlayTrack(ctx.resources.boneImpactHitTrack, 0);
              // TODO wrap this in a func() so that it doesnt allow more than one sound to occur until its done.
              // PlayTrack doesnt allow repeating while the track is already playing so might be better to use it instead actually.
            }
          } else {
            // push back player
            objA.velocity = glm::vec2(50, 0) * - objA.direction;
          }

          // defaultResponse(); // TODO need to handle correctly
        }
        break;
      }
      case ObjectClass::Portal:
      {
        game::switchToLevel(ctx.engine, objB.data.portal.nextLevel);
        break;
      }
      case ObjectClass::Player:
      {
        break;
      }
    }
  } else if (objA.objClass == ObjectClass::Projectile) {

    bool passthrough = false;
    switch (objA.data.bullet.state) {
      case BulletState::moving:
      {
        switch (objB.objClass) {
          case ObjectClass::Level:
          {
            if (!MIX_PlayTrack(ctx.resources.hitTrack, 0)) {
            // SDL_Log("Play failed: %s", SDL_GetError());
            };
            // if (!MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioShootHit)) {
            // // SDL_Log("Play failed: %s", SDL_GetError());
            // };

            break;
          }
          case ObjectClass::Enemy:
          {

            EntityResources entityRes = ctx.resources.m_currLevel->texCharacterMap.at(objB.spriteType);
            EnemyData &d = objB.data.enemy;

            // function damageObject(state, damageAmount, entityRes, dieTrack, hitTrack)
            if (d.state != EnemyState::dead) {
              objB.direction = -1 * objA.direction;
              objB.shouldFlash = true;
              objB.flashTimer.reset();
              objB.texture = entityRes.texHit;
              objB.currentAnimation = ctx.resources.ANIM_HIT;
              d.state = EnemyState::hurt;

              // damage and flag dead
              if (d.damageTimer.isTimedOut()) {
                d.healthPoints -= 50;
                d.damageTimer.reset();
              }

              if (d.healthPoints <= 0) {
                d.state = EnemyState::dead;
                objB.texture = entityRes.texDie;
                objB.currentAnimation = ctx.resources.ANIM_DIE;
                MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioEnemyDie);
              }
              // MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioProjectileEnemyHit);
              MIX_PlayTrack(ctx.resources.enemyProjectileHitTrack, 0);
            } else {
              passthrough = true;
            }
            break;
          }
          case ObjectClass::Player:
          {
             passthrough = true;
          }
        }

        if (!passthrough) {
          defaultResponse();
          objA.velocity *= 0;
          objA.data.bullet.state = BulletState::colliding;
          objA.texture = ctx.resources.texBulletHit;
          objA.currentAnimation = ctx.resources.ANIM_BULLET_HIT;
        }
        break;
      }
    }

  }
  else if (objA.objClass == ObjectClass::Enemy) {
    switch (objB.objClass) {
      case ObjectClass::Player:
      {
        if (objA.data.enemy.state == EnemyState::attack) {
          EntityResources playerRes = ctx.resources.m_currLevel->texCharacterMap.at(objB.spriteType);
            PlayerData &playerData = objB.data.player;
            if (playerData.state != PlayerState::dead) {
              // objA.direction = -1 * objA.direction;
              // objB.position.y -= rectC.h; // up
              objB.shouldFlash = true;
              objB.flashTimer.reset();
              objB.texture = playerRes.texHit;
              objB.currentAnimation = ctx.resources.ANIM_HIT;
              playerData.state = PlayerState::hurt;

              // damage and flag dead
              if (playerData.damageTimer.isTimedOut()) {
                playerData.healthPoints -= 33.0;
                playerData.damageTimer.reset();
              }

              if (playerData.healthPoints <= 0) {
                playerData.state = PlayerState::dead;
                objB.texture = playerRes.texDie;
                objB.currentAnimation = ctx.resources.ANIM_DIE;
                MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioEnemyDie);
                objB.velocity = glm::vec2(0);
              }
              MIX_PlayTrack(ctx.resources.boneImpactHitTrack, 0);
            }
        }
        // if collision if with player and enemy is swinging
        // then reduce players health and do damage
        break;
      }
      case ObjectClass::Level:
      {
        if (objB.data.level.isHazard) {
            EntityResources entityRes = ctx.resources.m_currLevel->texCharacterMap.at(objA.spriteType);
            EnemyData &d = objA.data.enemy;
            if (d.state != EnemyState::dead) {
              // objA.direction = -1 * objA.direction;
              objA.position.y -= rectC.h; // up
              objA.shouldFlash = true;
              objA.flashTimer.reset();
              objA.texture = entityRes.texHit;
              objA.currentAnimation = ctx.resources.ANIM_HIT;
              d.state = EnemyState::hurt;

              // damage and flag dead
              if (d.damageTimer.isTimedOut()) {
                d.healthPoints -= 50;
                d.damageTimer.reset();
              }

              if (d.healthPoints <= 0) {
                d.state = EnemyState::dead;
                objA.texture = entityRes.texDie;
                objA.currentAnimation = ctx.resources.ANIM_DIE;
                MIX_PlayAudio(ctx.resources.mixer, ctx.resources.audioEnemyDie);
                objA.velocity = glm::vec2(0);
                // ctx.gameState.currentView = GameView::GameOver;
              }
              MIX_PlayTrack(ctx.resources.boneImpactHitTrack, 0);
            }

        } else {
          defaultResponse();
        }
        break;
      }
      case ObjectClass::Enemy:
      {
        if (objB.data.enemy.state != EnemyState::dead) {
          objA.velocity = glm::vec2(50, 0) * - objA.direction;
        }
        break;
      }
    }
  }

}

static void handleCollision(SimContext& ctx, GameObject &a, GameObject &b, float deltaTime) {

  SDL_FRect rectA{
    .x = a.position.x + a.collider.x,
    .y = a.position.y + a.collider.y,
    .w = a.collider.w,
    .h = a.collider.h
  };
  SDL_FRect rectB{
    .x = b.position.x + b.collider.x,
    .y = b.position.y + b.collider.y,
    .w = b.collider.w,
    .h = b.collider.h
  };
  SDL_FRect rectC{ 0 };

  if (SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC)) {
    // found interection
    collisionResponse(ctx, rectA, rectB, rectC, a, b, deltaTime);
  };
};

class DefaultSimulationSystem final : public game::ISimulationSystem {
public:
  void update(game_engine::Engine& engine, float deltaTime, const UIManager::UIActions& actions) override {
    SimContext ctx{engine, engine.getGameState(), engine.getResources(), engine.getSDLState()};
    auto& player = engine.getPlayer();
    updateGameplayState(ctx, deltaTime, player, actions);
  }
};

} // namespace

namespace game {

std::unique_ptr<ISimulationSystem> createDefaultSimulationSystem() {
  return std::make_unique<DefaultSimulationSystem>();
}

} // namespace game
