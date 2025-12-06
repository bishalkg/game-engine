#include "gameengine.h"

GameObject &GameEngine::GameEngine::getPlayer() {
 return gs.player(LAYER_IDX_CHARACTERS);
};

GameEngine::SDLState &GameEngine::GameEngine::getSDLState() {
  return state;
};

GameEngine::GameState &GameEngine::GameEngine::getGameState() {
  return gs;
};

GameEngine::Resources &GameEngine::GameEngine::getResources() {
  return res;
};

void GameEngine::GameEngine::setWindowSize(int height, int width) {
  state.width = width;
  state.height = height;
}

bool GameEngine::GameEngine::init(int width, int height, int logW, int logH) {

  // SDL, ImGUI are initialized

  // init window and renderer
  if (!this->initWindowAndRenderer(width, height, logW, logH)) {
    return false;
  };

  // load game assets
  res.load(state);
  if (!res.texIdle) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Idle Texture load failed", "Failed to load idle image", nullptr);
    this->cleanup();
    return false;
  }

  // setup game data
  gs = GameState(state);
  return this->initAllTiles();
}


void GameEngine::GameEngine::runGameLoop() {
    // start the game loop
  uint64_t prevTime = SDL_GetTicks();
  this->running = true;

  GameState &gs = this->getGameState();
  SDLState &sdl = this->getSDLState();
  Resources &res = this->getResources();
  while (this->running){

    GameObject &player = this->getPlayer();  // fetch each frame in case index changes
    // use gs.currentView directly so state changes take effect immediately

    uint64_t nowTime = SDL_GetTicks();
    float deltaTime = (nowTime - prevTime) / 1000.0f; // convert to seconds; time bw frames

    this->runEventLoop(player);

    // this->updateImGuiMenuRenderState()
    // 4) Start a new ImGui frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // 5) Build your ImGui UI for THIS frame
    // (menus, pause, debug overlay, etc.)
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImVec2 buttonSize = ImVec2(150, 50); // TODO put in config

    switch (gs.currentView) {
      case GameScreen::MainMenu:
        ImGui::Begin("Main Menu", nullptr, sdl.ImGuiWindowFlags);
        ImGui::Text("Hello from ImGui in SDL3!");
        if (ImGui::Button("Start", buttonSize)) {
            // startGame();
            std::cout << "start game" << std::endl;
            std::puts("Start clicked");
            gs.currentView = GameScreen::Playing;
        }
        if (ImGui::Button("Multiplayer",buttonSize)) {
          gs.currentView = GameScreen::MultiPlayerOptionsMenu;
          std::cout << "multi game" << std::endl;
        }
        if (ImGui::Button("Quit", buttonSize)) {
            std::cout << "quit game" << std::endl;
            running = false;
        }
        ImGui::End();
        break;
      case GameScreen::PauseMenu:
        ImGui::Begin("Pause", nullptr, sdl.ImGuiWindowFlags);
        if (ImGui::Button("Resume")) gs.currentView = GameScreen::Playing;
        if (ImGui::Button("Quit")) running = false;
        ImGui::End();
        break;
      case GameScreen::MultiPlayerOptionsMenu:
        // drawSettings();
        ImGui::Begin("MultiPlayer Menu", nullptr, sdl.ImGuiWindowFlags);
        if (ImGui::Button("Host",buttonSize)) {
          // todo
        }
        if (ImGui::Button("Client",buttonSize)) {
          // todo
        }
        if (ImGui::Button("Back to Menu",buttonSize)) {
          // todo; should reset game state unless saved
          gs.currentView = GameScreen::MainMenu;
        }
        ImGui::End();
        break;
      case GameScreen::Playing:
        ImGuiWindowFlags ImGuiWindowFlags =
          sdl.ImGuiWindowFlags | ImGuiWindowFlags_NoBackground;
          // ImGuiWindowFlags_NoSavedSettings;
          ImGui::Begin("HUD", nullptr, ImGuiWindowFlags);
          // Optional: remove padding so the button hugs the corner
          ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
          ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
          if (ImGui::Button("Back to Menu", buttonSize)) {
              gs.currentView = GameScreen::MainMenu;
          }
          ImGui::SameLine(0, 2.0f);
          if (ImGui::Button("Save Game", buttonSize)) {
            // TODO
          }
          ImGui::PopItemFlag();
          ImGui::PopStyleVar();
          ImGui::End();
        break;
    }

    // this->clearRendererBackbuffer()
    // clear the backbuffer before drawing onto it with black from draw color above
    SDL_SetRenderDrawColor(sdl.renderer, 20, 10, 30, 255);
    SDL_RenderClear(sdl.renderer);

    // updateGamePlayState()
    bool playing = (gs.currentView == GameScreen::Playing);
    if (playing) {
      // update & draw game world to sdl.renderer here (before ImGui::Render)
      // TODO make into helper: UpdateAllObjects()
      // update all objects;
      for (auto &layer : gs.layers) {
        for (GameObject &obj : layer) { // for each obj in layer
          // optimization to avoid n*m comparisions
          if (obj.dynamic) {
            this->updateGameObject(obj, deltaTime);
          }
        }
      }

      // update bullet physics
      for (GameObject &bullet : gs.bullets) {
        this->updateGameObject(bullet, deltaTime);
      }

      // TODO wrap all below in Render() function
      // calculate viewport position based on player updated position
      gs.mapViewport.x = (player.position.x + player.spritePixelW / 2) - gs.mapViewport.w / 2;

      // SDL_SetRenderDrawColor(sdl.renderer, 20, 10, 30, 255);

      // // clear the backbuffer before drawing onto it with black from draw color above
      // SDL_RenderClear(sdl.renderer);

      // Perform drawing commands:

      // draw background images
      SDL_RenderTexture(sdl.renderer, res.texBg1, nullptr, nullptr);
      this->drawParalaxBackground(res.texBg4, player.velocity.x, gs.bg4scroll, 0.075f, deltaTime);
      this->drawParalaxBackground(res.texBg3, player.velocity.x, gs.bg3scroll, 0.15f, deltaTime);
      this->drawParalaxBackground(res.texBg2, player.velocity.x, gs.bg2scroll, 0.3f, deltaTime);

      // draw all background objects
      for (auto &tile : gs.backgroundTiles) {
        SDL_FRect dst{
          .x = tile.position.x - gs.mapViewport.x,
          .y = tile.position.y,
          .w = static_cast<float>(tile.texture->w),
          .h = static_cast<float>(tile.texture->h),
        };
        SDL_RenderTexture(sdl.renderer, tile.texture, nullptr, &dst);
      }

      // draw all interactable objects
      for (auto &layer : gs.layers) {
        for (GameObject &obj : layer) {
          this->drawObject(obj, obj.spritePixelH, obj.spritePixelW, deltaTime);
        }
      }

      // draw bullets
      for (GameObject &bullet: gs.bullets) {
        if (bullet.data.bullet.state != BulletState::inactive) {
          this->drawObject(bullet, bullet.collider.h, bullet.collider.w, deltaTime);
        }
      }

      // draw all foreground objects
      for (auto &tile : gs.foregroundTiles) {
        SDL_FRect dst{
          .x = tile.position.x - gs.mapViewport.x,
          .y = tile.position.y,
          .w = static_cast<float>(tile.texture->w),
          .h = static_cast<float>(tile.texture->h),
        };
        SDL_RenderTexture(sdl.renderer, tile.texture, nullptr, &dst);
      }

      // debugging
      if (gs.debugMode) {
        SDL_SetRenderDrawColor(sdl.renderer, 255, 255, 255, 255);
        SDL_RenderDebugText(
            sdl.renderer,
            5,
            5,
            std::format("State3: {}  Direction: {} B: {}, G: {}", static_cast<int>(player.data.player.state), player.direction, gs.bullets.size(), player.grounded).c_str());
      }
    }

    // swap backbuffer to display new state
    // Textures live in GPU memory; the renderer batches copies/draws and flushes them on present.
    // 6) Render ImGui on top of your SDL frame
    // doRenderUpdates()
    SDL_SetRenderLogicalPresentation(sdl.renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdl.renderer);

    SDL_SetRenderLogicalPresentation(sdl.renderer, sdl.logW, sdl.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

    SDL_RenderPresent(sdl.renderer);

    prevTime = nowTime;
  };

};


void GameEngine::GameEngine::runEventLoop(GameObject &player) {
    // event loop
    SDL_Event event{0};
    while (SDL_PollEvent(&event)) {

      // 2) Give every event to ImGui first
      ImGui_ImplSDL3_ProcessEvent(&event);

      // always honor quit
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
        continue;
      }

      // 3) only handle game input if ImGui doesn't want it
      // ImGuiIO& io = ImGui::GetIO();
      // bool uiWantsKeyboard = io.WantCaptureKeyboard;
      // bool uiWantsMouse    = io.WantCaptureMouse;

      // if (!uiWantsKeyboard || !uiWantsMouse) {
      // TODO abstract this to game.handleGameInput(event);
      switch (event.type) {
        case SDL_EVENT_QUIT:
        {
          running = false;
          break;
        }
        case SDL_EVENT_WINDOW_RESIZED:
        {
          this->setWindowSize(event.window.data2, event.window.data1);
          break;
        }
        case SDL_EVENT_KEY_DOWN: // non-continuous presses
        {
          this->handleKeyInput(player, event.key.scancode, true);
          break;
        }
        case SDL_EVENT_KEY_UP:
        {
          this->handleKeyInput(player, event.key.scancode, false);
          if (event.key.scancode == SDL_SCANCODE_Q) {
            gs.debugMode = !gs.debugMode;
          }
          break;
        }
      }
    }
    // }
}

bool GameEngine::GameEngine::initWindowAndRenderer(int width, int height, int logW, int logH) {

  state.width = width;
  state.height = height;
  state.logW = logW;
  state.logH = logH;

  if (!SDL_Init(SDL_INIT_VIDEO)) { // later to add audio we'll also need SDL_INIT_AUDIO
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL Init Error", "Failed to initialize SDL.", nullptr);
    return false;
  };

  // SDL_CreateWindow("SDL Game Engine",width, height, fullscreen ? SDL_WINDOW_FULLSCREEN : 0),
  // SDL_CreateRenderer(sdlstate.window, nullptr)

  if (!SDL_CreateWindowAndRenderer("SDL Game Engine", state.width, state.height, SDL_WINDOW_RESIZABLE, &state.window, &state.renderer)) {

    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "SDL init error", SDL_GetError(), nullptr);

    this->cleanup();
    return false;
  }
  SDL_SetRenderVSync(state.renderer, 1);

  // configure presentation
  SDL_SetRenderLogicalPresentation(state.renderer, state.logW , state.logH, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  // Setup ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  (void)io;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // optional

  // Setup ImGui style
  ImGui::StyleColorsDark();

  // Setup ImGui platform/renderer backends
  ImGui_ImplSDL3_InitForSDLRenderer(state.window, state.renderer);
  ImGui_ImplSDLRenderer3_Init(state.renderer);

  return true;

}

void GameEngine::GameEngine::cleanupTextures() {
  this->res.unload();
}

void GameEngine::GameEngine::cleanup() {
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(state.renderer); // destroy renderer before window
  SDL_DestroyWindow(state.window);
  SDL_Quit();
}

void GameEngine::GameEngine::drawObject(GameObject &obj, float height, float width, float deltaTime) {

    // pull out specific sprite frame from sprite sheet
    float srcX = obj.currentAnimation != -1 ?
      obj.animations[obj.currentAnimation].currentFrame() * width
      : (obj.spriteFrame -1)*width;

    SDL_FRect src = {
      .x = srcX, // different starting x position in sprite sheet
      .y = 0,
      .w = width,
      .h = height
    };

    SDL_FRect dst = {
      .x = obj.position.x - gs.mapViewport.x, // move objects according to updated viewport position
      .y = obj.position.y,
      .w = width,
      .h = height,
    };

    SDL_FlipMode flipMode = obj.direction == -1 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    // set flash animations on enemies
    if (obj.shouldFlash) {
      SDL_SetTextureColorModFloat(obj.texture, 2.5f, 1.0f, 1.0f);
    }
    SDL_RenderTextureRotated(state.renderer, obj.texture, &src, &dst, 0, nullptr, flipMode);
    if (obj.shouldFlash) {
      SDL_SetTextureColorModFloat(obj.texture, 1.0f, 1.0f, 1.0f);
      if (obj.flashTimer.step(deltaTime)) {
        obj.shouldFlash = false;
      }
    }

    if (gs.debugMode) {
      // display each objects collision hitbox
      SDL_FRect rectA{
        .x = obj.position.x + obj.collider.x - gs.mapViewport.x,
        .y = obj.position.y + obj.collider.y,
        .w = obj.collider.w,
        .h = obj.collider.h
      };
      SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(state.renderer, 255, 0, 0, 100);
      SDL_RenderFillRect(state.renderer, &rectA);
      SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);

      SDL_FRect sensor{
        .x = obj.position.x + obj.collider.x - gs.mapViewport.x,
        .y = obj.position.y + obj.collider.y + obj.collider.h,
        .w = obj.collider.w, .h = 1
      };
      SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(state.renderer, 0, 0, 255, 255);
      SDL_RenderFillRect(state.renderer, &sensor);
      SDL_SetRenderDrawBlendMode(state.renderer, SDL_BLENDMODE_NONE);
    }
}

// update updates the state of the passed in game object every render loop
void GameEngine::GameEngine::updateGameObject(GameObject &obj, float deltaTime) {

  if (obj.currentAnimation != -1) {
    obj.animations[obj.currentAnimation].step(deltaTime);
  }

  // gravity applied globally; downward y force when not grounded
  if (obj.dynamic && !obj.grounded) {
    // if (obj.type == ObjectType::enemy) {
    //     std::cout << "grounded" << obj.grounded << std::endl;
    // }
    // increase downward velocity = acc*deltaTime every frame
    obj.velocity += GRAVITY * deltaTime;
  }

  float currDirection = 0;
  if (obj.type == ObjectType::player) {

    // this way player cant spam jump and fly; but sometimes gets stuck and is unable to jump
    // if (state.keys[SDL_SCANCODE_DOWN]) {
    //   handleKeyInput(obj, SDL_SCANCODE_DOWN, false);
    // }
    // if (state.keys[SDL_SCANCODE_UP]) {
    //   handleKeyInput(obj, SDL_SCANCODE_UP, true);
    // }
    // keep previous key state somewhere (per frame)
    // bool upPressed = state.keys[SDL_SCANCODE_UP];
    // if (upPressed && !prevUpPressed) {
    //     handleKeyInput(obj, SDL_SCANCODE_UP, true);  // key just went down
    // }
    // if (!upPressed && prevUpPressed) {
    //     handleKeyInput(obj, SDL_SCANCODE_UP, false); // key just went up (if you care)
    // }
    // prevUpPressed = upPressed;

    // update direction
    if (state.keys[SDL_SCANCODE_LEFT]) {
      currDirection += -1;
    }
    if (state.keys[SDL_SCANCODE_RIGHT]) {
      currDirection += 1;
    }

    Timer &weaponTimer = obj.data.player.weaponTimer;
    weaponTimer.step(deltaTime);

    const auto handleShooting = [this, &obj, &weaponTimer, &currDirection](
      SDL_Texture *tex, SDL_Texture *shootTex, int animIndex, int shootAnimIndex){
    // TODO use similar condition to prevent double jump
      if (state.keys[SDL_SCANCODE_A]) {

        // set player texture during shooting anims
        obj.texture = shootTex;
        obj.currentAnimation = shootAnimIndex;
        if (weaponTimer.isTimedOut()) {
          weaponTimer.reset();
          // create bullets
          GameObject bullet(4, 4);
          bullet.data.bullet = BulletData();
          bullet.type = ObjectType::bullet;
          bullet.direction = obj.direction;
          bullet.texture = res.texBullet;
          bullet.currentAnimation = res.ANIM_BULLET_MOVING;
          bullet.collider = SDL_FRect{
            .x = 0, .y = 0,
            .w = static_cast<float>(res.texBullet->h),
            .h = static_cast<float>(res.texBullet->h),
          };
          const int yJitter = 50;
          const float yVelocity = SDL_rand(yJitter) - yJitter / 2.0f;
          bullet.velocity = glm::vec2(
            obj.velocity.x + 600.0f,
            yVelocity
          ) * obj.direction;
          bullet.maxSpeedX = 1000.0f;
          bullet.animations = res.bulletAnims;

          // adjust depending on direction faced; lerp
          const float left = 4;
          const float right = 24;
          const float t = (obj.direction + 1) / 2.0f; // 0 or 1 taking into account neg sign
          const float xOffset = left + right * t;
          bullet.position = glm::vec2(
            obj.position.x + xOffset,
            obj.position.y + obj.spritePixelH / 2 + 1
          );

          bool foundInactive = false;
          for (int i = 0; i < gs.bullets.size() && !foundInactive; i++) {
            if (gs.bullets[i].data.bullet.state == BulletState::inactive) {
              foundInactive = true;
              gs.bullets[i] = bullet;
            }
          }

          // only add new if no inactive found
          if (!foundInactive) {
            this->gs.bullets.push_back(bullet); // push bullets so we can draw them
          }
        }
      } else {
          obj.texture = tex;
          obj.currentAnimation = animIndex;
      }
    };

    // update animation state
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        if (currDirection != 0) {
          obj.data.player.state = PlayerState::running;
          obj.texture = res.texRun;
          obj.currentAnimation = res.ANIM_PLAYER_RUN;
        } else {
          // decelerate faster than we speed up
          if (obj.velocity.x) {
            const float factor = obj.velocity.x > 0 ? -1.5f : 1.5f;
            float amount = factor * obj.acceleration.x * deltaTime;
            if (std::abs(obj.velocity.x) < std::abs(amount)) {
              obj.velocity.x = 0;
            } else {
              obj.velocity.x += amount;
            }
          }
        }

        handleShooting(res.texIdle, res.texShoot, res.ANIM_PLAYER_IDLE, res.ANIM_PLAYER_SHOOT);

        break;
      }
      case PlayerState::running:
      {
        if (currDirection == 0) {
          obj.data.player.state = PlayerState::idle;
        }

        // move in opposite dir of velocity, sliding
        if (obj.velocity.x * obj.direction < 0 && obj.grounded) {
          handleShooting(res.texSlide, res.texSlideShoot, res.ANIM_PLAYER_SLIDE, res.ANIM_PLAYER_SLIDE_SHOOT);
        } else {
          handleShooting(res.texRun, res.texRunShoot, res.ANIM_PLAYER_RUN, res.ANIM_PLAYER_RUN);
          // sprite sheets have same frames so we can seamlessly swap between the two sheets
        }

        break;
      }
      case PlayerState::jumping:
      {
        handleShooting(res.texRun, res.texRunShoot, res.ANIM_PLAYER_RUN, res.ANIM_PLAYER_RUN);
        // obj.texture = res.texRun;
        // obj.currentAnimation = res.ANIM_PLAYER_RUN;
        break;
      }
    }
  }
  else if (obj.type == ObjectType::bullet) {

    switch (obj.data.bullet.state) {
      case BulletState::moving:
      {
        if (obj.position.x - gs.mapViewport.x < 0 || obj.position.x - gs.mapViewport.x > state.logW ||
        obj.position.y - gs.mapViewport.y < 0 ||
        obj.position.y - gs.mapViewport.y > state.logH) {
        obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
      case BulletState::colliding:
      {
        if (obj.animations[obj.currentAnimation].isDone()) {
          obj.data.bullet.state = BulletState::inactive;
        }
        break;
      }
    }
  }
  else if (obj.type == ObjectType::enemy) {
    switch (obj.data.enemy.state) {
      case EnemyState::idle:
      {
        glm::vec2 distToPlayer = this->getPlayer().position - obj.position;
        if (glm::length(distToPlayer) < 100) {
          // face the enemy towards the player
          currDirection = 1;
          if (distToPlayer.x < 0) {
            currDirection = -1;
          };
          obj.acceleration = glm::vec2(30, 0);
        } else {
          // stop them from moving when too far away
          obj.acceleration = glm::vec2(0);
          obj.velocity.x = 0;
        }

        break;
      }
      case EnemyState::dying:
      {
        if (obj.data.enemy.damageTimer.step(deltaTime)) {
          obj.data.enemy.state = EnemyState::idle;
          obj.texture = res.texEnemy;
          obj.currentAnimation = res.ANIM_ENEMY;
          obj.data.enemy.damageTimer.reset();
        }
        break;
      }
      case EnemyState::dead:
      {
        obj.velocity.x = 0;
        if (obj.currentAnimation != -1 && obj.animations[obj.currentAnimation].isDone()) {
          // stop animations for dead enemy
          obj.currentAnimation = -1;
          obj.spriteFrame = 18; // TODO this is because enemy has 18 frames
        }
        break;
      }
    }
  }

  if (currDirection) {
    obj.direction = currDirection;
  }
  // update velocity based on currDirection (which way we're facing),
  // acceleration and deltaTime
  obj.velocity += currDirection * obj.acceleration * deltaTime;
  if (std::abs(obj.velocity.x) > obj.maxSpeedX) { // cap the max velocity
    obj.velocity.x = currDirection * obj.maxSpeedX;
  }
  // update position based on velocity
  obj.position += obj.velocity * deltaTime;

  // handle collision detection
  bool foundGround = false;
  for (auto &layer : gs.layers) {
    for (GameObject &objB: layer){
      // if (obj.type == ObjectType::enemy) {
      //   std::cout << "found Ground" << foundGround << std::endl;
      // }
      if (&obj != &objB && objB.collider.h != 0 && objB.collider.w != 0) {
        this->handleCollision(obj, objB, deltaTime);

        // update ground sensor only when landing on level tiles
        if (objB.type == ObjectType::level) {
          SDL_FRect sensor{
            .x = obj.position.x + obj.collider.x,
            .y = obj.position.y + obj.collider.y + obj.collider.h,
            .w = obj.collider.w, .h = 1
          };

          SDL_FRect rectB{
            .x = objB.position.x + objB.collider.x,
            .y = objB.position.y + objB.collider.y,
            .w = objB.collider.w, .h = objB.collider.h
          };

          SDL_FRect dummyRectC{0};

          if (SDL_GetRectIntersectionFloat(&sensor, &rectB, &dummyRectC)) {
            foundGround = true;
          }
        }

      }
    }
  }

  if (obj.grounded != foundGround) {
    // switching grounded state
    obj.grounded = foundGround;
    if (foundGround && obj.type == ObjectType::player) {
      obj.data.player.state = PlayerState::running;
    }
  }
}

/*
 collisionResponse will dictates what you want to happen given a collision has been detected
 The defaultResponse handles vertical and horizontal collisions by preventing sprites from overlapping due to collision
*/
void GameEngine::GameEngine::collisionResponse(const SDL_FRect &rectA, const SDL_FRect &rectB, const SDL_FRect &rectC, GameObject &objA, GameObject &objB, float deltaTime) {

  // logRectEvery(rectC, 100000);
  const auto defaultResponse = [&]() {
    if (rectC.w < rectC.h) {
      // horizontal collision
      if (objA.velocity.x > 0) {
        // traveling to right, colliding object must be to the right, so sub .w; need extra 0.1 to escape collision for next frame
        objA.position.x -= rectC.w+0.1;
      } else if (objA.velocity.x < 0) {
        objA.position.x += rectC.w+0.1;
      }
      objA.velocity.x = 0; // reset velocity to 0 so object stops
    } else {
      //vertical collison
      if (objA.velocity.y > 0) {
        objA.position.y -= rectC.h; // down
      } else if (objA.velocity.y < 0) {
        objA.position.y += rectC.h; // up
      }
      objA.velocity.y = 0;
    }
  };

  if (objA.type == ObjectType::player) {
    switch (objB.type) {
      case ObjectType::level:
      {
        defaultResponse();
        break;
      }
      case ObjectType::enemy:
      {
        if (objB.data.enemy.state != EnemyState::dead) {
          objA.velocity = glm::vec2(50, 0) * - objA.direction;
        }
        break;
      }
      case ObjectType::player:
      {
        break;
      }
    }
  } else if (objA.type == ObjectType::bullet) {

    bool passthrough = false;
    switch (objA.data.bullet.state) {
      case BulletState::moving:
      {
        switch (objB.type) {
          case ObjectType::level:
          {
            break;
          }
          case ObjectType::enemy:
          {
            EnemyData &d = objB.data.enemy;
            if (d.state != EnemyState::dead) {
              objB.direction = -1 * objA.direction;
              objB.shouldFlash = true;
              objB.flashTimer.reset();
              objB.texture = res.texEnemyHit;
              objB.currentAnimation = res.ANIM_ENEMY_HIT;
              d.state = EnemyState::dying;

              // damage and flag dead
              d.healthPoints -= 10;
              if (d.healthPoints <= 0) {
                d.state = EnemyState::dead;
                objB.texture = res.texEnemyDie;
                objB.currentAnimation = res.ANIM_ENEMY_DIE;
              }
            } else {
              passthrough = true;
            }
            break;
          }
        }

        if (!passthrough) {
          defaultResponse();
          objA.velocity *= 0;
          objA.data.bullet.state = BulletState::colliding;
          objA.texture = res.texBulletHit;
          objA.currentAnimation = res.ANIM_BULLET_HIT;
        }
        break;
      }
    }

  }
  else if (objA.type == ObjectType::enemy) {
    defaultResponse(); // ensure enemy doesnt fall through floor
  }

}

void GameEngine::GameEngine::handleCollision(GameObject &a, GameObject &b, float deltaTime) {

  SDL_FRect rectA{
    .x = a.position.x + a.collider.x,
    .y = a.position.y + a.collider.y,
    .w = a.collider.w,
    .h = a.collider.h
  };
  SDL_FRect rectB{
    .x = b.position.x + b.collider.x,
    .y = b.position.y + b.collider.y,
    .w = b.collider.w,
    .h = b.collider.h
  };
  SDL_FRect rectC{ 0 };

  if (SDL_GetRectIntersectionFloat(&rectA, &rectB, &rectC)) {
    // found interection
    this->collisionResponse(rectA, rectB, rectC, a, b, deltaTime);
  };
};

bool GameEngine::GameEngine::initAllTiles() {

  /*
    1 - Ground
    2 - Panel
    3 - Enemy
    4 - Player
    5 - Grass
    6 - Brick
  */

  short map[MAP_ROWS][MAP_COLS] = {
    0, 0, 0, 0, 4, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 3, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 0, 0, 0, 0, 3,2, 2, 2, 2, 2,2, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 2, 2, 0, 0, 3, 2, 2,2, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1, 1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,1, 1, 1, 1, 1,
  };

  short foregroundMap[MAP_ROWS][MAP_COLS] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    5, 5, 5, 0, 0, 5, 5, 5, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
  };

  short backgroundMap[MAP_ROWS][MAP_COLS] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 6, 6, 6, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,0, 0, 0, 0, 0,
  };

  // short maplayer[MAP_ROWS][MAP_COLS] really means short (*maplayer)[MAP_COLS]: a pointer to the first row (each row is an array of MAP_COLS shorts)
  // const short (&maplayer)[MAP_ROWS][MAP_COLS] if want by reference
  const auto loadMap = [this](short maplayer[MAP_ROWS][MAP_COLS]) {
    const auto createObject = [this](int r, int c, SDL_Texture *tex, ObjectType type, float spriteH, float spriteW) {
      GameObject o(spriteH, spriteW);
      o.type = type;
      o.position = glm::vec2(c * TILE_SIZE, state.logH - (MAP_ROWS - r) * TILE_SIZE);
      o.texture = tex;
      o.collider = {
        .x = 0,
        .y = 0,
        .w = TILE_SIZE,
        .h = TILE_SIZE
      };
      return o;
    };

    for (int r = 0; r < MAP_ROWS; r++) {
      for (int c = 0; c < MAP_COLS; c++) {
        switch (maplayer[r][c]) {
          case 1: // Ground
          {
            GameObject ground = createObject(r, c, res.texGround, ObjectType::level, TILE_SIZE, TILE_SIZE);
            ground.data.level = LevelData();
            gs.layers[LAYER_IDX_LEVEL].push_back(ground); // we do this so we can update each object and destroy the objects with easy access
            break;
          }
          case 2: // Panel
          {
            GameObject panel = createObject(r, c, res.texPanel, ObjectType::level, TILE_SIZE, TILE_SIZE);
            panel.data.level = LevelData();
            gs.layers[LAYER_IDX_LEVEL].push_back(panel);
            break;
          }
          case 3: // Enemy
          {
            GameObject enemy = createObject(r, c, res.texEnemy, ObjectType::enemy, TILE_SIZE, TILE_SIZE);
            enemy.data.enemy = EnemyData();
            enemy.currentAnimation = res.ANIM_ENEMY;
            enemy.animations = res.enemyAnims;
            enemy.dynamic = true;
            enemy.maxSpeedX = 15;
            // enemy.collider = {
            //   .x = 11,
            //   .y = 6,
            //   .w = 10,
            //   .h = 26
            // };
            gs.layers[LAYER_IDX_CHARACTERS].push_back(enemy);
            break;
          }
          case 4: // player
          {
            GameObject player = createObject(r, c, res.texIdle, ObjectType::player, 32, 32); // TODO update with new dimensions
            player.data.player = PlayerData();
            player.animations = res.playerAnims; // copies via std::vector copy assignment
            player.currentAnimation = res.ANIM_PLAYER_IDLE;
            player.acceleration = glm::vec2(300, 0);
            player.maxSpeedX = 100;
            player.dynamic = true;
            player.collider = {
              .x = 11,
              .y = 6,
              .w = 10,
              .h = 26
            };
            gs.layers[LAYER_IDX_CHARACTERS].push_back(player);
            gs.playerIndex = gs.layers[LAYER_IDX_CHARACTERS].size() - 1;
            break;
          }
          case 5:
          {
            GameObject grass = createObject(r, c, res.texGrass, ObjectType::level, TILE_SIZE, TILE_SIZE);
            grass.data.level = LevelData();
            // gs.layers[LAYER_IDX_LEVEL].push_back(grass);
            gs.foregroundTiles.push_back(grass);
            break;
          }
          case 6:
          {
            GameObject brick = createObject(r, c, res.texBrick, ObjectType::level, TILE_SIZE, TILE_SIZE);
            brick.data.level = LevelData();
            // gs.layers[LAYER_IDX_LEVEL].push_back(brick);
            gs.backgroundTiles.push_back(brick);
            break;
          }
        }
      }
    }
  };

  loadMap(map);
  loadMap(backgroundMap);
  loadMap(foregroundMap);

  // assert(gs.playerIndex != -1); // player index must be set
  return gs.playerIndex != -1;
};

void GameEngine::GameEngine::handleKeyInput(GameObject &obj, SDL_Scancode key, bool keyDown) {

  if (obj.type == ObjectType::player) {
    switch (obj.data.player.state) {
      case PlayerState::idle:
      {
        if (key == SDL_SCANCODE_UP && keyDown) {
          obj.velocity.y += JUMP_FORCE;
          obj.data.player.state = PlayerState::jumping;
        }
        break;
      }
      case PlayerState::jumping:
      {
        if (!keyDown) {
          obj.velocity.y = 0;
          obj.data.player.state = PlayerState::idle;
        }
        break;
      }
      case PlayerState::running:
      {
        if (key == SDL_SCANCODE_UP && keyDown) {
          obj.velocity.y += JUMP_FORCE;
          obj.data.player.state = PlayerState::jumping;
        }
        break;
      }
    }

  }



};

void GameEngine::GameEngine::drawParalaxBackground(SDL_Texture *texture, float xVelocity, float &scrollPos, float scrollFactor, float deltaTime) {
  scrollPos -= xVelocity * scrollFactor * deltaTime; // scroll position passed by reference, is updated every loop
  if (scrollPos <= -texture->w) {
    scrollPos = 0;
  }

  SDL_FRect dst{
    .x = scrollPos,
    .y = 40,
    .w = texture->w * 2.0f,
    .h = static_cast<float>(texture->h)
  };

  SDL_RenderTextureTiled(state.renderer, texture, nullptr, 1, &dst);
};