#pragma once
#include <cstdint>

// 8-bit chiptune sound for the marble game: one-shot SFX plus looping background
// music, all built from square-wave M5.Speaker.tone() beeps. Fully non-blocking
// -- call update() every loop. Music runs on one speaker channel and SFX on
// another, so SFX mix over the music instead of cutting it. Device-only
// (M5.Speaker / ES8311); tuned and verified by ear, not host tests.
class Sound {
 public:
  struct Note { uint16_t freq; uint16_t ms; };  // freq 0 == rest
  enum class Track : uint8_t { None, Menu, Game };

  void begin();                         // init the speaker + per-channel volumes
  void update(uint32_t now);            // advance music + SFX; call every loop
  void setMusic(Track t, uint32_t now); // switch the looping background track

  void sfxStart(uint32_t now);          // game start
  void sfxEat(uint32_t now);            // collected a dot
  void sfxFall(uint32_t now);           // fell into a hole
  void sfxGameOver(uint32_t now);       // time up
  void sfxBounce();                     // bounced off a wall (one short tick)

 private:
  // A non-blocking note sequence player: one tone at a time on a channel,
  // advancing by wall-clock time so it never stalls the game loop.
  struct Seq {
    const Note* notes = nullptr;
    int      count     = 0;
    int      idx       = 0;
    bool     loop      = false;
    bool     active    = false;
    uint32_t noteStart = 0;
    uint16_t curMs     = 0;
  };
  void start(Seq& s, const Note* notes, int count, bool loop, uint32_t now);
  void advance(Seq& s, int channel, uint32_t now);

  Seq music_;
  Seq sfx_;
};
