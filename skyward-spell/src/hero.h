// hero.h - Mira or Theo: movement feel and paper-doll animation.
#pragma once
#include "anim.h"
#include "art.h"

namespace sky {

struct World;
struct Fx;

/** What the land does to the hero this step: mud slows, wind pushes. */
struct HeroEnv {
  f32 speed_mul = 1;
  Vec2 push;               // velocity added by gusts
  bool can_fall = false;   // Skyreach: walking off the edge drops you
};

struct HeroPose {
  u8 view;
  f32 flip;
  PartXf leg[2], arm[2], torso, head, bag, scarf;
  Vec2 hair_anchor, hair_rest;
  Vec2 scarf_anchor, scarf_rest;
  Vec2 eye_offset;
};

struct Hero {
  // tuning: how movement feels
  static constexpr f32 MAX_SPEED = 128.0f;
  static constexpr f32 ACCEL = 1150.0f;
  static constexpr f32 FRICTION = 1500.0f;
  static constexpr f32 DASH_SPEED = 360.0f;
  static constexpr f32 DASH_TIME = 0.17f;
  static constexpr f32 DASH_COOLDOWN = 0.42f;
  static constexpr f32 RADIUS = 8.0f;
  static constexpr int MAX_HEARTS = 4;
  static constexpr f32 INVULN = 1.3f;

  int look = 0;  // 0 Mira, 1 Theo
  Vec2 pos, prev_pos, vel;
  Vec2 face = {0, 1};
  Vec2 look_dir = {0, 1};
  u8 view = VIEW_FRONT;
  f32 flip = 1;
  f32 phase = 0, walk = 0, t = 0;
  f32 dash_t = 0, dash_cd = 0;
  Vec2 dash_dir;
  f32 blink_t = 2.0f, blink = 0;
  f32 turn = 1;
  f32 land = 0;  // squash after a dash
  f32 step_acc = 0;
  f32 distance = 0;
  Chain hair, scarf;
  bool in_water = false;
  bool in_mud = false;

  // health
  int hearts = MAX_HEARTS;
  f32 invuln = 0;      // can't be hurt again yet
  f32 knock_t = 0;     // knocked back: no control
  f32 hurt_flash = 0;
  f32 faint_t = -1;    // >= 0 while fainting
  f32 fall_t = -1;     // >= 0 while falling off a cliff
  Vec2 safe_pos;       // last solid footing, for falls
  f32 safe_t = 0;

  void reset(Vec2 p, int look_id);
  void update(Vec2 move, bool dash_pressed, const World& w, Fx& fx, f32 dt, const HeroEnv& env = HeroEnv());
  /** Takes a hit from something at `from`. Returns false if the hero shrugged it off (dashing, already hurt). */
  bool hurt(Vec2 from, Fx& fx);
  bool fainted() const { return faint_t >= 0; }
  bool falling() const { return fall_t >= 0; }
  bool busy() const { return fainted() || falling(); }
  HeroPose pose(Vec2 root) const;
  void draw(Renderer& r, Vec2 at) const;
  Vec2 mouth_height() const { return {0, -30}; }
};

}  // namespace sky
