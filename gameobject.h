#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "animation.h"
#include <SDL3/SDL.h>

enum class PlayerState {
  idle, running, jumping
};

enum class BulletState {
  moving, colliding, inactive
};

enum class EnemyState {
  idle, dying, dead
};

struct PlayerData {
  PlayerState state;
  Timer weaponTimer;

  PlayerData(): weaponTimer(0.1f) { state = PlayerState::idle; };
};

struct LevelData {

};

struct EnemyData {
  EnemyState state;
  Timer damageTimer;
  int healthPoints;

  EnemyData(): state(EnemyState::idle), damageTimer(1.0f) {
    healthPoints = 100;
  };
};

struct BulletData{
  BulletState state;
  BulletData(): state(BulletState::moving) {};
};



union ObjectData {
  PlayerData player;
  LevelData level;
  EnemyData enemy;
  BulletData bullet;
};

enum class ObjectType
{
  player, level, enemy, bullet, sword
};

// define all objects in the game
struct GameObject {
  ObjectType type;
  ObjectData data; // by making this a union, the different object types can have different fields in their structs
  glm::vec2 position, velocity, acceleration; // we have x and y positions/velocities/accelerations
  float direction;
  float maxSpeedX;
  std::vector<Animation> animations;
  int currentAnimation;
  SDL_Texture *texture;
  bool dynamic;
  bool grounded;
  float spritePixelW;
  float spritePixelH;
  SDL_FRect collider;
  Timer flashTimer;
  bool shouldFlash;
  int spriteFrame;

  GameObject(float spriteH, float spriteW): data{.level = LevelData()}, spritePixelW(spriteW), spritePixelH(spriteH), collider{0}, flashTimer(0.05f) {
    type = ObjectType::level;
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
};