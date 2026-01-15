#pragma once
#include "net/net_client.h"
#include "game_net_common.h"


namespace game_engine {


  class GameClient : public net::client_interface<GameMsgHeaders> {
    public:

      GameClient() = default;
      ~GameClient() = default;

    private:
      std::unordered_map<uint32_t, NetGameObjectSnapshot> mapObjects;
      uint32_t nPlayerID = 0;
      NetGameObjectSnapshot descPlayer;

      bool bWaitingForConnection = true;


    public:
      bool OnUserCreate()
      {

        if (Connect("127.0.0.1", 60000))
        {
          return true;
        }

        return false;
      }

      bool OnUserUpdate(float fElapsedTime)
      {
        // Check for incoming network messages
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
              net::message<GameMsgHeaders> msg;
              msg.header.id = GameMsgHeaders::Client_RegisterWithServer;
              descPlayer.vPos = 3.0f;
              msg << descPlayer;
              Send(msg);
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
              NetGameObjectSnapshot desc;
              msg >> desc;
              mapObjects.insert_or_assign(desc.id, desc);

              if (desc.id == nPlayerID)
              {
                // Now we exist in game world
                bWaitingForConnection = false;
              }
              break;
            }

            case(GameMsgHeaders::Game_RemovePlayer):
            {
              uint32_t nRemovalID = 0;
              msg >> nRemovalID;
              mapObjects.erase(nRemovalID);
              break;
            }

            case(GameMsgHeaders::Game_UpdatePlayer):
            {
              NetGameObjectSnapshot desc;
              msg >> desc;
              mapObjects.insert_or_assign(desc.id, desc);
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

  };

};