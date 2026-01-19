#pragma once

#include "net/net_server.h"
#include "game_net_common.h"



namespace game_engine {

  class GameServer : public net::server_interface<GameMsgHeaders> {
    public:
      GameServer(uint16_t nPort) : net::server_interface<GameMsgHeaders>(nPort){}

      std::unordered_map<uint32_t, NetGameObjectSnapshot> m_mapPlayerRoster;
      std::vector<uint32_t> m_vGarbageIDs;

      NetGameStateSnapshot m_currGameState;

    protected:
      bool OnClientConnect(std::shared_ptr<net::connection<GameMsgHeaders>> client) override
      {
        // For now we will allow all
        return true;
      }

      void OnClientValidated(std::shared_ptr<net::connection<GameMsgHeaders>> client) override
      {
        // Client passed validation check, so send them a message informing
        // them they can continue to communicate
        net::message<GameMsgHeaders> msg;
        msg.header.id = GameMsgHeaders::Client_Accepted;
        client->Send(msg);
      }

      void OnClientDisconnect(std::shared_ptr<net::connection<GameMsgHeaders>> client) override
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
      void OnMessage(std::shared_ptr<net::connection<GameMsgHeaders>> client, net::message<GameMsgHeaders>& msg) override
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

            NetGameInput input = deserealizeNetGameInput(msg.body);
            std::cout << "got input: " << input.playerID << " move: " << static_cast<int>(input.move) << " projectile: " << static_cast<int>(input.fireProjectile) << " swing: " << static_cast<int>(input.swingWeapon) << '\n';

            // for now its ok to process one message at a time, but later we might want to process a bunch
            // that have queued up



            // BroadcastToClients(msg, client) will be called in runGameServerLoopThread.
            // generate a GameState message and broadcast to all clients
            // update the
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

  };

}