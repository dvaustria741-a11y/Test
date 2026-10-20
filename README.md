# Nature Game (C++ / OpenGL ES 3)

Infinite procedural stylized nature world (green meadows, autumn forests, pine forests,
desert with dead trees and rock formations, winding paths) built from the Stylized Nature
MegaKit. Pure C++ on Android NativeActivity, no Java code.

## Flow
Animated intro and loading screen, then the main menu over a slowly orbiting view of the
world (PLAY, SETTINGS, QUIT). In game, the gear opens the pause menu (RESUME, SETTINGS, MAIN MENU).

## Survival loop
- Walk up to trees, rocks, pebbles and mushrooms. The crosshair turns yellow when something is in reach. Hold HIT to chop or mine.
- Bare hands work (slowly). Craft an AXE and PICKAXE in the BAG for faster work.
- Craft a CAMPFIRE, WORKBENCH, BED and CHEST and place them with PLACE. Cook mushrooms next to a campfire.
- MAKE opens the build menu: wood and stone floors, walls, door and window walls, fence, roof. Pieces snap to a 2 m grid, TURN flips them, DEL removes (full refund).
- Hunger drains over time, starving costs health, eating heals. A full day/night cycle runs (about 14 min); sleep in a bed at night to skip to morning.
- Everything is saved automatically (position, inventory, built pieces, chopped trees).

## Controls
- Left joystick (fixed by default, floating in Settings), drag anywhere else to look
- HIT / PLACE / EAT big button, JUMP, hotbar at the bottom (tap a slot)
- BAG (inventory + crafting), MAKE (build menu), gear (pause and settings)

## Performance tiers
Detected from the GPU name, RAM and core count, then adapted live with dynamic resolution
scaling (and an automatic tier drop if the phone cannot hold the target FPS).

| Tier | Target | View | Shadows | Textures | Render scale |
|------|--------|------|---------|----------|--------------|
| Low  | 30 fps | 150 m | off | 256 px | 55-80% |
| Medium | 60 fps | 280 m | off | 512 px | 65-90% |
| High | 60 fps | 450 m | sun shadow map | 1024 px | 80-100% |

Trees use 3 LOD levels, grass and props are instanced and distance-faded, terrain is
chunked (64 m) with 3 LODs and streamed on a worker thread.

## Build
Push to `main`. GitHub Actions builds a signed APK (arm64-v8a and armeabi-v7a) and
publishes it as the `latest` pre-release plus a workflow artifact.

## Assets
`app/src/main/assets/props.pak` and `tex/*.ntx` are generated from the original glTF and
PNG packs by `tools/pack_assets.py` (mesh LODs via meshoptimizer, ETC2 textures with
mipmaps). Kit license: see the original `License_Standard.txt`.
