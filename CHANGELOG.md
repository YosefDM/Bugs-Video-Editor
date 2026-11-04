# Changelog

BUGS! Video Editor uses version numbers like `1.2.3`: the middle number goes up for new features,
the last for fixes, and the first for big changes. Each release notes the Shotcut version it builds on.

## Unreleased

### Fixed
- Work done in **Combine clips** was lost: going back and in again started over. Now the video starts on the timeline, and **Keep it** keeps your work for the next time you press Combine clips for that file.
- If a video or photo was moved or deleted while open, **Finish** saved a video that just said "INVALID". It now says the file can't be found.
- Closing BUGS!, or restarting it for a setting, from Combine clips now warns about changes you haven't finished.
- Resetting settings in Combine clips no longer empties your list of videos and photos.
- Clicking a file while a collage was open threw the collage away. BUGS! now asks first.
- The space bar plays and pauses again after clicking the picture.
- Double-clicking **Finish** stopped the save straight away. Double-clicking **Save this picture** saved it twice.
- In **Cut out**, dragging the red box at the very start or end could trim the video instead.
- The list of videos and photos jumped back to the top after opening one.
- Text could be dragged partly off the picture and was then cut off in the saved file.
- Choosing Hebrew in Combine clips swapped Simple mode's panels around.
- On small screens, the Text tool's colours and sizes were hidden, and tool names overlapped in a narrow window.
- The title bar said "Untitled - Automatic". The close warning now names the files with unsaved changes.

## 1.0.2

### Fixed
- The taskbar, title bar and the Advanced About box showed Shotcut's logo instead of BUGS!'s.
- Small icons, such as in the title bar, use a simpler BUGS! mark that stays sharp.

### Changed
- The Advanced About box credits Yosef.

## 1.0.1

The first release. Built on Shotcut 26.9.27. Version 1.0.0 was tagged, but its Windows
installer build stalled, so it was never released.

### Fixed
- The Windows installer build: it now uses the build machine's own Inno Setup, and stops after
  15 minutes instead of hanging.

## 1.0.0

Not released. Everything below first ships in 1.0.1.

### Added
- Simple mode, which opens by default: a library of Originals and Finished files, a preview, a filmstrip, and six tools: Trim, Cut out, Crop, Caption, Text and Save picture.
- Text: put words anywhere on a photo or video, such as names next to people. Add as many as you like, drag them into place, and pick from six fonts (two with Hebrew), ten colours and three sizes. New text takes whichever colour stands out best against the picture behind it.
- Collage: pick several photos and get a neat layout, in Square, Story (9:16), Wide (16:9) or 4:5. Shuffle for another arrangement, tap two photos to swap them, and choose a border. The layout comes from PhotoCollage by Adrien Vergé.
- Finish saves a new H.264/AAC MP4 (or a JPG/PNG for photos) to the Finished folder. Originals are never changed, and existing files are never overwritten.
- Phone videos (including HEVC, portrait and HDR), WhatsApp videos, older formats, and JPG, PNG and HEIC photos.
- Light and dark mode, remembered between sessions.
- A first-launch welcome screen.
- Combine clips opens the full Shotcut editor with the current file; Back to Simple returns.
- BUGS! names, logo and installer artwork, separate from any Shotcut install on the same computer.
- Works fully offline: no update checks.
