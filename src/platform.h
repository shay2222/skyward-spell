// platform.h - the boundary between the C++ game and the browser.
//
// The browser side (docs/host.js) is deliberately thin: it opens a WebGL2 canvas, forwards
// keyboard and gamepad input into the Input struct below, calls game_frame() once per display
// frame, and draws the vertex, light and map buffers the game hands back. All decisions are
// made in C++.
#pragma once
#include "core.h"

#define SKY_EXPORT(name) extern "C" __attribute__((export_name(#name), visibility("default")))
#define SKY_IMPORT(name) __attribute__((import_module("env"), import_name(#name)))

extern "C" {
SKY_IMPORT(js_log) void js_log(const char* text, int len);
/** Game-to-page events (UI updates, sounds later). `kind` is one of sky::Event. */
SKY_IMPORT(js_event) void js_event(int kind, const char* text, int len, float a, float b);
/** Hands a prompt to the page's language model (docs/llm.js). The answer comes back later
 *  through game_llm_reply(id, len), or game_llm_fail(id). */
SKY_IMPORT(js_llm_ask) void js_llm_ask(int id, const char* prompt, int len);
}

namespace sky {

inline int cstr_len(const char* s) { int n = 0; while (s[n]) n++; return n; }
inline void log(const char* s) { js_log(s, cstr_len(s)); }
inline void log(const StrBuf& s) { js_log(s.buf, s.len); }
inline void event(int kind, const char* s = "", f32 a = 0, f32 b = 0) { js_event(kind, s, cstr_len(s), a, b); }

enum Button : u32 {
  BTN_UP = 1u << 0,
  BTN_DOWN = 1u << 1,
  BTN_LEFT = 1u << 2,
  BTN_RIGHT = 1u << 3,
  BTN_DASH = 1u << 4,
  BTN_FIRE = 1u << 5,
  BTN_INTERACT = 1u << 6,
  BTN_PAUSE = 1u << 7,
};

enum Event : int {
  EV_READY = 1,       // engine finished baking art
  EV_HINT = 2,        // short on-screen hint text
  EV_TOAST = 3,       // celebratory message
  EV_FLINT_SAYS = 4,  // Flint's speech bubble text (a = seconds to show)
  EV_HUD = 5,         // a = hearts, b = max hearts
  EV_SPELL = 6,       // a = bitmask of ingredients found, text = the one just found (or "")
  EV_BANNER = 7,      // text = "Land|Chapter", a = region index
  EV_SAVE = 8,        // text = "region,look,mask" for the page to keep in localStorage
  EV_OBJECTIVE = 9,   // text = the current goal, shown small under the land's name
  EV_MODE = 10,       // a = 0 title, 1 playing, 2 travelling, 3 story scene, 4 ending
  EV_SAYS = 11,       // someone other than Flint speaks: text = "Name|line", a = seconds, b = speaker slot
  EV_STORY = 12,      // story card: text = "Speaker|line" ("" hides it), a = 1 when waiting for the player
  EV_ENDING = 13,     // the journey is over: show the ending card
  EV_BOSS = 14,       // Malgrin's health: a = hits taken, b = hits needed (b = 0 hides the bar)
  EV_FLASH = 15,      // a = 1 for a white flash, 2 for a purple one (page-side colour wash)
  EV_TALK_OPEN = 16,  // a conversation starts: text = "Name|role", a = person
  EV_TALK_LINE = 17,  // text = "Speaker|line", a = where it came from (0 offline, 1 live model, 2 safe fallback)
  EV_TALK_CHOICES = 18,  // text = "intent:label;intent:label;..."
  EV_TALK_CLOSE = 19, // a = seconds to wait before closing
  EV_TALK_WAIT = 20,  // a = 1 while waiting for the live model
  EV_DEVLOG = 21,     // one dialogue decision for the dev panel: "who|intent|source|verdict|notes|line"
  EV_PROMPT = 22,     // the "press E to talk" prompt: text = label ("" hides it)
  EV_PAUSE = 23,      // a = 1 paused, 0 playing
};

/** Written by the browser every frame before game_frame() runs. */
struct Input {
  f32 move_x = 0, move_y = 0;  // gamepad stick, -1..1 (keyboard is folded in by C++)
  u32 held = 0;                // Button bits currently down
  u32 pressed = 0;             // Button bits pressed since the previous frame
  f32 view_w = 1280, view_h = 720;  // drawing buffer size in device pixels
  f32 dpr = 1;
  f32 css_h = 720;             // canvas height in CSS pixels, for zoom decisions
};

/** Read by the browser after game_frame(); indices are mirrored in docs/host.js. */
enum FrameSlot : int {
  FI_CAM_X, FI_CAM_Y, FI_ZOOM, FI_TIME,
  FI_AMB_R, FI_AMB_G, FI_AMB_B, FI_EXPOSURE,
  FI_MAP_W, FI_MAP_H, FI_TILE, FI_MAP_VERSION,
  FI_ATLAS_VERSION, FI_LIT_QUADS, FI_ALL_QUADS, FI_LIGHTS,
  FI_FADE, FI_BLOOM, FI_VIGNETTE, FI_SATURATION,
  FI_GRADE_R, FI_GRADE_G, FI_GRADE_B, FI_REGION,
  FI_SHAKE_X, FI_SHAKE_Y, FI_WIND, FI_COUNT_
};

}  // namespace sky
