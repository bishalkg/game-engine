#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "animation.h"
#include <SDL3/SDL.h>

enum class PlayerState: std::uint32_t {
  idle, running, jumping, swingWeapon
};

enum class BulletState: std::uint32_t {
  moving, colliding, inactive
};

enum class EnemyState: std::uint32_t {
  idle, dying, dead
};

enum class SpriteType: std::uint32_t {
  Player_Knight, Player_Mage, Minotaur_1, Skeleton_Warrior
};

static std::unordered_map<std::string, SpriteType> characterNameToSpriteType = {
  {"Player_Knight", SpriteType::Player_Knight},
  {"Player_Mage", SpriteType::Player_Mage},
  {"Player_Mage", SpriteType::Player_Mage},
  {"Minotaur_1", SpriteType::Minotaur_1},
  {"Skeleton_Warrior", SpriteType::Skeleton_Warrior},
};

struct PlayerData {
  PlayerState state;
  Timer damageTimer;
  int healthPoints;
  Timer weaponTimer;

  PlayerData(): weaponTimer(0.1f), damageTimer(1.0f) { state = PlayerState::idle; healthPoints = 100; };
};

struct LevelData {
  SDL_FRect     src{};
  SDL_FRect    dst{};
};

struct EnemyData {
  EnemyState state;
  Timer damageTimer;
  int healthPoints;
  int srcH, srcW;


  EnemyData(): state(EnemyState::idle), damageTimer(1.0f) {
    healthPoints = 100;
  };
};

struct BulletData{
  BulletState state;
  Timer liveTimer;
  BulletData(): state(BulletState::moving), liveTimer(0.5f) {};
};



union ObjectData {
  PlayerData player;
  LevelData level;
  EnemyData enemy;
  BulletData bullet;

  ObjectData() { new (&level) LevelData{}; }   // pick one as default
  ~ObjectData() {}  // and destroy the active member appropriately if you change it
};

enum class ObjectClass : std::uint32_t
{
  Player, Level, Background, Enemy, Bullet, Sword
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
  SDL_Texture *texture;
  bool dynamic;
  bool grounded;

  float bgscroll;
  float scrollFactor;
  float drawScale = 1.0f;
  float spritePixelW;
  float spritePixelH;
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
    currentAnimation = -1;
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

    // std::cout << "x: " << collider.x << " y: " << collider.y << " w: " << collider.w << " h: " << collider.h << std::endl;
  };
};
