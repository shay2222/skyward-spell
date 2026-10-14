// npc.h - the people and creatures met on the journey: who they are, how they talk, and the
// lines that belong only to them. What they *know* about the world right now comes from the
// game state (see brain.h), so nothing they say can drift out of date.
#pragma once
#include "art.h"

namespace sky {

struct NpcDef {
  const char* name;
  const char* role;          // "the beekeeper of Willowmere"
  Region land;
  int section;               // which stretch of the land they stand in
  const char* persona;       // who they are and how they talk (also the heart of the model prompt)
  const char* tic;           // a word or phrase they use now and then
  const char* greet_first[3];
  const char* greet_again[2];
  const char* self[2];       // about themselves
  const char* place[2];      // about where they live
  const char* malgrin[2];
  const char* flint[2];
  const char* bye[2];
  const char* gift;          // when they hand over a heart
};

extern const NpcDef NPCS[NPC_COUNT];

/** Two people stand in each land. */
inline int npcs_in(Region r, int out[2]) {
  int n = 0;
  for (int i = 0; i < NPC_COUNT && n < 2; i++)
    if (NPCS[i].land == r) out[n++] = i;
  return n;
}

}  // namespace sky
