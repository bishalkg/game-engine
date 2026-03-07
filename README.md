# game_engine

2D SDL3-based game project split into an `engine` static library and a `game` app.

## Repo Layout

```text
engine/
  include/engine/
    engine.h
    igame_rules.h
    ui_manager.h
    net/
  src/
game/
  include/game/
    app.h
    game_rules.h
    ui_controller.h
    level_manifest.h
  src/
    main.cpp
    app.cpp
    game_rules.cpp
    ui_controller.cpp
    level_manifest.cpp
```

## Build

Configure:

```bash
cmake --preset default
```

Build engine + game app:

```bash
cmake --build build --target game
```

Run from build output:

```bash
./build/game.app/Contents/MacOS/game
```

## macOS App Bundle Packaging

This project includes a dedicated bundle target that creates a standalone `.app` with:

- executable in `Contents/MacOS`
- game assets in `Contents/Resources/data`
- runtime dylibs in `Contents/Frameworks` with fixed install names

Build the bundle:

```bash
cmake --preset default
cmake --build build --target bundle_game
```

Bundle output:

```text
build/game.app
```

### Bundle Layout and File Roles

```text
build/game.app/
  Contents/
    Info.plist
    MacOS/
      game
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
- `Contents/MacOS/game`
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
test -d build/game.app && echo OK
```

Check a packaged asset:

```bash
test -f build/game.app/Contents/Resources/data/maps/level_1/level_1.tmx && echo OK
```

Inspect dylib linkage:

```bash
otool -L build/game.app/Contents/MacOS/game
```

You should see SDL and related libs resolved from:

```text
@executable_path/../Frameworks/
```

Launch bundle:

```bash
open "build/game.app"
```

## Packaging Implementation Files

- `CMakeLists.txt`
  - macOS bundle properties
  - resource copy step
  - `bundle_game` target
- `cmake/MacBundle.plist.in`
  - Info.plist template
- `cmake/FixupBundle.cmake.in`
  - `fixup_bundle(...)` script used by `bundle_game`
- `game/src/app.cpp`
  - runtime cwd switch to `Contents/Resources` when launched from bundle
