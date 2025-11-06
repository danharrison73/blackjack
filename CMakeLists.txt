cmake_minimum_required(VERSION 3.20)
project(blackjack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# CLI11 (argument parsing / command model)
FetchContent_Declare(
  cli11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG v2.4.2
)
FetchContent_MakeAvailable(cli11)

# FTXUI (rich TUI / rendering / live updates)
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG v5.0.0
)
FetchContent_MakeAvailable(ftxui)

add_executable(blackjack src/main.cpp)

target_link_libraries(blackjack
  PRIVATE
    CLI11::CLI11
    ftxui::component   # component includes dom + screen
    ftxui::dom
    ftxui::screen
)

# Faster builds on GCC/Clang (optional)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(blackjack PRIVATE -O2 -march=native -Wall -Wextra -Wpedantic)
endif()

