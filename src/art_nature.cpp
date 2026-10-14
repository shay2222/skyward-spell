// art_nature.cpp - Willowmere Meadows: trees, bushes, flowers, grass, rocks, water plants,
// Malgrin's cursed brambles, and the small life that makes the meadow feel alive.
#include "art_util.h"

namespace sky {
using namespace paint;

namespace {

struct MeadowIds {
  u16 tree[6];
  u16 bush[4], flower[8], grass[6], rock[4], pebble[2], reed[2], lily[2], bramble[2], bramble_burnt, mushroom[2], log, stump;
} M;

void bake_trees(Atlas& a) {
  const f32 sc = 2.0f;
  const Color trunk_line = hex(0x3a2418);
  const Foliage oak{hex(0x5f9e3c), hex(0xa8d65a), hex(0x2c6236), hex(0x1f4a2a)};
  const Foliage birch{hex(0x8cc251), hex(0xd2ec7a), hex(0x4f8a35), hex(0x34642a)};
  const Foliage blossom{hex(0xf2a0c0), hex(0xffe0ec), hex(0xbe5f8c), hex(0x8a3d63)};
  const Foliage apple{hex(0x579a3d), hex(0x9fd35c), hex(0x285e33), hex(0x1d472a)};

  auto oak_tree = [&](u32 seed, const Foliage& f, Color bark, bool apples) {
    return a.bake({-50, -116, 50, 6}, sc, [&, seed](Canvas& cv) {
      paint_trunk(cv, 40, 12, 8, bark, trunk_line, seed);
      Rng r(seed);
      Blob b;
      b.k = 3.5f;
      Vec2 c{0, -70};
      b.add(c + Vec2{0, 0}, 24);
      b.add(c + Vec2{-21 + r.range(-2, 2), 8 + r.range(-2, 2)}, 18);
      b.add(c + Vec2{21 + r.range(-2, 2), 7 + r.range(-2, 2)}, 18);
      b.add(c + Vec2{-14, -15 + r.range(-2, 2)}, 18);
      b.add(c + Vec2{14, -17 + r.range(-2, 2)}, 18.5f);
      b.add(c + Vec2{r.range(-3, 3), -27}, 15);
      b.add(c + Vec2{-27, -4}, 13);
      b.add(c + Vec2{27, -6}, 13);
      b.add(c + Vec2{-6, 17}, 16);
      b.add(c + Vec2{9, 16}, 15);
      paint_foliage(cv, b, c + Vec2{-4, -6}, 44, f, seed);
      if (apples) {
        for (int i = 0; i < 9; i++) {
          Vec2 p = c + Vec2{r.range(-28, 28), r.range(-22, 20)};
          if (b.sd(p) > -4) continue;
          fill(cv, Circle{p, 2.6f}, Solid{hex(0x6e1414)});
          fill(cv, Circle{p, 2.0f}, Volume{hex(0xe0473a), hex(0xff9a7a), hex(0xa82424), p + Vec2{-0.5f, -0.5f}, 2.4f, 0.6f, 0.3f});
          fill(cv, Circle{p + Vec2{-0.7f, -0.7f}, 0.6f}, Solid{{1, 1, 1, 0.8f}});
        }
      }
    });
  };

  M.tree[0] = oak_tree(11, oak, hex(0x7a5236), false);
  M.tree[1] = oak_tree(23, oak, hex(0x6f4a31), false);
  M.tree[4] = oak_tree(57, apple, hex(0x765036), true);

  // birch: slender white trunk, lighter leaves
  M.tree[2] = a.bake({-40, -122, 40, 6}, sc, [&](Canvas& cv) {
    paint_trunk(cv, 56, 9, 6, hex(0xece6da), hex(0x4b4440), 31, true);
    Blob b;
    b.k = 3;
    Vec2 c{0, -82};
    b.add(c, 18).add(c + Vec2{-14, 10}, 14).add(c + Vec2{14, 8}, 14).add(c + Vec2{-9, -14}, 14).add(c + Vec2{10, -16}, 13);
    b.add(c + Vec2{0, -27}, 11).add(c + Vec2{-4, 20}, 12).add(c + Vec2{8, 18}, 11).add(c + Vec2{-20, -2}, 10).add(c + Vec2{20, -4}, 10);
    paint_foliage(cv, b, c + Vec2{-3, -6}, 34, birch, 31, 1.2f);
  });

  // a pink blossom tree: the meadow's showpiece
  M.tree[3] = a.bake({-52, -116, 52, 6}, sc, [&](Canvas& cv) {
    paint_trunk(cv, 40, 12, 7, hex(0x5e3b30), hex(0x2e1a16), 43);
    Path br;  // a couple of visible branches
    br.move({-1, -36}).quad({-10, -48}, {-18, -54});
    fill(cv, br.stroke(4.0f, 1.8f), Solid{hex(0x5e3b30)});
    Blob b;
    b.k = 3.5f;
    Vec2 c{0, -70};
    b.add(c, 22).add(c + Vec2{-23, 6}, 17).add(c + Vec2{23, 5}, 17).add(c + Vec2{-14, -16}, 18).add(c + Vec2{14, -17}, 17);
    b.add(c + Vec2{0, -28}, 14).add(c + Vec2{-30, -6}, 12).add(c + Vec2{30, -8}, 12).add(c + Vec2{-8, 16}, 15).add(c + Vec2{10, 15}, 14);
    paint_foliage(cv, b, c + Vec2{-4, -6}, 44, blossom, 43, 1.4f);
    Rng r(44);
    for (int i = 0; i < 26; i++) {  // tiny white blossoms
      Vec2 p = c + Vec2{r.range(-32, 32), r.range(-30, 20)};
      if (b.sd(p) > -2) continue;
      for (int k = 0; k < 5; k++) fill(cv, Circle{p + from_angle(k * TAU / 5, 1.1f), 0.9f}, Solid{hex(0xfff5f9)}, Blend::Atop);
      fill(cv, Circle{p, 0.6f}, Solid{hex(0xf7c84a)}, Blend::Atop);
    }
  });

  // a dark pine for the forest edge
  M.tree[5] = a.bake({-40, -124, 40, 6}, sc, [&](Canvas& cv) {
    paint_trunk(cv, 22, 10, 7, hex(0x6a4630), trunk_line, 61);
    Color base = hex(0x2f7550), light = hex(0x6fb57a), dark = hex(0x18432f), line = hex(0x10301f);
    for (int tier = 0; tier < 4; tier++) {
      f32 top = -118.0f + tier * 18.0f, bot = -40.0f - (3 - tier) * 15.0f;
      f32 half = 14.0f + tier * 8.0f;
      Path p;
      p.move({0, top}).quad({half * 0.35f, (top + bot) * 0.5f}, {half, bot}).quad({half * 0.5f, bot + 6}, {0, bot + 3}).quad({-half * 0.5f, bot + 6}, {-half, bot}).quad({-half * 0.35f, (top + bot) * 0.5f}, {0, top});
      auto shape = wobble(p.poly(), 1.2f, 0.45f, 61 + tier);
      fill(cv, grow(shape, 1.2f), Solid{line});
      fill(cv, shape, noisy(Volume{base, light, dark, {-half * 0.3f, (top + bot) * 0.5f}, half * 1.3f, 5.0f, 0.45f, TO_LIGHT}, 0.035f, 0.4f, 61));
      Rng r(70 + tier);
      for (int i = 0; i < 18; i++) {
        f32 y = r.range(top + 8, bot);
        f32 x = r.range(-half, half) * saturate((y - top) / (bot - top));
        fill(cv, Ellipse{{x, y}, {2.4f, 0.8f}, x < 0 ? -0.5f : 0.5f}, Solid{(x < 0 ? light : dark).with_alpha(0.5f)}, Blend::Atop);
      }
    }
  });
}

void bake_bushes(Atlas& a) {
  const Foliage leaf{hex(0x4f9a3e), hex(0x9fd863), hex(0x23573a), hex(0x1b4029)};
  for (int v = 0; v < 4; v++) {
    M.bush[v] = a.bake({-24, -34, 24, 4}, 2.5f, [&, v](Canvas& cv) {
      Rng r(100 + v);
      Blob b;
      b.k = 3;
      Vec2 c{0, -14};
      b.add(c, 12).add(c + Vec2{-11, 4}, 9.5f).add(c + Vec2{11, 4}, 9.5f).add(c + Vec2{-6, -8}, 9).add(c + Vec2{6, -9}, 9).add(c + Vec2{0, 7}, 9);
      for (int i = 0; i < b.c.count; i++) b.c[i].c += Vec2{r.range(-1.5f, 1.5f), r.range(-1.5f, 1.5f)};
      fill(cv, Ellipse{{0, 0}, {18, 4}}, Solid{hex(0x1d3d22, 0.35f)}, Blend::Over, 3);
      paint_foliage(cv, b, c + Vec2{-2, -3}, 22, leaf, 100 + v);
      Color dot = v == 1 ? hex(0xd8344a) : v == 2 ? hex(0xfff6fb) : hex(0x7a9ff0);
      if (v == 0) return;
      int n = v == 3 ? 7 : 12;
      for (int i = 0; i < n; i++) {
        Vec2 p = c + Vec2{r.range(-17, 17), r.range(-15, 12)};
        if (b.sd(p) > -2.5f) continue;
        if (v == 1) {  // red berries
          fill(cv, Circle{p, 1.9f}, Solid{hex(0x5a1020)});
          fill(cv, Circle{p, 1.5f}, Solid{dot});
          fill(cv, Circle{p + Vec2{-0.5f, -0.5f}, 0.5f}, Solid{{1, 1, 1, 0.85f}});
        } else if (v == 2) {  // little white flowers
          for (int k = 0; k < 5; k++) fill(cv, Circle{p + from_angle(k * TAU / 5 + i, 1.3f), 1.0f}, Solid{dot}, Blend::Atop);
          fill(cv, Circle{p, 0.7f}, Solid{hex(0xf2c440)}, Blend::Atop);
        } else {  // blue clusters
          Blob cl;
          cl.k = 1;
          for (int k = 0; k < 6; k++) cl.add(p + Vec2{r.range(-2.5f, 2.5f), r.range(-2.5f, 2.5f)}, 1.6f);
          fill(cv, cl, Volume{dot, lighten(dot, 0.5f), darken(dot, 0.35f), p + Vec2{-1, -1}, 4}, Blend::Atop);
        }
      }
    });
  }
}

void bake_flowers(Atlas& a) {
  const f32 sc = 3.0f;
  const Rect box{-9, -20, 9, 2};
  // 0 daisies
  M.flower[0] = a.bake(box, sc, [](Canvas& cv) {
    Vec2 heads[2] = {{-2.5f, -13}, {3.5f, -9}};
    stem(cv, {-1, 0}, heads[0], -1.5f);
    stem(cv, {1, 0}, heads[1], 1.0f);
    fill(cv, Ellipse{{2, -3}, {2.4f, 0.9f}, -0.5f}, Solid{hex(0x5aa043)});
    for (Vec2 h : heads) {
      for (int k = 0; k < 9; k++) {
        f32 ang = k * TAU / 9;
        fill(cv, Ellipse{h + from_angle(ang, 2.3f), {1.7f, 0.75f}, ang}, Solid{hex(0x8f8a9a)});
        fill(cv, Ellipse{h + from_angle(ang, 2.3f), {1.45f, 0.55f}, ang}, Solid{hex(0xffffff)});
      }
      fill(cv, Circle{h, 1.25f}, Volume{hex(0xf7c632), hex(0xffe98a), hex(0xd18c16), h + Vec2{-0.4f, -0.4f}, 1.4f});
    }
  });
  // 1 poppies
  M.flower[1] = a.bake(box, sc, [](Canvas& cv) {
    Vec2 heads[2] = {{-3, -14}, {3, -10}};
    stem(cv, {-1, 0}, heads[0], -1.5f, 0.8f);
    stem(cv, {1, 0}, heads[1], 1.5f, 0.8f);
    for (Vec2 h : heads) {
      Blob b;
      b.k = 0.6f;
      for (int k = 0; k < 4; k++) b.add(h + from_angle(k * TAU / 4 + 0.4f, 1.6f), 2.0f);
      fill(cv, grow(b, 0.5f), Solid{hex(0x7a1a1a)});
      fill(cv, b, Volume{hex(0xe8433a), hex(0xff8f72), hex(0xb02424), h + Vec2{-0.6f, -0.6f}, 3.4f, 0.8f, 0.3f});
      fill(cv, Circle{h, 0.9f}, Solid{hex(0x2a1418)});
    }
  });
  // 2 bluebells
  M.flower[2] = a.bake(box, sc, [](Canvas& cv) {
    Path p;
    p.move({0, 0}).quad({1, -9}, {4, -15});
    fill(cv, p.stroke(0.85f, 0.6f), Solid{hex(0x4f8f3a)});
    Vec2 bells[3] = {{1.2f, -9}, {2.6f, -12.2f}, {4.4f, -14.6f}};
    for (Vec2 b : bells) {
      Path bell;
      bell.move(b + Vec2{-1.6f, 0}).quad(b + Vec2{-1.5f, 2.8f}, b + Vec2{-1.9f, 3.4f}).line(b + Vec2{1.9f, 3.4f}).quad(b + Vec2{1.5f, 2.8f}, b + Vec2{1.6f, 0}).quad(b + Vec2{0, -1.2f}, b + Vec2{-1.6f, 0});
      fill(cv, grow(bell.poly(), 0.45f), Solid{hex(0x2f2f6e)});
      fill(cv, bell.poly(), Linear{b, b + Vec2{0, 3.4f}, hex(0x8c9cff), hex(0x5a64d8)});
    }
  });
  // 3 buttercups
  M.flower[3] = a.bake(box, sc, [](Canvas& cv) {
    Vec2 heads[3] = {{-4, -10}, {0.5f, -13}, {4.5f, -8}};
    for (int i = 0; i < 3; i++) stem(cv, {(f32)(i - 1), 0}, heads[i], (f32)(i - 1), 0.7f);
    for (Vec2 h : heads) {
      for (int k = 0; k < 5; k++) fill(cv, Circle{h + from_angle(k * TAU / 5 - HALF_PI, 1.2f), 1.25f}, Solid{hex(0xffcf2a)});
      fill(cv, Circle{h + Vec2{-0.5f, -0.6f}, 0.7f}, Solid{hex(0xfff6b0)});
      fill(cv, Circle{h, 0.55f}, Solid{hex(0xd98c10)});
    }
  });
  // 4 lavender
  M.flower[4] = a.bake(box, sc, [](Canvas& cv) {
    for (int s = 0; s < 3; s++) {
      Vec2 base{(f32)(s - 1) * 2.2f, 0};
      Vec2 tip = base + Vec2{(f32)(s - 1) * 2.0f, -15.0f + s * 1.5f};
      stem(cv, base, tip, 0.4f, 0.7f);
      for (int k = 0; k < 7; k++) {
        Vec2 p = lerp(tip, lerp(base, tip, 0.45f), k / 7.0f);
        Color c = mix(hex(0xb48af0), hex(0x7a52c8), k / 7.0f);
        fill(cv, Ellipse{p + Vec2{-0.5f, 0}, {0.9f, 1.2f}}, Solid{c});
        fill(cv, Ellipse{p + Vec2{0.6f, 0.3f}, {0.85f, 1.1f}}, Solid{darken(c, 0.12f)});
      }
    }
  });
  // 5 pink cosmos
  M.flower[5] = a.bake(box, sc, [](Canvas& cv) {
    Vec2 h{0.5f, -13};
    stem(cv, {0, 0}, h, -1.2f);
    fill(cv, Ellipse{{-2, -5}, {2.2f, 0.8f}, 0.6f}, Solid{hex(0x5aa043)});
    for (int k = 0; k < 8; k++) {
      f32 ang = k * TAU / 8 + 0.2f;
      Ellipse pe{h + from_angle(ang, 2.6f), {2.1f, 1.15f}, ang};
      fill(cv, grow(pe, 0.35f), Solid{hex(0xa13d6b)});
      fill(cv, pe, Linear{h, h + from_angle(ang, 4.5f), hex(0xffc2dd), hex(0xf06aa4)});
    }
    fill(cv, Circle{h, 1.2f}, Solid{hex(0xf6c23a)});
  });
  // 6 red tulips
  M.flower[6] = a.bake(box, sc, [](Canvas& cv) {
    Vec2 heads[2] = {{-2.2f, -13}, {2.8f, -10.5f}};
    for (int i = 0; i < 2; i++) {
      stem(cv, {(f32)i * 1.5f - 0.8f, 0}, heads[i], i ? 0.8f : -0.8f, 1.0f);
      fill(cv, Ellipse{heads[i] + Vec2{i ? 2.0f : -2.0f, 7}, {1.0f, 3.5f}, i ? 0.4f : -0.4f}, Solid{hex(0x4f9a3e)});
    }
    for (Vec2 h : heads) {
      Path cup;
      cup.move(h + Vec2{-2.4f, -2.2f}).line(h + Vec2{-1.2f, -0.8f}).line(h + Vec2{0, -2.6f}).line(h + Vec2{1.2f, -0.8f}).line(h + Vec2{2.4f, -2.2f}).quad(h + Vec2{2.6f, 2.6f}, h + Vec2{0, 2.4f}).quad(h + Vec2{-2.6f, 2.6f}, h + Vec2{-2.4f, -2.2f});
      fill(cv, grow(cup.poly(), 0.45f), Solid{hex(0x6e1420)});
      fill(cv, cup.poly(), Volume{hex(0xe63e4a), hex(0xff9aa0), hex(0xa81e2e), h + Vec2{-1, -1}, 3.5f, 0.8f, 0.3f});
    }
  });
  // 7 white clover
  M.flower[7] = a.bake(box, sc, [](Canvas& cv) {
    for (int k = 0; k < 3; k++) {
      Vec2 c{(f32)(k - 1) * 4.0f, -2.5f - (k == 1) * 1.5f};
      for (int j = 0; j < 3; j++) fill(cv, Circle{c + from_angle(j * TAU / 3 - HALF_PI, 1.3f), 1.35f}, Solid{hex(0x5aa346)});
      fill(cv, Circle{c, 0.5f}, Solid{hex(0x3f7f33)});
    }
    Vec2 h{1.5f, -9};
    stem(cv, {0.5f, -2}, h, 0.6f, 0.6f);
    for (int k = 0; k < 14; k++) fill(cv, Ellipse{h + from_angle(k * 2.4f, 0.7f + (k % 3) * 0.45f), {0.55f, 0.95f}, k * 2.4f}, Solid{k % 2 ? hex(0xffffff) : hex(0xf4e6ee)});
  });
}

void bake_grass(Atlas& a) {
  for (int v = 0; v < 6; v++) {
    M.grass[v] = a.bake({-10, -20, 10, 2}, 3.0f, [v](Canvas& cv) {
      Rng r(200 + v);
      int blades = 6 + v % 3;
      Color base = v >= 4 ? hex(0x6f9a38) : hex(0x3f7a2e);
      Color tip = v >= 4 ? hex(0xe0d68a) : hex(0xb2de6a);
      for (int i = 0; i < blades; i++) {
        f32 x = r.range(-5.5f, 5.5f);
        f32 h = r.range(8.0f, v >= 4 ? 17.0f : 14.0f);
        f32 lean = r.range(-4.0f, 4.0f) + x * 0.3f;
        f32 w = r.range(1.4f, 2.1f);
        Vec2 b0{x - w * 0.5f, 0.5f}, b1{x + w * 0.5f, 0.5f}, t{x + lean, -h};
        Path p;
        p.move(b0).quad({x + lean * 0.35f - w * 0.3f, -h * 0.55f}, t).quad({x + lean * 0.35f + w * 0.3f, -h * 0.5f}, b1).line(b0);
        Color tc = mix(tip, hex(0xd4ef8c), r.unit() * 0.4f);
        fill(cv, p.poly(), Linear{{x, 0}, t, base, tc});
      }
      if (v >= 4) {  // seed heads, like wild wheat
        for (int i = 0; i < 3; i++) {
          Vec2 t{r.range(-4, 4), r.range(-18.0f, -14.0f)};
          for (int k = 0; k < 5; k++) fill(cv, Ellipse{t + Vec2{(k % 2 ? 0.7f : -0.7f), k * 1.1f}, {0.7f, 1.0f}, k % 2 ? 0.5f : -0.5f}, Solid{hex(0xe8d48a)});
        }
      }
    });
  }
}

void bake_rocks(Atlas& a) {
  Color base = hex(0x9aa4b0), light = hex(0xd6dde3), dark = hex(0x5a6474), line = hex(0x2f3540);
  struct R { f32 w, h; u32 seed; };
  R sizes[4] = {{22, 16, 301}, {16, 12, 302}, {30, 20, 303}, {11, 8, 304}};
  for (int v = 0; v < 4; v++) {
    R s = sizes[v];
    M.rock[v] = a.bake({-s.w - 4, -s.h * 2 - 4, s.w + 4, 4}, 2.5f, [=](Canvas& cv) {
      Rng r(s.seed);
      Blob b;
      b.k = s.w * 0.2f;
      Vec2 c{0, -s.h * 0.9f};
      b.add(c + Vec2{-s.w * 0.35f, s.h * 0.15f}, s.h * 0.75f);
      b.add(c + Vec2{s.w * 0.3f, s.h * 0.2f}, s.h * 0.7f);
      b.add(c + Vec2{r.range(-2, 2), -s.h * 0.2f}, s.h * 0.8f);
      auto flat = intersect(wobble(b, s.w * 0.05f, 0.35f, s.seed), RBox{{0, -s.h * 2}, {s.w * 2, s.h * 2}});
      fill(cv, Ellipse{{1, 0}, {s.w * 0.95f, s.h * 0.3f}}, Solid{hex(0x1d2a22, 0.35f)}, Blend::Over, 3);
      fill(cv, grow(flat, 1.1f), Solid{line});
      fill(cv, flat, noisy(Volume{base, light, dark, c + Vec2{-s.w * 0.2f, -s.h * 0.3f}, s.w * 1.1f, 4.0f, 0.4f, TO_LIGHT}, 0.05f, 0.6f, s.seed));
      // moss on top
      Circle moss{c + Vec2{-s.w * 0.15f, -s.h * 0.75f}, s.w * 0.55f};
      fill(cv, wobble(moss, 2.0f, 0.5f, s.seed + 1), noisy(Linear{c + Vec2{0, -s.h}, c, hex(0x8fc85a), hex(0x4a8a3a)}, 0.06f, 0.8f), Blend::Atop);
      // a crack
      Path cr;
      cr.move(c + Vec2{s.w * 0.25f, -s.h * 0.1f}).line(c + Vec2{s.w * 0.12f, s.h * 0.25f}).line(c + Vec2{s.w * 0.3f, s.h * 0.55f});
      fill(cv, cr.stroke(0.7f, 0.3f), Solid{dark.with_alpha(0.7f)}, Blend::Atop);
    });
  }
  for (int v = 0; v < 2; v++) {
    M.pebble[v] = a.bake({-7, -5, 7, 3}, 3.0f, [v](Canvas& cv) {
      Rng r(330 + v);
      for (int i = 0; i < 3; i++) {
        Vec2 p{r.range(-4, 4), r.range(-2.5f, 0.5f)};
        Ellipse e{p, {r.range(1.3f, 2.2f), r.range(0.9f, 1.4f)}, r.range(-0.4f, 0.4f)};
        fill(cv, grow(e, 0.4f), Solid{hex(0x6a5a50, 0.8f)});
        fill(cv, e, Volume{hex(0xb8aa98), hex(0xe2d8c8), hex(0x857464), p, 2.0f, 0.6f, 0.2f});
      }
    });
  }
}

void bake_water_plants(Atlas& a) {
  for (int v = 0; v < 2; v++) {
    M.reed[v] = a.bake({-12, -40, 12, 2}, 2.5f, [v](Canvas& cv) {
      Rng r(400 + v);
      for (int i = 0; i < 7; i++) {
        f32 x = r.range(-6, 6), h = r.range(20, 34);
        Vec2 tip{x + r.range(-4, 4), -h};
        Path p;
        p.move({x - 0.9f, 0.5f}).quad({x, -h * 0.5f}, tip).quad({x + 0.3f, -h * 0.5f}, {x + 0.9f, 0.5f});
        fill(cv, p.poly(), Linear{{x, 0}, tip, hex(0x3d6f3a), hex(0xa7cf6a)});
      }
      for (int i = 0; i < (v ? 3 : 2); i++) {  // cattails
        f32 x = r.range(-4, 4);
        Vec2 top{x + r.range(-1, 1), -r.range(28, 36)};
        Path s;
        s.move({x, 0}).quad({x, top.y * 0.5f}, top);
        fill(cv, s.stroke(0.9f), Solid{hex(0x5a7a3a)});
        Capsule head{top + Vec2{0, 2}, top + Vec2{0, 9}, 1.8f, 1.8f};
        fill(cv, grow(head, 0.4f), Solid{hex(0x2a1810)});
        fill(cv, head, Volume{hex(0x7a4a2a), hex(0xb07a4a), hex(0x4a2a18), top + Vec2{-1, 4}, 4});
        fill(cv, Capsule{top - Vec2{0, 1.5f}, top + Vec2{0, 2}, 0.35f, 0.35f}, Solid{hex(0x5a7a3a)});
      }
    });
  }
  for (int v = 0; v < 2; v++) {
    M.lily[v] = a.bake({-12, -9, 12, 9}, 3.0f, [v](Canvas& cv) {
      Ellipse pad{{0, 0}, {10, 6.5f}};
      Path notch;
      notch.move({0, 0}).line({10.5f, -2.2f}).line({10.5f, 1.4f});
      auto leaf = subtract(pad, notch.poly());
      fill(cv, grow(leaf, 0.7f), Solid{hex(0x1d4a30, 0.9f)});
      fill(cv, leaf, Volume{hex(0x4fa35a), hex(0x9fdc7a), hex(0x2a6a3e), {-3, -2}, 11, 2.0f, 0.3f});
      for (int k = 0; k < 6; k++) {
        Path vein;
        f32 ang = PI * 0.25f + k * (PI * 1.5f / 6);
        vein.move({0, 0}).line(from_angle(ang) * Vec2{8.5f, 5.2f});
        fill(cv, vein.stroke(0.4f, 0.2f), Solid{hex(0x2f7a40, 0.5f)}, Blend::Atop);
      }
      if (v == 1) {  // a pink water lily
        Vec2 c{-2, -2};
        for (int k = 0; k < 8; k++) {
          f32 ang = k * TAU / 8;
          Ellipse pe{c + from_angle(ang, 2.6f) * Vec2{1, 0.7f}, {2.3f, 1.1f}, ang};
          fill(cv, grow(pe, 0.3f), Solid{hex(0xa8406e)});
          fill(cv, pe, Linear{c, c + from_angle(ang, 4), hex(0xfff0f6), hex(0xf28ab8)});
        }
        fill(cv, Circle{c, 1.2f}, Solid{hex(0xf7cd3e)});
      }
    });
  }
}

void bake_brambles(Atlas& a) {
  // Malgrin's cursed brambles: twisted thorny vines with a sickly violet sheen
  for (int v = 0; v < 2; v++) {
    M.bramble[v] = a.bake({-28, -42, 28, 6}, 2.5f, [v](Canvas& cv) {
      Rng r(500 + v);
      fill(cv, Ellipse{{0, 0}, {24, 6}}, Solid{hex(0x1a1020, 0.4f)}, Blend::Over, 4);
      Color vine = hex(0x4b2a3e), vine_light = hex(0x8a5a72), line = hex(0x1c0f19);
      for (int i = 0; i < 16; i++) {
        Vec2 a0{r.range(-18, 18), r.range(-2, 2)};
        Vec2 a1{r.range(-22, 22), r.range(-36, -8)};
        Vec2 ctrl{(a0.x + a1.x) * 0.5f + r.range(-14, 14), (a0.y + a1.y) * 0.5f + r.range(-10, 6)};
        Path p;
        p.move(a0).quad(ctrl, a1, 12);
        f32 w = r.range(1.8f, 3.0f);
        fill(cv, p.stroke(w + 1.0f, 1.0f), Solid{line});
        fill(cv, p.stroke(w, 0.5f), Linear{a0, a1, vine, vine_light});
        for (int k = 2; k < p.pts.count - 1; k += 3) {  // thorns
          Vec2 q = p.pts[k];
          Vec2 dir = normalize(p.pts[k + 1] - p.pts[k - 1]);
          Vec2 n = perp(dir) * ((k / 3) % 2 ? 1.0f : -1.0f);
          Path th;
          th.move(q - dir * 0.9f).line(q + n * 2.6f + dir * 0.6f).line(q + dir * 0.9f);
          fill(cv, th.poly(), Solid{hex(0xd8c8b0)});
        }
      }
      for (int i = 0; i < 6; i++) {  // dark berries with a cursed glint
        Vec2 p{r.range(-16, 16), r.range(-30, -8)};
        fill(cv, Circle{p, 2.0f}, Solid{hex(0x200a24)});
        fill(cv, Circle{p, 1.6f}, Volume{hex(0x5a1a6e), hex(0xb46bd8), hex(0x2a0a38), p + Vec2{-0.5f, -0.5f}, 2});
      }
    });
  }
  M.bramble_burnt = a.bake({-26, -18, 26, 6}, 2.5f, [](Canvas& cv) {
    Rng r(520);
    fill(cv, Ellipse{{0, 0}, {22, 6}}, Solid{hex(0x2a2420, 0.55f)}, Blend::Over, 3);
    fill(cv, wobble(Ellipse{{0, -1}, {17, 4.5f}}, 1.2f, 0.4f, 5), noisy(Solid{hex(0x6a625c)}, 0.08f, 0.9f));
    for (int i = 0; i < 9; i++) {
      Vec2 a0{r.range(-14, 14), r.range(-2, 1)};
      Vec2 a1 = a0 + Vec2{r.range(-5, 5), r.range(-12, -5)};
      Path p;
      p.move(a0).quad((a0 + a1) * 0.5f + Vec2{r.range(-2, 2), 0}, a1);
      fill(cv, p.stroke(1.8f, 0.6f), Solid{hex(0x1e1816)});
    }
    for (int i = 0; i < 5; i++) fill(cv, Circle{{r.range(-12, 12), r.range(-3, 0)}, 0.9f}, Solid{hex(0xff8a3a, 0.8f)});
  });
}

void bake_small_things(Atlas& a) {
  for (int v = 0; v < 2; v++) {
    M.mushroom[v] = a.bake({-12, -18, 12, 3}, 3.0f, [v](Canvas& cv) {
      Rng r(600 + v);
      int n = v ? 3 : 2;
      Color cap = v ? hex(0xe8a23a) : hex(0xd8443a);
      for (int i = 0; i < n; i++) {
        f32 x = (i - (n - 1) * 0.5f) * 5.5f + r.range(-1, 1);
        f32 h = r.range(5, 9) - i * 0.8f;
        f32 cr = r.range(3.6f, 5.0f);
        Capsule st{{x, 0}, {x, -h}, 1.7f, 1.3f};
        fill(cv, grow(st, 0.6f), Solid{hex(0x5a4030)});
        fill(cv, st, Linear{{x - 2, 0}, {x + 2, 0}, hex(0xfff3dc), hex(0xd8c4a4)});
        Path c;
        c.move({x - cr, -h + 0.6f}).cubic({x - cr, -h - cr * 1.2f}, {x + cr, -h - cr * 1.2f}, {x + cr, -h + 0.6f}).quad({x, -h + 1.8f}, {x - cr, -h + 0.6f});
        fill(cv, grow(c.poly(), 0.7f), Solid{darken(cap, 0.6f)});
        fill(cv, c.poly(), Volume{cap, lighten(cap, 0.45f), darken(cap, 0.3f), {x - 1.5f, -h - 2}, cr * 1.2f, 1.2f, 0.3f});
        for (int k = 0; k < 4; k++) fill(cv, Ellipse{{x + r.range(-cr * 0.6f, cr * 0.6f), -h - r.range(0.5f, cr * 0.7f)}, {0.9f, 0.6f}}, Solid{hex(0xfff8ec)}, Blend::Atop);
      }
    });
  }
  M.log = a.bake({-30, -18, 30, 4}, 2.5f, [](Canvas& cv) {
    fill(cv, Ellipse{{0, 0}, {26, 5}}, Solid{hex(0x1d3022, 0.35f)}, Blend::Over, 4);
    Capsule body{{-22, -7}, {20, -7}, 7.5f, 7.5f};
    fill(cv, grow(body, 1.1f), Solid{hex(0x2e1c12)});
    fill(cv, body, noisy(Linear{{0, -14}, {0, 0}, hex(0x9a6a44), hex(0x5a3a24)}, 0.06f, 0.5f));
    for (int i = 0; i < 5; i++) {
      Path s;
      f32 y = -11.0f + i * 2.2f;
      s.move({-18, y}).quad({0, y + 0.8f}, {16, y - 0.4f});
      fill(cv, s.stroke(0.5f), Solid{hex(0x3a2416, 0.45f)}, Blend::Atop);
    }
    Ellipse end{{21, -7}, {4.6f, 7.2f}};
    fill(cv, grow(end, 0.8f), Solid{hex(0x2e1c12)});
    fill(cv, end, Solid{hex(0xd8b07a)});
    for (int k = 1; k < 4; k++) fill(cv, grow(Ellipse{{21, -7}, {4.6f - k * 1.2f, 7.2f - k * 1.8f}}, 0.25f), Solid{hex(0xa87a4a, 0.6f)});
    fill(cv, wobble(Ellipse{{-6, -13}, {10, 3.5f}}, 1.2f, 0.6f, 3), noisy(Solid{hex(0x6fae4a)}, 0.06f, 0.9f), Blend::Atop);
  });
  M.stump = a.bake({-18, -22, 18, 4}, 2.5f, [](Canvas& cv) {
    fill(cv, Ellipse{{0, 0}, {15, 4.5f}}, Solid{hex(0x1d3022, 0.35f)}, Blend::Over, 3);
    Path p;
    p.move({-12, 0}).quad({-10, -2}, {-9, -6}).line({-9, -12}).line({9, -12}).line({9, -6}).quad({10, -2}, {12, 0}).quad({0, 2}, {-12, 0});
    fill(cv, grow(p.poly(), 1.0f), Solid{hex(0x2e1c12)});
    fill(cv, p.poly(), noisy(Linear{{-10, 0}, {10, 0}, hex(0x9a6a44), hex(0x4a2e1c)}, 0.06f, 0.6f));
    Ellipse top{{0, -12}, {9.2f, 3.6f}};
    fill(cv, grow(top, 0.8f), Solid{hex(0x2e1c12)});
    fill(cv, top, Solid{hex(0xdcb47c)});
    for (int k = 1; k < 4; k++) fill(cv, grow(Ellipse{{0, -12}, {9.2f - k * 2.2f, 3.6f - k * 0.85f}}, 0.25f), Solid{hex(0xa87a4a, 0.55f)});
  });
}

}  // namespace

void bake_butterflies(Atlas& a) {
  const Color wings[3][2] = {{hex(0xff9a2a), hex(0xd8501a)}, {hex(0x7ac8ff), hex(0x3a7ad8)}, {hex(0xfff2a8), hex(0xe8c040)}};
  for (int v = 0; v < 3; v++) {
    S.butterfly[v] = a.bake({-9, -8, 9, 8}, 4.0f, [&, v](Canvas& cv) {
      for (int side = -1; side <= 1; side += 2) {
        Ellipse up{{side * 4.2f, -2.2f}, {4.2f, 3.4f}, side * 0.5f};
        Ellipse lo{{side * 3.0f, 2.6f}, {2.8f, 2.4f}, -side * 0.4f};
        auto w = unite(up, lo);
        fill(cv, grow(w, 0.6f), Solid{hex(0x2a1a1a)});
        fill(cv, w, Radial{{side * 1.0f, 0}, 7, wings[v][0], wings[v][1]});
        fill(cv, Circle{{side * 5.6f, -3.6f}, 0.8f}, Solid{{1, 1, 1, 0.9f}}, Blend::Atop);
      }
      fill(cv, Capsule{{0, -3.5f}, {0, 4}, 0.9f, 0.6f}, Solid{hex(0x2a1a1a)});
    });
  }
}

void bake_meadow(Atlas& a) {
  bake_trees(a);
  bake_bushes(a);
  bake_flowers(a);
  bake_grass(a);
  bake_rocks(a);
  bake_water_plants(a);
  bake_brambles(a);
  bake_small_things(a);

  RegionArt& R = S.ra;
  const Color warm{1.0f, 0.85f, 0.6f, 1};
  (void)warm;
  R.edge.add(M.tree[5], 0.55f).r(10).col({0, -3}).sw(0.12f).sh(2.4f).sc(0.85f, 1.15f).noflip().rev(40, 112);
  R.edge.add(M.tree[0], 0.15f).r(10).col({0, -3}).sw(0.12f).sh(2.4f).sc(0.85f, 1.15f).noflip().rev(40, 112);
  R.edge.add(M.tree[1], 0.1f).r(10).col({0, -3}).sw(0.12f).sh(2.4f).sc(0.85f, 1.15f).noflip().rev(40, 112);
  R.edge.add(M.tree[2], 0.2f).r(9).col({0, -3}).sw(0.12f).sh(2.2f).sc(0.85f, 1.15f).noflip().rev(34, 118);
  R.trees.add(M.tree[0], 0.18f).r(9).col({0, -3}).sw(0.14f).sh(2.6f).noflip().rev(40, 112);
  R.trees.add(M.tree[1], 0.18f).r(9).col({0, -3}).sw(0.14f).sh(2.6f).noflip().rev(40, 112);
  R.trees.add(M.tree[2], 0.22f).r(8).col({0, -3}).sw(0.14f).sh(2.2f).noflip().rev(34, 118);
  R.trees.add(M.tree[4], 0.2f).r(9).col({0, -3}).sw(0.14f).sh(2.6f).noflip().rev(40, 112);
  R.trees.add(M.tree[3], 0.22f).r(9).col({0, -3}).sw(0.14f).sh(2.6f).noflip().rev(42, 112).em(EMIT_PETALS);
  for (int i = 0; i < 4; i++) R.bushes.add(M.bush[i]).r(12).col({0, -6}).sw(0.06f).sh(1.2f).sc(0.85f, 1.15f).noflip().rev(18, 30);
  const f32 rock_r[4] = {15, 11, 20, 0}, rock_sh[4] = {1.0f, 1.0f, 1.6f, 0.0f}, rock_w[4] = {0.35f, 0.35f, 0.15f, 0.15f};
  for (int i = 0; i < 4; i++) R.rocks.add(M.rock[i], rock_w[i]).r(rock_r[i]).col({0, -5}).sh(rock_sh[i]).sc(1, 1).noflip();
  for (int i = 0; i < 6; i++) R.cover.add(M.grass[i], i >= 4 ? 0.15f : 1.0f).sw(0.6f).sc(0.8f, 1.25f);
  const f32 fw[8] = {3, 2, 2, 3, 2, 2, 1.5f, 2.5f};
  for (int i = 0; i < 8; i++) R.flowers.add(M.flower[i], fw[i]).sw(0.45f).sc(0.8f, 1.15f);
  for (int i = 0; i < 2; i++) R.shore.add(M.reed[i]).sw(0.55f).sc(0.8f, 1.15f);
  R.floaters.add(M.lily[0], 3).flat().sc(0.8f, 1.2f);
  R.floaters.add(M.lily[1], 1).flat().sc(0.8f, 1.2f);
  R.extras.add(M.log).r(14).col({-6, -6}).sc(1, 1);
  R.extras.add(M.stump).r(12).col({0, -6}).sh(0.9f).sc(1, 1);
  for (int i = 0; i < 2; i++) R.near_trees.add(M.mushroom[i]).sc(0.9f, 1.1f);
  for (int i = 0; i < 2; i++) R.path_bits.add(M.pebble[i]).flat().sc(1, 1);
  R.blocker[0] = M.bramble[0];
  R.blocker[1] = M.bramble[1];
  R.blocker_burnt = M.bramble_burnt;
}

}  // namespace sky
