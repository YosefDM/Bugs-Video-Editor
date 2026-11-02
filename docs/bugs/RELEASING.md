# Releasing BUGS! Video Editor

Everything runs in GitHub Actions. There are no secrets to set up.

| Workflow | Runs when | What it does |
|----------|-----------|--------------|
| **CI** | Pull requests, pushes to `main` | Checks code style, builds on Linux, runs the Simple mode tests |
| **Build Windows installer** | Started by hand from the Actions tab | Builds the installer and attaches it to the run |
| **Release** | A tag like `v26.11.1` is pushed | Builds the installer and creates a draft GitHub Release |

## Versions

Versions are the release date, the same scheme Shotcut uses: `26.11.2` is 2 November 2026, tagged `v26.11.2`. A build made from a tag takes its version from the tag; any other build uses the date it was built. Release at most once a day; if a second release is really needed, use the next day's date.

`CHANGELOG.md` lists what changed. Move the **Unreleased** notes under a heading with the new version before tagging.

## Branches

- `main` is what has been released or is ready to release. Changes reach it through pull requests, which run CI.
- Work happens on short-lived branches, such as `bugs/simple-mode`.
- Shotcut updates come in as a merge of an upstream release tag, kept separate from BUGS! changes.

## To make a release

1. Make sure CI is green on `main`.
2. In `CHANGELOG.md`, rename **Unreleased** to the version and date, and merge that to `main`.
3. Tag it, using the date as the version: `git tag v26.11.1 && git push origin v26.11.1`
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

- The Windows build compiles FFmpeg and other dependencies from source, so expect it to take hours. Windows minutes count double against the GitHub Actions allowance on a private repository.
- The installer is not code signed, so Windows SmartScreen warns on first run until it is. Choose **More info**, then **Run anyway**.
- Files in a private repository's releases can only be downloaded by people with access to it. Send Levi the installer directly.
