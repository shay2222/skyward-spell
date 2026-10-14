#include "hero.h"
#include "fx.h"
#include "world.h"

namespace sky {

namespace {
const f32 MIRA_HAIR[3] = {6.2f, 6.0f, 7.0f};
const f32 THEO_TUFT[2] = {4.6f, 4.0f};
const f32 SCARF_LEN[3] = {6.4f, 6.0f, 5.8f};

Vec2 approach(Vec2 v, Vec2 target, f32 step) {
  Vec2 d = target - v;
  f32 l = length(d);
  return l <= step ? target : v + d / l * step;
}
}  // namespace

void Hero::reset(Vec2 p, int look_id) {
  look = look_id;
  pos = prev_pos = p;
  vel = {0, 0};
  face = {0, 1};
  view = VIEW_FRONT;
  flip = 1;
  walk = phase = 0;
  dash_t = dash_cd = 0;
  HeroPose ps = pose(pos);
  if (look == 0) hair.init(4, MIRA_HAIR, ps.hair_anchor, ps.hair_rest);
  else hair.init(3, THEO_TUFT, ps.hair_anchor, ps.hair_rest);
  scarf.init(4, SCARF_LEN, ps.scarf_anchor, ps.scarf_rest);
  invuln = knock_t = hurt_flash = 0;
  faint_t = fall_t = -1;
  safe_pos = p;
  safe_t = 0;
}

bool Hero::hurt(Vec2 from, Fx& fx) {
  if (invuln > 0 || dash_t > 0 || busy()) return false;
  hearts = max(0, hearts - 1);
  invuln = INVULN;
  hurt_flash = 1;
  knock_t = 0.16f;
  Vec2 d = pos - from;
  d = length2(d) > 1e-3f ? normalize(d) : -face;
  vel = d * 270.0f;
  fx.sparkle(pos + Vec2{0, -18}, 10, {1.0f, 0.55f, 0.55f, 1}, 8);
  if (hearts == 0) faint_t = 0;
  return true;
}

void Hero::update(Vec2 move, bool dash_pressed, const World& w, Fx& fx, f32 dt, const HeroEnv& env) {
  t += dt;
  invuln = max(0.0f, invuln - dt);
  hurt_flash = max(0.0f, hurt_flash - dt * 3);
  if (busy()) {
    // fainting (sinking down) or falling (dropping away below the cliff)
    if (fainted()) faint_t += dt; else fall_t += dt;
    prev_pos = pos;
    vel = damp(vel, {0, 0}, fainted() ? 8.0f : 3.0f, dt);
    pos = fainted() ? w.resolve(pos + vel * dt, RADIUS) : pos + vel * dt;
    walk = damp(walk, 0, 8, dt);
    HeroPose ps = pose(pos);
    hair.step(ps.hair_anchor, ps.hair_rest, 9.0f, 420.0f, 0.9f, dt);
    scarf.step(ps.scarf_anchor, ps.scarf_rest, 7.0f, 520.0f, 0.86f, dt);
    return;
  }
  if (knock_t > 0) {
    knock_t -= dt;
    move = {0, 0};
    dash_pressed = false;
  }
  f32 ml = length(move);
  if (ml > 1) { move = move / ml; ml = 1; }

  dash_cd = max(0.0f, dash_cd - dt);
  if (dash_pressed && dash_cd <= 0 && dash_t <= 0) {
    dash_t = DASH_TIME;
    dash_cd = DASH_COOLDOWN;
    dash_dir = ml > 0.2f ? move : face;
    fx.dust(pos, -dash_dir, 7, 1.2f);
    turn = 0.6f;
  }

  if (dash_t > 0) {
    f32 k = dash_t / DASH_TIME;
    vel = dash_dir * DASH_SPEED * (0.55f + 0.45f * k);
    dash_t -= dt;
    if (dash_t <= 0) { land = 1; vel = dash_dir * MAX_SPEED; }
  } else {
    Vec2 target = move * MAX_SPEED * env.speed_mul;
    vel = approach(vel, target, (ml > 0.05f ? ACCEL : FRICTION) * dt);
  }

  prev_pos = pos;
  Vec2 next = pos + (vel + env.push) * dt;
  bool hit = false;
  pos = w.resolve(next, RADIUS, &hit, !env.can_fall);
  if (hit) vel = (pos - prev_pos - env.push * dt) / dt * 0.98f + vel * 0.02f;  // slide along what we bumped

  // over the edge?
  if (env.can_fall && dash_t <= 0 && w.sample(w.water, pos) > 0.62f) {
    fall_t = 0;
    vel *= 0.35f;
    fx.dust(pos, -face, 6, 1.0f);
    return;
  }
  safe_t += dt;
  if (safe_t > 0.25f) {
    safe_t = 0;
    bool firm = w.solid_at(pos) < 0.05f;
    for (int k = 0; k < 8 && firm; k++) firm = w.solid_at(pos + from_angle(k * (TAU / 8), 44)) < 0.2f;
    if (firm) safe_pos = pos;
  }

  f32 speed = length(vel);
  distance += speed * dt;
  walk = damp(walk, saturate(speed / MAX_SPEED), 12.0f, dt);
  if (speed > 6) {
    phase += speed * dt / 30.0f;
  } else {
    // settle feet into a standing pose
    f32 target = roundf(phase * 2.0f) * 0.5f;
    phase = damp(phase, target, 10.0f, dt);
  }
  if (speed > 12) {
    face = normalize(vel);
    choose_view(face, view, flip);
  }
  look_dir = damp(look_dir, speed > 12 ? face : Vec2{look_dir.x * 0.5f, 0.6f}, 6.0f, dt);
  turn = min(1.0f, turn + dt * 7);
  land = max(0.0f, land - dt * 5);

  // little dust puffs on footfalls when running
  if (speed > 70 && dash_t <= 0) {
    step_acc += speed * dt;
    if (step_acc > 15) { step_acc = 0; fx.dust(pos + Vec2{fx.rng.range(-3, 3), 0}, -face, 1, 0.6f); }
  }
  Liquid lq = w.def->liquid;
  bool wet = (lq == LQ_WATER || lq == LQ_BOG || lq == LQ_GOO) && w.sample(w.water, pos) > 0.3f;
  if (wet && speed > 40 && fx.rng.chance(dt * 12)) fx.splash(pos, 2);
  in_water = wet;
  in_mud = env.speed_mul < 0.99f;
  if (in_mud && speed > 20 && fx.rng.chance(dt * 10)) fx.mud(pos, 2);

  blink_t -= dt;
  if (blink_t <= 0) { blink = 0.13f; blink_t = 2.2f + fx.rng.range(0, 3.2f); }
  blink = max(0.0f, blink - dt);

  // hair and scarf follow the body with momentum
  HeroPose ps = pose(pos);
  if (look == 0) hair.step(ps.hair_anchor, ps.hair_rest, 9.0f, 420.0f, 0.9f, dt);
  else hair.step(ps.hair_anchor, ps.hair_rest, 26.0f, -60.0f, 0.86f, dt);
  scarf.step(ps.scarf_anchor, ps.scarf_rest, 7.0f, 520.0f, 0.86f, dt);
}

HeroPose Hero::pose(Vec2 root) const {
  HeroPose o;
  o.view = view;
  const f32 F = view == VIEW_SIDE ? flip : 1.0f;
  o.flip = F;
  const f32 s = walk;
  const f32 ph = phase * TAU;
  const f32 sp = sinf(ph), cp = cosf(ph);
  const f32 breath = sinf(t * 2.3f) * (1 - s);
  f32 bob = -absf(sp) * 1.7f * s + breath * 0.35f;
  const bool dashing = dash_t > 0;
  // squash and stretch: stretch during a dash, squash on landing and when turning
  f32 sx = 1, sy = 1;
  if (dashing) { sx = 0.9f; sy = 1.08f; }
  sx *= lerp(0.86f, 1.0f, turn);
  sy *= 1.0f - land * 0.08f;
  sx *= 1.0f + land * 0.06f;
  auto P = [&](f32 x, f32 y) { return root + Vec2{x * F * sx, y * sy}; };
  const Color far_tint{0.78f, 0.78f, 0.84f, 1};

  if (view != VIEW_SIDE) {
    f32 lift_l = max(0.0f, sp) * 2.6f * s, lift_r = max(0.0f, -sp) * 2.6f * s;
    if (view == VIEW_BACK) { f32 t2 = lift_l; lift_l = lift_r; lift_r = t2; }
    o.leg[0] = {P(-3.6f, -12 + bob * 0.25f - lift_l), 0, {sx, sy * (1 - lift_l * 0.05f)}};
    o.leg[1] = {P(3.6f, -12 + bob * 0.25f - lift_r), 0, {-sx, sy * (1 - lift_r * 0.05f)}};
    o.torso = {P(0, -12 + bob), sp * 0.02f * s, {sx * (1 + breath * 0.01f), sy * (1 - breath * 0.01f)}};
    f32 swing = sp * 0.14f * s;
    o.arm[0] = {P(-7.3f, -21.3f + bob), 0.1f + swing + (dashing ? 0.35f : 0), {sx, sy * (1 - max(0.0f, -sp) * 0.12f * s)}};
    o.arm[1] = {P(7.3f, -21.3f + bob), -0.1f + swing - (dashing ? 0.35f : 0), {-sx, sy * (1 - max(0.0f, sp) * 0.12f * s)}};
    o.bag = {P(view == VIEW_FRONT ? 6.8f : -6.8f, -10.8f + bob), sinf(ph + 1.2f) * 0.14f * s, {sx, sy}};
    o.scarf = {P(0, -24 + bob * 1.05f), 0, {sx, sy}};
    o.head = {P(0, -24 + bob * 1.2f), sp * 0.025f * s, {sx, sy}};
  } else {
    f32 swing = sp * 0.6f * s;
    f32 lift_n = max(0.0f, cp) * 1.3f * s, lift_f = max(0.0f, -cp) * 1.3f * s;
    o.leg[0] = {P(-0.9f, -12 + bob * 0.25f - lift_f), swing * F, {F * sx, sy}, far_tint};
    o.leg[1] = {P(0.2f, -12 + bob * 0.25f - lift_n), -swing * F, {F * sx, sy}};
    f32 lean = (0.05f * s + (dashing ? 0.16f : 0.0f)) * F;
    o.torso = {P(0, -12 + bob), lean, {F * sx * (1 + breath * 0.01f), sy * (1 - breath * 0.01f)}};
    o.arm[0] = {P(-0.6f, -21.2f + bob), -swing * 1.1f * F - (dashing ? 0.6f * F : 0), {sx, sy}, far_tint};
    o.arm[1] = {P(0.4f, -21.2f + bob), swing * 1.1f * F - (dashing ? 0.6f * F : 0), {sx, sy}};
    o.scarf = {P(0, -24 + bob * 1.05f), lean * 0.5f, {F * sx, sy}};
    o.head = {P(0.4f, -24 + bob * 1.2f), lean * 0.4f + 0.02f * sp * s * F, {F * sx, sy}};
  }

  // where the ponytail / cowlick and the scarf ends hang from
  if (look == 0) {
    Vec2 a = view == VIEW_FRONT ? Vec2{0, -21.6f} : view == VIEW_BACK ? Vec2{0, -19.6f} : Vec2{-10.0f, -19.6f};
    o.hair_anchor = o.head.apply(a);
    o.hair_rest = view == VIEW_SIDE ? normalize(Vec2{-0.55f * F, 1}) : Vec2{0, 1};
  } else {
    Vec2 a = view == VIEW_SIDE ? Vec2{-1.0f, -27.4f} : Vec2{1.6f, -27.2f};
    o.hair_anchor = o.head.apply(a);
    o.hair_rest = normalize(Vec2{(view == VIEW_SIDE ? -0.45f : 0.25f) * F, -1});
  }
  Vec2 sa = view == VIEW_FRONT ? Vec2{-4.4f, 2.6f} : view == VIEW_BACK ? Vec2{2.4f, 1.8f} : Vec2{-3.2f, 1.6f};
  o.scarf_anchor = o.scarf.apply(sa);
  o.scarf_rest = view == VIEW_SIDE ? normalize(Vec2{-0.45f * F, 1.0f}) : normalize(Vec2{view == VIEW_FRONT ? 0.12f : -0.1f, 1});
  o.eye_offset = view == VIEW_FRONT ? Vec2{look_dir.x * 1.1f, max(-0.4f, look_dir.y * 0.4f)} : Vec2{0, 0};
  return o;
}

void Hero::draw(Renderer& r, Vec2 at) const {
  const HeroArt& A = S.hero[look];
  HeroPose o = pose(at);
  // falling sinks the hero away below the cliff; fainting fades them out before the waystone brings them back
  f32 alpha = 1.0f;
  Vec2 drop{0, 0};
  if (falling()) { f32 k = saturate(fall_t / 0.7f); alpha = 1.0f - k; drop = {0, k * k * 46.0f}; }
  if (fainted()) alpha = 1.0f - saturate((faint_t - 0.6f) / 0.8f);
  f32 flash = hurt_flash > 0 ? hurt_flash * 0.8f : (invuln > 0 && fmodf(invuln, 0.2f) > 0.1f ? 0.35f : 0.0f);
  if (alpha < 1.0f || flash > 0 || drop.y > 0) {
    PartXf* parts[] = {&o.leg[0], &o.leg[1], &o.arm[0], &o.arm[1], &o.torso, &o.head, &o.bag, &o.scarf};
    for (PartXf* p : parts) { p->tint.a *= alpha; p->pos += drop; }
  }
  const f32 sy = at.y;
  Vec2 chain_off = at - pos + drop;  // chains are simulated at the physics position
  const u16 hair_ids[3] = {A.tail_seg[0], A.tail_seg[1], A.tail_seg[2]};
  const u16 tuft_ids[2] = {A.tail_seg[0], A.tail_seg[2]};
  const u16 scarf_ids[3] = {A.scarf_seg[0], A.scarf_seg[0], A.scarf_seg[1]};
  const bool girl = look == 0;
  const u16 eyes = blink > 0 ? A.blink[o.view] : A.eyes[o.view];
  PartXf eye_x = o.head;
  eye_x.pos += rotate(o.eye_offset, o.head.rot);
  const Color chain_tint{1, 1, 1, alpha};
  const usize first_draw = (usize)r.draw_count;

  if (!falling()) r.draw({.id = S.shadow, .pos = at + Vec2{1, 0}, .layer = LAYER_GROUND, .scale = {0.62f, 0.62f}, .color = {0.02f, 0.08f, 0.06f, 0.34f * alpha}});

  if (o.view == VIEW_FRONT) {
    if (girl) hair.draw(r, hair_ids, 3, sy, chain_off, chain_tint);
    draw_part(r, A.hair_back_f, o.head, sy);
    draw_part(r, A.leg[0], o.leg[0], sy);
    draw_part(r, A.leg[0], o.leg[1], sy);
    draw_part(r, A.torso[0], o.torso, sy);
    draw_part(r, A.bag, o.bag, sy);
    draw_part(r, A.arm, o.arm[0], sy);
    draw_part(r, A.arm, o.arm[1], sy);
    draw_part(r, A.scarf_wrap[0], o.scarf, sy);
    draw_part(r, A.head[0], o.head, sy);
    draw_part(r, eyes, eye_x, sy);
    draw_part(r, A.hair_front[0], o.head, sy);
    scarf.draw(r, scarf_ids, 3, sy, chain_off, chain_tint);
    if (!girl) hair.draw(r, tuft_ids, 2, sy, chain_off, chain_tint);
  } else if (o.view == VIEW_BACK) {
    draw_part(r, A.leg[1], o.leg[0], sy);
    draw_part(r, A.leg[1], o.leg[1], sy);
    draw_part(r, A.torso[1], o.torso, sy);
    PartXf bag = o.bag;
    draw_part(r, A.bag, bag, sy);
    draw_part(r, A.arm, o.arm[0], sy);
    draw_part(r, A.arm, o.arm[1], sy);
    draw_part(r, A.scarf_wrap[1], o.scarf, sy);
    scarf.draw(r, scarf_ids, 3, sy, chain_off, chain_tint);
    draw_part(r, A.head[1], o.head, sy);
    draw_part(r, A.hair_front[1], o.head, sy);
    hair.draw(r, girl ? hair_ids : tuft_ids, girl ? 3 : 2, sy, chain_off, chain_tint);
  } else {
    if (girl) hair.draw(r, hair_ids, 3, sy, chain_off, chain_tint);
    scarf.draw(r, scarf_ids, 3, sy, chain_off, chain_tint);
    draw_part(r, A.arm, o.arm[0], sy);
    draw_part(r, A.leg[2], o.leg[0], sy);
    draw_part(r, A.leg[2], o.leg[1], sy);
    draw_part(r, A.torso[2], o.torso, sy);
    draw_part(r, A.scarf_wrap[2], o.scarf, sy);
    draw_part(r, A.head[2], o.head, sy);
    draw_part(r, eyes, eye_x, sy);
    draw_part(r, A.hair_front[2], o.head, sy);
    draw_part(r, A.arm, o.arm[1], sy);
    if (!girl) hair.draw(r, tuft_ids, 2, sy, chain_off, chain_tint);
  }
  if (flash > 0)
    for (usize i = first_draw + (falling() ? 0 : 1); i < (usize)r.draw_count; i++) r.draws[i].flash = flash;
}

}  // namespace sky
