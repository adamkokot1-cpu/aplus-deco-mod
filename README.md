# A+ Deco Mod (Geometry Dash, Geode)

An AI that decorates your Geometry Dash levels. In the editor, press **A+**, type a description like
*"neon cyber city with pink and cyan glow"*, choose how many objects (1 to 5000 by default), and press **Generate**.

- Target: **Geode SDK v5** and **Geometry Dash 2.2081** (Windows, macOS, Android, iOS)
- Language: C++23

---

## 1. Build it (pick one)

### Option A: GitHub (easiest, nothing to install)
1. Create a new GitHub repository and upload everything in this folder (keep the `.github` folder).
2. Open the **Actions** tab. The "Build Geode Mod" workflow runs on its own (or press *Run workflow*).
3. When it finishes (about 10 minutes), the finished `aplusdev.aplus-deco.geode` is saved in the repo's
   `build-output/` folder, and also as the **Build Output** download on the Actions run page.
   If a platform failed, its error log is in `build-output/` too.

### Option B: Your own computer
1. Install the Geode CLI and SDK: https://docs.geode-sdk.org/getting-started/
2. In this folder, run:
   ```
   geode build
   ```
3. The `.geode` file appears in `build/`.

## 2. Install it
Copy `aplusdev.aplus-deco.geode` into your Geode mods folder
(Geode menu in GD > the folder icon), then restart Geometry Dash.

## 3. Use it
1. Open a level in the editor and press the **A+** button. It sits in the right-hand edit button column, or under undo/redo if the node-ids mod isn't installed.
2. Type a description (or press **Idea** for a random one).
3. Set **Objects**: type a number, use **- / +**, or press **100 / 500 / 2K**.
4. Pick an **Area**: *Whole level*, *Current view*, or *Around selection*.
5. Press **Generate**. Don't like it? Press **Undo Last** (or Ctrl+Z) and generate again. Every run is different.

**AI Plan** shows what the AI understood: theme, colors, mix, and how many objects went where.

### What you can write
| You write | The AI does |
|---|---|
| neon, cyber, synthwave, city | Neon City: glow, rings, poles, skyline |
| forest, jungle, garden, fairy | Enchanted Forest: grass, bushes, fireflies |
| lava, hell, fire, demon | Inferno: spikes, chains, embers |
| ice, snow, winter, frozen | Frozen Peaks: icicles, snow, diamonds |
| space, galaxy, cosmic, night | Cosmic Void: stars, rings, bokeh |
| spooky, haunted, graveyard, ghost | Haunted: chains, thorns, toxic green |
| ocean, underwater, sea, coral | Deep Ocean: bubbles, seaweed |
| cute, love, pastel, candy | Sweet Dream: hearts, stars, clouds |
| party, rave, music, disco | Party Rave: music notes, pulses |
| desert, egypt, temple, sand | Golden Desert: diamonds, rods |
| minimal, clean, modern | Clean Minimal: rings, dots, skyline |
| castle, dungeon, cave | Dark Dungeon: chains, torches |
| factory, metal, robot, steam | Industrial: chains, poles |

- **Two themes** mix: *"spooky neon city"*
- **Colors**: red, pink, cyan, purple, gold, teal... plus *dark / light / vivid*, or *rainbow*
- **Objects**: stars, hearts, notes, rings, bubbles, orbs, gems, particles, embers, clouds, chains, grass, bushes, spikes, icicles, poles, lamps, pillars, towers
- **No / without / don't**: *"no clouds"*, *"without chains"*, *"no glow"*
- **Size**: huge, big / small, tiny
- **Spread**: dense, lots, busy / sparse, subtle, airy
- **Rotation**: chaotic, wild / neat, tidy
- **Mood**: dark, gloomy

### Settings (Geode > A+ Deco Mod > Settings)
| Setting | Default | What it does |
|---|---|---|
| Default Object Count | 300 | Starting count in the window |
| Max Object Count | 5000 | Safety cap per generation |
| Theme Colors On By Default | on | Adds 5 color triggers at the level start |
| First Color Channel | 50 | Uses channels 50 to 54. Change it if your level already uses them |
| Deco Group ID | 0 | Put all generated deco in one group (0 = off) |
| Discover Extra Objects | on | Also uses other deco objects found in your game |
| Keep Window Open | off | Keep the window open after generating |
| Use Claude AI By Default | off | Start with the Claude toggle on |
| Claude API Key | (empty) | Your Anthropic key, for Claude mode |
| Claude Model | claude-sonnet-5-5 | Model used in Claude mode |

### Claude AI (optional, online)
The built-in AI works offline with no key. For smarter theme design, get an API key at
https://console.anthropic.com, paste it in **Claude API Key**, and turn on **Claude AI (online)** in the A+ window.
Claude designs the theme, colors and mix, and the mod still places the objects locally.
If the request fails, the built-in AI takes over and the status line says why.
API usage is billed to your Anthropic account. One generation is one short request.

---

## How it works
1. **Brain** (`src/core/DecoBrain.cpp`, or `src/ClaudeBrain.cpp` online) turns your words into a *plan*: theme, 5 colors, glow, how the budget is split between ground / hanging / floating / sky / skyline, which objects to favor, size, spin, and clustering.
2. **Level scan** (`src/DecoEngine.cpp`) maps your level onto a 30x30 grid of solid blocks and gameplay objects.
3. **Layout** (`src/core/Layout.cpp`) finds spots: block tops and the floor, undersides of ceilings, air near the gameplay (thinner inside tunnels so the path stays readable), and the sky band. It places exactly N objects.
4. **Placement** builds a GD object string and creates everything in one go as a single undo step, plus color triggers at x = 15.

Safety: deco objects have no hitbox, and skyline blocks are *no-touch*, so gameplay never changes.
Objects outside the built-in verified ID list are only used if the game itself reports them as decoration.

## Tests
The core logic (brain + layout + object strings) is plain C++ and has its own tests:
```
cd tests
g++ -std=c++23 -O2 -I../src/core ../src/core/*.cpp test_core.cpp -o test_core && ./test_core
```

## Before publishing
- Change `"developer"` and the first part of `"id"` in `mod.json` (`aplusdev`) to your own name.
- If a newer GD or Geode version is out, update `"gd"` and `"geode"` in `mod.json`.
