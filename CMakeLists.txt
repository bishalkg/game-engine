cmake_minimum_required(VERSION 3.10)

project(game_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# ensure doesnt use homebrew binary
# set(CMAKE_BUILD_RPATH   "$ENV{HOME}/SDL3-3.3/lib;$ENV{HOME}/SDL3-mixer-3.1.0/lib;$ENV{HOME}/SDL3-image-3.3/lib")
# set(CMAKE_INSTALL_RPATH "$ENV{HOME}/SDL3-3.3/lib;$ENV{HOME}/SDL3-mixer-3.1.0/lib;$ENV{HOME}/SDL3-image-3.3/lib")

find_package(SDL3 REQUIRED CONFIG) # config means look for the config.make file in the CMAKE_PREFIX_PATH provided
find_package(SDL3_mixer REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

# link asio as vendor
set(ASIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/asio)
add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${ASIO_DIR})
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
find_package(Threads REQUIRED)
target_link_libraries(asio INTERFACE Threads::Threads)

add_executable(game_engine
  main.cpp
  app.cpp
  gameengine.cpp
  ui_manager.cpp
  game_server.cpp
  tmx.cpp
  vendor/tinyxml2/tinyxml2.cpp
)
# target_link_libraries(game_engine PRIVATE SDL3::SDL3 SDL3_image::SDL3_image glm::glm SDL3_mixer::SDL3_mixer) # add sdl3 and sdl3_image to the compiler header link paths

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
target_include_directories(game_engine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyxml2)

target_link_libraries(imgui PUBLIC SDL3::SDL3)  # backend needs SDL headers
target_link_libraries(game_engine
  PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer glm::glm imgui asio
)

add_executable(net_common_tests
  tests/net_common_tests.cpp
)
target_include_directories(net_common_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(net_common_tests
  PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer glm::glm asio
)


target_compile_features(game_engine PRIVATE cxx_std_23)
