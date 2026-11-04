# Contributing to BUGS! Video Editor

Thanks for helping. BUGS! is a video and photo editor for people who are not technical, so every change is judged by one question: would someone who has never edited a video understand it straight away?

Everyone taking part follows the [Code of Conduct](CODE_OF_CONDUCT.md). Security problems are reported privately, as described in [SECURITY.md](SECURITY.md).

## What BUGS! promises

These are not up for debate in a pull request:

- **Originals are never changed.** Nothing writes to the file someone opened. Every save is a new file in the Finished folder, and an existing file is never overwritten.
- **The preview matches the saved file.** Preview and save are built from the same description of the edit (`compileXml()` in `src/simple/simpleedit.cpp`). An effect that only appears in one of them is a bug.
- **No technical questions.** Finish never asks about formats, codecs or settings.
- **Works offline,** with no account and no network calls.
- **Plain words.** Write "Cut out a part", not "Remove segment". Short sentences, no jargon.

## Report a bug

Open an issue with the **Bug report** template. Include the BUGS! version (shown in the ⓘ About screen), your Windows version, what you did, and what happened. A screenshot or the file that caused it helps a lot.

## Suggest an idea

Open an issue with the **Feature idea** template and describe what you were trying to do, rather than how the screen should look. Simple mode stays small on purpose; anything advanced can already be done under **Combine clips**.

## Change the code

1. **Set up.** On Linux (or this repository's devcontainer), run `scripts/bugs/setup-dev-linux.sh`. It builds the same FFmpeg and MLT versions as the Windows release. On Windows, `scripts/bugs/build-windows.ps1` builds the app with MSYS2, the same way the release does, and puts a shortcut on the desktop.
2. **Read** [`AGENTS.md`](AGENTS.md) for how the code is organised. Simple mode lives in `src/simple/` and `src/qml/views/simple/`.
3. **Build and test.**
   ```bash
   source scripts/bugs/env.sh
   cmake --build build/dev && cmake --install build/dev
   Xvfb :99 &   # the tests need a display
   ctest --test-dir build/dev -R simple --output-on-failure
   ```
   Add a test for what you change. `tests/test_simple_edit.cpp` covers the edit model, `tests/test_collage.cpp` the collage layout, and `tests/test_simple_pipeline.cpp` saves real files and checks them.
4. **Format** C++ with `clang-format-14 -i <files>` (the setup script installs it). CI checks this, and uses version 14 exactly.
5. **Open a pull request** against `main`, from a branch named `bugs/<topic>`. CI checks the formatting, builds and tests it, and CodeQL scans it. It can be merged once CI passes and the maintainer has looked at it. A first-time contributor's CI waits for the maintainer's approval before it runs.

### Code style

- Follow the code around you. The C++ conventions are in [`AGENTS.md`](AGENTS.md): `m_` members, `PascalCase` classes, new-style `connect()`.
- Comments explain why, not what.
- Colours, icons and spacing for Simple mode come from `src/qml/views/simple/Theme.js`. Match the designs in `docs/bugs/design/`.

### Advanced mode

Advanced mode is the full Shotcut editor. Keep changes there small: Simple mode is where BUGS! does its own thing.

## Releases

Maintainers release by bumping `VERSION`, updating `CHANGELOG.md` and pushing a matching tag. See [`docs/bugs/RELEASING.md`](docs/bugs/RELEASING.md).

## License

By contributing you agree that your contribution is licensed under the [GNU General Public License v3](COPYING), like the rest of BUGS!.
