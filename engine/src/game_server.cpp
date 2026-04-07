#include "engine/net/game_server.h"
#include "engine/engine.h"

namespace game_engine {

  GameServer::GameServer(uint16_t nPort, GameState& gs)
      : net::server_interface<GameMsgHeaders>(nPort),
        m_currGameState(gs) {}


  bool GameServer::OnClientConnect(std::shared_ptr<net::connection<GameMsgHeaders>> client)
  {
    // For now we will allow all
    return true;
  }

  void GameServer::OnClientValidated(std::shared_ptr<net::connection<GameMsgHeaders>> client)
  {
    // Client passed validation check, so send them a message informing
    // them they can continue to communicate
    net::message<GameMsgHeaders> msg;
    msg.header.id = GameMsgHeaders::Client_Accepted;
    client->Send(msg);
  }

  void GameServer::OnClientDisconnect(std::shared_ptr<net::connection<GameMsgHeaders>> client)
  {
    if (client)
    {
      if (m_mapPlayerRoster.find(client->GetID()) == m_mapPlayerRoster.end())
      {
        // client never added to roster, so just let it disappear
      }
      else
      {
        auto& pd = m_mapPlayerRoster[client->GetID()];
        std::cout << "[UNGRACEFUL REMOVAL]:" + std::to_string(pd.id) + "\n";
        m_mapPlayerRoster.erase(client->GetID());
        m_vGarbageIDs.push_back(client->GetID());
      }
    }
  }

  // OnMessage is an override that is called in .ProcessIncomingMessages on the server
  void GameServer::OnMessage(std::shared_ptr<net::connection<GameMsgHeaders>> client, net::message<GameMsgHeaders>& msg)
  {
    if (!m_vGarbageIDs.empty()) {
      for (auto pid : m_vGarbageIDs)
      {
        net::message<GameMsgHeaders> m;
        m.header.id = GameMsgHeaders::Game_RemovePlayer;
        m << pid;
        std::cout << "Removing " << pid << "\n";
        BroadcastToClients(m);
      }
      m_vGarbageIDs.clear();
    }



    switch (msg.header.id) {
      case GameMsgHeaders::Client_RegisterWithServer:
        {
          // NetGameObjectSnapshot desc;
          // msg >> desc;
          // desc.id = client->GetID();
          // m_mapPlayerRoster.insert_or_assign(desc.id, desc);

          // net::message<GameMsgHeaders> msgSendID;
          // msgSendID.header.id = GameMsgHeaders::Client_AssignID;
          // msgSendID << desc.id;
          // MessageClient(client, msgSendID);

          // net::message<GameMsgHeaders> msgAddPlayer;
          // msgAddPlayer.header.id = GameMsgHeaders::Game_AddPlayer;
          // msgAddPlayer << desc;
          // BroadcastToClients(msgAddPlayer);

          // for (const auto& player : m_mapPlayerRoster)
          // {
          //   net::message<GameMsgHeaders> msgAddOtherPlayers;
          //   msgAddOtherPlayers.header.id = GameMsgHeaders::Game_AddPlayer;
          //   msgAddOtherPlayers << player.second;
          //   MessageClient(client, msgAddOtherPlayers);
          // }

          break;
        }

      case GameMsgHeaders::Client_UnregisterWithServer:
        {
          break;
        }

      case GameMsgHeaders::Game_PlayerInput:
      {

        NetGameInput input;
        input.deserealizeNetGameInput(msg.body);
        // std::cout << "got input: " << input.playerID << " move: " << static_cast<int>(input.move) << " projectile: " << static_cast<int>(input.fireProjectile) << " swing: " << static_cast<int>(input.swingWeapon) << '\n';

        m_playerInputQueue.push_back(input);

        // OnMessage: keep this light—deserialize NetGameInput, tag it with client->GetID() (ignore playerID in the payload to prevent spoofing), push it into a per-player input queue, and return. Apply inputs during the server tick so physics/collision run once per frame with deterministic ordering, not per-message.

        break;
      }
      // case GameMsgHeaders::Game_UpdatePlayer:
      //   {
      //     // this should be the msg that is the user inputs

      //     // Simply bounce update to everyone except incoming client
      //     BroadcastToClients(msg, client);
      //     break;
      //   }

    }

  }


  void GameServer::applyPlayerInputs() {
    while (!m_playerInputQueue.empty())
    {
      auto input = m_playerInputQueue.pop_front();


    };

    m_playerInputQueue.clear();
  };

  void GameServer::step() {

  };



}
