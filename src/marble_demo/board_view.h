#pragma once
#include <M5Unified.h>
#include "marble.h"

// Flicker-free renderer: draws the whole frame into one off-screen M5Canvas and
// pushes it in a single shot (repo convention). Holds no game state. `best` is
// the best score this power-cycle, shown on the game-over overlay.
class BoardView {
 public:
  void begin();  // create the full-screen sprite
  void render(const marble::GameState& s, const marble::Config& cfg, int best);
 private:
  M5Canvas canvas_{&M5.Display};
};
