#include "fx.h"
#include "world.h"

namespace sky {

void Fx::update(f32 dt, Rect view) {
  for (int i = 0; i < count;) {
    Particle& p = parts[i];
    p.life += dt;
    if (p.life >= p.max_life) { parts[i] = parts[--count]; continue; }
    p.vel += p.accel * dt;
    if (p.drag > 0) p.vel *= expf(-p.drag * dt);
    p.pos += p.vel * dt;
    if (p.spin < 98) p.rot += p.spin * dt;
    if (p.gz != 0) {
      p.vz += p.gz * dt;
      p.z += p.vz * dt;
      if (p.z < 0) { p.z = 0; p.vz = -p.vz * 0.35f; p.vel *= 0.5f; p.spin *= 0.5f; }
    }
    i++;
  }

  // what drifts through the air around the camera: pollen, spores, ash, fireflies, wind, curse motes
  mote_timer -= dt;
  while (mote_timer <= 0) {
    const f32 rate[6] = {0.12f, 0.07f, 0.045f, 0.1f, 0.09f, 0.08f};
    mote_timer += rate[ambience < 6 ? ambience : 0];
    Vec2 at{rng.range(view.x0, view.x1), rng.range(view.y0, view.y1)};
    switch (ambience) {
      case AMB_SPORES: spore(at, rng.chance(0.6f) ? Color{0.55f, 1.0f, 0.85f, 0.8f} : Color{0.85f, 1.0f, 0.6f, 0.7f}); break;
      case AMB_FIREFLIES: firefly(at); break;
      case AMB_CURSE: spore(at, rng.chance(0.7f) ? Color{0.8f, 0.55f, 1.0f, 0.85f} : Color{1.0f, 0.6f, 0.9f, 0.7f}); break;
      case AMB_WIND:
        if (rng.chance(0.4f)) { wind_streak(at, {1, rng.range(-0.15f, 0.1f)}); break; }
        [[fallthrough]];
      case AMB_ASH:
        if (ambience == AMB_ASH) {
          Particle* a = spawn();
          if (!a) break;
          bool ember = rng.chance(0.18f);
          a->pos = at;
          a->vel = ember ? Vec2{rng.range(-6, 6), rng.range(-26, -10)} : Vec2{rng.range(6, 18), rng.range(8, 20)};
          a->max_life = rng.range(3.0f, 5.5f);
          a->size0 = a->size1 = ember ? rng.range(0.12f, 0.2f) : rng.range(0.16f, 0.3f);
          a->c0 = ember ? Color{1.0f, 0.62f, 0.25f, 0.9f} : Color{0.62f, 0.6f, 0.6f, 0.7f};
          a->sprite = ember ? S.mote : S.dust;
          a->layer = ember ? LAYER_GLOW : LAYER_TOP;
          a->add = ember ? 1.0f : 0.0f;
          a->spin = 99;
          a->rot = rng.range(0, TAU);
          break;
        }
        [[fallthrough]];
      default: {
        Particle* m = spawn();
        if (!m) break;
        m->pos = at;
        m->vel = {rng.range(4, 14), rng.range(-8, -2)};
        m->max_life = rng.range(3.5f, 6.0f);
        m->size0 = m->size1 = rng.range(0.25f, 0.5f);
        m->c0 = ambience == AMB_WIND ? Color{1.0f, 1.0f, 1.0f, 0.7f} : Color{1.0f, 0.92f, 0.62f, 0.85f};
        m->sprite = S.mote;
        m->layer = LAYER_GLOW;
        m->add = 1;
        m->spin = 99;  // marker: twinkle alpha in, out
      }
    }
  }

  for (Butterfly& b : butterflies) {
    b.t += dt;
    b.phase += dt * 14.0f;
    if (dist(b.pos, b.target) < 8 || b.t > 4) {
      b.target = b.home + rng.in_circle(70);
      b.t = 0;
      b.speed = rng.range(28, 50);
    }
    Vec2 d = normalize(b.target - b.pos);
    b.pos += (d * b.speed + Vec2{sinf(b.phase * 0.31f) * 16, cosf(b.phase * 0.23f) * 10}) * dt;
    b.z = 12 + sinf(b.phase * 0.2f) * 5;
  }
}

void Fx::draw(Renderer& r, f32 time) const {
  for (int i = 0; i < count; i++) {
    const Particle& p = parts[i];
    f32 t = p.life / p.max_life;
    f32 s = lerp(p.size0, p.size1, t);
    Color c = mix(p.c0, p.c1, t);
    if (p.spin == 99) {  // motes fade in and out and twinkle
      f32 a = sinf(t * PI) * (0.55f + 0.45f * sinf(time * 5 + i));
      c = p.c0.with_alpha(a * p.c0.a);
    } else if (p.spin == 98) {
      c = Color{1, 1, 1, sinf(t * PI) * 0.45f};
    }
    Vec2 at = p.pos - Vec2{0, p.z};
    r.draw({.id = p.sprite, .pos = at, .sort_y = p.pos.y + p.sort_bias, .layer = p.layer, .rot = p.spin == 99 && p.sprite == S.mote ? 0 : p.rot, .scale = {s, s}, .color = c, .add = p.add});
    if (p.light > 0) r.light(at, p.light * (1 - t * 0.5f), p.light_col, p.spin == 99 ? 0.45f * c.a : 0.5f * (1 - t));
  }
  for (const Butterfly& b : butterflies) {
    f32 flap = 0.25f + 0.75f * absf(sinf(b.phase));
    r.draw({.id = S.shadow, .pos = b.pos, .layer = LAYER_GROUND, .scale = {0.18f, 0.18f}, .color = {0, 0.1f, 0.05f, 0.18f}});
    r.draw({.id = S.butterfly[b.kind], .pos = b.pos - Vec2{0, b.z}, .sort_y = b.pos.y + 4, .layer = LAYER_WORLD, .rot = sinf(b.phase * 0.17f) * 0.3f, .scale = {flap * 0.85f, 0.85f}});
  }
}

void Fx::dust(Vec2 at, Vec2 dir, int n, f32 strength) {
  for (int i = 0; i < n; i++) {
    Particle* p = spawn();
    if (!p) return;
    p->pos = at + rng.in_circle(3);
    p->vel = (dir * rng.range(10, 40) + rng.in_circle(18)) * strength;
    p->drag = 4;
    p->accel = {0, -6};
    p->max_life = rng.range(0.35f, 0.6f);
    p->size0 = rng.range(0.25f, 0.4f) * strength;
    p->size1 = p->size0 * 2.2f;
    p->c0 = Color{0.86f, 0.78f, 0.62f, 0.55f};
    p->c1 = Color{0.86f, 0.78f, 0.62f, 0.0f};
    p->sprite = S.dust;
    p->layer = LAYER_TOP;
    p->rot = rng.range(0, TAU);
  }
}

void Fx::flame(Vec2 from, Vec2 dir, f32 speed, f32 spread) {
  Particle* p = spawn();
  if (!p) return;
  Vec2 d = rotate(dir, rng.range(-spread, spread));
  p->pos = from + rng.in_circle(1.5f);
  p->vel = d * speed * rng.range(0.75f, 1.1f);
  p->drag = 2.2f;
  p->accel = {0, -40};
  p->max_life = rng.range(0.32f, 0.5f);
  p->size0 = rng.range(0.35f, 0.5f);
  p->size1 = rng.range(0.9f, 1.3f);
  p->c0 = Color{1.0f, 0.92f, 0.7f, 0.9f};
  p->c1 = Color{1.0f, 0.35f, 0.1f, 0.0f};
  p->sprite = S.flame;
  p->layer = LAYER_GLOW;
  p->add = 0.35f;
  p->rot = atan2f(d.y, d.x) + HALF_PI;
  p->spin = rng.range(-2, 2);
  if (rng.chance(0.15f)) { p->light = 44; p->light_col = {1.0f, 0.62f, 0.28f, 1}; }
}

void Fx::smoke(Vec2 at, int n, f32 size) {
  for (int i = 0; i < n; i++) {
    Particle* p = spawn();
    if (!p) return;
    p->pos = at + rng.in_circle(6 * size);
    p->vel = {rng.range(-8, 8), rng.range(-30, -14)};
    p->drag = 0.8f;
    p->max_life = rng.range(0.9f, 1.6f);
    p->size0 = rng.range(0.4f, 0.6f) * size;
    p->size1 = p->size0 * 2.4f;
    p->c0 = Color{0.42f, 0.38f, 0.42f, 0.55f};
    p->c1 = Color{0.62f, 0.6f, 0.62f, 0.0f};
    p->sprite = S.smoke;
    p->layer = LAYER_TOP;
    p->rot = rng.range(0, TAU);
    p->spin = rng.range(-0.8f, 0.8f);
  }
}

void Fx::embers(Vec2 at, int n) {
  for (int i = 0; i < n; i++) {
    Particle* p = spawn();
    if (!p) return;
    p->pos = at + rng.in_circle(10);
    p->vel = {rng.range(-25, 25), rng.range(-70, -30)};
    p->drag = 1.2f;
    p->accel = {rng.range(-10, 10), 10};
    p->max_life = rng.range(0.6f, 1.2f);
    p->size0 = rng.range(0.12f, 0.22f);
    p->size1 = 0.05f;
    p->c0 = Color{1.0f, 0.8f, 0.4f, 1.0f};
    p->c1 = Color{1.0f, 0.3f, 0.1f, 0.0f};
    p->sprite = S.mote;
    p->layer = LAYER_GLOW;
    p->add = 1;
  }
}

void Fx::sparkle(Vec2 at, int n, Color c, f32 radius) {
  for (int i = 0; i < n; i++) {
    Particle* p = spawn();
    if (!p) return;
    p->pos = at + rng.in_circle(radius);
    p->vel = rng.in_circle(30) + Vec2{0, -15};
    p->drag = 3;
    p->max_life = rng.range(0.4f, 0.8f);
    p->size0 = rng.range(0.4f, 0.7f);
    p->size1 = 0.0f;
    p->c0 = c;
    p->c1 = c.with_alpha(0);
    p->sprite = S.spark;
    p->layer = LAYER_GLOW;
    p->add = 0.8f;
    p->rot = rng.range(0, TAU);
    p->spin = rng.range(-4, 4);
  }
}

void Fx::hex_sparks(Vec2 at, int n) {
  for (int i = 0; i < n; i++) {
    Particle* p = spawn();
    if (!p) return;
    p->pos = at + rng.in_circle(8);
    p->vel = rng.in_circle(40) + Vec2{0, -10};
    p->drag = 2.5f;
    p->max_life = rng.range(0.5f, 0.9f);
    p->size0 = rng.range(0.3f, 0.55f);
    p->size1 = 0.05f;
    p->c0 = Color{0.85f, 0.55f, 1.0f, 1};
    p->c1 = Color{0.5f, 0.2f, 0.9f, 0};
    p->sprite = rng.chance(0.3f) ? S.rune : S.spark;
    p->layer = LAYER_GLOW;
    p->add = 0.9f;
    p->rot = rng.range(0, TAU);
    p->spin = rng.range(-3, 3);
    if (rng.chance(0.2f)) { p->light = 26; p->light_col = {0.7f, 0.4f, 1.0f, 1}; }
  }
}

void Fx::splash(Vec2 at, int n) {
  for (int i = 0; i < n; i++) {
    Particle* p = spawn();
    if (!p) return;
    p->pos = at + rng.in_circle(4);
    p->vel = rng.in_circle(30);
    p->z = 1;
    p->vz = rng.range(40, 80);
    p->gz = -260;
    p->max_life = rng.range(0.4f, 0.7f);
    p->size0 = rng.range(0.12f, 0.2f);
    p->size1 = 0.08f;
    p->c0 = Color{0.85f, 0.95f, 1.0f, 0.9f};
    p->c1 = Color{0.85f, 0.95f, 1.0f, 0.0f};
    p->sprite = S.soft;
    p->layer = LAYER_TOP;
  }
}

void Fx::petal(Vec2 at) {
  Particle* p = spawn();
  if (!p) return;
  p->pos = at;
  p->vel = {rng.range(8, 22), rng.range(4, 12)};
  p->z = rng.range(40, 70);
  p->vz = -rng.range(6, 12);
  p->gz = 0.001f;
  p->max_life = rng.range(4.0f, 6.0f);
  p->size0 = p->size1 = rng.range(0.8f, 1.1f);
  p->c0 = Color{1, 1, 1, 1};
  p->c1 = Color{1, 1, 1, 0};
  p->sprite = S.petal;
  p->layer = LAYER_TOP;
  p->rot = rng.range(0, TAU);
  p->spin = rng.range(-2.5f, 2.5f);
}

void Fx::leaf(Vec2 at) {
  petal(at);
  if (count) {
    Particle& p = parts[count - 1];
    p.sprite = S.leaf;
  }
}

void Fx::ring(Vec2 at, Color c, f32 size) {
  Particle* p = spawn();
  if (!p) return;
  p->pos = at;
  p->max_life = 0.45f;
  p->size0 = size * 0.3f;
  p->size1 = size;
  p->c0 = c;
  p->c1 = c.with_alpha(0);
  p->sprite = S.soft_ring;
  p->layer = LAYER_GLOW;
  p->add = 0.7f;
}

void Fx::mud(Vec2 at, int n) {
  for (int i = 0; i < n; i++) {
    Particle* p = spawn();
    if (!p) return;
    p->pos = at + rng.in_circle(4);
    p->vel = rng.in_circle(24);
    p->z = 1;
    p->vz = rng.range(30, 60);
    p->gz = -260;
    p->max_life = rng.range(0.35f, 0.6f);
    p->size0 = rng.range(0.14f, 0.22f);
    p->size1 = 0.1f;
    p->c0 = Color{0.36f, 0.27f, 0.17f, 0.95f};
    p->c1 = Color{0.3f, 0.22f, 0.14f, 0.0f};
    p->sprite = S.soft;
    p->layer = LAYER_TOP;
  }
}

void Fx::spore(Vec2 at, Color c) {
  Particle* p = spawn();
  if (!p) return;
  p->pos = at;
  p->vel = {rng.range(-5, 5), rng.range(-14, -5)};
  p->accel = {rng.range(-2, 2), 0};
  p->max_life = rng.range(3.0f, 5.5f);
  p->size0 = p->size1 = rng.range(0.22f, 0.4f);
  p->c0 = c;
  p->sprite = S.mote;
  p->layer = LAYER_GLOW;
  p->add = 1;
  p->spin = 99;
}

void Fx::firefly(Vec2 at) {
  Particle* p = spawn();
  if (!p) return;
  p->pos = at;
  p->vel = rng.in_circle(10);
  p->accel = rng.in_circle(6);
  p->max_life = rng.range(4.0f, 7.0f);
  p->size0 = p->size1 = rng.range(0.3f, 0.45f);
  p->c0 = rng.chance(0.75f) ? Color{0.85f, 1.0f, 0.45f, 1.0f} : Color{0.5f, 0.9f, 1.0f, 1.0f};
  p->sprite = S.mote;
  p->layer = LAYER_GLOW;
  p->add = 1;
  p->spin = 99;
  p->z = rng.range(6, 22);
  if (rng.chance(0.3f)) { p->light = 30; p->light_col = p->c0; }
}

void Fx::fire(Vec2 at, f32 size) {
  Particle* p = spawn();
  if (!p) return;
  p->pos = at + Vec2{rng.range(-3.5f, 3.5f) * size, rng.range(-1, 1)};
  p->vel = {rng.range(-6, 6), rng.range(-34, -20) * size};
  p->drag = 1.5f;
  p->max_life = rng.range(0.35f, 0.6f);
  p->size0 = rng.range(0.35f, 0.5f) * size;
  p->size1 = 0.08f;
  p->c0 = Color{1.0f, 0.88f, 0.55f, 0.95f};
  p->c1 = Color{1.0f, 0.35f, 0.1f, 0.0f};
  p->sprite = S.flame;
  p->layer = LAYER_GLOW;
  p->add = 0.4f;
  p->rot = rng.range(-0.2f, 0.2f);
}

void Fx::wind_streak(Vec2 at, Vec2 dir) {
  Particle* p = spawn();
  if (!p) return;
  p->pos = at;
  p->vel = normalize(dir) * rng.range(180, 260);
  p->max_life = rng.range(0.5f, 0.9f);
  p->size0 = rng.range(0.8f, 1.3f);
  p->size1 = p->size0 * 1.4f;
  p->c0 = Color{1, 1, 1, 0.0f};
  p->c1 = Color{1, 1, 1, 0.0f};
  p->sprite = S.game.wind;
  p->layer = LAYER_TOP;
  p->rot = atan2f(dir.y, dir.x);
  p->spin = 98;  // marker: fade in and out
}

void Fx::seed_butterflies(const World& w, int n) {
  butterflies.clear();
  for (int tries = 0; tries < 2000 && butterflies.count < n; tries++) {
    const Prop& p = w.props[rng.irange(0, w.prop_count - 1)];
    if (p.kind != PK_FLOWER) continue;
    Butterfly b;
    b.home = p.pos;
    b.pos = p.pos + rng.in_circle(30);
    b.target = b.pos;
    b.kind = (u8)rng.irange(0, 2);
    b.phase = rng.range(0, 10);
    butterflies.push(b);
  }
}

}  // namespace sky
