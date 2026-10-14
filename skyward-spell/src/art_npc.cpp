// art_npc.cpp - the people (and creatures) met along the way, two in each land. Each is one
// painted sprite plus a "blink" overlay that closes their eyes; the game adds breathing and turning.
#include "art_util.h"
#include "npc.h"

namespace sky {

namespace {

constexpr f32 SC = 3.0f;
const Color INK = hex(0x2a1e1a);

struct Pal { Color base, light, dark; };
Pal pal(u32 b) { Color c = hex(b); return {c, lighten(c, 0.35f), darken(c, 0.4f)}; }

template <class Sh>
void part(Canvas& cv, const Sh& shape, Pal p, Vec2 c, f32 r, f32 lw = 0.9f, Color line = INK) {
  fill(cv, grow(shape, lw), Solid{line});
  fill(cv, shape, Volume{p.base, p.light, p.dark, c, r, 2.0f, 0.3f});
}

void eye(Canvas& cv, Vec2 c, f32 s, Color iris, bool closed, Color lid) {
  if (closed) {
    fill(cv, Ellipse{c, {s * 1.15f, s * 1.1f}}, Solid{lid});
    Path l;
    l.move(c + Vec2{-s, 0.1f * s}).quad(c + Vec2{0, s * 0.75f}, c + Vec2{s, 0.1f * s});
    fill(cv, l.stroke(0.55f), Solid{INK});
    return;
  }
  fill(cv, Ellipse{c, {s, s * 1.08f}}, Solid{INK});
  fill(cv, Ellipse{c + Vec2{0, 0.1f * s}, {s * 0.8f, s * 0.88f}}, Solid{iris});
  fill(cv, Circle{c + Vec2{0, 0.15f * s}, s * 0.45f}, Solid{INK});
  fill(cv, Circle{c + Vec2{-0.3f * s, -0.35f * s}, s * 0.3f}, Solid{{1, 1, 1, 0.95f}});
}

void eyes(Canvas& cv, Vec2 c, f32 gap, f32 s, Color iris, bool closed, Color lid) {
  eye(cv, c + Vec2{-gap, 0}, s, iris, closed, lid);
  eye(cv, c + Vec2{gap, 0}, s, iris, closed, lid);
}

void smile(Canvas& cv, Vec2 c, f32 w, f32 depth = 0.9f) {
  Path m;
  m.move(c + Vec2{-w, 0}).quad(c + Vec2{0, depth * 1.6f}, c + Vec2{w, 0});
  fill(cv, m.stroke(0.6f), Solid{INK});
}

void blush(Canvas& cv, Vec2 c, f32 gap) {
  for (int s = -1; s <= 1; s += 2) fill(cv, Ellipse{c + Vec2{s * gap, 0}, {1.7f, 1.0f}}, Solid{hex(0xff8a8a, 0.45f)}, Blend::Atop, 0.6f);
}

void shadow(Canvas& cv, f32 w) { fill(cv, Ellipse{{0, 0}, {w, w * 0.32f}}, Solid{hex(0x101810, 0.3f)}, Blend::Over, 2.5f); }

// ---------------------------------------------------------------- humans share a body plan
struct Human {
  Color skin, hair, coat, coat2, legs;
  f32 height = 1;  // 1 = adult, smaller for short folk
};

void human_body(Canvas& cv, const Human& h, f32 width = 1) {
  f32 k = h.height;
  for (int s = -1; s <= 1; s += 2) {  // legs and boots
    part(cv, Capsule{{s * 3.2f, -10 * k}, {s * 3.4f, -2}, 2.4f, 2.2f}, pal(0x5a4a3a), {s * 3.0f, -6}, 4);
    part(cv, Ellipse{{s * 3.6f, -1.2f}, {3.2f, 2.0f}}, pal(0x4a3426), {s * 3.4f, -2}, 3);
  }
  Path coat;
  coat.move({-6.5f * width, -24 * k}).cubic({-9.5f * width, -16 * k}, {-11 * width, -9 * k}, {-10.5f * width, -6 * k})
      .quad({0, -4.5f * k}, {10.5f * width, -6 * k}).cubic({11 * width, -9 * k}, {9.5f * width, -16 * k}, {6.5f * width, -24 * k}).quad({0, -26 * k}, {-6.5f * width, -24 * k});
  Pal cp{h.coat, lighten(h.coat, 0.3f), darken(h.coat, 0.42f)};
  part(cv, coat.poly(), cp, {-3, -18 * k}, 14);
  for (int s = -1; s <= 1; s += 2) {  // arms
    part(cv, Capsule{{s * 7.0f * width, -22 * k}, {s * 9.5f * width, -12 * k}, 2.6f, 2.3f}, cp, {s * 7.0f, -18 * k}, 5);
    part(cv, Circle{{s * 9.8f * width, -10.8f * k}, 2.2f}, Pal{h.skin, lighten(h.skin, 0.2f), darken(h.skin, 0.25f)}, {s * 9.5f, -11.5f * k}, 2.5f, 0.7f);
  }
}

Vec2 human_head(Canvas& cv, const Human& h, f32 r = 8.2f) {
  Vec2 c{0, -31.0f * h.height};
  part(cv, Ellipse{c, {r, r * 0.95f}}, Pal{h.skin, lighten(h.skin, 0.25f), darken(h.skin, 0.3f)}, c + Vec2{-2.5f, -3}, r * 1.2f);
  return c;
}

// ---------------------------------------------------------------- the twelve
void granny_willa(Canvas& cv, bool closed) {
  Human h{hex(0xf2c9a8), hex(0xd8d4d0), hex(0x5a8a4a), hex(0x9a7ac0), hex(0x4a3a30), 0.92f};
  Vec2 c{0, -31.0f * h.height};
  if (closed) { eyes(cv, c + Vec2{0, 0.5f}, 3.4f, 1.5f, hex(0x5a7a9a), true, h.skin); return; }
  shadow(cv, 12);
  human_body(cv, h, 1.05f);
  // a lavender shawl and a jar of honey
  Path shawl;
  shawl.move({-8, -24}).quad({0, -15}, {8, -24}).line({6.5f, -17}).quad({0, -11}, {-6.5f, -17});
  part(cv, shawl.poly(), pal(0x9a7ac0), {-2, -21}, 9);
  part(cv, RBox{{8.5f, -11}, {3.2f, 3.6f}, 1.2f}, pal(0xf0b030), {7.6f, -12.5f}, 4, 0.8f);
  fill(cv, RBox{{8.5f, -14.6f}, {3.4f, 0.9f}, 0.4f}, Solid{hex(0xd8c8a8)});
  part(cv, Ellipse{c + Vec2{0, -6.5f}, {6.5f, 4.2f}}, pal(0xd8d4d0), c + Vec2{-2, -8}, 6);  // hair
  human_head(cv, h);
  part(cv, Circle{c + Vec2{0, -11.5f}, 3.6f}, pal(0xe0dcd8), c + Vec2{-1, -12.5f}, 4);  // the bun
  fill(cv, Ellipse{c + Vec2{0, -5.5f}, {8.0f, 2.8f}}, Solid{hex(0xd8d4d0)}, Blend::Atop);
  eyes(cv, c + Vec2{0, 0.5f}, 3.4f, 1.5f, hex(0x5a7a9a), false, h.skin);
  for (int s = -1; s <= 1; s += 2) fill(cv, subtract(Circle{c + Vec2{s * 3.4f, 0.5f}, 2.7f}, Circle{c + Vec2{s * 3.4f, 0.5f}, 2.1f}), Solid{hex(0xb8902a)});  // spectacles
  fill(cv, Capsule{c + Vec2{-0.7f, 0.3f}, c + Vec2{0.7f, 0.3f}, 0.35f, 0.35f}, Solid{hex(0xb8902a)});
  blush(cv, c + Vec2{0, 3.0f}, 5.0f);
  smile(cv, c + Vec2{0, 4.2f}, 2.2f);
}

void tumble(Canvas& cv, bool closed) {
  Pal fur = pal(0xb8875a), belly = pal(0xf0dcc0);
  Vec2 c{0, -22};
  if (closed) { eyes(cv, c + Vec2{0, 0.5f}, 3.3f, 1.7f, hex(0x2a1a10), true, fur.base); return; }
  shadow(cv, 10);
  for (int s = -1; s <= 1; s += 2) part(cv, Ellipse{{s * 3.6f, -1.6f}, {3.4f, 2.2f}}, fur, {s * 3.0f, -2.4f}, 3);
  part(cv, Ellipse{{0, -9}, {7.0f, 7.8f}}, fur, {-2, -12}, 9);
  part(cv, Ellipse{{0, -8}, {4.4f, 5.4f}}, belly, {-1, -10}, 6, 0.0f);
  fill(cv, RBox{{4.6f, -9}, {2.6f, 3.0f}, 0.8f, -0.2f}, Solid{hex(0x7a5a30)});  // satchel
  // ears: one up, one flopped
  part(cv, Ellipse{c + Vec2{-3.4f, -12}, {2.3f, 7.0f}, -0.15f}, fur, c + Vec2{-4, -13}, 6);
  part(cv, Ellipse{c + Vec2{6.0f, -9}, {2.2f, 6.5f}, 1.0f}, fur, c + Vec2{5, -10}, 6);
  fill(cv, Ellipse{c + Vec2{-3.4f, -12}, {1.1f, 5.2f}, -0.15f}, Solid{hex(0xe8a8a0)});
  part(cv, Ellipse{c, {7.2f, 6.6f}}, fur, c + Vec2{-2, -2.5f}, 8);
  // red scout bandana
  Path band;
  band.move(c + Vec2{-6.8f, -2.5f}).quad(c + Vec2{0, -6.4f}, c + Vec2{6.8f, -2.5f}).line(c + Vec2{6.4f, -4.4f}).quad(c + Vec2{0, -8.4f}, c + Vec2{-6.4f, -4.4f});
  fill(cv, band.poly(), Solid{hex(0xd83a3a)});
  fill(cv, Ellipse{c + Vec2{7.6f, -4.0f}, {1.8f, 1.0f}, 0.6f}, Solid{hex(0xd83a3a)});
  eyes(cv, c + Vec2{0, 0.5f}, 3.3f, 1.7f, hex(0x2a1a10), false, fur.base);
  part(cv, Ellipse{c + Vec2{0, 3.6f}, {1.3f, 0.9f}}, pal(0xe87a8a), c + Vec2{0, 3.2f}, 1.5f, 0.4f);
  smile(cv, c + Vec2{0, 4.8f}, 1.4f, 0.6f);
  blush(cv, c + Vec2{0, 3.4f}, 4.6f);
}

void mossback(Canvas& cv, bool closed) {
  Pal skin = pal(0x7a9a4a), belly = pal(0xd8d0a0);
  Vec2 c{0, -14};
  if (closed) { eyes(cv, c + Vec2{0, -6}, 5.4f, 2.0f, hex(0xe8b030), true, skin.base); return; }
  shadow(cv, 16);
  part(cv, Ellipse{{0, -8}, {14, 9}}, skin, {-4, -12}, 16);
  part(cv, Ellipse{{0, -6}, {8.5f, 5.5f}}, belly, {-2, -8}, 9, 0);
  for (int s = -1; s <= 1; s += 2) part(cv, Ellipse{{s * 11.0f, -1.5f}, {4.6f, 2.4f}}, skin, {s * 10.0f, -2.6f}, 5);
  Rng r(31);
  for (int i = 0; i < 10; i++) fill(cv, Circle{{r.range(-12, 12), r.range(-15, -9)}, r.range(0.6f, 1.2f)}, Solid{darken(skin.base, 0.25f)}, Blend::Atop);
  for (int s = -1; s <= 1; s += 2) part(cv, Circle{c + Vec2{s * 5.4f, -6}, 3.6f}, skin, c + Vec2{s * 5.4f - 1, -7}, 4);
  // half-shut, sleepy eyes
  eyes(cv, c + Vec2{0, -6}, 5.4f, 2.0f, hex(0xe8b030), false, skin.base);
  for (int s = -1; s <= 1; s += 2) fill(cv, Ellipse{c + Vec2{s * 5.4f, -7.4f}, {2.6f, 1.4f}}, Solid{skin.base});
  Path m;
  m.move(c + Vec2{-8, 0.5f}).quad(c + Vec2{0, 3.0f}, c + Vec2{8, 0.5f});
  fill(cv, m.stroke(0.7f), Solid{INK});
  // a little straw hat with a reed stuck in it
  part(cv, Ellipse{c + Vec2{1, -10.5f}, {7.5f, 1.8f}}, pal(0xe0c070), c + Vec2{-1, -11}, 7, 0.7f);
  part(cv, Ellipse{c + Vec2{1, -12.2f}, {4.0f, 2.4f}}, pal(0xe0c070), c + Vec2{0, -13}, 4, 0.7f);
  Path reed;
  reed.move(c + Vec2{4, -12}).line(c + Vec2{9, -19});
  fill(cv, reed.stroke(0.6f), Solid{hex(0x5a7a3a)});
}

void wren(Canvas& cv, bool closed) {
  Human h{hex(0x8a5a3a), hex(0x2a1a14), hex(0x2a7a7a), hex(0x1a5a5a), hex(0x3a3a3a), 1.05f};
  Vec2 c{0, -31.0f * h.height};
  if (closed) { eyes(cv, c + Vec2{0, 0.8f}, 3.2f, 1.5f, hex(0x3a2a1a), true, h.skin); return; }
  shadow(cv, 12);
  // the lantern pole behind her
  Path pole;
  pole.move({-11, 0}).line({-11, -46}).quad({-11, -50}, {-7, -50});
  fill(cv, pole.stroke(1.6f), Solid{INK});
  fill(cv, pole.stroke(1.0f), Solid{hex(0x7a5a3a)});
  part(cv, RBox{{-6.5f, -45}, {2.4f, 3.2f}, 0.8f}, pal(0x3a3a3a), {-7, -46}, 3, 0.6f);
  fill(cv, RBox{{-6.5f, -45}, {1.5f, 2.2f}, 0.6f}, Solid{hex(0xffe08a)});
  human_body(cv, h, 0.95f);
  // hooded cloak and braids
  Path hood;
  hood.move(c + Vec2{-9.5f, 6}).cubic(c + Vec2{-11, -6}, c + Vec2{-6, -12}, c + Vec2{0, -12}).cubic(c + Vec2{6, -12}, c + Vec2{11, -6}, c + Vec2{9.5f, 6}).line(c + Vec2{-9.5f, 6});
  part(cv, hood.poly(), Pal{h.coat, lighten(h.coat, 0.3f), darken(h.coat, 0.45f)}, c + Vec2{-3, -4}, 12);
  for (int s = -1; s <= 1; s += 2) part(cv, Capsule{c + Vec2{s * 6.5f, 2}, c + Vec2{s * 7.5f, 11}, 1.7f, 1.3f}, pal(0x2a1a14), c + Vec2{s * 6.0f, 4}, 3, 0.6f);
  human_head(cv, h, 7.4f);
  fill(cv, Ellipse{c + Vec2{0, -5.2f}, {7.0f, 2.8f}}, Solid{h.hair}, Blend::Atop);
  eyes(cv, c + Vec2{0, 0.8f}, 3.2f, 1.5f, hex(0x3a2a1a), false, h.skin);
  smile(cv, c + Vec2{0, 4.4f}, 1.8f, 0.6f);
}

void cobble(Canvas& cv, bool closed) {
  Human h{hex(0xe8b090), hex(0xd8662a), hex(0x7a5a3a), hex(0x5a3a26), hex(0x4a3a30), 0.8f};
  Vec2 c{0, -31.0f * h.height};
  if (closed) { eyes(cv, c + Vec2{0, -0.5f}, 3.0f, 1.3f, hex(0x3a6a8a), true, h.skin); return; }
  shadow(cv, 13);
  // a pickaxe over the shoulder
  Path handle;
  handle.move({-12, -8}).line({8, -32});
  fill(cv, handle.stroke(2.2f), Solid{INK});
  fill(cv, handle.stroke(1.4f), Solid{hex(0x9a6a3a)});
  Path head;
  head.move({3, -36}).quad({8, -34}, {13, -27}).line({11.5f, -26.5f}).quad({8, -31}, {4, -33});
  part(cv, head.poly(), pal(0x9aa0aa), {6, -32}, 6, 0.7f);
  human_body(cv, h, 1.3f);
  part(cv, RBox{{0, -11}, {6.5f, 5}, 1.5f}, pal(0x8a5a30), {-2, -13}, 7, 0.6f);  // leather apron
  human_head(cv, h, 7.6f);
  // the big beard
  Path beard;
  beard.move(c + Vec2{-7.5f, 0}).cubic(c + Vec2{-9, 9}, c + Vec2{-4, 13}, c + Vec2{0, 13.5f}).cubic(c + Vec2{4, 13}, c + Vec2{9, 9}, c + Vec2{7.5f, 0}).quad(c + Vec2{0, 4.5f}, c + Vec2{-7.5f, 0});
  part(cv, beard.poly(), pal(0xd8662a), c + Vec2{-2, 4}, 10);
  // helmet with a lamp
  Path helm;
  helm.move(c + Vec2{-8.6f, -2.5f}).cubic(c + Vec2{-9, -12}, c + Vec2{9, -12}, c + Vec2{8.6f, -2.5f}).line(c + Vec2{-8.6f, -2.5f});
  part(cv, helm.poly(), pal(0xc89a3a), c + Vec2{-3, -7}, 9);
  fill(cv, RBox{c + Vec2{0, -2.6f}, {10, 1.1f}, 0.5f}, Solid{hex(0x8a6420)});
  part(cv, Circle{c + Vec2{0, -8}, 2.4f}, pal(0x5a5a5a), c + Vec2{-0.6f, -8.6f}, 2.4f, 0.6f);
  fill(cv, Circle{c + Vec2{0, -8}, 1.6f}, Solid{hex(0xfff0a0)});
  eyes(cv, c + Vec2{0, -0.5f}, 3.0f, 1.3f, hex(0x3a6a8a), false, h.skin);
  part(cv, Ellipse{c + Vec2{0, 2.3f}, {2.1f, 1.7f}}, pal(0xe89070), c + Vec2{-0.6f, 1.8f}, 2, 0.5f);  // nose
}

void ember(Canvas& cv, bool closed) {
  Pal skin = pal(0xe8642a), belly = pal(0xffc060);
  Vec2 c{0, -27};
  if (closed) { eyes(cv, c + Vec2{0, 0}, 3.2f, 1.8f, hex(0x2a1a10), true, skin.base); return; }
  shadow(cv, 10);
  Path tail;
  tail.move({3, -6}).cubic({12, -4}, {16, -10}, {13, -16});
  fill(cv, tail.stroke(4.6f, 1.2f), Solid{INK});
  fill(cv, tail.stroke(3.4f, 0.6f), Solid{skin.base});
  for (int s = -1; s <= 1; s += 2) part(cv, Ellipse{{s * 3.6f, -1.6f}, {3.2f, 2.0f}}, skin, {s * 3.0f, -2.4f}, 3);
  part(cv, Ellipse{{0, -11}, {6.4f, 9}}, skin, {-2, -14}, 10);
  part(cv, Ellipse{{0, -10}, {3.8f, 6.5f}}, belly, {-1, -12}, 7, 0);
  fill(cv, RBox{{0, -7}, {6.8f, 1.2f}, 0.5f}, Solid{hex(0x5a3a26)});  // belt with vials
  for (int i = -1; i <= 1; i++) part(cv, RBox{{i * 3.4f, -5.2f}, {0.9f, 1.6f}, 0.5f}, pal(i == 0 ? 0x5ad0ff : 0x7af07a), {i * 3.4f, -5.6f}, 1.5f, 0.4f);
  for (int s = -1; s <= 1; s += 2) part(cv, Capsule{{s * 5.6f, -15}, {s * 7.6f, -8}, 1.8f, 1.5f}, skin, {s * 6.0f, -12}, 3);
  part(cv, Ellipse{c, {7.4f, 6.4f}}, skin, c + Vec2{-2, -2}, 8);
  for (int i = 0; i < 4; i++) fill(cv, Circle{c + Vec2{-4.0f + i * 2.6f, -4.6f + (i % 2) * 0.8f}, 0.8f}, Solid{hex(0xffd060)}, Blend::Atop);  // spots
  // goggles pushed up on the head
  for (int s = -1; s <= 1; s += 2) part(cv, Circle{c + Vec2{s * 3.0f, -5.4f}, 2.2f}, pal(0xb8902a), c + Vec2{s * 3.0f - 0.5f, -6}, 2.5f, 0.6f);
  for (int s = -1; s <= 1; s += 2) fill(cv, Circle{c + Vec2{s * 3.0f, -5.4f}, 1.3f}, Solid{hex(0x8ad0e8)});
  eyes(cv, c, 3.2f, 1.8f, hex(0x2a1a10), false, skin.base);
  smile(cv, c + Vec2{0, 3.0f}, 3.0f, 1.0f);
}

void sage_rowan(Canvas& cv, bool closed) {
  Human h{hex(0xd8a888), hex(0xf0f0ea), hex(0x4a6a3a), hex(0x3a5a2a), hex(0x4a3a30), 1.05f};
  Vec2 c{0, -31.0f * h.height};
  if (closed) { eyes(cv, c + Vec2{0, -0.6f}, 3.2f, 1.3f, hex(0x4a7a4a), true, h.skin); return; }
  shadow(cv, 12);
  Path staff;
  staff.move({11, 0}).quad({12, -24}, {10, -46});
  fill(cv, staff.stroke(2.6f, 2.0f), Solid{INK});
  fill(cv, staff.stroke(1.8f, 1.2f), Solid{hex(0x7a5a3a)});
  fill(cv, wobble(Circle{{10, -45}, 3.0f}, 0.8f, 0.8f, 2), Solid{hex(0x6aa84a)});
  human_body(cv, h, 1.0f);
  // a cloak of leaves
  Rng r(77);
  for (int i = 0; i < 18; i++) {
    Vec2 p{r.range(-8, 8), r.range(-25, -8)};
    fill(cv, Ellipse{p, {2.2f, 1.2f}, r.range(0, PI)}, Solid{r.chance(0.5f) ? hex(0x6aa84a) : hex(0x4a8a3a)}, Blend::Atop);
  }
  human_head(cv, h, 7.6f);
  Path beard;
  beard.move(c + Vec2{-6.5f, 1}).cubic(c + Vec2{-7, 10}, c + Vec2{-2, 17}, c + Vec2{0.5f, 19}).cubic(c + Vec2{2, 15}, c + Vec2{7, 9}, c + Vec2{6.5f, 1}).quad(c + Vec2{0, 4.5f}, c + Vec2{-6.5f, 1});
  part(cv, beard.poly(), pal(0xf0f0ea), c + Vec2{-2, 6}, 12);
  fill(cv, Ellipse{c + Vec2{0, -5.4f}, {7.4f, 3.2f}}, Solid{hex(0xf0f0ea)}, Blend::Atop);
  // a crown of twigs
  for (int s = -1; s <= 1; s += 2) {
    Path tw;
    tw.move(c + Vec2{s * 3.0f, -7}).line(c + Vec2{s * 5.5f, -13}).line(c + Vec2{s * 8.0f, -14.5f});
    fill(cv, tw.stroke(1.2f, 0.6f), Solid{hex(0x6a4a2a)});
    fill(cv, Ellipse{c + Vec2{s * 8.0f, -14.5f}, {1.4f, 0.8f}, s * 0.5f}, Solid{hex(0x7ac05a)});
  }
  eyes(cv, c + Vec2{0, -0.6f}, 3.2f, 1.3f, hex(0x4a7a4a), false, h.skin);
  for (int s = -1; s <= 1; s += 2) fill(cv, Capsule{c + Vec2{s * 1.5f, -3.4f}, c + Vec2{s * 5.0f, -3.0f}, 0.8f, 0.6f}, Solid{hex(0xf0f0ea)});
}

void hoot(Canvas& cv, bool closed) {
  Pal feathers = pal(0x8a6a4a), chest = pal(0xe8d4b0);
  Vec2 c{0, -20};
  if (closed) { eyes(cv, c + Vec2{0, -1}, 4.2f, 2.6f, hex(0xffb020), true, chest.base); return; }
  shadow(cv, 11);
  for (int s = -1; s <= 1; s += 2) part(cv, Ellipse{{s * 3.0f, -1.0f}, {2.4f, 1.4f}}, pal(0xe0a030), {s * 3.0f, -1.6f}, 2, 0.6f);
  part(cv, Ellipse{{0, -14}, {10.5f, 13.5f}}, feathers, {-3, -18}, 15);
  part(cv, Ellipse{{0, -10}, {6.5f, 8.0f}}, chest, {-2, -12}, 9, 0);
  for (int i = 0; i < 6; i++) {
    Path v;
    Vec2 p{-3.0f + (i % 3) * 3.0f, -13.0f + (i / 3) * 3.5f};
    v.move(p + Vec2{-1, 0}).line(p + Vec2{0, 1}).line(p + Vec2{1, 0});
    fill(cv, v.stroke(0.5f), Solid{darken(chest.base, 0.3f)}, Blend::Atop);
  }
  // a book under one wing
  part(cv, RBox{{-8.5f, -10}, {2.4f, 4.4f}, 0.6f, 0.25f}, pal(0x9a3a3a), {-9, -11}, 4, 0.6f);
  for (int s = -1; s <= 1; s += 2) part(cv, Ellipse{{s * 9.5f, -13}, {3.0f, 7.5f}, s * 0.2f}, feathers, {s * 9.0f, -15}, 7);
  // ear tufts
  for (int s = -1; s <= 1; s += 2) {
    Path t;
    t.move(c + Vec2{s * 4.0f, -7}).line(c + Vec2{s * 8.5f, -13}).line(c + Vec2{s * 8.0f, -5});
    part(cv, t.poly(), feathers, c + Vec2{s * 6.0f, -8}, 4, 0.7f);
  }
  part(cv, Ellipse{c + Vec2{0, -1}, {9.5f, 7.0f}}, chest, c + Vec2{-2, -3}, 10, 0);
  eyes(cv, c + Vec2{0, -1}, 4.2f, 2.6f, hex(0xffb020), false, chest.base);
  // big round spectacles
  for (int s = -1; s <= 1; s += 2) fill(cv, subtract(Circle{c + Vec2{s * 4.2f, -1}, 3.9f}, Circle{c + Vec2{s * 4.2f, -1}, 3.2f}), Solid{hex(0x3a2a1a)});
  Path beak;
  beak.move(c + Vec2{-1.3f, 2}).line(c + Vec2{1.3f, 2}).line(c + Vec2{0, 4.8f});
  part(cv, beak.poly(), pal(0xe0a030), c + Vec2{0, 2.5f}, 2, 0.5f);
}

void captain_gale(Canvas& cv, bool closed) {
  Human h{hex(0xf0c8a0), hex(0x8a4a2a), hex(0x2a3a6a), hex(0x1a2a4a), hex(0x3a3a3a), 1.02f};
  Vec2 c{0, -31.0f * h.height};
  if (closed) { eyes(cv, c + Vec2{0, 0.8f}, 3.2f, 1.5f, hex(0x3a7a5a), true, h.skin); return; }
  shadow(cv, 12);
  // a long scarf streaming in the wind
  Path sc;
  sc.move({-2, -25}).cubic({-10, -26}, {-16, -22}, {-20, -24}).line({-19.5f, -21}).cubic({-15, -19}, {-10, -22}, {-2, -22});
  part(cv, sc.poly(), pal(0xe0b040), {-10, -24}, 8, 0.7f);
  human_body(cv, h, 1.0f);
  for (int i = 0; i < 3; i++) fill(cv, Circle{{1.8f, -21.0f + i * 4.0f}, 0.9f}, Solid{hex(0xe8c050)});  // brass buttons
  part(cv, Capsule{{9.5f, -12}, {14, -8}, 1.3f, 1.6f}, pal(0xb8902a), {11, -11}, 3, 0.6f);  // spyglass
  human_head(cv, h, 7.8f);
  // aviator cap and goggles
  Path cap;
  cap.move(c + Vec2{-8.6f, 1}).cubic(c + Vec2{-9.5f, -10}, c + Vec2{9.5f, -10}, c + Vec2{8.6f, 1}).line(c + Vec2{6.5f, 1}).quad(c + Vec2{0, -4}, c + Vec2{-6.5f, 1});
  part(cv, cap.poly(), pal(0x7a4a2a), c + Vec2{-3, -5}, 9);
  for (int s = -1; s <= 1; s += 2) {
    part(cv, Circle{c + Vec2{s * 3.4f, -5.4f}, 2.6f}, pal(0xb8902a), c + Vec2{s * 3.4f - 0.6f, -6}, 3, 0.6f);
    fill(cv, Circle{c + Vec2{s * 3.4f, -5.4f}, 1.7f}, Solid{hex(0x9ad8f0)});
  }
  eyes(cv, c + Vec2{0, 0.8f}, 3.2f, 1.5f, hex(0x3a7a5a), false, h.skin);
  smile(cv, c + Vec2{0.4f, 4.4f}, 2.0f, 0.7f);
}

void lumi(Canvas& cv, bool closed) {
  Vec2 c{0, -20};
  if (closed) { eyes(cv, c + Vec2{0, 0}, 3.2f, 1.6f, hex(0x5a8ad8), true, hex(0xffffff)); return; }
  shadow(cv, 8);
  fill(cv, Circle{{0, -18}, 15}, Solid{hex(0xbfe4ff, 0.35f)}, Blend::Over, 8);
  Blob b;
  b.k = 2.5f;
  b.add({0, -20}, 8).add({-7, -17}, 5.5f).add({7, -17}, 5.5f).add({-4, -26}, 5).add({5, -26}, 5.5f).add({0, -13}, 6);
  fill(cv, grow(b, 0.9f), Solid{hex(0x6a8ab8)});
  fill(cv, b, Volume{hex(0xf4faff), hex(0xffffff), hex(0xb8d0ec), {-3, -24}, 12, 2.5f, 0.3f});
  // a tiny rainbow tail
  const u32 cols[4] = {0xff8a8a, 0xffd06a, 0x8ae08a, 0x8ab8ff};
  for (int i = 0; i < 4; i++) {
    Path arc;
    arc.move({4.0f + i * 1.3f, -11}).quad({8.0f + i * 1.3f, -4}, {3.0f + i * 1.2f, 0});
    fill(cv, arc.stroke(1.2f), Solid{hex(cols[i])});
  }
  eyes(cv, c, 3.2f, 1.6f, hex(0x5a8ad8), false, hex(0xffffff));
  blush(cv, c + Vec2{0, 2.6f}, 4.6f);
  smile(cv, c + Vec2{0, 3.4f}, 1.5f, 0.8f);
}

void grizelda(Canvas& cv, bool closed) {
  Pal skin = pal(0x86ad4c);
  Vec2 c{0, -27};
  if (closed) { eyes(cv, c + Vec2{0, -0.5f}, 3.6f, 1.7f, hex(0xffd23a), true, skin.base); return; }
  shadow(cv, 11);
  // her broom
  Path br;
  br.move({-10, -2}).line({-6, -36});
  fill(cv, br.stroke(1.8f), Solid{INK});
  fill(cv, br.stroke(1.1f), Solid{hex(0x9a6a3a)});
  Path bristles;
  bristles.move({-12.5f, 1}).line({-7.5f, 1}).line({-8.5f, -6}).line({-11.0f, -6});
  part(cv, bristles.poly(), pal(0xd8b060), {-10, -3}, 4, 0.6f);
  for (int s = -1; s <= 1; s += 2) part(cv, Ellipse{{s * 3.4f, -1.4f}, {3.2f, 2.0f}}, pal(0x5a3a24), {s * 3.0f, -2.2f}, 3);
  part(cv, Ellipse{{0, -10}, {7.6f, 8.5f}}, pal(0x7a5236), {-2, -13}, 10);
  part(cv, RBox{{0, -8.5f}, {5.0f, 5.5f}, 1.4f}, pal(0xd8c8a0), {-1, -10}, 6, 0.6f);  // a patched apron
  fill(cv, RBox{{2.0f, -7.0f}, {1.6f, 1.6f}, 0.3f}, Solid{hex(0x9a7ac0)}, Blend::Atop);
  for (int s = -1; s <= 1; s += 2) {  // ears, one bandaged
    Path e;
    e.move(c + Vec2{s * 6.5f, -2}).quad(c + Vec2{s * 13.0f, -5}, c + Vec2{s * 17.0f, -7}).quad(c + Vec2{s * 13.0f, 0}, c + Vec2{s * 7.0f, 2.5f});
    part(cv, e.poly(), skin, c + Vec2{s * 11.0f, -2}, 7, 0.8f);
  }
  fill(cv, RBox{c + Vec2{12.5f, -3.5f}, {1.4f, 2.4f}, 0.4f, -0.5f}, Solid{hex(0xf0e8d8)});
  part(cv, Ellipse{c, {8.2f, 7.6f}}, skin, c + Vec2{-2.5f, -3}, 10);
  // a headscarf instead of a helmet: she's left Malgrin's service
  Path scarf;
  scarf.move(c + Vec2{-8, -1}).cubic(c + Vec2{-8.5f, -9}, c + Vec2{8.5f, -9}, c + Vec2{8, -1}).quad(c + Vec2{0, -4}, c + Vec2{-8, -1});
  part(cv, scarf.poly(), pal(0x5a9ad0), c + Vec2{-3, -5}, 8);
  for (int i = 0; i < 4; i++) fill(cv, Circle{c + Vec2{-4.5f + i * 3.0f, -4.8f}, 0.7f}, Solid{hex(0xf0f0ff)}, Blend::Atop);
  eyes(cv, c + Vec2{0, -0.5f}, 3.6f, 1.7f, hex(0xffd23a), false, skin.base);
  part(cv, Ellipse{c + Vec2{0, 2.4f}, {1.8f, 2.2f}}, pal(0x98c05a), c + Vec2{-0.6f, 1.8f}, 2, 0.5f);
  Path m;  // a nervous, wobbly smile
  m.move(c + Vec2{-3.4f, 5.2f}).quad(c + Vec2{-1.5f, 6.2f}, c + Vec2{0, 5.4f}).quad(c + Vec2{1.6f, 6.4f}, c + Vec2{3.4f, 5.0f});
  fill(cv, m.stroke(0.6f), Solid{INK});
}

void nib(Canvas& cv, bool closed) {
  Pal scale = pal(0x6a7ad8), belly = pal(0xd8e0ff);
  Vec2 c{0, -17};
  if (closed) { eyes(cv, c + Vec2{0, 0.5f}, 3.0f, 1.9f, hex(0x2a1a3a), true, scale.base); return; }
  shadow(cv, 9);
  Path tail;
  tail.move({2, -4}).cubic({9, -3}, {12, -7}, {10, -11});
  fill(cv, tail.stroke(3.6f, 1.0f), Solid{INK});
  fill(cv, tail.stroke(2.6f, 0.5f), Solid{scale.base});
  for (int s = -1; s <= 1; s += 2) {  // tiny hexed wings
    Path w;
    w.move({s * 4.0f, -11}).quad({s * 11.0f, -16}, {s * 12.0f, -10}).quad({s * 8.0f, -9}, {s * 4.0f, -8});
    part(cv, w.poly(), pal(0x9a8ae8), {s * 8.0f, -12}, 5, 0.6f);
    fill(cv, Capsule{{s * 6.0f, -10.5f}, {s * 10.0f, -12.0f}, 0.6f, 0.6f}, Solid{hex(0xc07aff)}, Blend::Atop);
  }
  for (int s = -1; s <= 1; s += 2) part(cv, Ellipse{{s * 3.2f, -1.4f}, {2.6f, 1.8f}}, scale, {s * 3.0f, -2}, 2.5f);
  part(cv, Ellipse{{0, -7}, {5.6f, 6.2f}}, scale, {-2, -9}, 7);
  part(cv, Ellipse{{0, -6}, {3.4f, 4.4f}}, belly, {-1, -7.5f}, 5, 0);
  part(cv, Ellipse{c, {7.0f, 6.0f}}, scale, c + Vec2{-2, -2}, 8);
  // a bit of eggshell still on its head
  Path shell;
  shell.move(c + Vec2{-5, -4.6f}).line(c + Vec2{-3.6f, -8}).line(c + Vec2{-1.5f, -6}).line(c + Vec2{0.6f, -9}).line(c + Vec2{2.6f, -6.2f}).line(c + Vec2{4.5f, -8.2f}).line(c + Vec2{5.2f, -4.6f}).quad(c + Vec2{0, -3.4f}, c + Vec2{-5, -4.6f});
  part(cv, shell.poly(), pal(0xf4f0e4), c + Vec2{-1, -6}, 6, 0.6f);
  eyes(cv, c + Vec2{0, 0.5f}, 3.0f, 1.9f, hex(0x2a1a3a), false, scale.base);
  Path m;  // a small, sad mouth
  m.move(c + Vec2{-1.6f, 4.6f}).quad(c + Vec2{0, 3.6f}, c + Vec2{1.6f, 4.6f});
  fill(cv, m.stroke(0.55f), Solid{INK});
}

using Painter = void (*)(Canvas&, bool);
const Painter PAINTERS[NPC_COUNT] = {granny_willa, tumble, mossback, wren, cobble, ember, sage_rowan, hoot, captain_gale, lumi, grizelda, nib};

}  // namespace

void bake_npcs(Atlas& a, Region r) {
  for (int i = 0; i < NPC_COUNT; i++) {
    S.npc[i] = {0, 0};
    if (NPCS[i].land != r) continue;
    Painter p = PAINTERS[i];
    S.npc[i].body = a.bake({-22, -52, 22, 4}, SC, [&](Canvas& cv) { p(cv, false); });
    S.npc[i].blink = a.bake({-22, -52, 22, 4}, SC, [&](Canvas& cv) { p(cv, true); });
  }
}

}  // namespace sky
