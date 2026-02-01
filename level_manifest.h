#pragma once
#include <cstdint>
#include <unordered_map>
#include <string>


enum class LevelIndex: std::uint32_t  {
  LEVEL_1,
  LEVEL_2,
  LEVEL_3,
};


struct LevelAssets {
  std::string mapPath;
  std::string background4PathName; //Skyx32
  std::string background3PathName; //Clouds_x32
  std::string background2PathName; //Flora1x32
  std::string background1PathName; //Flora1x32
  std::string backgroundAudioPath; // data/audio/Level_3_Final_Floor.wav
};

// extern std::unordered_map<LevelIndex, std::string> LevelAssetsMap;

inline std::unordered_map<LevelIndex, LevelAssets> LevelAssetsMap = {
  {LevelIndex::LEVEL_1, LevelAssets{
    .mapPath = "data/maps/level_1/level_1_v2.tmx",
    .background4PathName = "Skyx32",
    .background3PathName = "Clouds_x32",
    .background2PathName = "Flora2x32",
    .background1PathName = "Flora1x32",
    .backgroundAudioPath = "data/audio/Level_3_Final_Floor.wav",
  },
  },

};
