// audio.h - the game's music and sound effects, synthesised in C++: a small sequencer plays an
// original tune for each land (melody, arpeggios, bass, pads and drums), sound effects are built
// from the same oscillators and noise, and a gentle reverb ties it together. The browser just
// queues the samples (see docs/host.js).
#pragma once
#include "core.h"

namespace sky {

enum Song : u8 { SONG_TITLE, SONG_MEADOW, SONG_MARSH, SONG_CRAGS, SONG_ELDERWOOD, SONG_SKYREACH, SONG_HOLLOW, SONG_BOSS, SONG_COUNT, SONG_NONE = 255 };

enum Sfx : u8 {
  SFX_DASH, SFX_FIRE, SFX_BURN, SFX_POOF, SFX_HURT, SFX_HEART, SFX_WAYSTONE, SFX_INGREDIENT, SFX_GATE, SFX_TALK, SFX_ALERT,
  SFX_VENT, SFX_ZAP, SFX_ORB, SFX_BOSS_HIT, SFX_SPELL, SFX_FALL, SFX_SNAP, SFX_UI, SFX_BLOCK, SFX_FLAP, SFX_COUNT,
};

struct Audio {
  void init(Arena& a, int sample_rate);
  /** Interleaved stereo floats for the next `frames` samples. */
  void render(f32* out, int frames);
  void play(Song s);
  void sfx(Sfx s, f32 volume = 1.0f, f32 pan = 0.0f);
  f32 music_volume = 0.55f, sfx_volume = 0.8f;
  Song current = SONG_NONE;
  int rate = 48000;
  bool ready = false;  // the page starts sound on the first click or key press
};

}  // namespace sky
