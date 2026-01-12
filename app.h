#pragma once

#include "gameengine.h"
// #include "net/net.h"


namespace App {


  class App
  {

    game_engine::Engine GameEngine;
    // Net::Client client; // TODO, build subclass into the App or GameEngine
    // Net::Server server; // TODO

    // some persistence object like database
    public:
      App() = default;

      ~App() = default;

      void Run();
  };

};