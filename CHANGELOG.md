# Changelog

BUGS! Video Editor uses version numbers like `1.2.3`: the middle number goes up for new features,
the last for fixes, and the first for big changes. Each release notes the Shotcut version it builds on.

## Unreleased

## 1.1.0

### Added
- **Collages with text and stickers.** Add words, or a ready phrase such as "Mazel Tov!", "מזל טוב" or "Good Shabbos", in six fonts and ten colours, and pick from 1,600 stickers: a Jewish group (Magen David, menorah, candle, kiddush cup, challah, the Israel flag and more), then faces, people, animals, food, celebrations, places, things and symbols. Drag them into place, pull the corner dot to resize, turn them with the knob, bring them to the front or back, or remove them. Anything not tznius or inappropriate is left out, and every figure of a person was checked by eye.
- Collage spacing and rounded corners sliders, and ten background colours, in place of the three border choices.
- Undo and redo on the collage screen, for its text, stickers and look.
- Buttons by Play to go back 10 seconds and forward 30 seconds.
- A speed button by the strip: watch at 1×, 1.5× or 2× to find a moment faster. The saved video stays normal speed.
- **Cut out** has **From** and **To** boxes, so a part can be cut by typing its times, such as 11:13 to 12:33.
- **Save picture** lists the pictures saved from the video, with **Don't keep** to take one back. It goes to the Recycle Bin.

### Fixed
- Saving a video is faster: frames are prepared on several cores at once, as Shotcut's own export does.
- Saving a picture while another was still saving could replace it, even though both said "Picture saved". Each picture now gets its own name, with tenths of a second in it.
- When a video finished saving while another file was open, the list jumped to Finished and hid the file being worked on. It now stays put.
- A save could fail at the very end if another program, such as a virus scanner, was looking at the new file. BUGS! now waits a few seconds for it.
- Checking a long video after saving no longer holds up the screen.
- Adding or moving text froze BUGS! for a moment on long videos while it picked a colour. The words now show straight away and take their colour a moment later.
- Typing a caption no longer holds up the screen each time you pause.
- Making a collage reads the photos in the background, and the button says "Making…" meanwhile.
- Switching tools, and undoing changes that don't show in the picture, no longer reload the video when nothing on screen changes.
- The yellow trim frame and handles only show while **Trim** is picked.
- Words typed near the edge of the picture grew off it. They now stay on the picture, in the preview and the saved file.
- After typing a time in **Trim** and pressing Enter, Space typed into the box instead of playing, and the box stopped following the strip.
- Space, the arrow keys and Ctrl+Z work after clicking any button or the strip, not only the video.
- In the Text tool, the words box could show the text from before, and typing then replaced the wrong words.
- The full editor's Elements panel no longer has animated emoji that aren't tznius, such as the bikini, kissing couples, same-sex families and the middle finger.

### Changed
- BUGS! says once, in the note under the tools, that your original is never changed, instead of in the title bar, the file list and every message.

## 1.0.3

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
- **Combine clips** with nothing open showed the full editor's "New Project" screen; it now waits until a video or photo is open.
- On a small laptop screen, Combine clips could run off the edges of the screen. It now fills the screen instead.
- When a collage leaves out a photo BUGS! can't open, the message names it, and problem messages stay up longer.

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
