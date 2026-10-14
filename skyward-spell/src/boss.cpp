#include "boss.h"
#include "actors.h"
#include "fx.h"
#include "hero.h"

namespace sky {

namespace {
const Color HEX{0.72f, 0.42f, 1.0f, 1};
const Color HEX_HOT{0.9f, 0.7f, 1.0f, 1};

Vec2 approach(Vec2 v, Vec2 target, f32 step) {
  Vec2 d = target - v;
  f32 l = length(d);
  return l <= step ? target : v + d / l * step;
}
}  // namespace

void Boss::appear(Vec2 at) {
  *this = Boss();
  pos = prev_pos = target = at;
  state = BS_APPARITION;
  alpha = 0;
  z = 46;
  scale = 1.05f;
  face = -1;
}

void Boss::arrive(Vec2 centre, f32 radius) {
  *this = Boss();
  arena = centre;
  arena_r = radius;
  pos = prev_pos = target = centre + Vec2{radius * 0.35f, -radius * 0.4f};
  state = BS_ARRIVE;
  alpha = 0;
  z = 90;
  face = -1;
}

void Boss::calm() {
  orbs.clear();
  runes.clear();
  if (fighting()) {
    set(BS_HOVER);
    attacks = 0;
    shield = 1;
    vel = {0, 0};
  }
}

Vec2 Boss::staff_tip() const {
  Vec2 base = pos - Vec2{0, z};
  f32 rot = face * (0.12f + sinf(t * 1.3f) * 0.05f) - cast * face * 0.5f;
  Vec2 hand = base + Vec2{face * 21.0f, -16.0f} * scale;
  return hand + rotate(Vec2{0.4f * face, -31.0f} * scale, rot);
}

BossNews Boss::update(const Hero& hero, Actors& actors, Fx& fx, bool fire_hit, Vec2 flint_pos, f32 dt) {
  BossNews n;
  t += dt;
  state_t += dt;
  prev_pos = pos;
  flash = max(0.0f, flash - dt * 3);
  block_cd = max(0.0f, block_cd - dt);
  beam = max(0.0f, beam - dt);
  if (state == BS_NONE || state == BS_GONE) { orbs.clear(); runes.clear(); return n; }

  if (fighting() && absf(hero.pos.x - pos.x) > 12) face = hero.pos.x < pos.x ? -1.0f : 1.0f;

  // hex orbs fly straight; runes charge, then strike
  for (int i = 0; i < orbs.count;) {
    HexOrb& o = orbs[i];
    o.t += dt;
    o.pos += o.vel * dt;
    if (fx.rng.chance(dt * 14)) fx.spore(o.pos - Vec2{0, 14} + fx.rng.in_circle(3), HEX_HOT.with_alpha(0.8f));
    bool gone = o.t > 4.5f || dist(o.pos, arena) > arena_r + 70;
    if (!gone && !hero.busy() && dist(o.pos, hero.pos) < 13) {
      n.damage++;
      n.from = o.pos;
      gone = true;
      fx.hex_sparks(o.pos - Vec2{0, 14}, 6);
    }
    if (gone) orbs.remove_swap(i); else i++;
  }
  for (int i = 0; i < runes.count;) {
    HexRune& r = runes[i];
    r.t += dt;
    if (r.t >= 1.0f && r.t - dt < 1.0f) {
      n.zap = true;
      fx.ring(r.pos, HEX.with_alpha(0.9f), 2.0f);
      fx.hex_sparks(r.pos + Vec2{0, -20}, 6);
    }
    if (r.t > 1.0f && r.t < 1.3f && dist(hero.pos, r.pos) < 20) { n.damage++; n.from = r.pos; }
    if (r.t > 1.35f) runes.remove_swap(i); else i++;
  }

  const f32 hover_z = 24 + sinf(t * 2.0f) * 3.0f;
  switch (state) {
    case BS_APPARITION:
      alpha = min(0.85f, state_t * 2.0f);
      z = 46 + sinf(t * 2.0f) * 4.0f;
      if (fx.rng.chance(dt * 10)) fx.hex_sparks(centre() + fx.rng.in_circle(20), 1);
      break;
    case BS_ARRIVE:
      alpha = min(1.0f, state_t * 1.5f);
      z = lerp(90.0f, hover_z, smoothstep(0.0f, 1.0f, state_t / 1.2f));
      shield = min(1.0f, state_t);
      break;
    case BS_HOVER: {
      z = damp(z, hover_z, 4, dt);
      shield = min(1.0f, shield + dt * 2);
      cast = max(0.0f, cast - dt * 3);
      Vec2 to = target - pos;
      if (length(to) < 10 || state_t > 2.6f) {
        for (int k = 0; k < 8; k++) {
          target = arena + from_angle(fx.rng.range(0, TAU), fx.rng.range(0.15f, 0.55f) * arena_r) * Vec2{1, 0.8f};
          if (dist(target, hero.pos) > 90) break;
        }
      }
      vel = approach(vel, length(to) > 4 ? normalize(to) * 70.0f : Vec2{0, 0}, 300 * dt);
      f32 wait = hits == 0 ? 2.0f : hits == 1 ? 1.6f : 1.2f;
      if (state_t > wait) {
        if (attacks >= (hits >= 2 ? 3 : 2)) {
          set(BS_TIRED);
          n.tired = true;
          fx.ring(centre(), HEX.with_alpha(0.9f), 3.0f);
          break;
        }
        static const BossState orders[3][3] = {{BS_VOLLEY, BS_RUNES, BS_VOLLEY}, {BS_VOLLEY, BS_SUMMON, BS_RUNES}, {BS_RUNES, BS_VOLLEY, BS_SUMMON}};
        BossState next = orders[min(hits, 2)][next_attack % 3];
        next_attack++;
        if (next == BS_SUMMON && actors.summoned_alive() >= 3) next = BS_VOLLEY;
        set(next);
        volley_left = hits >= 2 ? 3 : 2;
        volley_t = 0;
        runes_left = 4 + hits * 2;
      }
      break;
    }
    case BS_VOLLEY: {
      z = damp(z, hover_z, 4, dt);
      cast = min(1.0f, cast + dt * 4);
      vel = approach(vel, {0, 0}, 300 * dt);
      volley_t -= dt;
      if (fx.rng.chance(dt * 20)) fx.hex_sparks(staff_tip(), 1);
      if (state_t > 0.55f && volley_t <= 0 && volley_left > 0) {
        int count = 3 + hits;
        f32 speed = 115.0f + hits * 22.0f;
        Vec2 from = pos + Vec2{face * 22.0f, 4};
        Vec2 aim = hero.pos + hero.vel * 0.25f - from;
        f32 base = atan2f(aim.y, aim.x);
        f32 spread = 0.22f + hits * 0.03f;
        for (int k = 0; k < count; k++) {
          f32 a = base + (k - (count - 1) * 0.5f) * spread;
          orbs.push({from, from_angle(a, speed), 0});
        }
        fx.ring(staff_tip(), HEX.with_alpha(0.8f), 1.4f);
        n.volley = true;
        volley_left--;
        volley_t = 0.62f;
      }
      if (volley_left == 0 && volley_t <= 0) { attacks++; set(BS_HOVER); }
      break;
    }
    case BS_RUNES: {
      z = damp(z, hover_z, 4, dt);
      cast = min(1.0f, cast + dt * 4);
      vel = approach(vel, {0, 0}, 300 * dt);
      volley_t -= dt;
      if (state_t > 0.4f && volley_t <= 0 && runes_left > 0) {
        Vec2 p = runes_left == 4 + hits * 2 ? hero.pos + hero.vel * 0.45f : hero.pos + fx.rng.in_circle(85);
        Vec2 d = p - arena;
        if (length(d) > arena_r - 20) p = arena + normalize(d) * (arena_r - 20);
        runes.push({p, 0});
        runes_left--;
        volley_t = 0.17f;
      }
      if (runes_left == 0 && state_t > 1.9f) { attacks++; set(BS_HOVER); }
      break;
    }
    case BS_SUMMON:
      z = damp(z, hover_z, 4, dt);
      cast = min(1.0f, cast + dt * 4);
      vel = approach(vel, {0, 0}, 300 * dt);
      if (state_t >= 0.6f && state_t - dt < 0.6f) {
        for (int k = 0; k < 2; k++) {
          f32 a = atan2f(hero.pos.y - arena.y, hero.pos.x - arena.x) + (k ? 0.9f : -0.9f);
          Vec2 p = arena + from_angle(a, arena_r * 0.78f);
          actors.spawn_goblin(p, arena);
          fx.smoke(p + Vec2{0, -10}, 8, 1.1f);
          fx.hex_sparks(p + Vec2{0, -14}, 8);
          fx.ring(p, HEX.with_alpha(0.8f), 2.0f);
        }
        n.summoned = true;
      }
      if (state_t > 1.3f) { attacks++; set(BS_HOVER); }
      break;
    case BS_TIRED:
      z = damp(z, 8, 3, dt);
      shield = max(0.0f, shield - dt * 3);
      cast = max(0.0f, cast - dt * 3);
      vel = approach(vel, {0, 0}, 300 * dt);
      if (fire_hit) {
        set(BS_HURT);
        hits++;
        flash = 1;
        n.hurt = true;
        Vec2 away = normalize(pos - flint_pos + Vec2{0.01f, 0});
        vel = away * 170.0f;
        fx.hex_sparks(centre(), 18);
        fx.ring(centre(), HEX_HOT.with_alpha(0.9f), 2.6f);
      } else if (state_t > 3.8f) {
        set(BS_HOVER);
        attacks = 0;
        n.recovered = true;
      }
      break;
    case BS_HURT:
      z = damp(z, hover_z, 3, dt);
      vel = approach(vel, {0, 0}, 260 * dt);
      if (fx.rng.chance(dt * 12)) fx.smoke(centre(), 1, 0.6f);
      if (state_t > 1.3f) {
        if (hits >= MAX_HITS) { set(BS_DEFEAT); n.defeated = true; orbs.clear(); runes.clear(); }
        else { set(BS_HOVER); attacks = 0; next_attack = 0; }
      }
      break;
    case BS_DEFEAT:
      shield = max(0.0f, shield - dt * 3);
      vel = approach(vel, {0, 0}, 300 * dt);
      if (state_t <= 2.5f) spin += dt * (5.0f + state_t * 10.0f);
      scale = lerp(1.0f, 0.42f, smoothstep(0.4f, 2.2f, state_t));
      z = lerp(hover_z, 0.0f, smoothstep(0.6f, 2.2f, state_t));
      if (fx.rng.chance(dt * 30)) fx.hex_sparks(centre() + fx.rng.in_circle(16), 2);
      if (state_t > 2.5f) {
        spin = 0;
        z = absf(sinf(t * 9)) * 2.0f;  // a tiny, dizzy goblin now, hopping on the spot
        if (may_flee) set(BS_FLEE);
      }
      break;
    case BS_FLEE: {
      // a tiny goblin now, legging it out of the lair
      shield = 0;
      Vec2 away = normalize(pos - hero.pos + Vec2{0.01f, 0.0f});
      target = arena + away * (arena_r + 260);
      vel = approach(vel, normalize(target - pos) * 150.0f, 600 * dt);
      z = absf(sinf(t * 14)) * 3.0f;
      if (dist(pos, arena) > arena_r + 220) set(BS_GONE);
      break;
    }
    default: break;
  }

  if (!vulnerable() && fire_hit && shield > 0.5f && block_cd <= 0 && fighting()) {
    n.blocked = true;
    block_cd = 2.2f;
    fx.sparkle(centre(), 14, HEX_HOT, 26);
    fx.ring(centre(), HEX_HOT.with_alpha(0.9f), 3.0f);
  }
  pos += vel * dt;
  if (fighting()) {
    Vec2 d = pos - arena;
    if (length(d) > arena_r - 30) pos = arena + normalize(d) * (arena_r - 30);
  }
  return n;
}

void Boss::draw(Renderer& r, f32 a, f32 time) const {
  const MalgrinArt& M = S.malgrin;
  // runes and orbs first: they outlive him by a moment
  for (const HexRune& rn : runes) {
    f32 k = rn.t < 1.0f ? 0.3f + 0.7f * rn.t : 1.0f;
    r.draw({.id = S.game.rune, .pos = rn.pos, .layer = LAYER_GROUND, .color = {0.35f, 0.18f, 0.5f, 0.5f}});
    r.draw({.id = S.game.rune, .pos = rn.pos, .layer = LAYER_GLOW, .color = HEX.with_alpha(k), .add = 1});
    if (rn.t > 1.0f) {
      r.draw({.id = S.soft, .pos = rn.pos + Vec2{0, -30}, .layer = LAYER_GLOW, .scale = {0.5f, 2.6f}, .color = {0.85f, 0.6f, 1.0f, 0.75f}, .add = 1});
      r.draw({.id = S.white, .pos = rn.pos + Vec2{0, -40}, .layer = LAYER_GLOW, .scale = {0.4f, 11.0f}, .color = {1, 0.9f, 1, 0.9f}, .add = 1});
      r.light(rn.pos + Vec2{0, -20}, 110, HEX, 0.9f);
    } else if (rn.t > 0.5f) {
      r.light(rn.pos, 50, HEX, 0.4f * k);
    }
  }
  for (int i = 0; i < orbs.count; i++) {
    const HexOrb& o = orbs[i];
    Vec2 p = o.pos + o.vel * (a - 1.0f) * (1.0f / 120.0f);
    Vec2 up = p - Vec2{0, 14};
    f32 pulse = 0.85f + 0.15f * sinf(time * 20 + i);
    r.draw({.id = S.shadow, .pos = p, .layer = LAYER_GROUND, .scale = {0.22f, 0.22f}, .color = {0.1f, 0.0f, 0.15f, 0.3f}});
    r.draw({.id = S.soft, .pos = up, .layer = LAYER_GLOW, .scale = {0.9f * pulse, 0.9f * pulse}, .color = HEX.with_alpha(0.55f), .add = 1});
    r.draw({.id = M.orb, .pos = up, .layer = LAYER_GLOW, .rot = time * 6 + i, .scale = {0.8f, 0.8f}, .color = HEX_HOT, .add = 0.6f});
    if (i % 2 == 0) r.light(up, 54, HEX, 0.55f);
  }

  if (!visible()) return;
  Vec2 p = lerp(prev_pos, pos, a);
  Vec2 base = p - Vec2{0, z};
  const bool ghost = state == BS_APPARITION;
  const f32 sc = scale;
  const f32 sx = face * sc * cosf(spin);
  const f32 sway = sinf(t * 1.5f) * 0.04f;
  const Color tint{1, 1, 1, alpha};
  const f32 add = ghost ? 0.3f : 0.0f;
  const f32 fl = ghost ? 0.15f : flash;
  if (!ghost) r.draw({.id = S.shadow, .pos = p, .layer = LAYER_GROUND, .scale = {1.25f * sc / (1 + z * 0.015f), 1.0f * sc / (1 + z * 0.015f)}, .color = {0.05f, 0.0f, 0.1f, 0.38f * alpha}});
  r.draw({.id = S.soft, .pos = base + Vec2{0, -30} * sc, .layer = LAYER_GLOW, .scale = {2.4f * sc, 2.4f * sc}, .color = HEX.with_alpha((ghost ? 0.3f : 0.16f) * alpha), .add = 1});

  // staff behind or in front, depending on which way he faces (it's held on his facing side)
  f32 srot = face * (0.12f + sinf(t * 1.3f) * 0.05f) - cast * face * 0.5f;
  Vec2 hand = base + rotate(Vec2{face * 21.0f * cosf(spin), -16.0f} * sc, sway);
  Draw staff{.id = M.staff, .pos = hand, .sort_y = p.y + 0.5f, .layer = LAYER_WORLD, .rot = srot + sway, .scale = {face * sc * cosf(spin), sc}, .color = tint, .add = add, .flash = fl};
  r.draw({.id = M.robe, .pos = base, .sort_y = p.y, .layer = LAYER_WORLD, .rot = sway, .scale = {sx, sc}, .color = tint, .add = add, .flash = fl});
  Vec2 neck = base + rotate(Vec2{0, -40} * sc, sway);
  r.draw({.id = M.head, .pos = neck, .sort_y = p.y + 0.2f, .layer = LAYER_WORLD, .rot = sway * 1.5f, .scale = {sx, sc}, .color = tint, .add = add, .flash = fl});
  r.draw(staff);

  // glowing eyes and staff orb, unlit so they shine in the dark
  for (int s = -1; s <= 1; s += 2) {
    Vec2 e = neck + Vec2{s * 3.4f * sx, -11.0f * sc};
    f32 k = 0.75f + 0.25f * sinf(time * 3 + s);
    r.draw({.id = S.mote, .pos = e, .layer = LAYER_GLOW, .scale = {0.32f * sc, 0.32f * sc}, .color = {1.0f, 0.85f, 0.25f, k * alpha}, .add = 1});
  }
  r.light(neck + Vec2{0, -10 * sc}, 46, {1.0f, 0.8f, 0.3f, 1}, 0.25f * alpha);
  Vec2 tip = hand + rotate(Vec2{0.4f * face * cosf(spin), -31.0f} * sc, srot + sway);
  f32 og = (0.35f + cast * 0.5f) * alpha;
  r.draw({.id = S.soft, .pos = tip, .layer = LAYER_GLOW, .scale = {0.7f + cast * 0.4f, 0.7f + cast * 0.4f}, .color = HEX.with_alpha(og), .add = 1});
  r.light(tip, 70 + cast * 50, HEX, 0.5f * og + 0.2f);

  // the shield
  if (shield > 0.02f && state != BS_APPARITION) {
    f32 pulse = 0.85f + 0.15f * sinf(time * 4);
    Vec2 c = centre();
    r.draw({.id = S.soft, .pos = c, .layer = LAYER_GLOW, .scale = {2.7f * sc, 2.7f * sc}, .color = HEX.with_alpha(0.1f * shield), .add = 1});
    r.draw({.id = S.soft_ring, .pos = c, .layer = LAYER_GLOW, .rot = time * 0.5f, .scale = {3.0f * sc * pulse, 3.0f * sc * pulse}, .color = HEX_HOT.with_alpha(0.5f * shield), .add = 0.85f});
  }
  // seeing stars
  if (state == BS_TIRED) {
    Vec2 h = head() + Vec2{0, -4};
    for (int k = 0; k < 3; k++) {
      f32 ang = time * 4.0f + k * TAU / 3;
      r.draw({.id = S.star, .pos = h + Vec2{cosf(ang) * 16, sinf(ang) * 5}, .layer = LAYER_GLOW, .rot = time * 3, .scale = {0.55f, 0.55f}, .color = {1.0f, 0.9f, 0.4f, 1}, .add = 0.6f});
    }
  }
  // the prologue's hex beam
  if (beam > 0) {
    Vec2 from = tip;
    Vec2 d = beam_to - from;
    f32 len = length(d);
    f32 k = min(1.0f, beam * 2.0f);
    r.draw({.id = S.white, .pos = (from + beam_to) * 0.5f, .layer = LAYER_GLOW, .rot = atan2f(d.y, d.x), .scale = {len / 4.0f, 0.9f}, .color = {1.0f, 0.85f, 1.0f, k}, .add = 1});
    r.draw({.id = S.white, .pos = (from + beam_to) * 0.5f, .layer = LAYER_GLOW, .rot = atan2f(d.y, d.x), .scale = {len / 4.0f, 2.6f}, .color = HEX.with_alpha(0.5f * k), .add = 1});
    r.light(beam_to, 90, HEX, 0.8f * k);
  }
}

}  // namespace sky
