# The Skyward Spell

A storybook adventure written in C++ and played in the browser. The goblin sorcerer Malgrin has hexed the wings of Flint, a little green dragon. Play as Mira or Theo, cross six lands, talk to the people who live there, and gather the five ingredients of the Skyward Spell so Flint can fly again.

**Play it:** https://shay2222.github.io/skyward-spell/
**Project page:** https://shay2222.github.io/skyward-spell-portfolio/

## How to play

| Keyboard | Gamepad | Touch | Action |
| --- | --- | --- | --- |
| WASD or arrow keys | Left stick or d-pad | Drag on the left half | Walk |
| Space or Shift | A or RB | Dash | Dash |
| F or K | X or RT | Fire | Ask Flint to breathe fire |
| E or Enter | B | Talk | Talk to someone nearby |
| Esc or P | Start | Pause | Pause, volume and controls |
| 1 to 6 | | Tap a choice | Pick a question in a conversation |
| T | | | Open the dialogue inspector |

Malgrin has blocked every land's crossings with walls of thorns, and with barricades in his own Hollow; Flint's fire clears them. Goblins chase you, and Flint sends them home by himself. Each land has its own hazards (mud, vents, snapjaws, cliff-edge gusts, rune traps), hidden hearts, and waystones that remember where you wake up if you faint. Your progress is saved after each land.

## The story

1. **Willowmere Meadows**: the Scroll of Knowledge
2. **Mistfen Marsh**: the Water of Miracles
3. **Cinderpeak Crags**: the Fire of Power
4. **The Elderwood**: the Elder Staff
5. **Skyreach Cliffs**: the Feather of the Four Winds
6. **Malgrin's Hollow**: cast the spell in Malgrin's own rune circle, then face him

Malgrin is shielded until he wears himself out. When he's dazed, Flint can dive in. Three hits and the sky is free.

## Characters and generative dialogue

Twelve characters live along the way, two in each land: Granny Willa, Tumble, Old Mossback, Wren, Cobble, Ember, Sage Rowan, Hoot, Captain Gale, Lumi, Grizelda and Nib. You can pick a suggested question or type anything.

The game owns every fact; a writer only chooses the words.

1. **Understand**: `Brain::classify` works out what the player is asking: the way, danger, help, Malgrin, small talk, or an attempt to trick the character.
2. **Gather facts**: `make_facts` reads the game state from the speaker's position: the compass direction and distance to the shrine, closed crossings, the nearest goblin camp, hearts, and whether the speaker has a gift left to give.
3. **Write**: a line is written either by a live language model running in the player's browser or by the offline generator, which assembles one from the character's own phrases and verbal tics.
4. **Guard**: `Brain::guard` checks the line against the facts:
   - length
   - off-world or unsafe words
   - names that don't exist in the world
   - ingredients placed in the wrong land
   - directions that disagree with the map
   - gift promises the game hasn't approved
   - spoilers
   - repeated lines

   A failed line is retried and then replaced with a safe line.
5. **Act**: a line can suggest an action (give a heart, show a heart, show the goal), but only the game decides whether it happens.

Tutorial lines, objectives and boss cues are hand-written and deterministic. Conversations and Flint's small talk are generated.

Press **T** in the game to open the dialogue inspector. It logs every decision: the speaker, the intent, the source (live model or offline generator) and the guard's verdict with its reason. **Run guard tests** sends known-bad lines through the guard.

### The optional live model

By default, dialogue comes from the offline generator, which needs no download and works everywhere. From the inspector you can turn on a small open model, **Llama 3.2 1B** or **Qwen 2.5 0.5B**. It runs entirely in your browser on your graphics card through [WebLLM](https://github.com/mlc-ai/web-llm) and WebGPU. It is free, needs no account, and nothing you type leaves your computer.

- **Requirements:** a WebGPU-capable browser (recent Chrome or Edge on desktop). The first load is a one-time download of several hundred megabytes.
- **How it plugs in:** C++ builds the prompt (`Brain::build_prompt`) and asks for JSON (`{"say": ..., "action": ...}`). C++ parses the reply (`Brain::parse_reply`) and passes it through the same guard. Anything slow (over about 18 seconds), malformed or rejected falls back to the offline generator, so the game never depends on the model.
- **Testing status:** the integration was tested with a stand-in model that returns good, bad, non-JSON and over-generous replies. A real model download could not be tested in the build environment.

## How it's built

The whole game is freestanding C++20, compiled to WebAssembly with nothing but clang: no Emscripten, no engine and no C++ standard library. A thin JavaScript host forwards input, uploads the buffers the C++ prepares, draws them with WebGL 2, and plays the audio the C++ synthesises.

- **Art painted in code.** At startup a small vector painter (`paint.h`) bakes every sprite into one 2048×2048 atlas using signed distance fields, with anti-aliased edges, outlines and soft shading. The shared art is baked once, and each land's scenery and characters are re-baked when you arrive (about 170 sprites in roughly half a second). No image files ship with the game.
- **Paper-doll animation.** Mira, Theo, Flint, Malgrin and the goblins are built from separate parts posed every frame. Hair, scarves and tails are spring chains.
- **Generated lands.** `world.cpp` builds each land from a seed and rules:
  - a winding spline path
  - barrier bands with one guaranteed crossing
  - liquids with depth and shores
  - goblin camps and hazards near the path
  - hidden hearts and waystones
  - thousands of props
- **Rendering.** The C++ side sorts every sprite by depth with a radix sort and writes one vertex buffer per frame. The WebGL host then draws:
  - the ground in a shader from per-land palettes: grass, paths, and water, bog, lava, sky and goo
  - a light map with self-lit lava and crystals
  - drifting fog, bloom and a colour grade
- **Simulation.** Gameplay runs at a fixed 120 steps per second with interpolation, so movement is smooth at any refresh rate. Goblins and Malgrin are state machines; Flint follows your trail, helps in fights and reacts to what you do.
- **Music and sound.** `audio.cpp` is a small synthesiser built from bells, plucked strings (Karplus–Strong), flute, pads, bass, celesta and drums, with a Schroeder reverb. It plays eight original songs and twenty-one sound effects. The page queues the rendered samples on the Web Audio clock.
- **Performance.** The C++ side uses about 1 ms per frame. `skyward.wasm` is about 1.15 MB.

## Files

| Path | What it does |
| --- | --- |
| `src/core.h`, `src/text.h` | Types, maths, random numbers, noise, memory arena, containers, text helpers |
| `src/paint.h`, `src/atlas.h` | The vector painter and the atlas packer |
| `src/art.h`, `src/art_*.cpp` | Every sprite in the game, painted in code |
| `src/render.h`, `src/render.cpp` | Sprite batching, depth sorting, lights |
| `src/regions.h`, `src/regions.cpp` | The six lands: palettes, rules, ingredients |
| `src/world.h`, `src/world.cpp` | Land generation, props and collision |
| `src/hero.*`, `src/dragon.*`, `src/anim.h` | Mira and Theo, Flint, and the animation helpers |
| `src/actors.*`, `src/boss.*` | Goblins, hazards, hearts and waystones; Malgrin's fight |
| `src/npc.*`, `src/brain.*` | The twelve characters; intent, facts, generator, guard and prompt |
| `src/audio.*` | The synthesiser, songs and sound effects |
| `src/fx.*` | Particles, fireflies and light motes |
| `src/game.cpp` | The game loop, story scenes, camera, travel, conversations, and the functions the page calls |
| `docs/index.html` | The page: title screen, HUD, story cards, conversation panel, inspector, pause menu, touch controls |
| `docs/host.js` | WebGL 2 drawing, input, audio output |
| `docs/llm.js` | The optional in-browser model (WebLLM) |
| `docs/skyward.wasm` | The built game |

## Building

You need clang with the WebAssembly target (clang 15 or newer).

```sh
./build.sh                         # writes docs/skyward.wasm
python3 -m http.server -d docs     # then open http://localhost:8000
```

The site is served by GitHub Pages from the `docs/` folder.
