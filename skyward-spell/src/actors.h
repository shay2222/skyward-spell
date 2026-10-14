// actors.h - everything in a land that moves or reacts: Malgrin's goblins, hazards, hearts to
// pick up, waystones that remember where you've been, and the shrine holding an ingredient.
#pragma once
#include "anim.h"
#include "art.h"

namespace sky {

struct World;
struct Fx;
struct Hero;

enum ActorKind : u8 { AK_GOBLIN, AK_HEART, AK_VENT, AK_SNAPJAW, AK_RUNE, AK_MUD, AK_WAYSTONE, AK_SHRINE };

enum GoblinState : u8 {
  GS_PATROL,   // wandering around the camp
  GS_ALERT,    // spotted the hero: "!"
  GS_CHASE,
  GS_WINDUP,   // club raised
  GS_SWING,
  GS_RECOVER,
  GS_RETURN,   // lost interest, heading home
  GS_POOF,     // Flint's fire sends them running back to Malgrin in a puff of smoke
};

struct Actor {
  ActorKind kind = AK_GOBLIN;
  u8 state = 0;
  bool alive = true;
  u8 view = VIEW_FRONT;
  f32 flip = 1;
  Vec2 pos, prev_pos, vel, home, target;
  Vec2 face = {0, 1};
  f32 t = 0, state_t = 0, phase = 0, walk = 0;
  f32 heat = 0;       // fire on goblins and snapjaws
  f32 timer = 0;      // general purpose: patrol pauses, hazard cycles
  f32 flash = 0;
  bool hit_done = false;
  int index = 0;      // waystone number / ingredient
};

/** What happened this step that the game reacts to (speech, HUD, sound later). */
struct ActorNews {
  int damage = 0;          // hits on the hero
  Vec2 damage_from;
  bool goblin_spotted = false;
  int goblins_poofed = 0;
  int snapjaws_burnt = 0;
  int hearts = 0;          // picked up
  int waystone = -1;       // newly lit waystone
  bool ingredient = false; // took the shrine's ingredient
  bool hazard_near = false;
  Vec2 where;
  int vents = 0, zaps = 0, snaps = 0;  // for sound
  Vec2 sound_at;
};

struct Actors {
  static constexpr int CAP = 160;
  Actor list[CAP];
  int count = 0;
  int last_waystone = 0;
  bool ingredient_taken = false;
  f32 light_k = 1;  // glows are toned down in bright lands so they don't wash out

  void spawn(const World& w, Rng& rng, int ingredient, bool taken);
  /** Sends every goblin home and calms them (after the hero faints). */
  void calm();
  ActorNews update(const Hero& hero, World& w, Fx& fx, f32 dt);
  void draw(Renderer& r, f32 alpha, f32 time) const;

  /** Hazard effects on the hero this step. */
  f32 speed_mul_at(Vec2 p) const;
  /** A goblin chasing the hero within `range` of `near`, or -1. */
  int chasing_goblin(Vec2 near, f32 range) const;
  /** The best thing for Flint's fire among the actors (goblins, snapjaws), or -1. */
  int find_target(Vec2 from, Vec2 dir, f32 range) const;
  bool burnable(int i) const { return i >= 0 && i < count && list[i].alive && (list[i].kind == AK_GOBLIN || list[i].kind == AK_SNAPJAW) && list[i].state != GS_POOF; }
  /** Heats goblins and snapjaws in a cone of fire. Returns how many caught. */
  int heat_cone(Vec2 mouth, Vec2 dir, f32 range, f32 dt, Fx& fx);
  int goblins_left() const;
  /** Malgrin calls goblins into his lair. */
  Actor* spawn_goblin(Vec2 at, Vec2 home);
  int summoned_alive() const;
  void dismiss_summoned(Fx& fx);

 private:
  Actor* add(ActorKind k, Vec2 p);
  void update_goblin(Actor& a, const Hero& hero, World& w, Fx& fx, f32 dt, ActorNews& news);
  void update_hazard(Actor& a, const Hero& hero, World& w, Fx& fx, f32 dt, ActorNews& news);
  void draw_goblin(Renderer& r, const Actor& a, Vec2 at, f32 time) const;
};

}  // namespace sky
