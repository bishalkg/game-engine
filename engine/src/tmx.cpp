#include "tmx.h"

#include <filesystem>
#include <sstream>
#include <iostream>

std::unique_ptr<tmx::Map> tmx::loadMap(const std::string &mapFilePath) {
  using namespace tinyxml2;
  std::filesystem::path mapPath(mapFilePath);

  tmx::Map *map = nullptr;

  XMLDocument doc;
  const XMLError mapLoadResult = doc.LoadFile(mapPath.string().c_str());
  if (mapLoadResult != XML_SUCCESS) {
    std::cerr << "tmx::loadMap failed to load '" << mapPath.string()
              << "': " << doc.ErrorStr() << "\n";
    return nullptr;
  }
  XMLElement *mapDoc = doc.FirstChildElement("map");
  if (!mapDoc) {
    std::cerr << "tmx::loadMap missing <map> root in '" << mapPath.string() << "'\n";
    return nullptr;
  }

  using clock = std::chrono::steady_clock;
  if (mapDoc) {
    map = new tmx::Map;
    map->mapWidth = mapDoc->IntAttribute("width");
    map->mapHeight = mapDoc->IntAttribute("height");
    map->tileWidth = mapDoc->IntAttribute("tilewidth");
    map->tileHeight = mapDoc->IntAttribute("tileheight");

    auto t0 = clock::now();
    for (XMLElement *child = mapDoc->FirstChildElement(); child != nullptr; child = child->NextSiblingElement())
    {
      if (std::strcmp(child->Name(), "tileset") == 0) {
        uint32_t firstgid = child->IntAttribute("firstgid");

        if (child->FirstChildElement("image") && child) {
          // if child->has child element "image" -> extract the sourcePath, width and height
            int tileWidth = child->IntAttribute("tilewidth");
            int tileHeight = child->IntAttribute("tileheight");
            int count = child->IntAttribute("count");
            int columns = child->IntAttribute("columns");
            tmx::TileSet newTileSet(count, tileWidth, tileHeight, columns, firstgid);

            // process tileset elements
            XMLElement *image = child->FirstChildElement("image");
            if (image) {
              newTileSet.image.source = image->Attribute("source");
              newTileSet.image.width = image->IntAttribute("width");
              newTileSet.image.height = image->IntAttribute("height");
            }

            // process tile (id) -> objectgroup (id) -> object (x,y,w,h) to get the collider FRect
            for (auto *tileElem = child->FirstChildElement("tile"); tileElem; tileElem = tileElem->NextSiblingElement("tile")) {
              uint32_t localId = tileElem->IntAttribute("id");
              auto &meta = newTileSet.tiles[localId]; // insert if dne

              if (auto *og = tileElem->FirstChildElement("objectgroup")) {
                if (auto *obj = og->FirstChildElement("object")) {
                  meta.collider.emplace(SDL_FRect{
                    static_cast<float>(obj->FloatAttribute("x")),
                    static_cast<float>(obj->FloatAttribute("y")),
                    static_cast<float>(obj->FloatAttribute("width")),
                    static_cast<float>(obj->FloatAttribute("height"))
                  });
                }
              }
            }
            // TODO here we can also process the frame files here!
            // move all elements to map; avoid copy
            map->tileSets.push_back(std::move(newTileSet));
        } else {

          // load tileset XML
          XMLDocument tilesetDoc;
          auto sourcePath = mapPath.parent_path().append(child->Attribute("source")); // generate full filepath
          tilesetDoc.LoadFile(sourcePath.string().c_str());

          XMLElement *ts = tilesetDoc.FirstChildElement("tileset");
          if (ts) {
            int tileWidth = ts->IntAttribute("tilewidth");
            int tileHeight = ts->IntAttribute("tileheight");
            int count = ts->IntAttribute("count");
            int columns = ts->IntAttribute("columns");
            tmx::TileSet newTileSet(count, tileWidth, tileHeight, columns, firstgid);

            XMLElement *image = ts->FirstChildElement("image");
            if (image) {
              newTileSet.image.source = image->Attribute("source");
              newTileSet.image.width = image->IntAttribute("width");
              newTileSet.image.height = image->IntAttribute("height");
            }
            // TODO here we can also process the frame files here!

            // move all elements to map; avoid copy
            map->tileSets.push_back(std::move(newTileSet));
          }
        }


      } else if (std::strcmp(child->Name(), "layer") == 0) {

        auto layer = parseLayer(child, map->mapWidth, map->mapHeight);
        map->layers.push_back(std::move(layer));

      } else if (std::strcmp(child->Name(), "group") == 0) {

        // Background comes in multiple layers.
        // 1. Parse the imagelayer -> image source

        //          <group id="4" name="Background">
        //   <imagelayer id="13" name="Sky" parallaxx="1.1" repeatx="1">
        //    <image source="../../Skyx32.png" width="960" height="544"/>
        //   </imagelayer>
        //   <imagelayer id="14" name="Flora1" parallaxx="1.2" repeatx="1">
        //    <image source="../../Flora1x32.png" width="960" height="544"/>
        //   </imagelayer>
        //   <imagelayer id="15" name="Flora2" parallaxx="1.3" repeatx="1">
        //    <image source="../../Flora2x32.png" width="960" height="544"/>
        //   </imagelayer>
        //   <layer id="5" name="Clouds2" width="100" height="17" parallaxx="1.1">
        //    <data encoding="csv">
        // 0,0,0,0,0,0,0,0,0,0,0,0,0,0,

        for (auto *elem = child->FirstChildElement(); elem; elem = elem->NextSiblingElement()) {

          if (strcmp(elem->Name(), "imagelayer") == 0) {
            tmx::Layer layer;
            layer.name = elem->Attribute("name");
            layer.id   = elem->IntAttribute("id");

            if (auto *img = elem->FirstChildElement("image")) {
              layer.img = Image{
                .source = img->Attribute("source"),
                .width  = img->IntAttribute("width"),
                .height = img->IntAttribute("height"),
              };
            };
            // optional: store parallax/repeat
            layer.parallaxX = elem->FloatAttribute("parallaxx", 1.0f);
            layer.parallaxY = elem->FloatAttribute("parallaxx", 1.0f);

            map->layers.push_back(std::move(layer));
          } else if (strcmp(elem->Name(), "layer") == 0) {
            auto layer = parseLayer(elem, map->mapWidth, map->mapHeight);
            map->layers.push_back(std::move(layer));
          }
        }


      }  else if (std::strcmp(child->Name(), "objectgroup") == 0) {

        tmx::ObjectGroup layer;
        layer.name = child->Attribute("name");
        layer.id = child->IntAttribute("id");
        for (XMLElement *elem = child->FirstChildElement("object"); elem != nullptr; elem = elem->NextSiblingElement("object"))
        {
          tmx::LayerObject obj;
          obj.id = elem->IntAttribute("id");
          obj.x = elem->FloatAttribute("x");
          obj.y = elem->FloatAttribute("y");


          const char *attrName = elem->Attribute("name");
          if (attrName) {
            obj.name = attrName;
          }

          const char *className = elem->Attribute("type");
          if (className) {
            obj.type = className;
          }
          layer.objects.push_back(obj);
        }

        map->layers.push_back(std::move(layer));

      }
    }
    auto t5 = clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t0).count();
    std::cout << "Total time to parse Elapsed: " << ms << " ms\n";

  }

  return std::unique_ptr<tmx::Map>(map);
}


tmx::Layer tmx::parseLayer(const tinyxml2::XMLElement* child, int mapW, int mapH) {
    tmx::Layer layer;
    layer.name = child->Attribute("name");
    layer.id   = child->IntAttribute("id");
    layer.data.reserve(mapW * mapH);

    const tinyxml2::XMLElement* data = child->FirstChildElement("data");
    if (data && data->GetText()) {
        std::stringstream dataStream(data->GetText());
        for (uint32_t i; dataStream >> i; ) {
            layer.data.push_back(i);
            if (dataStream.peek() == ',') dataStream.ignore();
        }
    }

    // DEBUG: Print layer info
    std::cout << "Parsed layer: " << layer.name
              << " expected=" << (mapW * mapH)
              << " actual=" << layer.data.size()
              << " non-zero=";
    int nonZeroCount = 0;
    for (auto gid : layer.data) if (gid != 0) nonZeroCount++;
    std::cout << nonZeroCount << std::endl;

    return layer;
}

// tmx::Layer tmx::parseLayer(const tinyxml2::XMLElement* child, int mapW, int mapH) {
//     tmx::Layer layer;
//     layer.name = child->Attribute("name");
//     layer.id   = child->IntAttribute("id");
//     layer.data.reserve(mapW * mapH);

//     const tinyxml2::XMLElement* data = child->FirstChildElement("data");
//     if (data && data->GetText()) {
//         std::stringstream dataStream(data->GetText());
//         for (int i; dataStream >> i; ) {
//             layer.data.push_back(i);
//             if (dataStream.peek() == ',') dataStream.ignore();
//         }
//     }
//     return layer; // NRVO/move
// }
