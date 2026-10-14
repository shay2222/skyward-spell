// art_lands.cpp - scenery for the five lands after the meadow. Each bake function paints its
// land's pieces and fills S.ra, the tables the level generator places from.
#include "art_util.h"

namespace sky {

namespace {

// ---------------------------------------------------------------- shared painters
struct Stone { Color base, light, dark, line; };

void rock_blob(Canvas& cv, f32 w, f32 h, const Stone& s, u32 seed, Color moss, f32 moss_amt, Color vein = {0, 0, 0, 0}) {
  Rng r(seed);
  Blob b;
  b.k = w * 0.2f;
  Vec2 c{0, -h * 0.9f};
  b.add(c + Vec2{-w * 0.35f, h * 0.15f}, h * 0.75f);
  b.add(c + Vec2{w * 0.3f, h * 0.2f}, h * 0.7f);
  b.add(c + Vec2{r.range(-2, 2), -h * 0.2f}, h * 0.8f);
  auto shape = intersect(wobble(b, w * 0.05f, 0.35f, seed), RBox{{0, -h * 2}, {w * 2, h * 2}});
  fill(cv, Ellipse{{1, 0}, {w * 0.95f, h * 0.3f}}, Solid{hex(0x101412, 0.35f)}, Blend::Over, 3);
  fill(cv, grow(shape, 1.1f), Solid{s.line});
  fill(cv, shape, noisy(Volume{s.base, s.light, s.dark, c + Vec2{-w * 0.2f, -h * 0.3f}, w * 1.1f, 4.0f, 0.4f, TO_LIGHT}, 0.05f, 0.6f, seed));
  if (moss_amt > 0) fill(cv, wobble(Circle{c + Vec2{-w * 0.15f, -h * 0.75f}, w * 0.55f * moss_amt}, 2.0f, 0.5f, seed + 1), noisy(Solid{moss}, 0.06f, 0.8f), Blend::Atop);
  if (vein.a > 0) {
    for (int i = 0; i < 3; i++) {
      Path v;
      Vec2 a0 = c + Vec2{r.range(-w * 0.5f, w * 0.3f), r.range(-h * 0.6f, h * 0.2f)};
      v.move(a0).line(a0 + Vec2{r.range(2, 6), r.range(2, 6)}).line(a0 + Vec2{r.range(4, 10), r.range(-1, 4)});
      fill(cv, v.stroke(1.1f, 0.4f), Solid{vein}, Blend::Atop);
    }
  }
}

void tuft(Canvas& cv, Color base, Color tip, u32 seed, int blades, f32 h0, f32 h1, f32 lean = 4.0f) {
  Rng r(seed);
  for (int i = 0; i < blades; i++) {
    f32 x = r.range(-5.5f, 5.5f);
    f32 h = r.range(h0, h1);
    f32 l = r.range(-lean, lean) + x * 0.3f;
    f32 w = r.range(1.4f, 2.1f);
    Vec2 b0{x - w * 0.5f, 0.5f}, b1{x + w * 0.5f, 0.5f}, t{x + l, -h};
    Path p;
    p.move(b0).quad({x + l * 0.35f - w * 0.3f, -h * 0.55f}, t).quad({x + l * 0.35f + w * 0.3f, -h * 0.5f}, b1).line(b0);
    fill(cv, p.poly(), Linear{{x, 0}, t, base, mix(tip, lighten(tip, 0.3f), r.unit() * 0.4f)});
  }
}

void thorn_wall(Canvas& cv, u32 seed, Color vine, Color vine_light, Color line, Color thorn, Color berry, Color berry_light) {
  Rng r(seed);
  fill(cv, Ellipse{{0, 0}, {24, 6}}, Solid{hex(0x1a1020, 0.4f)}, Blend::Over, 4);
  for (int i = 0; i < 16; i++) {
    Vec2 a0{r.range(-18, 18), r.range(-2, 2)};
    Vec2 a1{r.range(-22, 22), r.range(-36, -8)};
    Vec2 ctrl{(a0.x + a1.x) * 0.5f + r.range(-14, 14), (a0.y + a1.y) * 0.5f + r.range(-10, 6)};
    Path p;
    p.move(a0).quad(ctrl, a1, 12);
    f32 w = r.range(1.8f, 3.0f);
    fill(cv, p.stroke(w + 1.0f, 1.0f), Solid{line});
    fill(cv, p.stroke(w, 0.5f), Linear{a0, a1, vine, vine_light});
    for (int k = 2; k < p.pts.count - 1; k += 3) {
      Vec2 q = p.pts[k];
      Vec2 dir = normalize(p.pts[k + 1] - p.pts[k - 1]);
      Vec2 n = perp(dir) * ((k / 3) % 2 ? 1.0f : -1.0f);
      Path th;
      th.move(q - dir * 0.9f).line(q + n * 2.6f + dir * 0.6f).line(q + dir * 0.9f);
      fill(cv, th.poly(), Solid{thorn});
    }
  }
  for (int i = 0; i < 6; i++) {
    Vec2 p{r.range(-16, 16), r.range(-30, -8)};
    fill(cv, Circle{p, 2.0f}, Solid{line});
    fill(cv, Circle{p, 1.6f}, Volume{berry, berry_light, darken(berry, 0.5f), p + Vec2{-0.5f, -0.5f}, 2});
  }
}

void burnt_pile(Canvas& cv, Color ash) {
  Rng r(520);
  fill(cv, Ellipse{{0, 0}, {22, 6}}, Solid{hex(0x2a2420, 0.55f)}, Blend::Over, 3);
  fill(cv, wobble(Ellipse{{0, -1}, {17, 4.5f}}, 1.2f, 0.4f, 5), noisy(Solid{ash}, 0.08f, 0.9f));
  for (int i = 0; i < 9; i++) {
    Vec2 a0{r.range(-14, 14), r.range(-2, 1)};
    Vec2 a1 = a0 + Vec2{r.range(-5, 5), r.range(-12, -5)};
    Path p;
    p.move(a0).quad((a0 + a1) * 0.5f + Vec2{r.range(-2, 2), 0}, a1);
    fill(cv, p.stroke(1.8f, 0.6f), Solid{hex(0x1e1816)});
  }
  for (int i = 0; i < 5; i++) fill(cv, Circle{{r.range(-12, 12), r.range(-3, 0)}, 0.9f}, Solid{hex(0xff8a3a, 0.8f)});
}

/** Bare, twisted branches: dead trees in the marsh, the crags and Malgrin's land. */
void dead_tree(Canvas& cv, u32 seed, Color bark, Color line, f32 height, Color moss = {0, 0, 0, 0}) {
  Rng r(seed);
  paint_trunk(cv, height * 0.55f, 11, 6, bark, line, seed);
  struct B { Vec2 a; f32 ang, len, w; int depth; };
  FixedVec<B, 64> stack;
  stack.push({{0, -height * 0.5f}, -HALF_PI + r.range(-0.15f, 0.15f), height * 0.42f, 5.0f, 0});
  FixedVec<Vec2, 64> tips;
  while (!stack.empty()) {
    B b = stack.back();
    stack.count--;
    Vec2 e = b.a + from_angle(b.ang, b.len);
    Vec2 c = (b.a + e) * 0.5f + perp(from_angle(b.ang)) * r.range(-b.len * 0.2f, b.len * 0.2f);
    Path p;
    p.move(b.a).quad(c, e, 8);
    fill(cv, p.stroke(b.w + 1.1f, max(0.6f, b.w * 0.45f) + 1.1f), Solid{line});
    fill(cv, p.stroke(b.w, max(0.5f, b.w * 0.45f)), Linear{b.a + Vec2{-2, 0}, b.a + Vec2{2, 0}, lighten(bark, 0.2f), darken(bark, 0.3f)});
    if (b.depth < 3) {
      int kids = b.depth == 0 ? 3 : 2;
      for (int k = 0; k < kids; k++) stack.push({e, b.ang + r.range(-0.7f, 0.7f) + (k - (kids - 1) * 0.5f) * 0.5f, b.len * r.range(0.55f, 0.75f), b.w * 0.62f, b.depth + 1});
    } else {
      tips.push(e);
    }
  }
  if (moss.a > 0) {  // hanging moss
    for (Vec2 t : tips) {
      Path m;
      f32 len = r.range(6, 14);
      m.move(t).quad(t + Vec2{r.range(-2, 2), len * 0.5f}, t + Vec2{r.range(-2, 2), len});
      fill(cv, m.stroke(1.6f, 0.4f), Solid{moss});
    }
  }
}

void crystal(Canvas& cv, Vec2 base, f32 h, f32 w, f32 lean, Color c, Color line) {
  Vec2 tip = base + Vec2{lean, -h};
  Path p;
  p.move(base + Vec2{-w, 0}).line(base + Vec2{-w * 0.9f + lean * 0.7f, -h * 0.75f}).line(tip).line(base + Vec2{w * 0.9f + lean * 0.7f, -h * 0.75f}).line(base + Vec2{w, 0});
  Poly sh = p.poly();
  fill(cv, grow(sh, 0.8f), Solid{line});
  fill(cv, sh, Linear{base + Vec2{-w, 0}, base + Vec2{w, 0}, lighten(c, 0.45f), darken(c, 0.3f)});
  Path facet;
  facet.move(base + Vec2{lean * 0.1f, 0}).line(tip);
  fill(cv, facet.stroke(0.6f), Solid{lighten(c, 0.6f).with_alpha(0.8f)}, Blend::Atop);
}

void crystal_cluster(Canvas& cv, u32 seed, Color c, Color line, f32 size) {
  Rng r(seed);
  fill(cv, Ellipse{{0, 0}, {10 * size, 3.5f * size}}, Solid{hex(0x101010, 0.35f)}, Blend::Over, 2);
  fill(cv, Ellipse{{0, -6 * size}, {12 * size, 10 * size}}, Solid{c.with_alpha(0.25f)}, Blend::Over, 8 * size);
  for (int i = 0; i < 5; i++) {
    f32 x = r.range(-6, 6) * size;
    crystal(cv, {x, 0.5f}, r.range(8, 18) * size * (i == 2 ? 1.3f : 1.0f), r.range(2.0f, 3.2f) * size, x * 0.35f, c, line);
  }
}

void mushroom_big(Canvas& cv, Color cap, Color spots, Color stem_c, f32 h, f32 cap_r, u32 seed) {
  Capsule st{{0, 0}, {0, -h}, cap_r * 0.28f, cap_r * 0.2f};
  fill(cv, grow(st, 0.8f), Solid{hex(0x2a2018)});
  fill(cv, st, Linear{{-cap_r * 0.3f, 0}, {cap_r * 0.3f, 0}, stem_c, darken(stem_c, 0.25f)});
  Path c;
  c.move({-cap_r, -h + 1}).cubic({-cap_r, -h - cap_r * 1.1f}, {cap_r, -h - cap_r * 1.1f}, {cap_r, -h + 1}).quad({0, -h + cap_r * 0.35f}, {-cap_r, -h + 1});
  fill(cv, grow(c.poly(), 0.9f), Solid{darken(cap, 0.65f)});
  fill(cv, c.poly(), Volume{cap, lighten(cap, 0.4f), darken(cap, 0.35f), {-cap_r * 0.3f, -h - cap_r * 0.4f}, cap_r * 1.2f, 1.5f, 0.3f});
  Rng r(seed);
  for (int k = 0; k < 6; k++) fill(cv, Ellipse{{r.range(-cap_r * 0.7f, cap_r * 0.7f), -h - r.range(0.5f, cap_r * 0.6f)}, {cap_r * 0.12f, cap_r * 0.08f}}, Solid{spots}, Blend::Atop);
}

void fern(Canvas& cv, Color base, Color tip, u32 seed, f32 size) {
  Rng r(seed);
  for (int f = 0; f < 6; f++) {
    f32 ang = -HALF_PI + (f - 2.5f) * 0.42f + r.range(-0.1f, 0.1f);
    Vec2 end = from_angle(ang, size * r.range(0.8f, 1.05f));
    Vec2 ctrl = end * 0.55f + Vec2{0, -size * 0.25f};
    Path rib;
    rib.move({0, 0}).quad(ctrl, end, 10);
    fill(cv, rib.stroke(0.8f, 0.3f), Solid{darken(base, 0.3f)});
    for (int k = 1; k < rib.pts.count; k++) {
      Vec2 q = rib.pts[k];
      Vec2 d = normalize(rib.pts[k] - rib.pts[k - 1]);
      f32 len = size * 0.22f * (1.0f - k / (f32)rib.pts.count);
      for (int s = -1; s <= 1; s += 2) {
        Ellipse leaf{q + perp(d) * (s * len * 0.6f), {len * 0.6f, len * 0.22f}, atan2f(d.y, d.x) + s * 1.1f};
        fill(cv, leaf, Linear{{0, 0}, end, base, tip});
      }
    }
  }
}

// ---------------------------------------------------------------- Mistfen Marsh
void willow(Canvas& cv, u32 seed) {
  paint_trunk(cv, 30, 14, 9, hex(0x5a4a3a), hex(0x2a2018), seed);
  Blob b;
  b.k = 4;
  Vec2 c{0, -58};
  b.add(c, 24).add(c + Vec2{-20, 6}, 17).add(c + Vec2{20, 5}, 17).add(c + Vec2{-10, -14}, 16).add(c + Vec2{11, -14}, 16).add(c + Vec2{0, 12}, 16);
  Foliage f{hex(0x7aa04a), hex(0xc8e07a), hex(0x3f6a2e), hex(0x24401c)};
  paint_foliage(cv, b, c + Vec2{-4, -6}, 40, f, seed, 0.7f);
  Rng r(seed + 5);
  for (int i = 0; i < 46; i++) {  // the drooping fronds that make a willow a willow
    f32 x = r.range(-34, 34);
    f32 top = c.y + r.range(-6, 14) + absf(x) * 0.25f;
    f32 len = r.range(18, 34);
    Path p;
    p.move({x, top}).quad({x + r.range(-3, 3), top + len * 0.5f}, {x + r.range(-4, 4), top + len});
    Color col = r.chance(0.5f) ? hex(0x9cc45a) : hex(0x6a9a42);
    fill(cv, p.stroke(2.2f, 0.6f), Solid{darken(col, 0.45f)});
    fill(cv, p.stroke(1.5f, 0.4f), Solid{col});
  }
}

void bake_marsh_art(Atlas& a) {
  RegionArt& R = S.ra;
  u16 willow0 = a.bake({-46, -100, 46, 6}, 2.0f, [](Canvas& cv) { willow(cv, 801); });
  u16 willow1 = a.bake({-46, -100, 46, 6}, 2.0f, [](Canvas& cv) { willow(cv, 815); });
  u16 dead = a.bake({-40, -104, 40, 6}, 2.0f, [](Canvas& cv) { dead_tree(cv, 830, hex(0x6a6a5a), hex(0x2a2a22), 96, hex(0x8aa05a, 0.85f)); });
  u16 shroom_t = a.bake({-24, -60, 24, 4}, 2.5f, [](Canvas& cv) { mushroom_big(cv, hex(0x4ab8b0), hex(0xb8fff0), hex(0xe8e0c8), 38, 18, 840); });
  u16 shroom_p = a.bake({-20, -50, 20, 4}, 2.5f, [](Canvas& cv) { mushroom_big(cv, hex(0x9a5ad0), hex(0xf0c8ff), hex(0xe0d8c8), 30, 15, 841); });
  Stone mossy{hex(0x7a8278), hex(0xb0b8a8), hex(0x46504a), hex(0x242a26)};
  u16 rock0 = a.bake({-26, -36, 26, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 20, 15, mossy, 850, hex(0x7aa04a), 1.0f); });
  u16 rock1 = a.bake({-18, -26, 18, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 13, 10, mossy, 851, hex(0x7aa04a), 1.1f); });
  u16 tuft0 = a.bake({-10, -22, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x4a5a2a), hex(0xa8b05a), 860, 8, 9, 17); });
  u16 tuft1 = a.bake({-10, -22, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x3a5a32), hex(0x8ab05a), 861, 7, 8, 15); });
  u16 tuft2 = a.bake({-10, -22, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x5a5a3a), hex(0xc0b070), 862, 6, 10, 18); });
  u16 iris = a.bake({-9, -22, 9, 2}, 3.0f, [](Canvas& cv) {
    for (int i = 0; i < 3; i++) {
      Vec2 h{-3.5f + i * 3.5f, -13.0f - (i == 1) * 3.0f};
      stem(cv, {(f32)(i - 1), 0}, h, (f32)(i - 1) * 0.6f, 0.8f);
      for (int k = 0; k < 3; k++) {
        f32 ang = -HALF_PI + (k - 1) * 1.1f;
        Ellipse pe{h + from_angle(ang, 1.8f), {2.0f, 1.0f}, ang};
        fill(cv, grow(pe, 0.35f), Solid{hex(0x6a4a10)});
        fill(cv, pe, Solid{hex(0xffd23a)});
      }
    }
  });
  u16 cotton = a.bake({-9, -22, 9, 2}, 3.0f, [](Canvas& cv) {
    Rng r(870);
    for (int i = 0; i < 4; i++) {
      Vec2 h{r.range(-5, 5), r.range(-17, -10)};
      stem(cv, {h.x * 0.3f, 0}, h, r.range(-1, 1), 0.6f);
      fill(cv, Circle{h, 2.2f}, Solid{hex(0xfafaf2)}, Blend::Over, 1.0f);
    }
  });
  u16 reed = a.bake({-12, -40, 12, 2}, 2.5f, [](Canvas& cv) {
    Rng r(880);
    for (int i = 0; i < 9; i++) {
      f32 x = r.range(-7, 7), h = r.range(22, 36);
      Vec2 tip{x + r.range(-4, 4), -h};
      Path p;
      p.move({x - 0.9f, 0.5f}).quad({x, -h * 0.5f}, tip).quad({x + 0.3f, -h * 0.5f}, {x + 0.9f, 0.5f});
      fill(cv, p.poly(), Linear{{x, 0}, tip, hex(0x3d5f3a), hex(0xb0c06a)});
    }
    for (int i = 0; i < 3; i++) {
      f32 x = r.range(-4, 4);
      Vec2 top{x, -r.range(28, 36)};
      Capsule head{top + Vec2{0, 2}, top + Vec2{0, 9}, 1.8f, 1.8f};
      fill(cv, grow(head, 0.4f), Solid{hex(0x2a1810)});
      fill(cv, head, Volume{hex(0x7a4a2a), hex(0xb07a4a), hex(0x4a2a18), top + Vec2{-1, 4}, 4});
    }
  });
  u16 lily = a.bake({-12, -9, 12, 9}, 3.0f, [](Canvas& cv) {
    Ellipse pad{{0, 0}, {10, 6.5f}};
    Path notch;
    notch.move({0, 0}).line({10.5f, -2.2f}).line({10.5f, 1.4f});
    auto leaf = subtract(pad, notch.poly());
    fill(cv, grow(leaf, 0.7f), Solid{hex(0x1d3a24, 0.9f)});
    fill(cv, leaf, Volume{hex(0x5a8a3a), hex(0x9ac06a), hex(0x2f5a2a), {-3, -2}, 11, 2.0f, 0.3f});
  });
  u16 log = a.bake({-30, -18, 30, 4}, 2.5f, [](Canvas& cv) {
    fill(cv, Ellipse{{0, 0}, {26, 5}}, Solid{hex(0x1d3022, 0.35f)}, Blend::Over, 4);
    Capsule body{{-22, -7}, {20, -7}, 7.5f, 7.5f};
    fill(cv, grow(body, 1.1f), Solid{hex(0x2a221a)});
    fill(cv, body, noisy(Linear{{0, -14}, {0, 0}, hex(0x7a6a52), hex(0x4a3e30)}, 0.07f, 0.5f));
    fill(cv, wobble(Ellipse{{-2, -12}, {16, 4.5f}}, 1.5f, 0.5f, 3), noisy(Solid{hex(0x7aa04a)}, 0.07f, 0.9f), Blend::Atop);
    for (int k = 0; k < 3; k++) fill(cv, Circle{{-12.0f + k * 9, -14.0f}, 2.0f}, Solid{hex(0xd8c8a0)}, Blend::Over);
  });
  u16 small_shroom = a.bake({-10, -16, 10, 3}, 3.0f, [](Canvas& cv) {
    for (int i = 0; i < 3; i++) {
      Canvas& c = cv;
      f32 x = (i - 1) * 5.0f;
      Capsule st{{x, 0}, {x, -6.0f + i}, 1.2f, 0.9f};
      fill(c, st, Solid{hex(0xe8e0c8)});
      fill(c, Ellipse{{x, -6.5f + i}, {3.2f, 2.2f}}, Volume{hex(0x4ab8b0), hex(0xa8fff0), hex(0x2a7a74), {x - 1, -8}, 3});
    }
  });
  u16 bl0 = a.bake({-28, -42, 28, 6}, 2.5f, [](Canvas& cv) { thorn_wall(cv, 890, hex(0x3e3a2a), hex(0x7a7448), hex(0x15140c), hex(0xd8d0a8), hex(0x5a7a1a), hex(0xc0e05a)); });
  u16 bl1 = a.bake({-28, -42, 28, 6}, 2.5f, [](Canvas& cv) { thorn_wall(cv, 891, hex(0x3e3a2a), hex(0x7a7448), hex(0x15140c), hex(0xd8d0a8), hex(0x5a7a1a), hex(0xc0e05a)); });
  u16 burnt = a.bake({-26, -18, 26, 6}, 2.5f, [](Canvas& cv) { burnt_pile(cv, hex(0x5a5a50)); });

  const Color teal{0.4f, 1.0f, 0.9f, 1}, violet{0.75f, 0.5f, 1.0f, 1};
  R.edge.add(willow0, 0.35f).r(11).col({0, -3}).sw(0.18f).sh(2.4f).sc(0.85f, 1.15f).noflip().rev(42, 100);
  R.edge.add(willow1, 0.35f).r(11).col({0, -3}).sw(0.18f).sh(2.4f).sc(0.85f, 1.15f).noflip().rev(42, 100);
  R.edge.add(dead, 0.3f).r(9).col({0, -3}).sw(0.04f).sh(1.6f).sc(0.85f, 1.15f).rev(34, 100);
  R.trees.add(willow0, 0.3f).r(11).col({0, -3}).sw(0.18f).sh(2.6f).noflip().rev(42, 100);
  R.trees.add(willow1, 0.3f).r(11).col({0, -3}).sw(0.18f).sh(2.6f).noflip().rev(42, 100);
  R.trees.add(dead, 0.2f).r(9).col({0, -3}).sw(0.04f).sh(1.6f).rev(34, 100);
  R.trees.add(shroom_t, 0.12f).r(7).col({0, -2}).sh(1.0f).glow(70, teal).em(EMIT_SPORES);
  R.trees.add(shroom_p, 0.08f).r(6).col({0, -2}).sh(0.9f).glow(60, violet).em(EMIT_SPORES);
  R.bushes.add(tuft0, 1).sc(1.6f, 2.0f).sw(0.5f);
  R.rocks.add(rock0, 0.5f).r(14).col({0, -5}).sh(1.0f).noflip().sc(1, 1);
  R.rocks.add(rock1, 0.5f).r(10).col({0, -4}).sh(0.8f).noflip().sc(1, 1);
  R.cover.add(tuft0, 1).sw(0.6f).sc(0.8f, 1.2f);
  R.cover.add(tuft1, 1).sw(0.6f).sc(0.8f, 1.2f);
  R.cover.add(tuft2, 0.6f).sw(0.6f).sc(0.8f, 1.2f);
  R.flowers.add(iris, 1).sw(0.45f);
  R.flowers.add(cotton, 1).sw(0.5f);
  R.shore.add(reed, 1).sw(0.55f).sc(0.8f, 1.15f);
  R.floaters.add(lily, 1).flat().sc(0.8f, 1.2f);
  R.extras.add(log, 1).r(14).col({-6, -6}).sc(1, 1);
  R.extras.add(shroom_t, 0.6f).r(7).col({0, -2}).sh(1.0f).glow(70, teal).em(EMIT_SPORES);
  R.near_trees.add(small_shroom, 1).glow(26, teal).sc(0.9f, 1.1f);
  R.blocker[0] = bl0;
  R.blocker[1] = bl1;
  R.blocker_burnt = burnt;
}

// ---------------------------------------------------------------- Cinderpeak Crags
void spire(Canvas& cv, u32 seed, f32 h) {
  Rng r(seed);
  fill(cv, Ellipse{{0, 0}, {20, 6}}, Solid{hex(0x0a0808, 0.45f)}, Blend::Over, 4);
  for (int i = 0; i < 4; i++) {
    f32 x = (i - 1.5f) * 8.0f + r.range(-2, 2);
    f32 top = -h * r.range(0.6f, 1.0f);
    Path p;
    p.move({x - 5, 0}).line({x - 4.5f, top + 3}).line({x - 1.5f, top}).line({x + 3.5f, top + 1.5f}).line({x + 5, 0});
    Poly col = p.poly();
    fill(cv, grow(col, 1.0f), Solid{hex(0x0e0a0a)});
    fill(cv, col, Linear{{x - 5, 0}, {x + 5, 0}, hex(0x5a5052), hex(0x231e20)});
    fill(cv, RBox{{x - 0.5f, top + 2}, {4.5f, 1.4f}, 0.5f, 0.1f}, Solid{hex(0x7a6e6e)}, Blend::Atop);
    Path crack;
    crack.move({x, top + 8}).line({x + 1.5f, top + 16}).line({x - 0.5f, top + 24});
    fill(cv, crack.stroke(1.2f, 0.5f), Solid{hex(0xff7a2a)}, Blend::Atop);
  }
}

void bake_crags_art(Atlas& a) {
  RegionArt& R = S.ra;
  u16 spire0 = a.bake({-24, -80, 24, 6}, 2.0f, [](Canvas& cv) { spire(cv, 900, 74); });
  u16 spire1 = a.bake({-24, -64, 24, 6}, 2.0f, [](Canvas& cv) { spire(cv, 901, 58); });
  u16 charred = a.bake({-40, -96, 40, 6}, 2.0f, [](Canvas& cv) { dead_tree(cv, 910, hex(0x2a2224), hex(0x0a0808), 88); });
  Stone basalt{hex(0x4a4446), hex(0x7a7072), hex(0x221e20), hex(0x0c0a0a)};
  u16 rock0 = a.bake({-26, -36, 26, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 20, 15, basalt, 920, {}, 0, hex(0xff8a3a)); });
  u16 rock1 = a.bake({-18, -26, 18, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 13, 10, basalt, 921, {}, 0, hex(0xff8a3a)); });
  u16 rock2 = a.bake({-34, -46, 34, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 28, 20, basalt, 922, {}, 0, hex(0xff8a3a)); });
  u16 tuft0 = a.bake({-10, -20, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x4a4036), hex(0x9a8a6a), 930, 6, 7, 13); });
  u16 tuft1 = a.bake({-10, -20, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x3a3430), hex(0x7a6a5a), 931, 5, 6, 11); });
  u16 crys0 = a.bake({-16, -36, 16, 4}, 2.5f, [](Canvas& cv) { crystal_cluster(cv, 940, hex(0xff9a3a), hex(0x4a1a08), 1.2f); });
  u16 crys1 = a.bake({-14, -28, 14, 4}, 2.5f, [](Canvas& cv) { crystal_cluster(cv, 941, hex(0xffc84a), hex(0x4a2a08), 0.9f); });
  u16 lily = a.bake({-9, -16, 9, 2}, 3.0f, [](Canvas& cv) {  // fire lilies
    for (int i = 0; i < 2; i++) {
      Vec2 h{-2.5f + i * 5.0f, -12.0f + i * 2.0f};
      stem(cv, {(f32)i - 0.5f, 0}, h, i ? 1.0f : -1.0f, 0.8f);
      for (int k = 0; k < 5; k++) {
        f32 ang = -HALF_PI + (k - 2) * 0.55f;
        Ellipse pe{h + from_angle(ang, 2.0f), {2.2f, 0.8f}, ang};
        fill(cv, grow(pe, 0.3f), Solid{hex(0x5a1008)});
        fill(cv, pe, Linear{h, h + from_angle(ang, 4), hex(0xffd23a), hex(0xff4a1a)});
      }
    }
  });
  u16 shard = a.bake({-12, -18, 12, 3}, 3.0f, [](Canvas& cv) {
    Rng r(950);
    for (int i = 0; i < 4; i++) crystal(cv, {r.range(-7, 7), 0.5f}, r.range(6, 13), r.range(1.6f, 2.4f), r.range(-3, 3), hex(0x2a2430), hex(0x0a080c));
  });
  u16 vent_smoke = a.bake({-14, -12, 14, 6}, 2.5f, [](Canvas& cv) {
    fill(cv, wobble(Ellipse{{0, 0}, {11, 4.5f}}, 1.2f, 0.4f, 6), Solid{hex(0x1a1416)});
    fill(cv, Ellipse{{0, 0}, {5, 1.8f}}, Solid{hex(0xff8a3a, 0.9f)}, Blend::Over, 1.5f);
    for (int i = 0; i < 5; i++) fill(cv, Ellipse{from_angle(i * 1.3f, 8) * Vec2{1, 0.4f}, {2.4f, 1.6f}}, Volume{hex(0x6a6062), hex(0x9a9092), hex(0x3a3436), {-1, -1}, 3});
  });
  u16 bl0 = a.bake({-28, -42, 28, 6}, 2.5f, [](Canvas& cv) { thorn_wall(cv, 960, hex(0x2a2226), hex(0x5a4a52), hex(0x0a0808), hex(0xffb070), hex(0x8a2a10), hex(0xff8a3a)); });
  u16 bl1 = a.bake({-28, -42, 28, 6}, 2.5f, [](Canvas& cv) { thorn_wall(cv, 961, hex(0x2a2226), hex(0x5a4a52), hex(0x0a0808), hex(0xffb070), hex(0x8a2a10), hex(0xff8a3a)); });
  u16 burnt = a.bake({-26, -18, 26, 6}, 2.5f, [](Canvas& cv) { burnt_pile(cv, hex(0x4a4446)); });

  const Color ember{1.0f, 0.55f, 0.2f, 1}, gold{1.0f, 0.75f, 0.3f, 1};
  R.edge.add(spire0, 0.5f).r(16).col({0, -4}).sh(1.8f).sc(0.9f, 1.2f).rev(26, 76);
  R.edge.add(spire1, 0.5f).r(16).col({0, -4}).sh(1.8f).sc(0.9f, 1.2f).rev(26, 60);
  R.trees.add(charred, 0.6f).r(8).col({0, -3}).sh(1.4f).rev(34, 92).em(EMIT_EMBERS);
  R.trees.add(spire1, 0.4f).r(16).col({0, -4}).sh(1.8f).sc(0.8f, 1.0f).rev(26, 60);
  R.bushes.add(crys0, 1).r(10).col({0, -3}).glow(80, ember).sc(0.9f, 1.15f);
  R.bushes.add(crys1, 1).r(8).col({0, -3}).glow(64, gold).sc(0.9f, 1.15f);
  R.rocks.add(rock0, 0.4f).r(14).col({0, -5}).sh(1.0f).noflip().sc(1, 1);
  R.rocks.add(rock1, 0.4f).r(10).col({0, -4}).sh(0.8f).noflip().sc(1, 1);
  R.rocks.add(rock2, 0.2f).r(22).col({0, -6}).sh(1.6f).noflip().sc(1, 1);
  R.cover.add(tuft0, 1).sw(0.35f).sc(0.8f, 1.2f);
  R.cover.add(tuft1, 1).sw(0.35f).sc(0.8f, 1.2f);
  R.flowers.add(lily, 1).sw(0.35f).glow(18, ember);
  R.shore.add(shard, 1).sc(0.8f, 1.2f);
  R.extras.add(vent_smoke, 1).flat().em(EMIT_SMOKE).glow(40, ember).sc(1, 1);
  R.extras.add(crys0, 0.6f).r(10).col({0, -3}).glow(80, ember);
  R.path_bits.add(shard, 1).sc(0.5f, 0.7f).flat();
  R.blocker[0] = bl0;
  R.blocker[1] = bl1;
  R.blocker_burnt = burnt;
}

// ---------------------------------------------------------------- the Elderwood
void ancient_tree(Canvas& cv, u32 seed) {
  // roots spreading over the ground
  Rng r(seed);
  for (int i = 0; i < 5; i++) {
    f32 dir = (i - 2) * 0.55f;
    Path root;
    root.move({dir * 6, -6}).quad({dir * 16, -1}, {dir * 30 + r.range(-3, 3), 2});
    fill(cv, root.stroke(6.0f, 1.5f), Solid{hex(0x2a1c12)});
    fill(cv, root.stroke(4.5f, 1.0f), Solid{hex(0x6a4a30)});
  }
  paint_trunk(cv, 64, 26, 17, hex(0x5e4632), hex(0x24180e), seed);
  fill(cv, Ellipse{{-3, -30}, {3, 5}}, Solid{hex(0x1a120a)});  // a hollow
  fill(cv, wobble(Ellipse{{4, -14}, {9, 12}}, 1.5f, 0.5f, seed), noisy(Solid{hex(0x5a8a3a)}, 0.08f, 0.8f), Blend::Atop);
  Blob b;
  b.k = 5;
  Vec2 c{0, -108};
  b.add(c, 38).add(c + Vec2{-34, 12}, 28).add(c + Vec2{34, 10}, 28).add(c + Vec2{-22, -24}, 26).add(c + Vec2{22, -26}, 27);
  b.add(c + Vec2{0, -40}, 22).add(c + Vec2{-46, -6}, 18).add(c + Vec2{46, -8}, 18).add(c + Vec2{-12, 28}, 22).add(c + Vec2{14, 26}, 22);
  Foliage f{hex(0x3f7a3a), hex(0x8ac25a), hex(0x1a4026), hex(0x0f2a18)};
  paint_foliage(cv, b, c + Vec2{-6, -10}, 66, f, seed, 0.8f);
}

void bake_elderwood_art(Atlas& a) {
  RegionArt& R = S.ra;
  u16 ancient0 = a.bake({-70, -180, 70, 8}, 1.6f, [](Canvas& cv) { ancient_tree(cv, 1001); });
  u16 ancient1 = a.bake({-70, -180, 70, 8}, 1.6f, [](Canvas& cv) { ancient_tree(cv, 1017); });
  u16 oak = a.bake({-50, -116, 50, 6}, 2.0f, [](Canvas& cv) {
    paint_trunk(cv, 40, 13, 8, hex(0x5a4030), hex(0x24180e), 1030);
    Blob b;
    b.k = 3.5f;
    Vec2 c{0, -70};
    b.add(c, 24).add(c + Vec2{-21, 8}, 18).add(c + Vec2{21, 7}, 18).add(c + Vec2{-14, -15}, 18).add(c + Vec2{14, -17}, 18.5f).add(c + Vec2{0, -27}, 15).add(c + Vec2{-6, 17}, 16).add(c + Vec2{9, 16}, 15);
    paint_foliage(cv, b, c + Vec2{-4, -6}, 44, Foliage{hex(0x3a7038), hex(0x7ab050), hex(0x173a22), hex(0x0f2a18)}, 1030);
  });
  u16 fern0 = a.bake({-22, -26, 22, 3}, 2.5f, [](Canvas& cv) { fern(cv, hex(0x3a7a3a), hex(0x8ac85a), 1040, 20); });
  u16 fern1 = a.bake({-16, -20, 16, 3}, 3.0f, [](Canvas& cv) { fern(cv, hex(0x4a8a3a), hex(0xa0d86a), 1041, 14); });
  Stone mossy{hex(0x6a7268), hex(0xa0a898), hex(0x3a423c), hex(0x1c221e)};
  u16 rock0 = a.bake({-26, -36, 26, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 20, 15, mossy, 1050, hex(0x5a9a42), 1.3f); });
  u16 rock1 = a.bake({-18, -26, 18, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 13, 10, mossy, 1051, hex(0x5a9a42), 1.4f); });
  u16 moss = a.bake({-10, -14, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x2a5a2a), hex(0x6aa84a), 1060, 7, 5, 10, 2); });
  u16 clover = a.bake({-10, -12, 10, 2}, 3.0f, [](Canvas& cv) {
    Rng r(1061);
    for (int k = 0; k < 5; k++) {
      Vec2 c{r.range(-6, 6), r.range(-6, -1)};
      for (int j = 0; j < 3; j++) fill(cv, Circle{c + from_angle(j * TAU / 3 - HALF_PI, 1.2f), 1.25f}, Solid{hex(0x4a9a46)});
    }
  });
  u16 glowbell = a.bake({-9, -20, 9, 2}, 3.0f, [](Canvas& cv) {
    for (int i = 0; i < 3; i++) {
      Vec2 h{-4.0f + i * 4.0f, -12.0f - (i == 1) * 3.0f};
      stem(cv, {(f32)(i - 1), 0}, h, 0.5f, 0.7f);
      fill(cv, Circle{h, 3.2f}, Solid{hex(0x8ae0ff, 0.35f)}, Blend::Over, 2.5f);
      fill(cv, Ellipse{h, {1.8f, 2.2f}}, Volume{hex(0x6ad0ff), hex(0xe0fbff), hex(0x2a8ad8), h + Vec2{-0.6f, -0.6f}, 2.5f});
    }
  });
  u16 bluebell = a.bake({-9, -20, 9, 2}, 3.0f, [](Canvas& cv) {
    Path p;
    p.move({0, 0}).quad({1, -9}, {4, -15});
    fill(cv, p.stroke(0.85f, 0.6f), Solid{hex(0x4f8f3a)});
    Vec2 bells[3] = {{1.2f, -9}, {2.6f, -12.2f}, {4.4f, -14.6f}};
    for (Vec2 b : bells) fill(cv, Ellipse{b + Vec2{0, 1.6f}, {1.7f, 1.9f}}, Linear{b, b + Vec2{0, 3.4f}, hex(0x9caaff), hex(0x5a64d8)});
  });
  u16 shrooms = a.bake({-14, -20, 14, 3}, 3.0f, [](Canvas& cv) {
    for (int i = 0; i < 3; i++) {
      f32 x = (i - 1) * 6.0f;
      f32 h = 9.0f - i * 1.5f + (i == 1) * 3.0f;
      Capsule st{{x, 0}, {x, -h}, 1.3f, 1.0f};
      fill(cv, st, Solid{hex(0xd8f0f0)});
      Path c;
      c.move({x - 4.5f, -h + 0.5f}).cubic({x - 4.5f, -h - 5}, {x + 4.5f, -h - 5}, {x + 4.5f, -h + 0.5f}).line({x - 4.5f, -h + 0.5f});
      fill(cv, grow(c.poly(), 0.6f), Solid{hex(0x0a3a5a)});
      fill(cv, c.poly(), Volume{hex(0x4ab8ff), hex(0xc8f4ff), hex(0x1a6ab8), {x - 1.5f, -h - 2}, 5});
    }
  });
  u16 log = a.bake({-40, -22, 40, 4}, 2.0f, [](Canvas& cv) {
    fill(cv, Ellipse{{0, 0}, {34, 6}}, Solid{hex(0x0d1a12, 0.4f)}, Blend::Over, 4);
    Capsule body{{-30, -9}, {28, -9}, 9.5f, 9.5f};
    fill(cv, grow(body, 1.1f), Solid{hex(0x1e140c)});
    fill(cv, body, noisy(Linear{{0, -18}, {0, 0}, hex(0x7a5a3e), hex(0x3e2a1c)}, 0.07f, 0.5f));
    fill(cv, wobble(Ellipse{{-4, -16}, {22, 5}}, 1.8f, 0.5f, 7), noisy(Solid{hex(0x5a9a42)}, 0.08f, 0.9f), Blend::Atop);
    Ellipse end{{29, -9}, {5.5f, 9.2f}};
    fill(cv, grow(end, 0.8f), Solid{hex(0x1e140c)});
    fill(cv, end, Solid{hex(0xc8a070)});
    for (int k = 1; k < 4; k++) fill(cv, grow(Ellipse{{29, -9}, {5.5f - k * 1.4f, 9.2f - k * 2.3f}}, 0.25f), Solid{hex(0x8a6a44, 0.6f)});
  });
  u16 bl0 = a.bake({-28, -42, 28, 6}, 2.5f, [](Canvas& cv) { thorn_wall(cv, 1070, hex(0x3a2a34), hex(0x6a4a5a), hex(0x120a10), hex(0xd8c8b0), hex(0x5a1a6e), hex(0xb46bd8)); });
  u16 bl1 = a.bake({-28, -42, 28, 6}, 2.5f, [](Canvas& cv) { thorn_wall(cv, 1071, hex(0x3a2a34), hex(0x6a4a5a), hex(0x120a10), hex(0xd8c8b0), hex(0x5a1a6e), hex(0xb46bd8)); });
  u16 burnt = a.bake({-26, -18, 26, 6}, 2.5f, [](Canvas& cv) { burnt_pile(cv, hex(0x5a544c)); });

  const Color blue{0.4f, 0.8f, 1.0f, 1};
  R.edge.add(ancient0, 0.5f).r(18).col({0, -6}).sw(0.06f).sh(3.6f).sc(0.85f, 1.1f).noflip().rev(64, 180).em(EMIT_FIREFLIES);
  R.edge.add(ancient1, 0.5f).r(18).col({0, -6}).sw(0.06f).sh(3.6f).sc(0.85f, 1.1f).noflip().rev(64, 180);
  R.trees.add(ancient0, 0.35f).r(18).col({0, -6}).sw(0.06f).sh(3.6f).noflip().rev(64, 180).em(EMIT_FIREFLIES);
  R.trees.add(ancient1, 0.25f).r(18).col({0, -6}).sw(0.06f).sh(3.6f).noflip().rev(64, 180);
  R.trees.add(oak, 0.4f).r(9).col({0, -3}).sw(0.12f).sh(2.6f).noflip().rev(40, 112);
  R.bushes.add(fern0, 1).sw(0.25f).sc(0.9f, 1.2f);
  R.bushes.add(fern1, 1).sw(0.25f).sc(0.9f, 1.2f);
  R.rocks.add(rock0, 0.5f).r(14).col({0, -5}).sh(1.0f).noflip().sc(1, 1);
  R.rocks.add(rock1, 0.5f).r(10).col({0, -4}).sh(0.8f).noflip().sc(1, 1);
  R.cover.add(moss, 1).sw(0.3f).sc(0.8f, 1.2f);
  R.cover.add(clover, 0.7f).sc(0.8f, 1.2f);
  R.cover.add(fern1, 0.4f).sw(0.25f).sc(0.6f, 0.8f);
  R.flowers.add(glowbell, 1).sw(0.35f).glow(30, blue);
  R.flowers.add(bluebell, 1.5f).sw(0.4f);
  R.shore.add(fern1, 1).sw(0.3f);
  R.extras.add(log, 0.6f).r(18).col({-6, -8}).sc(1, 1);
  R.extras.add(shrooms, 1).glow(50, blue).sc(0.9f, 1.1f);
  R.near_trees.add(shrooms, 1).glow(46, blue).sc(0.8f, 1.0f);
  R.near_trees.add(fern1, 1).sw(0.25f);
  R.blocker[0] = bl0;
  R.blocker[1] = bl1;
  R.blocker_burnt = burnt;
}

// ---------------------------------------------------------------- Skyreach Cliffs
void bent_pine(Canvas& cv, u32 seed) {
  Rng r(seed);
  Path trunk;
  trunk.move({-3, 1}).quad({-2, -26}, {14, -50}).line({17, -48}).quad({3, -26}, {4, 1}).line({-3, 1});
  fill(cv, grow(trunk.poly(), 1.0f), Solid{hex(0x2a1c12)});
  fill(cv, trunk.poly(), Linear{{-3, 0}, {4, 0}, hex(0x9a6a44), hex(0x5a3a24)});
  Color base = hex(0x3f7a5a), light = hex(0x8ac08a), dark = hex(0x1f4a36), line = hex(0x12301f);
  Vec2 pads[5] = {{8, -48}, {-2, -40}, {20, -54}, {12, -34}, {-6, -30}};
  f32 rad[5] = {13, 10, 10, 9, 8};
  for (int i = 0; i < 5; i++) {
    Ellipse pad{pads[i], {rad[i] * 1.5f, rad[i] * 0.6f}, -0.15f};
    auto sh = wobble(pad, 1.2f, 0.5f, seed + i);
    fill(cv, grow(sh, 1.1f), Solid{line});
    fill(cv, sh, noisy(Volume{base, light, dark, pads[i] + Vec2{-3, -3}, rad[i] * 1.5f, 2.5f, 0.4f, TO_LIGHT}, 0.04f, 0.5f, seed));
  }
}

void bake_skyreach_art(Atlas& a) {
  RegionArt& R = S.ra;
  u16 pine0 = a.bake({-30, -72, 42, 6}, 2.0f, [](Canvas& cv) { bent_pine(cv, 1101); });
  u16 pine1 = a.bake({-30, -72, 42, 6}, 2.0f, [](Canvas& cv) { bent_pine(cv, 1115); });
  Stone granite{hex(0xb8b4ac), hex(0xeae6de), hex(0x7a766e), hex(0x3a3630)};
  u16 rock0 = a.bake({-26, -36, 26, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 20, 15, granite, 1120, hex(0x9ac06a), 0.6f); });
  u16 rock1 = a.bake({-18, -26, 18, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 13, 10, granite, 1121, hex(0x9ac06a), 0.5f); });
  u16 rock2 = a.bake({-34, -46, 34, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 28, 20, granite, 1122, hex(0x9ac06a), 0.5f); });
  u16 tuft0 = a.bake({-10, -22, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x6a9a4a), hex(0xe0e8a0), 1130, 7, 9, 17, 7); });
  u16 tuft1 = a.bake({-10, -22, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x7aa05a), hex(0xf0eab0), 1131, 6, 8, 15, 7); });
  u16 heather = a.bake({-20, -24, 20, 3}, 2.5f, [](Canvas& cv) {
    Blob b;
    b.k = 2.5f;
    b.add({0, -9}, 9).add({-8, -5}, 7).add({8, -5}, 7).add({0, -3}, 8);
    paint_foliage(cv, b, {-2, -10}, 14, Foliage{hex(0x6a7a4a), hex(0xa8b87a), hex(0x3a4a2a), hex(0x22301a)}, 1140, 0.6f);
    Rng r(1141);
    for (int i = 0; i < 28; i++) {
      Vec2 p{r.range(-14, 14), r.range(-17, -3)};
      if (b.sd(p) > -1) continue;
      fill(cv, Circle{p, 1.2f}, Solid{r.chance(0.5f) ? hex(0xd87ac8) : hex(0xf0a8e0)}, Blend::Atop);
    }
  });
  u16 edel = a.bake({-9, -16, 9, 2}, 3.0f, [](Canvas& cv) {
    for (int i = 0; i < 2; i++) {
      Vec2 h{-2.5f + i * 5.0f, -10.0f - i * 2.0f};
      stem(cv, {(f32)i - 0.5f, 0}, h, 0.6f, 0.7f);
      for (int k = 0; k < 6; k++) {
        f32 ang = k * TAU / 6;
        Ellipse pe{h + from_angle(ang, 2.0f), {1.9f, 0.85f}, ang};
        fill(cv, grow(pe, 0.3f), Solid{hex(0x8a8a7a)});
        fill(cv, pe, Solid{hex(0xfafaf0)});
      }
      fill(cv, Circle{h, 1.1f}, Solid{hex(0xe8d48a)});
    }
  });
  u16 gentian = a.bake({-9, -16, 9, 2}, 3.0f, [](Canvas& cv) {
    for (int i = 0; i < 3; i++) {
      Vec2 h{-3.5f + i * 3.5f, -8.0f - (i == 1) * 2.0f};
      stem(cv, {(f32)(i - 1), 0}, h, 0, 0.7f);
      Path cup;
      cup.move(h + Vec2{-1.8f, -2.5f}).quad(h + Vec2{-1.6f, 1.5f}, h + Vec2{0, 1.5f}).quad(h + Vec2{1.6f, 1.5f}, h + Vec2{1.8f, -2.5f}).line(h + Vec2{-1.8f, -2.5f});
      fill(cv, grow(cup.poly(), 0.35f), Solid{hex(0x1a2a6a)});
      fill(cv, cup.poly(), Linear{h + Vec2{0, -2.5f}, h + Vec2{0, 1.5f}, hex(0x5a8aff), hex(0x2a3ad8)});
    }
  });
  u16 pillar = a.bake({-14, -60, 14, 5}, 2.5f, [](Canvas& cv) {
    fill(cv, Ellipse{{0, 0}, {13, 4}}, Solid{hex(0x1a2030, 0.35f)}, Blend::Over, 3);
    RBox base{{0, -2}, {11, 3}, 1.0f};
    fill(cv, grow(base, 1.0f), Solid{hex(0x3a3630)});
    fill(cv, base, Solid{hex(0xd8d2c6)});
    Path col;
    col.move({-7, -4}).line({-6.5f, -44}).line({-2, -48}).line({1, -43}).line({4, -50}).line({6.5f, -45}).line({7, -4});
    fill(cv, grow(col.poly(), 1.0f), Solid{hex(0x3a3630)});
    fill(cv, col.poly(), Linear{{-7, 0}, {7, 0}, hex(0xf0ece4), hex(0x9a948a)});
    for (int k = 0; k < 4; k++) {
      Path f;
      f32 x = -4.5f + k * 3.0f;
      f.move({x, -6}).line({x, -42});
      fill(cv, f.stroke(0.7f), Solid{hex(0x8a847a, 0.6f)}, Blend::Atop);
    }
    fill(cv, wobble(Ellipse{{-3, -30}, {4, 8}}, 1.2f, 0.6f, 4), Solid{hex(0x8ab05a)}, Blend::Atop);
  });
  u16 flags = a.bake({-34, -52, 34, 4}, 2.5f, [](Canvas& cv) {
    for (int s = -1; s <= 1; s += 2) {
      Capsule pole{{s * 30.0f, 0}, {s * 30.0f, -48}, 1.2f, 1.0f};
      fill(cv, grow(pole, 0.6f), Solid{hex(0x2a1c12)});
      fill(cv, pole, Solid{hex(0x8a6a44)});
    }
    Path rope;
    rope.move({-30, -46}).quad({0, -30}, {30, -46}, 20);
    fill(cv, rope.stroke(0.7f), Solid{hex(0x5a4a3a)});
    const u32 cols[5] = {0xf2d24a, 0x5ac8f0, 0xf05a5a, 0x7ad87a, 0xf0f0f0};
    for (int i = 1; i < 10; i++) {
      Vec2 p = rope.pts[i * 2];
      Path f;
      f.move(p + Vec2{-2.4f, 0}).line(p + Vec2{2.4f, 0}).line(p + Vec2{2.0f, 6.5f}).line(p + Vec2{-2.0f, 6.5f});
      fill(cv, f.poly(), Solid{hex(cols[i % 5])});
    }
  });
  u16 cloud = a.bake({-40, -16, 40, 16}, 1.6f, [](Canvas& cv) {
    Blob b;
    b.k = 6;
    b.add({-20, 2}, 12).add({0, -2}, 15).add({20, 3}, 11).add({-8, 6}, 11).add({10, 6}, 12);
    fill(cv, b, [](Vec2 p, f32 d) {
      f32 k = saturate(-d / 8.0f);
      f32 l = 0.9f + 0.1f * saturate(-(p.y + 6) / 12.0f);
      return Color{l, l, min(1.0f, l + 0.03f), 0.9f * k};
    }, Blend::Over, 4);
  });
  u16 bl0 = a.bake({-28, -42, 28, 6}, 2.5f, [](Canvas& cv) { thorn_wall(cv, 1150, hex(0x4a2a3e), hex(0x8a5a72), hex(0x1c0f19), hex(0xd8c8b0), hex(0x5a1a6e), hex(0xb46bd8)); });
  u16 bl1 = a.bake({-28, -42, 28, 6}, 2.5f, [](Canvas& cv) { thorn_wall(cv, 1151, hex(0x4a2a3e), hex(0x8a5a72), hex(0x1c0f19), hex(0xd8c8b0), hex(0x5a1a6e), hex(0xb46bd8)); });
  u16 burnt = a.bake({-26, -18, 26, 6}, 2.5f, [](Canvas& cv) { burnt_pile(cv, hex(0x6a645c)); });

  R.edge.add(pine0, 0.3f).r(8).col({0, -3}).sw(0.1f).sh(1.8f).sc(0.85f, 1.15f).rev(34, 70);
  R.edge.add(pine1, 0.3f).r(8).col({0, -3}).sw(0.1f).sh(1.8f).sc(0.85f, 1.15f).rev(34, 70);
  R.edge.add(rock2, 0.4f).r(22).col({0, -6}).sh(1.6f).noflip().sc(0.9f, 1.2f);
  R.trees.add(pine0, 0.5f).r(8).col({0, -3}).sw(0.12f).sh(1.8f).rev(34, 70);
  R.trees.add(pine1, 0.5f).r(8).col({0, -3}).sw(0.12f).sh(1.8f).rev(34, 70);
  R.bushes.add(heather, 1).r(10).col({0, -5}).sw(0.08f).sh(1.0f).noflip();
  R.rocks.add(rock0, 0.4f).r(14).col({0, -5}).sh(1.0f).noflip().sc(1, 1);
  R.rocks.add(rock1, 0.4f).r(10).col({0, -4}).sh(0.8f).noflip().sc(1, 1);
  R.rocks.add(rock2, 0.2f).r(22).col({0, -6}).sh(1.6f).noflip().sc(1, 1);
  R.cover.add(tuft0, 1).sw(0.85f).sc(0.8f, 1.2f);
  R.cover.add(tuft1, 1).sw(0.85f).sc(0.8f, 1.2f);
  R.flowers.add(edel, 1).sw(0.6f);
  R.flowers.add(gentian, 1).sw(0.6f);
  R.shore.add(rock1, 1).sc(0.5f, 0.8f).noflip();
  R.floaters.add(cloud, 1).flat().sc(0.9f, 1.4f);
  R.extras.add(pillar, 1).r(10).col({0, -3}).noflip().sc(0.9f, 1.1f).rev(16, 54);
  R.extras.add(flags, 0.7f).sw(0.3f).noflip().sc(1, 1);
  R.blocker[0] = bl0;
  R.blocker[1] = bl1;
  R.blocker_burnt = burnt;
}

// ---------------------------------------------------------------- Malgrin's Hollow
void cauldron(Canvas& cv) {
  fill(cv, Ellipse{{0, 0}, {14, 4}}, Solid{hex(0x0a0810, 0.45f)}, Blend::Over, 3);
  Path pot;
  pot.move({-12, -14}).cubic({-14, -4}, {-8, 1}, {0, 1}).cubic({8, 1}, {14, -4}, {12, -14}).line({-12, -14});
  fill(cv, grow(pot.poly(), 1.0f), Solid{hex(0x08080a)});
  fill(cv, pot.poly(), Volume{hex(0x3a3a42), hex(0x6a6a74), hex(0x18181c), {-4, -9}, 13});
  Ellipse brew{{0, -14}, {12, 3.4f}};
  fill(cv, grow(brew, 0.8f), Solid{hex(0x08080a)});
  fill(cv, brew, Radial{{0, -14}, 12, hex(0xc8ff7a), hex(0x4a9a1a)});
  for (int i = 0; i < 3; i++) fill(cv, Circle{{-5.0f + i * 5.0f, -15.0f + (i % 2)}, 1.4f}, Solid{hex(0xe8ffb0)});
}

void palisade(Canvas& cv) {
  // a wall of sharpened stakes; segments overlap to form the goblin walls
  Rng r(1210);
  fill(cv, RBox{{0, 0}, {18, 3}, 2}, Solid{hex(0x0a0810, 0.4f)}, Blend::Over, 3);
  for (int i = 0; i < 5; i++) {
    f32 x = -14.0f + i * 7.0f;
    f32 top = -r.range(36, 44);
    Path p;
    p.move({x - 3.4f, 0}).line({x - 3.4f, top + 6}).line({x, top}).line({x + 3.4f, top + 6}).line({x + 3.4f, 0});
    fill(cv, grow(p.poly(), 1.0f), Solid{hex(0x1a0e08)});
    fill(cv, p.poly(), Linear{{x - 3.4f, 0}, {x + 3.4f, 0}, hex(0x9a6a40), hex(0x4a3020)});
  }
  for (int k = 0; k < 2; k++) {
    RBox band{{0, -12.0f - k * 14.0f}, {18, 1.3f}, 0.6f};
    fill(cv, band, Solid{hex(0x3a2a20)});
  }
}

void barricade(Canvas& cv, u32 seed) {
  Rng r(seed);
  fill(cv, Ellipse{{0, 0}, {24, 6}}, Solid{hex(0x0a0810, 0.45f)}, Blend::Over, 4);
  for (int i = 0; i < 6; i++) {
    f32 ang = r.range(-0.6f, 0.6f) + (i % 2 ? 0.9f : -0.9f);
    Vec2 c{r.range(-14, 14), r.range(-14, -6)};
    Vec2 d = from_angle(ang - HALF_PI, 18);
    Capsule stake{c - d, c + d, 2.2f, 1.2f};
    fill(cv, grow(stake, 1.0f), Solid{hex(0x1a0e08)});
    fill(cv, stake, Linear{c - d, c + d, hex(0x5a3a24), hex(0xa07a4a)});
  }
  RBox plank{{0, -10}, {16, 2.4f}, 1.0f, 0.12f};
  fill(cv, grow(plank, 1.0f), Solid{hex(0x1a0e08)});
  fill(cv, plank, Solid{hex(0x7a5a3a)});
  fill(cv, Ellipse{{0, -10}, {4.4f, 2.6f}}, Solid{hex(0x6a2aa8)});
  fill(cv, Circle{{0, -10}, 1.4f}, Solid{hex(0xf2d24a)});
}

void bake_hollow_art(Atlas& a) {
  RegionArt& R = S.ra;
  u16 twisted0 = a.bake({-40, -104, 40, 6}, 2.0f, [](Canvas& cv) { dead_tree(cv, 1201, hex(0x4a3a4e), hex(0x150c18), 96); });
  u16 twisted1 = a.bake({-40, -88, 40, 6}, 2.0f, [](Canvas& cv) { dead_tree(cv, 1215, hex(0x3e3044), hex(0x150c18), 80); });
  u16 crys0 = a.bake({-16, -36, 16, 4}, 2.5f, [](Canvas& cv) { crystal_cluster(cv, 1220, hex(0xb46bff), hex(0x2a0a48), 1.2f); });
  u16 crys1 = a.bake({-14, -28, 14, 4}, 2.5f, [](Canvas& cv) { crystal_cluster(cv, 1221, hex(0xe07aff), hex(0x3a0a48), 0.9f); });
  Stone dark{hex(0x4a4250), hex(0x7a7080), hex(0x26202c), hex(0x0e0a12)};
  u16 rock0 = a.bake({-26, -36, 26, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 20, 15, dark, 1230, {}, 0, hex(0xb46bff)); });
  u16 rock1 = a.bake({-18, -26, 18, 4}, 2.5f, [&](Canvas& cv) { rock_blob(cv, 13, 10, dark, 1231, {}, 0, hex(0xb46bff)); });
  u16 tuft0 = a.bake({-10, -20, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x3a3040), hex(0x8a7a8a), 1240, 6, 7, 14); });
  u16 tuft1 = a.bake({-10, -20, 10, 2}, 3.0f, [](Canvas& cv) { tuft(cv, hex(0x2e2834), hex(0x6a5a72), 1241, 5, 6, 12); });
  u16 bloom = a.bake({-9, -18, 9, 2}, 3.0f, [](Canvas& cv) {  // curse blooms
    for (int i = 0; i < 2; i++) {
      Vec2 h{-2.5f + i * 5.0f, -11.0f - i * 2.0f};
      stem(cv, {(f32)i - 0.5f, 0}, h, i ? 1.0f : -1.0f, 0.7f);
      fill(cv, Circle{h, 3.0f}, Solid{hex(0xd08aff, 0.3f)}, Blend::Over, 2.0f);
      for (int k = 0; k < 5; k++) {
        f32 ang = k * TAU / 5;
        Ellipse pe{h + from_angle(ang, 1.6f), {1.7f, 0.8f}, ang};
        fill(cv, pe, Linear{h, h + from_angle(ang, 3), hex(0xf0c8ff), hex(0x8a3ad0)});
      }
    }
  });
  u16 shard = a.bake({-12, -18, 12, 3}, 3.0f, [](Canvas& cv) {
    Rng r(1250);
    for (int i = 0; i < 4; i++) crystal(cv, {r.range(-7, 7), 0.5f}, r.range(6, 13), r.range(1.6f, 2.4f), r.range(-3, 3), hex(0x9a5ae0), hex(0x200a38));
  });
  u16 pot = a.bake({-16, -22, 16, 5}, 2.5f, [](Canvas& cv) { cauldron(cv); });
  u16 wall = a.bake({-20, -48, 20, 5}, 2.0f, [](Canvas& cv) { palisade(cv); });
  u16 bl0 = a.bake({-28, -32, 28, 6}, 2.5f, [](Canvas& cv) { barricade(cv, 1260); });
  u16 bl1 = a.bake({-28, -32, 28, 6}, 2.5f, [](Canvas& cv) { barricade(cv, 1261); });
  u16 burnt = a.bake({-26, -18, 26, 6}, 2.5f, [](Canvas& cv) { burnt_pile(cv, hex(0x4a4048)); });

  const Color violet{0.72f, 0.42f, 1.0f, 1}, green{0.6f, 1.0f, 0.4f, 1};
  R.edge.add(twisted0, 0.4f).r(9).col({0, -3}).sw(0.03f).sh(1.6f).sc(0.85f, 1.15f).rev(34, 100);
  R.edge.add(twisted1, 0.4f).r(9).col({0, -3}).sw(0.03f).sh(1.6f).sc(0.85f, 1.15f).rev(34, 84);
  R.edge.add(crys0, 0.2f).r(10).col({0, -3}).glow(80, violet);
  R.trees.add(twisted0, 0.5f).r(9).col({0, -3}).sw(0.03f).sh(1.6f).rev(34, 100);
  R.trees.add(twisted1, 0.5f).r(9).col({0, -3}).sw(0.03f).sh(1.6f).rev(34, 84);
  R.bushes.add(crys0, 1).r(10).col({0, -3}).glow(80, violet).sc(0.9f, 1.15f);
  R.bushes.add(crys1, 1).r(8).col({0, -3}).glow(64, violet).sc(0.9f, 1.15f);
  R.rocks.add(rock0, 0.5f).r(14).col({0, -5}).sh(1.0f).noflip().sc(1, 1);
  R.rocks.add(rock1, 0.5f).r(10).col({0, -4}).sh(0.8f).noflip().sc(1, 1);
  R.cover.add(tuft0, 1).sw(0.4f).sc(0.8f, 1.2f);
  R.cover.add(tuft1, 1).sw(0.4f).sc(0.8f, 1.2f);
  R.flowers.add(bloom, 1).sw(0.4f).glow(26, violet);
  R.shore.add(shard, 1).sc(0.8f, 1.2f).glow(24, violet);
  R.extras.add(pot, 0.5f).r(12).col({0, -5}).glow(60, green).em(EMIT_SPORES).noflip().sc(1, 1);
  R.extras.add(S.game.banner, 0.6f).r(3).col({0, -1}).sw(0.15f).noflip().sc(1, 1);
  R.extras.add(S.game.torch, 0.8f).r(4).col({0, -1}).noflip().sc(1, 1).glow(110, Color{1.0f, 0.6f, 0.3f, 1}).em(EMIT_SPARKS);
  R.path_bits.add(shard, 1).sc(0.4f, 0.6f).flat();
  R.blocker[0] = bl0;
  R.blocker[1] = bl1;
  R.blocker_burnt = burnt;
  R.wall = wall;
}

}  // namespace

void bake_marsh(Atlas& a) { bake_marsh_art(a); }
void bake_crags(Atlas& a) { bake_crags_art(a); }
void bake_elderwood(Atlas& a) { bake_elderwood_art(a); }
void bake_skyreach(Atlas& a) { bake_skyreach_art(a); }
void bake_hollow(Atlas& a) { bake_hollow_art(a); }

}  // namespace sky
