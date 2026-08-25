# MSVC_RUNTIME_LIBRARY のサポートが 3.15 から
cmake_minimum_required(VERSION 3.15.0)

if(MSVC)
  add_compile_options(
    /utf-8 /wd4819 /wd4996 /wd4244 /wd4018 /wd4267 /MP)
endif()

project(movie_player_test)

if (!movieplayer)
add_subdirectory(../../ movieplayer)
endif()

find_package(GLEW REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# ------------------------------------------------------------------------------
# movie_player_test
# ------------------------------------------------------------------------------

add_executable(movie_player_test GLRenderer.cpp movie_player_test.cpp)
target_link_libraries(movie_player_test PRIVATE 
  movieplayer
  GLEW::GLEW
  glfw
)

# ------------------------------------------------------------------------------
# movie_exporter
# ------------------------------------------------------------------------------

add_executable(movie_exporter movie_exporter.cpp)
target_link_libraries(movie_exporter PRIVATE 
  movieplayer
)
