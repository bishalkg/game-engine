#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "animation.h"
#include <SDL3/SDL.h>

enum class PlayerState {
  idle, running, jumping
};

struct PlayerData {
  PlayerState state;
  PlayerData(){ state = PlayerState::idle; };
};

struct LevelData {

};

struct EnemyData {

};

union ObjectData {
  PlayerData player;
  LevelData level;
  EnemyData enemy;
};

enum class ObjectType
{
  player, level, enemy
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
  const float spritePixelSize;
  SDL_FRect collider;

  GameObject(int spriteSize) : data{.level = LevelData()}, spritePixelSize(spriteSize), collider{0} {
    type = ObjectType::level;
    maxSpeedX = 0;
    direction = 1;
    position = velocity = acceleration = glm::vec2(0);
    currentAnimation = -1;
    texture = nullptr;
    dynamic = false;
    grounded = false;
  }
};