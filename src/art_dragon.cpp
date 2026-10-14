// art_dragon.cpp - Flint, a small jade dragon whose wings are bound by Malgrin's hex.
// Painted as paper-doll pieces in three views; every piece is drawn around its joint.
#include "art.h"

namespace sky {
using namespace paint;

namespace {

constexpr f32 SC = 3.0f;
constexpr f32 LW = 0.95f;

const Color BODY = hex(0x47c193), BODY_L = hex(0x9ee9c2), BODY_D = hex(0x21806a);
const Color LINE = hex(0x15433c);
const Color BELLY = hex(0xffe3b4), BELLY_D = hex(0xe2ae76);
const Color WING = hex(0xff9478), WING_D = hex(0xd65a58);
const Color HORN = hex(0xfff2d4), HORN_D = hex(0xd9b884);
const Color SPIKE = hex(0xff8a6c);
const Color EYE = hex(0x1d1f36), IRIS = hex(0xf2b33b);
const Color BLUSH = hex(0xff8aa0, 0.42f);
const Color HEX_DARK = hex(0x3a1658);

template <class Sh>
void part(Canvas& cv, const Sh& shape, Color base, Color light, Color dark, Vec2 center, f32 radius, f32 rim = 2.2f) {
  fill(cv, grow(shape, LW), Solid{LINE});
  fill(cv, shape, Volume{base, light, dark, center, radius, rim, 0.32f});
}
template <class Sh>
void skin(Canvas& cv, const Sh& shape, Vec2 center, f32 radius) {
  fill(cv, grow(shape, LW), Solid{LINE});
  fill(cv, shape, noisy(Volume{BODY, BODY_L, BODY_D, center, radius, 2.4f, 0.32f}, 0.018f, 1.4f, 5));
}

void spike(Canvas& cv, Vec2 base, Vec2 dir, f32 size) {
  Vec2 n = perp(dir);
  Path p;
  p.move(base - n * size * 0.7f).quad(base + dir * size * 0.6f - n * size * 0.2f, base + dir * size * 1.3f).quad(base + dir * size * 0.5f + n * size * 0.3f, base + n * size * 0.7f).line(base - n * size * 0.7f);
  fill(cv, grow(p.poly(), LW * 0.8f), Solid{LINE});
  fill(cv, p.poly(), Linear{base, base + dir * size * 1.3f, darken(SPIKE, 0.15f), lighten(SPIKE, 0.25f)});
}

void belly_lines(Canvas& cv, Vec2 c, f32 w, f32 h, int n) {
  for (int i = 1; i < n; i++) {
    f32 y = c.y - h + (2 * h) * i / n;
    Path p;
    p.move({c.x - w * 0.8f, y - 0.4f}).quad({c.x, y + 0.6f}, {c.x + w * 0.8f, y - 0.4f});
    fill(cv, p.stroke(0.55f), Solid{BELLY_D.with_alpha(0.75f)}, Blend::Atop);
  }
}

// ---------------------------------------------------------------- body
void body(Canvas& cv, View v) {
  if (v == VIEW_SIDE) {
    for (int i = 0; i < 4; i++) {
      f32 t = i / 3.0f;
      Vec2 b{lerp(-9.5f, 2.5f, t), lerp(-15.0f, -18.0f, t) + (i == 0 ? 1.2f : 0)};
      spike(cv, b, normalize(Vec2{-0.35f, -1}), 2.4f - t * 0.5f);
    }
    auto sh = unite(Ellipse{{-1, -9}, {11.8f, 8.4f}}, Ellipse{{5.2f, -11}, {6.4f, 7.6f}});
    skin(cv, sh, {-2, -13}, 15);
    Ellipse belly{{1.8f, -4.8f}, {9.6f, 3.9f}};
    fill(cv, belly, Volume{BELLY, lighten(BELLY, 0.4f), BELLY_D, {0, -6}, 10}, Blend::Atop);
    for (int i = 0; i < 5; i++) {
      Path p;
      f32 x = -5.5f + i * 3.2f;
      p.move({x, -8.4f}).quad({x + 0.6f, -5}, {x, -1.2f});
      fill(cv, p.stroke(0.5f), Solid{BELLY_D.with_alpha(0.8f)}, Blend::Atop);
    }
    return;
  }
  auto sh = unite(Ellipse{{0, -10.5f}, {10.2f, 9.8f}}, Ellipse{{0, -6.5f}, {11.2f, 6.8f}});
  if (v == VIEW_BACK) {
    skin(cv, sh, {-2.5f, -13}, 15);
    for (int i = 0; i < 4; i++) spike(cv, {0, -17.5f + i * 4.0f}, {0, -1}, 2.3f - i * 0.25f);
    return;
  }
  skin(cv, sh, {-2.5f, -13}, 15);
  Ellipse belly{{0, -8.4f}, {6.3f, 7.4f}};
  fill(cv, grow(belly, 0.5f), Solid{BELLY_D});
  fill(cv, belly, Volume{BELLY, lighten(BELLY, 0.45f), BELLY_D, {-2, -11}, 9});
  belly_lines(cv, {0, -8.4f}, 6.3f, 7.4f, 5);
}

// ---------------------------------------------------------------- head
enum Mouth { MOUTH_SMILE, MOUTH_OPEN };

void horn(Canvas& cv, Vec2 a, Vec2 b) {
  Capsule h{a, b, 1.9f, 0.5f};
  fill(cv, grow(h, LW), Solid{LINE});
  fill(cv, h, Linear{a, b, HORN_D, HORN});
}

void fin(Canvas& cv, Vec2 a, Vec2 tip, Vec2 b) {
  Path p;
  p.move(a).quad((a + tip) * 0.5f + perp(normalize(tip - a)) * 0.8f, tip).quad((tip + b) * 0.5f, b).line(a);
  fill(cv, grow(p.poly(), LW * 0.85f), Solid{LINE});
  fill(cv, p.poly(), Linear{a, tip, WING_D, WING});
}

void head(Canvas& cv, View v, Mouth mouth) {
  if (v == VIEW_SIDE) {
    horn(cv, {-2.6f, -13.2f}, {-8.6f, -17.8f});
    fin(cv, {-4.0f, -11.5f}, {-10.6f, -11.0f}, {-4.6f, -6.6f});
    for (int i = 0; i < 3; i++) spike(cv, {-1.0f - i * 2.8f, -15.0f + i * 0.9f}, normalize(Vec2{-0.5f, -1}), 1.6f);
    auto sh = unite(Ellipse{{1.5f, -7.8f}, {8.8f, 8.0f}}, Ellipse{{8.2f, -4.6f}, {5.4f, 3.9f}});
    skin(cv, sh, {0, -11}, 12);
    fill(cv, Ellipse{{8.6f, -3.6f}, {4.6f, 2.6f}}, Solid{lighten(BODY, 0.25f).with_alpha(0.5f)}, Blend::Atop, 1.5f);
    fill(cv, Ellipse{{12.0f, -5.6f}, {0.8f, 0.6f}}, Solid{LINE});
    fill(cv, Ellipse{{4.6f, -3.4f}, {2.0f, 1.2f}}, Solid{BLUSH}, Blend::Atop, 1.2f);
    if (mouth == MOUTH_OPEN) {
      Ellipse m{{10.4f, -2.2f}, {2.4f, 1.9f}};
      fill(cv, grow(m, 0.5f), Solid{LINE});
      fill(cv, m, Radial{{10.4f, -2.0f}, 2.4f, hex(0xffd36a), hex(0xb8302a)});
    } else {
      Path mo;
      mo.move({5.0f, -2.4f}).quad({8.5f, -1.0f}, {12.0f, -2.6f});
      fill(cv, mo.stroke(0.7f, 0.5f), Solid{LINE});
    }
    return;
  }
  for (int s = -1; s <= 1; s += 2) {
    horn(cv, {s * 4.6f, -14.6f}, {s * 7.0f, -20.6f});
    fin(cv, {s * 8.4f, -12.2f}, {s * 13.6f, -14.6f}, {s * 10.2f, -7.6f});
  }
  spike(cv, {0, -16.6f}, {0, -1}, 1.8f);
  Ellipse h{{0, -8.2f}, {10.4f, 8.8f}};
  skin(cv, h, {-3, -11.5f}, 13);
  if (v == VIEW_BACK) return;
  Ellipse snout{{0, -3.6f}, {6.6f, 4.4f}};
  fill(cv, snout, Volume{lighten(BODY, 0.28f), lighten(BODY, 0.55f), BODY, {-1.5f, -5.5f}, 7}, Blend::Atop);
  for (int s = -1; s <= 1; s += 2) {
    fill(cv, Ellipse{{s * 1.9f, -4.6f}, {0.75f, 0.55f}}, Solid{LINE});
    fill(cv, Ellipse{{s * 6.9f, -4.6f}, {2.0f, 1.25f}}, Solid{BLUSH}, Blend::Atop, 1.2f);
  }
  if (mouth == MOUTH_OPEN) {
    Ellipse m{{0, -1.3f}, {2.7f, 2.1f}};
    fill(cv, grow(m, 0.5f), Solid{LINE});
    fill(cv, m, Radial{{0, -1.0f}, 2.8f, hex(0xffd36a), hex(0xb8302a)});
  } else {
    Path mo;
    mo.move({-3.2f, -1.6f}).quad({0, 0.2f}, {3.2f, -1.6f});
    fill(cv, mo.stroke(0.7f, 0.7f), Solid{LINE});
    Path fang;
    fang.move({1.0f, -0.9f}).line({1.7f, 0.5f}).line({2.2f, -1.1f});
    fill(cv, fang.poly(), Solid{HORN});
  }
}

enum EyeMood { EYES_OPEN, EYES_BLINK, EYES_SAD };

void dragon_eye(Canvas& cv, Vec2 c, Vec2 r, EyeMood mood, f32 side) {
  if (mood == EYES_BLINK) {
    Path p;
    p.move(c + Vec2{-r.x, -0.2f}).quad(c + Vec2{0, 1.4f}, c + Vec2{r.x, -0.2f});
    fill(cv, p.stroke(0.9f, 0.7f), Solid{EYE});
    return;
  }
  fill(cv, Ellipse{c, r + Vec2{0.25f, 0.25f}}, Solid{EYE});
  fill(cv, Ellipse{c + Vec2{0, r.y * 0.25f}, r * Vec2{0.78f, 0.7f}}, Linear{c + Vec2{0, -1}, c + Vec2{0, r.y}, EYE, IRIS});
  fill(cv, Circle{c + Vec2{-r.x * 0.32f, -r.y * 0.38f}, r.x * 0.42f}, Solid{{1, 1, 1, 1}});
  fill(cv, Circle{c + Vec2{r.x * 0.36f, r.y * 0.4f}, r.x * 0.2f}, Solid{{1, 1, 1, 0.85f}});
  if (mood == EYES_SAD) {
    // heavy upper lid and worried brow
    fill(cv, RBox{c + Vec2{0, -r.y * 0.85f}, {r.x * 1.3f, r.y * 0.5f}}, Solid{BODY}, Blend::Over);
    Path lid;
    lid.move(c + Vec2{-r.x - 0.2f, -r.y * 0.3f}).line(c + Vec2{r.x + 0.2f, -r.y * 0.3f});
    fill(cv, lid.stroke(0.85f), Solid{EYE});
    Path brow;
    brow.move(c + Vec2{-side * r.x * 1.1f, -r.y * 1.25f}).line(c + Vec2{side * r.x * 0.8f, -r.y * 0.85f});
    fill(cv, brow.stroke(0.8f, 0.5f), Solid{BODY_D});
  }
}

void eyes(Canvas& cv, View v, EyeMood mood) {
  if (v == VIEW_SIDE) {
    dragon_eye(cv, {4.4f, -9.0f}, {2.0f, 2.9f}, mood, 1);
    return;
  }
  for (int s = -1; s <= 1; s += 2) dragon_eye(cv, {s * 4.3f, -8.8f}, {2.5f, 3.1f}, mood, (f32)s);
}

// ---------------------------------------------------------------- wings, limbs, tail
void wing(Canvas& cv, Vec2 dir_up, f32 flip, f32 size) {
  // a small bat-like wing pointing out (+x) and up; the hex binds it with dark bands
  auto P = [&](f32 x, f32 y) { return Vec2{x * flip, y} * size; };
  Path m;
  m.move(P(0.5f, 0.5f)).line(P(6.5f, -6.5f)).line(P(10.8f, -9.0f)).quad(P(10.2f, -5.5f), P(8.6f, -3.6f)).quad(P(7.6f, -1.4f), P(5.2f, -1.6f)).quad(P(4.0f, 0.8f), P(1.6f, 1.6f)).line(P(0.5f, 0.5f));
  Poly mem = m.poly();
  fill(cv, grow(mem, LW), Solid{LINE});
  fill(cv, mem, Linear{P(1, 0), P(9, -7), WING_D, WING});
  Capsule b1{P(0, 0), P(6.5f, -6.5f), 1.3f * size, 1.0f * size};
  Capsule b2{P(6.5f, -6.5f), P(10.8f, -9.0f), 1.0f * size, 0.5f * size};
  fill(cv, grow(unite(b1, b2), LW * 0.7f), Solid{LINE});
  fill(cv, unite(b1, b2), Linear{P(0, 0), P(10, -9), BODY_D, BODY});
  for (int i = 0; i < 2; i++) {
    Path vein;
    vein.move(P(6.5f, -6.5f)).quad(P(6.8f - i * 1.8f, -4.0f + i), P(5.4f - i * 2.6f, -1.2f + i * 1.6f));
    fill(cv, vein.stroke(0.5f * size, 0.25f), Solid{WING_D.with_alpha(0.8f)}, Blend::Atop);
  }
  // the hex: a dark band across the wing
  RBox band{P(4.6f, -2.8f), Vec2{0.95f, 5.2f} * size, 0.5f, flip * 0.75f};
  fill(cv, band, Solid{HEX_DARK}, Blend::Atop);
  (void)dir_up;
}

void foot(Canvas& cv) {
  Ellipse f{{0, 0.6f}, {3.6f, 2.3f}};
  skin(cv, f, {-1, -0.5f}, 4);
  for (int i = -1; i <= 1; i++) fill(cv, Circle{{i * 1.6f, 2.3f - absf((f32)i) * 0.3f}, 0.7f}, Solid{HORN});
}

void arm_front(Canvas& cv) {
  Capsule a{{0, 0}, {0.4f, 4.6f}, 2.2f, 1.9f};
  Circle paw{{0.4f, 5.4f}, 1.9f};
  fill(cv, grow(unite(a, paw), LW), Solid{LINE});
  fill(cv, unite(a, paw), Volume{BODY, BODY_L, BODY_D, {-0.8f, 1}, 5});
  for (int i = -1; i <= 1; i++) fill(cv, Circle{{0.4f + i * 1.1f, 7.0f}, 0.5f}, Solid{HORN});
}

void leg_side_hind(Canvas& cv) {
  Ellipse thigh{{0, 1.5f}, {4.4f, 4.8f}};
  Ellipse ft{{1.8f, 6.6f}, {3.8f, 1.9f}};
  fill(cv, grow(unite(thigh, ft), LW), Solid{LINE});
  fill(cv, unite(thigh, ft), Volume{BODY, BODY_L, BODY_D, {-1.5f, -0.5f}, 7});
  for (int i = 0; i < 3; i++) fill(cv, Circle{{3.2f + i * 1.0f, 7.9f - i * 0.25f}, 0.6f}, Solid{HORN});
}

void leg_side_front(Canvas& cv) {
  Capsule a{{0, 0}, {0.8f, 6.4f}, 2.3f, 2.0f};
  Ellipse paw{{1.8f, 7.1f}, {2.8f, 1.7f}};
  fill(cv, grow(unite(a, paw), LW), Solid{LINE});
  fill(cv, unite(a, paw), Volume{BODY, BODY_L, BODY_D, {-0.8f, 1}, 6});
  for (int i = 0; i < 3; i++) fill(cv, Circle{{3.0f + i * 0.8f, 8.3f - i * 0.2f}, 0.5f}, Solid{HORN});
}

void tail_piece(Canvas& cv, int which) {
  f32 r0[3] = {3.5f, 2.9f, 2.1f}, r1[3] = {2.9f, 2.1f, 1.0f}, len[3] = {5.6f, 5.4f, 5.0f};
  Capsule c{{0, 0}, {0, len[which]}, r0[which], r1[which]};
  if (which == 2) {
    Path tip;  // a little spade at the end
    tip.move({0, 4.0f}).quad({-3.6f, 5.2f}, {-2.8f, 7.8f}).quad({-1.0f, 8.0f}, {0, 10.6f}).quad({1.0f, 8.0f}, {2.8f, 7.8f}).quad({3.6f, 5.2f}, {0, 4.0f});
    fill(cv, grow(tip.poly(), LW), Solid{LINE});
    fill(cv, tip.poly(), Linear{{0, 4}, {0, 10.6f}, darken(SPIKE, 0.15f), lighten(SPIKE, 0.2f)});
  }
  skin(cv, c, {-1.2f, 1}, 5);
  fill(cv, Ellipse{{0.9f, len[which] * 0.5f}, {r1[which] * 0.5f, len[which] * 0.45f}}, Solid{BELLY.with_alpha(0.75f)}, Blend::Atop, 0.8f);
}

void hex_band(Canvas& cv) {
  // white so it can be tinted; drawn additively with a pulse
  fill(cv, RBox{{0, 0}, {1.4f, 5.6f}, 1.0f}, Solid{{1, 1, 1, 0.55f}}, Blend::Over, 3.0f);
  fill(cv, RBox{{0, 0}, {0.75f, 5.0f}, 0.6f}, Solid{{1, 1, 1, 1}});
  for (int i = -1; i <= 1; i++) fill(cv, Circle{{0, i * 3.2f}, 1.15f}, Solid{{1, 1, 1, 1}});
}

}  // namespace

void bake_dragon(Atlas& a) {
  DragonArt& D = S.flint;
  for (int v = 0; v < 3; v++) {
    View vw = (View)v;
    D.body[v] = a.bake({-14.5f, -22.5f, 14.5f, 2.5f}, SC, [&](Canvas& cv) { body(cv, vw); });
    Rect hb = v == VIEW_SIDE ? Rect{-12.5f, -21.5f, 15.5f, 2.5f} : Rect{-16, -23.5f, 16, 2.5f};
    D.head[v] = a.bake(hb, SC, [&](Canvas& cv) { head(cv, vw, MOUTH_SMILE); });
    D.head_open[v] = v == VIEW_BACK ? D.head[v] : a.bake(hb, SC, [&](Canvas& cv) { head(cv, vw, MOUTH_OPEN); });
    if (v != VIEW_BACK) {
      Rect eb{-8.5f, -14.5f, 8.5f, -4.0f};
      D.eyes[v] = a.bake(eb, SC, [&](Canvas& cv) { eyes(cv, vw, EYES_OPEN); });
      D.blink[v] = a.bake(eb, SC, [&](Canvas& cv) { eyes(cv, vw, EYES_BLINK); });
      D.sad[v] = a.bake(eb, SC, [&](Canvas& cv) { eyes(cv, vw, EYES_SAD); });
    } else {
      D.eyes[v] = D.blink[v] = D.sad[v] = 0xffff;
    }
  }
  D.wing_front = a.bake({-2.5f, -12, 13, 3.5f}, SC, [](Canvas& cv) { wing(cv, {0, -1}, 1, 1.0f); });
  D.wing_side = a.bake({-2.5f, -13, 14.5f, 3.5f}, SC, [](Canvas& cv) { wing(cv, {0, -1}, 1, 1.1f); });
  D.wing_back = a.bake({-2.5f, -14, 16, 4}, SC, [](Canvas& cv) { wing(cv, {0, -1}, 1, 1.25f); });
  D.foot = a.bake({-5, -2.5f, 5, 4.5f}, SC, [](Canvas& cv) { foot(cv); });
  D.arm_front = a.bake({-3.5f, -3, 4.5f, 8.5f}, SC, [](Canvas& cv) { arm_front(cv); });
  D.leg_side_hind = a.bake({-6, -4.5f, 7.5f, 9.5f}, SC, [](Canvas& cv) { leg_side_hind(cv); });
  D.leg_side_front = a.bake({-3.5f, -3, 6.5f, 9.5f}, SC, [](Canvas& cv) { leg_side_front(cv); });
  for (int i = 0; i < 3; i++) D.tail_seg[i] = a.bake({-5, -4.5f, 5, 12}, SC, [&](Canvas& cv) { tail_piece(cv, i); });
  D.hex_band = a.bake({-5, -9.5f, 5, 9.5f}, SC, [](Canvas& cv) { hex_band(cv); });
}

}  // namespace sky
