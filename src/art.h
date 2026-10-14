// art.h - ids for every sprite the game bakes. Characters and gameplay pieces are baked once;
// each land's scenery is baked when that land loads, into the same part of the atlas.
#pragma once
#include "atlas.h"
#include "regions.h"

namespace sky {

enum View : u8 { VIEW_FRONT = 0, VIEW_BACK = 1, VIEW_SIDE = 2 };

/** The paper-doll pieces of the hero, one set per view. Animation moves and rotates them. */
struct HeroArt {
  u16 leg[3], arm, torso[3], bag;
  u16 head[3], eyes[3], blink[3];
  u16 hair_front[3], hair_back_f;  // hair_front: the main hair piece per view; hair_back_f sits behind the head in front view
  u16 tail_seg[3];                 // ponytail (Mira) or cowlick (Theo) chain pieces: root, middle, tip
  u16 scarf_wrap[3], scarf_seg[2]; // scarf around the neck, and its trailing ends
};

/** Flint's pieces. */
struct DragonArt {
  u16 body[3], head[3], head_open[3], eyes[3], blink[3], sad[3];
  u16 wing_front, wing_side, wing_back;
  u16 foot, arm_front, leg_side_hind, leg_side_front;
  u16 tail_seg[3];
  u16 hex_band;  // the glowing curse bands on his wings
};

/** Malgrin's goblin minions: a body per view, plus pieces that animate. */
struct GoblinArt {
  u16 body[3];
  u16 foot, club, alert;
};

/** Malgrin, the goblin sorcerer. He floats, so he has no feet. */
struct MalgrinArt {
  u16 robe, head, staff, orb;
};

/** People met on the way: one painted sprite each, plus an overlay that closes their eyes. */
constexpr int NPC_COUNT = 12;
struct NpcArt { u16 body, blink; };

/** Particle and effect a prop gives off now and then. */
enum Emit : u8 { EMIT_NONE, EMIT_PETALS, EMIT_LEAVES, EMIT_SPORES, EMIT_EMBERS, EMIT_FIREFLIES, EMIT_SMOKE, EMIT_SPARKS };

/** How one kind of scenery behaves when the generator places it. */
struct PropSpec {
  u16 sprite = 0;
  f32 weight = 1;
  f32 radius = 0;           // collision radius; 0 = walk through
  Vec2 col_off = {0, -4};   // collider centre relative to the feet
  f32 sway = 0;             // wind
  f32 shadow = 0;           // soft ground shadow scale; 0 = none
  f32 scale0 = 0.9f, scale1 = 1.1f;
  bool flip = true;
  bool ground = false;      // lies flat on the ground layer
  f32 light = 0;            // light radius it gives off
  Color light_col = {1, 1, 1, 1};
  u8 emit = EMIT_NONE;
  f32 reveal_w = 0, reveal_h = 0;  // turns see-through when someone walks behind it
  PropSpec& r(f32 v) { radius = v; return *this; }
  PropSpec& col(Vec2 v) { col_off = v; return *this; }
  PropSpec& sw(f32 v) { sway = v; return *this; }
  PropSpec& sh(f32 v) { shadow = v; return *this; }
  PropSpec& sc(f32 a, f32 b) { scale0 = a; scale1 = b; return *this; }
  PropSpec& noflip() { flip = false; return *this; }
  PropSpec& flat() { ground = true; return *this; }
  PropSpec& glow(f32 radius_, Color c) { light = radius_; light_col = c; return *this; }
  PropSpec& em(u8 e) { emit = e; return *this; }
  PropSpec& rev(f32 w, f32 h) { reveal_w = w; reveal_h = h; return *this; }
};

struct PropSet {
  PropSpec items[10];
  int count = 0;
  PropSpec& add(u16 sprite, f32 weight = 1) {
    PropSpec& s = items[count < 10 ? count++ : 9];
    s = PropSpec();
    s.sprite = sprite;
    s.weight = weight;
    return s;
  }
  const PropSpec* pick(Rng& rng) const {
    if (!count) return nullptr;
    f32 total = 0;
    for (int i = 0; i < count; i++) total += items[i].weight;
    f32 x = rng.unit() * total;
    for (int i = 0; i < count; i++) {
      if (x < items[i].weight) return &items[i];
      x -= items[i].weight;
    }
    return &items[count - 1];
  }
  void clear() { count = 0; }
};

/** The current land's scenery, refilled when a land loads. */
struct RegionArt {
  PropSet edge;        // the dense woods (or rocks) around the border and in barrier lines
  PropSet trees;       // trees out in the open
  PropSet bushes, rocks, cover, flowers;
  PropSet shore;       // at the liquid's edge
  PropSet floaters;    // on the liquid
  PropSet extras;      // logs, stumps, crystals, ruins
  PropSet near_trees;  // mushrooms and ferns at the feet of trees
  PropSet path_bits;   // pebbles and the like on the path
  u16 blocker[2] = {0, 0}, blocker_burnt = 0;  // what Malgrin blocks the crossings with (burnable)
  Color blocker_glow = {0.7f, 0.4f, 1.0f, 1};
  u16 wall = 0;        // palisade segment for walled lands
  void clear() {
    edge.clear(); trees.clear(); bushes.clear(); rocks.clear(); cover.clear(); flowers.clear();
    shore.clear(); floaters.clear(); extras.clear(); near_trees.clear(); path_bits.clear();
    wall = 0;
  }
};

/** Pieces every land uses: goblins, pickups, shrines, hazards, camps. */
struct GameArt {
  GoblinArt goblin;
  u16 heart, heart_glow;
  u16 waystone, waystone_lit;
  u16 pedestal;
  u16 ingredient[INGREDIENT_COUNT];
  u16 arrow;
  u16 vent, snapjaw[2], rune, mud;
  u16 tent, campfire, banner, torch;
  u16 wind;
  u16 talk;  // a speech bubble over someone you can talk to
};

struct Sprites {
  // particles and utility
  u16 white, soft, soft_ring, spark, star, flame, smoke, leaf, petal, mote, dust, rune;
  u16 shadow;
  // characters
  HeroArt hero[2];  // 0 = Mira, 1 = Theo
  DragonArt flint;
  GameArt game;
  MalgrinArt malgrin;
  NpcArt npc[NPC_COUNT];  // only the current land's people are baked
  // the land that is loaded right now
  RegionArt ra;
  // meadow scenery that other lands borrow
  u16 butterfly[3];
};

extern Sprites S;

void bake_common(Atlas& a);
void bake_hero(Atlas& a);
void bake_dragon(Atlas& a);
void bake_gameplay(Atlas& a);
void bake_malgrin(Atlas& a);
void bake_meadow(Atlas& a);
void bake_marsh(Atlas& a);
void bake_crags(Atlas& a);
void bake_elderwood(Atlas& a);
void bake_skyreach(Atlas& a);
void bake_hollow(Atlas& a);
void bake_npcs(Atlas& a, Region r);

/** Bakes what every land needs. Call once. */
inline void bake_shared(Atlas& a) {
  bake_common(a);
  bake_hero(a);
  bake_dragon(a);
  bake_gameplay(a);
  bake_malgrin(a);
}

/** Bakes one land's scenery (the atlas should be rewound to the shared mark first). */
inline void bake_region(Atlas& a, Region r) {
  S.ra.clear();
  switch (r) {
    case R_MEADOW: bake_meadow(a); break;
    case R_MARSH: bake_marsh(a); break;
    case R_CRAGS: bake_crags(a); break;
    case R_ELDERWOOD: bake_elderwood(a); break;
    case R_SKYREACH: bake_skyreach(a); break;
    default: bake_hollow(a); break;
  }
  bake_npcs(a, r);
}

// shared colour helpers for the painters
inline Color lighten(Color c, f32 k) { return mix(c, Color{1, 1, 1, c.a}, k); }
inline Color darken(Color c, f32 k) { return Color{c.r * (1 - k), c.g * (1 - k), c.b * (1 - k), c.a}; }

}  // namespace sky
