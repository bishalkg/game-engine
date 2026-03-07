#include "engine/engine.h"

void game_engine::Engine::asyncSwitchToLevel(LevelIndex lvl) {
  m_gameState.currentView = UIManager::GameView::LevelLoading;
  m_gameState.setLevelLoadProgress(0);
  m_levelLoadThd = std::thread(&game_engine::Engine::initNextLevel, this, lvl);
}

void game_engine::Engine::initNextLevel(LevelIndex lvl) {

  m_gameState.setLevelLoadProgress(10);

  m_resources.loadLevel(lvl, m_sdlState, m_gameState, m_resources.m_masterAudioGain, false);

  // throw away old world; create new gameState that will be updated in initAllTiles
  GameState newGameState(m_sdlState);
  newGameState.currentView = UIManager::GameView::LevelLoading;

  // rebuild layers/objects from the newly loaded map
  initAllTiles(newGameState); // this mutates gameState
  m_gameState.setLevelLoadProgress(70);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  {
    std::lock_guard<std::mutex> lock(m_levelMutex);
    m_gameState = std::move(newGameState);
  }

  // m_gameState = std::move(newGameState); // now its safe to transfer ownership
  m_gameState.setLevelLoadProgress(100);
}

bool game_engine::Engine::initAllTiles(GameState &newGameState) {

  struct LayerVisitor
  {

    const SDLState &state;
    GameState &gs;
    Resources &res;
    int countModColliders = 0;

    LayerVisitor(const SDLState &state, GameState &gs, Resources &res): state(state), gs(gs), res(res){}

    const tmx::TileSet* pickTileset(uint32_t gid) {
      const tmx::TileSet* match = nullptr;
      for (const auto& ts : res.m_currLevel->map->tileSets) {
        if (gid >= (uint32_t)ts.firstgid) match = &ts;
        else break;
      }
      return match; // assumes sets are sorted by firstGid
    }

    const GameObject createObject(int r, int c, SDL_Texture *tex, ObjectClass type, float spriteH, float spriteW, int srcX, int srcY) {
      GameObject o(spriteH, spriteW);
      o.objClass = type;
      // o.position = glm::vec2(c * TILE_SIZE, state.logH - (20 - r) * TILE_SIZE);
      o.texture = tex;

      // default collider for level objects
      // TODO I need to define a specific collider for each type of level tile
      o.collider = {
        .x = 0,
        .y = 0, // update collider x.y position to  detemine how much overlap is allowed between objects
        .w = spriteW,
        .h = spriteH
      };

      if (type == ObjectClass::Level || type == ObjectClass::Portal) {
        o.position = { c * spriteW, r * spriteH };
        // o.position = glm::vec2(c * TILE_SIZE, state.logH - (20 - r) * TILE_SIZE);
        // pick out the exact tile from the tilesheet
        o.data.level.src = SDL_FRect{
          .x = static_cast<float>(srcX),
          .y = static_cast<float>(srcY),
          .w = static_cast<float>(spriteW),
          .h = static_cast<float>(spriteH)
        };
        o.data.level.dst = SDL_FRect{
          .x = static_cast<float>(c) * spriteW,
          .y = static_cast<float>(r) * spriteH,
          .w = static_cast<float>(spriteW),
          .h = static_cast<float>(spriteH)
        };

      }
      return o;
    };

    // for each std::varient thats in tileSet
    void operator()(tmx::Layer &layer)
    {
      std::vector<GameObject> newLayer;

      if (!layer.img.has_value()) {
        for (int r = 0; r < res.m_currLevel->map->mapHeight; ++r){

          for (int c = 0; c < res.m_currLevel->map->mapWidth; ++c) {
            const uint32_t rawGid = layer.data[r * res.m_currLevel->map->mapWidth + c]; // packed Tiled GID (includes flip flags)

            // Tiled encodes flips in the top 3 bits; mask them off so we lookup the real tile index.
            // Without this the computed srcY can overflow the texture height, causing tiles to vanish.
            const uint32_t gid = rawGid & 0x1FFFFFFF; // clear H/V/diag flip flags
            if (gid) {

              // find the texture corresponding to this gID
              // const auto itr = std::find_if(res.map->tileSets.begin(), res.map->tileSets.end(),
              // [tGid](const tmx::TileSet &ts) {
              //   return tGid >= ts.firstgid && tGid < ts.firstgid + ts.texture.size();
              // });
              const tmx::TileSet* ts = pickTileset(gid);

              if (!ts) continue;

              uint32_t localId = rawGid - ts->firstgid; // local index within the tileSet data array
              int srcX = (localId % ts->columns) * ts->tileWidth; // col * tileWidth;
              int srcY = (localId / ts->columns) * ts->tileHeight; // row * tileHeight

              bool isHazard = false;
              if (layer.name == "Hazard") {
                isHazard = true;
              }

              auto tile = createObject(r, c, ts->texture, ObjectClass::Level, ts->tileHeight, ts->tileWidth, srcX, srcY);

              if (layer.name != "Level") {
                tile.collider.w = tile.collider.h = 0;
              }

              if (layer.name == "Level" || isHazard) { // 4616 firstgid of Tileset
                if (auto it = ts->tiles.find(localId); it != ts->tiles.end() && it->second.collider) {
                  tile.collider = *(it->second.collider);
                  tile.data.level.isHazard = isHazard;
                  countModColliders += 1;
                };

              };

              newLayer.push_back(tile);

            }

          }
        }
      } else if (layer.name == "Background_4") { // 4
        auto bgImg = createObject(0, 0, res.m_currLevel->map->tileSets[res.m_currLevel->bg4Idx].texture, ObjectClass::Background, 0, 0, 0, 0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.2f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_3") { // 3
        auto bgImg = createObject(0, 0, res.m_currLevel->map->tileSets[res.m_currLevel->bg3Idx].texture, ObjectClass::Background, 0, 0, 0, 0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.2f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_2") { // 2
        auto bgImg = createObject(0, 0, res.m_currLevel->map->tileSets[res.m_currLevel->bg2Idx].texture, ObjectClass::Background, 0, 0, 0, 0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.3f;
        newLayer.push_back(bgImg);
      } else if (layer.name == "Background_1") { // 1
        auto bgImg = createObject(0, 0, res.m_currLevel->map->tileSets[res.m_currLevel->bg1Idx].texture, ObjectClass::Background, 0, 0, 0, 0);
        bgImg.bgscroll = 0;
        bgImg.scrollFactor = 0.4f;
        newLayer.push_back(bgImg);
      }

      gs.layers.push_back(std::move(newLayer));
    };

    void operator()(tmx::ObjectGroup &objectGroup)
    {
      std::vector<GameObject> newLayer;
      for (tmx::LayerObject &obj : objectGroup.objects)
      {
        glm::vec2 objStartingPos(
          obj.x - res.m_currLevel->map->tileWidth / 2, //17
          obj.y - res.m_currLevel->map->tileHeight / 2 //411
        );

        if (obj.type == "Portal") {
            GameObject portal = createObject(1, 1, nullptr, ObjectClass::Portal, 32, 32, 0, 0); // todo populate w/h passing from tmx file

            LevelIndex lvl = LevelIndex::LEVEL_2;
            if (res.m_currLevel->lvlIdx == LevelIndex::LEVEL_1) {
              lvl = LevelIndex::LEVEL_2;
            }
            if (res.m_currLevel->lvlIdx == LevelIndex::LEVEL_2) {
              lvl = LevelIndex::LEVEL_3;
            }
            portal.data.portal = PortalData(lvl);
            portal.colliderNorm = { .x=0.0, .y=0.5, .w=1.0, .h=1.0}; // TODO setting .y = 0.5 and h = 0.5 worked well for level2
            portal.applyScale();
            portal.position = objStartingPos;

            newLayer.push_back(std::move(portal));
        }


        if (obj.type == "Enemy") {

          SpriteType spriteType = CHARACTER_NAME_TO_SPRITE_TYPE.at(obj.name);
          GameObject enemy = createObject(1, 1, res.m_currLevel->texCharacterMap.at(spriteType).texIdle, ObjectClass::Enemy, 128, 128, 0, 0);
          enemy.spriteType = spriteType;
          // set the appropriate texture based on the obj.name

          // enemy.data.enemy.srcW = 128; // unsued
          // enemy.data.enemy.srcH = 128;// unsued
          switch (spriteType) {
            case SpriteType::Minotaur_1:
            {
              enemy.drawScale = 2.0f;
              break;
            }
            case SpriteType::Skeleton_Warrior:
            case SpriteType::Red_Werewolf:
            case SpriteType::Skeleton_Pikeman:
            {
              enemy.drawScale = 1.5f;
              break;
            }
          }
          float wFrac = 0.30f, hFrac = 0.6f;
          enemy.colliderNorm = { .x=0.35f, .y=0.4, .w=wFrac, .h=0.6}; // TODO setting .y = 0.5 and h = 0.5 worked well for level2
          enemy.applyScale();

          float feetY   = objStartingPos.y;                // baseline from Tiled
          float centerX = objStartingPos.x;                // baseline from Tiled
          enemy.position.x = centerX - enemy.collider.w * 0.5f;        // collider centered
          enemy.position.y = feetY - (enemy.collider.y + enemy.collider.h); // collider bottom on feet
          enemy.data.enemy = EnemyData();
          enemy.currentAnimation = res.ANIM_IDLE;
          enemy.animations = res.m_currLevel->texCharacterMap.at(spriteType).anims;
          enemy.dynamic = true;
          enemy.maxSpeedX = 15;
          newLayer.push_back(std::move(enemy));
        }

        // Must handle multiple players here; all players start in same position, so here we create a player
        if (obj.type == "Player") {
          SpriteType spriteType = CHARACTER_NAME_TO_SPRITE_TYPE.at(obj.name);
          int texDim = 128;
          // if (spriteType == SpriteType::Player_Marie) {
          //   texDim = 96;
          // }

          GameObject player = createObject(1, 1, res.m_currLevel->texCharacterMap.at(spriteType).texIdle, ObjectClass::Player, texDim, texDim, 0, 0); // NEED TO PASS DOWN CHARACTER TILE SIXES
          // Marie: 96
          // Knight: Mage: 128
          player.spriteType = spriteType;
          player.drawScale = 1.5f;

          float wFrac = 0.30f, hFrac = 0.40f;
          // Position collision box to overlay on character (character is left of center in sprite)
          // adjust
          player.colliderNorm = { .x=0.10f, .y=0.9f - hFrac, .w=wFrac, .h=hFrac };
          switch (spriteType) {
            case SpriteType::Player_Knight:
            {
              player.colliderNorm = { .x=0.1f, .y=0.5f, .w=wFrac, .h=0.5f };
              break;
            }
            case SpriteType::Player_Mage:
            {
              player.colliderNorm = { .x=0.30f, .y=0.5f, .w=wFrac, .h=0.5f };
              break;
            }
            case SpriteType::Player_Marie:
            {
              player.colliderNorm = { .x=0.30f, .y=0.5f, .w=wFrac, .h=0.5f };
              player.drawScale = 2.0f;
              break;
            }
          };

          player.applyScale();

          // TODO need a collider for attacks that get used during the attack state.

          float drawW = player.spritePixelW / player.drawScale;
          float drawH = player.spritePixelH / player.drawScale;

          // obj.x/obj.y from Tiled: treat as centerX/feetY
          float centerX = obj.x;
          float feetY   = obj.y;

          // place sprite top-left so its bottom is at feetY and center on centerX
          player.position.x = centerX - drawW * 0.5f;
          player.position.y = feetY   - drawH;

          player.data.player = PlayerData();
          player.animations = res.m_currLevel->texCharacterMap.at(spriteType).anims;
          player.currentAnimation = res.ANIM_IDLE;
          player.acceleration = glm::vec2(500, 0);
          player.maxSpeedX = 100;
          player.dynamic = true;

          newLayer.push_back(player);
          gs.playerIndex = newLayer.size() - 1;
          gs.playerLayer = gs.layers.size();
        }
      };
      gs.layers.push_back(std::move(newLayer));
    }
  };

  LayerVisitor visitor(m_sdlState, newGameState, m_resources);
  for (std::variant<tmx::Layer, tmx::ObjectGroup> &layer : m_resources.m_currLevel->map->layers) {
    std::visit(visitor, layer);
  };

  std::cout << "count:" << visitor.countModColliders << std::endl;


  return newGameState.playerIndex != -1;
};
