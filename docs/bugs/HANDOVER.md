# BUGS! Video Editor — Product Brief

> The original brief BUGS! was built from, kept for its reasoning. For what BUGS! does now, see the [README](../../README.md) and [CHANGELOG](../../CHANGELOG.md); for how it is built, [AGENTS.md](../../AGENTS.md).

This brief covers **what** to build and **why**. The **how** is entirely yours: architecture, approach, tooling, sequencing and testing. If anything here conflicts with good engineering, flag it to Yosef rather than following it blindly.

## 1. The user

**Levi** is Yosef's friend, and this app is for him.

- **Not technical at all.** If something needs explaining, it's too complicated.
- **Edits one file at a time,** the way WhatsApp or a phone's Photos app works. He opens a video or photo, fixes it up and saves a finished copy. He doesn't think in projects, tracks or timelines.
- **Laptop:** Windows, with **Intel integrated graphics** (exact model TBD).
- **Fully offline:** the app must work without internet and without any account or sign-in.

## 2. Product direction (decided with Yosef)

- **Based on Shotcut:** BUGS! is a fork of [Shotcut](https://github.com/mltframework/shotcut). Yosef chose this so the app inherits Shotcut's mature engine, format support and stability, and so more advanced features stay easy to expose later.
- **Simple mode first:** BUGS! opens into a new **Simple mode** designed for Levi, which is the default experience.
- **Shotcut kept whole:** Shotcut's full interface stays available behind an **Advanced** switch, with everything still working.
- **Rebranded:** the app is called **BUGS! Video Editor**.

If, once you're in the codebase, you think a different technical approach serves these goals better, raise it with Yosef.

## 3. Reference files

```
HANDOVER.md                 ← this brief
design/Main.dc.html         ← Simple mode mockup (the visual and UX reference)
design/Welcome.dc.html      ← welcome screen mockup (partly outdated, see §4.7)
design/logo/                ← final logo pack (SVG + PNG)
```

**About the mockups:**
- They're HTML with inline styles. The colours (light and dark), copy, spacing and sample content are all in there.
- They show the intended look and flow. They aren't a technical spec, so match the experience, not the markup.
- Yosef can also share the interactive canvas version, where you can click through the tools.

## 4. Requirements

### 4.1 Core promise: originals are never touched

**Library:**
- Levi's files live in two places, shown as two tabs: **Originals** and **Finished**.
- Originals are never modified, in any situation.
- Every save produces a new file in Finished.

**Messaging:** the UI says this plainly:
- "Original stays untouched" in the header.
- "Your original is never changed. **Finish** saves a new copy to the Finished tab." in the side panel.

### 4.2 Main screen layout (see mockup)

**Header:**
- BUGS! logo
- "Editing: <file name>", with "Original stays untouched" underneath
- Undo, Redo
- Light/dark toggle
- **Combine clips** (see §4.6)
- A prominent **Finish** button

**Left — Library:**
- **Originals / Finished** tabs with counts and a one-line hint.
- An **Add videos or photos** button (Originals tab), plus drag-and-drop.
- **All / Videos / Photos** filters.
- A thumbnail grid:
  - Videos show the duration; photos say "Photo".
  - Finished items have a ✓ badge.
- Clicking an item opens it for editing.

**Centre — Editor:**
- A large preview.
- A WhatsApp-style **filmstrip** under it, with trim handles and a playhead.
- A row of five large tool buttons: **Trim, Cut out, Crop, Caption, Save picture**.

**Right — Tool options:** shows only the options for the selected tool, written in plain words.

### 4.3 Tools

**Trim:**
- Cut off the start and/or end by dragging handles on the filmstrip.
- Typing exact start and end times should also work.

**Cut out:**
- Select a section in the middle of a video and remove it. The rest stays and joins together seamlessly.
- Show what's selected ("0:17 → 0:23, 6 seconds will be removed") and a clear **Remove this part** button.
- More than one section can be removed.
- This is likely Levi's most-used edit.

**Crop:**
- Works on videos **and photos**.
- Shape presets: **Original, Free, Square, 9:16, 16:9, 4:5**.
- A draggable crop box on the preview.
- **Rotate** and **Flip**.

**Caption:**
- Puts a name or title across the bottom of a video or photo. This is different from the file name.
- A "Name" field, an optional second line, and three simple styles: **Bar**, **Center** and **Bold**.
- What he sees in the preview must match the saved file.

**Save picture:**
- Pause on any moment of a video and save that frame as a full-quality photo with one click.
- No dialogs. The picture goes straight into Finished, with a confirmation message.

**Photos:** for photos, only the tools that make sense (Crop, Caption) are available.

### 4.4 Finish

- **Finish** saves the edited result as a new file in **Finished**, then shows it there.
- Confirmation: "Saved to Finished. Your original is untouched."
- **Output:** a widely compatible format that plays anywhere and shares easily (e.g. on WhatsApp), at the original quality.
- **No overwriting:** it never overwrites an existing file.
- **Speed:** saving should be reasonably fast on an Intel integrated-graphics laptop.
- **No interruptions:** no technical choices or settings dialogs are put in front of Levi.

### 4.5 Look and feel

- **Clean, calm and modern,** with large click targets and plain language.
- **Light and dark mode,** with a toggle in the header that's remembered between sessions.
- **Preview:** the video preview stays dark in both modes.
- **Display size:** must be comfortable on a typical laptop screen (1366×768 and up).
- **Fonts:** must look right fully offline (no fonts loaded from the internet).

### 4.6 Advanced / Combine clips

- **Combine clips** takes Levi into the full Shotcut interface with his current file. That covers joining clips and anything advanced.
- An obvious **Back to Simple** button returns him.
- **Later:** a simple, guided "combine clips" flow inside Simple mode.

### 4.7 Startup

- **First launch:** a simple welcome screen with the logo, "Add your videos and photos", a big add button / drop area, and "Works offline · No account needed".
- **After that:** the app opens straight into Simple mode with his library.
- **Outdated step:** `Welcome.dc.html` still shows a "pick a project shape" step. **That step is dropped.**

### 4.8 Branding

- **Name:** **BUGS! Video Editor** wherever the app's name appears.
- **Coexisting with Shotcut:** it shouldn't clash with a regular Shotcut install on the same machine.
- **Logo files:** in `design/logo/`.
  - App icon: `bugs-app-icon-red`.
  - In-app mark: `bugs-mark-on-light` and `bugs-mark-on-dark`.
- **Colours:** red `#E5392B`, ink `#16171B`, cream `#F4F1EA`.
- **Credit:** acknowledge Shotcut in the About screen, and respect its GPL license.

## 5. Files Levi is likely to use

These all need to work:
- Videos from an iPhone (including HEVC/portrait) and from Android
- Videos received through WhatsApp
- Occasionally older formats
- Photos in JPG and PNG, plus HEIC from iPhones

## 6. What "done" looks like for v1

Levi can do each of these without help, and the results are correct:

1. Add videos and photos and find them in Originals.
2. Open a video, trim the ends, cut a piece out of the middle, add a caption, and hit Finish. He gets a correct new video in Finished that plays fine and shares on WhatsApp.
3. Save a picture from a moment in a video.
4. Crop a photo to a square and save it.
5. Switch light/dark mode.
6. Go into Advanced mode and come back.

And all of these hold:
- His originals are unchanged.
- It all works offline on his Windows laptop with Intel graphics.
- He gets an installer he can run like any normal Windows app.

## 7. Open questions for Yosef

- Levi's exact laptop model and Windows version.
- Whether adding files should copy them into a BUGS! library folder or leave them where they are.
- Whether captions should be able to show for only part of a video in v1, or always for the whole video.
