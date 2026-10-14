#include "actors.h"
#include "fx.h"
#include "hero.h"
#include "world.h"

namespace sky {

namespace {
constexpr f32 GOBLIN_RADIUS = 8.0f;
constexpr f32 SIGHT = 140.0f;
constexpr f32 LEASH = 330.0f;
const Color HEX_COLOR{0.72f, 0.42f, 1.0f, 1};
const Color JADE{0.45f, 1.0f, 0.75f, 1};
const Color GOLD{1.0f, 0.85f, 0.45f, 1};

Vec2 approach(Vec2 v, Vec2 target, f32 step) {
  Vec2 d = target - v;
  f32 l = length(d);
  return l <= step ? target : v + d / l * step;
}
}  // namespace

Actor* Actors::add(ActorKind k, Vec2 p) {
  if (count >= CAP) return nullptr;
  Actor* a = &list[count++];
  *a = Actor();
  a->kind = k;
  a->pos = a->prev_pos = a->home = a->target = p;
  return a;
}

void Actors::spawn(const World& w, Rng& rng, int ingredient, bool taken) {
  count = 0;
  last_waystone = 0;
  ingredient_taken = taken;
  const Color amb = w.def->ambient;
  light_k = clamp(1.55f - (amb.r + amb.g + amb.b) / 3.0f, 0.45f, 1.0f);
  for (int i = 0; i < w.waystones.count; i++) {
    Actor* a = add(AK_WAYSTONE, w.waystones[i]);
    if (a) { a->index = i; a->state = i == 0 ? 1 : 0; }
  }
  if (ingredient >= 0) {
    Actor* a = add(AK_SHRINE, w.shrine + Vec2{0, -10});
    if (a) { a->index = ingredient; a->state = taken ? 1 : 0; }
  }
  for (Vec2 c : w.camps) {
    for (int g = 0; g < w.def->goblins; g++) {
      Actor* a = add(AK_GOBLIN, c + from_angle(g * TAU / w.def->goblins + rng.range(-0.4f, 0.4f), rng.range(34, 56)));
      if (!a) break;
      a->home = c;
      a->timer = rng.range(0.2f, 2.0f);
      a->phase = rng.range(0, 1);
    }
  }
  for (const Spot& s : w.spots) {
    ActorKind k = AK_HEART;
    if (s.kind == SPOT_HAZARD) {
      switch (w.def->hazard) {
        case HZ_MUD: k = AK_MUD; break;
        case HZ_VENT: k = AK_VENT; break;
        case HZ_SNAPJAW: k = AK_SNAPJAW; break;
        case HZ_RUNE: k = AK_RUNE; break;
        default: continue;
      }
    }
    Actor* a = add(k, s.pos);
    if (!a) break;
    a->timer = rng.range(0.5f, 3.5f);
    a->flip = rng.chance(0.5f) ? -1.0f : 1.0f;
    a->t = rng.range(0, 10);
  }
}

void Actors::calm() {
  for (int i = 0; i < count; i++) {
    Actor& a = list[i];
    if (a.kind != AK_GOBLIN || !a.alive || a.state == GS_POOF) continue;
    a.pos = a.prev_pos = a.home + from_angle(i * 2.4f, 40);
    a.state = GS_PATROL;
    a.state_t = 0;
    a.vel = {0, 0};
  }
}

f32 Actors::speed_mul_at(Vec2 p) const {
  f32 m = 1;
  for (int i = 0; i < count; i++) {
    const Actor& a = list[i];
    if (a.kind != AK_MUD) continue;
    Vec2 d = (p - a.pos) / Vec2{20, 8};
    if (length2(d) < 1) m = min(m, 0.5f);
  }
  return m;
}

int Actors::chasing_goblin(Vec2 near, f32 range) const {
  int best = -1;
  f32 bd = range * range;
  for (int i = 0; i < count; i++) {
    const Actor& a = list[i];
    if (a.kind != AK_GOBLIN || !a.alive) continue;
    if (a.state == GS_PATROL || a.state == GS_RETURN || a.state == GS_POOF) continue;
    f32 d = length2(a.pos - near);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

int Actors::find_target(Vec2 from, Vec2 dir, f32 range) const {
  int best = -1;
  f32 best_score = 1e9f;
  for (int i = 0; i < count; i++) {
    if (!burnable(i)) continue;
    const Actor& a = list[i];
    Vec2 d = a.pos - from;
    f32 l = length(d);
    if (l > range) continue;
    f32 facing = dot(d / max(l, 1.0f), dir);
    f32 score = l * (1.6f - facing * 0.6f);
    if (a.kind == AK_GOBLIN && a.state != GS_PATROL && a.state != GS_RETURN) score *= 0.6f;
    if (score < best_score) { best_score = score; best = i; }
  }
  return best;
}

int Actors::heat_cone(Vec2 mouth, Vec2 dir, f32 range, f32 dt, Fx& fx) {
  int caught = 0;
  for (int i = 0; i < count; i++) {
    if (!burnable(i)) continue;
    Actor& a = list[i];
    Vec2 d = a.pos + Vec2{0, -10} - mouth;
    f32 l = length(d);
    if (l > range + 12) continue;
    if (dot(d / max(l, 1.0f), dir) < 0.7f && l > 18) continue;
    a.heat += dt * 2.8f;
    a.flash = 0.6f;
    if (a.heat >= 1.0f) {
      caught++;
      if (a.kind == AK_GOBLIN) {
        a.state = GS_POOF;
        a.state_t = 0;
      } else {
        a.alive = false;
        fx.smoke(a.pos + Vec2{0, -8}, 6, 0.9f);
        fx.embers(a.pos + Vec2{0, -8}, 8);
      }
    }
  }
  return caught;
}

constexpr int SUMMONED = 77;

Actor* Actors::spawn_goblin(Vec2 at, Vec2 home) {
  // reuse a slot from a goblin that's already gone, so long fights never run out of room
  Actor* a = nullptr;
  for (int i = 0; i < count && !a; i++)
    if (list[i].kind == AK_GOBLIN && !list[i].alive && list[i].index == SUMMONED) a = &list[i];
  if (a) { *a = Actor(); a->pos = a->prev_pos = a->home = a->target = at; } else a = add(AK_GOBLIN, at);
  if (!a) return nullptr;
  a->home = home;
  a->index = SUMMONED;
  a->state = GS_ALERT;
  a->flash = 1;
  return a;
}

int Actors::summoned_alive() const {
  int n = 0;
  for (int i = 0; i < count; i++)
    if (list[i].kind == AK_GOBLIN && list[i].alive && list[i].index == SUMMONED) n++;
  return n;
}

void Actors::dismiss_summoned(Fx& fx) {
  for (int i = 0; i < count; i++) {
    Actor& a = list[i];
    if (a.kind != AK_GOBLIN || !a.alive || a.index != SUMMONED) continue;
    a.alive = false;
    fx.smoke(a.pos + Vec2{0, -10}, 6, 1.0f);
  }
}

int Actors::goblins_left() const {
  int n = 0;
  for (int i = 0; i < count; i++)
    if (list[i].kind == AK_GOBLIN && list[i].alive) n++;
  return n;
}

ActorNews Actors::update(const Hero& hero, World& w, Fx& fx, f32 dt) {
  ActorNews news;
  for (int i = 0; i < count; i++) {
    Actor& a = list[i];
    if (!a.alive) continue;
    a.t += dt;
    a.state_t += dt;
    a.prev_pos = a.pos;
    a.flash = max(0.0f, a.flash - dt * 4);
    switch (a.kind) {
      case AK_GOBLIN: update_goblin(a, hero, w, fx, dt, news); break;
      case AK_HEART:
        if (dist(hero.pos, a.pos) < 17) {
          a.alive = false;
          news.hearts++;
          news.where = a.pos;
          fx.sparkle(a.pos + Vec2{0, -8}, 14, {1.0f, 0.55f, 0.65f, 1}, 10);
          fx.ring(a.pos + Vec2{0, -8}, {1.0f, 0.6f, 0.7f, 0.8f}, 1.6f);
        }
        break;
      case AK_WAYSTONE:
        if (a.state == 0 && dist(hero.pos, a.pos + Vec2{0, 8}) < 50) {
          a.state = 1;
          a.state_t = 0;
          news.waystone = a.index;
          last_waystone = a.index;
          fx.sparkle(a.pos + Vec2{0, -16}, 22, JADE, 12);
          fx.ring(a.pos + Vec2{0, -14}, JADE.with_alpha(0.8f), 2.6f);
        } else if (a.state == 1 && dist(hero.pos, a.pos + Vec2{0, 8}) < 50) {
          last_waystone = max(last_waystone, a.index);
        }
        if (a.state == 1 && fx.rng.chance(dt * 2.5f)) fx.sparkle(a.pos + Vec2{fx.rng.range(-5, 5), -14 + fx.rng.range(-8, 8)}, 1, JADE, 2);
        break;
      case AK_SHRINE:
        if (a.state == 0) {
          if (fx.rng.chance(dt * 9)) fx.sparkle(a.pos + Vec2{fx.rng.range(-12, 12), -34 + fx.rng.range(-12, 12)}, 1, GOLD, 3);
          if (dist(hero.pos, a.pos + Vec2{0, 10}) < 30) {
            a.state = 1;
            a.state_t = 0;
            ingredient_taken = true;
            news.ingredient = true;
            news.where = a.pos;
            fx.sparkle(a.pos + Vec2{0, -34}, 40, GOLD, 16);
            fx.ring(a.pos + Vec2{0, -34}, GOLD.with_alpha(0.9f), 3.6f);
            fx.ring(a.pos + Vec2{0, -34}, {1, 1, 1, 0.8f}, 2.2f);
          }
        }
        break;
      default: update_hazard(a, hero, w, fx, dt, news); break;
    }
  }
  // goblins keep a little space between each other
  for (int i = 0; i < count; i++) {
    Actor& a = list[i];
    if (a.kind != AK_GOBLIN || !a.alive) continue;
    for (int j = i + 1; j < count; j++) {
      Actor& b = list[j];
      if (b.kind != AK_GOBLIN || !b.alive) continue;
      Vec2 d = a.pos - b.pos;
      f32 l2 = length2(d);
      if (l2 < 18 * 18 && l2 > 1e-4f) {
        f32 l = sqrtf(l2);
        Vec2 push = d / l * (18 - l) * 0.5f;
        a.pos += push;
        b.pos -= push;
      }
    }
  }
  return news;
}

void Actors::update_goblin(Actor& a, const Hero& hero, World& w, Fx& fx, f32 dt, ActorNews& news) {
  Vec2 to_hero = hero.pos - a.pos;
  f32 dh = length(to_hero);
  bool hero_in_reach = dh < SIGHT && dist(hero.pos, a.home) < LEASH && !hero.fainted();
  Vec2 want{0, 0};
  f32 speed = 0;
  switch (a.state) {
    case GS_PATROL: {
      a.timer -= dt;
      if (a.timer <= 0 && dist(a.pos, a.target) < 6) {
        a.target = a.home + fx.rng.in_circle(70);
        a.timer = fx.rng.range(1.0f, 2.6f);
      }
      if (a.timer <= 0 || dist(a.pos, a.target) > 6) { want = a.target - a.pos; speed = 38; }
      if (a.timer > 0 && dist(a.pos, a.target) <= 6) speed = 0;
      if (hero_in_reach) {
        a.state = GS_ALERT;
        a.state_t = 0;
        news.goblin_spotted = true;
        news.where = a.pos;
      }
      break;
    }
    case GS_ALERT:
      a.face = normalize(to_hero);
      if (a.state_t > 0.5f) { a.state = GS_CHASE; a.state_t = 0; }
      break;
    case GS_CHASE:
      want = to_hero;
      speed = 94;
      if (dh < 30) { a.state = GS_WINDUP; a.state_t = 0; }
      else if (!hero_in_reach && (dh > 260 || dist(hero.pos, a.home) > LEASH)) { a.state = GS_RETURN; a.state_t = 0; }
      break;
    case GS_WINDUP:
      a.face = normalize(to_hero);
      if (a.state_t > 0.38f) a.flash = 0.5f;
      if (a.state_t > 0.46f) { a.state = GS_SWING; a.state_t = 0; a.hit_done = false; }
      break;
    case GS_SWING:
      if (!a.hit_done) {
        a.hit_done = true;
        Vec2 club = a.pos + a.face * 14 + Vec2{0, -4};
        if (dist(club, hero.pos) < 24) {
          news.damage++;
          news.damage_from = a.pos;
        }
        fx.dust(a.pos + a.face * 18, a.face, 4, 0.8f);
      }
      if (a.state_t > 0.14f) { a.state = GS_RECOVER; a.state_t = 0; }
      break;
    case GS_RECOVER:
      if (a.state_t > 0.7f) { a.state = hero_in_reach ? GS_CHASE : GS_RETURN; a.state_t = 0; }
      break;
    case GS_RETURN:
      want = a.home - a.pos;
      speed = 60;
      if (length(want) < 20) { a.state = GS_PATROL; a.state_t = 0; a.target = a.pos; }
      if (hero_in_reach && a.state_t > 1.5f) { a.state = GS_ALERT; a.state_t = 0; }
      break;
    case GS_POOF:
      a.flash = 1;
      if (fx.rng.chance(dt * 40)) fx.flame(a.pos + Vec2{fx.rng.range(-6, 6), -6}, {0, -1}, 50, 0.6f);
      if (a.state_t > 0.5f) {
        a.alive = false;
        news.goblins_poofed++;
        news.where = a.pos;
        fx.smoke(a.pos + Vec2{0, -10}, 10, 1.2f);
        fx.hex_sparks(a.pos + Vec2{0, -14}, 10);
        fx.ring(a.pos + Vec2{0, -12}, HEX_COLOR.with_alpha(0.8f), 2.4f);
        if (fx.rng.chance(a.index == SUMMONED ? 0.5f : 0.3f)) {
          Actor* h = add(AK_HEART, a.pos);
          if (h) h->t = 0;
        }
      }
      break;
  }

  // movement with a little momentum
  Vec2 target_v = speed > 0 && length2(want) > 1 ? normalize(want) * speed : Vec2{0, 0};
  a.vel = approach(a.vel, target_v, 700 * dt);
  if (a.state == GS_WINDUP || a.state == GS_SWING || a.state == GS_RECOVER || a.state == GS_ALERT || a.state == GS_POOF) a.vel = approach(a.vel, {0, 0}, 1400 * dt);
  a.pos = w.resolve(a.pos + a.vel * dt, GOBLIN_RADIUS);
  f32 sp = length(a.vel);
  if (sp > 10) a.face = a.vel / sp;
  choose_view(a.face, a.view, a.flip);
  a.walk = damp(a.walk, saturate(sp / 80.0f), 10, dt);
  a.phase += sp * dt / 22.0f;
  if (a.heat > 0 && a.state != GS_POOF) a.heat = max(0.0f, a.heat - dt * 0.5f);
}

void Actors::update_hazard(Actor& a, const Hero& hero, World& w, Fx& fx, f32 dt, ActorNews& news) {
  f32 dh = dist(hero.pos, a.pos);
  if (dh < 120) news.hazard_near = true;
  switch (a.kind) {
    case AK_VENT: {
      a.timer -= dt;
      if (a.state == 0 && a.timer <= 0) { a.state = 1; a.timer = 0.9f; }
      else if (a.state == 1) {
        if (fx.rng.chance(dt * 14)) fx.smoke(a.pos + fx.rng.in_circle(5), 1, 0.6f);
        if (fx.rng.chance(dt * 10)) fx.embers(a.pos, 1);
        if (a.timer <= 0) { a.state = 2; a.timer = 1.0f; if (dh < 360) { news.vents++; news.sound_at = a.pos; } }
      } else if (a.state == 2) {
        for (int k = 0; k < 2; k++)
          if (fx.rng.chance(dt * 60)) fx.flame(a.pos + fx.rng.in_circle(4), {0, -1}, 150, 0.18f);
        if (dh < 20) { news.damage++; news.damage_from = a.pos; }
        if (a.timer <= 0) { a.state = 0; a.timer = fx.rng.range(2.2f, 3.8f); fx.smoke(a.pos, 4, 1.0f); }
      }
      break;
    }
    case AK_RUNE: {
      a.timer -= dt;
      if (a.state == 0 && a.timer <= 0) { a.state = 1; a.timer = 0.9f; }
      else if (a.state == 1) {
        if (fx.rng.chance(dt * 16)) fx.hex_sparks(a.pos + fx.rng.in_circle(10) * Vec2{1, 0.45f}, 1);
        if (a.timer <= 0) {
          a.state = 2;
          a.timer = 0.35f;
          fx.ring(a.pos, HEX_COLOR.with_alpha(0.9f), 2.0f);
          fx.hex_sparks(a.pos + Vec2{0, -20}, 8);
          if (dh < 360) { news.zaps++; news.sound_at = a.pos; }
        }
      } else if (a.state == 2) {
        if (dh < 18) { news.damage++; news.damage_from = a.pos; }
        if (a.timer <= 0) { a.state = 0; a.timer = fx.rng.range(2.0f, 3.2f); }
      }
      break;
    }
    case AK_SNAPJAW: {
      // states: 0 waiting, 1 opening (warning), 2 snap, 3 resting
      a.timer -= dt;
      if (a.state == 0 && dh < 42) { a.state = 1; a.timer = 0.32f; }
      else if (a.state == 1 && a.timer <= 0) {
        a.state = 2;
        a.timer = 0.12f;
        news.snaps++;
        news.sound_at = a.pos;
        if (dh < 32) { news.damage++; news.damage_from = a.pos; }
      } else if (a.state == 2 && a.timer <= 0) { a.state = 3; a.timer = 1.2f; }
      else if (a.state == 3 && a.timer <= 0) a.state = 0;
      if (a.heat > 0) a.heat = max(0.0f, a.heat - dt * 0.4f);
      break;
    }
    default: break;
  }
}

void Actors::draw_goblin(Renderer& r, const Actor& a, Vec2 at, f32 time) const {
  const GoblinArt& G = S.game.goblin;
  f32 F = a.view == VIEW_SIDE ? a.flip : 1.0f;
  f32 s = a.walk;
  f32 ph = a.phase * TAU;
  f32 sp = sinf(ph);
  f32 bob = -absf(sp) * 1.6f * s;
  f32 shrink = a.state == GS_POOF ? max(0.0f, 1.0f - a.state_t * 1.6f) : 1.0f;
  f32 hop = a.state == GS_ALERT ? -sinf(min(1.0f, a.state_t * 3.0f) * PI) * 6.0f : 0.0f;
  f32 squash = a.state == GS_WINDUP ? 1.0f - min(0.08f, a.state_t * 0.2f) : 1.0f;
  Vec2 base = at + Vec2{0, bob + hop};
  f32 flash = max(a.flash, a.state == GS_POOF ? 0.6f : 0.0f);
  Color tint{1, 1, 1, 1};
  if (a.heat > 0) tint = mix(tint, Color{1.0f, 0.6f, 0.4f, 1}, saturate(a.heat) * 0.6f);
  r.draw({.id = S.shadow, .pos = at + Vec2{1, 0}, .layer = LAYER_GROUND, .scale = {0.58f * shrink, 0.58f * shrink}, .color = {0.02f, 0.06f, 0.04f, 0.32f}});
  if (shrink <= 0.01f) return;

  // feet
  for (int k = 0; k < 2; k++) {
    f32 side = k == 0 ? -1.0f : 1.0f;
    f32 lift = max(0.0f, side * sp) * 2.4f * s;
    Vec2 fp = a.view == VIEW_SIDE ? Vec2{(k == 0 ? -2.5f : 3.0f) * F + sp * side * 3.0f * s * F, -lift} : Vec2{side * 4.2f, -lift};
    r.draw({.id = G.foot, .pos = at + fp * shrink, .sort_y = at.y, .layer = LAYER_WORLD, .scale = {F * shrink, shrink}, .color = tint, .flash = flash});
  }
  // the club: held low while walking, raised on the windup, smashed down on the swing
  f32 club_rot = 0.35f + sp * 0.12f * s;
  if (a.state == GS_WINDUP) club_rot = lerp(0.35f, -1.9f, saturate(a.state_t / 0.3f));
  else if (a.state == GS_SWING) club_rot = lerp(-1.9f, 1.9f, saturate(a.state_t / 0.1f));
  else if (a.state == GS_RECOVER) club_rot = lerp(1.9f, 0.35f, saturate(a.state_t / 0.5f));
  Vec2 hand = a.view == VIEW_SIDE ? Vec2{3.5f * F, -9.0f} : a.view == VIEW_FRONT ? Vec2{9.0f, -9.0f} : Vec2{-9.0f, -9.0f};
  f32 cr = a.view == VIEW_SIDE ? club_rot * F : a.view == VIEW_FRONT ? club_rot : -club_rot;
  Draw club{.id = G.club, .pos = base + hand * shrink, .sort_y = at.y + (a.view == VIEW_BACK ? -0.5f : 0.5f), .layer = LAYER_WORLD, .rot = cr, .scale = {shrink, shrink}, .color = tint, .flash = flash};
  if (a.view == VIEW_BACK) r.draw(club);
  r.draw({.id = G.body[a.view], .pos = base, .sort_y = at.y, .layer = LAYER_WORLD, .rot = a.view == VIEW_SIDE ? sp * 0.04f * s * F : sp * 0.03f * s,
          .scale = {F * shrink * (2.0f - squash), shrink * squash}, .color = tint, .flash = flash});
  if (a.view != VIEW_BACK) r.draw(club);
  if (a.state == GS_ALERT || (a.state == GS_CHASE && a.state_t < 0.3f)) {
    f32 k = a.state == GS_ALERT ? min(1.0f, a.state_t * 6.0f) : 1.0f - a.state_t / 0.3f;
    r.draw({.id = G.alert, .pos = base + Vec2{0, -46}, .layer = LAYER_GLOW, .rot = sinf(time * 20) * 0.1f, .scale = {k, k}, .color = {1, 1, 1, k}});
  }
}

void Actors::draw(Renderer& r, f32 alpha, f32 time) const {
  for (int i = 0; i < count; i++) {
    const Actor& a = list[i];
    Vec2 at = lerp(a.prev_pos, a.pos, alpha);
    switch (a.kind) {
      case AK_GOBLIN:
        if (a.alive) draw_goblin(r, a, at, time);
        break;
      case AK_HEART: {
        if (!a.alive) break;
        f32 z = 6 + sinf(a.t * 3.0f) * 2.5f;
        f32 pop = min(1.0f, a.t * 4.0f);
        r.draw({.id = S.shadow, .pos = at, .layer = LAYER_GROUND, .scale = {0.32f, 0.32f}, .color = {0.1f, 0.02f, 0.04f, 0.3f}});
        r.draw({.id = S.soft, .pos = at + Vec2{0, -z - 7}, .layer = LAYER_GLOW, .scale = {0.75f, 0.75f}, .color = {1.0f, 0.4f, 0.5f, 0.35f + 0.1f * sinf(a.t * 4)}, .add = 1});
        r.draw({.id = S.game.heart, .pos = at + Vec2{0, -z}, .sort_y = at.y, .layer = LAYER_WORLD, .rot = sinf(a.t * 2.2f) * 0.12f, .scale = {pop, pop}});
        r.light(at + Vec2{0, -z - 6}, 40, {1.0f, 0.45f, 0.55f, 1}, 0.35f * light_k);
        break;
      }
      case AK_WAYSTONE: {
        r.draw({.id = S.game.waystone, .pos = at, .sort_y = at.y, .layer = LAYER_WORLD});
        if (a.state == 1) {
          f32 k = min(1.0f, a.state_t * 2.0f) * (0.8f + 0.2f * sinf(time * 2.5f + i));
          r.draw({.id = S.game.waystone_lit, .pos = at, .layer = LAYER_GLOW, .color = JADE.with_alpha(k), .add = 1});
          r.light(at + Vec2{0, -16}, 74, JADE, 0.5f * k * light_k);
        }
        break;
      }
      case AK_SHRINE: {
        r.draw({.id = S.game.pedestal, .pos = at + Vec2{0, 10}, .sort_y = at.y + 10, .layer = LAYER_WORLD});
        if (a.state == 1 && a.state_t > 0.2f) break;
        f32 bob = sinf(time * 2.0f) * 2.5f;
        f32 k = a.state == 1 ? 1.0f - a.state_t / 0.2f : 1.0f;
        Vec2 p = at + Vec2{0, -14 + bob - (a.state == 1 ? a.state_t * 60 : 0)};
        r.draw({.id = S.soft, .pos = p + Vec2{0, -16}, .layer = LAYER_GLOW, .scale = {1.4f, 1.4f}, .color = GOLD.with_alpha(0.32f * k * light_k), .add = 1});
        r.draw({.id = S.game.ingredient[a.index], .pos = p, .sort_y = at.y + 11, .layer = LAYER_WORLD, .rot = sinf(time * 1.3f) * 0.06f, .scale = {k, k}});
        r.light(p + Vec2{0, -16}, 130, GOLD, 0.6f * k * light_k);
        break;
      }
      case AK_MUD:
        r.draw({.id = S.game.mud, .pos = at, .layer = LAYER_GROUND, .scale = {a.flip, 1}});
        break;
      case AK_VENT: {
        f32 glow = a.state == 0 ? 0.25f : a.state == 1 ? 0.4f + 0.4f * (1.0f - a.timer / 0.9f) : 1.0f;
        Vec2 shake = a.state == 1 ? Vec2{sinf(time * 70) * 0.6f, 0} : Vec2{0, 0};
        r.draw({.id = S.game.vent, .pos = at + shake, .layer = LAYER_GROUND});
        r.draw({.id = S.soft, .pos = at + Vec2{0, -2}, .layer = LAYER_GLOW, .scale = {0.9f, 0.45f}, .color = {1.0f, 0.55f, 0.2f, 0.35f * glow}, .add = 1});
        if (a.state == 2) {
          r.draw({.id = S.soft, .pos = at + Vec2{0, -26}, .layer = LAYER_GLOW, .scale = {0.8f, 2.2f}, .color = {1.0f, 0.6f, 0.25f, 0.4f}, .add = 1});
          r.light(at + Vec2{0, -20}, 130, {1.0f, 0.55f, 0.2f, 1}, 0.9f);
        } else if (a.state == 1) {
          r.light(at, 60, {1.0f, 0.5f, 0.2f, 1}, 0.4f * glow);
        }
        break;
      }
      case AK_RUNE: {
        f32 k = a.state == 0 ? 0.3f + 0.08f * sinf(time * 2 + i) : a.state == 1 ? 0.45f + 0.55f * (1.0f - a.timer / 0.9f) : 1.0f;
        r.draw({.id = S.game.rune, .pos = at, .layer = LAYER_GROUND, .rot = 0, .color = {0.35f, 0.18f, 0.5f, 0.55f}});
        r.draw({.id = S.game.rune, .pos = at, .layer = LAYER_GLOW, .color = HEX_COLOR.with_alpha(k), .add = 1});
        if (a.state == 2) {
          r.draw({.id = S.soft, .pos = at + Vec2{0, -30}, .layer = LAYER_GLOW, .scale = {0.5f, 2.6f}, .color = {0.85f, 0.6f, 1.0f, 0.75f}, .add = 1});
          r.draw({.id = S.white, .pos = at + Vec2{0, -40}, .layer = LAYER_GLOW, .scale = {0.4f, 11.0f}, .color = {1, 0.9f, 1, 0.9f}, .add = 1});
          r.light(at + Vec2{0, -20}, 120, HEX_COLOR, 1.0f);
        } else if (a.state == 1) {
          r.light(at, 50, HEX_COLOR, 0.5f * k);
        }
        break;
      }
      case AK_SNAPJAW: {
        if (!a.alive) {
          r.draw({.id = S.ra.blocker_burnt, .pos = at, .sort_y = at.y, .layer = LAYER_WORLD, .scale = {0.45f, 0.45f}});
          break;
        }
        bool open = a.state == 1 || a.state == 2;
        f32 sway = sinf(a.t * 1.7f) * 0.06f;
        f32 snap = a.state == 2 ? 1.12f : 1.0f;
        Color tint{1, 1, 1, 1};
        if (a.heat > 0) tint = mix(tint, Color{1.0f, 0.6f, 0.4f, 1}, saturate(a.heat) * 0.6f);
        r.draw({.id = S.shadow, .pos = at, .layer = LAYER_GROUND, .scale = {0.5f, 0.5f}, .color = {0.02f, 0.06f, 0.04f, 0.3f}});
        r.draw({.id = S.game.snapjaw[open ? 1 : 0], .pos = at, .sort_y = at.y, .layer = LAYER_WORLD, .rot = sway, .scale = {a.flip * snap, snap}, .color = tint, .flash = a.flash});
        break;
      }
    }
  }
}

}  // namespace sky
