#include "render.h"

namespace sky {

static u32 pack_fx(f32 sway, f32 add, f32 flash) {
  auto q = [](f32 v) { return (u32)(saturate(v) * 255.0f + 0.5f); };
  return q(sway) | (q(add) << 8) | (q(flash) << 16);
}

/** LSD radix sort on 64-bit keys, 8 bits per pass. Stable, so equal keys keep submission order. */
static void radix_sort(u64* a, u64* tmp, int n, int passes) {
  u32 counts[256];
  for (int pass = 0; pass < passes; pass++) {
    int shift = pass * 8;
    memset(counts, 0, sizeof(counts));
    for (int i = 0; i < n; i++) counts[(a[i] >> shift) & 255]++;
    u32 sum = 0;
    for (int i = 0; i < 256; i++) { u32 c = counts[i]; counts[i] = sum; sum += c; }
    for (int i = 0; i < n; i++) tmp[counts[(a[i] >> shift) & 255]++] = a[i];
    u64* t = a; a = tmp; tmp = t;
  }
  // an even number of passes leaves the result back in the caller's array
}

void Renderer::finish() {
  // key: layer (top byte) | sort y (fixed point) | draw index (low 24 bits)
  for (int i = 0; i < draw_count; i++) {
    const Draw& d = draws[i];
    u64 y = d.layer == LAYER_WORLD ? (u64)clamp((d.sort_y + 4096.0f) * 16.0f, 0.0f, 16777215.0f) : 0;
    keys[i] = ((u64)d.layer << 48) | (y << 24) | (u64)i;
  }
  radix_sort(keys, keys_tmp, draw_count, 8);

  const f32 inv = 1.0f / ATLAS_SIZE;
  quad_count = 0;
  lit_quads = 0;
  for (int k = 0; k < draw_count; k++) {
    const Draw& d = draws[keys[k] & 0xffffff];
    const SpriteDef& s = (*atlas)[d.id];
    if (d.layer != LAYER_GLOW) lit_quads++;

    // corners relative to the pivot, then scale, rotate, translate
    Vec2 c[4] = {{-s.px, -s.py}, {s.w - s.px, -s.py}, {s.w - s.px, s.h - s.py}, {-s.px, s.h - s.py}};
    f32 cs = 1, sn = 0;
    if (d.rot != 0) { cs = cosf(d.rot); sn = sinf(d.rot); }
    u32 col = pack_rgba8(d.color);
    u32 fx_top = pack_fx(d.sway, d.add, d.flash);
    u32 fx_bottom = pack_fx(0, d.add, d.flash);
    f32 u0 = s.tx * inv, v0 = s.ty * inv, u1 = (s.tx + s.tw) * inv, v1 = (s.ty + s.th) * inv;
    f32 us[4] = {u0, u1, u1, u0};
    f32 vs[4] = {v0, v0, v1, v1};
    Vertex* v = verts + quad_count * 4;
    for (int i = 0; i < 4; i++) {
      Vec2 p = c[i] * d.scale;
      Vec2 r{p.x * cs - p.y * sn, p.x * sn + p.y * cs};
      v[i] = {d.pos.x + r.x, d.pos.y + r.y, us[i], vs[i], col, i < 2 ? fx_top : fx_bottom};
    }
    quad_count++;
  }
}

}  // namespace sky
