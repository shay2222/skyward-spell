// game.cpp - runs the game: a fixed-rate simulation, the title screen, the story scenes,
// travelling between the six lands, the final fight, the camera, hearts and respawning,
// Flint's lines, and the functions the browser calls.
#include "actors.h"
#include "audio.h"
#include "boss.h"
#include "brain.h"
#include "dragon.h"
#include "fx.h"
#include "hero.h"
#include "platform.h"
#include "world.h"

using namespace sky;

namespace {

constexpr f32 STEP = 1.0f / 120.0f;

alignas(64) u8 g_heap[84u << 20];  // everything the game allocates lives here

Arena g_perm;    // atlas, renderer, particles: lives for the whole session
Arena g_level;   // the current land: reset when a land loads
Atlas g_atlas;
Atlas::Mark g_shared_mark;
Renderer g_r;
World g_world;
Hero g_hero;
Flint g_flint;
Actors g_actors;
Boss g_boss;
Fx g_fx;
Input g_input;
f32 g_info[FI_COUNT_];
f32 g_debug[40];
f32 g_palette[40];
f32 g_speakers[32];  // world positions of whoever can talk: 0 Flint, 1 Malgrin, 2.. people met on the way
char g_text[512];    // the page writes what the player typed here
Brain g_brain;
Audio g_audio;
f32 g_audio_buf[4096 * 2];

/** Someone standing in this land who can be talked to. */
struct Person {
  int id = -1;
  Vec2 pos;
  f32 t = 0, blink_t = 2, blink = 0, flip = 1, near_k = 0;
  int talked = 0;
  bool gift_used = false;
  u32 asked = 0;
};
Person g_people[2];
int g_people_n = 0;

/** A question out with the live model. */
struct Pending {
  int id = 0;
  bool waiting = false;
  Intent intent = IN_CHAT;
  f32 t = 0;
} g_llm;
bool g_live = false;       // the page says a model is loaded and ready
char g_prompt[4096];

enum Mode { MODE_TITLE, MODE_PLAY, MODE_TRAVEL, MODE_SCENE, MODE_ENDING, MODE_TALK };

Song land_song(Region r) { return (Song)((int)SONG_MEADOW + (int)r); }
enum Scene { SC_NONE, SC_INTRO, SC_RITUAL, SC_VICTORY };

/** Flint's authored lines for now; in a later step these come from the dialogue brain. */
struct Lines {
  bool intro = false, intro2 = false, gate_seen = false, first_goblin = false, first_poof = false, first_burn = false;
  bool first_fly = false, first_hazard = false, first_hurt = false, low = false, first_way = false, gust_warned = false;
  bool lair = false, first_block = false, first_tired = false;
  f32 cooldown = 0, malgrin_cd = 0;
  int fly_count = 0, poof_count = 0, tired_count = 0;
} g_lines;

struct Story {
  Scene scene = SC_NONE;
  int beat = -1;
  f32 beat_t = 0, min_t = 0;
  bool next = false;          // the page asked to move on (tap or click)
  bool cam_set = false;
  Vec2 cam;
  bool hero_walk = false;
  Vec2 hero_to;
  bool ritual_done = false;
  f32 ritual_k = 0;           // the circle's glow
  f32 placed_t = -1;          // ingredients flying to their pedestals
  bool hex_broken = false;
  f32 heal_k = 0;             // the Hollow healing after Malgrin's defeat
  bool healed = false;
  f32 ending_t = 0;
  bool ending_sent = false;
} st;

struct Game {
  Mode mode = MODE_TITLE;
  f32 time = 0, mode_t = 0;
  f32 acc = 0;
  Vec2 cam, cam_prev;
  f32 zoom = 2.0f;
  f32 fade = 1.0f;
  int look = 0;
  f32 shake = 0;
  u32 seed = 1;
  u32 pending = 0;  // buttons pressed since the last simulation step
  Vec2 hero_at, flint_at;
  Region region = R_MEADOW;
  u32 collected = 0;     // ingredients found, one bit per land
  f32 travel_t = 0;      // after finding an ingredient: celebrate, then fade and travel
  bool travel_loaded = false;
  f32 respawn_t = -1;    // fading through black after fainting
  f32 gust_t = -1, gust_cd = 8;
  Vec2 gust_dir;
  f32 gust_k = 0;
  f32 idle_t = 0;
  f32 arrow_k = 0;
  Vec2 objective;
  bool has_objective = false;
  int objective_id = -2;
  f32 flash = 0;         // white flash
  Color flash_col = {1, 1, 1, 1};
  f32 zoom_k = 1;        // the ending pulls the camera back
  int talk = -1;         // who we're talking to
  f32 talk_close_t = -1; // counting down to closing the conversation
  int near_person = -1;
  f32 chat_t = 0;        // time since Flint last said anything
  Vec2 pointer;          // where someone just pointed us
  f32 pointer_t = 0;
  bool paused = false;
  bool was_dashing = false;
} g;

/** A sound effect, panned and softened by where it happens relative to the camera. */
void sound(Sfx s, Vec2 at, f32 vol = 1.0f) {
  f32 dx = at.x - g.cam.x, d = length(at - g.cam);
  f32 k = clamp(1.25f - d / 520.0f, 0.0f, 1.0f);
  g_audio.sfx(s, vol * k, clamp(dx / 320.0f, -0.75f, 0.75f));
}
void sound(Sfx s, f32 vol = 1.0f) { g_audio.sfx(s, vol, 0); }

int popcount(u32 v) { int n = 0; while (v) { n += v & 1; v >>= 1; } return n; }
const char* hero_name() { return g.look ? "Theo" : "Mira"; }

/** Builds a short line with the hero's name in it (one at a time; the result is copied out at once). */
const char* with_name(const char* before, const char* after = "") {
  static char buf[400];
  StrBuf s;
  s.add(before).add(hero_name()).add(after);
  int n = min(s.len, 399);
  memcpy(buf, s.buf, n);
  buf[n] = 0;
  return buf;
}

void say(const char* text, f32 seconds = 3.6f, bool force = true) {
  if (!force && g_lines.cooldown > 0) return;
  event(EV_FLINT_SAYS, text, seconds);
  g_lines.cooldown = seconds + 1.2f;
  g.chat_t = 0;
}

void malgrin_says(const char* text, f32 seconds = 3.0f, bool force = false) {
  if (!force && g_lines.malgrin_cd > 0) return;
  StrBuf s;
  s.add("Malgrin|").add(text);
  event(EV_SAYS, s.buf, seconds, 1);
  g_lines.malgrin_cd = seconds + 1.5f;
}

void hud() { event(EV_HUD, "", (f32)g_hero.hearts, (f32)Hero::MAX_HEARTS); }
void boss_bar() { event(EV_BOSS, "Malgrin", (f32)g_boss.hits, g_boss.fighting() || g_boss.state == BS_DEFEAT ? (f32)Boss::MAX_HITS : 0.0f); }

void save() {
  StrBuf s;
  s.add_int(g.region).add(",").add_int(g.look).add(",").add_int((int)g.collected);
  event(EV_SAVE, s.buf);
}

Vec2 move_input() {
  Vec2 m{g_input.move_x, g_input.move_y};
  u32 h = g_input.held;
  if (h & BTN_LEFT) m.x -= 1;
  if (h & BTN_RIGHT) m.x += 1;
  if (h & BTN_UP) m.y -= 1;
  if (h & BTN_DOWN) m.y += 1;
  f32 l = length(m);
  if (l < 0.18f) return {0, 0};  // stick dead zone
  return l > 1 ? m / l : m;
}

f32 g_zoom_override = 0;  // tests only

f32 target_zoom() {
  if (g_zoom_override > 0) return g_zoom_override;
  f32 z = min(g_input.view_h / 340.0f, g_input.view_w / 320.0f);
  if (g.mode == MODE_TITLE) z *= 1.35f;
  return max(z * g.zoom_k, 0.4f);
}

Vec2 clamp_cam(Vec2 c, f32 zoom) {
  f32 hw = g_input.view_w * 0.5f / zoom, hh = g_input.view_h * 0.5f / zoom;
  f32 W = g_world.px_w(), H = g_world.px_h();
  c.x = W <= hw * 2 ? W * 0.5f : clamp(c.x, hw, W - hw);
  c.y = H <= hh * 2 ? H * 0.5f : clamp(c.y, hh, H - hh);
  return c;
}

Vec2 camera_goal() {
  f32 hw = g_input.view_w * 0.5f / g.zoom, hh = g_input.view_h * 0.5f / g.zoom;
  if (g.mode == MODE_TITLE) {
    Vec2 mid = (g_hero.pos + g_flint.pos) * 0.5f + Vec2{0, -18};
    bool wide = g_input.view_w > g_input.view_h * 1.1f;
    return wide ? mid + Vec2{hw * 0.32f, 0} : mid + Vec2{0, hh * 0.3f};
  }
  if ((g.mode == MODE_SCENE || g.mode == MODE_ENDING) && st.cam_set) return st.cam;
  if (g.mode == MODE_TALK && g.talk >= 0) return (g_hero.pos + g_people[g.talk].pos) * 0.5f + Vec2{0, -22 + hh * 0.42f};  // above the panel
  Vec2 goal = g_hero.pos + g_hero.vel * 0.24f + Vec2{0, -16};
  if (g_boss.fighting()) goal = lerp(goal, g_boss.pos - Vec2{0, g_boss.z + 30}, 0.38f);  // keep Malgrin in the picture
  return goal;
}

// ---------------------------------------------------------------- how each land looks
/** Malgrin's Hollow once his magic is gone: dawn light, green grass, clear water. */
const Palette HEALED = {
    {0.55f, 0.74f, 0.38f, 1}, {0.40f, 0.58f, 0.30f, 1}, {0.74f, 0.82f, 0.46f, 1}, {0.24f, 0.30f, 0.22f, 1},
    {0.80f, 0.68f, 0.54f, 1}, {0.66f, 0.54f, 0.44f, 1}, {0.80f, 0.74f, 0.56f, 1},
    {0.50f, 0.82f, 0.86f, 1}, {0.22f, 0.50f, 0.68f, 1}, {0.95f, 0.97f, 0.98f, 1}, {1.0f, 0.85f, 0.6f, 1}, {1.0f, 0.92f, 0.86f, 1}, 0.08f};

struct Look { Color ambient, grade; f32 bloom, vignette, saturation; };

Look region_look() {
  const RegionDef& d = REGIONS[g.region];
  Look l{d.ambient, d.grade, d.bloom, d.vignette, d.saturation};
  if (g.region == R_HOLLOW && st.heal_k > 0) {
    f32 k = st.heal_k;
    l.ambient = mix(l.ambient, Color{1.0f, 0.93f, 0.86f, 1}, k);
    l.grade = mix(l.grade, Color{1.03f, 1.0f, 0.97f, 1}, k);
    l.bloom = lerp(l.bloom, 0.3f, k);
    l.vignette = lerp(l.vignette, 0.3f, k);
    l.saturation = lerp(l.saturation, 1.08f, k);
  }
  return l;
}

void fill_palette() {
  const RegionDef& d = REGIONS[g.region];
  const Palette& p = d.pal;
  const bool heal = g.region == R_HOLLOW && st.heal_k > 0;
  const Color* cols[11] = {&p.grass_a, &p.grass_b, &p.grass_c, &p.floor, &p.dirt_a, &p.dirt_b, &p.shore, &p.shallow, &p.deep, &p.foam, &p.glow};
  const Color* healed[11] = {&HEALED.grass_a, &HEALED.grass_b, &HEALED.grass_c, &HEALED.floor, &HEALED.dirt_a, &HEALED.dirt_b,
                             &HEALED.shore, &HEALED.shallow, &HEALED.deep, &HEALED.foam, &HEALED.glow};
  for (int i = 0; i < 11; i++) {
    Color c = heal ? mix(*cols[i], *healed[i], st.heal_k) : *cols[i];
    g_palette[i * 3 + 0] = c.r;
    g_palette[i * 3 + 1] = c.g;
    g_palette[i * 3 + 2] = c.b;
  }
  Color fog = heal ? mix(p.fog, HEALED.fog, st.heal_k) : p.fog;
  g_palette[33] = fog.r;
  g_palette[34] = fog.g;
  g_palette[35] = fog.b;
  g_palette[36] = heal ? lerp(p.fog_amount, HEALED.fog_amount, st.heal_k) : p.fog_amount;
  g_palette[37] = (f32)(st.healed && g.region == R_HOLLOW ? LQ_WATER : d.liquid);
  g_palette[38] = (f32)(st.healed && g.region == R_HOLLOW ? R_MEADOW : g.region);  // healed ground: grass, not cracked stone
}

// ---------------------------------------------------------------- goals
const char* objective_text() {
  static char buf[160];
  if (g.region == R_HOLLOW) {
    if (g_boss.fighting()) return "Defeat Malgrin: dodge his magic, and when his shield drops, press F!";
    if (st.ritual_done) return "";
    for (int i = 0; i < g_world.gates.count; i++)
      if (!g_world.gates[i].open) return "Malgrin has walled the way ahead. Ask Flint to burn through (F).";
    return "Bring the five ingredients to the circle in Malgrin's lair.";
  }
  for (int i = 0; i < g_world.gates.count; i++)
    if (!g_world.gates[i].open) return "Malgrin has blocked the way ahead. Ask Flint to burn through (F).";
  if (g_actors.ingredient_taken) return "";
  StrBuf s;
  s.add("Find ").add(REGIONS[g.region].ingredient).add(" at the shrine.");
  int n = min(s.len, 159);
  memcpy(buf, s.buf, n);
  buf[n] = 0;
  return buf;
}

void update_objective(bool force = false) {
  int id = -1;
  Vec2 at = g_world.shrine;
  for (int i = 0; i < g_world.gates.count; i++)
    if (!g_world.gates[i].open) { id = i; at = g_world.gates[i].pos; break; }
  if (id < 0 && ((g_actors.ingredient_taken && g.region != R_HOLLOW) || st.ritual_done)) id = 99;
  if (g_boss.fighting()) id = 98;
  g.has_objective = id >= 0 && id < 98 ? true : id == -1;
  g.objective = at;
  if (id != g.objective_id || force) {
    g.objective_id = id;
    event(EV_OBJECTIVE, objective_text());
  }
}

// ---------------------------------------------------------------- loading lands
void load_region(Region r) {
  g.region = r;
  g_atlas.rewind(g_shared_mark);
  bake_region(g_atlas, r);
  g_level.reset();
  g_world.generate(g_level, r, g.seed + (u32)r * 7919u);
  g_fx.clear();
  g_fx.ambience = REGIONS[r].ambience;
  if (r == R_MEADOW || r == R_ELDERWOOD || r == R_SKYREACH) g_fx.seed_butterflies(g_world, r == R_MEADOW ? 14 : 8);
  Rng rng(g.seed + (u32)r * 31u, 5);
  int ingredient = r < R_HOLLOW ? (int)r : -1;
  g_actors.spawn(g_world, rng, ingredient, (g.collected >> r) & 1);
  g_hero.reset(g_world.start, g.look);
  g_hero.hearts = Hero::MAX_HEARTS;
  g_flint.reset(g_world.flint_start);
  g_flint.fire_power = (g.collected >> R_CRAGS) & 1;
  g_flint.hex_strength = 1.0f - 0.16f * popcount(g.collected);
  g_flint.fly_cd = 18;
  g_flint.can_fly = g_flint.flying = g_flint.scripted = g_flint.ext_active = false;
  g_boss = Boss();
  st = Story();
  g_people_n = 0;
  for (int i = 0; i < g_world.people.count; i++) {
    Person& p = g_people[g_people_n++];
    p = Person();
    p.id = g_world.people_id[i];
    p.pos = g_world.people[i];
    p.blink_t = 1.0f + i;
  }
  g.talk = -1;
  g.near_person = -1;
  g.pointer_t = 0;
  g.chat_t = 0;
  event(EV_PROMPT, "");
  g.cam = g.cam_prev = clamp_cam(camera_goal(), g.zoom);
  g.gust_t = -1;
  g.gust_cd = 9;
  g.idle_t = 0;
  g.objective_id = -2;
  g.zoom_k = 1;
  g_lines = Lines();
  fill_palette();
  StrBuf s;
  s.add("land ").add(REGIONS[r].name).add(": ").add_int(g_world.prop_count).add(" props, ").add_int(g_actors.count).add(" actors, atlas ")
      .add_int(g_atlas.count).add(" sprites / ").add_int(g_atlas.used_rows()).add(" rows").add(g_atlas.overflow ? " OVERFLOW" : "");
  log(s);
}

void enter_play() {
  g_audio.play(land_song(g.region));
  g.mode = MODE_PLAY;
  g.mode_t = 0;
  g_flint.face_camera = false;
  g_flint.scripted = false;
  hud();
  event(EV_SPELL, "", (f32)g.collected);
  StrBuf b;
  b.add(REGIONS[g.region].name).add("|").add(REGIONS[g.region].subtitle);
  event(EV_BANNER, b.buf, (f32)g.region);
  event(EV_MODE, "", 1);
  update_objective(true);
  save();
}

// ---------------------------------------------------------------- story scenes
void card(const char* speaker, const char* text, f32 min_t = 0.8f) {
  StrBuf s;
  s.add(speaker).add("|").add(text);
  event(EV_STORY, s.buf, 1);
  st.min_t = min_t;
}

Vec2 pedestal_pos(int i) { return g_world.arena + from_angle(-HALF_PI + i * TAU / 5, 96) * Vec2{1, 0.78f}; }

void flash(f32 k, Color c = {1, 1, 1, 1}) {
  g.flash = k;
  g.flash_col = c;
}

void start_scene(Scene s) {
  st.scene = s;
  st.beat = -1;
  st.beat_t = 0;
  st.min_t = 0;
  st.next = true;  // runs beat 0 straight away
  st.cam_set = false;
  st.hero_walk = false;
  g.mode = MODE_SCENE;
  g.mode_t = 0;
  event(EV_MODE, "", 3);
}

void end_scene();

void begin_beat(int b) {
  st.beat = b;
  st.beat_t = 0;
  st.next = false;
  const char* hero = hero_name();
  switch (st.scene) {
    case SC_INTRO:
      switch (b) {
        case 0:
          st.cam_set = true;
          st.cam = (g_hero.pos + g_flint.pos) * 0.5f + Vec2{50, -24};
          g_flint.hop();
          card("", "Long ago, dragons filled the skies over Willowmere. And no dragon loved to fly more than little Flint.", 1.0f);
          break;
        case 1:
          g_boss.appear(g_hero.pos + Vec2{120, -26});
          sound(SFX_BOSS_HIT, 0.6f);
          g_fx.ring(g_boss.centre(), {0.72f, 0.42f, 1.0f, 0.9f}, 4.0f);
          g_fx.hex_sparks(g_boss.centre(), 24);
          g.shake = 4;
          flash(0.5f, {0.75f, 0.45f, 1.0f, 1});
          card("Malgrin", "Hee hee hee! The sky's magic belongs to ME now. Every last dragon's... starting with yours, little one!", 1.2f);
          break;
        case 2:
          g_boss.beam = 1.4f;
          sound(SFX_ZAP);
          g_boss.beam_to = g_flint.pos + Vec2{0, -16};
          g_flint.hex_strength = 1;
          g_flint.hex_glow = 2.0f;
          g_fx.hex_sparks(g_flint.pos + Vec2{0, -16}, 20);
          g.shake = 3;
          card("Flint", "Hey! What did you do to my wings?! They won't... I can't fly!", 2.4f);
          break;
        case 3:
          card("Malgrin", "Want them back? Then find the Skyward Spell... if you can! Hee hee hee!", 1.8f);
          break;
        case 4:
          card(hero, "Don't worry, Flint. Grandma's old spellbook tells of a Skyward Spell that can break any hex.", 0.8f);
          break;
        case 5:
          card("Flint", "Then let's find all five pieces: the Scroll of Knowledge, the Water of Miracles, the Fire of Power, the Elder Staff and the Feather of the Four Winds!", 1.0f);
          break;
        default: end_scene(); break;
      }
      break;
    case SC_RITUAL:
      switch (b) {
        case 0:
          st.cam_set = true;
          st.cam = g_world.arena + Vec2{0, -14};
          st.hero_walk = true;
          st.hero_to = g_world.arena + Vec2{0, 54};
          g_flint.scripted = true;
          g_flint.script_to = g_world.arena + Vec2{0, 6};
          card("Flint", "This is it: Malgrin's own circle. If the Skyward Spell can be cast anywhere, it's here.", 1.6f);
          break;
        case 1:
          st.placed_t = 0;
          sound(SFX_WAYSTONE);
          card("", with_name("", " set the five treasures around the circle: the scroll, the water, the fire, the staff and the feather."), 2.8f);
          break;
        case 2:
          card(hero, "By scroll and water, fire and staff and feather... SKYWARD!", 2.6f);
          break;
        case 3:
          g_flint.can_fly = true;
          g_flint.flying = true;
          g_flint.hop();
          sound(SFX_FLAP);
          card("Flint", "I'm... I'm FLYING! Woo-hoo! Look at me!", 2.2f);
          break;
        case 4:
          g_boss.arrive(g_world.arena, g_world.arena_r);
          g_audio.play(SONG_BOSS);
          sound(SFX_BOSS_HIT);
          g_fx.ring(g_boss.centre(), {0.72f, 0.42f, 1.0f, 0.9f}, 5.0f);
          g.shake = 5;
          flash(0.6f, {0.75f, 0.45f, 1.0f, 1});
          g_flint.script_to = g_hero.pos + Vec2{-44, -10};
          card("Malgrin", "WHAT?! Who dares break my hex? That sky magic is MINE!", 1.6f);
          break;
        case 5:
          card("Flint", with_name("Not anymore it isn't! ", ", keep moving. When his shield drops, press F and I'll dive at him!"), 1.0f);
          break;
        default: end_scene(); break;
      }
      break;
    case SC_VICTORY:
      switch (b) {
        case 0:
          st.cam_set = true;
          st.cam = (g_boss.pos + g_hero.pos) * 0.5f + Vec2{0, -20};
          g_flint.scripted = true;
          g_flint.script_to = g_hero.pos + Vec2{-40, -8};
          card("Malgrin", "No, no, NO! My magic... my beautiful sky magic!", 2.4f);
          break;
        case 1:
          g_boss.may_flee = true;
          card("Malgrin", "This isn't over! ...Is it? Eep!", 1.2f);
          break;
        case 2:
          card("Flint", "And STAY out of the sky!", 0.8f);
          break;
        case 3:
          flash(1.0f);
          sound(SFX_SPELL);
          g_audio.play(SONG_TITLE);
          st.healed = true;
          st.heal_k = 0.001f;
          g_fx.ambience = AMB_POLLEN;
          g_fx.seed_butterflies(g_world, 18);
          g.shake = 2;
          card("", "With Malgrin's magic broken, light poured into the Hollow, and every hexed dragon in Willowmere felt their wings again.", 3.0f);
          break;
        case 4:
          card(hero, "You did it, Flint!", 0.8f);
          break;
        case 5:
          card("Flint", "WE did it, partner. Come on, hop on. Let's go flying!", 1.0f);
          break;
        default: end_scene(); break;
      }
      break;
    default: break;
  }
}

/** Finishes a scene, applying everything its beats would have done (so skipping is safe). */
void end_scene() {
  Scene s = st.scene;
  st.scene = SC_NONE;
  st.hero_walk = false;
  event(EV_STORY, "");
  switch (s) {
    case SC_INTRO:
      g_boss = Boss();
      g_flint.hex_glow = 0.45f;
      g_flint.hex_strength = 1;
      st.cam_set = false;
      enter_play();
      event(EV_HINT, "WASD or arrow keys to walk · Space to dash · F for Flint's fire");
      break;
    case SC_RITUAL:
      st.ritual_done = true;
      st.placed_t = 99;
      st.ritual_k = 1;
      st.hex_broken = true;
      g_flint.hex_strength = 0;
      g_flint.can_fly = g_flint.flying = true;
      if (!g_boss.fighting()) g_boss.arrive(g_world.arena, g_world.arena_r);
      g_boss.start_fight();
      g_audio.play(SONG_BOSS);
      g_flint.scripted = false;
      g_flint.ext_active = true;
      st.cam_set = false;
      g.mode = MODE_PLAY;
      g.mode_t = 0;
      event(EV_MODE, "", 1);
      boss_bar();
      update_objective(true);
      event(EV_HINT, "Dodge Malgrin's magic. When he's dazed and his shield drops, press F!");
      break;
    case SC_VICTORY:
      st.healed = true;
      g_audio.play(SONG_TITLE);
      if (st.heal_k <= 0) st.heal_k = 0.001f;
      g_fx.ambience = AMB_POLLEN;
      if (g_boss.state != BS_GONE) g_boss.vanish();
      g.mode = MODE_ENDING;
      g.mode_t = 0;
      st.ending_t = 0;
      st.cam_set = true;
      g_flint.scripted = true;
      g_flint.flying = true;
      event(EV_MODE, "", 4);
      event(EV_BOSS, "", 0, 0);
      event(EV_SAVE, "");  // the journey is complete: next time starts fresh
      break;
    default: break;
  }
}

/** What happens during each beat, every step. */
void scene_step(f32 dt) {
  st.beat_t += dt;
  switch (st.scene) {
    case SC_INTRO:
      if (st.beat == 2 && st.beat_t > 0.9f && st.beat_t - dt <= 0.9f) g_flint.try_fly();
      if (st.beat == 3 && st.beat_t > 1.6f && st.beat_t - dt <= 1.6f && g_boss.visible()) {
        g_fx.smoke(g_boss.centre(), 14, 1.4f);
        g_fx.hex_sparks(g_boss.centre(), 16);
        g_boss.vanish();
      }
      break;
    case SC_RITUAL:
      if (st.beat >= 1 && st.placed_t >= 0) st.placed_t += dt;
      if (st.beat == 1) {
        for (int i = 0; i < INGREDIENT_COUNT; i++) {
          f32 land = 0.5f + i * 0.45f;
          if (st.placed_t >= land && st.placed_t - dt < land) {
            g_fx.sparkle(pedestal_pos(i) + Vec2{0, -30}, 14, {1.0f, 0.85f, 0.45f, 1}, 10);
            g_fx.ring(pedestal_pos(i) + Vec2{0, -28}, {1.0f, 0.85f, 0.45f, 0.8f}, 1.6f);
          }
        }
      }
      if (st.beat == 2) {
        st.ritual_k = min(1.0f, st.ritual_k + dt * 0.8f);
        if (g_fx.rng.chance(dt * 30)) g_fx.sparkle(g_world.arena + g_fx.rng.in_circle(90) * Vec2{1, 0.78f}, 1, {1.0f, 0.85f, 0.45f, 1}, 4);
        g_flint.hex_glow = 0.6f + st.beat_t;
        if (!st.hex_broken && st.beat_t > 1.8f) {
          st.hex_broken = true;
          g_flint.hex_strength = 0;
          Vec2 c = g_flint.pos + Vec2{0, -18};
          g_fx.hex_sparks(c, 40);
          g_fx.ring(c, {0.9f, 0.7f, 1.0f, 1}, 4.0f);
          g_fx.ring(c, {1.0f, 0.9f, 0.6f, 1}, 2.6f);
          g_fx.sparkle(c, 30, {1.0f, 0.95f, 0.7f, 1}, 20);
          g.shake = 4;
          flash(0.8f);
          sound(SFX_SPELL);
        }
      }
      if (st.beat == 3) {
        f32 a = st.beat_t * 1.7f;
        g_flint.script_to = g_world.arena + from_angle(a - HALF_PI, 120) * Vec2{1, 0.7f};
      }
      break;
    case SC_VICTORY:
      if (st.beat >= 0) st.cam = damp(st.cam, (g_boss.visible() ? (g_boss.pos + g_hero.pos) * 0.5f : g_hero.pos) + Vec2{0, -20}, 2, dt);
      break;
    default: break;
  }
  if (st.healed && st.heal_k < 1) {
    st.heal_k = min(1.0f, st.heal_k + dt / 3.0f);
    fill_palette();
  }

  bool can_move_on = st.beat_t >= st.min_t;
  if (st.next && can_move_on) begin_beat(st.beat + 1);
  else if (st.beat_t > st.min_t + 16) begin_beat(st.beat + 1);  // nobody there: keep the story going
  st.next = false;
}

void ending_step(f32 dt) {
  st.ending_t += dt;
  g.zoom_k = damp(g.zoom_k, 0.72f, 0.6f, dt);
  f32 a = st.ending_t * 0.9f;
  g_flint.script_to = g_hero.pos + from_angle(a, 130) * Vec2{1, 0.6f};
  st.cam = damp(st.cam, g_hero.pos + Vec2{0, -30}, 2, dt);
  if (st.healed && st.heal_k < 1) {
    st.heal_k = min(1.0f, st.heal_k + dt / 3.0f);
    fill_palette();
  }
  if (!st.ending_sent && st.ending_t > 3.0f) {
    st.ending_sent = true;
    event(EV_ENDING, hero_name());
  }
}

// ---------------------------------------------------------------- the world reacting
void update_blockers(f32 dt) {
  for (int i = 0; i < g_world.prop_count; i++) {
    Prop& p = g_world.props[i];
    if (p.kind != PK_BLOCKER) continue;
    if (!(p.flags & PF_BURNING)) {
      p.heat = max(0.0f, p.heat - dt * 0.6f);
      continue;
    }
    p.burn_t += dt;
    Vec2 base = p.pos + Vec2{0, -6};
    if (g_fx.rng.chance(dt * 26)) g_fx.flame(base + Vec2{g_fx.rng.range(-14, 14), g_fx.rng.range(-4, 4)}, {0, -1}, 70, 0.5f);
    if (g_fx.rng.chance(dt * 10)) g_fx.embers(base, 1);
    if (g_fx.rng.chance(dt * 6)) g_fx.smoke(base + Vec2{0, -14}, 1, 0.8f);
    if (p.burn_t > 1.6f) {
      p.flags = (u8)((p.flags & ~PF_BURNING) | PF_BURNT);
      p.sprite = S.ra.blocker_burnt;
      p.radius = 0;
      p.heat = 0;
      p.sway = 0;
      p.light = 0;
      g_fx.smoke(base, 6, 1.1f);
      g_fx.embers(base, 6);
    }
  }
  for (int i = 0; i < g_world.gates.count; i++) {
    Gate& gt = g_world.gates[i];
    if (gt.open || g_world.blockers_left(i) > 0) continue;
    gt.open = true;
    sound(SFX_GATE, gt.pos);
    event(EV_TOAST, "The way is open!");
    say(i == g_world.gates.count - 1 ? (g.region == R_HOLLOW ? "His lair is just ahead. Ready?" : "Nothing between us and the shrine now!")
                                     : "That's one less of Malgrin's tricks. Onward!", 3.4f);
    update_objective();
  }
}

/** Props that give off particles: blossom petals, spores, embers, fireflies, campfire flames. */
void emit_from_props(f32 dt) {
  const bool leafy = g.region == R_MEADOW || g.region == R_ELDERWOOD || g.region == R_MARSH || g.region == R_SKYREACH;
  g_world.each_in(g_r.view, [&](Prop& p, int) {
    if (p.flags & (PF_GONE | PF_BURNT)) return;
    Rng& rng = g_fx.rng;
    switch (p.emit) {
      case EMIT_PETALS:
        if (rng.chance(dt * 1.2f)) g_fx.petal(p.pos + Vec2{rng.range(-30, 30) * p.scale, rng.range(-4, 10)});
        break;
      case EMIT_LEAVES:
        if (rng.chance(dt * 0.3f)) g_fx.leaf(p.pos + Vec2{rng.range(-30, 30) * p.scale, rng.range(-4, 10)});
        break;
      case EMIT_SPORES:
        if (rng.chance(dt * 2.0f)) g_fx.spore(p.pos + Vec2{rng.range(-12, 12), -20 * p.scale + rng.range(-8, 8)}, p.light_col.with_alpha(0.85f));
        break;
      case EMIT_EMBERS:
        if (rng.chance(dt * 1.2f)) g_fx.embers(p.pos + Vec2{rng.range(-14, 14), -rng.range(20, 60) * p.scale}, 1);
        break;
      case EMIT_FIREFLIES:
        if (rng.chance(dt * 0.5f)) g_fx.firefly(p.pos + Vec2{rng.range(-50, 50), rng.range(-30, 20)});
        break;
      case EMIT_SMOKE:
        if (rng.chance(dt * 1.6f)) g_fx.smoke(p.pos + rng.in_circle(4), 1, 0.7f);
        if (rng.chance(dt * 1.0f)) g_fx.embers(p.pos, 1);
        break;
      case EMIT_SPARKS: {
        bool torch = p.sprite == S.game.torch;
        Vec2 at = p.pos + Vec2{0, torch ? -26.0f * p.scale : -1.0f};
        if (rng.chance(dt * (torch ? 14.0f : 22.0f))) g_fx.fire(at, torch ? 0.7f * p.scale : 1.0f);
        if (rng.chance(dt * 2.0f)) g_fx.embers(at + Vec2{0, -6}, 1);
        break;
      }
      default:
        if (leafy && (p.kind == PK_TREE || p.kind == PK_EDGE) && rng.chance(dt * 0.05f)) g_fx.leaf(p.pos + Vec2{rng.range(-30, 30) * p.scale, rng.range(-4, 10)});
        break;
    }
  });
}

Facts make_facts(int person);

void region_lines(const FlintNews& news, const ActorNews& an) {
  g_lines.cooldown = max(0.0f, g_lines.cooldown - STEP);
  g_lines.malgrin_cd = max(0.0f, g_lines.malgrin_cd - STEP);
  if (g.mode != MODE_PLAY) return;
  const Region r = g.region;
  if (!g_lines.intro && g.mode_t > 1.4f && !(r == R_HOLLOW && st.ritual_done)) {
    g_lines.intro = true;
    const char* intro[R_COUNT] = {
        "Ready when you are! Let's find everything for the Skyward Spell.",
        "Mistfen Marsh. Smells like wet socks. The Water of Miracles is out here somewhere.",
        "Hot, hot, hot! Cinderpeak Crags. Watch the vents: when they rumble, they blow.",
        "The Elderwood. These trees are older than dragons. Mind the snapjaws; I can toast them.",
        "Look how high we are! One day I'll fly over all of this. Careful near the edges.",
        "Malgrin's Hollow. His magic is everywhere. Stay close to me.",
    };
    say(intro[r], 4.4f);
    return;
  }
  if (!g_lines.intro2 && g.mode_t > 7.5f && r == R_MEADOW) {
    g_lines.intro2 = true;
    say("The Scroll of Knowledge sits on an old shrine, east of here. Follow the path!", 4.2f, false);
    return;
  }
  if (an.ingredient) return;  // the game says something special
  if (news.goblins_hit) {
    g_lines.poof_count++;
    if (!g_lines.first_poof) { g_lines.first_poof = true; say("Ha! Back to Malgrin with you!", 3.0f); }
    else if (g_lines.poof_count % 3 == 0 && g_lines.cooldown <= 0) {
      Facts f = make_facts(-1);
      Line l;
      g_brain.flint(f, FT_POOF, "", l);
      say(l.text, 2.6f, false);
    }
    return;
  }
  if (news.caught_fire && !g_lines.first_burn) {
    g_lines.first_burn = true;
    say("My wings may be hexed, but my fire isn't!", 3.2f);
    return;
  }
  if (an.goblin_spotted && !g_lines.first_goblin && !g_boss.fighting()) {
    g_lines.first_goblin = true;
    say("Goblins! Malgrin's lot. Keep moving, I'll handle them!", 3.6f);
    event(EV_HINT, "Flint fights goblins by himself. Press F to point him at one, and Space to dash out of trouble.");
    return;
  }
  if (an.waystone > 0) {
    event(EV_TOAST, "Waystone awakened");
    if (!g_lines.first_way) {
      g_lines.first_way = true;
      say("A waystone! If things go badly, we'll wake up here.", 3.4f, false);
    }
    return;
  }
  if (an.damage && !g_lines.first_hurt) {
    g_lines.first_hurt = true;
    say("Are you all right? Dash with Space to get out of the way!", 3.4f, false);
    return;
  }
  if (g_hero.hearts == 1 && !g_lines.low && !g_hero.busy()) {
    g_lines.low = true;
    say("You're hurt! Hearts are hidden around here if we look.", 3.6f);
    return;
  }
  if (an.hazard_near && !g_lines.first_hazard) {
    g_lines.first_hazard = true;
    const char* l[R_COUNT] = {"", "Mud! It'll slow you right down. Go around if you can.", "That vent's rumbling. Wait for it to blow, then run past!",
                              "Snapjaw! It bites when you get close. Ask me to burn it, F!", "", "Those runes crackle before they strike. Time your steps."};
    if (*l[r]) { say(l[r], 3.8f, false); return; }
  }
  if (!g_lines.gate_seen && g_lines.cooldown <= 0) {
    for (const Gate& gt : g_world.gates)
      if (!gt.open && dist(gt.pos, g_hero.pos) < 190) {
        g_lines.gate_seen = true;
        say(r == R_MEADOW ? "Those brambles reek of Malgrin's magic. Press F and I'll burn them!" : "Malgrin's blocked the way again. F, and I'll clear it!", 4.0f);
        event(EV_HINT, "Press F (or the gamepad's X button) to ask Flint to breathe fire.");
        return;
      }
  }
  if (r == R_HOLLOW && !g_lines.lair && !st.ritual_done && dist(g_hero.pos, g_world.arena) < g_world.arena_r + 200) {
    g_lines.lair = true;
    say("There. Malgrin's lair. Whatever happens next... thank you for coming this far with me.", 5.0f);
    return;
  }
  if (news.landed_sad) {
    g_lines.fly_count++;
    if (!g_lines.first_fly) {
      g_lines.first_fly = true;
      say("Hmph. Still can't fly. Not until the Skyward Spell breaks Malgrin's hex.", 4.2f, false);
    } else {
      const char* lines[3] = {"One day these wings will work. One day.", "...Nope. Still grounded.", "I felt it that time! Almost!"};
      say(lines[g_lines.fly_count % 3], 3.0f, false);
    }
  }
}

void found_ingredient() {
  g.collected |= 1u << g.region;
  const RegionDef& d = REGIONS[g.region];
  event(EV_SPELL, d.ingredient, (f32)g.collected);
  StrBuf toast;
  toast.add("You found ").add(d.ingredient).add("!");
  event(EV_TOAST, toast.buf);
  const char* lines[INGREDIENT_COUNT] = {
      "The Scroll of Knowledge! It's got the whole Skyward Spell written in it. Four more things to find.",
      "The Water of Miracles! I can feel the hex loosening already.",
      "The Fire of Power! Whoa... my flame feels twice as big!",
      "The Elder Staff. It hums. I think it likes you.",
      "The Feather of the Four Winds! That's everything. Now... Malgrin's Hollow.",
  };
  say(lines[g.region], 5.0f);
  g_flint.hex_strength = 1.0f - 0.16f * popcount(g.collected);
  if (g.region == R_CRAGS) g_flint.fire_power = true;
  g.travel_t = 0;
  g.travel_loaded = false;
  g.shake = 2.0f;
  sound(SFX_INGREDIENT);
  save();
}

void respawn_at_waystone() {
  Vec2 at = g_world.waystones[clamp(g_actors.last_waystone, 0, g_world.waystones.count - 1)] + Vec2{0, 26};
  if (g_boss.fighting()) at = g_world.arena + Vec2{-g_world.arena_r * 0.6f, 20};
  g_hero.reset(g_world.resolve(at, Hero::RADIUS), g.look);
  g_hero.hearts = Hero::MAX_HEARTS;
  g_hero.invuln = 1.5f;
  Vec2 fp = at + Vec2{-30, 10};
  g_flint.reset(g_flint.flying ? fp : g_world.resolve(fp, Flint::RADIUS));
  g_actors.calm();
  if (g_boss.fighting()) {
    g_boss.calm();
    g_actors.dismiss_summoned(g_fx);
  }
  g.cam = g.cam_prev = clamp_cam(camera_goal(), g.zoom);
  hud();
  g_fx.sparkle(g_hero.pos + Vec2{0, -16}, 18, {0.45f, 1.0f, 0.75f, 1}, 12);
  if (g_boss.fighting()) say("Up you get! He's not so tough. Wait for his shield to drop!", 4.0f);
  else say("Up you get. The waystone brought us back. Let's try that again, carefully.", 4.0f);
}

// ---------------------------------------------------------------- talking to people
const char* const COMPASS8[8] = {"east", "south-east", "south", "south-west", "west", "north-west", "north", "north-east"};

const char* compass(Vec2 d) {
  f32 a = atan2f(d.y, d.x);
  int i = (int)floorf((a + PI / 8) / (PI / 4));
  return COMPASS8[(i % 8 + 8) % 8];
}

const char* far_word(f32 d) { return d < 450 ? "not far" : d < 1100 ? "a short walk" : "a long walk"; }

/** Everything true right now that a speaker may mention, seen from where they stand. */
Facts make_facts(int person) {
  Facts f;
  Vec2 from = person >= 0 ? g_people[person].pos : g_hero.pos;
  f.npc = person >= 0 ? g_people[person].id : -1;
  f.land = g.region;
  f.hero = hero_name();
  const bool hollow = g.region == R_HOLLOW;
  f.ingredient = hollow ? "" : REGIONS[g.region].ingredient;
  f.ingredient_taken = (g.collected >> g.region) & 1;
  f.collected = popcount(g.collected);
  Vec2 goal = hollow ? g_world.arena : g_world.shrine;
  f.goal_dir = compass(goal - from);
  f.goal_far = far_word(dist(goal, from));
  // the blocked crossing that matters: the first one ahead of the speaker, or failing that the first one at all
  Vec2 gate = goal;
  bool ahead = false;
  for (const Gate& gt : g_world.gates)
    if (!gt.open) {
      if (f.gates_closed == 0) gate = gt.pos;
      if (!ahead && gt.pos.x > from.x - 40) { gate = gt.pos; ahead = true; }
      f.gates_closed++;
    }
  f.gate_dir = compass(gate - from);
  // the nearest camp that still has goblins in it
  f32 best = 1e18f;
  Vec2 camp = from;
  for (Vec2 c : g_world.camps) {
    int n = 0;
    for (int i = 0; i < g_actors.count; i++) {
      const Actor& a = g_actors.list[i];
      if (a.kind == AK_GOBLIN && a.alive && a.index != 77 && dist(a.home, c) < 1) n++;
    }
    if (n > 0 && length2(c - from) < best) { best = length2(c - from); camp = c; f.goblins_near = n; }
  }
  f.camp_dir = compass(camp - from);
  best = 1e18f;
  for (int i = 0; i < g_actors.count; i++) {
    const Actor& a = g_actors.list[i];
    if (a.kind != AK_HEART || !a.alive) continue;
    if (length2(a.pos - from) < best) { best = length2(a.pos - from); f.heart_known = true; f.heart_dir = compass(a.pos - from); }
  }
  f.hearts = g_hero.hearts;
  f.max_hearts = Hero::MAX_HEARTS;
  f.flint_flies = g_flint.can_fly;
  f.malgrin_beaten = g_boss.state >= BS_DEFEAT || st.healed;
  if (person >= 0) {
    f.times_talked = g_people[person].talked;
    f.gift_left = !g_people[person].gift_used;
    f.asked = g_people[person].asked;
  }
  return f;
}

Vec2 nearest_heart(Vec2 from) {
  Vec2 best_p = from;
  f32 best = 1e18f;
  for (int i = 0; i < g_actors.count; i++) {
    const Actor& a = g_actors.list[i];
    if (a.kind == AK_HEART && a.alive && length2(a.pos - from) < best) { best = length2(a.pos - from); best_p = a.pos; }
  }
  return best_p;
}

void talk_choices() {
  const Person& p = g_people[g.talk];
  StrBuf s;
  auto add = [&](Intent in, const char* label) {
    if (s.len) s.add(";");
    s.add_int(in).add(":").add(label);
  };
  const bool hollow = g.region == R_HOLLOW;
  if (!hollow && !((g.collected >> g.region) & 1)) {
    StrBuf l;
    l.add("Where's ").add(REGIONS[g.region].ingredient).add("?");
    add(IN_INGREDIENT, l.buf);
  } else if (hollow && !g_flint.can_fly) {
    add(IN_WAY, "Where's Malgrin's lair?");
  } else {
    add(IN_WAY, "Which way now?");
  }
  add(IN_DANGER, "Is it dangerous around here?");
  if (g_hero.hearts < Hero::MAX_HEARTS) add(IN_HELP, "I'm hurt. Can you help?");
  else if (!(p.asked & (1u << IN_SELF))) add(IN_SELF, "Tell me about yourself.");
  else add(IN_FLINT, "What do you think of Flint?");
  if (!(p.asked & (1u << IN_MALGRIN))) add(IN_MALGRIN, "What do you know about Malgrin?");
  else add(IN_PLACE, "What's this place like?");
  add(IN_BYE, "Goodbye!");
  event(EV_TALK_CHOICES, s.buf);
}

void deliver(const Line& l) {
  if (g.talk < 0) return;
  Person& p = g_people[g.talk];
  StrBuf s;
  s.add(NPCS[p.id].name).add("|").add(l.text);
  event(EV_TALK_LINE, s.buf, (f32)l.source);
  sound(SFX_TALK, 0.8f);
  // the line can only suggest an action; the game decides whether it happens
  switch (l.action) {
    case ACT_GIFT_HEART:
      if (!p.gift_used && g_hero.hearts < Hero::MAX_HEARTS) {
        p.gift_used = true;
        g_hero.hearts = min(Hero::MAX_HEARTS, g_hero.hearts + 2);
        hud();
        sound(SFX_HEART);
        g_fx.sparkle(g_hero.pos + Vec2{0, -18}, 18, {1.0f, 0.55f, 0.65f, 1}, 10);
        g_fx.ring(g_hero.pos + Vec2{0, -16}, {1.0f, 0.6f, 0.7f, 0.8f}, 1.8f);
      }
      break;
    case ACT_SHOW_HEART:
      g.pointer = nearest_heart(g_hero.pos);
      g.pointer_t = 12;
      break;
    case ACT_SHOW_GOAL:
      g.pointer = g.objective;
      g.pointer_t = 12;
      break;
    default: break;
  }
  if (l.intent == IN_BYE) {
    g.talk_close_t = 1.8f;
    event(EV_TALK_CLOSE, "", 1.8f);
  } else {
    talk_choices();
  }
}

/** What a picked topic means in the player's own words (for the model's prompt). */
const char* topic_words(Intent in) {
  switch (in) {
    case IN_INGREDIENT: return "Where can we find this land's ingredient for the Skyward Spell?";
    case IN_DANGER: return "Is it dangerous around here?";
    case IN_HELP: return "I'm hurt. Can you help?";
    case IN_SELF: return "Tell me about yourself.";
    case IN_MALGRIN: return "What do you know about Malgrin?";
    case IN_PLACE: return "What's this place like?";
    case IN_FLINT: return "What do you think of Flint?";
    case IN_WAY: return "Which way should we go?";
    default: return "Hello!";
  }
}

void respond(Intent in, const char* said = nullptr) {
  if (g.talk < 0 || g.talk_close_t >= 0 || g_llm.waiting) return;
  Person& p = g_people[g.talk];
  Facts f = make_facts(g.talk);
  p.asked |= 1u << in;
  // with a live model ready, it writes the line; the guard still checks it before anyone sees it
  if (g_live && in != IN_BYE) {
    g_llm.id++;
    g_llm.waiting = true;
    g_llm.intent = in;
    g_llm.t = 0;
    int n = g_brain.build_prompt(f, in, said ? said : topic_words(in), g_prompt, sizeof(g_prompt));
    event(EV_TALK_WAIT, "", 1);
    js_llm_ask(g_llm.id, g_prompt, n);
    return;
  }
  Line l;
  g_brain.reply(f, in, l);
  deliver(l);
}

/** The model's answer arrived (in g_text), or it gave up. */
void llm_answer(int id, const char* text, int len) {
  if (!g_llm.waiting || id != g_llm.id) return;
  g_llm.waiting = false;
  event(EV_TALK_WAIT, "", 0);
  if (g.talk < 0) return;
  Facts f = make_facts(g.talk);
  Line l;
  char say[320];
  Action act = ACT_NONE;
  if (text && Brain::parse_reply(text, len, say, sizeof(say), act)) {
    g_brain.accept_live(f, g_llm.intent, say, (int)cstr_len(say), act, l);
  } else {
    brain_log(NPCS[g_people[g.talk].id].name, g_llm.intent, SRC_LIVE, false, text ? "answer wasn't the JSON asked for" : "model failed or timed out", text ? text : "");
    g_brain.reply(f, g_llm.intent, l);
  }
  deliver(l);
}

void open_talk(int i) {
  Person& p = g_people[i];
  g.talk = i;
  g.mode = MODE_TALK;
  g.mode_t = 0;
  g.talk_close_t = -1;
  g_hero.face = normalize(p.pos - g_hero.pos + Vec2{0.01f, 0});
  choose_view(g_hero.face, g_hero.view, g_hero.flip);
  g_flint.scripted = true;
  g_flint.script_to = g_hero.pos + Vec2{(g_hero.pos.x < p.pos.x ? -1.0f : 1.0f) * 32.0f, 10};
  g.near_person = -1;
  event(EV_PROMPT, "");
  StrBuf h;
  h.add(NPCS[p.id].name).add("|").add(NPCS[p.id].role);
  event(EV_TALK_OPEN, h.buf, (f32)p.id);
  event(EV_MODE, "", 5);
  Facts f = make_facts(i);
  Line l;
  g_brain.reply(f, IN_GREET, l);
  deliver(l);
  p.talked++;
}

void close_talk() {
  if (g.talk < 0) return;
  if (g_llm.waiting) { g_llm.waiting = false; event(EV_TALK_WAIT, "", 0); }
  const char* name = NPCS[g_people[g.talk].id].name;
  g.talk = -1;
  g.mode = MODE_PLAY;
  g.mode_t = 10;
  g.talk_close_t = -1;
  g_flint.scripted = false;
  event(EV_TALK_CLOSE, "", 0);
  event(EV_MODE, "", 1);
  if (g_fx.rng.chance(0.6f)) {
    Facts f = make_facts(-1);
    Line l;
    g_brain.flint(f, FT_AFTER_TALK, name, l);
    say(l.text, 3.0f);
  }
}

void people_step(f32 dt) {
  int near = -1;
  f32 best = 44;
  for (int i = 0; i < g_people_n; i++) {
    Person& p = g_people[i];
    p.t += dt;
    p.blink_t -= dt;
    if (p.blink_t <= 0) { p.blink = 0.13f; p.blink_t = 2.0f + g_fx.rng.range(0, 3.0f); }
    p.blink = max(0.0f, p.blink - dt);
    f32 d = dist(g_hero.pos, p.pos);
    if (d < 160 && absf(g_hero.pos.x - p.pos.x) > 4) p.flip = g_hero.pos.x < p.pos.x ? -1.0f : 1.0f;
    bool can = g.mode == MODE_PLAY && !g_hero.busy() && !g_boss.fighting();
    p.near_k = damp(p.near_k, (d < 50 && can) || g.talk == i ? 1.0f : 0.0f, 8, dt);
    if (can && d < best) { best = d; near = i; }
    if (d < 14 && d > 0.01f) g_hero.pos = p.pos + (g_hero.pos - p.pos) / d * 14;
  }
  if (near != g.near_person && g.mode == MODE_PLAY) {
    g.near_person = near;
    if (near >= 0) {
      StrBuf s;
      s.add("Talk to ").add(NPCS[g_people[near].id].name);
      event(EV_PROMPT, s.buf);
    } else {
      event(EV_PROMPT, "");
    }
  }
}

void draw_people(f32 time) {
  for (int i = 0; i < g_people_n; i++) {
    const Person& p = g_people[i];
    const NpcArt& art = S.npc[p.id];
    f32 br = sinf(p.t * 2.2f + i) * 0.014f;
    f32 lift = p.id == 9 ? 6 + sinf(p.t * 2.0f) * 3.0f : 0.0f;  // Lumi floats
    Vec2 at = p.pos - Vec2{0, lift};
    g_r.draw({.id = S.shadow, .pos = p.pos + Vec2{1, 0}, .layer = LAYER_GROUND, .scale = {0.62f, 0.62f}, .color = {0.02f, 0.08f, 0.06f, 0.32f}});
    g_r.draw({.id = art.body, .pos = at, .sort_y = p.pos.y, .layer = LAYER_WORLD, .scale = {p.flip * (1 - br), 1 + br}});
    if (p.blink > 0) g_r.draw({.id = art.blink, .pos = at, .sort_y = p.pos.y + 0.1f, .layer = LAYER_WORLD, .scale = {p.flip * (1 - br), 1 + br}});
    if (p.near_k > 0.02f && g.talk != i) {
      f32 k = p.near_k;
      g_r.draw({.id = S.game.talk, .pos = at + Vec2{0, -58 + sinf(time * 3) * 1.5f}, .layer = LAYER_GLOW, .scale = {k, k}, .color = {1, 1, 1, k}});
    }
  }
}

void update_gusts(f32 dt, HeroEnv& env) {
  if (g.region != R_SKYREACH || g.mode != MODE_PLAY) { g.gust_k = 0; return; }
  if (g.gust_t < 0) {
    g.gust_cd -= dt;
    if (g.gust_cd <= 0) {
      g.gust_t = 0;
      f32 side = g_fx.rng.chance(0.5f) ? 1.0f : -1.0f;
      g.gust_dir = normalize(Vec2{g_fx.rng.range(-0.35f, 0.35f), side});
      if (!g_lines.gust_warned) {
        g_lines.gust_warned = true;
        say("Hold on! The wind up here could push us right off the cliffs!", 3.6f);
        event(EV_HINT, "Gusts push you toward the edges. Walk into the wind, or wait them out.");
      }
    }
  } else {
    g.gust_t += dt;
    const f32 len = 1.7f;
    g.gust_k = sinf(saturate(g.gust_t / len) * PI);
    env.push = g.gust_dir * (g.gust_k * 90.0f);
    Rect v = g_r.view;
    if (g_fx.rng.chance(dt * 45 * g.gust_k)) {
      Vec2 at{g_fx.rng.range(v.x0, v.x1), g_fx.rng.range(v.y0, v.y1)};
      g_fx.wind_streak(at, g.gust_dir);
    }
    if (g.gust_t > len) { g.gust_t = -1; g.gust_cd = g_fx.rng.range(5.0f, 9.0f); }
  }
}

void boss_step(const FlintNews& news, f32 dt) {
  BossNews bn = g_boss.update(g_hero, g_actors, g_fx, news.ext_hit, g_flint.pos, dt);
  g_flint.ext_active = g_boss.fighting();
  g_flint.ext_open = g_boss.vulnerable();
  g_flint.ext_pos = g_boss.centre();
  if (bn.damage && g.mode == MODE_PLAY && g_hero.hurt(bn.from, g_fx)) { g.shake = max(g.shake, 3.0f); hud(); sound(SFX_HURT); }
  if (bn.volley) sound(SFX_ORB, g_boss.pos, 0.8f);
  if (bn.zap) sound(SFX_ZAP, g_hero.pos, 0.7f);
  if (bn.tired) sound(SFX_UI, 0.6f);
  if (bn.blocked) sound(SFX_BLOCK, g_boss.pos);
  if (bn.summoned) sound(SFX_POOF, g_boss.pos);
  if (bn.hurt) sound(SFX_BOSS_HIT);
  if (bn.defeated) sound(SFX_SPELL, 0.7f);
  if (bn.tired) {
    g_lines.tired_count++;
    say(g_lines.tired_count == 1 ? "Look, he's worn himself out! His shield's down. NOW, press F!" : "He's dazed again! Press F!", 3.0f);
    event(EV_HINT, "Malgrin's shield is down! Press F and Flint will dive at him.");
    malgrin_says("Huff... puff... just... a little... rest...", 2.4f, true);
  }
  if (bn.recovered) malgrin_says("Too slow! Hee hee hee!", 2.4f, true);
  if (bn.blocked) {
    malgrin_says("Ha! My shield laughs at your little flame!", 2.6f);
    if (!g_lines.first_block) { g_lines.first_block = true; say("His shield's too strong right now. Wait till he tires himself out!", 3.6f); }
  }
  if (bn.summoned) malgrin_says("Goblins! To me!", 2.0f);
  if (bn.hurt) {
    boss_bar();
    const char* l[3] = {"OWW! You'll pay for that, you overgrown lizard!", "My shield! My beautiful shield!", "No... NO!"};
    malgrin_says(l[clamp(g_boss.hits - 1, 0, 2)], 2.6f, true);
    const char* f[3] = {"Got him! Two more like that!", "One more time! We can do this!", "That's for my wings!"};
    say(f[clamp(g_boss.hits - 1, 0, 2)], 2.6f);
    g.shake = 4;
  }
  if (bn.defeated) {
    g_actors.dismiss_summoned(g_fx);
    event(EV_BOSS, "", (f32)g_boss.hits, 0);
    start_scene(SC_VICTORY);
  }
  // nobody walks through Malgrin, and the lair's magic keeps the fight inside
  if (g_boss.fighting()) {
    Vec2 d = g_hero.pos - g_boss.pos;
    f32 l = length(d);
    if (l < 22 && l > 0.01f) g_hero.pos = g_boss.pos + d / l * 22;
    Vec2 c = g_hero.pos - g_world.arena;
    f32 lim = g_world.arena_r - 14;
    if (length(c) > lim) g_hero.pos = g_world.arena + normalize(c) * lim;
    if (g_fx.rng.chance(dt * 18)) g_fx.spore(g_world.arena + from_angle(g_fx.rng.range(0, TAU), g_world.arena_r) * Vec2{1, 0.92f}, {0.8f, 0.55f, 1.0f, 0.8f});
  }
}

void sim_step(f32 dt) {
  u32 pressed = g.pending;
  g.pending = 0;
  if (g.mode == MODE_PLAY && (pressed & BTN_PAUSE) && !g.paused) {
    g.paused = true;
    event(EV_PAUSE, "", 1);
    return;
  }
  if (g.paused) return;
  g.time += dt;
  g.mode_t += dt;
  g.cam_prev = g.cam;
  g.flash = max(0.0f, g.flash - dt * 1.6f);

  // fades: in after loading, out before travelling or respawning
  bool fading_out = (g.mode == MODE_TRAVEL && !g.travel_loaded) || (g.respawn_t >= 0 && g.respawn_t < 0.6f);
  g.fade = fading_out ? min(1.0f, g.fade + dt * 1.6f) : max(0.0f, g.fade - dt * 1.3f);

  FlintNews news;
  ActorNews an;
  if (g.mode == MODE_SCENE && (pressed & (BTN_INTERACT | BTN_DASH | BTN_FIRE))) st.next = true;
  if (g.mode == MODE_PLAY && (pressed & BTN_INTERACT) && g.near_person >= 0 && !g_hero.busy()) {
    open_talk(g.near_person);
    pressed &= ~BTN_INTERACT;
  }
  if (g.mode == MODE_TALK) {
    // the conversation holds the world still around it
    g_hero.update({0, 0}, false, g_world, g_fx, dt);
    g_flint.update(g_hero, g_world, g_actors, g_fx, false, dt);
    if (g.talk_close_t >= 0) {
      g.talk_close_t -= dt;
      if (g.talk_close_t < 0) close_talk();
    }
    if (g_llm.waiting) {
      g_llm.t += dt;
      if (g_llm.t > 20) llm_answer(g_llm.id, nullptr, 0);  // too slow: the offline generator answers instead
    }
    if (pressed & BTN_PAUSE) close_talk();
  } else if (g.mode == MODE_PLAY || g.mode == MODE_TRAVEL || g.mode == MODE_SCENE || g.mode == MODE_ENDING) {
    HeroEnv env;
    env.speed_mul = g_actors.speed_mul_at(g_hero.pos);
    env.can_fall = g_world.sky_liquid();
    update_gusts(dt, env);
    bool control = g.mode == MODE_PLAY && g.respawn_t < 0;
    Vec2 move = control ? move_input() : Vec2{0, 0};
    if (g.mode == MODE_SCENE && st.hero_walk) {
      Vec2 d = st.hero_to - g_hero.pos;
      if (length(d) > 5) move = normalize(d) * 0.7f;
      else { st.hero_walk = false; g_hero.face = {0, 1}; g_hero.view = VIEW_FRONT; }
    }
    g_hero.update(move, control && (pressed & BTN_DASH) != 0, g_world, g_fx, dt, env);
    news = g_flint.update(g_hero, g_world, g_actors, g_fx, control && (pressed & BTN_FIRE) != 0, dt);
    if (g.mode != MODE_SCENE && g.mode != MODE_ENDING) an = g_actors.update(g_hero, g_world, g_fx, dt);
    if (an.damage && g_hero.hurt(an.damage_from, g_fx)) {
      g.shake = max(g.shake, 3.0f);
      hud();
      sound(SFX_HURT);
    }
    if (an.hearts) {
      g_hero.hearts = min(Hero::MAX_HEARTS, g_hero.hearts + an.hearts);
      hud();
      g_lines.low = false;
      sound(SFX_HEART);
    }
    if (an.waystone > 0) sound(SFX_WAYSTONE);
    if (an.goblin_spotted) sound(SFX_ALERT, an.where, 0.8f);
    if (an.goblins_poofed) sound(SFX_POOF, an.where);
    if (an.vents) sound(SFX_VENT, an.sound_at, 0.8f);
    if (an.zaps) sound(SFX_ZAP, an.sound_at, 0.6f);
    if (an.snaps) sound(SFX_SNAP, an.sound_at);
    if (news.started_fire) sound(SFX_FIRE, g_flint.pos, g_flint.fire_power ? 1.0f : 0.8f);
    if (news.caught_fire) sound(SFX_BURN, g_flint.pos);
    if (news.goblins_hit) sound(SFX_POOF, g_flint.pos, 0.6f);
    bool dashing = g_hero.dash_t > 0;
    if (dashing && !g.was_dashing) sound(SFX_DASH, 0.8f);
    g.was_dashing = dashing;
    if (an.ingredient) found_ingredient();
    if (an.goblin_spotted || an.ingredient) update_objective();
    boss_step(news, dt);

    // fainting and falling
    if (g_hero.fainted() && g.respawn_t < 0 && g_hero.faint_t > 1.2f) g.respawn_t = 0;
    if (g_hero.falling() && g_hero.fall_t <= STEP) sound(SFX_FALL);
    if (g_hero.falling() && g.respawn_t < 0 && g_hero.fall_t > 0.75f) {
      g_hero.hearts = max(0, g_hero.hearts - 1);
      hud();
      if (g_hero.hearts == 0) {
        g.respawn_t = 0;
      } else {
        Vec2 back = g_hero.safe_pos;
        g_hero.reset(back, g.look);
        g_hero.invuln = 1.2f;
        g_flint.reset(g_world.resolve(back + Vec2{-28, 10}, Flint::RADIUS));
        g_fx.sparkle(back + Vec2{0, -14}, 12, {0.75f, 1.0f, 0.85f, 1});
        g.shake = 2.0f;
        say("Whoa! Careful! I can't swoop down and catch you. Not yet, anyway.", 3.6f);
      }
    }
    if (g.respawn_t >= 0) {
      g.respawn_t += dt;
      if (g.respawn_t >= 0.6f && g.respawn_t - dt < 0.6f) respawn_at_waystone();
      if (g.respawn_t > 1.4f) g.respawn_t = -1;
    }

    // after the ingredient: celebrate, then travel on
    if (g_actors.ingredient_taken && g.mode == MODE_PLAY && g.region < R_HOLLOW && (g.collected >> g.region & 1)) {
      g.travel_t += dt;
      if (g.travel_t > 4.2f) { g.mode = MODE_TRAVEL; g.mode_t = 0; event(EV_MODE, "", 2); }
    }
    if (g.mode == MODE_TRAVEL && !g.travel_loaded && g.fade >= 1.0f) {
      g.travel_loaded = true;
      load_region((Region)(g.region + 1));
      enter_play();
    }
    // reaching Malgrin's circle with everything: the ritual
    if (g.mode == MODE_PLAY && g.region == R_HOLLOW && !st.ritual_done && st.scene == SC_NONE && !g_hero.busy() &&
        dist(g_hero.pos, g_world.arena) < g_world.arena_r * 0.72f) {
      start_scene(SC_RITUAL);
    }
    if (g.mode == MODE_SCENE) scene_step(dt);
    if (g.mode == MODE_ENDING) ending_step(dt);
  } else {
    g_hero.update({0, 0}, false, g_world, g_fx, dt);
    g_hero.face = {0, 1};
    g_hero.view = VIEW_FRONT;
    g_flint.face_camera = true;
    news = g_flint.update(g_hero, g_world, g_actors, g_fx, false, dt);
  }
  if (news.caught_fire || news.goblins_hit) g.shake = max(g.shake, 2.5f);
  if (news.landed_sad) g.shake = max(g.shake, 1.2f);
  update_blockers(dt);
  region_lines(news, an);
  people_step(dt);
  g.pointer_t = max(0.0f, g.pointer_t - dt);
  // Flint's own small talk, now and then, when nothing else is going on
  if (g.mode == MODE_PLAY) {
    g.chat_t += dt;
    if (g.chat_t > 40 && g_lines.cooldown <= 0 && !g_boss.fighting() && !g_hero.busy() && g_actors.chasing_goblin(g_hero.pos, 300) < 0) {
      Facts f = make_facts(-1);
      Line l;
      g_brain.flint(f, FT_IDLE, "", l);
      say(l.text, 3.6f);
    }
  }

  emit_from_props(dt);
  g_fx.update(dt, g_r.view);

  // where to go next: a gentle arrow when the hero stands still
  if (g.mode == MODE_PLAY) {
    bool still = length(g_hero.vel) < 6 && !g_hero.busy();
    g.idle_t = still ? g.idle_t + dt : 0;
    bool far = g.has_objective && dist(g.objective, g_hero.pos) > 230;
    bool pointed = g.pointer_t > 0 && dist(g.pointer, g_hero.pos) > 50;
    g.arrow_k = damp(g.arrow_k, ((g.idle_t > 1.6f && far) || pointed) ? 1.0f : 0.0f, 6, dt);
  } else {
    g.arrow_k = damp(g.arrow_k, 0, 8, dt);
  }

  if (g.mode != MODE_ENDING) g.zoom_k = damp(g.zoom_k, g_boss.fighting() ? 0.86f : g.mode == MODE_TALK ? 1.22f : 1.0f, 1.5f, dt);
  g.zoom = damp(g.zoom, target_zoom(), g.mode == MODE_ENDING ? 1.0f : 3.0f, dt);
  f32 cam_rate = g.mode == MODE_TITLE ? 2.5f : (g.mode == MODE_SCENE || g.mode == MODE_ENDING || g.mode == MODE_TALK) ? 2.2f : 5.5f;
  g.cam = clamp_cam(damp(g.cam, camera_goal(), cam_rate, dt), g.zoom);
  g.shake = max(0.0f, g.shake - dt * 8);
}

/** The lair: Malgrin's great rune circle, five pedestals, and the ingredients on them. */
void draw_lair(f32 time) {
  if (!g_world.has_arena) return;
  const Vec2 c = g_world.arena;
  const Color hexc{0.72f, 0.42f, 1.0f, 1}, gold{1.0f, 0.85f, 0.45f, 1};
  f32 rk = st.ritual_k;
  Color ground = mix(Color{0.35f, 0.18f, 0.5f, 0.6f}, Color{0.9f, 0.75f, 0.4f, st.healed ? 0.3f : 0.6f}, rk);
  g_r.draw({.id = S.game.rune, .pos = c, .layer = LAYER_GROUND, .scale = {6.6f, 6.6f}, .color = ground});
  f32 pulse = 0.75f + 0.25f * sinf(time * 1.8f);
  // brightest while the spell is being cast, then a steady warm glow through the fight
  f32 cast = st.scene == SC_RITUAL && st.beat == 2 ? 0.55f : 0.3f;
  Color glow = st.healed ? gold.with_alpha(0.18f) : mix(hexc.with_alpha(0.3f * pulse), gold.with_alpha(cast * pulse), rk);
  g_r.draw({.id = S.game.rune, .pos = c, .layer = LAYER_GLOW, .rot = 0, .scale = {6.6f, 6.6f}, .color = glow, .add = 0.8f});
  if (rk > 0 && !st.healed) g_r.light(c, 220, gold, 0.3f * rk);
  for (int i = 0; i < INGREDIENT_COUNT; i++) {
    Vec2 p = pedestal_pos(i);
    g_r.draw({.id = S.game.pedestal, .pos = p, .sort_y = p.y, .layer = LAYER_WORLD, .scale = {0.72f, 0.72f}});
    if (st.placed_t < 0) continue;
    f32 land = 0.5f + i * 0.45f;
    f32 k = saturate((st.placed_t - (land - 0.5f)) / 0.5f);
    if (k <= 0) continue;
    Vec2 from = g_hero.pos + Vec2{0, -20};
    Vec2 to = p + Vec2{0, -16 + sinf(time * 2 + i) * 2};
    Vec2 at = lerp(from, to, smoothstep(0.0f, 1.0f, k)) - Vec2{0, sinf(k * PI) * 40};
    g_r.draw({.id = S.soft, .pos = at + Vec2{0, -14}, .layer = LAYER_GLOW, .scale = {0.9f, 0.9f}, .color = gold.with_alpha(0.3f), .add = 1});
    g_r.draw({.id = S.game.ingredient[i], .pos = at, .sort_y = p.y + 1, .layer = LAYER_WORLD, .scale = {0.75f, 0.75f}});
    if (!st.healed) g_r.light(at + Vec2{0, -14}, 70, gold, 0.25f);
    // beams up into Flint while the spell is cast
    if (st.scene == SC_RITUAL && st.beat == 2 && !st.hex_broken) {
      Vec2 a = to + Vec2{0, -14}, b = g_flint.pos + Vec2{0, -18 - g_flint.z};
      Vec2 d = b - a;
      f32 len = length(d);
      f32 bk = saturate(st.beat_t * 1.5f) * (0.7f + 0.3f * sinf(time * 20 + i));
      g_r.draw({.id = S.white, .pos = (a + b) * 0.5f, .layer = LAYER_GLOW, .rot = atan2f(d.y, d.x), .scale = {len / 4.0f, 0.7f}, .color = Color{1.0f, 0.95f, 0.75f, bk}, .add = 1});
      g_r.draw({.id = S.white, .pos = (a + b) * 0.5f, .layer = LAYER_GLOW, .rot = atan2f(d.y, d.x), .scale = {len / 4.0f, 2.2f}, .color = gold.with_alpha(0.35f * bk), .add = 1});
    }
  }
}

void render(f32 alpha) {
  Vec2 cam = lerp(g.cam_prev, g.cam, alpha);
  f32 hw = g_input.view_w * 0.5f / g.zoom, hh = g_input.view_h * 0.5f / g.zoom;
  g_r.begin(Rect{cam.x - hw, cam.y - hh, cam.x + hw, cam.y + hh}.grow(60));
  g.hero_at = lerp(g_hero.prev_pos, g_hero.pos, alpha);
  g.flint_at = lerp(g_flint.prev_pos, g_flint.pos, alpha);
  Vec2 reveal[2] = {g.hero_at + Vec2{0, -20}, g.flint_at + Vec2{0, -12}};
  g_world.draw(g_r, g.time, reveal, 2);
  draw_lair(g.time);
  g_actors.draw(g_r, alpha, g.time);
  draw_people(g.time);
  g_hero.draw(g_r, g.hero_at);
  g_flint.draw(g_r, g.flint_at, g.time);
  g_boss.draw(g_r, alpha, g.time);
  g_fx.draw(g_r, g.time);
  if (g.arrow_k > 0.02f) {
    Vec2 d = normalize((g.pointer_t > 0 ? g.pointer : g.objective) - g.hero_at);
    f32 pulse = 1.0f + 0.12f * sinf(g.time * 5);
    g_r.draw({.id = S.game.arrow, .pos = g.hero_at + Vec2{0, -14} + d * (40 + 4 * sinf(g.time * 5)), .layer = LAYER_GLOW, .rot = atan2f(d.y, d.x),
              .scale = {pulse, pulse}, .color = {1, 1, 1, g.arrow_k}});
  }
  if (g.flash > 0.01f) {
    g_r.view = Rect{cam.x - hw, cam.y - hh, cam.x + hw, cam.y + hh}.grow(200);
    g_r.draw({.id = S.white, .pos = cam, .layer = LAYER_GLOW, .scale = {hw * 0.6f, hh * 0.6f}, .color = g.flash_col.with_alpha(min(1.0f, g.flash) * 0.85f), .add = 1});
  }
  g_r.finish();

  const Look lk = region_look();
  f32 sx = g.shake > 0 ? sinf(g.time * 91) * g.shake : 0, sy = g.shake > 0 ? cosf(g.time * 77) * g.shake : 0;
  g_info[FI_CAM_X] = cam.x;
  g_info[FI_CAM_Y] = cam.y;
  g_info[FI_ZOOM] = g.zoom;
  g_info[FI_TIME] = g.time;
  g_info[FI_AMB_R] = lk.ambient.r;
  g_info[FI_AMB_G] = lk.ambient.g;
  g_info[FI_AMB_B] = lk.ambient.b;
  g_info[FI_EXPOSURE] = 1.0f;
  g_info[FI_MAP_W] = (f32)g_world.w;
  g_info[FI_MAP_H] = (f32)g_world.h;
  g_info[FI_TILE] = TS;
  g_info[FI_MAP_VERSION] = (f32)g_world.ground_version + (f32)g.region * 1000.0f;
  g_info[FI_ATLAS_VERSION] = (f32)g_atlas.version;
  g_info[FI_LIT_QUADS] = (f32)g_r.lit_quads;
  g_info[FI_ALL_QUADS] = (f32)g_r.quad_count;
  g_info[FI_LIGHTS] = (f32)g_r.light_count;
  g_info[FI_FADE] = g.fade;
  g_info[FI_BLOOM] = lk.bloom;
  g_info[FI_VIGNETTE] = lk.vignette;
  g_info[FI_SATURATION] = lk.saturation;
  g_info[FI_GRADE_R] = lk.grade.r;
  g_info[FI_GRADE_G] = lk.grade.g;
  g_info[FI_GRADE_B] = lk.grade.b;
  g_info[FI_REGION] = (f32)g.region;
  g_info[FI_SHAKE_X] = sx;
  g_info[FI_SHAKE_Y] = sy;
  f32 base_wind = g.region == R_SKYREACH ? 0.35f : 0.0f;
  g_info[FI_WIND] = sinf(g.time * 0.21f) * 0.3f + base_wind + g.gust_k * 1.3f * (g.gust_dir.y >= 0 ? 1.0f : -1.0f);

  // who's talking, and where their heads are (for the speech bubbles)
  g_speakers[0] = g.flint_at.x;
  g_speakers[1] = g.flint_at.y - 46 - g_flint.z;
  Vec2 mh = g_boss.head();
  g_speakers[2] = mh.x;
  g_speakers[3] = mh.y - 18;
  for (int i = 0; i < g_people_n; i++) {
    g_speakers[4 + i * 2] = g_people[i].pos.x;
    g_speakers[5 + i * 2] = g_people[i].pos.y - 54;
  }

  g_debug[0] = g_hero.pos.x;
  g_debug[1] = g_hero.pos.y;
  g_debug[2] = g_flint.pos.x;
  g_debug[3] = g_flint.pos.y;
  g_debug[4] = (f32)(g_world.gates.count ? g_world.blockers_left(0) : 0);
  g_debug[5] = (f32)g_flint.state;
  g_debug[6] = (f32)g_r.draw_count;
  g_debug[7] = (f32)g_r.culled;
  g_debug[8] = (f32)g_fx.count;
  g_debug[9] = length(g_hero.vel);
  g_debug[10] = (f32)g_world.prop_count;
  g_debug[11] = (f32)g_atlas.count;
  g_debug[12] = (f32)g_atlas.used_rows();
  g_debug[13] = g_atlas.overflow ? 1.0f : 0.0f;
  g_debug[14] = (f32)(g_level.peak >> 10);
  g_debug[15] = (f32)(g_perm.used >> 10);
  g_debug[16] = (f32)g.mode;
  g_debug[17] = (f32)g_hero.view;
  g_debug[18] = g_hero.flip;
  g_debug[19] = g_flint.z;
  g_debug[22] = (f32)g.region;
  g_debug[23] = (f32)g_hero.hearts;
  g_debug[24] = (f32)g_actors.goblins_left();
  g_debug[25] = (f32)g_actors.count;
  int open = 0;
  for (const Gate& gt : g_world.gates) open += gt.open;
  g_debug[26] = (f32)open;
  g_debug[27] = (f32)g.collected;
  g_debug[28] = (f32)g_r.light_count;
  g_debug[29] = (f32)g_world.gates.count;
  g_debug[30] = (f32)g_boss.state;
  g_debug[31] = (f32)g_boss.hits;
  g_debug[32] = (f32)st.scene;
  g_debug[33] = (f32)st.beat;
  g_debug[34] = g_boss.pos.x;
  g_debug[35] = g_boss.pos.y;
}

}  // namespace

SKY_EXPORT(game_init) int game_init(u32 seed) {
  usize perm_bytes = 40u << 20;
  g_perm.init(g_heap, perm_bytes);
  g_level.init(g_heap + perm_bytes, sizeof(g_heap) - perm_bytes);
  g_atlas.init(g_perm);
  bake_shared(g_atlas);
  g_shared_mark = g_atlas.mark_shared();
  g_r.init(g_perm, &g_atlas);
  g_fx.init(g_perm);
  StrBuf s;
  s.add("baked ").add_int(g_atlas.count).add(" shared sprites, atlas rows used: ").add_int(g_atlas.used_rows()).add(g_atlas.overflow ? " (OVERFLOW)" : "");
  log(s);
  g.seed = seed;
  g.zoom = target_zoom();
  load_region(R_MEADOW);
  g_audio.play(SONG_TITLE);
  g_flint.fly_cd = 5.0f;
  event(EV_READY);
  event(EV_MODE, "", 0);
  return 1;
}

SKY_EXPORT(game_input) Input* game_input() { return &g_input; }
SKY_EXPORT(game_info) f32* game_info() { return g_info; }
SKY_EXPORT(game_debug) f32* game_debug() { return g_debug; }
SKY_EXPORT(game_palette) f32* game_palette() { return g_palette; }
SKY_EXPORT(game_speakers) f32* game_speakers() { return g_speakers; }
SKY_EXPORT(game_verts) Vertex* game_verts() { return g_r.verts; }
SKY_EXPORT(game_lights) Light* game_lights() { return g_r.lights; }
SKY_EXPORT(game_atlas) u8* game_atlas() { return g_atlas.rgba; }
SKY_EXPORT(game_atlas_size) int game_atlas_size() { return ATLAS_SIZE; }
SKY_EXPORT(game_ground) u32* game_ground() { return g_world.ground.cells; }

SKY_EXPORT(game_frame) void game_frame(f32 dt) {
  dt = clamp(dt, 0.0f, 0.1f);
  g.pending |= g_input.pressed;
  g_input.pressed = 0;
  g.acc += dt;
  int steps = 0;
  while (g.acc >= STEP && steps < 12) {
    sim_step(STEP);
    g.acc -= STEP;
    steps++;
  }
  if (steps == 12) g.acc = 0;
  render(g.acc / STEP);
}

SKY_EXPORT(game_set_look) void game_set_look(int look) {
  look = clamp(look, 0, 1);
  if (look == g.look) return;
  g.look = look;
  Vec2 p = g_hero.pos;
  int hearts = g_hero.hearts;
  g_hero.reset(p, look);
  g_hero.hearts = hearts;
  g_fx.sparkle(p + Vec2{0, -24}, 16, {1.0f, 0.95f, 0.7f, 1}, 14);
  g_fx.ring(p + Vec2{0, -20}, {1.0f, 0.95f, 0.75f, 0.8f}, 2.4f);
}

/** A new journey from the meadow, opening with the prologue. */
SKY_EXPORT(game_start) void game_start() {
  if (g.mode != MODE_TITLE) return;
  g.collected = 0;
  if (g.region != R_MEADOW) load_region(R_MEADOW);
  g_flint.fly_cd = 18.0f;
  g_flint.face_camera = false;
  start_scene(SC_INTRO);
}

/** Picks up a saved journey: the land you reached, with the ingredients you'd found. */
SKY_EXPORT(game_continue) void game_continue(int region, int mask) {
  if (g.mode != MODE_TITLE) return;
  region = clamp(region, 0, R_COUNT - 1);
  g.collected = (u32)mask & ((1u << INGREDIENT_COUNT) - 1);
  load_region((Region)region);
  g.fade = 1.0f;
  enter_play();
}

// ---------------------------------------------------------------- conversations, from the page
SKY_EXPORT(game_text_buf) char* game_text_buf() { return g_text; }
/** The player picked one of the suggested questions. */
SKY_EXPORT(game_talk_choose) void game_talk_choose(int intent) { if (g.mode == MODE_TALK) respond((Intent)clamp(intent, 0, IN_COUNT - 1)); }
/** The player typed something (written into game_text_buf first). */
SKY_EXPORT(game_talk_say) void game_talk_say(int len) {
  if (g.mode != MODE_TALK) return;
  len = clamp(len, 0, (int)sizeof(g_text) - 1);
  g_text[len] = 0;
  char said[200];
  int n = min(len, 199);
  memcpy(said, g_text, n);
  said[n] = 0;
  respond(g_brain.classify(said, n), said);
}
// ---------------------------------------------------------------- sound, pause and settings
SKY_EXPORT(game_audio_init) void game_audio_init(int sample_rate) { g_audio.init(g_perm, clamp(sample_rate, 8000, 96000)); }
/** Renders the next block of sound (interleaved stereo) and returns where it is. */
SKY_EXPORT(game_audio_render) f32* game_audio_render(int frames) {
  frames = clamp(frames, 0, 4096);
  if (g_audio.ready) g_audio.render(g_audio_buf, frames);
  else memset(g_audio_buf, 0, sizeof(f32) * frames * 2);
  return g_audio_buf;
}
SKY_EXPORT(game_audio_volume) void game_audio_volume(f32 music, f32 effects) {
  g_audio.music_volume = clamp(music, 0.0f, 1.0f) * 0.7f;
  g_audio.sfx_volume = clamp(effects, 0.0f, 1.0f);
}
SKY_EXPORT(game_sfx) void game_sfx(int id) { if (id >= 0 && id < SFX_COUNT) g_audio.sfx((Sfx)id, 0.8f); }
SKY_EXPORT(game_set_paused) void game_set_paused(int p) {
  bool on = p != 0 && g.mode == MODE_PLAY;
  if (on == g.paused) return;
  g.paused = on;
  event(EV_PAUSE, "", on ? 1.0f : 0.0f);
}

/** The page's model is ready (1) or not (0). */
SKY_EXPORT(game_llm_status) void game_llm_status(int ready) { g_live = ready != 0; }
SKY_EXPORT(game_llm_reply) void game_llm_reply(int id, int len) {
  len = clamp(len, 0, (int)sizeof(g_text) - 1);
  g_text[len] = 0;
  llm_answer(id, g_text, len);
}
SKY_EXPORT(game_llm_fail) void game_llm_fail(int id) { llm_answer(id, nullptr, 0); }
SKY_EXPORT(game_talk_close) void game_talk_close() { if (g.mode == MODE_TALK) close_talk(); }
/** Runs tricky lines through the guard for the dev panel. */
SKY_EXPORT(game_guard_selftest) void game_guard_selftest() { g_brain.self_test(make_facts(g_people_n ? 0 : -1)); }
/** Tests: where someone stands (debug slots 20..21). */
SKY_EXPORT(game_debug_person) int game_debug_person(int i) {
  if (i >= g_people_n) return 0;
  g_debug[20] = g_people[i].pos.x;
  g_debug[21] = g_people[i].pos.y;
  return g_people[i].id + 1;
}
/** Tests: walk up to someone and start talking. */
SKY_EXPORT(game_debug_talk) int game_debug_talk(int i) {
  if (i >= g_people_n) return 0;
  g_hero.pos = g_hero.prev_pos = g_people[i].pos + Vec2{-30, 6};
  g.cam = g.cam_prev = clamp_cam(camera_goal(), g.zoom);
  open_talk(i);
  return 1;
}

/** The page's "continue" tap on a story card. */
SKY_EXPORT(game_story_next) void game_story_next() { st.next = true; }
SKY_EXPORT(game_story_skip) void game_story_skip() { if (st.scene != SC_NONE) end_scene(); }

SKY_EXPORT(game_new_layout) void game_new_layout(u32 seed) {
  g.seed = seed;
  load_region(g.region);
  if (g.mode != MODE_TITLE) enter_play();
}

// ---------------------------------------------------------------- test helpers
SKY_EXPORT(game_debug_teleport) void game_debug_teleport(f32 x, f32 y) {
  g_hero.pos = g_hero.prev_pos = {x, y};
  g_flint.pos = g_flint.prev_pos = {x - 30, y + 10};
  g_flint.trail.clear();
  g.cam = g.cam_prev = clamp_cam(camera_goal(), g.zoom);
}

/** Runs the simulation forward without drawing. */
SKY_EXPORT(game_debug_step) void game_debug_step(int steps) {
  for (int i = 0; i < steps; i++) sim_step(STEP);
}

SKY_EXPORT(game_debug_zoom) void game_debug_zoom(f32 z) { g_zoom_override = z; g.zoom = z > 0 ? z : g.zoom; }

SKY_EXPORT(game_debug_fly) void game_debug_fly() { g_flint.fly_cd = 0; g_flint.idle_t = 5; }

/** Jumps straight to a land, in play. */
SKY_EXPORT(game_debug_region) void game_debug_region(int region, int mask) {
  if (st.scene != SC_NONE) { st.scene = SC_NONE; event(EV_STORY, ""); }
  g.collected = (u32)mask;
  load_region((Region)clamp(region, 0, R_COUNT - 1));
  g.fade = 0;
  enter_play();
}

/** Writes a place into debug slots 20..21: 0 gate n, 1 shrine, 2 camp n, 3 waystone n, 4 spot n, 5 arena. */
SKY_EXPORT(game_debug_place) int game_debug_place(int kind, int n) {
  Vec2 p;
  switch (kind) {
    case 0: if (n >= g_world.gates.count) return 0; p = g_world.gates[n].pos; break;
    case 1: p = g_world.shrine; break;
    case 2: if (n >= g_world.camps.count) return 0; p = g_world.camps[n]; break;
    case 3: if (n >= g_world.waystones.count) return 0; p = g_world.waystones[n]; break;
    case 4: if (n >= g_world.spots.count) return 0; p = g_world.spots[n].pos; break;
    default: if (!g_world.has_arena) return 0; p = g_world.arena; break;
  }
  g_debug[20] = p.x;
  g_debug[21] = p.y;
  return 1;
}

SKY_EXPORT(game_debug_hurt) void game_debug_hurt() {
  if (g_hero.hurt(g_hero.pos + Vec2{20, 0}, g_fx)) hud();
}

SKY_EXPORT(game_debug_heal) void game_debug_heal() { g_hero.hearts = Hero::MAX_HEARTS; g_hero.invuln = 99; hud(); }

SKY_EXPORT(game_debug_gust) void game_debug_gust() { g.gust_cd = 0; }

/** Opens every gate (tests). */
SKY_EXPORT(game_debug_open_gates) void game_debug_open_gates() {
  for (int i = 0; i < g_world.prop_count; i++) {
    Prop& p = g_world.props[i];
    if (p.kind == PK_BLOCKER && !(p.flags & PF_BURNT)) { p.flags |= PF_BURNING; p.burn_t = 1.5f; }
  }
}

/** Writes the position of the n-th unburnt blocker into debug slots 20..21. */
SKY_EXPORT(game_debug_bramble) int game_debug_bramble(int n) {
  for (int i = 0; i < g_world.prop_count; i++) {
    const Prop& p = g_world.props[i];
    if (p.kind != PK_BLOCKER || (p.flags & PF_BURNT)) continue;
    if (n-- == 0) { g_debug[20] = p.pos.x; g_debug[21] = p.pos.y; return 1; }
  }
  return 0;
}
