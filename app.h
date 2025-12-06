#pragma once

#include "gameengine.h"
#include "net.h"


namespace App {


  class App
  {

    GameEngine::GameEngine GameEngine;
    Net::Client client;
    Net::Server server;

    // some persistence object like database
    public:
      App() {};

      ~App(){}

      void Run();
  };

};