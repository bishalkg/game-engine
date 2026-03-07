# game_engine

2D SDL3-based game project with ImGui UI, SDL_mixer audio, SDL_ttf text, and tiled-map driven levels.

## Build

Configure:

```bash
cmake --preset default
```

Build game executable:

```bash
cmake --build build --target game_engine
```

Run from build output:

```bash
./build/game_engine
```

## macOS App Bundle Packaging

This project includes a dedicated bundle target that creates a standalone `.app` with:

- executable in `Contents/MacOS`
- game assets in `Contents/Resources/data`
- runtime dylibs in `Contents/Frameworks` with fixed install names

Build the bundle:

```bash
cmake --preset default
cmake --build build --target bundle_game_engine
```

Bundle output:

```text
build/game_engine.app
```

### Bundle Layout and File Roles

```text
build/game_engine.app/
  Contents/
    Info.plist
    MacOS/
      game_engine
    Resources/
      data/
        maps/
        players/
        enemies/
        audio/
        cutscenes/
        tiles/
        ...
    Frameworks/
      libSDL3.0.dylib
      libSDL3_image.0.dylib
      libSDL3_mixer.0.dylib
      libSDL3_ttf.0.dylib
      libglm.dylib
      libfreetype.6.dylib
      libpng16.16.dylib
      ...
```

- `Contents/Info.plist`
  - macOS bundle metadata (name, identifier, version).
  - Generated from `cmake/MacBundle.plist.in`.
- `Contents/MacOS/game_engine`
  - Main executable built from this repo.
- `Contents/Resources/data`
  - Full copy of the repo `data/` folder.
  - All runtime asset paths in code (`data/...`) resolve here.
- `Contents/Frameworks`
  - Dynamic libraries copied during bundle fixup.
  - Install names rewritten to `@executable_path/../Frameworks/...`.

### Runtime Path Behavior

At startup, `App::Run()` detects if it is running from `.app/Contents/MacOS` and changes current working directory to `.app/Contents/Resources`.  
This keeps existing relative asset paths (for example `data/cutscenes/fonts/...`) working without changing all loaders.

### Verify the Bundle

Check bundle exists:

```bash
test -d build/game_engine.app && echo OK
```

Check a packaged asset:

```bash
test -f build/game_engine.app/Contents/Resources/data/maps/level_1/level_1.tmx && echo OK
```

Inspect dylib linkage:

```bash
otool -L build/game_engine.app/Contents/MacOS/game_engine
```

You should see SDL and related libs resolved from:

```text
@executable_path/../Frameworks/
```

Launch bundle:

```bash
open "build/game_engine.app"
```

## Packaging Implementation Files

- `CMakeLists.txt`
  - macOS bundle properties
  - resource copy step
  - `bundle_game_engine` target
- `cmake/MacBundle.plist.in`
  - Info.plist template
- `cmake/FixupBundle.cmake.in`
  - `fixup_bundle(...)` script used by `bundle_game_engine`
- `app.cpp`
  - runtime cwd switch to `Contents/Resources` when launched from bundle
