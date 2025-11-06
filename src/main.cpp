#include <chrono>
#include <thread>
#include <string>
#include <atomic>

#include <CLI/CLI.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include "blackjack.hpp"

using namespace ftxui;

int run_blackjack(){
     return 0;
}


int main(int argc, char** argv) {
  CLI::App app{"Spectre-like Console CLI in C++ (CLI11 + FTXUI)"};
  app.require_subcommand(1);

  // blackjack subcommand
  auto* blackjack = app.add_subcommand("blackjack", "Play Blackjack");
  blackjack->callback([&] {run_blackjack(); });

  CLI11_PARSE(app, argc, argv);
  return 0;
}
