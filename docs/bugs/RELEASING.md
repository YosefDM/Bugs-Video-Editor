# Releasing BUGS! Video Editor

Everything runs in GitHub Actions. There are no secrets to set up.

| Workflow | Runs when | What it does |
|----------|-----------|--------------|
| **CI** | Pull requests, pushes to `main` | Checks C++ formatting (`format`), builds on Linux and runs the Simple mode tests (`build-and-test`) |
| **Build Windows installer** | By hand from the Actions tab, for each release, and on `main` every Monday and Thursday | Builds the installer and attaches it to the run |
| **Release** | A tag like `v1.0.2` is pushed | Checks the tag matches `VERSION`, builds the installer and creates a draft GitHub Release |
| **CodeQL** | Pull requests, pushes to `main`, weekly | Looks for security problems in the C++ code and the workflows (GitHub's default setup, not a file in the repo) |

Dependabot opens pull requests when a GitHub Action the workflows use has a new version.

## Versions

Versions look like `1.2.3`, and the current one is in the `VERSION` file:

- **Last number** for fixes: 1.0.1 to 1.0.2.
- **Middle number** for new features: 1.0.2 to 1.1.0.
- **First number** for big changes: 1.4.2 to 2.0.0.

Release tags are the version with a `v`: `v1.1.0`. The Release workflow refuses a tag that does not match `VERSION`, so bump the file first.

`CHANGELOG.md` lists what changed. Move the **Unreleased** notes under a heading with the new version before tagging.

## Branches and tags

- `main` holds what has been released or is ready to release. It only changes through pull requests, and a pull request can only be merged once `format` and `build-and-test` pass. Nobody can force-push to `main` or delete it.
- Work happens on short-lived branches named `bugs/<topic>`, which are deleted once merged.
- Release tags (`v*`) are permanent: they cannot be moved or deleted. A bad release is fixed with a new version, never by moving its tag.
- `scripts/bugs/configure-github.ps1` applies these rules and the repository's other settings. Run it again if they are ever changed by hand.

## To make a release

1. Make sure CI is green on `main`.
2. Set the new version in `VERSION`, move the **Unreleased** notes in `CHANGELOG.md` under it, and merge that to `main` through a pull request.
3. Tag the merge: `git tag -a v1.1.0 -m "BUGS! Video Editor 1.1.0" origin/main && git push origin v1.1.0`
4. When the Release workflow finishes, open the draft under **Releases** and download the installer.
5. Install it on a Windows machine and go through the checklist below.
6. Publish the draft and send Levi the link to the release.

If a release fails the checklist, delete the draft (the tag stays), fix the problem, and release the next version.

## Checklist on Windows

Use a real iPhone HEVC video, an Android video, a video received through WhatsApp, and a HEIC photo.

- [ ] The installer runs without administrator rights and the app opens to the welcome screen
- [ ] The taskbar and title bar show the BUGS! icon
- [ ] It runs with Wi-Fi turned off
- [ ] Add files with the button and by dragging them in
- [ ] Trim, cut out a part, add a caption, and Finish: the result is right and plays in Windows
- [ ] The result sends through WhatsApp and plays on a phone
- [ ] Put text on a photo, drag it into place, change its font and colour, and save it
- [ ] Save a picture from a video
- [ ] Crop a photo to a square and save it
- [ ] Open a `.tif` photo (the Linux dev machine can't, for lack of `libtiff.so.5`)
- [ ] Make a collage from several photos, shuffle it and save it
- [ ] Light and dark mode switch and are remembered after restarting
- [ ] No light strip along the bottom edge of the window in Simple mode, with the window not maximised
- [ ] Combine clips opens Advanced with the file, and Back to Simple returns
- [ ] Originals are unchanged
- [ ] A regular Shotcut install on the same machine still works and keeps its own settings

## Good to know

- The Windows build compiles FFmpeg, MLT and other libraries from source. They are cached between builds, so a release takes about 20 minutes. When the cache is missing, such as after MSYS2 updates a library they use, it takes about 90 minutes and refills the cache. The scheduled runs on `main` keep it ready, because GitHub only shares caches made on `main` with release tags and drops caches unused for 7 days.
- The installer step stops after 15 minutes, and the whole job after 350, so a stuck build fails rather than running for hours.
- The installer is built with the Inno Setup that comes with GitHub's Windows runner. `packaging/windows/shotcut.iss` works with Inno Setup 6.5 and later as well as older 6.x versions.
- The installer is not code signed, so Windows SmartScreen warns on first run until it is. Choose **More info**, then **Run anyway**.
- The repository is public, so GitHub Actions minutes are free.
