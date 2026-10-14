// art_util.h - painting helpers shared by every land: foliage masses, tree trunks, flower stems.
#pragma once
#include "art.h"
#include "paint.h"

namespace sky {
using namespace paint;

constexpr Vec2 TO_LIGHT = {-0.55f, -0.83f};

struct Foliage {
  Color base, light, dark, line;
};

/** A leafy mass: outlined blob, rounded shading, a highlight and shadow on every clump, leaf flecks. */
inline void paint_foliage(Canvas& cv, const Blob& blob, Vec2 center, f32 radius, const Foliage& f, u32 seed, f32 fleck_density = 1.0f) {
  auto shape = wobble(blob, 1.1f, 0.32f, seed);
  fill(cv, grow(shape, 1.25f), Solid{f.line.with_alpha(0.9f)});
  Volume vol{f.base, f.light, f.dark, center, radius, 7.0f, 0.4f, TO_LIGHT};
  fill(cv, shape, noisy(vol, 0.03f, 0.28f, seed));

  // each clump: shadow toward the bottom-right, highlight toward the top-left
  for (const Circle& c : blob.c)
    fill(cv, Circle{c.c + Vec2{c.r * 0.28f, c.r * 0.36f}, c.r * 0.92f}, Solid{f.dark.with_alpha(0.24f)}, Blend::Atop, c.r * 0.55f);
  for (const Circle& c : blob.c) {
    f32 facing = saturate(dot(normalize(c.c - center), TO_LIGHT) * 0.6f + 0.55f);
    fill(cv, Circle{c.c + Vec2{-c.r * 0.26f, -c.r * 0.34f}, c.r * 0.58f}, Solid{f.light.with_alpha(0.42f * facing)}, Blend::Atop, c.r * 0.5f);
  }

  // leaf flecks: light ones where the sun hits, dark ones in the shade
  Rng rng(seed * 7 + 3);
  Rect b = blob.bounds();
  int n = (int)((b.x1 - b.x0) * (b.y1 - b.y0) * 0.05f * fleck_density);
  for (int i = 0; i < n; i++) {
    Vec2 p{rng.range(b.x0, b.x1), rng.range(b.y0, b.y1)};
    f32 lit = dot(normalize(p - center), TO_LIGHT);
    Color c = lit > 0.05f ? lighten(f.light, 0.15f).with_alpha(rng.range(0.35f, 0.75f)) : f.dark.with_alpha(rng.range(0.2f, 0.45f));
    fill(cv, Ellipse{p, {rng.range(1.0f, 2.1f), rng.range(0.6f, 1.0f)}, rng.range(0.0f, PI)}, Solid{c}, Blend::Atop);
  }
}

inline void paint_trunk(Canvas& cv, f32 h, f32 wb, f32 wt, Color bark, Color line, u32 seed, bool birch = false) {
  Path p;
  p.move({-wb * 0.5f - 4.0f, 1.0f})
      .quad({-wb * 0.5f, 0.5f}, {-wb * 0.5f, -3.0f})
      .quad({-wt * 0.5f - 0.6f, -h * 0.5f}, {-wt * 0.5f, -h})
      .line({wt * 0.5f, -h})
      .quad({wt * 0.5f + 0.6f, -h * 0.5f}, {wb * 0.5f, -3.0f})
      .quad({wb * 0.5f, 0.5f}, {wb * 0.5f + 4.0f, 1.0f})
      .quad({0, 2.5f}, {-wb * 0.5f - 4.0f, 1.0f});
  Poly trunk = p.poly();
  fill(cv, grow(trunk, 1.1f), Solid{line});
  Color light = lighten(bark, 0.28f), dark = darken(bark, 0.42f);
  fill(cv, trunk, [&](Vec2 q, f32 d) {
    f32 t = saturate((q.x + wb * 0.5f) / wb);  // left (lit) to right (shade)
    Color c = mix(light, dark, smoothstep(0.15f, 1.0f, t));
    if (birch) {
      f32 mark = value_noise(q.x * 0.35f, q.y * 0.55f, seed);
      if (mark > 0.72f && absf(q.x) < wt * 0.5f) c = mix(c, hex(0x2d2a2a), smoothstep(0.72f, 0.78f, mark) * 0.85f);
    } else {
      f32 streak = value_noise(q.x * 0.9f, q.y * 0.08f, seed) - 0.5f;
      c = shade(c, 1.0f + streak * 0.35f);
    }
    return mix(c, dark, smoothstep(-1.5f, 0.0f, d) * 0.4f);
  });
}

inline void stem(Canvas& cv, Vec2 base, Vec2 top, f32 bend, f32 w = 0.9f) {
  Path p;
  p.move(base).quad({(base.x + top.x) * 0.5f + bend, (base.y + top.y) * 0.5f}, top, 8);
  fill(cv, p.stroke(w, w * 0.7f), Linear{base, top, hex(0x3f7f2f), hex(0x7cbf4a)});
}


}  // namespace sky
