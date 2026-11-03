<p align="center">
  <img src="docs/bugs/design/logo/bugs-logo-horizontal.svg" alt="BUGS! Video Editor" width="360">
</p>

# BUGS! Video Editor

A simple, friendly video and photo editor for Windows. Open a video or photo, fix it up, and save a finished copy that is ready to share. Your original is never changed.

- **Works offline.** No internet connection and no account needed.
- **One file at a time,** like editing in WhatsApp or a phone's Photos app. No projects, tracks or timelines.
- **Never touches your originals.** Every save is a new file in the Finished tab.

## What you can do

| Tool | What it does |
|------|--------------|
| **Trim** | Cut off the start or end by dragging the handles on the filmstrip, or type exact times. |
| **Cut out** | Remove one or more parts from the middle of a video. The rest joins up seamlessly. |
| **Crop** | Square, 9:16, 16:9, 4:5 or free shapes, plus rotate and flip. Works on videos and photos. |
| **Caption** | Put a name or title across the bottom, in one of three styles. |
| **Text** | Put words anywhere, such as names next to people, in six fonts, ten colours and three sizes. |
| **Save picture** | Save any moment of a video as a full-quality photo with one click. |

**Finish** saves the result as an MP4 that plays anywhere and shares easily, including on WhatsApp. Photos are saved as JPG or PNG.

It opens videos from iPhones (including HEVC and HDR) and Android phones, videos received through WhatsApp, older formats, and JPG, PNG and HEIC photos. Light and dark mode are both built in.

For bigger jobs, such as combining clips, **Combine clips** opens the full editor with your file, and **Back to Simple** returns.

## Installing

Download `bugs-video-editor-setup-<version>.exe` from [Releases](../../releases) and run it. It installs for your user account without asking for administrator rights.

Windows may show a SmartScreen warning the first time, because the installer is not yet code signed. Choose **More info**, then **Run anyway**.

## For developers

| | |
|---|---|
| Product brief and designs | [`docs/bugs/`](docs/bugs/) |
| How the code is organised | [`AGENTS.md`](AGENTS.md) |
| Releasing a new version | [`docs/bugs/RELEASING.md`](docs/bugs/RELEASING.md) |
| What changed in each version | [`CHANGELOG.md`](CHANGELOG.md) |

Set up a Linux development build with `scripts/bugs/setup-dev-linux.sh`, or build on Windows with `scripts/bugs/build-windows.ps1`. Every pull request is checked by CI, and pushing a `v1.2.3` tag that matches the `VERSION` file builds the Windows installer and drafts a release.

## License

Copyright © 2026 Yosef. BUGS! Video Editor is free software under the [GNU General Public License v3](COPYING).

Built with open-source software, including [Shotcut](https://www.shotcut.org/) by Meltytech, LLC, [MLT](https://www.mltframework.org/), [FFmpeg](https://ffmpeg.org/) and [Qt](https://www.qt.io/). The bundled fonts are licensed under the SIL Open Font License; see `src/qml/views/simple/fonts/`.
