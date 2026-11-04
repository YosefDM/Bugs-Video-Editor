# BUGS! Video Editor — Workspace Instructions

BUGS! is a fork of Shotcut for one non-technical user, Levi. It opens into **Simple mode**; the full Shotcut interface stays under **Advanced**. The product brief and mockups are in `docs/bugs/`.

## Simple mode

| Where | What |
|-------|------|
| `src/simple/simpleedit.*` | `Simple::Edit`, a plain value holding everything Levi changed, and `compileXml()` |
| `src/simple/collage.*` | Collage layout, ported from PhotoCollage; its tests are in `tests/test_collage.cpp` |
| `src/simple/simplemedia.*` | Opening files, thumbnails, filmstrip, caption picture, single-frame rendering |
| `src/simple/simplecontroller.*` | All Simple mode state and actions, exposed to QML as `simple` |
| `src/simple/simplemodewidget.*` | Lays out the views; borrows the video widget from the player |
| `src/qml/views/simple/` | The screens. Colours and icons are in `Theme.js` |

Rules that keep the product's promises:

- **Originals are read-only.** Nothing writes to `Edit::sourcePath`. Saves go to a `.part.` file in the Finished folder, are checked, then renamed with `uniquePath()`.
- **Preview matches the saved file** because both load the same `compileXml()` output. Do not add effects to only one path.
- **Finish never asks questions.** Output is always H.264/AAC MP4.

## Developing BUGS!

- **Linux** (this repo's devcontainer): `scripts/bugs/setup-dev-linux.sh` installs Qt 6.10.3, builds FFmpeg `release/9.0` and MLT `v7.42.0` (the versions the Windows release uses), configures `build/dev`, and writes `scripts/bugs/env.sh`. Then `source scripts/bugs/env.sh && cmake --build build/dev && cmake --install build/dev`.
- **Tests:** `ctest --test-dir build/dev -R simple --output-on-failure`, with a display such as `Xvfb :99 &`. They are `simple_edit` (`tests/test_simple_edit.cpp`), `simple_collage` (`tests/test_collage.cpp`) and `simple_pipeline` (`tests/test_simple_pipeline.cpp`, which saves real files and checks them).
- **Format** C++ with `clang-format-14 -i <files>`. CI uses version 14 and fails on any difference.
- **Windows:** `scripts/bugs/build-windows.ps1` builds on a Windows PC the same way as the release: MSYS2, then `scripts/build-shotcut-msys2.sh`. Setting `BUGS_PART=deps` or `BUGS_PART=app` builds only the libraries or only the app, which the release workflow uses to cache the libraries.
- **CI and releases** are in `.github/workflows/` and described in `docs/bugs/RELEASING.md`. The version is in `VERSION`; changes go in `CHANGELOG.md` under **Unreleased**.
- **Git:** work on a `bugs/<topic>` branch and open a pull request; `main` only accepts merges that pass `format` and `build-and-test`. Release tags cannot be moved or deleted. `scripts/bugs/configure-github.ps1` holds the repository settings.

The rest of this file is Shotcut's own guidance, which still applies to everything under Advanced. The executable is now `bugs-video-editor`, though the CMake target is still `shotcut`. Where Shotcut's guidance below says to use the `cc-debug` preset, BUGS! development uses `build/dev` as above.

---

# Shotcut — Workspace Instructions

Shotcut is a free, open-source, cross-platform video editor built on Qt 6 and the [MLT multimedia framework](https://www.mltframework.org/).

## Build and Run

```bash
# Configure (from repo root; output goes to build/cc-debug)
cmake --preset cc-debug

# Reformat code with clang-format
cmake --build build/cc-debug --target clang-format

# Build
cmake --build build/cc-debug

# Run directly from build tree
build/cc-debug/src/shotcut
```

The default preset `cc-debug` (defined in `CMakePresets.json`) uses Ninja, gcc or clang from `$PATH` and Qt 6.10.3 from `~/Qt/6.10.3`, and produces a Debug build.

To properly debug on Windows you must set the environment variable `QSG_RHI_BACKEND=d3d11` to prevent running as a child process detached from the debugger.

## Architecture

| Singleton | Macro | Role |
|-----------|-------|------|
| `Mlt::Controller` | `MLT` | Media engine: producer/consumer/seek/profile |
| `MainWindow` | `MAIN` | Central hub; owns all docks, undo stack, layout |
| `ShotcutActions` | `Actions` | Global `QAction` registry keyed by string |
| `ShotcutSettings` | `Settings` | `QSettings`-backed properties; bindable from QML |

**Data flow**: UI events → `Actions["someKey"]` → undoable `QUndoCommand` in `src/commands/` → `MLT` producer/filter mutation → Qt signals → model updates → QML views re-render.

**QML views** (in `src/qml/views/`): Timeline, Filter, and Keyframes panels are QML; individual filter parameter UIs live in `src/qml/filters/<filter-name>/`.

**Docks** (`src/docks/`): All panels are `QDockWidget` subclasses — Timeline, Playlist, Encode, Filters, Keyframes, Markers, Subtitles, Jobs, Recent, Files, Scope, Notes.

**Models** (`src/models/`): Qt item models bridge C++ data to QML. `MultitrackModel` is the timeline; `PlaylistModel` is the source bin.

## Code Conventions

- **Member variables**: `m_` prefix (e.g., `m_position`, `m_gridButton`)
- **Class naming**: `PascalCase`; methods/variables: `camelCase`
- **Slot naming**: `on<ObjectName><SignalName>()` (e.g., `onGridToggled`, `onFrameDisplayed`)
- **Header guards**: `#ifndef CLASSNAME_H` / `#define CLASSNAME_H`
- **Signal/slot style**: Do not replace old `SIGNAL()`/`SLOT()` macros unless the entire containing block is already using the new-style pointer syntax — do not mix old macros and new-style function pointers in the same block. Use new-style `QObject::connect()` with function pointers or lambdas for all new code.
- **Action registration**: Add new actions in `setupActions()` or analogous setup functions; register with `Actions.add("keyName", action, tr("Group"))`.
- **Grid/zoom data encoding**: Integer `data()` on `QAction` encodes the grid type:

	| # | Type | Range / Values | Examples |
	|---|------|---------------|----------|
	| 1 | Pixel grids | `>= 10000` | `10010` = 10 px, `10020` = 20 px |
	| 2 | Safe-area presets | specific values | `8090`, `95` |
	| 3 | Aspect-ratio frame overlays | `20001`–`29999` | `20001` = 1:1, `20043` = 4:3, `20169` = 16:9, `20916` = 9:16 |
- Update the copyright year in all modified files.

## Key Entry Points

- `src/main.cpp` — application entry
- `src/mainwindow.cpp` — central UI coordination
- `src/player.cpp` — video player widget with transport/zoom/grid controls
- `src/mltcontroller.cpp` — MLT wrapper
- `src/actions.cpp` — global action registry
- `src/settings.cpp` — persistent settings

## Tests

BUGS! adds C++ tests for Simple mode, described at the top of this file. Shotcut's own `tests/` (`gpuinfo`, `mix_transition_order`) build with them. Shotcut also has JavaScript tests for EDL/chapters export:

```bash
node src/qml/export-edl/test-node.js
node src/qml/export-chapters/test-node.js
```

## Dependencies

Qt 6.4+, MLT++ ≥ 7.36.0 (via pkg-config), FFTW, FFmpeg, Frei0r, SDL2. See `CMakeLists.txt` for the full list.

### Missing or Incompatible Dependencies

If configure or build fails due to missing or incompatible dependencies:

1. Report the missing/incompatible package name and required version in the error output.
2. Suggest installing or upgrading the package using the platform package manager.
3. Re-run configure:

```bash
cmake --preset cc-debug
```

4. If Qt is not found, verify Qt 6.10.3 is installed and available at `~/Qt/6.10.3`, or set `CMAKE_PREFIX_PATH` to the correct Qt location.
5. If MLT is not found, verify `pkg-config --modversion mlt++` works and reports version `7.36.0` or newer.

When dependency checks fail, stop and provide a clear remediation message instead of continuing with partial setup.
