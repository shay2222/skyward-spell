// regions.h - the six lands of the journey: how each looks, what blocks the way, and what it hides.
#pragma once
#include "core.h"

namespace sky {

enum Region : u8 { R_MEADOW, R_MARSH, R_CRAGS, R_ELDERWOOD, R_SKYREACH, R_HOLLOW, R_COUNT };

/** What fills the low ground: ordinary water, bog, lava, open sky below cliffs, or Malgrin's goo. */
enum Liquid : u8 { LQ_WATER, LQ_BOG, LQ_LAVA, LQ_SKY, LQ_GOO };

/** What runs across the land between sections, leaving one gap that Malgrin has blocked. */
enum Barrier : u8 { BAR_TREES, BAR_LIQUID, BAR_WALL };

/** Things in each land that can hurt or hinder. */
enum Hazard : u8 { HZ_NONE, HZ_MUD, HZ_VENT, HZ_SNAPJAW, HZ_GUST, HZ_RUNE };

/** Ambient particles that make each land feel alive. */
enum Ambience : u8 { AMB_POLLEN, AMB_SPORES, AMB_ASH, AMB_FIREFLIES, AMB_WIND, AMB_CURSE };

/** Colours the ground shader paints with; mirrored in docs/host.js. */
struct Palette {
  Color grass_a, grass_b, grass_c;  // ground: base, shade, highlight
  Color floor;                      // dark ground under dense woods
  Color dirt_a, dirt_b;             // the path
  Color shore;                      // the band where land meets liquid
  Color shallow, deep;              // liquid colours
  Color foam;                       // liquid edge
  Color glow;                       // emissive tint (lava, goo)
  Color fog;
  f32 fog_amount;
};

struct RegionDef {
  const char* name;
  const char* subtitle;
  const char* ingredient;       // what this land hides ("" for the last land)
  const char* ingredient_short;
  Palette pal;
  Color ambient;
  f32 bloom, vignette, saturation;
  Color grade;
  Liquid liquid;
  Barrier barrier;
  Hazard hazard;
  Ambience ambience;
  int width, height;            // tiles
  int gates;                    // blocked crossings between sections
  int goblins;                  // goblins per camp
  int ponds;
  f32 pond_r0, pond_r1;
  f32 tree_threshold;           // higher = fewer trees in the open
  int hazards;                  // how many hazards to place
};

extern const RegionDef REGIONS[R_COUNT];
constexpr int INGREDIENT_COUNT = 5;

}  // namespace sky
