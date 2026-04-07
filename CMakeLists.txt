cmake_minimum_required(VERSION 3.20)

project(game_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_mixer REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

# link asio as vendor
set(ASIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/asio)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${ASIO_DIR})
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
find_package(Threads REQUIRED)
target_link_libraries(asio INTERFACE Threads::Threads)

# ImGui: core + SDL3/SDL_Renderer3 backends
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)
set(IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
  ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

set(ENGINE_SOURCES
  engine/src/engine.cpp
  engine/src/ui_manager.cpp
  engine/src/game_server.cpp
  engine/src/tmx.cpp
  vendor/tinyxml2/tinyxml2.cpp
)

add_library(engine STATIC ${ENGINE_SOURCES})
target_include_directories(engine
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include/engine/net
    ${CMAKE_CURRENT_SOURCE_DIR}/game/include
    ${CMAKE_CURRENT_SOURCE_DIR}/game/include/game
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyxml2
)

target_link_libraries(engine
  PUBLIC
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_mixer::SDL3_mixer
    SDL3_ttf::SDL3_ttf
    glm::glm
    imgui
    asio
)

target_compile_features(engine PUBLIC cxx_std_23)

add_executable(game
  game/src/main.cpp
  game/src/app.cpp
  game/src/game_resources.cpp
  game/src/game_rules.cpp
  game/src/ui_controller.cpp
  game/src/default_bootstrap.cpp
  game/src/default_input_system.cpp
  game/src/default_ui_flow.cpp
  game/src/default_simulation_system.cpp
  game/src/default_render_system.cpp
  game/src/level_manifest.cpp
)

target_include_directories(game
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/game/include
    ${CMAKE_CURRENT_SOURCE_DIR}/game/include/game
)

target_link_libraries(game PRIVATE engine)
target_compile_features(game PRIVATE cxx_std_23)

add_executable(net_common_tests
  tests/net_common_tests.cpp
)

target_include_directories(net_common_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(net_common_tests PRIVATE engine)

if(APPLE)
  set_target_properties(game PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Game"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.bishalgautam.game"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacBundle.plist.in"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  )

  add_custom_command(TARGET game POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E remove_directory "$<TARGET_BUNDLE_DIR:game>/Contents/Resources/data"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_BUNDLE_DIR:game>/Contents/Resources"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
      "${CMAKE_CURRENT_SOURCE_DIR}/data"
      "$<TARGET_BUNDLE_DIR:game>/Contents/Resources/data"
    COMMENT "Copying game assets into app bundle Resources/data"
    VERBATIM
  )

  set(BUNDLE_FIXUP_SEARCH_DIRS
    "$ENV{HOME}/SDL3-3.3/lib"
    "$ENV{HOME}/SDL3-image-3.3/lib"
    "$ENV{HOME}/SDL3-mixer-3.1.0/lib"
    "$ENV{HOME}/SDL3_ttf-custom/lib"
    "/opt/homebrew/lib"
  )

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/FixupBundle.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/FixupBundle.cmake"
    @ONLY
  )

  add_custom_target(bundle_game
    COMMAND "${CMAKE_COMMAND}"
      -DAPP_BUNDLE=$<TARGET_BUNDLE_DIR:game>
      -P "${CMAKE_CURRENT_BINARY_DIR}/FixupBundle.cmake"
    DEPENDS game
    COMMENT "Bundling dependent dylibs into game.app/Contents/Frameworks"
    VERBATIM
  )

  install(TARGETS game BUNDLE DESTINATION .)
  install(DIRECTORY data/ DESTINATION "game.app/Contents/Resources/data")
endif()
