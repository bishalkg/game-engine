#pragma once
#include "level_manifest.h"


static std::unordered_map<std::string, SpriteType> CHARACTER_NAME_TO_SPRITE_TYPE = {
  {"Player_Knight", SpriteType::Player_Knight},
  {"Player_Mage", SpriteType::Player_Mage},
  {"Player_Marie", SpriteType::Player_Marie},
  {"Minotaur_1", SpriteType::Minotaur_1},
  {"Skeleton_Warrior", SpriteType::Skeleton_Warrior},
  {"Skeleton_Pikeman", SpriteType::Skeleton_Pikeman},
  {"Red_Werewolf", SpriteType::Red_Werewolf},
};


static std::unordered_map<SpriteType, SpriteAssets> PLAYER_CONFIG = {
  {
    SpriteType::Player_Mage,
    SpriteAssets{
      .paths = SpriteAssetPaths{
        .idleTex = "data/players/Mage/Idle.png",
        .walkTex = "data/players/Mage/Walk.png",
        .runTex = "data/players/Mage/Run.png",
        .attackTex = "data/players/Mage/Attack_2.png",
        .hitTex = "data/players/Mage/Hurt.png",
        .dieTex = "data/players/Mage/Dead.png",
        .shootTex = "data/players/Mage/Attack_1.png",
        .slideTex = "data/players/Mage/Hurt.png",
        .runShootTex = "data/players/Mage/Attack_1.png",
        .runAttackTex = "data/players/Mage/Attack_2.png",
        .slideShootTex = "data/players/Mage/Attack_1.png",
        .jumpTex = "data/players/Mage/Jump.png",
      },
      .animSettings = {
        { ANIM_IDLE,{ 8, 1.6f } },
        { ANIM_RUN, { 8, 0.6f } },
        { ANIM_SLIDE, { 4, 0.5f } },
        { ANIM_SHOOT, { 7, 0.6f } },
        { ANIM_HIT, { 4, 0.6f } },
        { ANIM_DIE, { 4, 0.6f } },
        { ANIM_SLIDE_SHOOT, { 7, 0.5f } },
        { ANIM_JUMP , { 8, 0.5f } },
        { ANIM_SWING , { 4, 1.0f } },
        { ANIM_RUN_ATTACK , { 6, 0.5f } },
      },
    },
  },
  {
    SpriteType::Player_Knight,
    SpriteAssets{
      .paths = SpriteAssetPaths{
        .idleTex = "data/players/Knight_3/Idle.png",
        .walkTex = "data/players/Knight_3/Run.png",
        .runTex = "data/players/Knight_3/Run.png",
        .attackTex = "data/players/Knight_3/Attack 1.png",
        .hitTex = "data/players/Knight_3/Hurt.png",
        .dieTex = "data/players/Knight_3/Dead.png",
        .shootTex = "data/players/Knight_3/Attack 3.png",
        .slideTex = "data/players/Knight_3/Run.png",
        .runShootTex = "data/players/Knight_3/Run+Attack.png",
        .runAttackTex = "data/players/Knight_3/Run+Attack.png",
        .slideShootTex = "data/players/Knight_3/Run+Attack.png",
        .jumpTex = "data/players/Knight_3/Jump.png",
      },
      .animSettings = {
        { ANIM_IDLE,{ 4, 0.5f } },
        { ANIM_RUN, { 7, 0.5f } },
        { ANIM_SLIDE, { 7, 1.0f } },
        { ANIM_SHOOT, { 5, 0.5f } },
        { ANIM_HIT, { 2, 0.6f } },
        { ANIM_DIE, { 6, 0.6f } },
        { ANIM_SLIDE_SHOOT, { 5, 0.5f } },
        { ANIM_JUMP , { 6, 0.5f } },
        { ANIM_SWING , { 5, 0.5f } },
        { ANIM_RUN_ATTACK , { 6, 0.5f } },
      },
    },
  },
  {
    SpriteType::Player_Marie,
    SpriteAssets{
      .paths = SpriteAssetPaths{
        .idleTex = "data/players/Marie/Idle.png",
        .walkTex = "data/players/Mage/Walk.png",
        .runTex = "data/players/Marie/Run.png",
        .attackTex = "data/players/Marie/Attack_1.png",
        .hitTex = "data/players/Mage/Hurt.png",
        .dieTex = "data/players/Mage/Dead.png",
        .shootTex = "data/players/Marie/Attack_1.png",
        .slideTex = "data/players/Marie/Run.png",
        .runShootTex = "data/players/Marie/Attack_1.png",
        .runAttackTex = "data/players/Marie/Attack_1.png", // TODO
        .slideShootTex = "data/players/Marie/Attack_1.png",
        .jumpTex = "data/players/Marie/Jump_2.png",
      },
      .animSettings = {
        { ANIM_IDLE,{ 6, 0.8f } },
        { ANIM_RUN, { 8, 0.6f } },
        { ANIM_SLIDE, { 8, 0.5f } },
        { ANIM_SHOOT, { 7, 0.6f } },
        { ANIM_HIT, { 4, 0.6f } },
        { ANIM_DIE, { 4, 0.6f } },
        { ANIM_SLIDE_SHOOT, { 7, 0.5f } },
        { ANIM_JUMP , { 6, 0.6f } },
        { ANIM_SWING , { 6, 0.4f } },
        { ANIM_RUN_ATTACK , { 6, 0.4f } },
      },
    },
  },
};

// enemyTypes map should be global and the level assets just has a vector of SpriteType::Minotaur_1
static std::unordered_map<SpriteType, SpriteAssets> ENEMY_CONFIG = {
  {
    SpriteType::Minotaur_1,
    SpriteAssets{
      .paths = SpriteAssetPaths{
        .idleTex = "data/enemies/Minotaur_1/Idle.png",
        .walkTex = "data/enemies/Minotaur_1/Walk.png",
        .runTex = "data/enemies/Minotaur_1/Walk.png",
        .attackTex = "data/enemies/Minotaur_1/Attack.png",
        .hitTex = "data/enemies/Minotaur_1/Hurt.png",
        .dieTex = "data/enemies/Minotaur_1/Dead.png",
      },
      .animSettings = {
        { ANIM_IDLE,{ 10, 1.0f } },
        { ANIM_RUN, { 12, 1.0f } },
        { ANIM_HIT, { 3, 0.5f } },
        { ANIM_DIE , { 5, 0.5f } },
        { ANIM_SWING , { 4, 0.5f } },
      },
    },
  },
  {
    SpriteType::Skeleton_Warrior,
    SpriteAssets{
      .paths = SpriteAssetPaths{
        .idleTex = "data/enemies/Skeleton_Warrior/Idle.png",
        .walkTex = "data/enemies/Skeleton_Warrior/Walk.png",
        .runTex = "data/enemies/Skeleton_Warrior/Run.png",
        .attackTex = "data/enemies/Skeleton_Warrior/Attack_1.png",
        .hitTex = "data/enemies/Skeleton_Warrior/Hurt.png",
        .dieTex = "data/enemies/Skeleton_Warrior/Dead.png",
      },
      .animSettings = {
        { ANIM_IDLE,{ 7, 1.0f } },
        { ANIM_RUN, { 8, 1.0f } },
        { ANIM_HIT, { 2, 0.5f } },
        { ANIM_DIE , { 4, 0.5f } },
        { ANIM_SWING , { 5, 0.5f } },
      },
    },
  },
  {
    SpriteType::Red_Werewolf,
    SpriteAssets{
      .paths = SpriteAssetPaths{
        .idleTex = "data/enemies/Red_Werewolf/Idle.png",
        .walkTex = "data/enemies/Red_Werewolf/Walk.png",  // 11 frames
        .runTex = "data/enemies/Red_Werewolf/Walk.png",
        .attackTex = "data/enemies/Red_Werewolf/Attack_1.png",
        .hitTex = "data/enemies/Red_Werewolf/Hurt.png",
        .dieTex = "data/enemies/Red_Werewolf/Dead.png",
      },
      .animSettings = {
        { ANIM_IDLE,{ 8, 1.0f } },
        { ANIM_RUN, { 11, 1.0f } },
        { ANIM_HIT, { 2, 0.5f } },
        { ANIM_DIE , { 2, 0.5f } },
        { ANIM_SWING , { 5, 1.0f } },
      },
    },
  },
  {
    SpriteType::Skeleton_Pikeman,
    SpriteAssets{
      .paths = SpriteAssetPaths{
        .idleTex = "data/enemies/Skeleton_Spearman/Idle.png",
        .walkTex = "data/enemies/Skeleton_Spearman/Walk.png",  // 11 frames
        .runTex = "data/enemies/Skeleton_Spearman/Run.png",
        .attackTex = "data/enemies/Skeleton_Spearman/Attack_1.png",
        .hitTex = "data/enemies/Skeleton_Spearman/Hurt.png",
        .dieTex = "data/enemies/Skeleton_Spearman/Dead.png",
      },
      .animSettings = {
        { ANIM_IDLE,{ 7, 1.0f } },
        { ANIM_RUN, { 6, 1.0f } },
        { ANIM_HIT, { 3, 0.5f } },
        { ANIM_DIE , { 5, 0.5f } },
        { ANIM_SWING , { 4, 0.5f } },
      },
    },
  },
};


inline constexpr const char* DEFAULT_GAME_OVER_SOUND = "data/audio/game_over.mp3";

// Define config for each level
inline std::unordered_map<LevelIndex, LevelAssets> LEVEL_CONFIG = {
  {
    LevelIndex::LEVEL_1,
    LevelAssets{
      .mapPath = "data/maps/level_1/level_1.tmx",
      .background4PathName = "Skyx32",
      .background3PathName = "Clouds_x32",
      .background2PathName = "Flora2x32",
      .background1PathName = "Flora1x32",
      // .backgroundAudioPath = "data/audio/Level_1_Forest_Outside_Castle.wav",
      // .backgroundAudioPath = "data/audio/1. Dawn of Blades.wav",
      .backgroundAudioPath = "data/audio/1. Dawn of Blades.wav",
      .gameOverAudioPath = DEFAULT_GAME_OVER_SOUND,
      .enemyTypes = { SpriteType::Minotaur_1,  SpriteType::Skeleton_Warrior},
    },
  },
  {
    LevelIndex::LEVEL_2,
    LevelAssets{
      .mapPath = "data/maps/level_2/level_2.tmx",
      .background4PathName = "background1",
      .background3PathName = "background3",
      .background2PathName = "background4a",
      .background1PathName = "background4b",
      .backgroundAudioPath = "data/audio/11. Whispers in the Fog.wav",
      .gameOverAudioPath = DEFAULT_GAME_OVER_SOUND,
      .enemyTypes = { SpriteType::Red_Werewolf,  SpriteType::Skeleton_Warrior,SpriteType::Skeleton_Pikeman },
    },
  },
};


// need enemyType To Assets Map? or just function with large switch case that


// extern: declaration only. Tells the compiler “this variable/function exists elsewhere.” You put extern int x; in headers and define int x; once in a .cpp. Avoids duplicate symbols.
// static (at namespace scope): gives the entity internal linkage. Every translation unit that sees static int x = 1; gets its own private copy. Good for TU‑local helpers; not for shared globals.
// inline (C++17 for variables, earlier for functions): allows the same definition to appear in multiple translation units, but the linker merges them into one. Use inline int x = 1; in headers instead of extern/definition pairs.
// Practical rules for headers:

// For shared globals: either extern in the header + one definition in a .cpp, or make the definition inline in the header.
// Don’t put non‑inline, non‑static definitions in headers; you’ll get duplicate symbol errors.
// Use static only when you want a per‑translation‑unit instance.