#include "audio.h"

namespace sky {

namespace {

// ---------------------------------------------------------------- instruments
enum Inst : u8 { I_BELL, I_PLUCK, I_FLUTE, I_PAD, I_BASS, I_CELESTA, I_LEAD, I_KICK, I_SNARE, I_HAT, I_SHAKER, I_TONE, I_NOISE };
enum Wave : u8 { W_SINE, W_TRI, W_SQUARE, W_SAW };

struct Voice {
  bool on = false, music = false;
  u8 inst = 0, wave = W_SINE;
  f32 f0 = 440, f1 = 440;        // pitch (sweeps from f0 to f1 over the note, for effects)
  f32 phase[3] = {0, 0, 0};
  f32 t = 0, delay = 0, dur = 0.2f, rel = 0.1f, decay = 0.3f;
  f32 amp = 0.2f, pan = 0, off_level = -1;
  f32 lp = 0, lp2 = 0, cutoff = 4000, cutoff_end = 4000;
  f32 crackle = 0;               // fire: random pops on top of the noise
  int ks_len = 0, ks_pos = 0;    // plucked string
  f32* ks = nullptr;
};

constexpr int MAX_VOICES = 56;
constexpr int KS_MAX = 2048;

struct NoteEv { u8 midi; u8 len; u16 at; };
struct Chord { u8 root; bool minor; };

/** One original tune per land: eight bars of melody over eight chords, played twice with variations. */
struct SongDef {
  f32 bpm;
  const char* chords;   // one chord per bar
  const char* melody;   // "e5:2 g5:2 ..." lengths in sixteenths, r = rest
  u8 lead, lead2;       // melody instrument, and the one for the second time round
  u8 arp;               // arpeggio instrument (255 none)
  u8 arp_every;         // arpeggio note every n sixteenths
  u8 drums;             // 0 none, 1 gentle, 2 driving, 3 battle
  u8 bass_style;        // 0 roots on 1 and 3, 1 pulsing eighths, 2 one long note
  f32 swing;
};

const SongDef SONGS[SONG_COUNT] = {
    // title and ending: a lullaby
    {80, "C Em F G C Am F G",
     "g5:4 e5:2 g5:2 c6:8 b5:4 g5:4 e5:8 a5:4 c6:2 a5:2 f5:8 g5:6 a5:2 b5:4 d6:4 c6:4 e6:4 d6:2 c6:2 b5:4 a5:4 c6:4 e5:8 f5:4 a5:4 c6:4 a5:4 g5:12 r:4",
     I_CELESTA, I_BELL, I_PLUCK, 4, 0, 2, 0},
    // Willowmere Meadows: bright and bouncy
    {100, "C G Am F C F G C",
     "e5:2 g5:2 c6:4 b5:2 g5:2 e5:4 d5:2 g5:2 b5:4 a5:2 g5:2 d5:4 c5:2 e5:2 a5:4 g5:2 e5:2 c5:2 e5:2 f5:4 a5:4 g5:6 r:2 "
     "e5:2 g5:2 c6:4 d6:2 c6:2 b5:2 g5:2 a5:3 g5:1 f5:2 a5:2 c6:6 r:2 b5:2 a5:2 g5:2 f5:2 e5:2 d5:2 b4:4 c5:8 r:4 g4:2 b4:2",
     I_BELL, I_PLUCK, I_PLUCK, 2, 1, 0, 0.08f},
    // Mistfen Marsh: a slow flute in the mist
    {76, "Dm G Dm C Dm G Am Dm",
     "d5:6 f5:2 a5:8 b5:4 a5:4 g5:8 f5:4 e5:2 d5:2 e5:8 c5:6 d5:2 e5:4 g5:4 a5:6 c6:2 d6:8 b5:4 a5:2 g5:2 a5:8 e5:4 f5:4 g5:2 e5:2 c5:4 d5:12 r:4",
     I_FLUTE, I_FLUTE, I_PLUCK, 4, 0, 2, 0},
    // Cinderpeak Crags: driving and adventurous
    {108, "Em C D Em Em C B Em",
     "e5:2 e5:2 g5:2 b5:4 a5:2 g5:2 f#5:2 e5:4 g5:2 e5:2 d5:4 r:4 f#5:2 f#5:2 a5:2 d6:4 c6:2 b5:2 a5:2 g5:4 f#5:4 e5:6 r:2 "
     "b5:2 b5:2 c6:2 b5:2 a5:2 g5:2 a5:4 g5:2 g5:2 a5:2 g5:2 f#5:2 e5:2 g5:4 f#5:4 d#5:4 b4:4 d#5:4 e5:12 r:4",
     I_PLUCK, I_LEAD, I_BELL, 2, 2, 1, 0},
    // The Elderwood: magical, with a lydian sparkle
    {84, "F G F Am F G C F",
     "c6:4 a5:2 f5:2 g5:4 a5:4 b5:6 a5:2 g5:8 a5:2 c6:2 f6:4 e6:4 c6:4 a5:8 r:4 c5:4 f5:2 a5:2 c6:4 d6:4 c6:4 b5:4 d6:4 g5:8 e5:2 g5:2 c6:4 b5:4 g5:4 f5:12 r:4",
     I_CELESTA, I_BELL, I_PLUCK, 2, 1, 2, 0},
    // Skyreach Cliffs: airy and soaring
    {116, "G F C G G F C D",
     "g5:2 a5:2 b5:2 d6:4 b5:2 a5:2 g5:2 f5:4 a5:4 c6:4 a5:4 e5:2 g5:2 c6:4 b5:2 c6:2 e6:4 d6:8 r:4 b5:2 c6:2 "
     "d6:2 b5:2 g5:2 b5:2 d6:4 g6:4 f6:4 e6:2 d6:2 c6:8 e6:2 d6:2 c6:2 b5:2 a5:4 c6:4 d6:4 a5:4 f#5:4 a5:4",
     I_FLUTE, I_PLUCK, I_PLUCK, 2, 1, 1, 0.06f},
    // Malgrin's Hollow: an eerie music box
    {88, "Am F Dm E Am F E Am",
     "a5:4 c6:2 b5:2 a5:4 e5:4 f5:4 a5:2 g5:2 f5:4 c5:4 d5:4 f5:2 a5:2 d6:4 c6:4 b5:8 g#5:4 e5:4 a5:2 b5:2 c6:2 e6:2 d6:4 c6:4 a5:4 f5:4 c6:8 b5:4 g#5:4 e5:4 g#5:4 a5:12 r:4",
     I_BELL, I_CELESTA, 255, 4, 0, 2, 0},
    // Malgrin himself
    {138, "Am Am F E Am Am Dm E",
     "a5:2 r:2 a5:2 c6:2 e6:4 d6:2 c6:2 b5:2 c6:2 b5:2 a5:2 g#5:4 e5:4 f5:2 r:2 f5:2 a5:2 c6:4 b5:2 a5:2 g#5:4 b5:4 e6:6 r:2 "
     "a5:2 r:2 a5:2 c6:2 e6:4 f6:2 e6:2 d6:2 c6:2 b5:2 c6:2 a5:8 d6:4 f6:4 a5:4 d6:4 e6:4 d6:2 c6:2 b5:4 g#5:4",
     I_LEAD, I_LEAD, I_PLUCK, 2, 3, 1, 0},
};

int parse_note(const char*& p) {
  const int base[7] = {9, 11, 0, 2, 4, 5, 7};  // a b c d e f g
  char c = *p++;
  if (c == 'r') return -1;
  int n = base[c - 'a'];
  if (*p == '#') { n++; p++; } else if (*p == 'b' && p[1] >= '0' && p[1] <= '9') { n--; p++; }
  int oct = *p++ - '0';
  return 12 * (oct + 1) + n;
}

int parse_melody(const char* s, NoteEv* out, int cap) {
  int n = 0, at = 0;
  const char* p = s;
  while (*p && n < cap) {
    while (*p == ' ') p++;
    if (!*p) break;
    int m = parse_note(p);
    if (*p == ':') p++;
    int len = 0;
    while (*p >= '0' && *p <= '9') len = len * 10 + (*p++ - '0');
    if (m >= 0) out[n++] = {(u8)m, (u8)len, (u16)at};
    at += len;
  }
  return n;
}

int parse_chords(const char* s, Chord* out) {
  const int base[7] = {9, 11, 0, 2, 4, 5, 7};
  int n = 0;
  const char* p = s;
  while (*p && n < 8) {
    while (*p == ' ') p++;
    if (!*p) break;
    int r = base[*p++ - 'A'];
    if (*p == '#') { r++; p++; } else if (*p == 'b') { r--; p++; }
    bool minor = false;
    if (*p == 'm') { minor = true; p++; }
    while (*p && *p != ' ') p++;
    out[n++] = {(u8)((r + 12) % 12), minor};
  }
  return n;
}

f32 mtof(f32 m) { return 440.0f * exp2f((m - 69.0f) / 12.0f); }

struct Comb { f32* buf; int len, pos; f32 fb, damp, store; };
struct Allpass { f32* buf; int len, pos; };

struct Engine {
  Voice voices[MAX_VOICES];
  f32* ks_pool = nullptr;
  u32 noise = 0x12345;
  int rate = 48000;
  // sequencer
  Song song = SONG_NONE, next = SONG_NONE;
  f32 fade = 0;             // music fade level
  double step_clock = 0;       // samples until the next sixteenth
  int step = 0;             // position in the song, 0..255 (two rounds of 128)
  NoteEv melody[SONG_COUNT][96];
  int melody_n[SONG_COUNT];
  Chord chords[SONG_COUNT][8];
  // reverb
  Comb comb[2][4];
  Allpass ap[2][2];

  f32 rnd() { noise ^= noise << 13; noise ^= noise >> 17; noise ^= noise << 5; return (noise & 0xffffff) / 8388608.0f - 1.0f; }
};

Engine E;

Voice* grab(bool music) {
  // a free voice, or else the quietest old one
  Voice* best = nullptr;
  f32 best_score = 1e9f;
  for (Voice& v : E.voices) {
    if (!v.on) return &v;
    f32 score = (v.music == music ? 0.0f : 0.5f) + v.amp - v.t * 0.05f;
    if (score < best_score) { best_score = score; best = &v; }
  }
  return best;
}

Voice* start(u8 inst, f32 freq, f32 dur, f32 amp, f32 pan, bool music, f32 delay = 0) {
  Voice* v = grab(music);
  f32* ks = v->ks;
  *v = Voice();
  v->ks = ks;
  v->on = true;
  v->music = music;
  v->inst = inst;
  v->f0 = v->f1 = freq;
  v->dur = dur;
  v->amp = amp;
  v->pan = pan;
  v->delay = delay;
  switch (inst) {
    case I_BELL: v->decay = 0.55f; v->rel = 0.3f; break;
    case I_CELESTA: v->decay = 1.1f; v->rel = 0.5f; break;
    case I_PLUCK: {
      v->rel = 0.25f;
      v->ks_len = clamp((int)(E.rate / max(freq, 30.0f)), 2, KS_MAX - 1);
      v->ks_pos = 0;
      f32 lp = 0;
      for (int i = 0; i < v->ks_len; i++) { lp += (E.rnd() - lp) * 0.6f; v->ks[i] = lp; }
      break;
    }
    case I_FLUTE: v->rel = 0.18f; break;
    case I_PAD: v->rel = 0.9f; v->cutoff = 1300; break;
    case I_BASS: v->rel = 0.12f; break;
    case I_LEAD: v->rel = 0.08f; v->cutoff = 2600; break;
    case I_KICK: v->f0 = 150; v->f1 = 42; v->dur = 0.32f; break;
    case I_SNARE: v->dur = 0.18f; break;
    case I_HAT: v->dur = 0.05f; break;
    case I_SHAKER: v->dur = 0.08f; break;
    default: v->rel = 0.05f; break;
  }
  return v;
}

/** Envelope level for a voice at its current time. */
f32 envelope(Voice& v) {
  f32 t = v.t;
  switch (v.inst) {
    case I_BELL:
    case I_CELESTA:
    case I_PLUCK:
      return t < 0.003f ? t / 0.003f : expf(-t / max(v.decay, 0.05f));
    case I_KICK: return expf(-t / 0.12f);
    case I_SNARE: return expf(-t / 0.06f);
    case I_HAT: return expf(-t / 0.018f);
    case I_SHAKER: return t < 0.01f ? t / 0.01f : expf(-(t - 0.01f) / 0.03f);
    default: break;
  }
  f32 a = 0.01f, d = 0.15f, s = 0.75f;
  if (v.inst == I_PAD) { a = 0.5f; d = 0.6f; s = 0.7f; }
  else if (v.inst == I_FLUTE) { a = 0.07f; d = 0.1f; s = 0.8f; }
  else if (v.inst == I_BASS) { a = 0.006f; d = 0.25f; s = 0.6f; }
  else if (v.inst == I_LEAD) { a = 0.008f; d = 0.2f; s = 0.55f; }
  else if (v.inst == I_TONE || v.inst == I_NOISE) { a = 0.004f; d = 0.0f; s = 1.0f; }
  f32 lvl = t < a ? t / a : t < a + d ? lerp(1.0f, s, (t - a) / max(d, 1e-4f)) : s;
  return lvl;
}

f32 osc(f32 phase, u8 wave) {
  switch (wave) {
    case W_TRI: { f32 x = phase * (1.0f / TAU); x -= floorf(x); return 4.0f * absf(x - 0.5f) - 1.0f; }
    case W_SQUARE: { f32 s = sinf(phase) * 3.0f; return s / (1.0f + absf(s)); }
    case W_SAW: { f32 x = phase * (1.0f / TAU); x -= floorf(x); return 2.0f * x - 1.0f; }
    default: return sinf(phase);
  }
}

/** One sample from one voice. Returns false when the voice has finished. */
bool voice_sample(Voice& v, f32 dt, f32& out) {
  out = 0;
  if (v.delay > 0) { v.delay -= dt; return true; }
  f32 env = envelope(v);
  if (v.t >= v.dur) {
    if (v.off_level < 0) v.off_level = env;
    f32 k = 1.0f - (v.t - v.dur) / max(v.rel, 1e-3f);
    if (k <= 0) return false;
    env = min(env, v.off_level) * k;
  }
  if (env < 1e-4f && v.t > 0.05f && (v.inst == I_BELL || v.inst == I_CELESTA || v.inst == I_PLUCK || v.inst >= I_KICK)) {
    if (v.inst >= I_KICK || v.t > 0.3f) return false;
  }
  f32 k = saturate(v.t / max(v.dur, 1e-3f));
  f32 freq = v.f0 == v.f1 ? v.f0 : v.f0 * powf(v.f1 / v.f0, k);
  f32 w = TAU * freq * dt;
  f32 s = 0;
  switch (v.inst) {
    case I_BELL:  // a music box tine: a sine with a quick metallic overtone
      s = sinf(v.phase[0]) + 0.35f * sinf(v.phase[1]) * expf(-v.t * 9);
      v.phase[0] += w;
      v.phase[1] += w * 3.01f;
      break;
    case I_CELESTA:
      s = sinf(v.phase[0]) + 0.3f * sinf(v.phase[1]) * expf(-v.t * 6) + 0.15f * sinf(v.phase[2]);
      v.phase[0] += w;
      v.phase[1] += w * 4.0f;
      v.phase[2] += w * 2.0f;
      break;
    case I_PLUCK: {
      int i = v.ks_pos, j = (v.ks_pos + 1) % v.ks_len;
      s = v.ks[i];
      v.ks[i] = (v.ks[i] + v.ks[j]) * 0.4985f;
      v.ks_pos = j;
      s *= 1.6f;
      break;
    }
    case I_FLUTE: {
      f32 vib = 1.0f + sinf(v.t * TAU * 5.2f) * 0.006f * saturate((v.t - 0.15f) * 4);
      s = sinf(v.phase[0]) + 0.18f * sinf(v.phase[1]);
      v.phase[0] += w * vib;
      v.phase[1] += w * vib * 2.0f;
      v.lp += (E.rnd() - v.lp) * 0.2f;
      s += v.lp * 0.12f;
      break;
    }
    case I_PAD: {
      s = osc(v.phase[0], W_SAW) + osc(v.phase[1], W_SAW) + 0.6f * osc(v.phase[2], W_TRI);
      v.phase[0] += w * 1.004f;
      v.phase[1] += w * 0.996f;
      v.phase[2] += w * 0.5f;
      f32 c = 1.0f - expf(-TAU * v.cutoff * dt);
      v.lp += (s - v.lp) * c;
      v.lp2 += (v.lp - v.lp2) * c;
      s = v.lp2 * 0.5f;
      break;
    }
    case I_BASS:
      s = osc(v.phase[0], W_TRI) + 0.5f * sinf(v.phase[1]);
      v.phase[0] += w;
      v.phase[1] += w * 0.5f;
      break;
    case I_LEAD: {
      s = osc(v.phase[0], W_SQUARE) * 0.7f + osc(v.phase[1], W_SAW) * 0.3f;
      v.phase[0] += w;
      v.phase[1] += w * 1.005f;
      f32 c = 1.0f - expf(-TAU * v.cutoff * dt);
      v.lp += (s - v.lp) * c;
      s = v.lp;
      break;
    }
    case I_KICK:
      s = sinf(v.phase[0]) * 1.4f;
      v.phase[0] += w;
      break;
    case I_SNARE: {
      f32 n = E.rnd();
      v.lp += (n - v.lp) * 0.5f;
      s = (n - v.lp) * 0.9f + sinf(v.phase[0]) * 0.4f * expf(-v.t * 30);
      v.phase[0] += TAU * 190 * dt;
      break;
    }
    case I_HAT:
    case I_SHAKER: {
      f32 n = E.rnd();
      v.lp += (n - v.lp) * (v.inst == I_HAT ? 0.7f : 0.45f);
      s = (n - v.lp);
      break;
    }
    case I_TONE:
      s = osc(v.phase[0], v.wave);
      v.phase[0] += w;
      break;
    default: {  // shaped noise for effects: a filter that can sweep, plus crackle
      f32 n = E.rnd();
      f32 cut = v.cutoff == v.cutoff_end ? v.cutoff : v.cutoff * powf(v.cutoff_end / v.cutoff, k);
      f32 c = 1.0f - expf(-TAU * cut * dt);
      v.lp += (n - v.lp) * c;
      v.lp2 += (v.lp - v.lp2) * c;
      s = v.lp2 * 1.8f;
      if (v.crackle > 0 && (E.noise & 1023) < (u32)(v.crackle * 40)) s += E.rnd() * 1.2f;
      break;
    }
  }
  v.t += dt;
  out = s * env * v.amp;
  return true;
}

void music_step(Audio& a) {
  if (E.song == SONG_NONE) return;
  const SongDef& d = SONGS[E.song];
  const int at = E.step % 128, round = E.step / 128;
  const int bar = at / 16, in_bar = at % 16;
  const Chord c = E.chords[E.song][bar];
  const f32 step_s = 60.0f / d.bpm / 4.0f;
  (void)a;
  // melody (the second time round on another instrument, an octave up for bells)
  for (int i = 0; i < E.melody_n[E.song]; i++) {
    const NoteEv& n = E.melody[E.song][i];
    if (n.at != at) continue;
    u8 inst = round == 0 ? d.lead : d.lead2;
    f32 m = (f32)n.midi + (round == 1 && inst == I_BELL && d.lead != I_BELL ? 12.0f : 0.0f);
    f32 vol = inst == I_LEAD ? 0.2f : inst == I_FLUTE ? 0.22f : inst == I_PLUCK ? 0.3f : 0.24f;
    start(inst, mtof(m), n.len * step_s * 0.92f, vol, 0.05f, true);
  }
  // pads hold the chord through the bar
  if (in_bar == 0) {
    const int third = c.minor ? 3 : 4;
    const int notes[3] = {0, third, 7};
    for (int k = 0; k < 3; k++) start(I_PAD, mtof(60 + c.root + notes[k] - (c.root > 7 ? 12 : 0)), 16 * step_s, 0.05f, (k - 1) * 0.35f, true);
  }
  // bass
  const f32 root_m = 36 + c.root + (c.root > 7 ? -12 : 0) + 12;
  if (d.bass_style == 0 && (in_bar == 0 || in_bar == 8)) start(I_BASS, mtof(root_m), 7 * step_s, 0.2f, 0, true);
  if (d.bass_style == 1 && in_bar % 2 == 0) start(I_BASS, mtof(root_m + (in_bar == 6 || in_bar == 14 ? 7 : 0)), 1.6f * step_s, 0.17f, 0, true);
  if (d.bass_style == 2 && in_bar == 0) start(I_BASS, mtof(root_m), 15 * step_s, 0.16f, 0, true);
  // arpeggios climb through the chord
  if (d.arp != 255 && in_bar % d.arp_every == 0 && !(round == 0 && bar < 1 && E.song != SONG_TITLE)) {
    const int third = c.minor ? 3 : 4;
    const int seq[8] = {0, third, 7, 12, 7, third, 12, 7 + 12};
    int idx = (in_bar / d.arp_every) % 8;
    f32 m = 60 + c.root + seq[idx] - (c.root > 7 ? 12 : 0);
    start(d.arp, mtof(m), 1.5f * d.arp_every * step_s, d.arp == I_PLUCK ? 0.1f : 0.07f, idx % 2 ? 0.45f : -0.45f, true);
  }
  // drums
  switch (d.drums) {
    case 1:
      if (in_bar == 0) start(I_KICK, 0, 0, 0.32f, 0, true);
      if (in_bar % 4 == 2) start(I_SHAKER, 0, 0, 0.05f, 0.3f, true);
      break;
    case 2:
      if (in_bar == 0 || in_bar == 8 || in_bar == 10) start(I_KICK, 0, 0, 0.4f, 0, true);
      if (in_bar == 4 || in_bar == 12) start(I_SNARE, 0, 0, 0.18f, 0.1f, true);
      if (in_bar % 2 == 0) start(I_HAT, 0, 0, 0.06f, -0.3f, true);
      break;
    case 3:
      if (in_bar == 0 || in_bar == 3 || in_bar == 8 || in_bar == 11) start(I_KICK, 0, 0, 0.45f, 0, true);
      if (in_bar == 4 || in_bar == 12) start(I_SNARE, 0, 0, 0.22f, 0.1f, true);
      start(I_HAT, 0, 0, in_bar % 2 ? 0.035f : 0.06f, -0.3f, true);
      break;
    default: break;
  }
}

}  // namespace

void Audio::init(Arena& a, int sample_rate) {
  if (ready) return;
  ready = true;
  rate = sample_rate;
  E.rate = sample_rate;
  E.ks_pool = a.make<f32>(MAX_VOICES * KS_MAX);
  for (int i = 0; i < MAX_VOICES; i++) E.voices[i].ks = E.ks_pool + i * KS_MAX;
  for (int s = 0; s < SONG_COUNT; s++) {
    E.melody_n[s] = parse_melody(SONGS[s].melody, E.melody[s], 96);
    parse_chords(SONGS[s].chords, E.chords[s]);
  }
  // reverb: four combs and two allpasses per side, with slightly different lengths left and right
  const int comb_len[4] = {1116, 1188, 1277, 1356};
  const int ap_len[2] = {556, 441};
  f32 sr = sample_rate / 44100.0f;
  for (int ch = 0; ch < 2; ch++) {
    for (int k = 0; k < 4; k++) {
      int l = (int)((comb_len[k] + ch * 23) * sr);
      E.comb[ch][k] = {a.make<f32>(l), l, 0, 0.8f, 0.3f, 0};
    }
    for (int k = 0; k < 2; k++) {
      int l = (int)((ap_len[k] + ch * 23) * sr);
      E.ap[ch][k] = {a.make<f32>(l), l, 0};
    }
  }
}

void Audio::play(Song s) {
  if (s == current) return;
  current = s;
  E.next = s;
}

void Audio::render(f32* out, int frames) {
  const f32 dt = 1.0f / rate;
  for (int i = 0; i < frames; i++) {
    // the sequencer: fade out the old tune, then start the new one from the top
    if (E.next != E.song) {
      E.fade -= dt / 0.6f;
      if (E.fade <= 0) {
        E.fade = 0;
        E.song = E.next;
        E.step = 0;
        E.step_clock = 0;
        for (Voice& v : E.voices) if (v.music) v.on = false;
      }
    } else if (E.fade < 1) {
      E.fade = min(1.0f, E.fade + dt / 1.2f);
    }
    if (E.song != SONG_NONE) {
      E.step_clock -= 1;
      if (E.step_clock <= 0) {
        const SongDef& d = SONGS[E.song];
        double step_samples = rate * 60.0 / d.bpm / 4.0;
        double swing = (E.step % 2 == 0 ? 1.0 + d.swing : 1.0 - d.swing);
        E.step_clock += step_samples * swing;
        music_step(*this);
        E.step = (E.step + 1) % 256;
      }
    }
    f32 ml = 0, mr = 0, sl = 0, sr = 0;
    for (Voice& v : E.voices) {
      if (!v.on) continue;
      f32 s;
      if (!voice_sample(v, dt, s)) { v.on = false; continue; }
      f32 pl = 0.5f - v.pan * 0.5f, pr = 0.5f + v.pan * 0.5f;
      if (v.music) { ml += s * pl; mr += s * pr; } else { sl += s * pl; sr += s * pr; }
    }
    f32 mg = music_volume * E.fade, sg = sfx_volume;
    f32 dry_l = ml * mg + sl * sg, dry_r = mr * mg + sr * sg;
    f32 send[2] = {ml * mg * 0.32f + sl * sg * 0.12f, mr * mg * 0.32f + sr * sg * 0.12f};
    f32 wet[2];
    for (int ch = 0; ch < 2; ch++) {
      f32 acc = 0;
      for (Comb& c : E.comb[ch]) {
        f32 y = c.buf[c.pos];
        c.store = y * (1 - c.damp) + c.store * c.damp;
        c.buf[c.pos] = send[ch] + c.store * c.fb;
        if (++c.pos >= c.len) c.pos = 0;
        acc += y;
      }
      acc *= 0.25f;
      for (Allpass& ap : E.ap[ch]) {
        f32 b = ap.buf[ap.pos];
        f32 y = -acc + b;
        ap.buf[ap.pos] = acc + b * 0.5f;
        if (++ap.pos >= ap.len) ap.pos = 0;
        acc = y;
      }
      wet[ch] = acc;
    }
    f32 l = (dry_l + wet[0] * 0.9f) * 2.1f, r = (dry_r + wet[1] * 0.9f) * 2.1f;
    // gentle limiter so stacked effects never clip harshly
    out[i * 2 + 0] = l / (1.0f + absf(l) * 0.6f);
    out[i * 2 + 1] = r / (1.0f + absf(r) * 0.6f);
  }
}

void Audio::sfx(Sfx s, f32 vol, f32 pan) {
  if (!ready || vol <= 0.01f) return;
  auto tone = [&](u8 wave, f32 f0, f32 f1, f32 dur, f32 amp, f32 delay = 0) {
    Voice* v = start(I_TONE, f0, dur, amp * vol, pan, false, delay);
    v->wave = wave;
    v->f1 = f1;
    return v;
  };
  auto noise = [&](f32 c0, f32 c1, f32 dur, f32 amp, f32 crackle = 0, f32 delay = 0) {
    Voice* v = start(I_NOISE, 0, dur, amp * vol, pan, false, delay);
    v->cutoff = c0;
    v->cutoff_end = c1;
    v->crackle = crackle;
    v->rel = dur * 0.6f;
    return v;
  };
  auto bell = [&](f32 midi, f32 amp, f32 delay, u8 inst = I_BELL) { start(inst, mtof(midi), 0.6f, amp * vol, pan, false, delay); };
  switch (s) {
    case SFX_DASH: noise(600, 3200, 0.16f, 0.25f); break;
    case SFX_FIRE: noise(500, 1600, 0.8f, 0.3f, 6); noise(120, 200, 0.7f, 0.25f); break;
    case SFX_BURN: noise(900, 400, 1.2f, 0.14f, 10); break;
    case SFX_POOF: noise(3000, 300, 0.32f, 0.35f); tone(W_SINE, 160, 60, 0.18f, 0.35f); bell(84, 0.08f, 0.05f); break;
    case SFX_HURT: tone(W_SQUARE, 520, 170, 0.24f, 0.22f); noise(2000, 800, 0.1f, 0.15f); break;
    case SFX_HEART: { const int n[4] = {84, 88, 91, 96}; for (int k = 0; k < 4; k++) bell(n[k], 0.2f, k * 0.06f); break; }
    case SFX_WAYSTONE: { const int n[4] = {79, 83, 86, 91}; for (int k = 0; k < 4; k++) bell(n[k], 0.16f, k * 0.09f, I_CELESTA); break; }
    case SFX_INGREDIENT: {
      const int n[6] = {72, 76, 79, 84, 88, 91};
      for (int k = 0; k < 6; k++) bell(n[k], 0.2f, k * 0.09f, I_CELESTA);
      for (int k = 0; k < 3; k++) start(I_PAD, mtof(60 + (k == 1 ? 4 : k == 2 ? 7 : 0)), 1.4f, 0.08f * vol, (k - 1) * 0.4f, false, 0.3f);
      break;
    }
    case SFX_GATE: { for (int k = 0; k < 6; k++) bell(79 + k * 2 + (k > 2), 0.13f, k * 0.05f); noise(4000, 6000, 0.4f, 0.05f); break; }
    case SFX_TALK: tone(W_SINE, 660, 880, 0.07f, 0.16f); break;
    case SFX_ALERT: tone(W_SQUARE, 700, 1100, 0.08f, 0.12f); tone(W_SQUARE, 700, 1100, 0.08f, 0.12f, 0.1f); break;
    case SFX_VENT: noise(180, 900, 0.9f, 0.32f, 3); break;
    case SFX_ZAP: tone(W_SQUARE, 1800, 220, 0.24f, 0.14f); noise(5000, 1500, 0.2f, 0.12f); break;
    case SFX_ORB: tone(W_SINE, 950, 420, 0.16f, 0.12f); break;
    case SFX_BOSS_HIT: start(I_KICK, 0, 0, 0.6f * vol, pan, false); noise(2500, 300, 0.4f, 0.3f); tone(W_SAW, 400, 90, 0.5f, 0.12f); break;
    case SFX_SPELL: {
      for (int k = 0; k < 8; k++) bell(72 + k * 3 + (k % 2), 0.12f, k * 0.12f, I_CELESTA);
      const int ch[4] = {60, 64, 67, 72};
      for (int k = 0; k < 4; k++) start(I_PAD, mtof(ch[k]), 2.6f, 0.09f * vol, (k - 1.5f) * 0.3f, false, 0.2f);
      break;
    }
    case SFX_FALL: tone(W_SINE, 900, 140, 0.7f, 0.18f); break;
    case SFX_SNAP: noise(4000, 1000, 0.06f, 0.35f); tone(W_TRI, 220, 110, 0.12f, 0.2f); break;
    case SFX_UI: tone(W_SINE, 1200, 1250, 0.04f, 0.1f); break;
    case SFX_BLOCK: bell(96, 0.15f, 0); bell(101, 0.1f, 0.01f); break;
    case SFX_FLAP: noise(400, 1400, 0.14f, 0.14f); break;
    default: break;
  }
}

}  // namespace sky
