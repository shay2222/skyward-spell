// anim.h - shared animation helpers: part transforms and springy chains (hair, scarves, tails).
#pragma once
#include "render.h"

namespace sky {

constexpr u16 NO_SPRITE = 0xffff;

/** Where one paper-doll piece goes this frame. */
struct PartXf {
  Vec2 pos;
  f32 rot = 0;
  Vec2 scale = {1, 1};
  Color tint = {1, 1, 1, 1};
  /** A point given in the piece's own coordinates, placed in the world. */
  Vec2 apply(Vec2 local) const { return pos + rotate(local * scale, rot); }
};

inline void draw_part(Renderer& r, u16 id, const PartXf& x, f32 sort_y, Vec2 offset = {0, 0}, f32 flash = 0) {
  if (id == NO_SPRITE) return;
  r.draw({.id = id, .pos = x.pos + offset, .sort_y = sort_y, .layer = LAYER_WORLD, .rot = x.rot, .scale = x.scale, .color = x.tint, .flash = flash});
}

/**
 * A chain of points that hangs from an anchor: verlet motion for momentum, a gentle pull toward
 * a rest direction for shape, and exact segment lengths so it never stretches.
 */
struct Chain {
  static constexpr int MAX = 6;
  Vec2 p[MAX], prev[MAX];
  f32 len[MAX];
  int n = 0;

  void init(int points, const f32* lens, Vec2 anchor, Vec2 dir) {
    n = points;
    p[0] = prev[0] = anchor;
    for (int i = 1; i < n; i++) {
      len[i - 1] = lens[i - 1];
      p[i] = prev[i] = p[i - 1] + dir * len[i - 1];
    }
  }

  void step(Vec2 anchor, Vec2 rest_dir, f32 stiffness, f32 gravity, f32 damping, f32 dt) {
    p[0] = prev[0] = anchor;
    for (int i = 1; i < n; i++) {
      Vec2 v = (p[i] - prev[i]) * damping;
      prev[i] = p[i];
      p[i] += v + Vec2{0, gravity * dt * dt};
    }
    f32 k = 1.0f - expf(-stiffness * dt);
    for (int i = 1; i < n; i++) {
      Vec2 target = p[i - 1] + rest_dir * len[i - 1];
      p[i] = lerp(p[i], target, k);
      Vec2 d = p[i] - p[i - 1];
      f32 l = length(d);
      p[i] = p[i - 1] + (l > 1e-4f ? d / l : rest_dir) * len[i - 1];
    }
  }

  void shift(Vec2 by) {
    for (int i = 0; i < n; i++) { p[i] += by; prev[i] += by; }
  }

  /** Draws pieces from the tip back to the root, so the root overlaps. `ids[i]` paints segment i. */
  void draw(Renderer& r, const u16* ids, int id_count, f32 sort_y, Vec2 offset, Color tint = {1, 1, 1, 1}) const {
    for (int i = n - 2; i >= 0; i--) {
      Vec2 d = p[i + 1] - p[i];
      f32 rot = atan2f(d.y, d.x) - HALF_PI;
      u16 id = ids[min(i, id_count - 1)];
      r.draw({.id = id, .pos = p[i] + offset, .sort_y = sort_y, .layer = LAYER_WORLD, .rot = rot, .color = tint});
    }
  }
};

/** Picks front, back or side from a direction, with hysteresis so it doesn't flicker on diagonals. */
inline void choose_view(Vec2 dir, u8& view, f32& flip) {
  f32 ax = absf(dir.x), ay = absf(dir.y);
  if (ax < 1e-3f && ay < 1e-3f) return;
  bool side_now = view == 2;
  bool want_side = side_now ? ax > ay * 0.8f : ax > ay * 1.25f;
  if (want_side) {
    view = 2;
    flip = dir.x < 0 ? -1.0f : 1.0f;
  } else {
    view = dir.y > 0 ? 0 : 1;
  }
}

}  // namespace sky
