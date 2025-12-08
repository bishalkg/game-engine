#pragma once

#include <vector>
#include <string>
#include <memory>
#include <variant>
#include <unordered_map>
#include "vendor/tinyxml2/tinyxml2.h"
#include <SDL3_image/SDL_image.h>

namespace tmx
{

  struct Layer
  {
    int id;
    std::string name;
    std::vector<uint32_t> data;  // GIDs, size = mapWidth * mapHeight
  };

  struct LayerObject
  {
    int id;
    std::string name, type;
    float x, y;
  };

  struct ObjectGroup
  {
    int id;
    std::string name;
    std::vector<LayerObject> objects;
  };

  struct Image
  {
    std::string source;// path from TSX
    int width, height;
  };

  // struct Tile
  // {
  //   int id;
  //   Image image;
  // };

  struct TileFrame { int tileId; int durationMs; };

  struct TileMeta {
    std::vector<TileFrame> animation; // empty if none
    // add properties here if needed
  };

  struct TileSet
  {
    int count, tileWidth, tileHeight, columns, firstgid;
    // std::vector<Tile> tiles;

    Image image;

    // runtime-only:
    SDL_Texture* texture = nullptr;
    std::unordered_map<int, TileMeta> tiles; // only entries for <tile> elements (animations, props)

    public:
      TileSet(int count, int tileWidth, int tileHeight, int columns, int firstgid)
        : count(count), tileWidth(tileWidth), tileHeight(tileHeight), columns(columns), firstgid(firstgid) {}

  };

  struct Map
  {
    int mapWidth, mapHeight;
    int tileWidth, tileHeight;
    std::vector<TileSet> tileSets;
    std::vector<std::variant<Layer, ObjectGroup>> layers; // variant indicates entry can be either Layer or ObjectGroup types, std::visit helps ref this
  };

  std::unique_ptr<Map> loadMap(const std::string &mapFilePath);
  tmx::Layer parseLayer(const tinyxml2::XMLElement* child, int mapW, int mapH);

};