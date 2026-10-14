// atlas.h - packs every baked sprite into one texture so a whole frame draws in one batch.
#pragma once
#include "paint.h"

namespace sky {

/** Where a sprite lives in the atlas and how it sits in the world. */
struct SpriteDef {
  u16 tx, ty, tw, th;  // texel rectangle
  f32 w, h;            // size in world units
  f32 px, py;          // pivot, measured from the top-left corner, in world units
};

constexpr int ATLAS_SIZE = 2048;
constexpr int MAX_SPRITES = 1536;
constexpr int SCRATCH_PX = 640 * 640;

struct Atlas {
  u8* rgba = nullptr;            // ATLAS_SIZE * ATLAS_SIZE * 4, premultiplied alpha
  paint::Px* scratch = nullptr;  // float canvas for one sprite at a time
  SpriteDef sprites[MAX_SPRITES];
  int count = 0;
  int cx = 0, cy = 0, row_h = 0;
  int pad = 3;
  u32 version = 0;
  bool overflow = false;
  u32 dither_seed = 1;

  void init(Arena& a) {
    rgba = (u8*)a.alloc((usize)ATLAS_SIZE * ATLAS_SIZE * 4, 64);
    memset(rgba, 0, (usize)ATLAS_SIZE * ATLAS_SIZE * 4);
    scratch = a.make<paint::Px>(SCRATCH_PX);
    count = 0;
    cx = cy = row_h = 0;
  }

  const SpriteDef& operator[](int id) const { return sprites[id]; }

  /**
   * Bakes one sprite. `local` is the area to paint, in world units relative to the sprite's
   * pivot (the point that sits on the entity's position). `scale` is texels per world unit.
   * Returns the sprite id.
   */
  template <class F>
  u16 bake(Rect local, f32 scale, const F& painter) {
    int tw = (int)ceilf((local.x1 - local.x0) * scale) + pad * 2;
    int th = (int)ceilf((local.y1 - local.y0) * scale) + pad * 2;
    if (tw * th > SCRATCH_PX || count >= MAX_SPRITES) { overflow = true; return 0; }
    if (cx + tw > ATLAS_SIZE) { cx = 0; cy += row_h; row_h = 0; }
    if (cy + th > ATLAS_SIZE) { overflow = true; return 0; }

    paint::Canvas cv;
    cv.px = scratch;
    cv.w = tw;
    cv.h = th;
    cv.scale = scale;
    cv.origin = {local.x0 - pad / scale, local.y0 - pad / scale};
    memset(scratch, 0, sizeof(paint::Px) * (usize)tw * th);
    painter(cv);

    // copy into the atlas with a touch of dithering so soft gradients don't band
    for (int y = 0; y < th; y++) {
      u8* dst = rgba + ((usize)(cy + y) * ATLAS_SIZE + cx) * 4;
      const paint::Px* src = scratch + (usize)y * tw;
      for (int x = 0; x < tw; x++) {
        f32 dz = (hash_unit(x + cx, y + cy, dither_seed) - 0.5f) * (1.0f / 255.0f);
        auto q = [&](f32 v) { return (u8)clamp((int)((v + dz) * 255.0f + 0.5f), 0, 255); };
        const paint::Px& s = src[x];
        u8 a = (u8)clamp((int)(s.a * 255.0f + 0.5f), 0, 255);
        dst[x * 4 + 0] = min(q(s.r), a);
        dst[x * 4 + 1] = min(q(s.g), a);
        dst[x * 4 + 2] = min(q(s.b), a);
        dst[x * 4 + 3] = a;
      }
    }

    SpriteDef& d = sprites[count];
    d.tx = (u16)cx; d.ty = (u16)cy; d.tw = (u16)tw; d.th = (u16)th;
    d.w = tw / scale; d.h = th / scale;
    d.px = -cv.origin.x; d.py = -cv.origin.y;
    cx += tw;
    row_h = max(row_h, th);
    version++;
    return (u16)count++;
  }

  int used_rows() const { return cy + row_h; }

  /** Where shared art ends. Each land bakes after this point and rewinds to it on the next load. */
  struct Mark { int count, cy; };
  Mark mark_shared() {
    cx = 0;
    cy += row_h;
    row_h = 0;
    return {count, cy};
  }
  void rewind(const Mark& m) {
    count = m.count;
    cx = 0;
    cy = m.cy;
    row_h = 0;
    overflow = false;
    memset(rgba + (usize)m.cy * ATLAS_SIZE * 4, 0, (usize)(ATLAS_SIZE - m.cy) * ATLAS_SIZE * 4);
    version++;
  }
};

}  // namespace sky
