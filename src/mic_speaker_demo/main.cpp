// Mic + speaker demo for the StickS3: live VU meter, push-to-talk record →
// playback, and a button tone -- exercising the shared ES8311 codec.
//
// All logic lives in App; this file just wires the Arduino entry points to it.
//   app.h      -- the state machine
//   audio_io.h -- mic/speaker hardware + codec toggle + record buffer
//   vu_view.h  -- renders a Frame into the sprite
//   lib/audio  -- pure, host-tested level math (computeLevel / dbToBar)
#include "app.h"

App app;

void setup() { app.begin(); }
void loop()  { app.loop(); }
