// world.h - a generated land: terrain fields for the ground shader, scenery props, collision,
// and the places the story needs (gates Malgrin has blocked, goblin camps, waystones, the shrine).
#pragma once
#include "render.h"
#include "art.h"

namespace sky {

constexpr f32 TS = 32.0f;  // world units per tile

enum PropKind : u8 {
  PK_EDGE, PK_TREE, PK_BUSH, PK_ROCK, PK_COVER, PK_FLOWER, PK_SHORE, PK_FLOATER,
  PK_EXTRA, PK_NEAR, PK_PATHBIT, PK_BLOCKER, PK_WALL, PK_CAMP,
};

enum PropFlag : u8 { PF_FLIP = 1, PF_GONE = 2, PF_BURNABLE = 4, PF_BURNING = 8, PF_BURNT = 16, PF_GROUND = 32 };

struct Prop {
  Vec2 pos;
  u16 sprite = 0;
  u8 kind = 0;
  u8 flags = 0;
  u8 emit = EMIT_NONE;
  u8 gate = 0;          // blockers: which gate they block
  f32 scale = 1;
  f32 sway = 0;
  f32 radius = 0;       // collision radius; 0 means walk-through
  Vec2 col_offset;      // collider centre relative to pos
  f32 heat = 0;         // burnables: how close to catching fire
  f32 burn_t = 0;       // burnables: time spent burning
  f32 shadow = 0;       // ground shadow size (0 = none)
  f32 light = 0;        // radius of the light it gives off
  Color light_col = {1, 1, 1, 1};
  f32 reveal_w = 0, reveal_h = 0;
  Color tint = {1, 1, 1, 1};
};

/** A crossing Malgrin has blocked. */
struct Gate {
  Vec2 pos;          // centre of the gap, on the path
  int section_after;
  bool open = false;
};

enum SpotKind : u8 { SPOT_HEART, SPOT_HAZARD };
struct Spot { Vec2 pos; u8 kind; u8 section; };

struct World {
  Region region = R_MEADOW;
  const RegionDef* def = nullptr;
  int w = 0, h = 0;           // in tiles
  u32 seed = 0;
  Grid<f32> water;            // liquid (water, bog, lava, open sky, goo), 0..1 per tile
  Grid<f32> path;             // 0..1 per tile
  Grid<f32> forest;           // 0..1 per tile: dense woods, rock or walls you can't walk through
  Grid<f32> lush;             // 0..1 per tile, flower fields
  Grid<f32> depth;            // 0..1 distance from the shore, inside liquid
  Grid<f32> dpath;            // distance to the path's centre line, in tiles
  Grid<u32> ground;           // RGBA8 per tile for the ground shader
  u32 ground_version = 0;

  Prop* props = nullptr;
  int prop_count = 0, prop_cap = 0;

  // spatial index: props bucketed by 64-unit cells
  static constexpr f32 CELL = 64.0f;
  int cw = 0, ch = 0;
  int* cell_start = nullptr;
  int* cell_items = nullptr;

  Vec2 start, flint_start, shrine;
  bool has_arena = false;     // Malgrin's Hollow ends in his lair: a round clearing
  Vec2 arena;
  f32 arena_r = 0;
  FixedVec<Vec2, 64> path_points;  // control points, start to end
  FixedVec<Vec2, 1024> samples;    // the path's centre line, finely sampled
  FixedVec<Gate, 4> gates;
  FixedVec<Vec2, 8> camps;
  FixedVec<Vec2, 8> waystones;     // [0] is at the start; one after each gate
  FixedVec<Spot, 48> spots;
  FixedVec<f32, 5> section_x;      // where each section begins (world x)
  FixedVec<Vec2, 2> people;        // where this land's two characters stand
  int people_id[2] = {-1, -1};

  void generate(Arena& arena, Region region, u32 seed);
  void build_ground();
  void build_index(Arena& arena);

  f32 px_w() const { return w * TS; }
  f32 px_h() const { return h * TS; }
  bool sky_liquid() const { return def->liquid == LQ_SKY; }
  /** Bilinear sample of a per-tile field at a world position. */
  f32 sample(const Grid<f32>& g, Vec2 p) const;
  f32 solid_at(Vec2 p, bool liquid_solid = true) const {
    f32 f = sample(forest, p);
    return liquid_solid ? max(f, sample(water, p)) : f;
  }
  bool is_liquid(Vec2 p) const { return sample(water, p) > 0.55f; }
  int section_of(Vec2 p) const;
  /** 0 at the start of the path, 1 at its end. */
  f32 progress_of(Vec2 p) const;

  /** Pushes a circle out of props and solid ground. Returns the corrected position. */
  Vec2 resolve(Vec2 p, f32 r, bool* hit = nullptr, bool liquid_solid = true) const;

  template <class F>
  void each_in(Rect area, F&& fn) {
    int x0 = clamp((int)floorf(area.x0 / CELL), 0, cw - 1), x1 = clamp((int)floorf(area.x1 / CELL), 0, cw - 1);
    int y0 = clamp((int)floorf(area.y0 / CELL), 0, ch - 1), y1 = clamp((int)floorf(area.y1 / CELL), 0, ch - 1);
    for (int cy = y0; cy <= y1; cy++)
      for (int cx = x0; cx <= x1; cx++) {
        int c = cy * cw + cx;
        for (int k = cell_start[c]; k < cell_start[c + 1]; k++) fn(props[cell_items[k]], cell_items[k]);
      }
  }
  template <class F>
  void each_in(Rect area, F&& fn) const {
    const_cast<World*>(this)->each_in(area, [&](Prop& p, int i) { fn((const Prop&)p, i); });
  }

  /** `reveal` points (the hero, Flint) make tall props in front of them see-through. */
  void draw(Renderer& r, f32 time, const Vec2* reveal, int reveal_count) const;
  int blockers_left(int gate) const;
  /** Burnable things (blockers) that fire can reach: returns the index of the best one, or -1. */
  int find_burnable(Vec2 from, Vec2 prefer_dir, f32 range) const;
};

}  // namespace sky
