#include "sound.h"
#include <M5Unified.h>

namespace {
constexpr int MUSIC_CH  = 0;  // looping background track
constexpr int SFX_CH    = 1;  // one-shot effects, mixed over the music
constexpr int BOUNCE_CH = 2;  // wall-bounce ticks (own channel: frequent, must not
                              // stomp the eat/fall/game-over effects)

using Note = Sound::Note;

// --- one-shot SFX (freq Hz, ms; freq 0 = rest) ---
const Note kStart[]    = {{523, 70}, {659, 70}, {784, 70}, {1047, 160}};      // rising "ready!"
const Note kEat[]      = {{988, 45}, {1319, 90}};                             // coin blip
const Note kFall[]     = {{784, 55}, {587, 55}, {440, 70}, {330, 130}};       // descending "whoops"
const Note kGameOver[] = {{523, 160}, {392, 160}, {330, 160}, {262, 440}};    // game-over jingle

// --- looping background music ---
// Menu: calm major arpeggios.
const Note kMenu[] = {
    {523, 190}, {659, 190}, {784, 190}, {659, 190},
    {440, 190}, {523, 190}, {659, 190}, {523, 190},
    {349, 190}, {440, 190}, {523, 190}, {440, 190},
    {392, 190}, {494, 190}, {587, 190}, {784, 190},
};
// Game: faster, driving loop with a couple of rests for groove.
const Note kGame[] = {
    {523, 130}, {523, 130}, {784, 130}, {0, 130},
    {659, 130}, {523, 130}, {587, 130}, {0, 130},
    {440, 130}, {523, 130}, {659, 130}, {784, 130},
    {698, 130}, {587, 130}, {523, 130}, {0, 130},
};

template <typename T, size_t N>
constexpr int countOf(const T (&)[N]) { return (int)N; }
}  // namespace

void Sound::begin() {
  M5.Speaker.begin();
  M5.Speaker.setVolume(200);                  // master 0..255
  M5.Speaker.setChannelVolume(MUSIC_CH, 180); // louder background music
  M5.Speaker.setChannelVolume(SFX_CH, 230);
  M5.Speaker.setChannelVolume(BOUNCE_CH, 255);
}

void Sound::start(Seq& s, const Note* notes, int count, bool loop, uint32_t now) {
  s.notes = notes;
  s.count = count;
  s.idx = 0;
  s.loop = loop;
  s.active = true;
  s.noteStart = now;
  s.curMs = 0;  // 0 => the first note fires on the next advance()
}

void Sound::advance(Seq& s, int channel, uint32_t now) {
  if (!s.active) return;
  if (now - s.noteStart < s.curMs) return;  // current note still sounding
  if (s.idx >= s.count) {
    if (!s.loop) { s.active = false; return; }
    s.idx = 0;
  }
  const Note& n = s.notes[s.idx++];
  if (n.freq) M5.Speaker.tone((float)n.freq, n.ms, channel, true);  // rest (freq 0) = silence
  s.noteStart = now;
  s.curMs = n.ms;
}

void Sound::update(uint32_t now) {
  advance(music_, MUSIC_CH, now);
  advance(sfx_, SFX_CH, now);
}

void Sound::setMusic(Track t, uint32_t now) {
  switch (t) {
    case Track::Menu: start(music_, kMenu, countOf(kMenu), true, now); break;
    case Track::Game: start(music_, kGame, countOf(kGame), true, now); break;
    case Track::None: music_.active = false; M5.Speaker.stop(MUSIC_CH); break;
  }
}

void Sound::sfxStart(uint32_t now)    { start(sfx_, kStart, countOf(kStart), false, now); }
void Sound::sfxEat(uint32_t now)      { start(sfx_, kEat, countOf(kEat), false, now); }
void Sound::sfxFall(uint32_t now)     { start(sfx_, kFall, countOf(kFall), false, now); }
void Sound::sfxGameOver(uint32_t now) { start(sfx_, kGameOver, countOf(kGameOver), false, now); }

// A punchy two-tone "tok" (bright transient + lower tail) at full channel volume
// so it cuts through the music. The second note queues in the channel's other
// slot, so it stays fire-and-forget (no sequencer needed).
void Sound::sfxBounce() {
  M5.Speaker.tone(880.0f, 28, BOUNCE_CH, true);   // bright tap
  M5.Speaker.tone(494.0f, 60, BOUNCE_CH, false);  // lower tail (queued)
}
