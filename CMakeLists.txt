cmake_minimum_required(VERSION 3.10)

project(game_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(SDL3 REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

add_executable(game_engine main.cpp)
target_link_libraries(game_engine PRIVATE SDL3::SDL3 SDL3_image::SDL3_image glm::glm) # add sdl3 and sdl3_image to the compiler header link paths
target_compile_features(game_engine PRIVATE cxx_std_23)