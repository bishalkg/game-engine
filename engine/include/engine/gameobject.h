#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "engine/animation.h"
#include <SDL3/SDL.h>
#include "engine/level_types.h"

enum class PlayerState: std::uint32_t {
  idle, running, jumping, swingWeapon, ultimate, hurt, dead
};

enum class PlayerSwingStage: std::uint32_t {
  None,
  Attack1,
  Attack2,
};

enum class BulletState: std::uint32_t {
  moving, colliding, inactive
};

enum class EnemyState: std::uint32_t {
  idle, hurt, dead, attack
};

enum class PresentationVariant : std::uint32_t {
  Idle,
  Run,
  Slide,
  Shoot,
  RunShoot,
  SlideShoot,
  Jump,
  JumpShoot,
  Swing,
  RunAttack,
  Swing2,
  Ultimate,
  Hit,
  Die,
  ProjectileMoving,
  ProjectileHit,
};

struct PlayerData {
  PlayerState state;
  Timer damageTimer;
  int healthPoints;
  int maxHealthPoints;
  int manaPoints;
  int maxManaPoints;
  int ultimatePoints;
  int maxUltimatePoints;
  Timer manaRecoveryTimer;
  Timer healthRecoveryTimer;
  Timer ultimateRecoveryTimer;
  Timer weaponTimer;
  Timer jumpWindupTimer;
  bool jumpImpulseApplied;
  bool playLandingFrame = false;
  PlayerSwingStage swingStage = PlayerSwingStage::None;
  bool queuedFollowupSwing = false;
  bool meleePressedThisFrame = false;
  bool ultimatePressedThisFrame = false;
  bool unlockedUltimateOne = false;
  int meleeDamage = 10;
  uint32_t activeUltimateCastId = 0;
  uint32_t nextUltimateCastId = 1;

  PlayerData()
    : damageTimer(0.5f),
      manaRecoveryTimer(0.2f),
      healthRecoveryTimer(0.2f),
      ultimateRecoveryTimer(1.0f),
      weaponTimer(0.1f),
      jumpWindupTimer(0.00f) { //unlockedUltimateOne(ultOneUnlocked)
    state = PlayerState::idle;
    healthPoints = maxHealthPoints = 100;
    manaPoints = maxManaPoints = 100;
    ultimatePoints = 0;
    maxUltimatePoints = 100;
  };
};

struct LevelData {
  SDL_FRect     src{};
  SDL_FRect    dst{};
  bool isHazard{false};
};

struct PortalData {
  LevelIndex nextLevel;
  PortalData(LevelIndex idx): nextLevel(idx) {};
};

struct EnemyData {
  EnemyState state;
  Timer damageTimer;
  Timer attackTimer;
  Timer idleTimer;
  int healthPoints;
  int srcH, srcW;
  uint32_t lastUltimatePlayerId;
  uint32_t lastUltimateCastId;
  float hitStopRemainingSeconds;
  float pendingKnockbackDirection;
  float pendingKnockbackMagnitude;
  bool hasPendingKnockback;


  EnemyData(): state(EnemyState::idle), damageTimer(0.4f), attackTimer(1.0), idleTimer(1.0) {
    healthPoints = 100;
    srcH = 0;
    srcW = 0;
    lastUltimatePlayerId = 0;
    lastUltimateCastId = 0;
    hitStopRemainingSeconds = 0.0f;
    pendingKnockbackDirection = 0.0f;
    pendingKnockbackMagnitude = 0.0f;
    hasPendingKnockback = false;
  };
};

struct BulletData{
  BulletState state;
  Timer liveTimer;
  uint32_t ownerPlayerId;
  BulletData(): state(BulletState::moving), liveTimer(0.7f), ownerPlayerId(0) {};
};

union ObjectData {
  PlayerData player;
  LevelData level;
  EnemyData enemy;
  PortalData portal;
  BulletData bullet;

  ObjectData() { new (&level) LevelData{}; }   // pick one as default
  ~ObjectData() {}  // and destroy the active member appropriately if you change it
};

enum class ObjectClass : std::uint32_t
{
  Player, Level, Portal, Background, Enemy, Projectile
};

// define all objects in the game
struct GameObject {
  uint32_t id = 0;
  ObjectClass objClass;
  SpriteType spriteType;
  ObjectData data; // by making this a union, the different object types can have different fields in their structs
  glm::vec2 position, velocity, acceleration; // we have x and y positions/velocities/accelerations
  float direction;
  float maxSpeedX;
  std::vector<Animation> animations;
  int currentAnimation;
  PresentationVariant presentationVariant;
  SDL_Texture *texture;
  bool dynamic;
  bool grounded;
  glm::vec2 renderPosition;
  bool renderPositionInitialized;

  float bgscroll;
  float scrollFactor;
  float drawScale = 1.0f;
  float spritePixelW;
  float spritePixelH;
  SDL_FRect baseCollider;
  SDL_FRect collider;
  SDL_FRect colliderNorm{0.25f, 0.25f, 0.5f, 0.5f}; // x,y,w,h as fractions of scaled sprite

  Timer flashTimer;
  bool shouldFlash;
  int spriteFrame;

  GameObject(float spriteH, float spriteW): data(), spritePixelW(spriteW), spritePixelH(spriteH), collider{0}, flashTimer(0.05f) {
    objClass = ObjectClass::Level;
    maxSpeedX = 0;
    direction = 1;
    position = velocity = acceleration = glm::vec2(0);
    renderPosition = glm::vec2(0);
    renderPositionInitialized = false;
    currentAnimation = -1;
    presentationVariant = PresentationVariant::Idle;
    texture = nullptr;
    dynamic = false;
    grounded = false;
    shouldFlash = false;
    spriteFrame = 1;
  }

  void applyScale() {
    float drawW = spritePixelW / drawScale;
    float drawH = spritePixelH / drawScale;
    collider = {
      colliderNorm.x * drawW,
      colliderNorm.y * drawH,
      colliderNorm.w * drawW,
      colliderNorm.h * drawH,
    };
    baseCollider = collider;

    // std::cout << "x: " << collider.x << " y: " << collider.y << " w: " << collider.w << " h: " << collider.h << std::endl;
  };
};
