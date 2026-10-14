// paint.h - a small vector painter that bakes the game's art into a texture atlas at startup.
//
// Every shape is a signed distance function (SDF): negative inside, positive outside, and the
// distance tells the painter how far a pixel is from the edge. That gives clean anti-aliasing,
// outlines (grow the shape and paint it darker underneath), soft shadows (paint with a wide
// feather), and "volume" shading that darkens toward the rim, all from the same few primitives.
#pragma once
#include "core.h"

namespace sky::paint {

/** Premultiplied RGBA. */
struct Px { f32 r = 0, g = 0, b = 0, a = 0; };

struct Canvas {
  Px* px = nullptr;
  int w = 0, h = 0;
  f32 scale = 1;       // pixels per world unit
  Vec2 origin;         // world position of pixel (0,0)'s top-left corner
};

enum class Blend { Over, Atop, Behind, Erase, Add };

// ---------------------------------------------------------------- shapes
inline Rect rect_around(Vec2 c, f32 r) { return {c.x - r, c.y - r, c.x + r, c.y + r}; }
inline Rect rect_union(const Rect& a, const Rect& b) { return {min(a.x0, b.x0), min(a.y0, b.y0), max(a.x1, b.x1), max(a.y1, b.y1)}; }

struct Circle {
  Vec2 c; f32 r;
  f32 sd(Vec2 p) const { return dist(p, c) - r; }
  Rect bounds() const { return rect_around(c, r); }
};

struct Ellipse {
  Vec2 c; Vec2 r; f32 rot = 0;
  f32 sd(Vec2 p) const {
    Vec2 q = p - c;
    if (rot != 0) q = rotate(q, -rot);
    f32 k0 = length(q / r);
    if (k0 < 1e-5f) return -min(r.x, r.y);
    f32 k1 = length(q / (r * r));
    return k0 * (k0 - 1.0f) / k1;
  }
  Rect bounds() const { return rect_around(c, max(r.x, r.y)); }
};

/** A segment with a radius at each end: limbs, tails, strands, stems. */
struct Capsule {
  Vec2 a, b; f32 ra, rb;
  f32 sd(Vec2 p) const {
    Vec2 ba = b - a;
    f32 h = length(ba);
    if (h < 1e-4f) return dist(p, a) - max(ra, rb);
    Vec2 dir = ba / h;
    Vec2 q = p - a;
    Vec2 l = {absf(dot(q, perp(dir))), dot(q, dir)};
    f32 k = (ra - rb) / h;
    if (k >= 1.0f) return dist(p, a) - ra;
    if (k <= -1.0f) return dist(p, b) - rb;
    f32 m = sqrtf(1.0f - k * k);
    f32 t = dot(l, {-k, m});
    if (t < 0) return length(l) - ra;
    if (t > m * h) return length(l - Vec2{0, h}) - rb;
    return dot(l, {m, k}) - ra;
  }
  Rect bounds() const { return rect_union(rect_around(a, ra), rect_around(b, rb)); }
};

struct RBox {
  Vec2 c; Vec2 half; f32 r = 0; f32 rot = 0;
  f32 sd(Vec2 p) const {
    Vec2 q = p - c;
    if (rot != 0) q = rotate(q, -rot);
    Vec2 d = abs(q) - half + Vec2{r, r};
    return length(vmax(d, {0, 0})) + min(max(d.x, d.y), 0.0f) - r;
  }
  Rect bounds() const { return rect_around(c, length(half) + 0.5f); }
};

constexpr int MAX_POLY = 192;

/** A closed polygon (curves are flattened when the path is built). */
struct Poly {
  FixedVec<Vec2, MAX_POLY> pts;
  Rect box;
  f32 sd(Vec2 p) const {
    const int n = pts.count;
    f32 d = length2(p - pts[0]);
    f32 s = 1.0f;
    for (int i = 0, j = n - 1; i < n; j = i, i++) {
      Vec2 e = pts[j] - pts[i];
      Vec2 w = p - pts[i];
      Vec2 b = w - e * clamp(dot(w, e) / dot(e, e), 0.0f, 1.0f);
      d = min(d, length2(b));
      bool c1 = p.y >= pts[i].y, c2 = p.y < pts[j].y, c3 = e.x * w.y > e.y * w.x;
      if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) s = -s;
    }
    return s * sqrtf(d);
  }
  Rect bounds() const { return box; }
};

/** An open polyline with a width that can taper from start to end. */
struct Stroke {
  FixedVec<Vec2, MAX_POLY> pts;
  f32 w0 = 1, w1 = 1;
  Rect box;
  f32 sd(Vec2 p) const {
    f32 best = 1e9f;
    const int n = pts.count;
    for (int i = 0; i + 1 < n; i++) {
      Vec2 a = pts[i], b = pts[i + 1];
      Vec2 ba = b - a, pa = p - a;
      f32 h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6f), 0.0f, 1.0f);
      f32 t = (i + h) / max(1, n - 1);
      f32 r = lerp(w0, w1, t) * 0.5f;
      best = min(best, length(pa - ba * h) - r);
    }
    return best;
  }
  Rect bounds() const { return box.grow(max(w0, w1)); }
};

/** Builds polygons and strokes from lines and Bezier curves, in world units. */
struct Path {
  FixedVec<Vec2, MAX_POLY> pts;
  Path& move(Vec2 p) { pts.clear(); pts.push(p); return *this; }
  Path& line(Vec2 p) { pts.push(p); return *this; }
  Path& quad(Vec2 c, Vec2 p, int seg = 10) {
    Vec2 s = pts.back();
    for (int i = 1; i <= seg; i++) {
      f32 t = (f32)i / seg, u = 1 - t;
      pts.push(s * (u * u) + c * (2 * u * t) + p * (t * t));
    }
    return *this;
  }
  Path& cubic(Vec2 c1, Vec2 c2, Vec2 p, int seg = 14) {
    Vec2 s = pts.back();
    for (int i = 1; i <= seg; i++) {
      f32 t = (f32)i / seg, u = 1 - t;
      pts.push(s * (u * u * u) + c1 * (3 * u * u * t) + c2 * (3 * u * t * t) + p * (t * t * t));
    }
    return *this;
  }
  Rect box() const {
    Rect r{1e9f, 1e9f, -1e9f, -1e9f};
    for (Vec2 p : pts) { r.x0 = min(r.x0, p.x); r.y0 = min(r.y0, p.y); r.x1 = max(r.x1, p.x); r.y1 = max(r.y1, p.y); }
    return r;
  }
  Poly poly() const { Poly p; p.pts = pts; p.box = box(); return p; }
  Stroke stroke(f32 w0, f32 w1 = -1) const {
    Stroke s; s.pts = pts; s.w0 = w0; s.w1 = w1 < 0 ? w0 : w1; s.box = box(); return s;
  }
};

/** Up to 24 circles blended together with a smooth minimum: clouds, foliage, rocks. */
struct Blob {
  FixedVec<Circle, 24> c;
  f32 k = 2.0f;
  Blob& add(Vec2 p, f32 r) { c.push({p, r}); return *this; }
  f32 sd(Vec2 p) const {
    f32 d = c[0].sd(p);
    for (int i = 1; i < c.count; i++) {
      f32 e = c[i].sd(p);
      f32 h = clamp(0.5f + 0.5f * (e - d) / k, 0.0f, 1.0f);
      d = lerp(e, d, h) - k * h * (1.0f - h);
    }
    return d;
  }
  Rect bounds() const {
    Rect r = c[0].bounds();
    for (int i = 1; i < c.count; i++) r = rect_union(r, c[i].bounds());
    return r;
  }
};

// combinators
template <class A> struct Grow {
  A a; f32 w;
  f32 sd(Vec2 p) const { return a.sd(p) - w; }
  Rect bounds() const { return a.bounds().grow(max(w, 0.0f)); }
};
template <class A, class B> struct Union {
  A a; B b;
  f32 sd(Vec2 p) const { return min(a.sd(p), b.sd(p)); }
  Rect bounds() const { return rect_union(a.bounds(), b.bounds()); }
};
template <class A, class B> struct Sub {
  A a; B b;
  f32 sd(Vec2 p) const { return max(a.sd(p), -b.sd(p)); }
  Rect bounds() const { return a.bounds(); }
};
template <class A, class B> struct Inter {
  A a; B b;
  f32 sd(Vec2 p) const { return max(a.sd(p), b.sd(p)); }
  Rect bounds() const { return a.bounds(); }
};
/** Wobbles a shape's edge with noise: leaves, moss, rough stone. */
template <class A> struct Wobble {
  A a; f32 amp; f32 freq; u32 seed;
  f32 sd(Vec2 p) const {
    f32 n = value_noise(p.x * freq, p.y * freq, seed) * 2.0f - 1.0f;
    return a.sd(p) + n * amp;
  }
  Rect bounds() const { return a.bounds().grow(amp); }
};

template <class A> Grow<A> grow(const A& a, f32 w) { return {a, w}; }
template <class A, class B> Union<A, B> unite(const A& a, const B& b) { return {a, b}; }
template <class A, class B> Sub<A, B> subtract(const A& a, const B& b) { return {a, b}; }
template <class A, class B> Inter<A, B> intersect(const A& a, const B& b) { return {a, b}; }
template <class A> Wobble<A> wobble(const A& a, f32 amp, f32 freq, u32 seed) { return {a, amp, freq, seed}; }

// ---------------------------------------------------------------- styles (return straight colour)
struct Solid {
  Color c;
  Color operator()(Vec2, f32) const { return c; }
};
struct Linear {
  Vec2 a, b; Color c0, c1;
  Color operator()(Vec2 p, f32) const {
    Vec2 ab = b - a;
    return mix(c0, c1, saturate(dot(p - a, ab) / dot(ab, ab)));
  }
};
struct Radial {
  Vec2 c; f32 r; Color c0, c1;
  Color operator()(Vec2 p, f32) const { return mix(c0, c1, saturate(dist(p, c) / r)); }
};

/**
 * Soft "clay" shading: lighter toward the light (top-left), darker away from it, and a gentle
 * darkening just inside the rim so every shape reads as round.
 */
struct Volume {
  Color base, light, dark;
  Vec2 center; f32 radius;
  f32 rim = 2.0f;          // width of the rim band, world units
  f32 rim_amt = 0.35f;
  Vec2 to_light = {-0.55f, -0.83f};
  Color operator()(Vec2 p, f32 d) const {
    Vec2 n = (p - center) / radius;
    f32 t = saturate(dot(n, to_light) * 0.5f + 0.5f);
    Color c = t > 0.5f ? mix(base, light, (t - 0.5f) * 2.0f) : mix(dark, base, t * 2.0f);
    f32 r = smoothstep(-rim, 0.0f, d);
    return mix(c, dark, r * rim_amt);
  }
};

/** Adds fine noise to any style: fabric, bark, leaves, stone. */
template <class S> struct Noisy {
  S s; f32 amt; f32 freq; u32 seed;
  Color operator()(Vec2 p, f32 d) const {
    Color c = s(p, d);
    f32 n = (fbm(p.x * freq, p.y * freq, seed, 3) - 0.5f) * 2.0f * amt;
    return {c.r + n, c.g + n, c.b + n, c.a};
  }
};
template <class S> Noisy<S> noisy(const S& s, f32 amt, f32 freq, u32 seed = 7) { return {s, amt, freq, seed}; }

// ---------------------------------------------------------------- the rasteriser
template <class S, class F>
void fill(Canvas& cv, const S& shape, const F& style, Blend mode = Blend::Over, f32 feather = 0.0f) {
  Rect b = shape.bounds().grow(feather + 2.0f / cv.scale);
  int ix0 = max(0, (int)floorf((b.x0 - cv.origin.x) * cv.scale));
  int iy0 = max(0, (int)floorf((b.y0 - cv.origin.y) * cv.scale));
  int ix1 = min(cv.w - 1, (int)ceilf((b.x1 - cv.origin.x) * cv.scale));
  int iy1 = min(cv.h - 1, (int)ceilf((b.y1 - cv.origin.y) * cv.scale));
  const f32 inv = 1.0f / cv.scale;
  for (int iy = iy0; iy <= iy1; iy++) {
    Px* row = cv.px + iy * cv.w;
    f32 wy = cv.origin.y + (iy + 0.5f) * inv;
    for (int ix = ix0; ix <= ix1; ix++) {
      Vec2 p{cv.origin.x + (ix + 0.5f) * inv, wy};
      f32 d = shape.sd(p);
      f32 cov = feather > 0 ? saturate(0.5f - d / feather) : saturate(0.5f - d * cv.scale);
      if (cov <= 0.0f) continue;
      Color c = style(p, d);
      f32 a = saturate(c.a) * cov;
      Px src{saturate(c.r) * a, saturate(c.g) * a, saturate(c.b) * a, a};
      Px& dst = row[ix];
      switch (mode) {
        case Blend::Over:
          dst = {src.r + dst.r * (1 - a), src.g + dst.g * (1 - a), src.b + dst.b * (1 - a), a + dst.a * (1 - a)};
          break;
        case Blend::Atop:
          dst = {src.r * dst.a + dst.r * (1 - a), src.g * dst.a + dst.g * (1 - a), src.b * dst.a + dst.b * (1 - a), dst.a};
          break;
        case Blend::Behind: {
          f32 k = 1 - dst.a;
          dst = {dst.r + src.r * k, dst.g + src.g * k, dst.b + src.b * k, dst.a + a * k};
          break;
        }
        case Blend::Erase:
          dst = {dst.r * (1 - a), dst.g * (1 - a), dst.b * (1 - a), dst.a * (1 - a)};
          break;
        case Blend::Add:
          dst = {min(1.0f, dst.r + src.r), min(1.0f, dst.g + src.g), min(1.0f, dst.b + src.b), min(1.0f, dst.a + src.a)};
          break;
      }
    }
  }
}

/** Paints a darker band around a shape, then the shape itself on top: the sticker look. */
template <class S, class F>
void outlined(Canvas& cv, const S& shape, const F& style, Color line, f32 width) {
  fill(cv, grow(shape, width), Solid{line});
  fill(cv, shape, style);
}

/** A soft drop shadow or glow: the shape painted with a wide feather. */
template <class S>
void soft(Canvas& cv, const S& shape, Color c, f32 feather, Blend mode = Blend::Over) {
  fill(cv, shape, Solid{c}, mode, feather);
}

}  // namespace sky::paint
