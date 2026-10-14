// brain.h - the dialogue system: what the player is asking (intent), what is true right now
// (facts), a reply (from a live language model when one is ready, otherwise from the offline
// generator), and the guard: a logic layer that checks every line against the game before
// anyone sees it. Rejected lines are retried or replaced, and every decision is logged.
#pragma once
#include "npc.h"

namespace sky {

enum Intent : u8 {
  IN_GREET, IN_INGREDIENT, IN_DANGER, IN_PLACE, IN_SELF, IN_MALGRIN, IN_FLINT, IN_HELP, IN_WAY,
  IN_THANKS, IN_BYE, IN_CHAT, IN_OFFWORLD, IN_TRICK, IN_RUDE, IN_COUNT,
};
extern const char* const INTENT_NAMES[IN_COUNT];

/** What a line may make happen. The model can only *suggest* these; the game decides. */
enum Action : u8 { ACT_NONE, ACT_GIFT_HEART, ACT_SHOW_HEART, ACT_SHOW_GOAL };

enum Source : u8 { SRC_OFFLINE, SRC_LIVE, SRC_FALLBACK };

/** Everything true about the world that a speaker may mention. Built fresh for every line. */
struct Facts {
  int npc = -1;                    // -1 means Flint is speaking
  Region land = R_MEADOW;
  const char* hero = "Mira";
  const char* ingredient = "";     // this land's ("" in the Hollow)
  bool ingredient_taken = false;
  int collected = 0;               // of five
  const char* goal_dir = "east";   // the shrine (or Malgrin's lair)
  const char* goal_far = "a short walk";
  int gates_closed = 0;
  const char* gate_dir = "east";
  int goblins_near = 0;
  const char* camp_dir = "north";
  bool heart_known = false;
  const char* heart_dir = "north";
  int hearts = 4, max_hearts = 4;
  bool flint_flies = false, malgrin_beaten = false;
  int times_talked = 0;
  bool gift_left = false;
  u32 asked = 0;                   // intents already asked of this speaker
};

struct Line {
  char text[320];
  Intent intent = IN_CHAT;
  Action action = ACT_NONE;
  Source source = SRC_OFFLINE;
  int tries = 0;
  char notes[200];
};

struct Verdict {
  bool ok = true;
  char why[160];
};

/** Flint's own small talk: things worth a remark that aren't tutorials. */
enum FlintTopic : u8 { FT_IDLE, FT_AFTER_TALK, FT_POOF, FT_HEART, FT_GATE };

struct Brain {
  Rng rng{99, 7};
  u32 last_hash = 0;
  int lines = 0, rejected = 0, live = 0, fallbacks = 0;

  Intent classify(const char* text, int len) const;
  /** The offline generator: the speaker's own lines and the true facts, assembled with variety. */
  void compose(const Facts& f, Intent in, Line& out);
  /** The logic layer. Checks one candidate line against the facts. */
  Verdict guard(const Facts& f, Intent in, const char* text, Action act) const;
  /** Offline reply with the guard and a retry, ready to show. */
  void reply(const Facts& f, Intent in, Line& out);
  /** Checks a line from the live model; on failure, replaces it with an offline one. */
  void accept_live(const Facts& f, Intent in, const char* text, int len, Action act, Line& out);
  void flint(const Facts& f, FlintTopic topic, const char* about, Line& out);

  /** The instructions and facts handed to a live model (see docs/llm.js). */
  int build_prompt(const Facts& f, Intent in, const char* player, char* out, int cap) const;
  /** Pulls {"say": "...", "action": "..."} out of a model's answer. */
  static bool parse_reply(const char* json, int len, char* say, int cap, Action& act);

  /** Runs tricky lines through the guard to show what it catches (the dev panel's button). */
  void self_test(const Facts& f);
};

/** One entry for the dev panel's log. */
void brain_log(const char* who, Intent in, Source src, bool ok, const char* notes, const char* line);

}  // namespace sky
