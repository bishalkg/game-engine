#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>


enum class LevelIndex: std::uint32_t  {
  LEVEL_1,
  LEVEL_2,
  LEVEL_3,
};

const int ANIM_IDLE = 0;
const int ANIM_RUN = 1;
const int ANIM_SLIDE = 2;
const int ANIM_SHOOT = 3;
const int ANIM_SLIDE_SHOOT = 4;
const int ANIM_SWING = 5;
const int ANIM_JUMP = 6;
const int ANIM_WALK = 7;
const int ANIM_HIT = 8;
const int ANIM_DIE = 9;

enum class SpriteType: std::uint32_t {
  Player_Knight, Player_Mage, Minotaur_1, Skeleton_Warrior
};

struct SpriteAssetPaths {
  std::string idleTex; // need to store frame count and length for animation here too
  std::string walkTex;
  std::string runTex;
  std::string attackTex;
  std::string hitTex;
  std::string dieTex;

  // player specific
  std::string shootTex;
  std::string slideTex;
  std::string runShootTex;
  std::string slideShootTex;
  std::string jumpTex;

};

struct SpriteAssets {
  SpriteAssetPaths paths;
  std::unordered_map<int, std::pair<int, float>> animSettings; // ressources::ANIM_IDLE -> {framecount, length}
};

struct LevelAssets {
  std::string mapPath;
  std::string background4PathName; //Skyx32
  std::string background3PathName; //Clouds_x32
  std::string background2PathName; //Flora1x32
  std::string background1PathName; //Flora1x32
  std::string backgroundAudioPath; // data/audio/Level_3_Final_Floor.wav
  std::vector<SpriteType> enemyTypes;
};