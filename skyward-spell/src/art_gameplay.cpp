// art_gameplay.cpp - pieces every land uses: Malgrin's goblins, hearts, waystones, the shrine
// and the five ingredients of the Skyward Spell, hazards, and goblin camps.
#include "art_util.h"

namespace sky {

void bake_butterflies(Atlas& a);

namespace {

constexpr f32 SC = 3.0f;
constexpr f32 LW = 0.9f;

const Color G_SKIN = hex(0x86ad4c), G_SKIN_L = hex(0xbfe07e), G_SKIN_D = hex(0x4c7330);
const Color G_LINE = hex(0x22261a);
const Color G_TUNIC = hex(0x7a5236), G_TUNIC_D = hex(0x4e321f);
const Color G_METAL = hex(0x8d929c), G_METAL_L = hex(0xd5d9df), G_METAL_D = hex(0x4d525c);
const Color G_EYE = hex(0xffd23a), HEX_PURPLE = hex(0x8a3fd0);

template <class Sh>
void part(Canvas& cv, const Sh& shape, Color base, Color light, Color dark, Vec2 c, f32 r, Color line = G_LINE, f32 lw = LW) {
  fill(cv, grow(shape, lw), Solid{line});
  fill(cv, shape, Volume{base, light, dark, c, r, 2.0f, 0.3f});
}

// ---------------------------------------------------------------- goblins
void goblin_ears(Canvas& cv, int side_only) {
  for (int s = -1; s <= 1; s += 2) {
    if (side_only && s != side_only) continue;
    Path e;
    e.move({s * 7.0f, -25.5f}).quad({s * 15.0f, -30.0f}, {s * 20.0f, -32.5f}).quad({s * 15.5f, -24.0f}, {s * 7.5f, -19.5f}).line({s * 7.0f, -25.5f});
    part(cv, e.poly(), G_SKIN, G_SKIN_L, G_SKIN_D, {s * 12.0f, -27}, 9);
    Path in;
    in.move({s * 9.0f, -24.5f}).quad({s * 14.0f, -27.5f}, {s * 17.0f, -30.0f}).quad({s * 13.5f, -24.5f}, {s * 9.0f, -21.5f});
    fill(cv, in.poly(), Solid{hex(0xd88a8a, 0.55f)}, Blend::Atop);
  }
}

void goblin_helmet(Canvas& cv, f32 dx) {
  Path h;
  h.move({dx - 9.2f, -26.0f}).cubic({dx - 9.4f, -35.5f}, {dx + 9.4f, -35.5f}, {dx + 9.2f, -26.0f}).quad({dx, -27.4f}, {dx - 9.2f, -26.0f});
  part(cv, h.poly(), G_METAL, G_METAL_L, G_METAL_D, {dx - 3, -32}, 10);
  fill(cv, RBox{{dx, -26.4f}, {10.0f, 1.2f}, 0.8f}, Solid{G_METAL_D});
  fill(cv, Circle{{dx + 3.5f, -31.0f}, 1.4f}, Solid{G_METAL_D.with_alpha(0.5f)}, Blend::Atop);  // a dent
  // Malgrin's mark: a purple eye
  fill(cv, Ellipse{{dx - 1.5f, -31.0f}, {2.2f, 1.3f}}, Solid{HEX_PURPLE}, Blend::Atop);
  fill(cv, Circle{{dx - 1.5f, -31.0f}, 0.6f}, Solid{hex(0xf4e0ff)}, Blend::Atop);
}

void goblin_body(Canvas& cv, View v) {
  // tunic body
  Ellipse torso{{0, -9.5f}, {8.2f, 8.0f}};
  if (v == VIEW_SIDE) torso = Ellipse{{0.5f, -9.5f}, {7.0f, 8.0f}};
  part(cv, torso, G_TUNIC, lighten(G_TUNIC, 0.3f), G_TUNIC_D, {-3, -13}, 11);
  fill(cv, RBox{{0, -5.2f}, {10, 1.1f}}, Solid{hex(0x3a2416)}, Blend::Atop);
  fill(cv, RBox{{v == VIEW_SIDE ? 4.0f : 0.0f, -5.2f}, {1.4f, 1.3f}, 0.3f}, Solid{hex(0xb89a4a)}, Blend::Atop);
  for (int i = 0; i < 4; i++) {
    Path st;
    st.move({-6.0f + i * 4.0f, -2.5f}).line({-5.5f + i * 4.0f, -0.6f});
    fill(cv, st.stroke(0.6f), Solid{G_TUNIC_D}, Blend::Atop);  // ragged hem
  }
  if (v == VIEW_BACK) {
    goblin_ears(cv, 0);
    Ellipse head{{0, -22.5f}, {9.2f, 8.4f}};
    part(cv, head, G_SKIN, G_SKIN_L, G_SKIN_D, {-3, -26}, 11);
    goblin_helmet(cv, 0);
    return;
  }
  if (v == VIEW_SIDE) {
    goblin_ears(cv, -1);
    auto head = unite(Ellipse{{0.5f, -22.5f}, {8.6f, 8.2f}}, Ellipse{{8.6f, -21.0f}, {3.6f, 2.4f}, 0.3f});
    part(cv, head, G_SKIN, G_SKIN_L, G_SKIN_D, {-2, -26}, 11);
    fill(cv, Ellipse{{4.6f, -24.0f}, {1.9f, 1.6f}}, Solid{G_EYE});
    fill(cv, Ellipse{{5.2f, -24.0f}, {0.7f, 1.3f}}, Solid{hex(0x1a1208)});
    Path brow;
    brow.move({2.2f, -26.8f}).line({6.6f, -25.6f});
    fill(cv, brow.stroke(0.9f), Solid{G_SKIN_D});
    Path m;
    m.move({3.5f, -18.6f}).quad({6.5f, -17.0f}, {9.0f, -18.6f});
    fill(cv, m.stroke(0.8f), Solid{G_LINE});
    Path tooth;
    tooth.move({6.0f, -18.0f}).line({6.6f, -16.6f}).line({7.2f, -17.9f});
    fill(cv, tooth.poly(), Solid{hex(0xfffbe8)});
    goblin_helmet(cv, -0.5f);
    return;
  }
  goblin_ears(cv, 0);
  Ellipse head{{0, -22.5f}, {9.2f, 8.4f}};
  part(cv, head, G_SKIN, G_SKIN_L, G_SKIN_D, {-3, -26}, 11);
  // a long nose, wicked yellow eyes, a toothy grin
  Ellipse nose{{0, -20.8f}, {2.1f, 2.8f}};
  part(cv, nose, lighten(G_SKIN, 0.1f), G_SKIN_L, G_SKIN_D, {-0.8f, -22}, 3, G_LINE, 0.6f);
  for (int s = -1; s <= 1; s += 2) {
    Vec2 e{s * 4.0f, -24.2f};
    fill(cv, Ellipse{e, {2.2f, 1.7f}, s * 0.2f}, Solid{G_LINE});
    fill(cv, Ellipse{e, {1.8f, 1.35f}, s * 0.2f}, Solid{G_EYE});
    fill(cv, Ellipse{e + Vec2{s * 0.3f, 0}, {0.55f, 1.15f}}, Solid{hex(0x1a1208)});
    Path brow;
    brow.move(e + Vec2{-s * 2.4f, -1.4f}).line(e + Vec2{s * 2.2f, -3.0f});
    fill(cv, brow.stroke(1.0f, 0.6f), Solid{G_SKIN_D});
  }
  Path m;
  m.move({-4.8f, -17.6f}).quad({0, -14.6f}, {4.8f, -17.6f}).quad({0, -16.4f}, {-4.8f, -17.6f});
  fill(cv, m.poly(), Solid{hex(0x3a1414)});
  for (int i = -1; i <= 1; i += 2) {
    Path t;
    t.move({i * 2.6f - 0.7f, -17.0f}).line({i * 2.6f, -15.6f}).line({i * 2.6f + 0.7f, -16.9f});
    fill(cv, t.poly(), Solid{hex(0xfffbe8)});
  }
  goblin_helmet(cv, 0);
}

void goblin_foot(Canvas& cv) {
  Ellipse f{{0, 0.6f}, {3.3f, 2.1f}};
  part(cv, f, hex(0x5a3a24), hex(0x8a6040), hex(0x341f12), {-1, 0}, 4);
}

void goblin_club(Canvas& cv) {
  Capsule handle{{0, 2}, {0, -9}, 1.0f, 1.3f};
  part(cv, handle, hex(0x8a5a34), hex(0xb8875a), hex(0x5a3820), {-0.5f, -3}, 6);
  Ellipse head{{0, -13.0f}, {3.4f, 5.2f}};
  part(cv, head, hex(0x9a6a40), hex(0xc89a6a), hex(0x5a3820), {-1, -15}, 6);
  for (int i = 0; i < 4; i++) {
    Vec2 p{(i % 2 ? 1.0f : -1.0f) * 3.0f, -15.5f + i * 1.8f};
    Path s;
    s.move(p + Vec2{0, -0.7f}).line(p + Vec2{(i % 2 ? 2.0f : -2.0f), 0}).line(p + Vec2{0, 0.7f});
    fill(cv, s.poly(), Solid{hex(0xd8d4c8)});
  }
}

void alert(Canvas& cv) {
  Path burst;
  for (int i = 0; i < 16; i++) {
    f32 a = i * TAU / 16;
    Vec2 v = from_angle(a, i % 2 ? 4.2f : 6.4f);
    if (i == 0) burst.move(v); else burst.line(v);
  }
  fill(cv, grow(burst.poly(), 0.6f), Solid{hex(0x5a1010)});
  fill(cv, burst.poly(), Radial{{0, 0}, 6, hex(0xffe066), hex(0xff6a2a)});
  fill(cv, RBox{{0, -1.2f}, {0.8f, 2.2f}, 0.6f}, Solid{hex(0x3a0a0a)});
  fill(cv, Circle{{0, 2.6f}, 0.85f}, Solid{hex(0x3a0a0a)});
}

// ---------------------------------------------------------------- pickups, waystones, shrine
Path heart_path(Vec2 c, f32 s) {
  Path p;
  p.move(c + Vec2{0, 5.5f} * s).cubic(c + Vec2{-3, 3} * s, c + Vec2{-7, -0.5f} * s, c + Vec2{-6.2f, -3.6f} * s)
      .cubic(c + Vec2{-5.4f, -6.8f} * s, c + Vec2{-1.2f, -6.6f} * s, c + Vec2{0, -3.4f} * s)
      .cubic(c + Vec2{1.2f, -6.6f} * s, c + Vec2{5.4f, -6.8f} * s, c + Vec2{6.2f, -3.6f} * s)
      .cubic(c + Vec2{7, -0.5f} * s, c + Vec2{3, 3} * s, c + Vec2{0, 5.5f} * s);
  return p;
}

void heart(Canvas& cv) {
  Path p = heart_path({0, -7}, 1.0f);
  fill(cv, grow(p.poly(), 0.8f), Solid{hex(0x5a0a1a)});
  fill(cv, p.poly(), Volume{hex(0xf0405a), hex(0xff9aa8), hex(0xa01830), {-2.5f, -10}, 8});
  fill(cv, Ellipse{{-3.0f, -10.5f}, {1.6f, 1.0f}, -0.6f}, Solid{{1, 1, 1, 0.8f}});
  Path leaf;
  leaf.move({0, -10.6f}).quad({2.4f, -14.6f}, {5.0f, -13.6f}).quad({3.0f, -11.4f}, {0, -10.6f});
  fill(cv, grow(leaf.poly(), 0.5f), Solid{hex(0x1d4a1a)});
  fill(cv, leaf.poly(), Solid{hex(0x5aa83a)});
}

void waystone(Canvas& cv) {
  fill(cv, Ellipse{{0, 0}, {11, 3.5f}}, Solid{hex(0x1d2a22, 0.35f)}, Blend::Over, 3);
  Path s;
  s.move({-7.5f, 1}).line({-8.5f, -14}).quad({-8.0f, -27}, {0, -29}).quad({8.0f, -27}, {8.2f, -14}).line({7.5f, 1}).quad({0, 2.4f}, {-7.5f, 1});
  part(cv, s.poly(), hex(0x9aa0aa), hex(0xd2d6dc), hex(0x5a606c), {-3, -18}, 16, hex(0x2c3038));
  fill(cv, wobble(Ellipse{{-2, -25}, {6, 3}}, 1.2f, 0.6f, 4), Solid{hex(0x7ab05a)}, Blend::Atop);
  Path r;  // carved rune
  r.move({0, -22}).line({0, -6});
  Path r2;
  r2.move({-4, -17}).line({0, -13}).line({4, -17});
  Path r3;
  r3.move({-3.5f, -9}).line({0, -6}).line({3.5f, -9});
  fill(cv, r.stroke(1.4f), Solid{hex(0x3a404a)}, Blend::Atop);
  fill(cv, r2.stroke(1.2f), Solid{hex(0x3a404a)}, Blend::Atop);
  fill(cv, r3.stroke(1.2f), Solid{hex(0x3a404a)}, Blend::Atop);
}

void waystone_lit(Canvas& cv) {
  // the rune's glow, white so it can be tinted and drawn additively
  Path r, r2, r3;
  r.move({0, -22}).line({0, -6});
  r2.move({-4, -17}).line({0, -13}).line({4, -17});
  r3.move({-3.5f, -9}).line({0, -6}).line({3.5f, -9});
  fill(cv, r.stroke(3.2f), Solid{{1, 1, 1, 0.4f}}, Blend::Over, 2.5f);
  fill(cv, r.stroke(1.2f), Solid{{1, 1, 1, 1}});
  fill(cv, r2.stroke(1.1f), Solid{{1, 1, 1, 1}});
  fill(cv, r3.stroke(1.1f), Solid{{1, 1, 1, 1}});
}

void pedestal(Canvas& cv) {
  fill(cv, Ellipse{{0, 0}, {18, 5}}, Solid{hex(0x1d2a22, 0.35f)}, Blend::Over, 3);
  Ellipse base{{0, -2}, {15, 5}};
  part(cv, base, hex(0xa8a196), hex(0xd8d2c6), hex(0x6a6458), {-4, -4}, 15, hex(0x302c26));
  Path col;
  col.move({-8, -3}).line({-7, -17}).line({7, -17}).line({8, -3}).quad({0, -1}, {-8, -3});
  part(cv, col.poly(), hex(0xb4ada2), hex(0xe2ddd2), hex(0x726c60), {-3, -11}, 12, hex(0x302c26));
  Ellipse top{{0, -18}, {11, 3.6f}};
  part(cv, top, hex(0xc6bfb4), hex(0xf0ece4), hex(0x86806f), {-3, -19}, 11, hex(0x302c26));
  for (int i = 0; i < 3; i++) {
    Path g;
    f32 x = -4.0f + i * 4.0f;
    g.move({x, -15}).line({x, -5});
    fill(cv, g.stroke(0.8f), Solid{hex(0x6a6458, 0.6f)}, Blend::Atop);
  }
  fill(cv, wobble(Ellipse{{-5, -16}, {5, 2}}, 1, 0.7f, 2), Solid{hex(0x7ab05a)}, Blend::Atop);
}

void ingredient(Canvas& cv, int which) {
  switch (which) {
    case 0: {  // the Scroll of Knowledge
      Capsule roll{{-8, -8}, {8, -8}, 3.6f, 3.6f};
      part(cv, roll, hex(0xf3e2b8), hex(0xfff7e0), hex(0xc8aa70), {-3, -10}, 9, hex(0x5a4020));
      for (int s = -1; s <= 1; s += 2) {
        Ellipse end{{s * 9.0f, -8}, {1.6f, 4.4f}};
        part(cv, end, hex(0xd8a83a), hex(0xffe08a), hex(0x8a6418), {s * 9.0f - 0.5f, -9}, 4, hex(0x4a3410));
      }
      fill(cv, RBox{{0, -8}, {1.5f, 4.0f}, 0.5f}, Solid{hex(0xc8303a)}, Blend::Atop);
      fill(cv, Circle{{0, -4.0f}, 2.2f}, Volume{hex(0xc8303a), hex(0xff7a7a), hex(0x801820), {-0.6f, -4.6f}, 2.4f});
      for (int i = 0; i < 3; i++) fill(cv, RBox{{-5.0f + i * 2.4f + (i > 0) * 4.8f, -9.5f}, {0.7f, 0.25f}}, Solid{hex(0x8a6a3a, 0.7f)}, Blend::Atop);
      break;
    }
    case 1: {  // the Water of Miracles
      Path f;
      f.move({-2.2f, -20}).line({-2.2f, -15}).cubic({-8.5f, -13}, {-8.5f, -1}, {0, -1}).cubic({8.5f, -1}, {8.5f, -13}, {2.2f, -15}).line({2.2f, -20});
      Poly glass = f.poly();
      fill(cv, grow(glass, 0.8f), Solid{hex(0x1a3a4a)});
      fill(cv, glass, Solid{hex(0xd8f4ff, 0.55f)});
      Circle liquid{{0, -8}, 6.6f};
      fill(cv, intersect(liquid, RBox{{0, -4.5f}, {8, 5}}), Linear{{0, -11}, {0, -2}, hex(0x7af0ff), hex(0x2a9ad8)}, Blend::Atop);
      fill(cv, Ellipse{{-2.8f, -10.5f}, {1.2f, 2.6f}, 0.3f}, Solid{{1, 1, 1, 0.75f}});
      RBox cork{{0, -21.2f}, {2.6f, 1.8f}, 0.6f};
      part(cv, cork, hex(0xb8875a), hex(0xe0b48a), hex(0x7a5232), {-1, -22}, 3, hex(0x3a2412), 0.6f);
      break;
    }
    case 2: {  // the Fire of Power, burning in a golden bowl
      Path bowl;
      bowl.move({-8, -8}).quad({-7, -1}, {0, -1}).quad({7, -1}, {8, -8}).line({-8, -8});
      part(cv, bowl.poly(), hex(0xe2b33a), hex(0xffe48a), hex(0x9a6a14), {-3, -6}, 9, hex(0x4a3008));
      fill(cv, Ellipse{{0, -8}, {8, 1.6f}}, Solid{hex(0x5a3008)});
      Path fl;
      fl.move({0, -26}).cubic({3.5f, -19}, {7, -14}, {5.5f, -9}).quad({0, -6.5f}, {-5.5f, -9}).cubic({-7, -14}, {-3.5f, -19}, {0, -26});
      fill(cv, fl.poly(), Linear{{0, -26}, {0, -8}, hex(0xffb03a), hex(0xfff6d8)});
      Path in;
      in.move({0, -19}).quad({3, -13}, {0, -9.5f}).quad({-3, -13}, {0, -19});
      fill(cv, in.poly(), Solid{hex(0xffffff, 0.85f)});
      break;
    }
    case 3: {  // the Elder Staff
      Path st;
      st.move({-1.2f, 0}).quad({-1.6f, -14}, {-0.6f, -27}).line({1.2f, -27}).quad({0.4f, -14}, {1.2f, 0}).line({-1.2f, 0});
      part(cv, st.poly(), hex(0x8a5a30), hex(0xc0905a), hex(0x4a2c14), {-1, -14}, 14, hex(0x2a1608), 0.7f);
      Path curl;
      curl.move({0, -26}).cubic({-6, -28}, {-6, -36}, {0, -36}).cubic({5, -36}, {5, -30}, {1.5f, -29.5f});
      fill(cv, curl.stroke(2.4f, 1.4f), Solid{hex(0x2a1608)});
      fill(cv, curl.stroke(1.6f, 0.8f), Solid{hex(0xa06a3a)});
      Circle gem{{-0.6f, -31.6f}, 2.6f};
      part(cv, gem, hex(0x4ad88a), hex(0xb8ffd8), hex(0x1a7a4a), {-1.4f, -32.4f}, 3, hex(0x0a3a20), 0.6f);
      for (int i = 0; i < 3; i++) fill(cv, Ellipse{{1.0f, -6.0f - i * 6.5f}, {1.6f, 0.7f}, 0.4f}, Solid{hex(0x6aa84a)}, Blend::Over);
      break;
    }
    default: {  // the Feather of the Four Winds
      Path f;
      f.move({0, 0}).cubic({-1, -6}, {-9, -14}, {-6, -24}).cubic({-3, -30}, {3, -30}, {4, -26}).cubic({6, -16}, {3, -8}, {0, 0});
      Poly vane = f.poly();
      fill(cv, grow(vane, 0.8f), Solid{hex(0x2a3a6a)});
      fill(cv, vane, Linear{{-2, -2}, {0, -28}, hex(0xfff8ff), hex(0x9ad0ff)});
      fill(cv, vane, [](Vec2 p, f32) { f32 t = saturate((-p.y - 8) / 20); return Color{0.75f, 0.6f, 1.0f, 0.4f * t * t}; }, Blend::Atop);
      for (int i = 0; i < 7; i++) {
        Path b;
        f32 y = -5.0f - i * 3.2f;
        b.move({-0.3f + i * 0.1f, y}).quad({-3.0f, y - 2.0f}, {-5.0f + i * 0.2f, y - 2.6f});
        fill(cv, b.stroke(0.4f, 0.2f), Solid{hex(0x6a8ad0, 0.6f)}, Blend::Atop);
      }
      Path q;
      q.move({0.8f, 2}).quad({-1.2f, -12}, {-1.2f, -26});
      fill(cv, q.stroke(0.9f, 0.4f), Solid{hex(0xf6f0e6)});
      break;
    }
  }
}

void arrow(Canvas& cv) {
  Path p;
  p.move({-5, -5}).line({4, 0}).line({-5, 5}).line({-2.5f, 0}).line({-5, -5});
  fill(cv, grow(p.poly(), 1.1f), Solid{hex(0x3a2408)});
  fill(cv, p.poly(), Linear{{-5, -5}, {4, 4}, hex(0xffe28a), hex(0xe9a12a)});
}

// ---------------------------------------------------------------- hazards
void vent(Canvas& cv) {
  fill(cv, wobble(Ellipse{{0, 0}, {13, 5.5f}}, 1.5f, 0.4f, 3), Solid{hex(0x161214)});
  fill(cv, wobble(Ellipse{{0, 0}, {9, 3.4f}}, 1.0f, 0.5f, 5), Radial{{0, 0}, 9, hex(0xffd36a), hex(0xd8401a)});
  for (int i = 0; i < 5; i++) {
    Path c;
    f32 a = i * TAU / 5 + 0.3f;
    c.move(from_angle(a, 8.5f) * Vec2{1, 0.42f}).line(from_angle(a + 0.25f, 14) * Vec2{1, 0.42f});
    fill(cv, c.stroke(1.0f, 0.3f), Solid{hex(0xff8a3a)});
  }
}

void snapjaw(Canvas& cv, bool open) {
  Color green = hex(0x4a9a3a), dark = hex(0x2a5a24), line = hex(0x16301a), spot = hex(0x8a3ad0);
  for (int s = -1; s <= 1; s += 2) {
    Path leaf;
    leaf.move({0, -1}).quad({s * 8.0f, -1}, {s * 13.0f, -6}).quad({s * 6.0f, -5}, {0, -1});
    fill(cv, grow(leaf.poly(), 0.7f), Solid{line});
    fill(cv, leaf.poly(), Solid{green});
  }
  Capsule st{{0, 0}, {0, -9}, 1.6f, 1.3f};
  fill(cv, grow(st, 0.7f), Solid{line});
  fill(cv, st, Solid{dark});
  if (!open) {
    Ellipse bud{{0, -15}, {6.5f, 7.5f}};
    part(cv, bud, hex(0x5aa84a), hex(0x9ad87a), dark, {-2, -18}, 8, line);
    for (int i = 0; i < 4; i++) fill(cv, Circle{{-2.5f + (i % 2) * 4.5f, -18.0f + i * 2.2f}, 1.0f}, Solid{spot}, Blend::Atop);
    Path seam;
    seam.move({-5, -15}).quad({0, -13}, {5, -15});
    fill(cv, seam.stroke(0.7f), Solid{line}, Blend::Atop);
  } else {
    for (int s = -1; s <= 1; s += 2) {
      Path jaw;
      jaw.move({0, -10}).quad({s * 9.0f, -10}, {s * 9.5f, -19}).quad({s * 6.0f, -24}, {s * 2.0f, -22}).quad({s * 3.0f, -15}, {0, -10});
      part(cv, jaw.poly(), hex(0x5aa84a), hex(0x9ad87a), dark, {s * 4.0f, -18}, 8, line);
      fill(cv, Ellipse{{s * 4.6f, -16}, {2.4f, 4.2f}, s * 0.4f}, Solid{hex(0xd84a6a)}, Blend::Atop);
      for (int t = 0; t < 4; t++) {
        Vec2 p{s * (2.4f + t * 1.6f), -21.0f + t * 2.6f};
        Path tooth;
        tooth.move(p).line(p + Vec2{-s * 1.6f, 0.6f}).line(p + Vec2{0, 1.4f});
        fill(cv, tooth.poly(), Solid{hex(0xfffbe8)});
      }
    }
  }
}

void rune_circle(Canvas& cv) {
  fill(cv, Ellipse{{0, 0}, {15, 7}}, Solid{{1, 1, 1, 0.25f}}, Blend::Over, 2.5f);
  fill(cv, subtract(Ellipse{{0, 0}, {14, 6.4f}}, Ellipse{{0, 0}, {12.4f, 5.4f}}), Solid{{1, 1, 1, 1}});
  fill(cv, subtract(Ellipse{{0, 0}, {8.6f, 3.8f}}, Ellipse{{0, 0}, {7.6f, 3.2f}}), Solid{{1, 1, 1, 0.9f}});
  for (int i = 0; i < 6; i++) {
    f32 a = i * TAU / 6;
    Vec2 p = from_angle(a) * Vec2{10.5f, 4.6f};
    Path g;
    g.move(p + Vec2{-0.9f, -0.6f}).line(p + Vec2{0.9f, 0.6f});
    fill(cv, g.stroke(0.7f), Solid{{1, 1, 1, 1}});
    fill(cv, Circle{p, 0.7f}, Solid{{1, 1, 1, 1}});
  }
}

void mud(Canvas& cv) {
  Blob b;
  b.k = 4;
  b.add({-7, 0}, 7).add({6, 1}, 7.5f).add({0, -2}, 6).add({2, 3}, 6);
  auto shape = wobble(intersect(b, RBox{{0, 0}, {20, 6.5f}}), 1.4f, 0.35f, 8);
  fill(cv, shape, [](Vec2 p, f32 d) {
    f32 k = saturate(-d / 4.0f);
    return mix(Color{0.36f, 0.28f, 0.18f, 0.7f}, Color{0.22f, 0.17f, 0.11f, 0.95f}, k);
  }, Blend::Over, 1.5f);
  fill(cv, Ellipse{{-4, -1.5f}, {3.5f, 1.0f}}, Solid{{1, 1, 1, 0.18f}}, Blend::Atop, 1.0f);
}

// ---------------------------------------------------------------- goblin camps
void tent(Canvas& cv) {
  fill(cv, Ellipse{{0, 0}, {26, 7}}, Solid{hex(0x1d2a22, 0.4f)}, Blend::Over, 4);
  Path t;
  t.move({-22, 0}).line({0, -34}).line({22, 0}).quad({0, 3}, {-22, 0});
  Poly hide = t.poly();
  fill(cv, grow(hide, 1.0f), Solid{hex(0x2a1c12)});
  fill(cv, hide, noisy(Linear{{-20, -10}, {18, 0}, hex(0xb08a5a), hex(0x6a4a2a)}, 0.05f, 0.5f));
  fill(cv, RBox{{9, -8}, {4.5f, 4}, 0.5f, 0.2f}, Solid{hex(0x8a6a3a)}, Blend::Atop);  // a patch
  for (int i = 0; i < 4; i++) {
    Path s;
    s.move({7.0f + i * 1.6f, -11.0f}).line({7.2f + i * 1.6f, -4.5f});
    fill(cv, s.stroke(0.5f), Solid{hex(0x3a2414)}, Blend::Atop);
  }
  Path door;
  door.move({-6, 0}).line({0, -16}).line({5, 0}).quad({0, 1.4f}, {-6, 0});
  fill(cv, door.poly(), Solid{hex(0x1a120c)});
  fill(cv, Capsule{{0, -34}, {-3, -42}, 0.9f, 0.7f}, Solid{hex(0x4a2e1a)});
  fill(cv, Capsule{{0, -34}, {4, -41}, 0.9f, 0.7f}, Solid{hex(0x4a2e1a)});
  Path flag;
  flag.move({4, -41}).line({11, -39}).line({5, -36.5f});
  fill(cv, flag.poly(), Solid{HEX_PURPLE});
}

void campfire(Canvas& cv) {
  for (int i = 0; i < 8; i++) {
    Vec2 p = from_angle(i * TAU / 8) * Vec2{10, 4.5f};
    Ellipse s{p, {3.0f, 2.2f}};
    fill(cv, grow(s, 0.6f), Solid{hex(0x2a2a2a)});
    fill(cv, s, Volume{hex(0x8a8a8a), hex(0xc8c8c8), hex(0x505050), p + Vec2{-1, -1}, 3});
  }
  fill(cv, Ellipse{{0, 0}, {7, 3}}, Solid{hex(0x2a1a12)});
  Capsule l1{{-6, -1}, {5, 1.5f}, 1.6f, 1.4f}, l2{{-5, 2}, {6, -1.5f}, 1.6f, 1.4f};
  fill(cv, grow(l1, 0.6f), Solid{hex(0x2a1608)});
  fill(cv, l1, Solid{hex(0x7a4a28)});
  fill(cv, grow(l2, 0.6f), Solid{hex(0x2a1608)});
  fill(cv, l2, Solid{hex(0x8a5a30)});
  fill(cv, Ellipse{{0, 0}, {4, 1.6f}}, Solid{hex(0xff8a3a, 0.8f)}, Blend::Over, 1.5f);
}

void banner(Canvas& cv) {
  fill(cv, Ellipse{{0, 0}, {6, 2}}, Solid{hex(0x1d2a22, 0.35f)}, Blend::Over, 2);
  Capsule pole{{0, 0}, {0, -44}, 1.2f, 1.0f};
  fill(cv, grow(pole, 0.6f), Solid{hex(0x2a1608)});
  fill(cv, pole, Solid{hex(0x6a4428)});
  fill(cv, Circle{{0, -45}, 1.8f}, Solid{hex(0xb89a4a)});
  Path f;
  f.move({1, -42}).line({16, -42}).line({16, -20}).line({12, -23}).line({8.5f, -19}).line({5, -23}).line({1, -20});
  fill(cv, grow(f.poly(), 0.7f), Solid{hex(0x1a0a24)});
  fill(cv, f.poly(), Linear{{1, -42}, {16, -20}, hex(0x6a2aa8), hex(0x3a1260)});
  fill(cv, Ellipse{{8.5f, -32}, {4.4f, 2.6f}}, Solid{hex(0xf2d24a)});
  fill(cv, Circle{{8.5f, -32}, 1.6f}, Solid{hex(0x1a0a24)});
}

void torch(Canvas& cv) {
  fill(cv, Ellipse{{0, 0}, {5, 2}}, Solid{hex(0x1d2a22, 0.35f)}, Blend::Over, 2);
  Capsule post{{0, 0}, {0, -22}, 1.4f, 1.2f};
  fill(cv, grow(post, 0.6f), Solid{hex(0x2a1608)});
  fill(cv, post, Linear{{-1, 0}, {1, 0}, hex(0x8a5a34), hex(0x5a3820)});
  Path bowl;
  bowl.move({-4.5f, -26}).quad({-4, -21}, {0, -21}).quad({4, -21}, {4.5f, -26}).line({-4.5f, -26});
  fill(cv, grow(bowl.poly(), 0.6f), Solid{hex(0x1a1a1a)});
  fill(cv, bowl.poly(), Solid{hex(0x5a5a62)});
}

void talk_icon(Canvas& cv) {
  Path b;
  b.move({-7, -3}).cubic({-7, -10}, {7, -10}, {7, -3}).cubic({7, 2}, {2, 2.5f}, {-1, 2.5f}).line({-4, 5.5f}).line({-3.5f, 2.2f}).cubic({-6, 1.6f}, {-7, 0}, {-7, -3});
  fill(cv, grow(b.poly(), 1.0f), Solid{hex(0x2a2238)});
  fill(cv, b.poly(), Linear{{0, -9}, {0, 3}, hex(0xffffff), hex(0xe8e0d0)});
  for (int i = -1; i <= 1; i++) fill(cv, Circle{{i * 3.0f, -3.4f}, 1.1f}, Solid{hex(0x6a5a8a)});
}

void wind_streak(Canvas& cv) {
  Path p;
  p.move({-14, 0}).quad({0, -2.5f}, {14, 0});
  fill(cv, p.stroke(1.4f, 0.3f), Solid{{1, 1, 1, 1}});
}

}  // namespace

void bake_gameplay(Atlas& a) {
  GameArt& G = S.game;
  for (int v = 0; v < 3; v++) G.goblin.body[v] = a.bake({-21, -37, 21, 3}, SC, [&](Canvas& cv) { goblin_body(cv, (View)v); });
  G.goblin.foot = a.bake({-4.5f, -2.5f, 4.5f, 3.5f}, SC, [](Canvas& cv) { goblin_foot(cv); });
  G.goblin.club = a.bake({-6, -20, 6, 4}, SC, [](Canvas& cv) { goblin_club(cv); });
  G.goblin.alert = a.bake({-8, -8, 8, 8}, 4, [](Canvas& cv) { alert(cv); });
  G.heart = a.bake({-8, -16, 8, 1}, 4, [](Canvas& cv) { heart(cv); });
  G.heart_glow = S.soft;
  G.waystone = a.bake({-12, -31, 12, 4}, 2.5f, [](Canvas& cv) { waystone(cv); });
  G.waystone_lit = a.bake({-7, -26, 7, -2}, 3, [](Canvas& cv) { waystone_lit(cv); });
  G.pedestal = a.bake({-19, -23, 19, 6}, 2.5f, [](Canvas& cv) { pedestal(cv); });
  for (int i = 0; i < INGREDIENT_COUNT; i++) G.ingredient[i] = a.bake({-13, -38, 13, 3}, 3.5f, [&](Canvas& cv) { ingredient(cv, i); });
  G.arrow = a.bake({-7, -7, 7, 7}, 4, [](Canvas& cv) { arrow(cv); });
  G.vent = a.bake({-16, -8, 16, 8}, 2.5f, [](Canvas& cv) { vent(cv); });
  G.snapjaw[0] = a.bake({-15, -25, 15, 3}, 3, [](Canvas& cv) { snapjaw(cv, false); });
  G.snapjaw[1] = a.bake({-15, -27, 15, 3}, 3, [](Canvas& cv) { snapjaw(cv, true); });
  G.rune = a.bake({-17, -9, 17, 9}, 3, [](Canvas& cv) { rune_circle(cv); });
  G.mud = a.bake({-22, -9, 22, 9}, 2.5f, [](Canvas& cv) { mud(cv); });
  G.tent = a.bake({-28, -46, 28, 8}, 2.5f, [](Canvas& cv) { tent(cv); });
  G.campfire = a.bake({-15, -7, 15, 7}, 3, [](Canvas& cv) { campfire(cv); });
  G.banner = a.bake({-8, -49, 19, 3}, 2.5f, [](Canvas& cv) { banner(cv); });
  G.torch = a.bake({-7, -29, 7, 3}, 3, [](Canvas& cv) { torch(cv); });
  G.wind = a.bake({-16, -4, 16, 3}, 3, [](Canvas& cv) { wind_streak(cv); });
  G.talk = a.bake({-9, -11, 9, 7}, 4, [](Canvas& cv) { talk_icon(cv); });
  bake_butterflies(a);
}

}  // namespace sky
