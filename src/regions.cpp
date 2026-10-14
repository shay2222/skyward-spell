#include "regions.h"

namespace sky {

namespace {
constexpr Color C(f32 r, f32 g, f32 b) { return Color{r, g, b, 1}; }
}  // namespace

const RegionDef REGIONS[R_COUNT] = {
    {
        "Willowmere Meadows", "Chapter one", "the Scroll of Knowledge", "Scroll",
        {C(0.50f, 0.75f, 0.32f), C(0.36f, 0.60f, 0.26f), C(0.68f, 0.81f, 0.39f), C(0.22f, 0.34f, 0.19f),
         C(0.83f, 0.69f, 0.50f), C(0.72f, 0.57f, 0.40f), C(0.85f, 0.79f, 0.59f),
         C(0.42f, 0.82f, 0.80f), C(0.17f, 0.50f, 0.64f), C(0.93f, 0.98f, 0.97f), C(0, 0, 0), C(0.9f, 0.95f, 0.9f), 0.0f},
        C(1.0f, 0.97f, 0.9f), 0.28f, 0.32f, 1.06f, C(1.02f, 1.0f, 0.97f),
        LQ_WATER, BAR_TREES, HZ_NONE, AMB_POLLEN,
        84, 50, 2, 2, 3, 3.0f, 5.0f, 0.53f, 0,
    },
    {
        "Mistfen Marsh", "Chapter two", "the Water of Miracles", "Water",
        {C(0.47f, 0.58f, 0.33f), C(0.33f, 0.44f, 0.26f), C(0.58f, 0.63f, 0.36f), C(0.20f, 0.26f, 0.19f),
         C(0.52f, 0.45f, 0.33f), C(0.42f, 0.36f, 0.27f), C(0.40f, 0.37f, 0.27f),
         C(0.33f, 0.50f, 0.40f), C(0.13f, 0.27f, 0.23f), C(0.62f, 0.73f, 0.58f), C(0.55f, 1.0f, 0.75f), C(0.74f, 0.84f, 0.78f), 0.34f},
        C(0.86f, 0.93f, 0.88f), 0.34f, 0.38f, 0.94f, C(0.98f, 1.02f, 0.99f),
        LQ_BOG, BAR_LIQUID, HZ_MUD, AMB_SPORES,
        88, 52, 2, 3, 6, 2.5f, 4.5f, 0.5f, 14,
    },
    {
        "Cinderpeak Crags", "Chapter three", "the Fire of Power", "Fire",
        {C(0.34f, 0.30f, 0.30f), C(0.24f, 0.21f, 0.22f), C(0.44f, 0.37f, 0.33f), C(0.16f, 0.14f, 0.15f),
         C(0.55f, 0.50f, 0.45f), C(0.44f, 0.40f, 0.37f), C(0.13f, 0.10f, 0.10f),
         C(1.0f, 0.62f, 0.18f), C(0.78f, 0.18f, 0.05f), C(1.0f, 0.92f, 0.55f), C(1.0f, 0.45f, 0.1f), C(0.52f, 0.36f, 0.30f), 0.12f},
        C(0.74f, 0.62f, 0.57f), 0.42f, 0.42f, 1.04f, C(1.05f, 0.98f, 0.94f),
        LQ_LAVA, BAR_LIQUID, HZ_VENT, AMB_ASH,
        88, 52, 3, 3, 4, 2.5f, 4.0f, 0.62f, 14,
    },
    {
        "The Elderwood", "Chapter four", "the Elder Staff", "Staff",
        {C(0.30f, 0.48f, 0.26f), C(0.20f, 0.34f, 0.20f), C(0.42f, 0.54f, 0.29f), C(0.15f, 0.21f, 0.14f),
         C(0.45f, 0.38f, 0.27f), C(0.35f, 0.29f, 0.21f), C(0.36f, 0.33f, 0.24f),
         C(0.25f, 0.50f, 0.47f), C(0.07f, 0.22f, 0.26f), C(0.70f, 0.86f, 0.80f), C(0.4f, 0.8f, 1.0f), C(0.56f, 0.70f, 0.62f), 0.16f},
        C(0.74f, 0.82f, 0.76f), 0.4f, 0.42f, 1.0f, C(0.97f, 1.02f, 1.0f),
        LQ_WATER, BAR_TREES, HZ_SNAPJAW, AMB_FIREFLIES,
        90, 54, 3, 3, 3, 2.5f, 4.0f, 0.42f, 12,
    },
    {
        "Skyreach Cliffs", "Chapter five", "the Feather of the Four Winds", "Feather",
        {C(0.66f, 0.80f, 0.46f), C(0.52f, 0.68f, 0.40f), C(0.80f, 0.87f, 0.56f), C(0.42f, 0.50f, 0.38f),
         C(0.85f, 0.81f, 0.71f), C(0.73f, 0.69f, 0.61f), C(0.50f, 0.46f, 0.43f),
         C(0.72f, 0.86f, 0.99f), C(0.40f, 0.60f, 0.92f), C(1.0f, 1.0f, 1.0f), C(0, 0, 0), C(0.92f, 0.95f, 1.0f), 0.1f},
        C(1.02f, 1.02f, 1.06f), 0.3f, 0.28f, 1.04f, C(0.99f, 1.0f, 1.03f),
        LQ_SKY, BAR_LIQUID, HZ_GUST, AMB_WIND,
        90, 54, 3, 3, 5, 3.0f, 5.5f, 0.6f, 0,
    },
    {
        "Malgrin's Hollow", "The last chapter", "", "",
        {C(0.36f, 0.31f, 0.38f), C(0.26f, 0.22f, 0.29f), C(0.45f, 0.37f, 0.43f), C(0.17f, 0.14f, 0.19f),
         C(0.52f, 0.45f, 0.44f), C(0.41f, 0.35f, 0.35f), C(0.20f, 0.14f, 0.22f),
         C(0.78f, 0.38f, 1.0f), C(0.32f, 0.10f, 0.45f), C(0.96f, 0.74f, 1.0f), C(0.75f, 0.4f, 1.0f), C(0.46f, 0.36f, 0.56f), 0.18f},
        C(0.74f, 0.66f, 0.86f), 0.45f, 0.42f, 1.0f, C(1.0f, 0.97f, 1.04f),
        LQ_GOO, BAR_WALL, HZ_RUNE, AMB_CURSE,
        92, 50, 2, 4, 4, 2.5f, 4.0f, 0.62f, 10,
    },
};

}  // namespace sky
