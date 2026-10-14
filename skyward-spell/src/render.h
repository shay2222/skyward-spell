// render.h - collects sprite draws and lights each frame, sorts them, and builds the vertex
// buffer the browser uploads to WebGL in one draw call.
#pragma once
#include "atlas.h"

namespace sky {

/** 24 bytes per vertex, interleaved; layout mirrored in docs/host.js. */
struct Vertex {
  f32 x, y;   // world position
  f32 u, v;   // atlas texture coordinate
  u32 color;  // RGBA8 tint
  u32 fx;     // byte 0: wind sway weight, byte 1: additive, byte 2: white flash
};

struct Light {
  f32 x, y, radius, intensity;
  f32 r, g, b, softness;
};

/**
 * Layers draw in this order. Ground decals are not y-sorted. World things are sorted by the
 * y of their feet so characters walk behind trees and in front of flowers. Glow is unlit and
 * added after lighting, so fire and magic stay bright at night.
 */
enum Layer : u8 { LAYER_GROUND = 0, LAYER_WORLD = 1, LAYER_TOP = 2, LAYER_GLOW = 3 };

struct Draw {
  u16 id = 0;
  Vec2 pos;
  f32 sort_y = 0;
  Layer layer = LAYER_WORLD;
  f32 rot = 0;
  Vec2 scale = {1, 1};
  Color color = {1, 1, 1, 1};
  f32 sway = 0;   // 0..1, how much the top of the sprite moves in the wind
  f32 add = 0;    // 0 = normal, 1 = fully additive (for glows)
  f32 flash = 0;  // 0..1 white flash
};

constexpr int MAX_DRAWS = 20000;
constexpr int MAX_LIGHTS = 256;

struct Renderer {
  const Atlas* atlas = nullptr;
  Draw* draws = nullptr;
  u64* keys = nullptr;
  u64* keys_tmp = nullptr;
  Vertex* verts = nullptr;
  int draw_count = 0;
  int quad_count = 0;
  int lit_quads = 0;
  Light lights[MAX_LIGHTS];
  int light_count = 0;
  Rect view;  // world-space rectangle currently on screen (plus margin), for culling
  int culled = 0;

  void init(Arena& a, const Atlas* at) {
    atlas = at;
    draws = a.make<Draw>(MAX_DRAWS);
    keys = a.make<u64>(MAX_DRAWS);
    keys_tmp = a.make<u64>(MAX_DRAWS);
    verts = a.make<Vertex>(MAX_DRAWS * 4);
  }

  void begin(Rect view_rect) {
    view = view_rect;
    draw_count = 0;
    light_count = 0;
    culled = 0;
  }

  bool visible(Vec2 p, f32 radius) const {
    return p.x + radius > view.x0 && p.x - radius < view.x1 && p.y + radius > view.y0 && p.y - radius < view.y1;
  }

  void draw(const Draw& d) {
    const SpriteDef& s = (*atlas)[d.id];
    f32 r = max(s.w, s.h) * max(absf(d.scale.x), absf(d.scale.y));
    if (!visible(d.pos, r)) { culled++; return; }
    if (draw_count >= MAX_DRAWS) return;
    draws[draw_count++] = d;
  }

  void light(Vec2 p, f32 radius, Color c, f32 intensity, f32 softness = 1.0f) {
    if (light_count >= MAX_LIGHTS || !visible(p, radius)) return;
    lights[light_count++] = {p.x, p.y, radius, intensity, c.r, c.g, c.b, softness};
  }

  void finish();
};

}  // namespace sky
