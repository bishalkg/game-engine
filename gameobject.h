#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "animation.h"
#include <SDL3/SDL.h>

enum class ObjectType
{
  player, level, enemy
};

// define all objects in the game
struct GameObject {
  ObjectType type;
  glm::vec2 position, velocity, acceleration;
  float direction;
  std::vector<Animation> animations;
  int currentAnimation;
  SDL_Texture *texture;

  GameObject(){
    type = ObjectType::level;
    direction = 1;
    position = velocity = acceleration = glm::vec2(0);
    currentAnimation = -1;
    texture = nullptr;
  }
};