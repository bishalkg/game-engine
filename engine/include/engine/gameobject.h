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

enum class MaterialType: std::uint32_t {
  none, coin, gem, healthPotion, manaPotion, attackUp, defenceUp
};

enum class MaterialState: std::uint32_t {
  present, collapsing, collected
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
  Present,
  Collapsing,
  // Collected
};

enum class HitStopStrength : uint8_t {
  Normal,
  Heavy,
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
  Timer damageTimer; // how long enemy is in hurt state
  Timer attackTimer; // how long enemy is in attack state
  Timer idleTimer; // ??
  int healthPoints;
  int maxHealthPoints;
  int srcH, srcW;
  uint32_t lastUltimatePlayerId;
  uint32_t lastUltimateCastId;
  float hitStopRemainingSeconds;
  float pendingKnockbackDirection;
  float pendingKnockbackMagnitude;
  bool hasPendingKnockback;
  bool isBoss = false;
  bool shouldDisplayHP = false; // when he boss is encountered display its HP
  float accelX = 30.0f;
  float distanceTrigger = 100.0f;


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

  EnemyData(bool isBoss, float damageResetTime, float attackResetTime, float idleResetTime, float accelX, float distanceTrigger, int healthPoints): isBoss(isBoss), healthPoints(healthPoints), accelX(accelX), distanceTrigger(distanceTrigger), state(EnemyState::idle), damageTimer(damageResetTime), attackTimer(attackResetTime), idleTimer(idleResetTime) {
    maxHealthPoints = healthPoints;
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

struct MaterialData{
  uint32_t count; // how many of this item player has
  MaterialType type;
  MaterialState state = MaterialState::present;
  MaterialData(uint32_t count, MaterialType type): count(count), type(type){};
};

// either an array of all materials
// or just one of each type
struct Inventory{
  // std::vector<MaterialData> consumables;
  MaterialData healthPotions;
  MaterialData manaPotions;
  MaterialData attackUps;
  MaterialData defenceUps;
  MaterialData coins;
  MaterialData gems;
  Inventory()
    : healthPotions(0, MaterialType::healthPotion),
      manaPotions(0, MaterialType::manaPotion),
      attackUps(0, MaterialType::attackUp),
      defenceUps(0, MaterialType::defenceUp),
      coins(0, MaterialType::coin),
      gems(0, MaterialType::gem) {}
  Inventory(
    uint32_t hpots,
    uint32_t manaPots,
    uint32_t attackUpAmt,
    uint32_t defenceUpAmt,
    uint32_t coinAmt,
    uint32_t gemAmt)
    : healthPotions(hpots, MaterialType::healthPotion),
      manaPotions(manaPots, MaterialType::manaPotion),
      attackUps(attackUpAmt, MaterialType::attackUp),
      defenceUps(defenceUpAmt, MaterialType::defenceUp),
      coins(coinAmt, MaterialType::coin),
      gems(gemAmt, MaterialType::gem) {}
};

struct PlayerData {
  PlayerState state;
  Inventory inventory;
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
  uint32_t coinPickupCueCount = 0;
  uint32_t gemPickupCueCount = 0;
  uint32_t coinPurchaseCueCount = 0;
  uint32_t gemPurchaseCueCount = 0;
  uint32_t consumableUseCueCount = 0;
  uint32_t activeUltimateCastId = 0;
  uint32_t nextUltimateCastId = 1;

  PlayerData()
    : damageTimer(0.5f),
      manaRecoveryTimer(1.0f),
      healthRecoveryTimer(1.0f),
      ultimateRecoveryTimer(1.0f),
      weaponTimer(0.1f),
      jumpWindupTimer(0.00f) { //unlockedUltimateOne(ultOneUnlocked)
    state = PlayerState::idle;
    healthPoints = maxHealthPoints = 100;
    manaPoints = maxManaPoints = 100;
    ultimatePoints = 0;
    maxUltimatePoints = 100;
    inventory = Inventory();
  };
};

union ObjectData {
  PlayerData player;
  LevelData level;
  EnemyData enemy;
  PortalData portal;
  BulletData bullet;
  MaterialData material;

  ObjectData() { new (&level) LevelData{}; }   // pick one as default
  ~ObjectData() {}  // and destroy the active member appropriately if you change it
};

enum class ObjectClass : std::uint32_t
{
  Player, Level, Portal, Background, Enemy, Projectile, Material
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
