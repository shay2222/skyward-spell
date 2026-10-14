// core.h - the engine's foundation: types, maths, colour, randomness, noise and memory.
//
// The game is built as freestanding C++20 for WebAssembly: no C or C++ standard library,
// only the compiler. Everything the engine needs (trig, noise, containers, allocation)
// lives here, which also makes the maths identical on every browser, so a level seed
// always produces exactly the same level.
#pragma once

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using i32 = int;
using i64 = long long;
using f32 = float;
using usize = __SIZE_TYPE__;

inline void* operator new(usize, void* p) noexcept { return p; }

extern "C" {
void* memcpy(void* dst, const void* src, usize n);
void* memset(void* dst, int v, usize n);
void* memmove(void* dst, const void* src, usize n);
}

namespace sky {

// ---------------------------------------------------------------- maths
constexpr f32 PI = 3.14159265358979f;
constexpr f32 TAU = 6.28318530717959f;
constexpr f32 HALF_PI = 1.57079632679490f;

template <class T> constexpr T min(T a, T b) { return a < b ? a : b; }
template <class T> constexpr T max(T a, T b) { return a > b ? a : b; }
template <class T> constexpr T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
template <class T> constexpr void swap(T& a, T& b) { T t = a; a = b; b = t; }

inline f32 absf(f32 x) { return __builtin_fabsf(x); }
inline f32 sqrtf(f32 x) { return __builtin_sqrtf(x); }
inline f32 floorf(f32 x) { return __builtin_floorf(x); }
inline f32 ceilf(f32 x) { return __builtin_ceilf(x); }
inline f32 roundf(f32 x) { return __builtin_floorf(x + 0.5f); }
inline f32 fract(f32 x) { return x - floorf(x); }
inline f32 signf(f32 x) { return x < 0 ? -1.0f : (x > 0 ? 1.0f : 0.0f); }
inline f32 saturate(f32 x) { return clamp(x, 0.0f, 1.0f); }
inline f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }
inline f32 unlerp(f32 a, f32 b, f32 v) { return (v - a) / (b - a); }
inline f32 remap(f32 v, f32 a0, f32 a1, f32 b0, f32 b1) { return lerp(b0, b1, unlerp(a0, a1, v)); }
inline f32 smoothstep(f32 e0, f32 e1, f32 x) {
  f32 t = saturate((x - e0) / (e1 - e0));
  return t * t * (3.0f - 2.0f * t);
}
inline f32 fmodf(f32 x, f32 y) { return x - y * floorf(x / y); }

/** sin on [-pi/2, pi/2] after range reduction; accurate to ~1e-7. */
inline f32 sinf(f32 x) {
  x -= TAU * floorf(x * (1.0f / TAU) + 0.5f);
  if (x > HALF_PI) x = PI - x;
  else if (x < -HALF_PI) x = -PI - x;
  f32 x2 = x * x;
  return x * (1.0f + x2 * (-1.0f / 6.0f + x2 * (1.0f / 120.0f + x2 * (-1.0f / 5040.0f + x2 * (1.0f / 362880.0f + x2 * (-1.0f / 39916800.0f))))));
}
inline f32 cosf(f32 x) { return sinf(x + HALF_PI); }

/** atan for |x| <= 1, minimax polynomial, error ~1e-5. */
inline f32 atan_unit(f32 x) {
  f32 x2 = x * x;
  return x * (0.99997726f + x2 * (-0.33262347f + x2 * (0.19354346f + x2 * (-0.11643287f + x2 * (0.05265332f + x2 * -0.01172120f)))));
}
inline f32 atan2f(f32 y, f32 x) {
  f32 ax = absf(x), ay = absf(y);
  if (ax == 0.0f && ay == 0.0f) return 0.0f;
  f32 a = ay > ax ? HALF_PI - atan_unit(ax / ay) : atan_unit(ay / ax);
  if (x < 0) a = PI - a;
  return y < 0 ? -a : a;
}

/** 2^x via exponent bits and a polynomial for the fraction. */
inline f32 exp2f(f32 x) {
  x = clamp(x, -126.0f, 126.0f);
  f32 xi = floorf(x);
  f32 f = x - xi;
  f32 p = 1.0f + f * (0.69314718f + f * (0.24022650f + f * (0.05550411f + f * (0.00961813f + f * 0.00133336f))));
  u32 bits = (u32)((i32)xi + 127) << 23;
  return p * __builtin_bit_cast(f32, bits);
}
inline f32 expf(f32 x) { return exp2f(x * 1.44269504f); }
inline f32 log2f(f32 x) {
  if (x <= 0) return -126.0f;
  u32 bits = __builtin_bit_cast(u32, x);
  f32 e = (f32)((i32)((bits >> 23) & 255) - 127);
  f32 m = __builtin_bit_cast(f32, (bits & 0x007fffffu) | 0x3f800000u) - 1.0f; // [0,1)
  return e + m * (1.44269504f + m * (-0.72134752f + m * (0.48089835f + m * (-0.34732155f + m * (0.24187369f + m * -0.09213432f)))));
}
inline f32 powf(f32 x, f32 y) { return x <= 0 ? 0.0f : exp2f(y * log2f(x)); }

/** Frame-rate independent smoothing: move a toward b with a half-life-like rate. */
inline f32 damp(f32 a, f32 b, f32 rate, f32 dt) { return lerp(b, a, expf(-rate * dt)); }

inline f32 wrap_angle(f32 a) { return a - TAU * floorf((a + PI) / TAU); }
inline f32 lerp_angle(f32 a, f32 b, f32 t) { return a + wrap_angle(b - a) * t; }

// ---------------------------------------------------------------- vectors
struct Vec2 {
  f32 x = 0, y = 0;
  constexpr Vec2() = default;
  constexpr Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}
  constexpr Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
  constexpr Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
  constexpr Vec2 operator*(f32 s) const { return {x * s, y * s}; }
  constexpr Vec2 operator*(Vec2 o) const { return {x * o.x, y * o.y}; }
  constexpr Vec2 operator/(f32 s) const { return {x / s, y / s}; }
  constexpr Vec2 operator/(Vec2 o) const { return {x / o.x, y / o.y}; }
  constexpr Vec2 operator-() const { return {-x, -y}; }
  Vec2& operator+=(Vec2 o) { x += o.x; y += o.y; return *this; }
  Vec2& operator-=(Vec2 o) { x -= o.x; y -= o.y; return *this; }
  Vec2& operator*=(f32 s) { x *= s; y *= s; return *this; }
};
inline constexpr f32 dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline constexpr f32 cross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
inline f32 length(Vec2 v) { return sqrtf(dot(v, v)); }
inline f32 length2(Vec2 v) { return dot(v, v); }
inline f32 dist(Vec2 a, Vec2 b) { return length(a - b); }
inline Vec2 normalize(Vec2 v) { f32 l = length(v); return l > 1e-6f ? v / l : Vec2{0, 0}; }
inline Vec2 lerp(Vec2 a, Vec2 b, f32 t) { return a + (b - a) * t; }
inline Vec2 damp(Vec2 a, Vec2 b, f32 rate, f32 dt) { return lerp(b, a, expf(-rate * dt)); }
inline Vec2 rotate(Vec2 v, f32 a) { f32 c = cosf(a), s = sinf(a); return {v.x * c - v.y * s, v.x * s + v.y * c}; }
inline Vec2 perp(Vec2 v) { return {-v.y, v.x}; }
inline Vec2 from_angle(f32 a, f32 r = 1.0f) { return {cosf(a) * r, sinf(a) * r}; }
inline Vec2 abs(Vec2 v) { return {absf(v.x), absf(v.y)}; }
inline Vec2 vmax(Vec2 a, Vec2 b) { return {max(a.x, b.x), max(a.y, b.y)}; }
inline Vec2 vmin(Vec2 a, Vec2 b) { return {min(a.x, b.x), min(a.y, b.y)}; }

struct Rect {
  f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  bool contains(Vec2 p) const { return p.x >= x0 && p.x <= x1 && p.y >= y0 && p.y <= y1; }
  bool overlaps(const Rect& o) const { return x0 < o.x1 && x1 > o.x0 && y0 < o.y1 && y1 > o.y0; }
  Rect grow(f32 m) const { return {x0 - m, y0 - m, x1 + m, y1 + m}; }
};

// ---------------------------------------------------------------- colour
/** Straight (not premultiplied) RGBA in 0..1. */
struct Color {
  f32 r = 0, g = 0, b = 0, a = 1;
  constexpr Color() = default;
  constexpr Color(f32 r_, f32 g_, f32 b_, f32 a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
  constexpr Color operator*(f32 s) const { return {r * s, g * s, b * s, a}; }
  Color with_alpha(f32 na) const { return {r, g, b, na}; }
};
constexpr Color hex(u32 rgb, f32 a = 1.0f) {
  return {((rgb >> 16) & 255) / 255.0f, ((rgb >> 8) & 255) / 255.0f, (rgb & 255) / 255.0f, a};
}
inline Color mix(Color a, Color b, f32 t) {
  return {lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), lerp(a.a, b.a, t)};
}
inline Color shade(Color c, f32 k) { return {c.r * k, c.g * k, c.b * k, c.a}; }
inline Color tint(Color c, Color t, f32 k) { return mix(c, t.with_alpha(c.a), k); }
inline u32 pack_rgba8(Color c) {
  auto q = [](f32 v) { return (u32)(saturate(v) * 255.0f + 0.5f); };
  return q(c.r) | (q(c.g) << 8) | (q(c.b) << 16) | (q(c.a) << 24);
}

// ---------------------------------------------------------------- randomness
/** PCG32: small, fast, and good enough for level generation. Seeded runs repeat exactly. */
struct Rng {
  u64 state = 0x853c49e6748fea9bULL;
  u64 inc = 0xda3e39cb94b95bdbULL;
  Rng() = default;
  explicit Rng(u64 seed, u64 stream = 54) { reseed(seed, stream); }
  void reseed(u64 seed, u64 stream = 54) {
    state = 0;
    inc = (stream << 1u) | 1u;
    next();
    state += seed;
    next();
  }
  u32 next() {
    u64 old = state;
    state = old * 6364136223846793005ULL + inc;
    u32 xorshifted = (u32)(((old >> 18u) ^ old) >> 27u);
    u32 rot = (u32)(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
  }
  f32 unit() { return (next() >> 8) * (1.0f / 16777216.0f); }  // [0,1)
  f32 range(f32 a, f32 b) { return a + (b - a) * unit(); }
  i32 irange(i32 a, i32 b) { return a + (i32)(next() % (u32)(b - a + 1)); }  // inclusive
  bool chance(f32 p) { return unit() < p; }
  f32 signed_unit() { return unit() * 2.0f - 1.0f; }
  Vec2 in_circle(f32 r) {
    f32 a = unit() * TAU, d = sqrtf(unit()) * r;
    return {cosf(a) * d, sinf(a) * d};
  }
};

inline u32 hash_u32(u32 x) {
  x ^= x >> 16; x *= 0x7feb352dU;
  x ^= x >> 15; x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}
inline u32 hash2(i32 x, i32 y, u32 seed = 0) { return hash_u32((u32)x * 0x8da6b343u ^ (u32)y * 0xd8163841u ^ seed * 0xcb1ab31fu); }
inline f32 hash_unit(i32 x, i32 y, u32 seed = 0) { return (hash2(x, y, seed) >> 8) * (1.0f / 16777216.0f); }

/** Smooth value noise in [0,1]. */
inline f32 value_noise(f32 x, f32 y, u32 seed = 0) {
  i32 ix = (i32)floorf(x), iy = (i32)floorf(y);
  f32 fx = x - ix, fy = y - iy;
  f32 ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
  f32 a = hash_unit(ix, iy, seed), b = hash_unit(ix + 1, iy, seed);
  f32 c = hash_unit(ix, iy + 1, seed), d = hash_unit(ix + 1, iy + 1, seed);
  return lerp(lerp(a, b, ux), lerp(c, d, ux), uy);
}
inline f32 fbm(f32 x, f32 y, u32 seed = 0, int octaves = 4) {
  f32 sum = 0, amp = 0.5f, norm = 0;
  for (int i = 0; i < octaves; i++) {
    sum += value_noise(x, y, seed + i * 131) * amp;
    norm += amp;
    x = x * 2.03f + 17.1f;
    y = y * 2.03f - 9.7f;
    amp *= 0.5f;
  }
  return sum / norm;
}

// ---------------------------------------------------------------- memory
/** Linear allocator over a fixed block. Levels allocate here and reset in one step on regrow. */
struct Arena {
  u8* base = nullptr;
  usize cap = 0, used = 0, peak = 0;
  void init(void* mem, usize bytes) { base = (u8*)mem; cap = bytes; used = 0; }
  void* alloc(usize bytes, usize align = 16) {
    usize p = (used + align - 1) & ~(align - 1);
    if (p + bytes > cap) return nullptr;
    used = p + bytes;
    if (used > peak) peak = used;
    return base + p;
  }
  template <class T> T* make(usize n = 1) {
    T* p = (T*)alloc(sizeof(T) * n, alignof(T) > 16 ? alignof(T) : 16);
    if (p) for (usize i = 0; i < n; i++) new (p + i) T();
    return p;
  }
  usize mark() const { return used; }
  void reset(usize m = 0) { used = m; }
};

/** Fixed-capacity array with value semantics; no heap, no surprises. */
template <class T, int N>
struct FixedVec {
  T items[N];
  int count = 0;
  static constexpr int capacity = N;
  T& operator[](int i) { return items[i]; }
  const T& operator[](int i) const { return items[i]; }
  T* begin() { return items; }
  T* end() { return items + count; }
  const T* begin() const { return items; }
  const T* end() const { return items + count; }
  bool full() const { return count >= N; }
  bool empty() const { return count == 0; }
  void clear() { count = 0; }
  T* push(const T& v) {
    if (count >= N) return nullptr;
    items[count] = v;
    return &items[count++];
  }
  T* push() {
    if (count >= N) return nullptr;
    items[count] = T();
    return &items[count++];
  }
  void remove_swap(int i) { items[i] = items[--count]; }
  T& back() { return items[count - 1]; }
};

/** A view into contiguous memory. */
template <class T>
struct Span {
  T* data = nullptr;
  int count = 0;
  T& operator[](int i) const { return data[i]; }
  T* begin() const { return data; }
  T* end() const { return data + count; }
};

/** Grid of values stored row-major, allocated from an arena. */
template <class T>
struct Grid {
  T* cells = nullptr;
  int w = 0, h = 0;
  void init(Arena& a, int w_, int h_) { w = w_; h = h_; cells = a.make<T>((usize)w * h); }
  bool in(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
  T& at(int x, int y) { return cells[y * w + x]; }
  const T& at(int x, int y) const { return cells[y * w + x]; }
  T get(int x, int y, T fallback) const { return in(x, y) ? cells[y * w + x] : fallback; }
};

// ---------------------------------------------------------------- strings (small, for logs and UI)
struct StrBuf {
  char buf[512] = {};
  int len = 0;
  StrBuf& add(const char* s) { while (*s && len < 511) buf[len++] = *s++; buf[len] = 0; return *this; }
  StrBuf& add(char c) { if (len < 511) buf[len++] = c; buf[len] = 0; return *this; }
  StrBuf& add_int(i64 v) {
    if (v < 0) { add('-'); v = -v; }
    char tmp[24]; int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 23);
    while (n) add(tmp[--n]);
    return *this;
  }
  StrBuf& add_float(f32 v, int decimals = 2) {
    if (v < 0) { add('-'); v = -v; }
    i64 whole = (i64)v;
    add_int(whole);
    if (decimals > 0) {
      add('.');
      f32 frac = v - (f32)whole;
      for (int i = 0; i < decimals; i++) { frac *= 10; int d = (int)frac; add((char)('0' + d)); frac -= d; }
    }
    return *this;
  }
  void clear() { len = 0; buf[0] = 0; }
  const char* c_str() const { return buf; }
};

}  // namespace sky
