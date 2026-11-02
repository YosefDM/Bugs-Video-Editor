# Builds BUGS! Video Editor on this Windows computer and puts a shortcut on the desktop.
#
# Run in an elevated (administrator) PowerShell. The one-line way to start it:
#   winget install -e --id GitHub.cli --accept-source-agreements --accept-package-agreements; $gh="$env:ProgramFiles\GitHub CLI\gh.exe"; & $gh auth status 2>$null; if(-not $?){& $gh auth login -w -p https}; & $gh api -H "Accept: application/vnd.github.raw" "repos/YosefDM/Bugs-Video-Editor/contents/scripts/bugs/build-windows.ps1?ref=bugs/simple-mode" | Out-String | iex
#
# It follows the same steps as .github/workflows/build-windows.yml: MSYS2, then Shotcut's
# scripts/build-shotcut-msys2.sh. Running it again updates the code and rebuilds.

# Not 'Stop': Windows PowerShell treats any stderr output from git or gh as fatal. Each step
# checks its own result instead.
$ErrorActionPreference = 'Continue'
$Branch = if ($env:BUGS_BRANCH) { $env:BUGS_BRANCH } else { 'bugs/simple-mode' }
$Msys = 'C:\msys64'
$Bash = Join-Path $Msys 'usr\bin\bash.exe'
$Gh = Join-Path $env:ProgramFiles 'GitHub CLI\gh.exe'

function Step($text) { Write-Host "`n==> $text" -ForegroundColor Cyan }

# 1. MSYS2, the build environment Shotcut uses on Windows
if (-not (Test-Path $Bash)) {
    Step 'Installing MSYS2'
    winget install -e --id MSYS2.MSYS2 --accept-source-agreements --accept-package-agreements
    if (-not (Test-Path $Bash)) { Write-Host "MSYS2 did not install to $Msys" -ForegroundColor Red; return }
}

# 2. Access to the private repository, through the GitHub sign-in
if (-not (Test-Path $Gh)) { Write-Host 'GitHub CLI is missing. Run the one-line command from the top of this script.' -ForegroundColor Red; return }
& $Gh auth status 2>$null
if (-not $?) { & $Gh auth login --web --git-protocol https }
$Token = (& $Gh auth token | Out-String).Trim()
if (-not $Token) { Write-Host 'Could not sign in to GitHub.' -ForegroundColor Red; return }

# 3. Everything else runs inside MSYS2's MINGW64 shell.
$Script = @'
set -euo pipefail
step() { printf '\n\033[36m==> %s\033[0m\n' "$1"; }

step 'Installing build tools and libraries'
PACKAGES=$(cat <<'LIST'
autotools base-devel bzip2 coreutils git gzip tar xz zip perl-List-MoreUtils perl-XML-Parser
mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-meson mingw-w64-x86_64-nasm
mingw-w64-x86_64-ninja mingw-w64-x86_64-yasm mingw-w64-x86_64-cairo mingw-w64-x86_64-eigen3
mingw-w64-x86_64-dlfcn mingw-w64-x86_64-fftw mingw-w64-x86_64-gcc-fortran mingw-w64-x86_64-gettext-tools
mingw-w64-x86_64-harfbuzz mingw-w64-x86_64-lame mingw-w64-x86_64-libarchive mingw-w64-x86_64-libepoxy
mingw-w64-x86_64-libexif mingw-w64-x86_64-libimagequant mingw-w64-x86_64-libsamplerate
mingw-w64-x86_64-libtheora mingw-w64-x86_64-libvorbis mingw-w64-x86_64-libvpx mingw-w64-x86_64-libwebp
mingw-w64-x86_64-libxml2 mingw-w64-x86_64-lilv mingw-w64-x86_64-onevpl mingw-w64-x86_64-openblas
mingw-w64-x86_64-openssl mingw-w64-x86_64-opus mingw-w64-x86_64-potrace mingw-w64-x86_64-python-pip
mingw-w64-x86_64-qt6-base mingw-w64-x86_64-qt6-charts mingw-w64-x86_64-qt6-declarative
mingw-w64-x86_64-qt6-imageformats mingw-w64-x86_64-qt6-multimedia mingw-w64-x86_64-qt6-multimedia-wmf
mingw-w64-x86_64-qt6-svg mingw-w64-x86_64-qt6-tools mingw-w64-x86_64-qt6-translations
mingw-w64-x86_64-qt6-websockets mingw-w64-x86_64-rubberband mingw-w64-x86_64-sox mingw-w64-x86_64-svt-av1
mingw-w64-x86_64-vid.stab mingw-w64-x86_64-vulkan-headers mingw-w64-x86_64-vulkan-loader
mingw-w64-x86_64-x264 mingw-w64-x86_64-x265 mingw-w64-x86_64-zimg
LIST
)
# shellcheck disable=SC2086
pacman -S --needed --noconfirm $PACKAGES
pacman -Rdd --noconfirm mingw-w64-x86_64-aom 2>/dev/null || true

if [ ! -f "$HOME/.bugs-prebuilt-v7" ]; then
    step 'Downloading prebuilt dependencies from the Shotcut project'
    cd "$HOME"
    curl -fLO --no-progress-meter https://s3.amazonaws.com/misc.meltymedia/shotcut-build/mlt-prebuilt-mingw64-v7.txz
    tar -xJf mlt-prebuilt-mingw64-v7.txz
    rm mlt-prebuilt-mingw64-v7.txz
    touch "$HOME/.bugs-prebuilt-v7"
fi

# The token is only passed for these commands; it is not saved in the repository.
AUTH="Authorization: Basic $(printf 'x:%s' "$BUGS_TOKEN" | base64 -w0)"
REPO="$HOME/Bugs-Video-Editor"
if [ -d "$REPO/.git" ]; then
    step "Updating the BUGS! code ($BUGS_BRANCH)"
    git -C "$REPO" -c http.extraHeader="$AUTH" fetch origin "$BUGS_BRANCH"
    git -C "$REPO" checkout -q -B "$BUGS_BRANCH" "origin/$BUGS_BRANCH"
else
    step "Downloading the BUGS! code ($BUGS_BRANCH)"
    git -c http.extraHeader="$AUTH" clone -b "$BUGS_BRANCH" https://github.com/YosefDM/Bugs-Video-Editor.git "$REPO"
fi
cd "$REPO"

# A fresh copy each time, so a rebuild picks up new code.
rm -rf "$HOME/bugs-build/src/shotcut"

cat > build-shotcut.conf <<EOF
INSTALL_DIR="$HOME/bugs-build"
SHOTCUT_VERSION="$(date +%y.%-m.%-d)"
BUGS_REPO="$REPO"
SHOTCUT_HEAD=0
SHOTCUT_REVISION=origin/$BUGS_BRANCH
MLT_HEAD=0
MLT_REVISION=v7.42.0
EOF

step 'Building BUGS! (the long part; a log is in ~/bugs-build/build.log)'
mkdir -p "$HOME/bugs-build"
bash scripts/build-shotcut-msys2.sh 2>&1 | tee "$HOME/bugs-build/build.log"
test -f "$HOME/bugs-build/Shotcut/bugs-video-editor.exe"
cygpath -w "$HOME/bugs-build/Shotcut/bugs-video-editor.exe" > "$HOME/bugs-build/exe-path.txt"
'@

$ScriptFile = Join-Path $env:TEMP 'bugs-build.sh'
[IO.File]::WriteAllText($ScriptFile, $Script.Replace("`r`n", "`n"))
$ScriptPath = (& (Join-Path $Msys 'usr\bin\cygpath.exe') -u $ScriptFile | Out-String).Trim()

$env:MSYSTEM = 'MINGW64'
$env:CHERE_INVOKING = '1'

# Its first update can replace the shell it runs in, so each round is its own run.
Step 'Updating MSYS2 (this can take a few minutes)'
& $Bash -lc 'pacman -Syu --noconfirm'
& $Bash -lc 'pacman -Syu --noconfirm'
$env:BUGS_TOKEN = $Token
$env:BUGS_BRANCH = $Branch
$Started = Get-Date
try {
    & $Bash -lc "bash '$ScriptPath'"
    $ExitCode = $LASTEXITCODE
} finally {
    Remove-Item Env:BUGS_TOKEN -ErrorAction SilentlyContinue
    Remove-Item $ScriptFile -ErrorAction SilentlyContinue
}
if ($ExitCode -ne 0) {
    Write-Host "`nThe build stopped with an error. Send the last lines above, or the log at:" -ForegroundColor Red
    Write-Host "  $Msys\home\$env:USERNAME\bugs-build\build.log"
    # return, not exit: this runs through iex, where exit would close the window.
    return
}

# 4. A desktop shortcut, then start it
$Exe = (Get-Content (Join-Path $Msys "home\$env:USERNAME\bugs-build\exe-path.txt")).Trim()
$Shell = New-Object -ComObject WScript.Shell
$Link = $Shell.CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'BUGS! Video Editor.lnk'))
$Link.TargetPath = $Exe
$Link.WorkingDirectory = Split-Path $Exe
$Link.IconLocation = $Exe
$Link.Save()

$Minutes = [int]((Get-Date) - $Started).TotalMinutes
Write-Host "`nBUGS! is built ($Minutes minutes). There is a shortcut on your desktop." -ForegroundColor Green
Write-Host "  $Exe"
Start-Process $Exe
