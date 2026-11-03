# Releasing BUGS! Video Editor

Everything runs in GitHub Actions. There are no secrets to set up.

| Workflow | Runs when | What it does |
|----------|-----------|--------------|
| **CI** | Pull requests, pushes to `main` | Checks code style, builds on Linux, runs the Simple mode tests |
| **Build Windows installer** | Started by hand from the Actions tab | Builds the installer and attaches it to the run |
| **Release** | A tag like `v1.0.0` is pushed | Builds the installer and creates a draft GitHub Release |

## Versions

Versions look like `1.2.3`, and the current one is in the `VERSION` file:

- **Last number** for fixes: 1.0.0 to 1.0.1.
- **Middle number** for new features: 1.0.1 to 1.1.0.
- **First number** for big changes: 1.4.2 to 2.0.0.

Release tags are the version with a `v`: `v1.1.0`. The Release workflow refuses a tag that does not match `VERSION`, so bump the file first.

`CHANGELOG.md` lists what changed. Move the **Unreleased** notes under a heading with the new version before tagging.

## Branches

- `main` is what has been released or is ready to release. Changes reach it through pull requests, which run CI.
- Work happens on short-lived branches, such as `bugs/simple-mode`.
- Shotcut updates come in as a merge of an upstream release tag, kept separate from BUGS! changes.

## To make a release

1. Make sure CI is green on `main`.
2. Set the new version in `VERSION`, move the **Unreleased** notes in `CHANGELOG.md` under it, and merge that to `main`.
3. Tag it: `git tag v1.1.0 && git push origin v1.1.0`
4. When the Release workflow finishes, open the draft under **Releases** and download the installer.
5. Install it on a Windows machine and go through the checklist below.
6. Publish the draft and send Levi the installer.

A release that fails the checklist stays a draft. Delete it, fix the problem, and use a new tag.

## Checklist on Windows

Use a real iPhone HEVC video, an Android video, a video received through WhatsApp, and a HEIC photo.

- [ ] The installer runs without administrator rights and the app opens to the welcome screen
- [ ] It runs with Wi-Fi turned off
- [ ] Add files with the button and by dragging them in
- [ ] Trim, cut out a part, add a caption, and Finish: the result is right and plays in Windows
- [ ] The result sends through WhatsApp and plays on a phone
- [ ] Save a picture from a video
- [ ] Crop a photo to a square and save it
- [ ] Light and dark mode switch and are remembered after restarting
- [ ] Combine clips opens Advanced with the file, and Back to Simple returns
- [ ] Originals are unchanged
- [ ] A regular Shotcut install on the same machine still works and keeps its own settings

## Good to know

- The Windows build caches FFmpeg, MLT and the other libraries it compiles. Scheduled runs on main (Monday and Thursday) keep the cache ready, so a release takes about 20 minutes. Without the cache, such as after MSYS2 updates a library, it takes about 90.
- The installer is not code signed, so Windows SmartScreen warns on first run until it is. Choose **More info**, then **Run anyway**.
- Files in a private repository's releases can only be downloaded by people with access to it. Send Levi the installer directly.
