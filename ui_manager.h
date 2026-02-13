#pragma once



namespace UIManager {

    enum class GameView {
      Playing,
      MainMenu,
      PauseMenu,
      LevelLoading,
      InventoryMenu,
      GameOver,
      MultiPlayerOptionsMenu, // this menu will show host or client buttons
  };

  class UI_Manager {




    UI_Manager() {};

    ~UI_Manager() = default;

    public:
    void beginFrame();

    void setScreenSize(int w, int h);

    // void showGameView(GameView gv);
    bool updateMenuRenderState(GameView view);







  };


}