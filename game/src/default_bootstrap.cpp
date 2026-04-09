#include "game/default_systems.h"

#include <thread>
#include <variant>

#include "engine/engine.h"

namespace {

using game_engine::Engine;
using game_engine::GameState;
using game_engine::SDLState;
using game::GameResources;

bool initAllTiles(Engine& engine, GameResources& resources, GameState& newGameState) {
  SDLState& sdlState = engine.getSDLState();

  struct LayerVisitor {
    const SDLState& state;
    GameState& gs;
    GameResources& res;
    int countModColliders = 0;

    LayerVisitor(const SDLState& state, GameState& gs, GameResources& res)
      : state(state), gs(gs), res(res) {}

    const tmx::TileSet* pickTileset(uint32_t gid) {
      const tmx::TileSet* match = nullptr;
      for (const auto& ts : res.m_currLevel->map->tileSets) {
        if (gid >= static_cast<uint32_t>(ts.firstgid)) {
          match = &ts;
        } else {
          break;
        }
      }
      return match;
    }

    GameObject createObject(
      int r,
      int c,
      SDL_Texture* tex,
      ObjectClass type,
      float spriteH,
      float spriteW,
      int srcX,
      int srcY) {
      GameObject o(spriteH, spriteW);
      o.objClass = type;
      o.texture = tex;
      o.collider = {.x = 0, .y = 0, .w = spriteW, .h = spriteH};

      if (type == ObjectClass::Level || type == ObjectClass::Portal) {
        o.position = {c * spriteW, r * spriteH};
        o.data.level.src = {
          .x = static_cast<float>(srcX),
          .y = static_cast<float>(srcY),
          .w = static_cast<float>(spriteW),
          .h = static_cast<float>(spriteH),
        };
        o.data.level.dst = {
          .x = static_cast<float>(c) * spriteW,
          .y = static_cast<float>(r) * spriteH,
          .w = static_cast<float>(spriteW),
          .h = static_cast<float>(spriteH),
        };
      }
      return o;
    }

    void operator()(tmx::Layer& layer) {
      std::vector<GameObject> newLayer;

      if (!layer.img.has_value()) {
        for (int r = 0; r < res.m_currLevel->map->mapHeight; ++r) {
          for (int c = 0; c < res.m_currLevel->map->mapWidth; ++c) {
            const uint32_t rawGid = layer.data[r * res.m_currLevel->map->mapWidth + c];
            const uint32_t gid = rawGid & 0x1FFFFFFF;
            if (!gid) {
              continue;
            }

            const tmx::TileSet* ts = pickTileset(gid);
            if (!ts) {
              continue;
            }

            uint32_t localId = rawGid - ts->firstgid;
            int srcX = (localId % ts->columns) * ts->tileWidth;
            int srcY = (localId / ts->columns) * ts->tileHeight;
            bool isHazard = (layer.name == "Hazard");

            auto tile = createObject(
              r,
              c,
              ts->texture,
              ObjectClass::Level,
              ts->tileHeight,
              ts->tileWidth,
              srcX,
              srcY);

            if (layer.name != "Level") {
              tile.collider.w = 0;
              tile.collider.h = 0;
            }

            if (layer.name == "Level" || isHazard) {
              if (auto it = ts->tiles.find(localId); it != ts->tiles.end() && it->second.collider) {
                tile.collider = *(it->second.collider);
                tile.data.level.isHazard = isHazard;
                countModColliders += 1;
              }
            }

            newLayer.push_back(tile);
          }
        }
      } else if (layer.name == "Background_4") {
        auto bgImg = createObject(
          0,
          0,
          res.m_currLevel->map->tileSets[res.m_currLevel->bg4Idx].texture,
          ObjectClass::Background,
          0,
          0,
          0,
          0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.2f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_3") {
        auto bgImg = createObject(
          0,
          0,
          res.m_currLevel->map->tileSets[res.m_currLevel->bg3Idx].texture,
          ObjectClass::Background,
          0,
          0,
          0,
          0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.2f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_2") {
        auto bgImg = createObject(
          0,
          0,
          res.m_currLevel->map->tileSets[res.m_currLevel->bg2Idx].texture,
          ObjectClass::Background,
          0,
          0,
          0,
          0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.3f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_1") {
        auto bgImg = createObject(
          0,
          0,
          res.m_currLevel->map->tileSets[res.m_currLevel->bg1Idx].texture,
          ObjectClass::Background,
          0,
          0,
          0,
          0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.4f;
        newLayer.push_back(bgImg);
      }

      gs.layers.push_back(std::move(newLayer));
    }

    void operator()(tmx::ObjectGroup& objectGroup) {
      std::vector<GameObject> newLayer;
      for (tmx::LayerObject& obj : objectGroup.objects) {
        glm::vec2 objStartingPos(
          obj.x - res.m_currLevel->map->tileWidth / 2,
          obj.y - res.m_currLevel->map->tileHeight / 2);

        if (obj.type == "Portal") {
          GameObject portal = createObject(1, 1, nullptr, ObjectClass::Portal, 32, 32, 0, 0);

          LevelIndex lvl = LevelIndex::LEVEL_2;
          if (res.m_currLevel->lvlIdx == LevelIndex::LEVEL_1) {
            lvl = LevelIndex::LEVEL_2;
          }
          if (res.m_currLevel->lvlIdx == LevelIndex::LEVEL_2) {
            lvl = LevelIndex::LEVEL_3;
          }
          portal.data.portal = PortalData(lvl);
          portal.colliderNorm = {.x = 0.0f, .y = 0.5f, .w = 1.0f, .h = 1.0f};
          portal.applyScale();
          portal.position = objStartingPos;

          newLayer.push_back(std::move(portal));
        }

        if (obj.type == "Enemy") {
          SpriteType spriteType = CHARACTER_NAME_TO_SPRITE_TYPE.at(obj.name);
          GameObject enemy = createObject(
            1,
            1,
            res.m_currLevel->texCharacterMap.at(spriteType).texIdle,
            ObjectClass::Enemy,
            128,
            128,
            0,
            0);
          enemy.spriteType = spriteType;

          switch (spriteType) {
            case SpriteType::Minotaur_1:
              enemy.drawScale = 2.0f;
              break;
            case SpriteType::Skeleton_Warrior:
            case SpriteType::Red_Werewolf:
            case SpriteType::Skeleton_Pikeman:
              enemy.drawScale = 1.5f;
              break;
            default:
              break;
          }
          float wFrac = 0.30f;
          enemy.colliderNorm = {.x = 0.35f, .y = 0.4f, .w = wFrac, .h = 0.6f};
          enemy.applyScale();

          float feetY = objStartingPos.y;
          float centerX = objStartingPos.x;
          enemy.position.x = centerX - enemy.collider.w * 0.5f;
          enemy.position.y = feetY - (enemy.collider.y + enemy.collider.h);
          enemy.data.enemy = EnemyData();
          enemy.currentAnimation = res.ANIM_IDLE;
          enemy.presentationVariant = PresentationVariant::Idle;
          enemy.animations = res.m_currLevel->texCharacterMap.at(spriteType).anims;
          enemy.dynamic = true;
          enemy.maxSpeedX = 15;
          newLayer.push_back(std::move(enemy));
        }

        if (obj.type == "Player") {
          SpriteType spriteType = gs.selectedPlayerSprite;
          int texDim = 128;

          GameObject player = createObject(
            1,
            1,
            res.m_currLevel->texCharacterMap.at(spriteType).texIdle,
            ObjectClass::Player,
            texDim,
            texDim,
            0,
            0);
          player.spriteType = spriteType;
          player.drawScale = 1.5f;

          float wFrac = 0.30f;
          float hFrac = 0.40f;
          player.colliderNorm = {.x = 0.10f, .y = 0.9f - hFrac, .w = wFrac, .h = hFrac};
          switch (spriteType) {
            case SpriteType::Player_Knight:
              player.colliderNorm = {.x = 0.1f, .y = 0.5f, .w = wFrac, .h = 0.5f};
              break;
            case SpriteType::Player_Mage:
              player.colliderNorm = {.x = 0.30f, .y = 0.5f, .w = wFrac, .h = 0.5f};
              break;
            case SpriteType::Player_Marie:
            case SpriteType::Player_Bonkfather:
              player.colliderNorm = {.x = 0.30f, .y = 0.5f, .w = wFrac, .h = 0.5f};
              player.drawScale = 2.0f;
              break;
            default:
              break;
          }

          player.applyScale();

          float drawW = player.spritePixelW / player.drawScale;
          float drawH = player.spritePixelH / player.drawScale;
          float centerX = obj.x;
          float feetY = obj.y;

          player.position.x = centerX - drawW * 0.5f;
          player.position.y = feetY - drawH;

          player.data.player = PlayerData();
          player.animations = res.m_currLevel->texCharacterMap.at(spriteType).anims;
          player.currentAnimation = res.ANIM_IDLE;
          player.presentationVariant = PresentationVariant::Idle;
          player.acceleration = glm::vec2(500, 0);
          player.maxSpeedX = 100;
          player.dynamic = true;

          newLayer.push_back(player);
          gs.playerIndex = static_cast<int>(newLayer.size()) - 1;
          gs.playerLayer = static_cast<int>(gs.layers.size());
        }
      }
      gs.layers.push_back(std::move(newLayer));
    }
  };

  LayerVisitor visitor(sdlState, newGameState, resources);
  for (std::variant<tmx::Layer, tmx::ObjectGroup>& layer : resources.m_currLevel->map->layers) {
    std::visit(visitor, layer);
  }

  return newGameState.playerIndex != -1;
}

class DefaultBootstrap final : public game::IBootstrap {
public:
  bool initialize(Engine& engine, GameResources& resources, bool headless) override {
    auto& sdlState = engine.getSDLState();
    auto& gameState = engine.getGameState();

    resources.loadAllAssets(sdlState, gameState, headless);
    if (!resources.m_currLevel) {
      return false;
    }
    if (!headless &&
        !resources.m_currLevel->texCharacterMap[SpriteType::Player_Knight].texIdle) {
      return false;
    }

    return initAllTiles(engine, resources, gameState);
  }
};

} // namespace

namespace game {

bool switchToLevel(game_engine::Engine& engine, GameResources& resources, LevelIndex levelId) {
  auto& gameState = engine.getGameState();
  auto& sdlState = engine.getSDLState();

  gameState.currentView = UIManager::GameView::LevelLoading;
  gameState.setLevelLoadProgress(0);
  gameState.setLevelLoadProgress(10);

  if (!resources.loadLevel(levelId, sdlState, gameState, resources.m_masterAudioGain, false)) {
    return false;
  }

  GameState newGameState(sdlState);
  newGameState.selectedPlayerSprite = gameState.selectedPlayerSprite;
  newGameState.currentView = UIManager::GameView::LevelLoading;
  if (!initAllTiles(engine, resources, newGameState)) {
    return false;
  }

  gameState = std::move(newGameState);
  gameState.setLevelLoadProgress(100);
  return true;
}

std::unique_ptr<IBootstrap> createDefaultBootstrap() {
  return std::make_unique<DefaultBootstrap>();
}

} // namespace game
