#pragma once
#include <atomic>
#include <iostream>
#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <glm/glm.hpp>

#include "engine/gameobject.h"
#include "engine/net/lan_discovery.h"
#include "engine/net/game_server.h" // needs complete type for unique_ptr destructor
#include "engine/net/game_client.h"
#include "engine/level_types.h"
#include "engine/ui_manager.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"

#include <SDL3_mixer/SDL_mixer.h>

#include "engine/tmx.h"

namespace eng {
  class IGameRules;
}

// GameApp class
// GameApp:start() -> creates GameEngine -> does everything thats done in the game loop rn
// GameApp class also has networking and persistence, imGui. We inject these as reference to the GameEngine
// persistence:
// Small/local only: write to files (JSON/TOML/INI) for settings; binary or JSON for save data. Use a schema/version field so you can evolve saves. Keep it in a known writable dir (per-platform: %APPDATA%, ~/Library/Application Support/, ~/.local/share/).
// Lightweight DB (SQLite): useful if you need multiple save slots with indexing, leaderboards, or richer queries. It’s a single file and embeddable, but adds a dependency and some overhead compared to just serializing structs.
// Remote/cloud: only if you need cross-device or online profiles; otherwise keep it local.


// Define the surface clearly: e.g., std::function<bool(const GameState&)> saveFn and maybe loadFn. Keep the signature narrow and types stable.
// Ownership/lifetime: store the function objects by value in GameEngine; ensure whatever they capture outlives the engine (or use a weak pointer pattern to detect expired).
// this way I can also create offline mode where networking object is nullptr
// and the functions dont do anything or defauly to local versions
// Error handling: decide how failures are signaled (bool/Status/exception) and what the engine should do if a save fails.
// Threading: if you ever call the callback off the main thread, make sure the persistence side is thread-safe.
// Testing: callbacks make it easy to inject fakes in tests.
// If you find you need more than one or two functions, consider a tiny interface/struct instead (e.g., struct PersistenceAPI { SaveFn save; LoadFn load; };) to avoid callback sprawl.


// sips -z 42 42 data/idle_marie.png --out data/idle_marie_42.png --> resize
// magick data/move_helmet_marie.png -filter point -resize 210x42! data/move_helmet_marie_42.png

namespace game_engine {

  // forward declare
  class GameClient;

  struct SDLState {
    SDL_Window *window;
    SDL_Renderer *renderer;
    int width, height, logW, logH;
    const bool *keys;
    bool fullscreen;

    // TODO create a new struct for imgui params (flags, button sizes, images etc.)
    ImGuiWindowFlags ImGuiWindowFlags =
      ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoSavedSettings;

    SDLState() : keys(SDL_GetKeyboardState(nullptr)) {}
  };

  // enum class GameView {
  //     Playing,
  //     MainMenu,
  //     PauseMenu,
  //     LevelLoading,
  //     InventoryMenu,
  //     GameOver,
  //     MultiPlayerOptionsMenu, // this menu will show host or client buttons
  // };

  struct GameState {

    GameState() = default;
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    // std::atomic is neither copy- nor move-assignable, so provide a custom move
    GameState(GameState&& other) noexcept
      : m_loadProgress(other.m_loadProgress.load()),
        currentView(other.currentView),
        currentLevelId(other.currentLevelId),
        m_stateLastUpdatedAt(other.m_stateLastUpdatedAt),
        layers(std::move(other.layers)),
        bullets(std::move(other.bullets)),
        debugMode(other.debugMode),
        selectedPlayerSprite(other.selectedPlayerSprite),
        playerLayer(other.playerLayer),
        playerIndex(other.playerIndex),
        mapViewport(other.mapViewport),
        bg2scroll(other.bg2scroll),
        bg3scroll(other.bg3scroll),
        bg4scroll(other.bg4scroll)
    {}

    GameState& operator=(GameState&& other) noexcept {
      if (this != &other) {
        m_loadProgress.store(other.m_loadProgress.load());
        currentView         = other.currentView;
        currentLevelId      = other.currentLevelId;
        m_stateLastUpdatedAt= other.m_stateLastUpdatedAt;
        layers              = std::move(other.layers);
        bullets             = std::move(other.bullets);
        debugMode           = other.debugMode;
        selectedPlayerSprite= other.selectedPlayerSprite;
        playerLayer         = other.playerLayer;
        playerIndex         = other.playerIndex;
        mapViewport         = other.mapViewport;
        bg2scroll           = other.bg2scroll;
        bg3scroll           = other.bg3scroll;
        bg4scroll           = other.bg4scroll;
      }
      return *this;
    }

      std::atomic<uint8_t> m_loadProgress = 100;

      UIManager::GameView currentView;
      LevelIndex currentLevelId{LevelIndex::LEVEL_1};

      uint64_t m_stateLastUpdatedAt; // when the gameState was last updated, by local or by server msg

      std::vector<std::vector<GameObject>> layers;
      // std::vector<GameObject> backgroundTiles;
      // std::vector<GameObject> foregroundTiles;
      std::vector<GameObject> bullets;
      bool debugMode;
      SpriteType selectedPlayerSprite{SpriteType::Player_Marie};

      int playerLayer, playerIndex;
      SDL_FRect mapViewport; // viewable part of map
      float bg2scroll, bg3scroll, bg4scroll;

      GameState(const SDLState &state): bg2scroll(0), bg3scroll(0), bg4scroll(0), currentView(UIManager::GameView::MainMenu) {
        playerIndex = -1;
        mapViewport = SDL_FRect{
          .x = 0, .y = 0,
          .w = static_cast<float>(state.logW),
          .h = static_cast<float>(state.logH)
        };
        debugMode = false;
      }

      // get current player
      GameObject &player(size_t layer_idx_chars) { return layers[playerLayer][playerIndex]; }

      void setLevelLoadProgress(uint8_t progress) {m_loadProgress.store(progress); }
      uint8_t getLevelLoadProgress() { return m_loadProgress.load(); }

      bool evaluateGameOver() {

        GameObject& p = player(playerIndex);

        if (!p.grounded && p.position.y > 1500) {
          currentView = UIManager::GameView::GameOver;
          return true;
        }
        return false;

      };

      game_engine::NetGameStateSnapshot extractNetSnapshot() const {
        NetGameStateSnapshot snapshot;
        snapshot.serverTick = m_stateLastUpdatedAt;
        snapshot.levelId = currentLevelId;
        snapshot.m_stateLastUpdatedAt = m_stateLastUpdatedAt;

        for (size_t layerIdx = 0; layerIdx < layers.size(); ++layerIdx) {
            for (const auto& obj : layers[layerIdx]) {
              if (obj.dynamic) {
                NetGameObjectSnapshot s{};
                s.id = obj.id;
                s.layer = static_cast<uint32_t>(layerIdx);
                s.type = obj.objClass;
                s.spriteType = obj.spriteType;
                s.position = obj.position;
                s.velocity = obj.velocity;
                s.acceleration = obj.acceleration;
                s.direction = obj.direction;
                s.maxSpeedX = obj.maxSpeedX;
                s.currentAnimation =
                  obj.currentAnimation >= 0 ? static_cast<uint32_t>(obj.currentAnimation) : UINT32_MAX;
                s.grounded = obj.grounded;
                s.shouldFlash = obj.shouldFlash;
                s.spriteFrame = static_cast<uint32_t>(obj.spriteFrame);
                s.animElapsed =
                  obj.currentAnimation >= 0 &&
                      obj.currentAnimation < static_cast<int>(obj.animations.size())
                    ? obj.animations[obj.currentAnimation].getElapsed()
                    : 0.0f;
                s.animTimedOut =
                  obj.currentAnimation >= 0 &&
                      obj.currentAnimation < static_cast<int>(obj.animations.size())
                    ? obj.animations[obj.currentAnimation].isDone()
                    : false;
                s.presentationVariant = obj.presentationVariant;
                s.data = obj.data; // union to be handled in encodeNetGameStateSnapshot
                snapshot.m_gameObjects[{obj.objClass, obj.id}] = s;
              };
            };
        };

        for (const auto& obj : bullets) {
          // if (obj.dynamic) {
          NetGameObjectSnapshot s{};
          s.id = obj.id;
          s.layer = static_cast<uint32_t>(playerLayer);
          s.type = obj.objClass;
          s.spriteType = obj.spriteType;
          s.position = obj.position;
          s.velocity = obj.velocity;
          s.acceleration = obj.acceleration;
          s.direction = obj.direction;
          s.maxSpeedX = obj.maxSpeedX;
          s.currentAnimation =
            obj.currentAnimation >= 0 ? static_cast<uint32_t>(obj.currentAnimation) : UINT32_MAX;
          s.grounded = obj.grounded;
          s.shouldFlash = obj.shouldFlash;
          s.spriteFrame = static_cast<uint32_t>(obj.spriteFrame);
          s.animElapsed =
            obj.currentAnimation >= 0 &&
                obj.currentAnimation < static_cast<int>(obj.animations.size())
              ? obj.animations[obj.currentAnimation].getElapsed()
              : 0.0f;
          s.animTimedOut =
            obj.currentAnimation >= 0 &&
                obj.currentAnimation < static_cast<int>(obj.animations.size())
              ? obj.animations[obj.currentAnimation].isDone()
              : false;
          s.presentationVariant = obj.presentationVariant;
          snapshot.m_gameObjects[{obj.objClass, obj.id}] = s;
          // }
        };

          return snapshot;
        };

  };

  /*
  Engine is the main class that provides all the functionality to run our game
  TODO: Must refactor this with Template pattern to make it a standalone reusable 2D game engine interface
  */
  class Engine
  {
    enum GameRunMode {
      SinglePlayer, // default to single player
      Host,
      Client,
    };

    private:
      SDLState m_sdlState;
      GameState m_gameState;
      MIX_Mixer* m_mixer{nullptr};
      std::atomic<bool> m_gameRunning{false};
      std::mutex m_levelMutex;
      GameRunMode m_gameType;

      std::unique_ptr<GameServer> m_gameServer = nullptr; // only create if gameType==Host
      bool isConnectedToServer = false;
      std::unique_ptr<GameClient> m_gameClient = nullptr; // only create if gameType!=SinglePlayer
      std::unique_ptr<DiscoveryHostService> m_discoveryHost = nullptr;
      std::unique_ptr<DiscoveryBrowserService> m_discoveryBrowser = nullptr;
      std::thread m_serverLoopThd;
      std::atomic<bool> m_serverLoopRunning{false};
      std::thread m_levelLoadThd;
      NetGameInput m_localInput{};
      uint32_t m_localInputSeq = 0;
      float m_inputSendAccumulator = 0.0f;
      std::string m_selectedJoinHost = "127.0.0.1";
      uint16_t m_selectedJoinPort = GAME_SERVER_PORT;
      bool m_hasSelectedJoinTarget = false;
      bool m_serverReadyForDiscovery = false;
      std::string m_multiplayerStatus;


    public:
    // TODO later will need to pass gameConfig to constructor
      Engine() : m_sdlState{}, m_gameState(m_sdlState), m_gameType(SinglePlayer) {
      }
      ~Engine();

      // physics should handle this and collision and response
      inline static constexpr glm::vec2 GRAVITY = glm::vec2(0, 600);
      inline static constexpr float JUMP_FORCE = -400.0f;

      inline static constexpr size_t LAYER_IDX_LEVEL = 0;
      inline static constexpr size_t LAYER_IDX_CHARACTERS = 1;
      inline static constexpr int TILE_SIZE = 32;

      // core engine/renderer
      bool init(int width, int height, int logW, int logH);
      void run(eng::IGameRules& rules);
      bool initWindowAndRenderer(int width, int height, int logW, int logH);
      void cleanupTextures();
      void cleanup(); // can be ref or pointer; if using pointer, need to use -> instead of

      // AudioManager
      void setAudioSoundtrack(MIX_Track* track, int loops = -1); // generic soundtrack play
      void stopAudioSoundtrack(MIX_Track* track);

      // Networking
      std::unique_ptr<GameServer> buildAuthoritativeStateForServer();
      bool handleMultiplayerConnections();
      void runGameServerLoopThread();
      void resetMultiplayerNetworkingState();
      void submitLocalInput(NetGameInput input);
      void flushLocalInput(float deltaTime);
      void restartMultiplayerSession();
      void synchronizeHostAuthoritativeState(bool refreshSpawnPositions = false);
      std::optional<LevelIndex> consumePendingHostLevelTransition();
      void broadcastHostSnapshot();
      bool copyHostSnapshot(NetGameStateSnapshot& out) const;
      std::vector<DiscoveredSessionInfo> copyDiscoveredSessions() const;
      bool selectDiscoveredSession(size_t index);
      bool hasSelectedJoinTarget() const;
      bool isServerReadyForDiscovery() const;
      const std::string& getMultiplayerStatus() const;
      const NetGameInput& getLocalInput() const;
      GameClient* getGameClient();
      const GameClient* getGameClient() const;
      bool isMultiplayerActive() const;
      // MIX_PauseTrack(track) / MIX_ResumeTrack(track)


      // File IO for Save Files
      std::filesystem::path getSaveRootDir();
      std::filesystem::path resolveSlotPath(const std::string& slotName);
      std::vector<uint8_t> readSlot(const std::string& slotName);
      bool writeToSlotPath(const std::string& slotName, const std::vector<uint8_t>& profileBytes);


      // getters
      GameObject &getPlayer();
      SDLState &getSDLState();
      GameState &getGameState();
      MIX_Mixer* getMixer();

      // setters
      void setWindowSize(int height, int width);
      void setRunModeSinglePlayer();
      void setRunModeHost();
      void setRunModeClient();
      void requestQuit();
      bool isRunning() const;
      bool isHostMode() const;
      bool isClientMode() const;

  };

}
