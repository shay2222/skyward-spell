// art_common.cpp - particles, glows and shadows shared by every land.
#include "art.h"

namespace sky {

Sprites S;
using namespace paint;

void bake_common(Atlas& a) {
  // a plain white square, for fades and debug rectangles
  S.white = a.bake({-2, -2, 2, 2}, 4, [](Canvas& cv) { fill(cv, RBox{{0, 0}, {2, 2}}, Solid{{1, 1, 1, 1}}); });

  // a soft round falloff: shadows, glows, dust, light motes (tinted at draw time)
  S.soft = a.bake({-16, -16, 16, 16}, 4, [](Canvas& cv) {
    fill(cv, Circle{{0, 0}, 16}, [](Vec2 p, f32) {
      f32 t = saturate(length(p) / 16.0f);
      f32 k = (1 - t * t);
      return Color{1, 1, 1, k * k};
    });
  });

  S.soft_ring = a.bake({-16, -16, 16, 16}, 4, [](Canvas& cv) {
    fill(cv, Circle{{0, 0}, 16}, [](Vec2 p, f32) {
      f32 t = length(p) / 16.0f;
      f32 k = saturate(1.0f - absf(t - 0.78f) / 0.2f);
      return Color{1, 1, 1, k * k};
    });
  });

  // ground shadow: a flat soft ellipse
  S.shadow = a.bake({-16, -8, 16, 8}, 4, [](Canvas& cv) {
    fill(cv, Ellipse{{0, 0}, {14.5f, 6.5f}}, Solid{{1, 1, 1, 1}}, Blend::Over, 5.0f);
  });

  // a four-pointed sparkle
  S.spark = a.bake({-8, -8, 8, 8}, 6, [](Canvas& cv) {
    Path p;
    p.move({0, -8}).quad({0.6f, -0.6f}, {8, 0}).quad({0.6f, 0.6f}, {0, 8}).quad({-0.6f, 0.6f}, {-8, 0}).quad({-0.6f, -0.6f}, {0, -8});
    fill(cv, p.poly(), Solid{{1, 1, 1, 1}});
    fill(cv, Circle{{0, 0}, 2.2f}, Solid{{1, 1, 1, 1}}, Blend::Over, 2.0f);
  });

  // a five-pointed star for magic
  S.star = a.bake({-8, -8, 8, 8}, 6, [](Canvas& cv) {
    Path p;
    for (int i = 0; i < 10; i++) {
      f32 ang = -HALF_PI + i * (PI / 5);
      f32 r = (i % 2 == 0) ? 7.5f : 3.2f;
      Vec2 v = from_angle(ang, r);
      if (i == 0) p.move(v); else p.line(v);
    }
    fill(cv, grow(p.poly(), 0.6f), Solid{{1, 1, 1, 1}});
  });

  // a flame tongue, white-hot at the base; drawn additively
  S.flame = a.bake({-7, -16, 7, 6}, 5, [](Canvas& cv) {
    Path p;
    p.move({0, -15}).cubic({2.5f, -9}, {6.5f, -4}, {5.5f, 0.5f}).cubic({4.8f, 4.5f}, {-4.8f, 4.5f}, {-5.5f, 0.5f}).cubic({-6.5f, -4}, {-2.5f, -9}, {0, -15});
    fill(cv, p.poly(), [](Vec2 q, f32 d) {
      f32 t = saturate((q.y + 15.0f) / 19.0f);
      Color c = t < 0.55f ? mix(Color{1.0f, 0.42f, 0.12f, 0.0f}, Color{1.0f, 0.62f, 0.2f, 1}, t / 0.55f)
                          : mix(Color{1.0f, 0.62f, 0.2f, 1}, Color{1.0f, 0.95f, 0.75f, 1}, (t - 0.55f) / 0.45f);
      c.a *= saturate(-d * 0.5f + 0.2f) + 0.4f;
      return c;
    }, Blend::Over, 1.5f);
  });

  // a puff of smoke: lumpy and soft
  S.smoke = a.bake({-14, -14, 14, 14}, 4, [](Canvas& cv) {
    Blob b;
    b.k = 4;
    b.add({-4, 2}, 7).add({4, 1}, 7.5f).add({0, -4}, 8).add({-1, 4}, 6.5f);
    fill(cv, wobble(b, 1.5f, 0.3f, 9), [](Vec2 p, f32 d) {
      f32 k = saturate(-d / 6.0f);
      f32 l = 0.82f + 0.18f * saturate(-(p.x + p.y) / 14.0f);
      return Color{l, l, l, 0.85f * k + 0.15f};
    }, Blend::Over, 3.0f);
  });

  // a small leaf, a blossom petal, a light mote, a dust puff
  S.leaf = a.bake({-5, -3, 5, 3}, 6, [](Canvas& cv) {
    Ellipse e{{0, 0}, {4.6f, 2.0f}};
    fill(cv, e, Linear{{0, -2}, {0, 2}, hex(0x9fd65f), hex(0x4e9a3a)});
    Path vein;
    vein.move({-4, 0}).quad({0, -0.6f}, {4, 0});
    fill(cv, vein.stroke(0.5f), Solid{hex(0x3d7a2c, 0.7f)}, Blend::Atop);
  });
  S.petal = a.bake({-4, -3, 4, 3}, 6, [](Canvas& cv) {
    Path p;
    p.move({-3.6f, 0}).quad({-0.5f, -3.0f}, {3.6f, -0.4f}).quad({0.5f, 2.6f}, {-3.6f, 0});
    fill(cv, p.poly(), Linear{{-3, 0}, {3, 0}, hex(0xffe3ee), hex(0xf59ac0)});
  });
  S.mote = a.bake({-6, -6, 6, 6}, 6, [](Canvas& cv) {
    fill(cv, Circle{{0, 0}, 6}, [](Vec2 p, f32) {
      f32 t = saturate(length(p) / 6.0f);
      f32 core = saturate(1.0f - t * 3.0f);
      return Color{1, 1, 1, max(core, (1 - t) * (1 - t) * 0.55f)};
    });
  });
  S.dust = a.bake({-8, -8, 8, 8}, 4, [](Canvas& cv) {
    Blob b;
    b.k = 3;
    b.add({-2, 1}, 5).add({2.5f, 0}, 4.5f).add({0, -2.5f}, 4);
    fill(cv, b, Solid{{1, 1, 1, 1}}, Blend::Over, 4.0f);
  });

  // a magic rune, used by Malgrin's hex
  S.rune = a.bake({-7, -7, 7, 7}, 6, [](Canvas& cv) {
    fill(cv, Circle{{0, 0}, 6.2f}, Solid{{1, 1, 1, 0.85f}});
    fill(cv, Circle{{0, 0}, 5.0f}, Solid{{1, 1, 1, 1}}, Blend::Erase);
    Path p;
    p.move({-2.6f, 3}).line({0, -3.4f}).line({2.6f, 3}).line({-3.2f, -0.8f}).line({3.2f, -0.8f}).line({-2.6f, 3});
    fill(cv, p.stroke(0.9f), Solid{{1, 1, 1, 1}});
  });
}

}  // namespace sky
