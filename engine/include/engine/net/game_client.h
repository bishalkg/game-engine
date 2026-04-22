#pragma once

#include "engine/net/game_net_common.h"
#include "net/net_client.h"

namespace game_engine {

class GameClient : public net::client_interface<GameMsgHeaders> {
public:
  GameClient() = default;
  ~GameClient() = default;

  bool IsClientValidated() const {
    return m_isClientValidated;
  }

  bool IsRegistered() const {
    return m_isRegistered;
  }

  uint32_t GetPlayerID() const {
    return m_playerID;
  }

  bool NeedsFullRebuild() const {
    return m_needsFullRebuild;
  }

  void MarkFullRebuildApplied() {
    m_needsFullRebuild = false;
  }

  bool IsRespawnPending() const {
    return m_respawnRequested;
  }

  void ProcessServerMessages() {
    if (!IsConnected()) {
      return;
    }

    bool haveNewSnapshot = false;
    NetGameStateSnapshot newestSnapshot;
    uint64_t newestTick = m_latestServerTickReceived;

    while (!Incoming().empty()) {
      auto msg = Incoming().pop_front().msg;

      switch (msg.header.id) {
        case GameMsgHeaders::Client_Accepted: {
          m_isClientValidated = true;
          break;
        }
        case GameMsgHeaders::Client_AssignID: {
          net::ByteReader reader(msg.body);
          m_playerID = reader.read_u32();
          m_isRegistered = true;
          break;
        }
        case GameMsgHeaders::Game_Snapshot: {
          NetGameStateSnapshot latestSnapshot;
          latestSnapshot.deserealizeNetGameStateSnapshot(msg.body);
          if (latestSnapshot.serverTick >= newestTick) {
            newestTick = latestSnapshot.serverTick;
            newestSnapshot = std::move(latestSnapshot);
            haveNewSnapshot = true;
          }
          break;
        }
        case GameMsgHeaders::Game_RemovePlayer: {
          net::ByteReader reader(msg.body);
          const uint32_t playerID = reader.read_u32();
          std::scoped_lock lock(m_gameStateMu);
          m_latestSnapshot.m_gameObjects.erase({ObjectClass::Player, playerID});
          break;
        }
        default:
          break;
      }
    }

    if (haveNewSnapshot) {
      std::scoped_lock lock(m_gameStateMu);
      if (!m_hasSnapshot || newestSnapshot.serverTick >= m_latestSnapshot.serverTick) {
        m_latestSnapshot = std::move(newestSnapshot);
        m_hasSnapshot = true;
        m_latestServerTickReceived = m_latestSnapshot.serverTick; // TODO why is this not newestSnapshot.serverTick
        auto it = m_latestSnapshot.m_gameObjects.find({ObjectClass::Player, m_playerID});
        if (it != m_latestSnapshot.m_gameObjects.end() &&
            it->second.data.player.state != PlayerState::dead) {
          m_respawnRequested = false;
        }
      }
    }
  }

  void RegisterWithServer(SpriteType spriteType) {
    if (!IsClientValidated() || m_isRegistered) {
      return;
    }

    net::message<GameMsgHeaders> msg;
    msg.header.id = GameMsgHeaders::Client_RegisterWithServer;
    net::ByteWriter writer;
    writer.write_enum(spriteType);
    msg.body = std::move(writer.buff);
    msg.header.bodySize = msg.body.size();
    Send(msg);
  }

  void SendInput(const NetGameInput& input) {
    if (!IsConnected() || !m_isRegistered) {
      return;
    }

    net::message<GameMsgHeaders> msg;
    msg.header.id = GameMsgHeaders::Game_PlayerInput;
    msg.body = input.serealizeNetGameInput();
    msg.header.bodySize = msg.body.size();
    Send(msg);
  }

  void UnregisterFromServer() {
    if (!IsConnected() || !m_isRegistered) {
      return;
    }

    net::message<GameMsgHeaders> msg;
    msg.header.id = GameMsgHeaders::Client_UnregisterWithServer;
    Send(msg);

    m_isRegistered = false;
    m_isClientValidated = false;
    m_playerID = 0;
    m_respawnRequested = false;
    ClearLatestSnapshot();
  }

  bool CopyLatestSnapshot(NetGameStateSnapshot& out) const {
    std::scoped_lock lock(m_gameStateMu);
    if (!m_hasSnapshot) {
      return false;
    }
    out = m_latestSnapshot;
    return true;
  }

  void ClearLatestSnapshot() {
    std::scoped_lock lock(m_gameStateMu);
    m_latestSnapshot = NetGameStateSnapshot{};
    m_hasSnapshot = false;
    m_needsFullRebuild = true;
  }

  void RequestRespawn() {
    if (IsConnected() && m_isRegistered && !m_respawnRequested) {
      net::message<GameMsgHeaders> msg;
      msg.header.id = GameMsgHeaders::Game_PlayerRespawnRequest;
      Send(msg);
      m_respawnRequested = true;
    }

    ClearLatestSnapshot();
  }

private:
  mutable std::mutex m_gameStateMu;
  NetGameStateSnapshot m_latestSnapshot;
  uint32_t m_playerID = 0;
  bool m_isClientValidated = false;
  bool m_isRegistered = false;
  bool m_hasSnapshot = false;
  bool m_respawnRequested = false;
  bool m_needsFullRebuild = true;
  uint64_t m_latestServerTickReceived = 0;
};

} // namespace game_engine
