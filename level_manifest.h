#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>
#include <vector>


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
const int ANIM_RUN_ATTACK = 10;

enum class SpriteType: std::uint32_t {
  Player_Knight, Player_Mage, Minotaur_1, Skeleton_Warrior, Red_Werewolf, Player_Marie, Skeleton_Pikeman
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
  std::string runAttackTex;
  std::string slideShootTex;
  std::string jumpTex;

};

struct SpriteAssets {
  SpriteAssetPaths paths;
  std::unordered_map<int, std::pair<int, float>> animSettings; // ressources::ANIM_IDLE -> {framecount, length}
};

struct CutsceneAsset {
    std::string texPath;
    std::pair<int, float> animSetting;
    std::vector<std::string> dialogue; // each animation can have multiple bubbles of dialogue
    int numFrameColumns;
    float frameW;
    float frameH;
    float yOffset = 0;
    float xOffset = 0;
    float scale = 1.0;
    bool loopScene = false;
};

struct LevelAssets {
  std::string mapPath;
  std::string background4PathName; //Skyx32
  std::string background3PathName; //Clouds_x32
  std::string background2PathName; //Flora1x32
  std::string background1PathName; //Flora1x32
  std::string backgroundAudioPath; // data/audio/Level_3_Final_Floor.wav
  std::string gameOverAudioPath;
  std::string stepAudioPath;
  std::vector<SpriteType> enemyTypes;
  std::vector<CutsceneAsset> cutsceneData;
};