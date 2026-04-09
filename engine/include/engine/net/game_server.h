#pragma once

#include "engine/net/game_net_common.h"
#include "net/net_server.h"

#include <memory>

namespace game_engine {

struct GameState;

struct AuthoritativeContext {
  std::unique_ptr<GameState> state;
  std::unordered_map<uint32_t, NetGameInput> latestPlayerInputs;
  uint64_t serverTick = 0;

  explicit AuthoritativeContext(GameState&& initialState);
};

class GameServer : public net::server_interface<GameMsgHeaders> {
public:
  GameServer(uint16_t nPort, std::unique_ptr<AuthoritativeContext> authCtx);

  std::unordered_map<uint32_t, NetGameObjectSnapshot> m_mapPlayerRoster;
  std::vector<uint32_t> m_vGarbageIDs;
  NetGameStateSnapshot m_currGameSnapshot;
  net::tsqueue<NetGameInput> m_playerInputQueue;
  std::unique_ptr<AuthoritativeContext> m_authCtx;

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
  void resetAuthoritativeState(GameState&& initialState);
  bool registerPlayer(uint32_t playerID, SpriteType spriteType);
  void removePlayer(uint32_t playerID);
};

} // namespace game_engine
