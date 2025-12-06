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

add_executable(game_engine main.cpp app.cpp gameengine.cpp)
target_link_libraries(game_engine PRIVATE SDL3::SDL3 SDL3_image::SDL3_image glm::glm SDL3_mixer::SDL3_mixer) # add sdl3 and sdl3_image to the compiler header link paths

# ImGui: core + SDL3/SDL_Renderer3 backends
set(IMGUI_DIR ${IMGUI_DIR})
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
target_link_libraries(imgui PUBLIC SDL3::SDL3)  # backend needs SDL headers
target_link_libraries(game_engine
  PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_mixer::SDL3_mixer glm::glm imgui
)

target_compile_features(game_engine PRIVATE cxx_std_23)