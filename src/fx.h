// fx.h - particles and small ambient life (butterflies, light motes).
#pragma once
#include "render.h"
#include "art.h"

namespace sky {

struct World;

struct Particle {
  Vec2 pos, vel, accel;
  f32 drag = 0;
  f32 life = 0, max_life = 1;
  f32 size0 = 1, size1 = 1;
  f32 rot = 0, spin = 0;
  Color c0 = {1, 1, 1, 1}, c1 = {1, 1, 1, 0};
  u16 sprite = 0;
  Layer layer = LAYER_TOP;
  f32 add = 0;
  f32 z = 0, vz = 0, gz = 0;  // height above the ground, for things that fall and bounce
  f32 light = 0;              // radius of light this particle gives off (0 = none)
  Color light_col = {1, 0.6f, 0.3f, 1};
  f32 sort_bias = 0;
};

struct Butterfly {
  Vec2 pos, target, home;
  f32 phase = 0, t = 0, speed = 40, z = 10;
  u8 kind = 0;
};

constexpr int MAX_PARTICLES = 3000;

struct Fx {
  Particle* parts = nullptr;
  int count = 0;
  FixedVec<Butterfly, 32> butterflies;
  Rng rng{77, 9};
  f32 mote_timer = 0;
  u8 ambience = AMB_POLLEN;

  void init(Arena& a) { parts = a.make<Particle>(MAX_PARTICLES); count = 0; }
  void clear() { count = 0; butterflies.clear(); }
  Particle* spawn() { return count < MAX_PARTICLES ? &(parts[count++] = Particle()) : nullptr; }

  void update(f32 dt, Rect view);
  void draw(Renderer& r, f32 time) const;

  // ready-made effects
  void dust(Vec2 at, Vec2 dir, int n, f32 strength = 1);
  void flame(Vec2 from, Vec2 dir, f32 speed, f32 spread);
  void smoke(Vec2 at, int n, f32 size = 1);
  void embers(Vec2 at, int n);
  void sparkle(Vec2 at, int n, Color c, f32 radius = 12);
  void hex_sparks(Vec2 at, int n);
  void splash(Vec2 at, int n);
  void petal(Vec2 at);
  void leaf(Vec2 at);
  void ring(Vec2 at, Color c, f32 size);
  void mud(Vec2 at, int n);
  void spore(Vec2 at, Color c);
  void firefly(Vec2 at);
  void fire(Vec2 at, f32 size);
  void wind_streak(Vec2 at, Vec2 dir);
  void seed_butterflies(const World& w, int n);
};

}  // namespace sky
