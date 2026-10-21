# Nature Game (C++ / OpenGL ES 3)

Infinite procedural stylized nature world (green meadows, autumn forests, pine forests,
desert with dead trees and rock formations, winding paths) built from the Stylized Nature
MegaKit. Pure C++ on Android NativeActivity, no Java code.

## Flow
Animated intro and loading screen, then the main menu over a slowly orbiting view of the
world (PLAY, SETTINGS, QUIT). In game, the gear opens the pause menu (RESUME, SETTINGS, MAIN MENU).

## Survival loop
- Walk up to trees, rocks, pebbles and mushrooms. The crosshair turns yellow when something is in reach. Hold HIT to chop or mine. Bare hands work (slowly).
- BAG: inventory, crafting (axe, pickaxe, sword, torch, campfire, workbench, bed, chest) and cooking next to a campfire.
- MAKE: build menu. Solid wood and stone walls (door and window variants), floors with foundations, fence, roof, roof ridge and a full-storey staircase. Pieces snap to a 2 m grid. TURN flips, DEL removes with a full refund, UP and DOWN raise the build level by one storey so you can put a second floor on top of the walls.
- Roofs: place ROOF pieces on the outer wall edges (TURN picks which side they cover) and ROOF RIDGE on the middle line.
- Animals: deer, rabbits, cows and foxes wander (flee when approached). Boars and bears fight back. Wolves hunt at night. Torches and campfires scare wolves and bears. Kill animals for raw meat and cook it.
- Hunger drains, starving costs health, a day/night cycle runs (about 14 min), sleep in a bed at night. Everything is saved automatically.

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

## Rebuilding the asset pack
```
assimp export EverythingLibrary_Animals_001.fbx Library.gltf
python tools/pack_assets.py --animals-gltf Library.gltf [other source dirs, see --help]
```
