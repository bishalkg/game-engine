#pragma once

#include "gameengine.h"
#include "net.h"


namespace App {


  class App
  {

    game_engine::Engine GameEngine;
    Net::Client client; // TODO
    Net::Server server; // TODO

    // some persistence object like database
    public:
      App() = default;

      ~App() = default;

      void Run();
  };

};