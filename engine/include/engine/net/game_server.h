#pragma once

#include "engine/net/game_net_common.h"
#include "net/net_server.h"

#include <memory>
#include <mutex>
#include <optional>

namespace game_engine {

struct GameState;

enum class PlayerSessionState : uint8_t {
  connected,
  alive,
  dead,
  respawning,
};

struct PlayerSession {
  SpriteType spriteType = SpriteType::Player_Marie;
  PlayerSessionState lifecycle = PlayerSessionState::connected;
  uint32_t lastInputSeq = 0;
  glm::vec2 spawnPosition{0.0f, 0.0f};
};

struct AuthoritativeContext {
  std::unique_ptr<GameState> state;
  std::unordered_map<uint32_t, NetGameInput> latestPlayerInputs;
  uint64_t serverTick = 0;

  explicit AuthoritativeContext(GameState&& initialState);
};

class GameServer : public net::server_interface<GameMsgHeaders> {
public:
  GameServer(uint16_t nPort, std::unique_ptr<AuthoritativeContext> authCtx);

  std::unordered_map<uint32_t, PlayerSession> m_playerSessions;
  std::vector<uint32_t> m_vGarbageIDs;
  NetGameStateSnapshot m_currGameSnapshot;
  net::tsqueue<NetGameInput> m_playerInputQueue;
  std::unique_ptr<AuthoritativeContext> m_authCtx;
  mutable std::recursive_mutex m_stateMu;
  mutable std::mutex m_pendingLevelTransitionMu;
  std::optional<LevelIndex> m_pendingLevelTransition;

protected:
  bool OnClientConnect(std::shared_ptr<net::connection<GameMsgHeaders>> client) override;
  void OnClientValidated(std::shared_ptr<net::connection<GameMsgHeaders>> client) override;
  void OnClientDisconnect(std::shared_ptr<net::connection<GameMsgHeaders>> client) override;
  void OnMessage(std::shared_ptr<net::connection<GameMsgHeaders>> client, net::message<GameMsgHeaders>& msg) override;

public:
  GameObject* findPlayerById(uint32_t playerID);
  void applyPlayerInputs();
  void step(float deltaTime);
  void refreshSnapshot();
  void broadcastSnapshot();
  bool copyCurrentSnapshot(NetGameStateSnapshot& out) const;
  void resetAuthoritativeState(GameState&& initialState, bool refreshSpawnPositions = false);
  bool registerPlayer(uint32_t playerID, SpriteType spriteType);
  bool respawnPlayer(uint32_t playerID);
  bool removePlayer(uint32_t playerID);
  bool HasPendingLevelTransition() const;
  std::optional<LevelIndex> ConsumePendingLevelTransition();
};

} // namespace game_engine
