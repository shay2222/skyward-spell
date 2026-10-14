// art_malgrin.cpp - Malgrin, the goblin sorcerer who hexed the dragons: a floating robed figure
// with a huge drooping hood, a crooked staff, and eyes that glow in the dark.
#include "art_util.h"

namespace sky {

namespace {

const Color ROBE = hex(0x4a2272), ROBE_L = hex(0x7c48b0), ROBE_D = hex(0x24103c), LINE = hex(0x140a1e);
const Color TRIM = hex(0xe0b040), TRIM_D = hex(0x8a6418);
const Color SKIN = hex(0x7fa84a), SKIN_L = hex(0xb8dc7a), SKIN_D = hex(0x466e2c);

template <class Sh>
void part(Canvas& cv, const Sh& shape, Color base, Color light, Color dark, Vec2 c, f32 r, f32 lw = 1.0f) {
  fill(cv, grow(shape, lw), Solid{LINE});
  fill(cv, shape, Volume{base, light, dark, c, r, 3.0f, 0.35f});
}

/** The robe: a bell of cloth from the shoulders to a ragged hem, sleeves at the sides. */
void robe(Canvas& cv) {
  // sleeves behind the body
  for (int s = -1; s <= 1; s += 2) {
    Path sl;
    sl.move({s * 9.0f, -38}).quad({s * 22.0f, -34}, {s * 25.0f, -20}).line({s * 27.0f, -14}).line({s * 19.0f, -16}).quad({s * 14.0f, -26}, {s * 8.0f, -28});
    part(cv, sl.poly(), ROBE, ROBE_L, ROBE_D, {s * 16.0f, -30}, 14);
    fill(cv, Ellipse{{s * 22.5f, -15.5f}, {4.6f, 2.0f}, s * -0.35f}, Solid{TRIM}, Blend::Atop);
  }
  Path r;
  r.move({-8, -40}).cubic({-14, -30}, {-20, -12}, {-22, -2});
  // ragged hem, left to right
  const f32 hem[9] = {-2, 1.5f, -1, 2.5f, -0.5f, 2, -1.5f, 1.5f, -2};
  for (int i = 0; i < 9; i++) r.line({-22.0f + i * 5.5f, hem[i]});
  r.cubic({20, -12}, {14, -30}, {8, -40}).quad({0, -43}, {-8, -40});
  Poly body = r.poly();
  part(cv, body, ROBE, ROBE_L, ROBE_D, {-6, -26}, 30);
  // folds
  for (int i = 0; i < 4; i++) {
    Path f;
    f32 x = -12.0f + i * 8.0f;
    f.move({x * 0.4f, -30}).quad({x * 0.8f, -16}, {x, 0});
    fill(cv, f.stroke(1.4f, 2.4f), Solid{ROBE_D.with_alpha(0.55f)}, Blend::Atop);
  }
  // gold trim down the front and along the hem
  Path front;
  front.move({0, -40}).line({0, -1});
  fill(cv, front.stroke(3.4f), Solid{TRIM_D}, Blend::Atop);
  fill(cv, front.stroke(2.2f), Solid{TRIM}, Blend::Atop);
  Path hemline;
  hemline.move({-22, -4});
  for (int i = 0; i < 9; i++) hemline.line({-22.0f + i * 5.5f, hem[i] - 3.0f});
  fill(cv, hemline.stroke(2.0f), Solid{TRIM}, Blend::Atop);
  // the clasp: Malgrin's eye
  fill(cv, Ellipse{{0, -24}, {5.2f, 3.4f}}, Solid{TRIM_D});
  fill(cv, Ellipse{{0, -24}, {4.2f, 2.6f}}, Solid{hex(0xffe8a0)});
  fill(cv, Circle{{0, -24}, 1.6f}, Solid{hex(0x6a1aa8)});
}

/** The head: a long, drooping hood; a green face with a hooked nose and pointed ears. */
void head(Canvas& cv) {
  // ears poking out through slits in the hood
  for (int s = -1; s <= 1; s += 2) {
    Path e;
    e.move({s * 8.0f, -10.0f}).quad({s * 18.0f, -14.0f}, {s * 25.0f, -18.0f}).quad({s * 18.0f, -7.0f}, {s * 8.5f, -4.5f});
    part(cv, e.poly(), SKIN, SKIN_L, SKIN_D, {s * 14.0f, -11}, 9, 0.9f);
    Path in;
    in.move({s * 10.0f, -9.0f}).quad({s * 17.0f, -12.0f}, {s * 21.0f, -15.5f}).quad({s * 16.0f, -8.0f}, {s * 10.0f, -6.0f});
    fill(cv, in.poly(), Solid{hex(0xd88a8a, 0.5f)}, Blend::Atop);
    fill(cv, Circle{{s * 20.0f, -14.0f}, 1.0f}, Solid{TRIM});  // an earring
  }
  // hood back and its long drooping tip
  Path hood;
  hood.move({-12, 0}).cubic({-15, -14}, {-10, -26}, {0, -28}).cubic({8, -30}, {16, -36}, {24, -30}).cubic({20, -28}, {14, -24}, {12, -14}).quad({13, -6}, {12, 0}).quad({0, 3}, {-12, 0});
  part(cv, hood.poly(), ROBE, ROBE_L, ROBE_D, {-4, -18}, 18);
  fill(cv, Circle{{24.5f, -30.5f}, 2.2f}, Solid{TRIM});  // a bell on the tip
  // the shadowed opening
  Ellipse open{{0, -10}, {9.2f, 10.0f}};
  fill(cv, open, Solid{hex(0x1a0a26)});
  // the face
  Ellipse face{{0, -8.5f}, {7.6f, 8.0f}};
  fill(cv, face, Volume{SKIN, SKIN_L, SKIN_D, {-2, -11}, 9, 2.0f, 0.45f});
  fill(cv, Ellipse{{0, -16.5f}, {9, 3}}, Solid{hex(0x1a0a26, 0.7f)}, Blend::Atop, 2.0f);  // hood shadow over the brow
  // eyes: dark sockets (the glow is drawn on top, unlit)
  for (int s = -1; s <= 1; s += 2) {
    fill(cv, Ellipse{{s * 3.4f, -11.0f}, {2.3f, 1.6f}, s * 0.25f}, Solid{hex(0x2a1a08)});
    fill(cv, Ellipse{{s * 3.4f, -11.0f}, {1.7f, 1.1f}, s * 0.25f}, Solid{hex(0xffd23a)});
    Path brow;
    brow.move({s * 1.0f, -12.2f}).line({s * 6.0f, -14.4f});
    fill(cv, brow.stroke(1.1f, 0.6f), Solid{SKIN_D});
  }
  // a long hooked nose
  Path nose;
  nose.move({-1.4f, -10}).quad({0.5f, -6}, {3.2f, -3.2f}).quad({1.0f, -2.6f}, {-1.6f, -4.4f}).line({-1.4f, -10});
  part(cv, nose.poly(), lighten(SKIN, 0.08f), SKIN_L, SKIN_D, {-0.5f, -7}, 4, 0.6f);
  fill(cv, Circle{{-3.6f, -5.0f}, 0.7f}, Solid{SKIN_D}, Blend::Atop);  // a wart
  // a wicked grin
  Path m;
  m.move({-4.6f, -3.4f}).quad({0, 0.2f}, {4.8f, -3.2f}).quad({0, -1.6f}, {-4.6f, -3.4f});
  fill(cv, m.poly(), Solid{hex(0x3a1414)});
  for (int i = -1; i <= 1; i += 2) {
    Path t;
    t.move({i * 2.4f - 0.6f, -2.6f}).line({i * 2.4f, -1.2f}).line({i * 2.4f + 0.6f, -2.5f});
    fill(cv, t.poly(), Solid{hex(0xfffbe8)});
  }
}

/** The crooked staff with a claw holding a glowing orb, and a green clawed hand. */
void staff(Canvas& cv) {
  Path st;
  st.move({-1.2f, 18}).quad({-2.6f, 0}, {-0.6f, -24}).line({1.4f, -24}).quad({-0.4f, 0}, {1.2f, 18}).line({-1.2f, 18});
  part(cv, st.poly(), hex(0x5a3a26), hex(0x8a6040), hex(0x2e1c10), {-1, -2}, 14, 0.8f);
  for (int s = -1; s <= 1; s += 2) {  // the claw
    Path c;
    c.move({0.4f, -23}).quad({s * 6.0f, -26}, {s * 4.5f, -34}).quad({s * 3.0f, -29}, {0.4f, -26});
    part(cv, c.poly(), hex(0x5a3a26), hex(0x8a6040), hex(0x2e1c10), {s * 3.0f, -28}, 5, 0.7f);
  }
  Circle orb{{0.4f, -31}, 4.6f};
  fill(cv, grow(orb, 0.8f), Solid{hex(0x1a0a26)});
  fill(cv, orb, Radial{{-1.0f, -32.5f}, 6, hex(0xf0c8ff), hex(0x7a2ad0)});
  fill(cv, Ellipse{{-1.2f, -33}, {1.4f, 0.9f}, -0.5f}, Solid{{1, 1, 1, 0.85f}});
  // the hand gripping it
  Ellipse hand{{0, 2}, {3.6f, 3.2f}};
  part(cv, hand, SKIN, SKIN_L, SKIN_D, {-1, 1}, 4, 0.8f);
  for (int i = 0; i < 3; i++) fill(cv, Capsule{{-2.0f + i * 1.8f, 0.5f}, {-2.6f + i * 1.8f, 4.6f}, 0.8f, 0.5f}, Solid{SKIN_D}, Blend::Atop);
}

/** A ball of hex magic, white so it can be tinted. */
void orb(Canvas& cv) {
  fill(cv, Circle{{0, 0}, 7}, Solid{{1, 1, 1, 0.35f}}, Blend::Over, 4);
  fill(cv, Circle{{0, 0}, 3.6f}, Solid{{1, 1, 1, 1}}, Blend::Over, 1.2f);
  for (int i = 0; i < 4; i++) {
    Path w;
    f32 a = i * HALF_PI;
    w.move(from_angle(a, 3.0f)).quad(from_angle(a + 0.6f, 6.0f), from_angle(a + 1.2f, 7.0f));
    fill(cv, w.stroke(1.0f, 0.2f), Solid{{1, 1, 1, 0.8f}});
  }
}

}  // namespace

void bake_malgrin(Atlas& a) {
  MalgrinArt& M = S.malgrin;
  M.robe = a.bake({-30, -46, 30, 6}, 3.0f, [](Canvas& cv) { robe(cv); });
  M.head = a.bake({-28, -38, 30, 5}, 3.0f, [](Canvas& cv) { head(cv); });
  M.staff = a.bake({-8, -38, 8, 22}, 3.0f, [](Canvas& cv) { staff(cv); });
  M.orb = a.bake({-9, -9, 9, 9}, 4.0f, [](Canvas& cv) { orb(cv); });
}

}  // namespace sky
