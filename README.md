# Nature Game (C++ / OpenGL ES 3)

Infinite procedural stylized nature world (green meadows, autumn forests, pine forests,
desert with dead trees and rock formations, winding paths) built from the Stylized Nature
MegaKit. Pure C++ on Android NativeActivity, no Java code.

## Flow
Animated intro and loading screen, then the main menu over a slowly orbiting view of the
world (PLAY, SETTINGS, QUIT). In game, the gear opens the pause menu (RESUME, SETTINGS, MAIN MENU).

## Controls
- Left joystick (fixed position by default, switchable to floating in Settings)
- Drag anywhere else to look (speed adjustable in Settings)
- Round button bottom-right: jump
- Settings: graphics AUTO / LOW / MED / HIGH, joystick FIXED / FLOATING, look speed, FPS counter

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
