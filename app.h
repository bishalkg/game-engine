#pragma once

#include "gameengine.h"
#include "net/net_client.h"


namespace App {


  class App
  {
    // TODO gameConfig will be passed to GameEngine
    // game_engine::Engine GameEngine;
    // Net::Client client; // TODO, build subclass into the App or GameEngine
    // Net::Server server; // TODO

    // some persistence object like database
    public:
      App() = default;

      ~App() = default;

      void Run();
  };


  // TODO define a server and a client
  // In App() -> use cli flag to init a server or a client and connect the two to test

  enum class CustomMsgTypes : uint32_t
  {
    ServerAccept,
    ServerDeny,
    ServerPing,
    MessageAll,
    ServerMessage,

    FireBullet
  };

  // class GameClient : public net::client_interface<CustomMsgTypes>
  // {

  //   public:
  //     bool FireBullet(float x, float y) {
  //       net::message<CustomMsgTypes> msg;
  //       msg.header.id = CustomMsgTypes::FireBullet;
  //       msg << x << y;
  //       // this->m_connection->Send(msg); // TODO
  //       Send(msg);
  //     }

  // };

};