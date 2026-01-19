#pragma once
#include "net/net_client.h"
#include "game_net_common.h"


namespace game_engine {


  class GameClient : public net::client_interface<GameMsgHeaders> {
    public:

      GameClient() = default;
      ~GameClient() = default;

    private:
      // tuple of object type and objectID
      // need this behind a mutex.
      // the idea is the client queue will dequeue and update the clients mapObjects state as snapshots come in
      // the game engine can then read from this mapObjects during its render loop (updateAllObjects) to update the actual game state
      std::mutex m_gameStateMu;
      NetGameStateSnapshot m_latestSnapshot;
      // std::unordered_map<std::tuple<ObjectType, uint32_t>, NetGameObjectSnapshot> m_gameObjects;
      // uint64_t m_stateLastUpdatedAt; // when the gameState was last updated
      uint32_t nPlayerID = 0;
      NetGameObjectSnapshot descPlayer;

      bool bWaitingForConnection = true;
      bool m_isClientValidated = false;


    public:
      bool OnUserCreate()
      {

        if (Connect("127.0.0.1", 60000))
        {
          return true;
        }

        return false;
      }

      bool IsClientValidated() const {
        return m_isClientValidated;
      }

      // Client calls this in game loop to read in messages from the server
      bool OnUserUpdate(float fElapsedTime)
      {
        // Check for incoming network messages from server
        if (IsConnected())
        {
          while (!Incoming().empty())
          {
            auto msg = Incoming().pop_front().msg;

            switch (msg.header.id)
            {
            case(GameMsgHeaders::Client_Accepted):
            {
              std::cout << "Server accepted client - you're in!\n";
              m_isClientValidated = true;

              // net::message<GameMsgHeaders> msg;
              // msg.header.id = GameMsgHeaders::Client_RegisterWithServer;
              // descPlayer.vPos = 3.0f;
              // msg << descPlayer;
              // Send(msg);
              break;
            }

            case(GameMsgHeaders::Client_AssignID):
            {
              // Server is assigning us OUR id
              msg >> nPlayerID;
              std::cout << "Assigned Client ID = " << nPlayerID << "\n";
              break;
            }

            case(GameMsgHeaders::Game_AddPlayer):
            {
              // NetGameObjectSnapshot desc;
              // msg >> desc;
              // mapObjects.insert_or_assign(desc.id, desc);

              // if (desc.id == nPlayerID)
              // {
              //   // Now we exist in game world
              //   bWaitingForConnection = false;
              // }
              break;
            }

            case(GameMsgHeaders::Game_RemovePlayer):
            {
              uint32_t nRemovalID = 0;
              msg >> nRemovalID;
              m_latestSnapshot.m_gameObjects.erase(std::pair(ObjectType::Player, nRemovalID));
              break;
            }

            case(GameMsgHeaders::Game_Snapshot):
            {
              // TODO deserealize the gameSnapshot and set it onto mapObjects and other private variables...We will need a mutex for reading the data so that it doesnt
              // get updated as we are reading it to set the client data

              // NetGameObjectSnapshot desc;
              // msg >> desc;
              // mapObjects.insert_or_assign(desc.id, desc);
              swapGameStateWithNew(msg);
              break;
            }


            }
          }
        }

        if (bWaitingForConnection)
        {
          // Clear(olc::DARK_BLUE);
          // DrawString({ 10,10 }, "Waiting To Connect...", olc::WHITE);
          return true;
        }

        return false;

      };

    private:
      void swapGameStateWithNew(net::message<game_engine::GameMsgHeaders>& msg) {
        std::scoped_lock lock(m_gameStateMu);
        NetGameStateSnapshot latestSnapshot;
        // TODO actual deserealization
        // msg >> latestSnapshot;
        // m_latestSnapshot = latestSnapshot;
      }

  };

};

// 1. write out the serealizer/deserealizer
// 2. write out the computation logic on the server (server should also have its own GameState like the client or use the snapshot? whichevers easier..)
// 3. client to use the read in snapshot to update objects in updateAllObjects