// dragon.h - Flint, the companion: follows, idles, tries to fly, and breathes fire to help.
#pragma once
#include "anim.h"
#include "art.h"

namespace sky {

struct World;
struct Fx;
struct Hero;
struct Actors;

enum FlintState : u8 {
  FL_FOLLOW,      // trotting after the hero along their trail
  FL_IDLE,        // sitting nearby, looking around
  FL_TRY_FLY,     // flapping hard against the hex
  FL_SAD,         // the hex wins again
  FL_APPROACH,    // walking up to something to burn it
  FL_BREATHE,     // fire!
  FL_HAPPY,       // a proud little hop
};

/** Things Flint did this frame that the game may want to react to (speech, sound, UI). */
struct FlintNews {
  bool tried_to_fly = false;
  bool landed_sad = false;
  bool started_fire = false;
  int caught_fire = 0;     // blockers that caught fire this frame
  int goblins_hit = 0;     // goblins and snapjaws sent packing
  bool no_target = false;  // breathed fire at nothing
  bool auto_help = false;  // Flint jumped in on his own
  bool ext_hit = false;    // his fire reached the outside target (Malgrin)
};

struct FlintPose {
  u8 view;
  f32 flip;
  PartXf body, head, wing[2], foot[2], arm[2], leg_far[2], leg_near[2];
  Vec2 tail_anchor, tail_rest;
  Vec2 mouth;
  Vec2 eye_offset;
};

struct Flint {
  static constexpr f32 RADIUS = 7.0f;
  static constexpr f32 SPEED = 150.0f;
  static constexpr f32 FOLLOW_DIST = 38.0f;
  static constexpr f32 FIRE_RANGE = 92.0f;

  Vec2 pos, prev_pos, vel;
  Vec2 face = {0, 1};
  u8 view = VIEW_FRONT;
  f32 flip = 1;
  f32 phase = 0, walk = 0, t = 0;
  FlintState state = FL_FOLLOW;
  f32 state_t = 0;
  f32 z = 0, vz = 0;
  f32 blink_t = 1.5f, blink = 0;
  Vec2 look = {0, 1};
  f32 idle_t = 0, look_t = 0, fly_cd = 9.0f;
  f32 hero_still = 0;
  f32 stuck_t = 0;
  f32 flap = 0, wing_rest = 0;
  f32 hex_glow = 0.45f;
  f32 wag = 0;
  bool mouth_open = false;
  Vec2 breath_dir = {1, 0};
  int target = -1;         // prop index, or actor index when target_actor
  bool target_actor = false;
  bool target_ext = false; // aiming at the outside target below

  // set by the game
  bool can_fly = false;    // after the Skyward Spell breaks the hex
  bool flying = false;
  bool scripted = false;   // in a story scene: go to script_to and wait
  Vec2 script_to;
  bool ext_active = false; // an outside target exists (Malgrin, during the fight)
  bool ext_open = false;   // ...and can be hurt right now
  Vec2 ext_pos;            // its centre, as drawn (includes how high it floats)
  f32 auto_cd = 0;         // time before Flint will jump in on his own again
  bool fire_power = false; // the Fire of Power makes his flame bigger
  f32 hex_strength = 1;    // fades as the spell comes together
  bool scored = false;     // this breath set something alight
  Vec2 stand;       // where to stand while breathing
  int side = 1;
  bool face_camera = false;  // on the title screen Flint sits facing the viewer
  Chain tail;
  FixedVec<Vec2, 96> trail;  // hero positions, oldest first

  void reset(Vec2 p);
  FlintNews update(const Hero& hero, World& w, Actors& actors, Fx& fx, bool fire_pressed, f32 dt);
  f32 fire_range() const { return fire_power ? FIRE_RANGE * 1.3f : FIRE_RANGE; }
  FlintPose pose(Vec2 root) const;
  void hop() { set_state(FL_HAPPY); }
  void try_fly() { set_state(FL_TRY_FLY); hex_glow = 1.2f; }
  void draw(Renderer& r, Vec2 at, f32 time) const;

 private:
  void set_state(FlintState s) { state = s; state_t = 0; }
  Vec2 follow_target(const Hero& hero) const;
  bool pick_target(const World& w, const Actors& actors, Vec2 from, Vec2 dir, f32 range);
  bool target_ok(const World& w, const Actors& actors) const;
  Vec2 target_pos(const World& w, const Actors& actors) const;
  void move_toward(Vec2 target, f32 max_speed, const World& w, f32 dt);
};

}  // namespace sky
