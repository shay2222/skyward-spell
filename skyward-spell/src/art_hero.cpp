// art_hero.cpp - Mira and Theo, painted as paper-doll pieces in three views (front, back, side).
// Each piece is painted around its joint (hip, shoulder, neck, chain root) so the animation
// code can rotate and move it smoothly. Coordinates are world units; y grows downward.
#include "art.h"

namespace sky {
using namespace paint;

namespace {

struct Look {
  bool girl;
  Color skin, skin_light, skin_dark;
  Color hair, hair_light, hair_dark;
  Color tunic, tunic_light, tunic_dark, trim;
  Color scarf, scarf_dark, scarf_stripe;
  Color pants, boots, boots_dark;
  Color bag, metal;
  Color eye, iris;
  Color line, mouth, blush;
};

const Look MIRA{
    true,
    hex(0xf4c8a4), hex(0xffe4cc), hex(0xd99a78),
    hex(0x86422c), hex(0xc8774a), hex(0x4e2218),
    hex(0x4c97d6), hex(0x93cff5), hex(0x2a64a2), hex(0xf6e2b0),
    hex(0xea5a4e), hex(0xb43a36), hex(0xffd7a8),
    hex(0x33385a), hex(0x7c4a2c), hex(0x4b2a18),
    hex(0xc28c4c), hex(0xf2c14e),
    hex(0x2c2238), hex(0x4f9a66),
    hex(0x3a2030), hex(0x7a2e34), hex(0xff8a86, 0.45f),
};

const Look THEO{
    false,
    hex(0xd9a07a), hex(0xf2c6a2), hex(0xb47552),
    hex(0x33261f), hex(0x6a5444), hex(0x1a120e),
    hex(0xc8423b), hex(0xf27c66), hex(0x8c2a2a), hex(0xf2cf7a),
    hex(0xf0b83a), hex(0xc0861c), hex(0xfff0b8),
    hex(0x4a4036), hex(0x6c4226), hex(0x40220f),
    hex(0x9c6b3a), hex(0xe8c25a),
    hex(0x241a16), hex(0x8a5428),
    hex(0x2e1c1a), hex(0x6a2a28), hex(0xf08070, 0.4f),
};

constexpr f32 SC = 3.0f;    // texels per world unit for characters
constexpr f32 LW = 0.95f;   // outline width

template <class Sh>
void part(Canvas& cv, const Sh& shape, Color base, Color light, Color dark, Vec2 center, f32 radius, Color line, f32 rim = 2.0f) {
  fill(cv, grow(shape, LW), Solid{line});
  fill(cv, shape, Volume{base, light, dark, center, radius, rim, 0.3f});
}

// ---------------------------------------------------------------- legs, arms, torso
void leg_front(Canvas& cv, const Look& L) {
  Capsule pants{{0, 0}, {0, 8.6f}, 3.1f, 2.7f};
  Capsule shin{{0, 7.4f}, {0, 10.6f}, 3.0f, 3.0f};
  Ellipse sole{{0, 11.0f}, {3.5f, 1.9f}};
  fill(cv, grow(unite(pants, unite(shin, sole)), LW), Solid{L.line});
  fill(cv, pants, Volume{L.pants, lighten(L.pants, 0.3f), darken(L.pants, 0.35f), {-1.5f, 2}, 6});
  auto boot = unite(shin, sole);
  fill(cv, boot, Volume{L.boots, lighten(L.boots, 0.32f), L.boots_dark, {-1.2f, 8.5f}, 5});
  fill(cv, RBox{{0, 7.7f}, {3.4f, 0.75f}, 0.5f}, Solid{lighten(L.boots, 0.25f)}, Blend::Atop);
  fill(cv, RBox{{0, 12.4f}, {4, 0.7f}}, Solid{darken(L.boots_dark, 0.3f)}, Blend::Atop);
}

void leg_side(Canvas& cv, const Look& L) {
  Capsule pants{{0, 0}, {0.4f, 8.6f}, 3.0f, 2.6f};
  Capsule shin{{0.3f, 7.6f}, {0.5f, 10.4f}, 2.9f, 2.7f};
  Capsule foot{{0.0f, 10.9f}, {3.8f, 11.3f}, 2.0f, 1.7f};
  fill(cv, grow(unite(pants, unite(shin, foot)), LW), Solid{L.line});
  fill(cv, pants, Volume{L.pants, lighten(L.pants, 0.3f), darken(L.pants, 0.35f), {-1, 2}, 6});
  fill(cv, unite(shin, foot), Volume{L.boots, lighten(L.boots, 0.32f), L.boots_dark, {0, 8.5f}, 5});
  fill(cv, RBox{{0.3f, 7.8f}, {3.2f, 0.75f}, 0.5f}, Solid{lighten(L.boots, 0.25f)}, Blend::Atop);
  fill(cv, RBox{{1.5f, 12.6f}, {5, 0.6f}}, Solid{darken(L.boots_dark, 0.3f)}, Blend::Atop);
}

void arm(Canvas& cv, const Look& L) {
  Capsule sleeve{{0, 0}, {0, 6.6f}, 2.75f, 2.35f};
  Circle hand{{0, 8.9f}, 2.2f};
  fill(cv, grow(unite(sleeve, hand), LW), Solid{L.line});
  fill(cv, hand, Volume{L.skin, L.skin_light, L.skin_dark, {-0.8f, 8}, 3});
  fill(cv, sleeve, Volume{L.tunic, L.tunic_light, L.tunic_dark, {-1, 1}, 6});
  fill(cv, RBox{{0, 6.5f}, {2.6f, 0.8f}, 0.4f}, Solid{L.trim}, Blend::Atop);
}

Poly tunic_shape(View v) {
  Path p;
  if (v == VIEW_SIDE) {
    p.move({-3.8f, -11.0f}).cubic({-5.4f, -10.6f}, {-5.6f, -6}, {-5.2f, -3}).line({-6.4f, 4.0f}).quad({0, 5.8f}, {6.8f, 4.0f}).line({5.8f, -3.0f}).cubic({6.8f, -6.0f}, {6.0f, -10.8f}, {3.6f, -11.2f}).quad({0, -12.0f}, {-3.8f, -11.0f});
  } else {
    p.move({-6.0f, -11.2f}).cubic({-8.2f, -11.0f}, {-8.4f, -7.0f}, {-7.8f, -3.2f}).line({-9.4f, 4.0f}).quad({0, 6.2f}, {9.4f, 4.0f}).line({7.8f, -3.2f}).cubic({8.4f, -7.0f}, {8.2f, -11.0f}, {6.0f, -11.2f}).quad({0, -12.6f}, {-6.0f, -11.2f});
  }
  return p.poly();
}

void torso(Canvas& cv, const Look& L, View v) {
  Poly t = tunic_shape(v);
  fill(cv, grow(t, LW), Solid{L.line});
  fill(cv, t, noisy(Volume{L.tunic, L.tunic_light, L.tunic_dark, {v == VIEW_SIDE ? -1.0f : -2.5f, -5}, 13, 2.5f, 0.3f}, 0.012f, 1.2f, 3));
  // hem trim and belt
  fill(cv, RBox{{0, 4.8f}, {12, 1.5f}}, Solid{L.trim}, Blend::Atop);
  fill(cv, RBox{{0, 3.2f}, {12, 0.3f}}, Solid{darken(L.trim, 0.45f).with_alpha(0.6f)}, Blend::Atop);
  Color belt = darken(L.bag, 0.35f);
  fill(cv, RBox{{0, -1.0f}, {12, 1.15f}}, Solid{belt}, Blend::Atop);
  Vec2 buckle = v == VIEW_SIDE ? Vec2{4.6f, -1.0f} : Vec2{0, -1.0f};
  if (v != VIEW_BACK) {
    fill(cv, RBox{buckle, {1.6f, 1.3f}, 0.4f}, Solid{L.metal});
    fill(cv, RBox{buckle, {0.7f, 0.5f}, 0.2f}, Solid{darken(L.metal, 0.5f)});
  }
  // gentle fold shading
  Path f1, f2;
  f1.move({-3.5f, -6}).quad({-4.5f, -2}, {-4.2f, 3});
  f2.move({3.2f, -5}).quad({4.4f, -1}, {5.0f, 3});
  fill(cv, f1.stroke(1.4f, 0.6f), Solid{L.tunic_dark.with_alpha(0.22f)}, Blend::Atop);
  if (v != VIEW_SIDE) fill(cv, f2.stroke(1.4f, 0.6f), Solid{L.tunic_dark.with_alpha(0.22f)}, Blend::Atop);
  // collar and satchel strap
  if (v == VIEW_FRONT) {
    Path vcol;
    vcol.move({-2.6f, -11.8f}).line({0, -8.0f}).line({2.6f, -11.8f});
    fill(cv, vcol.poly(), Solid{L.skin_dark}, Blend::Atop);
    fill(cv, vcol.stroke(0.8f), Solid{L.trim}, Blend::Atop);
  }
  if (v != VIEW_SIDE) {
    Path strap;
    f32 s = v == VIEW_FRONT ? 1.0f : -1.0f;
    strap.move({-5.4f * s, -11.0f}).line({7.0f * s, 0.8f});
    fill(cv, strap.stroke(2.4f), Solid{darken(L.bag, 0.5f)}, Blend::Atop);
    fill(cv, strap.stroke(1.6f), Solid{L.bag}, Blend::Atop);
  } else {
    Path strap;
    strap.move({1.0f, -11.4f}).line({-2.5f, 0.0f});
    fill(cv, strap.stroke(2.0f), Solid{L.bag}, Blend::Atop);
    RBox bag{{-6.6f, 0.6f}, {3.4f, 3.4f}, 1.3f};
    part(cv, bag, L.bag, lighten(L.bag, 0.3f), darken(L.bag, 0.4f), {-7.5f, -0.5f}, 5, L.line);
    fill(cv, RBox{{-6.6f, -1.0f}, {3.6f, 1.6f}, 1.0f}, Solid{darken(L.bag, 0.2f)}, Blend::Atop);
    fill(cv, Circle{{-6.6f, 0.4f}, 0.6f}, Solid{L.metal});
  }
}

void bag_front(Canvas& cv, const Look& L) {
  RBox bag{{0, 2.8f}, {3.6f, 3.2f}, 1.3f};
  part(cv, bag, L.bag, lighten(L.bag, 0.3f), darken(L.bag, 0.4f), {-1, 1.5f}, 5, L.line);
  fill(cv, RBox{{0, 1.0f}, {3.8f, 1.7f}, 1.0f}, Solid{darken(L.bag, 0.18f)}, Blend::Atop);
  fill(cv, Circle{{0, 2.3f}, 0.6f}, Solid{L.metal});
}

// ---------------------------------------------------------------- heads and faces
void head(Canvas& cv, const Look& L, View v) {
  Vec2 hc = v == VIEW_SIDE ? Vec2{0.6f, -11.2f} : Vec2{0, -11.2f};
  Capsule neck{{v == VIEW_SIDE ? 0.6f : 0.0f, 1.5f}, {v == VIEW_SIDE ? 0.2f : 0.0f, -3.5f}, 2.4f, 2.4f};
  fill(cv, grow(neck, LW), Solid{L.line});
  fill(cv, neck, Solid{L.skin_dark});
  if (v != VIEW_SIDE) {
    for (int s = -1; s <= 1; s += 2) {
      Circle ear{{s * 11.0f, -10.2f}, 2.7f};
      part(cv, ear, L.skin, L.skin_light, L.skin_dark, ear.c + Vec2{-0.5f, -0.5f}, 3, L.line);
      fill(cv, Ellipse{ear.c + Vec2{s * 0.3f, 0.2f}, {1.2f, 1.6f}}, Solid{L.skin_dark.with_alpha(0.6f)});
    }
  }
  if (v == VIEW_SIDE) {
    auto shape = unite(Ellipse{hc, {11.2f, 11.1f}}, Circle{{11.0f, -8.4f}, 1.6f});
    part(cv, shape, L.skin, L.skin_light, L.skin_dark, {hc.x - 2, hc.y - 3.5f}, 15, L.line, 2.5f);
    Ellipse ear{{-2.6f, -10.2f}, {2.4f, 2.9f}};
    part(cv, ear, L.skin, L.skin_light, L.skin_dark, ear.c + Vec2{-0.5f, -0.5f}, 3, L.line);
    fill(cv, Ellipse{ear.c + Vec2{0.3f, 0.2f}, {1.1f, 1.7f}}, Solid{L.skin_dark.with_alpha(0.6f)});
    fill(cv, Ellipse{{6.4f, -6.7f}, {2.2f, 1.3f}}, Solid{L.blush}, Blend::Atop, 1.4f);
    Path m;
    m.move({8.0f, -5.3f}).quad({9.2f, -4.4f}, {10.3f, -5.4f});
    fill(cv, m.stroke(0.75f), Solid{L.mouth});
    return;
  }
  Ellipse h{hc, {11.7f, 11.1f}};
  part(cv, h, L.skin, L.skin_light, L.skin_dark, {-3.5f, -14.5f}, 15, L.line, 2.5f);
  if (v == VIEW_FRONT) {
    for (int s = -1; s <= 1; s += 2) fill(cv, Ellipse{{s * 6.6f, -6.9f}, {2.3f, 1.35f}}, Solid{L.blush}, Blend::Atop, 1.4f);
    fill(cv, Ellipse{{0, -7.9f}, {0.8f, 0.55f}}, Solid{L.skin_dark});
    Path m;
    m.move({-1.7f, -5.2f}).quad({0, -3.7f}, {1.7f, -5.2f});
    fill(cv, m.stroke(0.75f), Solid{L.mouth});
  }
}

void brow(Canvas& cv, const Look& L, Vec2 c, f32 s) {
  Path b;
  if (L.girl) b.move(c + Vec2{-1.9f * s, -4.3f}).quad(c + Vec2{0, -5.5f}, c + Vec2{2.0f * s, -4.6f});
  else b.move(c + Vec2{-2.1f * s, -4.6f}).quad(c + Vec2{0, -5.4f}, c + Vec2{2.1f * s, -4.9f});
  fill(cv, b.stroke(L.girl ? 0.7f : 1.05f, L.girl ? 0.45f : 0.8f), Solid{L.hair_dark});
}

void eye_open(Canvas& cv, const Look& L, Vec2 c, Vec2 r, f32 outer) {
  fill(cv, Ellipse{c, r + Vec2{0.25f, 0.25f}}, Solid{L.eye});
  fill(cv, Ellipse{c + Vec2{0, 0.55f}, r * Vec2{0.8f, 0.74f}}, Linear{c + Vec2{0, -1.2f}, c + Vec2{0, r.y}, L.eye, L.iris});
  fill(cv, Circle{c + Vec2{-r.x * 0.32f, -r.y * 0.4f}, r.x * 0.42f}, Solid{{1, 1, 1, 1}});
  fill(cv, Circle{c + Vec2{r.x * 0.35f, r.y * 0.38f}, r.x * 0.2f}, Solid{{1, 1, 1, 0.85f}});
  Path lid;
  lid.move(c + Vec2{-r.x - 0.2f, -r.y * 0.45f}).quad(c + Vec2{0, -r.y * 1.35f}, c + Vec2{r.x + 0.2f, -r.y * 0.45f});
  fill(cv, lid.stroke(L.girl ? 0.95f : 0.75f, 0.5f), Solid{L.eye});
  if (L.girl) {
    Path lash;
    lash.move(c + Vec2{outer * (r.x - 0.1f), -r.y * 0.55f}).line(c + Vec2{outer * (r.x + 1.0f), -r.y * 0.9f});
    fill(cv, lash.stroke(0.65f, 0.3f), Solid{L.eye});
  }
}

void eye_closed(Canvas& cv, const Look& L, Vec2 c, Vec2 r) {
  Path p;
  p.move(c + Vec2{-r.x, 0.1f}).quad(c + Vec2{0, 1.4f}, c + Vec2{r.x, 0.1f});
  fill(cv, p.stroke(0.85f, 0.6f), Solid{L.eye});
}

void eyes(Canvas& cv, const Look& L, View v, bool open) {
  if (v == VIEW_SIDE) {
    Vec2 c{6.4f, -10.6f};
    brow(cv, L, c, 1);
    if (open) eye_open(cv, L, c, {1.55f, 2.85f}, 1);
    else eye_closed(cv, L, c, {1.6f, 2.8f});
    return;
  }
  for (int s = -1; s <= 1; s += 2) {
    Vec2 c{s * 4.6f, -10.6f};
    brow(cv, L, c, (f32)s);
    if (open) eye_open(cv, L, c, {2.15f, 2.95f}, (f32)s);
    else eye_closed(cv, L, c, {2.2f, 2.9f});
  }
}

// ---------------------------------------------------------------- hair
void hair_finish(Canvas& cv, const Poly& shape, const Look& L, Vec2 light_center, f32 radius) {
  fill(cv, grow(shape, LW), Solid{L.line});
  fill(cv, shape, noisy(Volume{L.hair, L.hair_light, L.hair_dark, light_center, radius, 2.2f, 0.35f}, 0.02f, 1.6f, 21));
}

void hair_strands(Canvas& cv, const Look& L, const Vec2* starts, const Vec2* ctrls, const Vec2* ends, int n) {
  for (int i = 0; i < n; i++) {
    Path s;
    s.move(starts[i]).quad(ctrls[i], ends[i]);
    fill(cv, s.stroke(0.6f, 0.25f), Solid{L.hair_dark.with_alpha(0.55f)}, Blend::Atop);
  }
}

void hair_shine(Canvas& cv, const Look& L, Vec2 a, Vec2 c1, Vec2 b, Vec2 c2, Vec2 e) {
  Path s;
  s.move(a).quad(c1, b).quad(c2, e);
  fill(cv, s.stroke(1.7f, 0.5f), Solid{L.hair_light.with_alpha(0.75f)}, Blend::Atop);
}

void mira_hair(Canvas& cv, const Look& L, View v) {
  Path p;
  if (v == VIEW_FRONT) {
    p.move({-12.0f, -3.2f}).cubic({-13.6f, -8}, {-14.2f, -15}, {-12.2f, -19.5f}).cubic({-10, -24.5f}, {-4.5f, -25.8f}, {0.5f, -25.4f})
        .cubic({6, -25.2f}, {11, -23.2f}, {12.8f, -18.6f}).cubic({14.4f, -14.4f}, {13.6f, -8}, {12.2f, -3.2f})
        .quad({11.2f, -6.5f}, {10.6f, -10.6f}).quad({10.2f, -13.4f}, {8.6f, -14.4f}).quad({7.2f, -12.4f}, {6.0f, -10.6f})
        .quad({5.4f, -13.6f}, {3.4f, -15.2f}).quad({2.2f, -13.0f}, {1.0f, -11.8f}).quad({0.0f, -14.4f}, {-1.6f, -15.4f})
        .quad({-2.8f, -13.0f}, {-4.0f, -11.4f}).quad({-4.8f, -14.4f}, {-6.8f, -15.2f}).quad({-8.4f, -13.2f}, {-9.4f, -11.4f})
        .quad({-10.6f, -12.4f}, {-10.9f, -10.0f}).quad({-11.2f, -6.5f}, {-12.0f, -3.2f});
    hair_finish(cv, p.poly(), L, {-3, -19}, 14);
    Vec2 s0[3] = {{-2, -24.5f}, {3, -24.6f}, {-6, -22}}, c0[3] = {{-7, -20}, {8, -20}, {-11, -14}}, e0[3] = {{-8.5f, -14.5f}, {9.5f, -14.5f}, {-11.6f, -6}};
    hair_strands(cv, L, s0, c0, e0, 3);
    hair_shine(cv, L, {-9.5f, -19.0f}, {-5, -22.8f}, {0, -22.6f}, {6, -22.8f}, {10.2f, -19.2f});
  } else if (v == VIEW_BACK) {
    p.move({0, -25.6f}).cubic({7.5f, -25.6f}, {13.2f, -21}, {13.2f, -14}).cubic({13.4f, -7}, {15.2f, -1}, {13.6f, 4.2f})
        .quad({9, 6.8f}, {4.6f, 5.4f}).quad({0, 6.6f}, {-4.6f, 5.4f}).quad({-9, 6.8f}, {-13.6f, 4.2f})
        .cubic({-15.2f, -1}, {-13.4f, -7}, {-13.2f, -14}).cubic({-13.2f, -21}, {-7.5f, -25.6f}, {0, -25.6f});
    hair_finish(cv, p.poly(), L, {-3, -18}, 18);
    Vec2 s0[5] = {{0, -18}, {-4, -18}, {4, -18}, {-8, -15}, {8, -15}}, c0[5] = {{0.5f, -6}, {-5.5f, -6}, {5.5f, -6}, {-11, -4}, {11, -4}},
         e0[5] = {{0, 5}, {-5, 4.5f}, {5, 4.5f}, {-11.5f, 3}, {11.5f, 3}};
    hair_strands(cv, L, s0, c0, e0, 5);
    hair_shine(cv, L, {-10, -16}, {-6, -22.5f}, {0, -22.5f}, {6, -22.5f}, {10, -16});
    Ellipse tie{{0, -19.6f}, {2.9f, 1.5f}};
    fill(cv, grow(tie, LW * 0.8f), Solid{L.line});
    fill(cv, tie, Volume{L.scarf, lighten(L.scarf, 0.4f), L.scarf_dark, {-1, -20.4f}, 3});
  } else {
    p.move({11.0f, -12.2f}).quad({11.6f, -16}, {10.6f, -19.5f}).cubic({8.4f, -24.6f}, {2, -26.0f}, {-3.5f, -25.0f})
        .cubic({-9.5f, -23.8f}, {-13.4f, -18.5f}, {-13.2f, -12}).cubic({-13.0f, -6}, {-12.6f, 0}, {-11.0f, 4.0f})
        .quad({-8.4f, 5.6f}, {-5.8f, 4.0f}).cubic({-5.6f, 0.5f}, {-5.0f, -3.5f}, {-5.2f, -6.0f}).quad({-5.0f, -9.5f}, {-1.6f, -13.6f})
        .quad({0.6f, -12.4f}, {2.0f, -11.2f}).quad({3.0f, -13.6f}, {4.8f, -14.6f}).quad({6.0f, -12.6f}, {7.4f, -11.2f})
        .quad({8.4f, -13.8f}, {9.6f, -14.0f}).quad({10.4f, -13.2f}, {11.0f, -12.2f});
    hair_finish(cv, p.poly(), L, {0, -19}, 15);
    Vec2 s0[3] = {{2, -24.6f}, {-4, -24.4f}, {-9, -20}}, c0[3] = {{-4, -19}, {-9, -16}, {-12, -9}}, e0[3] = {{-4.5f, -10}, {-9.8f, -4}, {-10.8f, 3}};
    hair_strands(cv, L, s0, c0, e0, 3);
    hair_shine(cv, L, {-9, -19}, {-6, -23.2f}, {-1, -23.6f}, {5, -23.2f}, {9.2f, -19});
    Ellipse tie{{-10.2f, -19.4f}, {1.4f, 2.6f}, -0.5f};
    fill(cv, grow(tie, LW * 0.8f), Solid{L.line});
    fill(cv, tie, Volume{L.scarf, lighten(L.scarf, 0.4f), L.scarf_dark, {-11, -20.4f}, 3});
  }
}

void mira_hair_back_front_view(Canvas& cv, const Look& L) {
  Path p;
  p.move({-12.6f, -17}).cubic({-15.4f, -10}, {-15.6f, -1}, {-13.4f, 4.2f}).quad({-9, 6.6f}, {-4.4f, 5.2f}).quad({0, 6.2f}, {4.4f, 5.2f})
      .quad({9, 6.6f}, {13.4f, 4.2f}).cubic({15.6f, -1}, {15.4f, -10}, {12.6f, -17}).quad({0, -27}, {-12.6f, -17});
  Poly sh = p.poly();
  fill(cv, grow(sh, LW), Solid{L.line});
  fill(cv, sh, Volume{darken(L.hair, 0.15f), L.hair, darken(L.hair_dark, 0.15f), {-4, -10}, 18, 2.2f, 0.3f});
}

void theo_hair(Canvas& cv, const Look& L, View v) {
  Path p;
  if (v == VIEW_FRONT) {
    p.move({-12.2f, -8.0f}).quad({-13.4f, -12}, {-12.8f, -17}).line({-15.2f, -19.6f}).quad({-12.6f, -21.4f}, {-11.0f, -22.6f})
        .line({-12.0f, -26.4f}).quad({-7.8f, -25.4f}, {-5.2f, -26.4f}).line({-3.6f, -29.2f}).quad({-0.4f, -26.4f}, {2.2f, -27.2f})
        .line({5.4f, -29.0f}).quad({6.6f, -26}, {9.6f, -24.2f}).line({13.2f, -24.4f}).quad({12.6f, -20.6f}, {13.0f, -18.0f})
        .quad({13.6f, -12}, {12.2f, -8.0f}).line({11.0f, -8.4f}).quad({10.8f, -11.5f}, {10.2f, -12.6f})
        .line({8.0f, -12.0f}).line({7.4f, -15.2f}).line({5.0f, -12.4f}).line({4.0f, -15.4f}).line({1.4f, -12.6f}).line({0.2f, -15.8f})
        .line({-2.6f, -12.8f}).line({-3.6f, -15.8f}).line({-6.4f, -12.6f}).line({-7.6f, -15.0f}).line({-10.2f, -12.8f})
        .quad({-10.8f, -11}, {-11.0f, -8.4f}).line({-12.2f, -8.0f});
    hair_finish(cv, p.poly(), L, {-3, -21}, 13);
    Vec2 s0[3] = {{-2, -25}, {4, -25}, {-8, -22}}, c0[3] = {{-4, -20}, {6, -20}, {-10, -18}}, e0[3] = {{-5, -15.5f}, {6, -15}, {-11, -13}};
    hair_strands(cv, L, s0, c0, e0, 3);
    hair_shine(cv, L, {-9, -21}, {-6, -23.5f}, {-1, -23.6f}, {4, -23.6f}, {8.5f, -21});
  } else if (v == VIEW_BACK) {
    p.move({0, -27.0f}).line({3.8f, -29.0f}).quad({6.5f, -26.2f}, {9.6f, -24.6f}).line({13.4f, -24.0f}).quad({12.8f, -20}, {13.4f, -16})
        .quad({13.6f, -10}, {12.4f, -5.0f}).line({10.4f, -3.0f}).line({8.6f, -5.2f}).line({6.0f, -2.4f}).line({3.6f, -4.6f}).line({0.8f, -1.8f})
        .line({-1.8f, -4.6f}).line({-4.4f, -2.2f}).line({-6.8f, -4.8f}).line({-9.4f, -2.6f}).line({-11.6f, -5.4f})
        .quad({-13.8f, -10}, {-13.4f, -16}).line({-15.0f, -19.8f}).quad({-12.4f, -21.6f}, {-11.2f, -23.4f}).line({-11.6f, -27.0f})
        .quad({-6, -26.6f}, {0, -27.0f});
    hair_finish(cv, p.poly(), L, {-3, -19}, 17);
    Vec2 s0[4] = {{0, -24}, {-6, -22}, {6, -22}, {0, -16}}, c0[4] = {{-1, -15}, {-8, -12}, {8, -12}, {1, -9}}, e0[4] = {{0.5f, -5}, {-7, -5}, {7, -5}, {-1, -3}};
    hair_strands(cv, L, s0, c0, e0, 4);
    hair_shine(cv, L, {-10, -18}, {-6, -23.6f}, {0, -23.6f}, {6, -23.6f}, {10, -18});
  } else {
    p.move({10.8f, -12.4f}).quad({11.8f, -16}, {10.8f, -19.6f}).line({13.4f, -22.4f}).quad({9.8f, -24}, {8.0f, -24.6f})
        .line({8.6f, -28.4f}).quad({4.6f, -26.6f}, {2.0f, -27.4f}).line({0.6f, -30.0f}).quad({-2.4f, -27.4f}, {-5.0f, -26.6f})
        .line({-8.6f, -28.0f}).quad({-9.4f, -24.6f}, {-11.0f, -22.8f}).line({-15.4f, -21.4f}).quad({-13.6f, -18.4f}, {-13.4f, -15.4f})
        .line({-16.0f, -12.6f}).quad({-13.4f, -10.4f}, {-12.6f, -7.6f}).line({-13.0f, -3.4f}).quad({-10.6f, -4.4f}, {-8.6f, -4.0f})
        .quad({-6.6f, -6.6f}, {-5.6f, -8.8f}).quad({-4.0f, -12.2f}, {-1.0f, -13.2f}).line({1.2f, -11.4f}).line({2.6f, -14.4f})
        .line({4.8f, -11.8f}).line({6.0f, -14.8f}).line({8.4f, -12.0f}).line({9.2f, -14.4f}).line({10.8f, -12.4f});
    hair_finish(cv, p.poly(), L, {0, -21}, 15);
    Vec2 s0[3] = {{4, -25}, {-2, -25}, {-8, -21}}, c0[3] = {{1, -19}, {-7, -18}, {-12, -13}}, e0[3] = {{-1, -14}, {-9, -8}, {-11.5f, -5}};
    hair_strands(cv, L, s0, c0, e0, 3);
    hair_shine(cv, L, {-9, -20.5f}, {-5, -24}, {0, -24.2f}, {5, -24}, {9, -20.5f});
  }
}

/** Ponytail (Mira) or springy cowlick (Theo) pieces; each is painted from its root at (0,0) downward. */
void tail_piece(Canvas& cv, const Look& L, int which) {
  if (L.girl) {
    f32 r0[3] = {3.6f, 3.1f, 2.5f}, r1[3] = {3.1f, 2.5f, 0.6f}, len[3] = {6.2f, 6.0f, 7.0f};
    Capsule c{{0, 0}, {which == 2 ? 0.8f : 0.0f, len[which]}, r0[which], r1[which]};
    fill(cv, grow(c, LW), Solid{L.line});
    fill(cv, c, Volume{L.hair, L.hair_light, L.hair_dark, {-1.2f, 1}, 5, 1.5f, 0.35f});
    Path s;
    s.move({-0.6f, 0.5f}).quad({-1.2f, len[which] * 0.5f}, {-0.4f, len[which] * 0.95f});
    fill(cv, s.stroke(0.8f, 0.3f), Solid{L.hair_light.with_alpha(0.6f)}, Blend::Atop);
    if (which == 0) {
      Ellipse tie{{0, 0.5f}, {3.0f, 1.4f}};
      fill(cv, grow(tie, LW * 0.8f), Solid{L.line});
      fill(cv, tie, Volume{L.scarf, lighten(L.scarf, 0.4f), L.scarf_dark, {-1, 0}, 3});
    }
  } else {
    f32 r0[3] = {1.7f, 1.3f, 1.0f}, r1[3] = {1.2f, 0.9f, 0.25f}, len[3] = {4.6f, 4.0f, 4.0f};
    Capsule c{{0, 0}, {0.4f, len[which]}, r0[which], r1[which]};
    fill(cv, grow(c, LW * 0.85f), Solid{L.line});
    fill(cv, c, Volume{L.hair, L.hair_light, L.hair_dark, {-0.6f, 0.5f}, 3});
  }
}

// ---------------------------------------------------------------- scarf
void scarf_wrap(Canvas& cv, const Look& L, View v) {
  RBox band = v == VIEW_SIDE ? RBox{{0.8f, 0.4f}, {5.4f, 2.4f}, 2.2f} : RBox{{0, 0.5f}, {7.3f, 2.45f}, 2.3f};
  part(cv, band, L.scarf, lighten(L.scarf, 0.35f), L.scarf_dark, {-2, -0.5f}, 8, L.line);
  for (int i = -3; i <= 3; i++) {
    Path st;
    st.move({i * 2.2f - 0.8f, -2.4f}).line({i * 2.2f + 0.8f, 3.2f});
    fill(cv, st.stroke(0.7f), Solid{L.scarf_stripe.with_alpha(0.55f)}, Blend::Atop);
  }
  if (v != VIEW_BACK) {
    Vec2 k = v == VIEW_SIDE ? Vec2{5.6f, 2.0f} : Vec2{-4.4f, 2.2f};
    Circle knot{k, 2.1f};
    part(cv, knot, L.scarf, lighten(L.scarf, 0.35f), L.scarf_dark, k + Vec2{-0.6f, -0.6f}, 3, L.line);
  }
}

void scarf_seg(Canvas& cv, const Look& L, bool end) {
  RBox s = end ? RBox{{0, 2.9f}, {2.25f, 3.3f}, 1.4f} : RBox{{0, 3.2f}, {2.3f, 3.7f}, 1.6f};
  part(cv, s, L.scarf, lighten(L.scarf, 0.35f), L.scarf_dark, {-1, 1.5f}, 5, L.line);
  fill(cv, RBox{{0, end ? 2.0f : 3.2f}, {3, 0.55f}}, Solid{L.scarf_stripe.with_alpha(0.85f)}, Blend::Atop);
  if (end) {
    for (int i = -2; i <= 2; i++) {
      Path f;
      f.move({i * 0.95f, 5.8f}).line({i * 1.05f, 8.0f});
      fill(cv, f.stroke(0.75f, 0.5f), Solid{L.scarf_dark});
    }
  }
}

void bake_look(Atlas& a, HeroArt& H, const Look& L) {
  const Rect leg_box{-5, -2, 6, 14.5f};
  H.leg[VIEW_FRONT] = a.bake(leg_box, SC, [&](Canvas& cv) { leg_front(cv, L); });
  H.leg[VIEW_BACK] = H.leg[VIEW_FRONT];
  H.leg[VIEW_SIDE] = a.bake({-4.5f, -2, 7.5f, 14.5f}, SC, [&](Canvas& cv) { leg_side(cv, L); });
  H.arm = a.bake({-4, -4, 4, 12.5f}, SC, [&](Canvas& cv) { arm(cv, L); });
  for (int v = 0; v < 3; v++) {
    H.torso[v] = a.bake({-11.5f, -14, 11.5f, 8}, SC, [&](Canvas& cv) { torso(cv, L, (View)v); });
    H.head[v] = a.bake({-15.5f, -25, 15.5f, 4.5f}, SC, [&](Canvas& cv) { head(cv, L, (View)v); });
    H.scarf_wrap[v] = a.bake({-9, -3.5f, 9, 5.5f}, SC, [&](Canvas& cv) { scarf_wrap(cv, L, (View)v); });
    H.hair_front[v] = a.bake({-17.5f, -31.5f, 17, 9}, SC, [&](Canvas& cv) {
      if (L.girl) mira_hair(cv, L, (View)v);
      else theo_hair(cv, L, (View)v);
    });
    if (v != VIEW_BACK) {
      H.eyes[v] = a.bake({-8.5f, -17.5f, 9.5f, -6}, SC, [&](Canvas& cv) { eyes(cv, L, (View)v, true); });
      H.blink[v] = a.bake({-8.5f, -17.5f, 9.5f, -6}, SC, [&](Canvas& cv) { eyes(cv, L, (View)v, false); });
    }
  }
  H.eyes[VIEW_BACK] = H.blink[VIEW_BACK] = 0xffff;
  H.hair_back_f = L.girl ? a.bake({-17, -28.5f, 17, 8.5f}, SC, [&](Canvas& cv) { mira_hair_back_front_view(cv, L); }) : 0xffff;
  for (int i = 0; i < 3; i++) H.tail_seg[i] = a.bake({-5, -5, 5.5f, 9}, SC, [&](Canvas& cv) { tail_piece(cv, L, i); });
  H.scarf_seg[0] = a.bake({-3.8f, -1.5f, 3.8f, 8}, SC, [&](Canvas& cv) { scarf_seg(cv, L, false); });
  H.scarf_seg[1] = a.bake({-3.8f, -1.5f, 3.8f, 9.5f}, SC, [&](Canvas& cv) { scarf_seg(cv, L, true); });
  H.bag = a.bake({-5, -1.5f, 5, 7.5f}, SC, [&](Canvas& cv) { bag_front(cv, L); });
}

}  // namespace

void bake_hero(Atlas& a) {
  bake_look(a, S.hero[0], MIRA);
  bake_look(a, S.hero[1], THEO);
}

}  // namespace sky
