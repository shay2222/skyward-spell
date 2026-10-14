// boss.h - Malgrin himself: his ghostly appearance in the prologue, and the final fight in his
// lair, where his shield only drops when he wears himself out.
#pragma once
#include "art.h"
#include "render.h"

namespace sky {

struct Fx;
struct Hero;
struct Actors;

enum BossState : u8 {
  BS_NONE,
  BS_APPARITION,  // the prologue: a ghostly Malgrin hexes Flint
  BS_ARRIVE,      // bursting into his lair
  BS_HOVER,       // drifting, shield up, choosing what to do next
  BS_VOLLEY,      // hex orbs at the hero
  BS_RUNES,       // rune traps around the hero
  BS_SUMMON,      // calls goblins
  BS_TIRED,       // worn out: shield down, seeing stars. Now's the time!
  BS_HURT,
  BS_DEFEAT,      // his magic pours out of him
  BS_FLEE,        // tiny and harmless, running away
  BS_GONE,
};

struct HexOrb { Vec2 pos, vel; f32 t = 0; };
struct HexRune { Vec2 pos; f32 t = 0; };

struct BossNews {
  int damage = 0;
  Vec2 from;
  bool tired = false;         // shield just dropped
  bool recovered = false;     // ...and came back up before Flint struck
  bool hurt = false;
  bool blocked = false;       // Flint's fire hit the shield
  bool defeated = false;
  bool summoned = false;
  bool volley = false, zap = false;  // for sound
};

struct Boss {
  static constexpr int MAX_HITS = 3;
  BossState state = BS_NONE;
  f32 t = 0, state_t = 0;
  Vec2 pos, prev_pos, vel, target;
  f32 z = 24, alpha = 1, scale = 1, spin = 0;
  f32 flash = 0, shield = 1;
  f32 face = 1;
  f32 cast = 0;               // staff raised
  int hits = 0;
  int attacks = 0;            // since he was last tired
  int next_attack = 0;
  int volley_left = 0;
  f32 volley_t = 0;
  int runes_left = 0;
  f32 block_cd = 0;
  Vec2 arena;
  f32 arena_r = 200;
  Vec2 beam_to;               // prologue: where the hex beam goes
  f32 beam = 0;
  bool may_flee = false;      // the story says when he runs
  FixedVec<HexOrb, 64> orbs;
  FixedVec<HexRune, 24> runes;

  void appear(Vec2 at);                 // prologue apparition
  void arrive(Vec2 arena_centre, f32 radius);
  void calm();                          // after the hero faints: clear the air and start over
  void start_fight() { if (state == BS_ARRIVE) set(BS_HOVER); }
  void vanish() { state = BS_GONE; }
  BossNews update(const Hero& hero, Actors& actors, Fx& fx, bool fire_hit, Vec2 flint_pos, f32 dt);
  void draw(Renderer& r, f32 alpha_t, f32 time) const;

  bool fighting() const { return state >= BS_ARRIVE && state <= BS_HURT; }
  bool vulnerable() const { return state == BS_TIRED; }
  bool visible() const { return state != BS_NONE && state != BS_GONE; }
  /** His middle, as drawn: what Flint aims at. */
  Vec2 centre() const { return pos + Vec2{0, -z - 30 * scale}; }
  Vec2 head() const { return pos + Vec2{0, -z - 62 * scale}; }

 private:
  void set(BossState s) { state = s; state_t = 0; }
  Vec2 staff_tip() const;
};

}  // namespace sky
