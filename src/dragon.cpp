#include "dragon.h"
#include "actors.h"
#include "fx.h"
#include "hero.h"
#include "world.h"

namespace sky {

namespace {
const f32 TAIL_LEN[4] = {5.6f, 5.4f, 5.0f, 4.0f};
const Color HEX_COLOR{0.72f, 0.42f, 1.0f, 1};
}  // namespace

void Flint::reset(Vec2 p) {
  pos = prev_pos = p;
  vel = {0, 0};
  state = FL_FOLLOW;
  state_t = 0;
  z = vz = 0;
  trail.clear();
  FlintPose ps = pose(pos);
  tail.init(4, TAIL_LEN, ps.tail_anchor, ps.tail_rest);
}

Vec2 Flint::follow_target(const Hero& hero) const {
  if (flying) return hero.pos + Vec2{(pos.x < hero.pos.x ? -1.0f : 1.0f) * 34.0f, 8.0f};
  if (hero_still > 0.5f || trail.count < 2) {
    // sit beside the hero, slightly behind, on whichever side Flint is already on
    Vec2 f = hero.face;
    if (absf(f.x) > absf(f.y)) return hero.pos + Vec2{-signf(f.x) * 34.0f, 5.0f};  // facing sideways: sit behind
    f32 sx = pos.x < hero.pos.x ? -1.0f : 1.0f;                                      // stay on whichever side Flint is on
    return hero.pos + Vec2{34.0f * sx, f.y > 0 ? -3.0f : 4.0f};                       // facing up or down: sit beside
  }
  // walk back along the hero's trail until FOLLOW_DIST is used up
  f32 left = FOLLOW_DIST;
  Vec2 at = hero.pos;
  for (int i = trail.count - 1; i >= 0; i--) {
    f32 d = dist(at, trail[i]);
    if (d >= left) return at + (trail[i] - at) * (left / d);
    left -= d;
    at = trail[i];
  }
  return at;
}

bool Flint::pick_target(const World& w, const Actors& actors, Vec2 from, Vec2 dir, f32 range) {
  int p = w.find_burnable(from, dir, range);
  int a = actors.find_target(from, dir, range);
  if (p < 0 && a < 0) return false;
  bool use_actor = a >= 0;
  if (p >= 0 && a >= 0) {
    // a chasing goblin wins; otherwise whichever is nearer
    bool chasing = actors.chasing_goblin(actors.list[a].pos, 1) == a;
    use_actor = chasing || dist(actors.list[a].pos, from) < dist(w.props[p].pos, from);
  }
  target = use_actor ? a : p;
  target_actor = use_actor;
  return true;
}

bool Flint::target_ok(const World& w, const Actors& actors) const {
  if (target_ext) return ext_active;
  if (target < 0) return false;
  if (target_actor) return actors.burnable(target);
  return !(w.props[target].flags & (PF_BURNT | PF_BURNING));
}

Vec2 Flint::target_pos(const World& w, const Actors& actors) const {
  if (target_ext) return ext_pos;
  return target_actor ? actors.list[target].pos : w.props[target].pos;
}

void Flint::move_toward(Vec2 target, f32 max_speed, const World& w, f32 dt) {
  Vec2 d = target - pos;
  f32 l = length(d);
  Vec2 want = l > 4 ? d / l * min(max_speed, l * (flying ? 3.5f : 5.0f)) : Vec2{0, 0};
  Vec2 dv = want - vel;
  f32 acc = (flying ? 700.0f : 900.0f) * dt;
  f32 dl = length(dv);
  vel = dl <= acc ? want : vel + dv / dl * acc;
  prev_pos = pos;
  if (flying) {  // nothing to bump into up here
    pos += vel * dt;
    pos.x = clamp(pos.x, TS, w.px_w() - TS);
    pos.y = clamp(pos.y, TS * 2, w.px_h() - TS);
    return;
  }
  bool hit = false;
  pos = w.resolve(pos + vel * dt, RADIUS, &hit);
  if (hit) vel = (pos - prev_pos) / dt;
}

FlintNews Flint::update(const Hero& hero, World& w, Actors& actors, Fx& fx, bool fire_pressed, f32 dt) {
  FlintNews news;
  t += dt;
  state_t += dt;
  auto_cd = max(0.0f, auto_cd - dt);

  // remember where the hero walks, so Flint can follow the same safe route
  if (trail.count == 0 || dist(trail.back(), hero.pos) > 7) {
    if (trail.full()) {
      for (int i = 1; i < trail.count; i++) trail[i - 1] = trail[i];
      trail.count--;
    }
    trail.push(hero.pos);
  }
  f32 hero_speed = length(hero.vel);
  hero_still = hero_speed < 8 ? hero_still + dt : 0;

  // a goblin is after the hero: Flint jumps in by himself
  if (scripted) fire_pressed = false;
  bool calm = state == FL_FOLLOW || state == FL_IDLE || state == FL_SAD || state == FL_HAPPY;
  if (!scripted && !fire_pressed && calm && auto_cd <= 0 && (z <= 0 || flying)) {
    int g = actors.chasing_goblin(hero.pos, 150);
    if (g >= 0 && dist(actors.list[g].pos, pos) < 240) {
      target = g;
      target_actor = true;
      target_ext = false;
      Vec2 from = normalize(pos - actors.list[g].pos);
      if (length2(from) < 0.01f) from = -hero.face;
      stand = w.resolve(actors.list[g].pos + from * 46.0f, RADIUS);
      set_state(FL_APPROACH);
      auto_cd = 2.6f;
      news.auto_help = true;
    }
  }

  // fire! find something to burn near the hero, or just puff in the hero's direction
  if (fire_pressed && state != FL_BREATHE && state != FL_APPROACH) {
    target_ext = ext_active && dist(ext_pos, pos) < 520;
    if (target_ext || pick_target(w, actors, hero.pos, hero.face, 190)) {
      if (target_ext) target = -1;
      else target_ext = false;
      Vec2 tp = target_pos(w, actors);
      Vec2 from = normalize(pos - tp);
      if (length2(from) < 0.01f) from = -hero.face;
      stand = target_ext ? tp + Vec2{from.x * 80.0f, 40.0f + z} : w.resolve(tp + Vec2{0, -6} + from * 52.0f, RADIUS);
      set_state(FL_APPROACH);
      auto_cd = 1.5f;
    } else {
      // nothing to burn: puff the way the hero faces, but never through the hero
      target = -1;
      target_ext = false;
      breath_dir = hero.face;
      Vec2 to_hero = hero.pos - pos;
      f32 lh = length(to_hero);
      if (lh < 80 && dot(to_hero / max(lh, 1.0f), breath_dir) > 0.6f) breath_dir = rotate(breath_dir, side * 0.9f);
      set_state(FL_BREATHE);
      news.no_target = true;
    }
  }

  if (flying) {
    z = damp(z, 30.0f + sinf(t * 2.2f) * 3.0f, 3.0f, dt);
    vz = 0;
  } else if (can_fly && state != FL_TRY_FLY) {
    z = damp(z, 0, 6, dt);
  }
  if (scripted) {
    // a story scene is moving Flint: go where we're told and wait there
    move_toward(script_to, SPEED * (flying ? 1.4f : 1.0f), w, dt);
    if (state != FL_IDLE && state != FL_HAPPY) set_state(FL_IDLE);
    if (state == FL_HAPPY && state_t > 1.0f) set_state(FL_IDLE);
    mouth_open = false;
  } else switch (state) {
    case FL_FOLLOW:
    case FL_IDLE: {
      Vec2 tgt = follow_target(hero);
      f32 d = dist(tgt, pos);
      f32 far = dist(hero.pos, pos);
      f32 speed = far > 130 ? SPEED * 1.6f : SPEED;
      move_toward(tgt, speed, w, dt);
      // catch up with a magic hop if stuck or left far behind
      stuck_t = (!flying && d > 40 && length(vel) < 15) ? stuck_t + dt : 0;
      if (far > 360 || stuck_t > 1.2f) {
        fx.sparkle(pos + Vec2{0, -12}, 8, {0.75f, 1.0f, 0.85f, 1});
        pos = prev_pos = w.resolve(hero.pos - hero.face * 30 + perp(hero.face) * 10.0f, RADIUS);
        tail.shift(pos - prev_pos);
        trail.clear();
        fx.sparkle(pos + Vec2{0, -12}, 10, {0.75f, 1.0f, 0.85f, 1});
        stuck_t = 0;
      }
      if (d < 10 && hero_still > 0.5f) {
        if (state != FL_IDLE) set_state(FL_IDLE);
        idle_t += dt;
        fly_cd -= dt;
        if (!can_fly && fly_cd <= 0 && idle_t > 2.5f) {
          set_state(FL_TRY_FLY);
          news.tried_to_fly = true;
          fly_cd = fx.rng.range(16, 26);
        }
      } else {
        if (state != FL_FOLLOW) set_state(FL_FOLLOW);
        idle_t = 0;
      }
      // pick which side to sit on
      side = cross(hero.face, pos - hero.pos) > 0 ? 1 : -1;
      break;
    }
    case FL_TRY_FLY: {
      vel = damp(vel, {0, 0}, 10, dt);
      prev_pos = pos;
      f32 k = state_t;
      flap = min(1.0f, k * 3);
      if (k < 1.15f) {
        z = damp(z, 9.0f + sinf(k * 16) * 1.5f, 5, dt);
        hex_glow = 0.5f + k * 0.7f;
        if (fx.rng.chance(dt * 10)) fx.hex_sparks(pos + Vec2{fx.rng.range(-10, 10), -20 - z}, 1);
      } else {
        if (hex_glow > 0.9f) {  // the hex snaps tight
          fx.ring(pos + Vec2{0, -16 - z}, HEX_COLOR, 2.2f);
          fx.hex_sparks(pos + Vec2{0, -18 - z}, 7);
        }
        hex_glow = 0.45f;
        flap = 0;
        vz -= 520 * dt;
        z += vz * dt;
        if (z <= 0) {
          z = 0;
          vz = 0;
          fx.dust(pos, {0, 0}, 8, 1.1f);
          set_state(FL_SAD);
          news.landed_sad = true;
        }
      }
      break;
    }
    case FL_SAD: {
      move_toward(pos, 0, w, dt);
      if (state_t > 1.8f || hero_speed > 20) set_state(FL_FOLLOW);
      break;
    }
    case FL_APPROACH: {
      if (!target_ok(w, actors)) { set_state(FL_FOLLOW); target = -1; break; }
      Vec2 tp = target_pos(w, actors);
      if (target_actor) {  // goblins move: keep re-aiming
        Vec2 from = normalize(pos - tp);
        if (length2(from) < 0.01f) from = -hero.face;
        stand = flying ? tp + from * 60.0f : w.resolve(tp + from * 46.0f, RADIUS);
      } else if (target_ext) {
        f32 sx = pos.x < tp.x ? -1.0f : 1.0f;
        stand = tp + Vec2{sx * 86.0f, 34.0f + z};
      }
      move_toward(stand, SPEED * (flying ? 2.4f : 1.5f), w, dt);
      Vec2 to = tp + Vec2{0, target_ext ? 0.0f : -8.0f} - (pos - Vec2{0, z});
      if (dist(pos, stand) < 10 || state_t > 2.5f || length(to) < fire_range() * 0.75f) {
        breath_dir = normalize(to);
        set_state(FL_BREATHE);
      }
      break;
    }
    case FL_BREATHE: {
      vel = damp(vel, {0, 0}, 12, dt);
      prev_pos = pos;
      pos = flying ? pos + vel * dt - breath_dir * 6 * dt : w.resolve(pos + vel * dt - breath_dir * 6 * dt, RADIUS);
      if (target_ok(w, actors)) breath_dir = normalize(target_pos(w, actors) + Vec2{0, target_ext ? 0.0f : -8.0f} - (pos - Vec2{0, z + 10}));
      if (state_t < 0.08f) news.started_fire = state_t <= dt;
      if (state_t <= dt) scored = false;
      mouth_open = state_t > 0.12f && state_t < 0.95f;
      if (mouth_open) {
        FlintPose ps = pose(pos);
        const f32 range = fire_range();
        if (fx.rng.chance(dt * (fire_power ? 95.0f : 70.0f))) fx.flame(ps.mouth, breath_dir, fire_power ? 290.0f : 230.0f, fire_power ? 0.26f : 0.2f);
        // anything flammable in the cone heats up
        Rect area{ps.mouth.x - range - 20, ps.mouth.y - range - 20, ps.mouth.x + range + 20, ps.mouth.y + range + 20};
        w.each_in(area, [&](Prop& p, int) {
          if (!(p.flags & PF_BURNABLE) || (p.flags & (PF_BURNT | PF_BURNING))) return;
          Vec2 d = p.pos + Vec2{0, -8} - ps.mouth;
          f32 l = length(d);
          if (l > range + 14) return;
          if (dot(d / max(l, 1.0f), breath_dir) < 0.72f && l > 18) return;
          p.heat += dt * (fire_power ? 3.2f : 2.2f);
          if (p.heat >= 1.0f) {
            p.flags |= PF_BURNING;
            p.burn_t = 0;
            news.caught_fire++;
          }
        });
        news.goblins_hit += actors.heat_cone(ps.mouth, breath_dir, range, dt, fx);
        if (ext_active) {
          Vec2 d = ext_pos - ps.mouth;
          f32 l = length(d);
          if (l < range + 30 && (dot(d / max(l, 1.0f), breath_dir) > 0.75f || l < 30)) news.ext_hit = true;
        }
        if (news.caught_fire || news.goblins_hit || (news.ext_hit && ext_open)) scored = true;
      }
      if (state_t > 1.05f) {
        mouth_open = false;
        set_state(scored ? FL_HAPPY : FL_FOLLOW);
        target = -1;
        target_ext = false;
      }
      break;
    }
    case FL_HAPPY: {
      move_toward(flying ? follow_target(hero) : pos, flying ? SPEED : 0.0f, w, dt);
      wag = 1;
      if (!flying) {
        if (z <= 0 && state_t < 0.9f) vz = 95;
        vz -= 520 * dt;
        z = max(0.0f, z + vz * dt);
      }
      if (state_t > 1.0f) { if (!flying) { z = 0; vz = 0; } set_state(FL_FOLLOW); }
      break;
    }
  }

  // which way Flint faces
  f32 speed = length(vel);
  walk = damp(walk, flying ? 0.0f : saturate(speed / SPEED), 10, dt);
  phase += speed * dt / 15.0f;
  if (speed < 5) phase = damp(phase, roundf(phase * 2) * 0.5f, 8, dt);
  if (state == FL_BREATHE) face = breath_dir;
  else if (speed > 14) face = normalize(vel);
  else if ((face_camera || scripted) && state == FL_IDLE && speed < 14) face = Vec2{0, 1};
  else if (state == FL_IDLE || state == FL_SAD) face = normalize(damp(face, normalize(hero.pos - pos + Vec2{0, 0.01f}), 2.0f, dt));
  choose_view(face, view, flip);

  // idle: look around at the world
  look_t -= dt;
  if (look_t <= 0) { look_t = fx.rng.range(1.2f, 3.5f); look = rotate(face, fx.rng.range(-1.2f, 1.2f)); }
  wag = max(0.0f, wag - dt * 0.8f);
  if (state != FL_TRY_FLY) flap = damp(flap, 0, 6, dt);
  wing_rest = state == FL_SAD ? 1 : 0;

  blink_t -= dt;
  if (blink_t <= 0) { blink = 0.12f; blink_t = 1.8f + fx.rng.range(0, 3.0f); }
  blink = max(0.0f, blink - dt);

  FlintPose ps = pose(pos);
  Vec2 rest = ps.tail_rest;
  if (wag > 0 || state == FL_IDLE) rest = normalize(rest + perp(rest) * sinf(t * (wag > 0 ? 16.0f : 4.0f)) * (wag > 0 ? 0.8f : 0.25f));
  tail.step(ps.tail_anchor, rest, 10.0f, 200.0f, 0.88f, dt);
  return news;
}

FlintPose Flint::pose(Vec2 root) const {
  FlintPose o;
  o.view = view;
  const f32 F = view == VIEW_SIDE ? flip : 1.0f;
  o.flip = F;
  const f32 s = walk;
  const f32 ph = phase * TAU, sp = sinf(ph), cp = cosf(ph);
  const f32 breath = sinf(t * 2.0f) * (1 - s);
  f32 bob = -absf(sp) * 1.4f * s + breath * 0.3f;
  root.y -= z;  // height in the air lifts every part together, so nothing comes apart in flight
  f32 sad = state == FL_SAD ? 1.0f : 0.0f;
  bool breathing = state == FL_BREATHE && mouth_open;
  auto P = [&](f32 x, f32 y) { return root + Vec2{x * F, y}; };
  const Color far_tint{0.74f, 0.78f, 0.8f, 1};
  f32 flap_a = flying ? sinf(t * 9.5f) * 0.6f + 0.15f : flap > 0 ? sinf(t * 26) * 0.75f * flap : 0.0f;
  f32 wing_droop = sad * 0.5f;
  f32 head_down = sad * 1.6f;

  if (view != VIEW_SIDE) {
    f32 roll = sp * 0.07f * s;
    f32 lift_l = max(0.0f, sp) * 2.0f * s, lift_r = max(0.0f, -sp) * 2.0f * s;
    o.foot[0] = {P(-5.6f, -1.6f - lift_l), 0, {1, 1}};
    o.foot[1] = {P(5.6f, -1.6f - lift_r), 0, {-1, 1}};
    o.body = {P(0, bob * 0.6f), roll, {1 + breath * 0.012f, 1 - breath * 0.012f}};
    o.arm[0] = {P(-8.0f, -11.5f + bob), 0.15f + sp * 0.2f * s, {1, 1}};
    o.arm[1] = {P(8.0f, -11.5f + bob), -0.15f + sp * 0.2f * s, {-1, 1}};
    f32 wr = 0.15f + flap_a + wing_droop;
    o.wing[0] = {P(-7.2f, -15.0f + bob), -wr * -1.0f, {-1, 1}};
    o.wing[1] = {P(7.2f, -15.0f + bob), -wr, {1, 1}};
    if (view == VIEW_BACK) { o.wing[0].scale = {-1.15f, 1.15f}; o.wing[1].scale = {1.15f, 1.15f}; }
    f32 lx = view == VIEW_FRONT ? look.x * (state == FL_IDLE ? 1.4f : 0.6f) : 0.0f;
    o.head = {P(lx, -16.0f + bob * 1.2f + head_down), roll * 0.6f + lx * 0.03f, {1, 1}};
    o.tail_anchor = view == VIEW_FRONT ? P(4.0f, -5.0f + bob * 0.6f) : P(0, -4.0f + bob * 0.6f);
    o.tail_rest = view == VIEW_FRONT ? normalize(Vec2{0.9f, 0.4f}) : Vec2{0, 1};
    o.mouth = o.head.apply(view == VIEW_FRONT ? Vec2{0, -1.3f} : Vec2{0, -4});
    o.eye_offset = view == VIEW_FRONT ? Vec2{look.x * 0.9f, sad ? 0.8f : max(-0.3f, look.y * 0.3f)} : Vec2{0, 0};
  } else {
    f32 swing = sp * 0.55f * s;
    o.body = {P(0, bob * 0.6f), sp * 0.03f * s * F, {F * (1 + breath * 0.012f), 1 - breath * 0.012f}};
    o.leg_far[0] = {P(-4.4f, -7.0f + bob * 0.4f), -swing * F, {F, 1}, far_tint};
    o.leg_far[1] = {P(6.0f, -9.0f + bob * 0.5f), swing * F, {F, 1}, far_tint};
    o.leg_near[0] = {P(-6.2f, -6.6f + bob * 0.4f - max(0.0f, cp) * 1.2f * s), swing * F, {F, 1}};
    o.leg_near[1] = {P(4.6f, -8.6f + bob * 0.5f - max(0.0f, -cp) * 1.2f * s), -swing * F, {F, 1}};
    o.wing[0] = {P(-0.8f, -15.4f + bob * 0.8f), (-1.15f - flap_a * 1.2f + wing_droop) * F, {F, 1}};
    o.wing[1] = o.wing[0];
    f32 nod = breathing ? 0.0f : sinf(ph * 2) * 0.03f * s;
    o.head = {P(6.6f, -15.4f + bob * 1.1f + head_down), (nod + (breathing ? -0.08f : 0.0f) + sad * 0.25f) * F, {F, 1}};
    o.tail_anchor = P(-10.5f, -6.5f + bob * 0.5f);
    o.tail_rest = normalize(Vec2{-1.0f * F, 0.35f});
    o.mouth = o.head.apply({10.4f, -2.2f});
    o.eye_offset = {0, sad ? 0.6f : 0.0f};
  }
  return o;
}

void Flint::draw(Renderer& r, Vec2 at, f32 time) const {
  const DragonArt& D = S.flint;
  FlintPose o = pose(at);
  const f32 sy = at.y;
  Vec2 chain_off = at - pos;
  const u16 tail_ids[3] = {D.tail_seg[0], D.tail_seg[1], D.tail_seg[2]};
  const bool sad = state == FL_SAD;
  u16 eyes = blink > 0 ? D.blink[o.view] : sad ? D.sad[o.view] : D.eyes[o.view];
  u16 head = mouth_open ? D.head_open[o.view] : D.head[o.view];
  PartXf eye_x = o.head;
  eye_x.pos += rotate(o.eye_offset, o.head.rot);
  f32 shadow_k = 1.0f / (1.0f + z * 0.06f);
  r.draw({.id = S.shadow, .pos = at + Vec2{0.5f, 0}, .layer = LAYER_GROUND, .scale = {0.66f * shadow_k, 0.66f * shadow_k}, .color = {0.02f, 0.08f, 0.06f, 0.34f * shadow_k}});

  // the hex bands glow on top of the wings, brighter when Flint strains against them
  auto hex = [&](const PartXf& wing, f32 extra) {
    Vec2 band_local = Vec2{4.6f, -2.8f} * (o.view == VIEW_BACK ? 1.25f : o.view == VIEW_SIDE ? 1.1f : 1.0f);
    Vec2 p = wing.apply(band_local);
    f32 pulse = 0.55f + 0.45f * sinf(time * 3.0f + extra);
    f32 k = saturate(hex_glow * pulse * hex_strength);
    r.draw({.id = D.hex_band, .pos = p, .layer = LAYER_GLOW, .rot = wing.rot + 0.75f * (wing.scale.x < 0 ? -1.0f : 1.0f), .scale = {0.75f, 0.75f}, .color = {HEX_COLOR.r, HEX_COLOR.g, HEX_COLOR.b, k}, .add = 0.85f});
    if (hex_glow > 0.8f) r.light(p, 30, HEX_COLOR, 0.25f * (hex_glow - 0.6f));
  };

  if (o.view == VIEW_FRONT) {
    tail.draw(r, tail_ids, 3, sy, chain_off);
    draw_part(r, D.wing_front, o.wing[0], sy);
    draw_part(r, D.wing_front, o.wing[1], sy);
    draw_part(r, D.foot, o.foot[0], sy);
    draw_part(r, D.foot, o.foot[1], sy);
    draw_part(r, D.body[0], o.body, sy);
    draw_part(r, D.arm_front, o.arm[0], sy);
    draw_part(r, D.arm_front, o.arm[1], sy);
    draw_part(r, head, o.head, sy);
    draw_part(r, eyes, eye_x, sy);
    hex(o.wing[0], 0);
    hex(o.wing[1], 1.3f);
  } else if (o.view == VIEW_BACK) {
    draw_part(r, D.foot, o.foot[0], sy);
    draw_part(r, D.foot, o.foot[1], sy);
    draw_part(r, D.body[1], o.body, sy);
    draw_part(r, D.head[1], o.head, sy);
    draw_part(r, D.wing_back, o.wing[0], sy);
    draw_part(r, D.wing_back, o.wing[1], sy);
    tail.draw(r, tail_ids, 3, sy, chain_off);
    hex(o.wing[0], 0);
    hex(o.wing[1], 1.3f);
  } else {
    tail.draw(r, tail_ids, 3, sy, chain_off);
    draw_part(r, D.leg_side_hind, o.leg_far[0], sy);
    draw_part(r, D.leg_side_front, o.leg_far[1], sy);
    draw_part(r, D.body[2], o.body, sy);
    draw_part(r, D.leg_side_hind, o.leg_near[0], sy);
    draw_part(r, D.leg_side_front, o.leg_near[1], sy);
    draw_part(r, D.wing_side, o.wing[0], sy);
    draw_part(r, head, o.head, sy);
    draw_part(r, eyes, eye_x, sy);
    hex(o.wing[0], 0);
  }
  if (mouth_open) r.light(o.mouth, fire_power ? 110.0f : 80.0f, {1.0f, 0.6f, 0.25f, 1}, 0.55f);
}

}  // namespace sky
