#include "world.h"
#include "npc.h"

namespace sky {

namespace {

/** Catmull-Rom point on the segment p1->p2. */
Vec2 catmull(Vec2 p0, Vec2 p1, Vec2 p2, Vec2 p3, f32 t) {
  f32 t2 = t * t, t3 = t2 * t;
  return (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 + (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
}

struct Disc { Vec2 p; f32 r; };

/** Places props and remembers which ones are big enough to keep others away. */
struct Placer {
  World& w;
  Rng& rng;
  FixedVec<Disc, 2048> big;   // trees, rocks, logs... in the open (not the edge woods)
  FixedVec<Disc, 64> keep;    // areas to keep clear: start, shrine, camps, gate gaps

  Prop* add(PropKind kind, u16 sprite, Vec2 pos) {
    if (w.prop_count >= w.prop_cap) return nullptr;
    Prop* p = &w.props[w.prop_count++];
    *p = Prop();
    p->kind = kind;
    p->sprite = sprite;
    p->pos = pos;
    return p;
  }
  Prop* add(PropKind kind, const PropSpec& s, Vec2 pos) {
    Prop* p = add(kind, s.sprite, pos);
    if (!p) return nullptr;
    p->scale = rng.range(s.scale0, s.scale1);
    p->radius = s.radius * p->scale;
    p->col_offset = s.col_off * p->scale;
    p->sway = s.sway;
    p->shadow = s.shadow * p->scale;
    if (s.flip && rng.chance(0.5f)) { p->flags |= PF_FLIP; p->col_offset.x = -p->col_offset.x; }
    if (s.ground) p->flags |= PF_GROUND;
    p->light = s.light * (0.85f + 0.15f * p->scale);
    p->light_col = s.light_col;
    p->emit = s.emit;
    p->reveal_w = s.reveal_w * p->scale;
    p->reveal_h = s.reveal_h * p->scale;
    return p;
  }
  void mark_big(const Prop* p, f32 r) { if (p) big.push({p->pos, r}); }
  bool clear_of_big(Vec2 p, f32 r) const {
    for (const Disc& d : big) {
      f32 rr = r + d.r;
      if (length2(d.p - p) < rr * rr) return false;
    }
    return true;
  }
  bool clear_of_keep(Vec2 p, f32 r) const {
    for (const Disc& d : keep) {
      f32 rr = r + d.r;
      if (length2(d.p - p) < rr * rr) return false;
    }
    return true;
  }
  Vec2 random_point(f32 margin_tiles) {
    return {rng.range(margin_tiles * TS, w.px_w() - margin_tiles * TS), rng.range(margin_tiles * TS, w.px_h() - margin_tiles * TS)};
  }
};

int nearest_sample(const World& w, Vec2 p) {
  int best = 0;
  f32 bd = 1e18f;
  for (int i = 0; i < w.samples.count; i += 2) {
    f32 d = length2(w.samples[i] - p);
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

int sample_at_x(const World& w, f32 x) {
  int i = 0;
  while (i + 1 < w.samples.count && w.samples[i].x < x) i++;
  return i;
}

Vec2 path_normal(const World& w, int i) {
  Vec2 t = normalize(w.samples[min(i + 2, w.samples.count - 1)] - w.samples[max(i - 2, 0)]);
  return perp(t);
}

}  // namespace

f32 World::sample(const Grid<f32>& g, Vec2 p) const {
  f32 fx = p.x / TS - 0.5f, fy = p.y / TS - 0.5f;
  int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
  f32 tx = fx - x0, ty = fy - y0;
  auto at = [&](int x, int y) { return g.cells[clamp(y, 0, h - 1) * w + clamp(x, 0, w - 1)]; };
  return lerp(lerp(at(x0, y0), at(x0 + 1, y0), tx), lerp(at(x0, y0 + 1), at(x0 + 1, y0 + 1), tx), ty);
}

int World::section_of(Vec2 p) const {
  int s = 0;
  for (int i = 1; i < section_x.count; i++)
    if (p.x >= section_x[i]) s = i;
  return s;
}

f32 World::progress_of(Vec2 p) const { return (f32)nearest_sample(*this, p) / max(1, samples.count - 1); }

void World::generate(Arena& A, Region r, u32 seed_) {
  region = r;
  def = &REGIONS[r];
  seed = seed_;
  w = def->width;
  h = def->height;
  const bool sky = def->liquid == LQ_SKY;
  water.init(A, w, h);
  path.init(A, w, h);
  forest.init(A, w, h);
  lush.init(A, w, h);
  depth.init(A, w, h);
  dpath.init(A, w, h);
  ground.init(A, w, h);
  prop_cap = 12000;
  props = A.make<Prop>(prop_cap);
  prop_count = 0;
  gates.clear();
  camps.clear();
  waystones.clear();
  spots.clear();
  section_x.clear();
  Rng rng(seed, 3);
  const RegionArt& RA = S.ra;

  // ---- where the gates go: evenly spaced between the start and the shrine
  const int G = def->gates;
  f32 gx[4];
  for (int i = 0; i < G; i++) gx[i] = 5.0f + (w - 10.0f) * (i + 1) / (G + 1) + rng.range(-1.5f, 1.5f);

  // ---- the winding path, left to right, running straight through each gate
  path_points.clear();
  auto push_pt = [&](f32 x, f32 y) { path_points.push({x * TS, y * TS}); };
  f32 y = h * 0.5f + rng.range(-5, 5);
  f32 x = 1;
  push_pt(x, y);
  f32 gate_y[4];
  has_arena = r == R_HOLLOW;
  arena_r = has_arena ? 7.0f * TS : 0.0f;
  for (int gi = 0; gi <= G; gi++) {
    f32 next = gi < G ? gx[gi] - 4.5f : (has_arena ? w - 12.0f : w - 1.0f);
    int n = max(1, (int)((next - x) / 8.5f + 0.5f));
    for (int k = 1; k <= n; k++) {
      f32 xx = lerp(x, next, (f32)k / n);
      y = clamp(y + rng.range(-7.5f, 7.5f), 9.0f, h - 9.0f);
      if (gi < G && k == n) y = clamp(y, h * 0.32f, h * 0.68f);
      if (gi == G && has_arena) {
        if (k == n) y = h * 0.5f;
        else if (k == n - 1) y = lerp(y, h * 0.5f, 0.6f);
      }
      push_pt(xx, y);
    }
    if (gi < G) {
      gate_y[gi] = y;
      push_pt(gx[gi], y);
      push_pt(gx[gi] + 4.5f, y);
      x = gx[gi] + 4.5f;
    }
  }
  samples.clear();
  for (int i = 0; i + 1 < path_points.count; i++) {
    Vec2 p0 = path_points[max(0, i - 1)], p1 = path_points[i], p2 = path_points[i + 1], p3 = path_points[min(path_points.count - 1, i + 2)];
    int seg = clamp((int)(dist(p1, p2) / 14.0f), 6, 40);
    for (int k = 0; k < seg && !samples.full(); k++) samples.push(catmull(p0, p1, p2, p3, (f32)k / seg));
  }
  samples.push(path_points.back());

  if (has_arena) arena = path_points.back();

  // ---- distance to the path, per tile
  for (int ty = 0; ty < h; ty++)
    for (int tx = 0; tx < w; tx++) {
      Vec2 c{(tx + 0.5f) * TS, (ty + 0.5f) * TS};
      f32 best = 1e18f;
      for (Vec2 s : samples) best = min(best, length2(c - s));
      dpath.at(tx, ty) = sqrtf(best) / TS;
    }

  // ---- the border, the barriers across the land, and the path
  const bool liquid_bar = def->barrier == BAR_LIQUID;
  auto band_x = [&](int gi, f32 ty) {
    f32 wig = (fbm(ty * 0.08f, gi * 13.7f, seed + 21) - 0.5f) * (liquid_bar ? 7.0f : 3.0f);
    return gx[gi] + wig;
  };
  for (int ty = 0; ty < h; ty++)
    for (int tx = 0; tx < w; tx++) {
      f32 dp = dpath.at(tx, ty);
      f32 edge = (f32)min(min(tx, ty), min(w - 1 - tx, h - 1 - ty));
      f32 n = fbm(tx * 0.13f, ty * 0.13f, seed + 1);
      f32 border = saturate((4.0f + (n - 0.5f) * 5.0f - edge) / 1.5f);
      border *= saturate((dp - 2.2f) / 2.0f);
      f32 band = 0;
      for (int gi = 0; gi < G; gi++) {
        f32 half = def->barrier == BAR_WALL ? 1.0f : liquid_bar ? 1.9f + (fbm(ty * 0.2f, gi * 3.1f, seed + 5) - 0.5f) * 1.2f : 2.3f;
        f32 d = absf(tx + 0.5f - band_x(gi, ty + 0.5f)) - half;
        band = max(band, saturate(0.5f - d / 1.0f));
      }
      band *= saturate((dp - 1.0f) / 0.8f);
      if (has_arena) {  // the lair: a clearing, with no woods or walls
        f32 da = dist(Vec2{tx + 0.5f, ty + 0.5f}, arena / TS) - arena_r / TS;
        f32 open = saturate((da - 0.5f) / 1.5f);
        border *= open;
        band *= open;
      }
      if (sky) {
        water.at(tx, ty) = border;  // Skyreach floats: open sky all around
      } else {
        forest.at(tx, ty) = border;
      }
      if (liquid_bar) water.at(tx, ty) = max(water.at(tx, ty), band);
      else forest.at(tx, ty) = max(forest.at(tx, ty), band);
      path.at(tx, ty) = saturate(0.5f + (1.2f - dp) / 0.7f);
    }

  // ---- start, shrine, gates and sections
  int si = sample_at_x(*this, 5.5f * TS);
  start = samples[si];
  flint_start = start + Vec2{-30, 14};
  shrine = has_arena ? arena : samples[sample_at_x(*this, (w - 7.0f) * TS)];
  section_x.push(0);
  for (int gi = 0; gi < G; gi++) {
    Gate g;
    g.pos = {band_x(gi, gate_y[gi]) * TS, 0};
    g.pos = samples[sample_at_x(*this, g.pos.x)];
    g.section_after = gi + 1;
    gates.push(g);
    section_x.push(g.pos.x);
  }

  Placer P{*this, rng};
  P.keep.push({start, 110});
  P.keep.push({shrine, 120});
  if (has_arena) P.keep.push({arena, arena_r + 40});
  for (const Gate& g : gates) P.keep.push({g.pos, 90});

  // ---- ponds (or bog pools, lava lakes, holes in the sky, goo), away from the path
  FixedVec<Vec2, 8> ponds;
  for (int tries = 0; tries < 400 && ponds.count < def->ponds; tries++) {
    Vec2 c{rng.range(9.0f, w - 9.0f), rng.range(8.0f, h - 8.0f)};
    f32 rad = rng.range(def->pond_r0, def->pond_r1);
    f32 dp = dpath.at((int)c.x, (int)c.y);
    bool ok = dp > rad + 3.2f;
    for (Vec2 o : ponds) ok = ok && dist(o, c) > rad + 9;
    for (int gi = 0; gi < G; gi++) ok = ok && absf(c.x - gx[gi]) > rad + 5;
    ok = ok && dist(c * TS, start) > (rad + 5) * TS && dist(c * TS, shrine) > (rad + 5) * TS + arena_r;
    if (!ok) continue;
    ponds.push(c);
    for (int ty = 0; ty < h; ty++)
      for (int tx = 0; tx < w; tx++) {
        Vec2 t{tx + 0.5f, ty + 0.5f};
        f32 d = dist(t, c) + (fbm(tx * 0.25f, ty * 0.25f, seed + 9) - 0.5f) * 3.0f;
        water.at(tx, ty) = max(water.at(tx, ty), saturate(0.5f + (rad - d) / 1.1f));
      }
  }

  // ---- liquid depth: distance from the shore
  {
    int* dist_t = A.make<int>(w * h);
    int* queue = A.make<int>(w * h);
    int qh = 0, qt = 0;
    for (int i = 0; i < w * h; i++) {
      bool wet = water.cells[i] > 0.5f;
      dist_t[i] = wet ? 1 << 20 : 0;
      if (!wet) queue[qt++] = i;
    }
    while (qh < qt) {
      int i = queue[qh++];
      int xx = i % w, yy = i / w;
      const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
      for (int k = 0; k < 4; k++) {
        int nx = xx + dx[k], ny = yy + dy[k];
        if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
        int j = ny * w + nx;
        if (dist_t[j] > dist_t[i] + 1) { dist_t[j] = dist_t[i] + 1; queue[qt++] = j; }
      }
    }
    for (int i = 0; i < w * h; i++) depth.cells[i] = water.cells[i] > 0.5f ? saturate((dist_t[i] - 0.5f) / 3.0f) : 0.0f;
  }

  // ---- flower fields
  for (int ty = 0; ty < h; ty++)
    for (int tx = 0; tx < w; tx++) {
      f32 n = fbm(tx * 0.09f, ty * 0.09f, seed + 7);
      lush.at(tx, ty) = smoothstep(0.55f, 0.72f, n) * (1 - path.at(tx, ty)) * (1 - water.at(tx, ty)) * (1 - forest.at(tx, ty));
    }

  auto open_land = [&](Vec2 p, f32 margin) {
    return solid_at(p) < 0.12f && solid_at(p + Vec2{margin, 0}) < 0.3f && solid_at(p - Vec2{margin, 0}) < 0.3f &&
           solid_at(p + Vec2{0, margin}) < 0.3f && solid_at(p - Vec2{0, margin}) < 0.3f;
  };
  auto near_gate = [&](Vec2 p, f32 r) {
    for (const Gate& g : gates)
      if (dist(p, g.pos) < r) return true;
    return false;
  };

  // ---- waystones: at the start, and just past each gate
  {
    int i0 = min(si + 4, samples.count - 1);
    Vec2 n = path_normal(*this, i0);
    Vec2 p = samples[i0] - n * 48.0f;
    if (!open_land(p, 12)) p = samples[i0] + n * 48.0f;
    waystones.push(p);
  }
  for (const Gate& g : gates) {
    int i = sample_at_x(*this, g.pos.x + 7.5f * TS);
    Vec2 n = path_normal(*this, i);
    Vec2 p = samples[i] - n * 46.0f;
    if (!open_land(p, 12)) p = samples[i] + n * 46.0f;
    waystones.push(p);
  }
  if (has_arena) {  // one last waystone at the way into the lair
    int i = sample_at_x(*this, arena.x - arena_r - 80);
    Vec2 n = path_normal(*this, i);
    Vec2 p = samples[i] - n * 46.0f;
    if (!open_land(p, 12)) p = samples[i] + n * 46.0f;
    waystones.push(p);
  }
  for (Vec2 ws : waystones) P.keep.push({ws, 46});

  // ---- goblin camps: one in each section past the first gate (and one early on, after the meadow)
  for (int s = 0; s <= G; s++) {
    if (s == 0 && region == R_MEADOW) continue;
    f32 x0 = section_x[s], x1 = s < G ? section_x[s + 1] : shrine.x;
    for (int tries = 0; tries < 200; tries++) {
      f32 cx = lerp(x0, x1, rng.range(0.3f, 0.7f));
      int i = sample_at_x(*this, cx);
      Vec2 n = path_normal(*this, i);
      Vec2 c = samples[i] + n * (rng.chance(0.5f) ? 1.0f : -1.0f) * rng.range(110.0f, 170.0f);
      if (!open_land(c, 60) || !P.clear_of_keep(c, 70) || dist(c, start) < 420) continue;
      camps.push(c);
      P.keep.push({c, 92});
      break;
    }
  }

  // ---- hazards near the path
  if (def->hazard != HZ_NONE && def->hazard != HZ_GUST) {
    int placed = 0;
    for (int tries = 0; tries < 1200 && placed < def->hazards; tries++) {
      int i = rng.irange(0, samples.count - 1);
      Vec2 n = path_normal(*this, i);
      f32 side = rng.chance(0.5f) ? 1.0f : -1.0f;
      f32 off = def->hazard == HZ_MUD ? rng.range(-12, 12) : def->hazard == HZ_RUNE ? rng.range(-16, 16) :
                def->hazard == HZ_VENT ? side * rng.range(26, 56) : side * rng.range(40, 54);
      Vec2 p = samples[i] + n * off;
      if (dist(p, start) < 190 || dist(p, shrine) < 120 || near_gate(p, 120) || !open_land(p, 10)) continue;
      bool ok = true;
      for (const Spot& s : spots) ok = ok && dist(s.pos, p) > 96;
      for (Vec2 c : camps) ok = ok && dist(c, p) > 120;
      for (Vec2 ws : waystones) ok = ok && dist(ws, p) > 70;
      if (!ok) continue;
      spots.push({p, SPOT_HAZARD, (u8)section_of(p)});
      P.keep.push({p, def->hazard == HZ_MUD ? 24.0f : 30.0f});
      placed++;
    }
  }

  // ---- the people who live here, standing beside the path
  people.clear();
  {
    int ids[2];
    int count = npcs_in(r, ids);
    for (int k = 0; k < count; k++) {
      int sec = clamp(NPCS[ids[k]].section, 0, G);
      f32 x0 = section_x[sec] + (sec == 0 ? 260.0f : 140.0f), x1 = (sec < G ? section_x[sec + 1] : shrine.x) - 160;
      if (has_arena && sec == G) x1 = arena.x - arena_r - 140;
      for (int tries = 0; tries < 400; tries++) {
        f32 cx = rng.range(x0, max(x0 + 1, x1));
        int i = sample_at_x(*this, cx);
        Vec2 n = path_normal(*this, i);
        Vec2 p = samples[i] + n * (rng.chance(0.5f) ? 1.0f : -1.0f) * rng.range(58.0f, 80.0f);
        if (!open_land(p, 22) || !P.clear_of_keep(p, 40)) continue;
        bool ok = true;
        for (Vec2 c : camps) ok = ok && dist(c, p) > 230;
        for (const Spot& s : spots) ok = ok && dist(s.pos, p) > 80;
        if (!ok) continue;
        people_id[people.count] = ids[k];
        people.push(p);
        P.keep.push({p, 46});
        break;
      }
    }
  }

  // ---- hearts to find, two in each section
  for (int s = 0; s <= G; s++) {
    f32 x0 = section_x[s] + 120, x1 = (s < G ? section_x[s + 1] : shrine.x) - 80;
    int placed = 0;
    for (int tries = 0; tries < 300 && placed < 2; tries++) {
      Vec2 p{rng.range(x0, x1), rng.range(5.0f * TS, px_h() - 5.0f * TS)};
      f32 dp = sample(dpath, p);
      if (dp < 2.0f || dp > 7.0f || !open_land(p, 16) || !P.clear_of_keep(p, 20)) continue;
      spots.push({p, SPOT_HEART, (u8)s});
      P.keep.push({p, 22});
      placed++;
    }
  }

  // ---- props
  auto path_near = [&](Vec2 p, f32 rr) {
    return sample(path, p) > 0.08f || sample(path, p + Vec2{rr, 0}) > 0.2f || sample(path, p - Vec2{rr, 0}) > 0.2f ||
           sample(path, p + Vec2{0, rr}) > 0.2f || sample(path, p - Vec2{0, rr}) > 0.2f;
  };
  auto wet_near = [&](Vec2 p, f32 rr) {
    return sample(water, p) > 0.05f || sample(water, p + Vec2{0, rr * 0.5f}) > 0.1f || sample(water, p + Vec2{rr, 0}) > 0.15f ||
           sample(water, p - Vec2{rr, 0}) > 0.15f;
  };

  // Malgrin's blockers across each gate
  for (int gi = 0; gi < gates.count; gi++) {
    Vec2 c = gates[gi].pos;
    int i = sample_at_x(*this, c.x);
    Vec2 nrm = path_normal(*this, i);
    for (int k = -2; k <= 2; k++) {
      Prop* b = P.add(PK_BLOCKER, RA.blocker[(k + 2) % 2], c + nrm * (k * 24.0f) + Vec2{rng.range(-4, 4), 0});
      if (!b) break;
      b->scale = rng.range(0.95f, 1.1f);
      b->radius = 15 * b->scale;
      b->col_offset = {0, -6};
      b->flags = PF_BURNABLE;
      b->gate = (u8)gi;
      b->shadow = 1.3f;
      b->light = 34;
      b->light_col = RA.blocker_glow;
      if (rng.chance(0.5f)) b->flags |= PF_FLIP;
      P.mark_big(b, 20);
    }
  }

  // walls: a palisade along each wall barrier
  if (def->barrier == BAR_WALL && RA.wall) {
    for (int gi = 0; gi < G; gi++)
      for (f32 wy = 4; wy < px_h() - 4; wy += 15) {
        Vec2 p{band_x(gi, wy / TS) * TS + rng.range(-2, 2), wy};
        if (sample(dpath, p) < 1.7f) continue;
        Prop* wl = P.add(PK_WALL, RA.wall, p);
        if (!wl) break;
        wl->scale = rng.range(0.95f, 1.05f);
        wl->shadow = 1.0f;
        wl->reveal_w = 22;
        wl->reveal_h = 46;
        if (rng.chance(0.5f)) wl->flags |= PF_FLIP;
      }
  }

  // the edge woods (and the woods across the land, where those are the barrier)
  if (RA.edge.count)
    for (f32 gy = 0; gy < px_h(); gy += 38)
      for (f32 gxx = 0; gxx < px_w(); gxx += 40) {
        Vec2 p{gxx + rng.range(-14, 14) + (((int)(gy / 38)) % 2) * 20, gy + rng.range(-12, 12)};
        if (sample(forest, p) < 0.42f || sample(path, p) > 0.15f || !rng.chance(0.92f)) continue;
        if (def->barrier == BAR_WALL && near_gate(p, 70)) continue;
        bool in_band = false;
        for (int gi = 0; gi < G; gi++) in_band = in_band || absf(p.x - band_x(gi, p.y / TS) * TS) < 1.6f * TS;
        if (def->barrier == BAR_WALL && in_band) continue;
        // tall trees just below the path would hide it (and Malgrin's blockers): use low growth there
        bool hides_path = sample(path, p + Vec2{0, -36}) > 0.2f || sample(path, p + Vec2{0, -72}) > 0.2f;
        const PropSpec* s = hides_path && RA.bushes.count ? RA.bushes.pick(rng) : RA.edge.pick(rng);
        Prop* t = P.add(PK_EDGE, *s, p);
        if (!t) break;
        f32 k = rng.range(0.82f, 1.0f);
        t->tint = {k, k, k * 0.98f, 1};
        t->emit = rng.chance(0.15f) ? t->emit : (u8)EMIT_NONE;
      }

  // goblin camps: a tent, a campfire, a banner
  for (Vec2 c : camps) {
    Prop* t = P.add(PK_CAMP, S.game.tent, c + Vec2{rng.range(-10, 10), -36});
    if (t) { t->radius = 20; t->col_offset = {0, -8}; t->reveal_w = 26; t->reveal_h = 44; t->shadow = 0; if (rng.chance(0.5f)) t->flags |= PF_FLIP; P.mark_big(t, 30); }
    Prop* f = P.add(PK_CAMP, S.game.campfire, c);
    if (f) { f->radius = 10; f->col_offset = {0, 0}; f->light = 120; f->light_col = {1.0f, 0.62f, 0.3f, 1}; f->emit = EMIT_SPARKS; P.mark_big(f, 14); }
    Prop* bn = P.add(PK_CAMP, S.game.banner, c + Vec2{rng.chance(0.5f) ? -44.0f : 40.0f, rng.range(-26, -10)});
    if (bn) { bn->radius = 3; bn->col_offset = {0, -1}; bn->sway = 0.12f; P.mark_big(bn, 8); }
  }

  // the lair: torches and cursed crystals in a ring
  if (has_arena) {
    for (int i = 0; i < 14; i++) {
      f32 a = i * TAU / 14 + 0.11f;
      Vec2 p = arena + from_angle(a, arena_r + 6) * Vec2{1, 0.92f};
      if (sample(dpath, p) < 1.6f && p.x < arena.x) continue;  // leave the way in open
      bool torch = i % 2 == 0;
      Prop* t = P.add(PK_CAMP, torch ? S.game.torch : RA.bushes.items[0].sprite, p);
      if (!t) break;
      t->radius = torch ? 5 : 10;
      t->col_offset = {0, -2};
      t->light = torch ? 110 : 80;
      t->light_col = torch ? Color{1.0f, 0.6f, 0.3f, 1} : Color{0.72f, 0.42f, 1.0f, 1};
      t->emit = torch ? (u8)EMIT_SPARKS : (u8)EMIT_NONE;
      t->scale = torch ? 1.2f : 1.3f;
      P.mark_big(t, 16);
    }
  }

  // trees out in the open, in groves
  for (int tries = 0; tries < 2600 && RA.trees.count; tries++) {
    Vec2 p = P.random_point(4);
    if (sample(forest, p) > 0.15f || path_near(p, 46) || wet_near(p, 40) || !P.clear_of_keep(p, 50)) continue;
    if (fbm(p.x * 0.006f, p.y * 0.006f, seed + 3) < def->tree_threshold) continue;
    if (!P.clear_of_big(p, 56)) continue;
    Prop* t = P.add(PK_TREE, *RA.trees.pick(rng), p);
    if (!t) break;
    P.mark_big(t, 10 + t->reveal_w * 0.3f);
  }
  // bushes (or ferns, crystals, heather)
  for (int tries = 0; tries < 900 && RA.bushes.count; tries++) {
    Vec2 p = P.random_point(3.5f);
    if (sample(forest, p) > 0.35f || path_near(p, 28) || wet_near(p, 22) || !P.clear_of_keep(p, 30)) continue;
    if (!P.clear_of_big(p, 30)) continue;
    Prop* b = P.add(PK_BUSH, *RA.bushes.pick(rng), p);
    if (!b) break;
    P.mark_big(b, 14);
  }
  // rocks
  for (int tries = 0; tries < 260 && RA.rocks.count; tries++) {
    Vec2 p = P.random_point(4);
    if (sample(forest, p) > 0.3f || sample(path, p) > 0.2f || sample(water, p) > 0.35f || !P.clear_of_keep(p, 30)) continue;
    if (!P.clear_of_big(p, 28)) continue;
    Prop* k = P.add(PK_ROCK, *RA.rocks.pick(rng), p);
    if (!k) break;
    P.mark_big(k, max(12.0f, k->radius));
  }
  // logs, stumps, crystals, ruins
  for (int tries = 0; tries < 220 && RA.extras.count; tries++) {
    Vec2 p = P.random_point(5);
    if (sample(forest, p) > 0.2f || path_near(p, 40) || wet_near(p, 30) || !P.clear_of_keep(p, 40)) continue;
    if (!P.clear_of_big(p, 40)) continue;
    Prop* k = P.add(PK_EXTRA, *RA.extras.pick(rng), p);
    if (!k) break;
    P.mark_big(k, max(16.0f, k->radius));
  }

  // at the shore, and floating on the liquid
  for (int tries = 0; tries < 900 && RA.shore.count; tries++) {
    Vec2 p = P.random_point(2);
    f32 wv = sample(water, p);
    if (wv < 0.3f || wv > 0.62f || near_gate(p, 70) || sample(path, p) > 0.3f) continue;
    if (P.add(PK_SHORE, *RA.shore.pick(rng), p) == nullptr) break;
  }
  for (int tries = 0; tries < 400 && RA.floaters.count; tries++) {
    Vec2 p = P.random_point(2);
    if (sample(depth, p) < 0.3f) continue;
    if (P.add(PK_FLOATER, *RA.floaters.pick(rng), p) == nullptr) break;
  }
  // bits along the path
  for (int tries = 0; tries < 500 && RA.path_bits.count; tries++) {
    Vec2 p = P.random_point(1);
    if (sample(path, p) < 0.55f || !rng.chance(0.5f) || !P.clear_of_keep(p, 4)) continue;
    if (P.add(PK_PATHBIT, *RA.path_bits.pick(rng), p) == nullptr) break;
  }
  // mushrooms and ferns at the feet of trees
  for (int tries = 0; tries < 200 && RA.near_trees.count && P.big.count; tries++) {
    Vec2 base = P.big[rng.irange(0, P.big.count - 1)].p;
    Vec2 p = base + from_angle(rng.range(0, TAU), rng.range(18, 30));
    if (sample(path, p) > 0.2f || sample(water, p) > 0.2f || !P.clear_of_keep(p, 6)) continue;
    if (P.add(PK_NEAR, *RA.near_trees.pick(rng), p) == nullptr) break;
  }
  // flowers in clusters, thickest in the flower fields
  for (int tries = 0; tries < 1400 && RA.flowers.count; tries++) {
    Vec2 c = P.random_point(3);
    f32 l = sample(lush, c);
    if (!rng.chance(l > 0.3f ? 0.9f : 0.12f)) continue;
    if (sample(path, c) > 0.25f || sample(water, c) > 0.2f || sample(forest, c) > 0.5f) continue;
    const PropSpec* spec = RA.flowers.pick(rng);
    int n = rng.irange(4, 9);
    for (int k = 0; k < n; k++) {
      Vec2 p = c + rng.in_circle(26);
      if (sample(path, p) > 0.3f || sample(water, p) > 0.25f || !P.clear_of_keep(p, 4)) continue;
      if (!P.clear_of_big(p, 4)) continue;
      if (P.add(PK_FLOWER, *spec, p) == nullptr) break;
    }
  }
  // ground cover everywhere, more along the path edge
  for (int tries = 0; tries < 9000 && RA.cover.count; tries++) {
    Vec2 p = P.random_point(0.5f);
    f32 pv = sample(path, p), wv = sample(water, p), fv = sample(forest, p);
    if (pv > 0.45f || wv > 0.32f || fv > 0.75f) continue;
    if (has_arena && dist(p, arena) < arena_r) continue;
    bool edge = pv > 0.2f || (wv > 0.12f);
    if (!edge && !rng.chance(0.55f)) continue;
    Prop* g = P.add(PK_COVER, *RA.cover.pick(rng), p);
    if (!g) break;
    f32 k = rng.range(0.9f, 1.06f);
    g->tint = {k, k, k * 0.95f, 1};
  }

  build_ground();
  build_index(A);
}

void World::build_ground() {
  for (int i = 0; i < w * h; i++) {
    f32 wv = water.cells[i];
    f32 b = wv > 0.3f ? depth.cells[i] : forest.cells[i];
    u32 r = (u32)(saturate(path.cells[i]) * 255 + 0.5f), g = (u32)(saturate(wv) * 255 + 0.5f);
    u32 bb = (u32)(saturate(b) * 255 + 0.5f), a = (u32)(saturate(lush.cells[i]) * 255 + 0.5f);
    ground.cells[i] = r | (g << 8) | (bb << 16) | (a << 24);
  }
  ground_version++;
}

void World::build_index(Arena& A) {
  cw = (int)ceilf(px_w() / CELL);
  ch = (int)ceilf(px_h() / CELL);
  int cells = cw * ch;
  cell_start = A.make<int>(cells + 1);
  cell_items = A.make<int>(max(prop_count, 1));
  int* fillc = A.make<int>(cells);
  auto cell_of = [&](Vec2 p) { return clamp((int)(p.y / CELL), 0, ch - 1) * cw + clamp((int)(p.x / CELL), 0, cw - 1); };
  for (int i = 0; i < prop_count; i++) cell_start[cell_of(props[i].pos) + 1]++;
  for (int c = 0; c < cells; c++) cell_start[c + 1] += cell_start[c];
  for (int i = 0; i < prop_count; i++) {
    int c = cell_of(props[i].pos);
    cell_items[cell_start[c] + fillc[c]++] = i;
  }
}

Vec2 World::resolve(Vec2 p, f32 r, bool* hit, bool liquid_solid) const {
  Vec2 q = p;
  bool any = false;
  for (int iter = 0; iter < 3; iter++) {
    each_in(Rect{q.x - r - 40, q.y - r - 40, q.x + r + 40, q.y + r + 40}, [&](const Prop& pr, int) {
      if (pr.radius <= 0 || (pr.flags & PF_GONE)) return;
      Vec2 c = pr.pos + pr.col_offset;
      Vec2 d = q - c;
      f32 min_d = r + pr.radius;
      f32 l2 = length2(d);
      if (l2 >= min_d * min_d) return;
      f32 l = sqrtf(l2);
      q = c + (l > 1e-4f ? d / l : Vec2{1, 0}) * min_d;
      any = true;
    });
    Vec2 push{0, 0};
    for (int k = 0; k < 8; k++) {
      Vec2 dir = from_angle(k * (TAU / 8));
      f32 s = solid_at(q + dir * r, liquid_solid);
      if (s > 0.5f) push -= dir * (s - 0.5f);
    }
    if (solid_at(q, liquid_solid) > 0.5f) {  // deep inside: walk out along the gradient
      f32 gxx = solid_at(q + Vec2{4, 0}, liquid_solid) - solid_at(q - Vec2{4, 0}, liquid_solid);
      f32 gyy = solid_at(q + Vec2{0, 4}, liquid_solid) - solid_at(q - Vec2{0, 4}, liquid_solid);
      push -= normalize(Vec2{gxx, gyy}) * 1.5f;
    }
    if (length2(push) > 0) { q += push * r * 0.9f; any = true; }
  }
  q.x = clamp(q.x, TS, px_w() - TS);
  q.y = clamp(q.y, TS, px_h() - TS);
  if (hit) *hit = any;
  return q;
}

void World::draw(Renderer& r, f32 time, const Vec2* reveal, int reveal_count) const {
  const Color shadow_col{0.05f, 0.1f, 0.09f, 0.3f};
  Rect area{r.view.x0 - 110, r.view.y0 - 40, r.view.x1 + 110, r.view.y1 + 200};
  each_in(area, [&](const Prop& p, int idx) {
    if (p.flags & PF_GONE) return;
    f32 sx = (p.flags & PF_FLIP) ? -p.scale : p.scale;
    f32 alpha = 1.0f;
    if (p.reveal_w > 0) {
      for (int i = 0; i < reveal_count; i++) {
        Vec2 q = reveal[i];
        if (q.y < p.pos.y - 2 && q.y > p.pos.y - p.reveal_h && absf(q.x - p.pos.x) < p.reveal_w) alpha = p.reveal_h > 60 ? 0.42f : 0.6f;
      }
    }
    if (p.shadow > 0) {
      bool tall = p.reveal_h > 60;
      Vec2 off = tall ? Vec2{9 * p.scale, -1} : Vec2{2, 0};
      r.draw({.id = S.shadow, .pos = p.pos + off, .layer = LAYER_GROUND, .scale = {p.shadow, p.shadow * (tall ? 0.85f : 0.9f)}, .color = shadow_col});
    }
    Layer layer = (p.flags & PF_GROUND) ? LAYER_GROUND : LAYER_WORLD;
    Color tint = p.tint;
    tint.a *= alpha;
    if ((p.flags & PF_BURNABLE) && p.heat > 0) tint = mix(tint, Color{1.0f, 0.6f, 0.4f, tint.a}, saturate(p.heat) * 0.6f);
    r.draw({.id = p.sprite, .pos = p.pos, .sort_y = p.pos.y, .layer = layer, .scale = {sx, p.scale}, .color = tint, .sway = p.sway,
            .flash = (p.flags & PF_BURNING) ? 0.25f + 0.2f * sinf(time * 30) : 0.0f});
    if (p.flags & PF_BURNING) r.light(p.pos + Vec2{0, -14}, 110, {1.0f, 0.55f, 0.22f, 1}, 0.8f + 0.2f * sinf(time * 23 + idx));
    else if (p.light > 0 && !(p.flags & PF_BURNT)) {
      f32 flicker = p.kind == PK_CAMP ? 0.85f + 0.15f * sinf(time * 17 + idx) * sinf(time * 7.3f) : 0.92f + 0.08f * sinf(time * 1.7f + idx);
      f32 lift = min(18.0f, p.reveal_h * 0.3f + 6.0f);
      r.light(p.pos + Vec2{0, -lift}, p.light, p.light_col, (p.kind == PK_BLOCKER ? 0.3f : 0.55f) * flicker);
    }
  });
}

int World::blockers_left(int gate) const {
  int n = 0;
  for (int i = 0; i < prop_count; i++)
    if (props[i].kind == PK_BLOCKER && props[i].gate == gate && !(props[i].flags & PF_BURNT)) n++;
  return n;
}

int World::find_burnable(Vec2 from, Vec2 prefer_dir, f32 range) const {
  int best = -1;
  f32 best_score = 1e9f;
  for (int i = 0; i < prop_count; i++) {
    const Prop& p = props[i];
    if (!(p.flags & PF_BURNABLE) || (p.flags & (PF_BURNT | PF_BURNING))) continue;
    Vec2 d = p.pos - from;
    f32 l = length(d);
    if (l > range) continue;
    f32 facing = dot(d / max(l, 1.0f), prefer_dir);
    f32 score = l * (1.6f - facing * 0.6f);
    if (score < best_score) { best_score = score; best = i; }
  }
  return best;
}

}  // namespace sky
