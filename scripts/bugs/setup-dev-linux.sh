#!/bin/bash
# Sets up a Linux development and test build of BUGS! on Ubuntu 24.04 (this repo's devcontainer).
# Levi's Windows build is made by .github/workflows/build-windows.yml, not by this script.
#
# It builds the same FFmpeg and MLT versions as the Windows build, so tests here are meaningful.
# Qt's prebuilt package bundles its own FFmpeg for its media player. That copy is removed because
# it clashes with ours; the Windows build script removes the same plugin.
set -euo pipefail

QT_VERSION=6.10.3
MLT_VERSION=v7.42.0
FFMPEG_BRANCH=release/9.0
REPO="$(cd "$(dirname "$0")/../.." && pwd)"

sudo apt-get update -qq
sudo apt-get install -y -qq --no-install-recommends \
    cmake ninja-build pkg-config nasm clang-format-14 python3-venv gdb \
    libx264-dev libx265-dev libzimg-dev libmp3lame-dev libvpx-dev libdav1d-dev libwebp-dev libopus-dev \
    libfftw3-dev libxml2-dev libsdl2-dev libexif-dev libsamplerate0-dev frei0r-plugins-dev libarchive-dev \
    libgl-dev libegl-dev libglx-dev libopengl-dev libvulkan-dev libx11-dev libxkbcommon-dev \
    libxkbcommon-x11-0 libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
    libxcb-render-util0 libxcb-shape0 libxcb-sync1 libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1 \
    libdbus-1-3 libfontconfig1 libglib2.0-0t64 libgl1 libegl1 libgl1-mesa-dri libglx-mesa0 \
    xvfb x11-utils xdotool imagemagick libheif-examples libheif-plugin-x265 libheif-plugin-libde265

QT="$HOME/Qt/$QT_VERSION/gcc_64"
if [ ! -d "$QT" ]; then
    python3 -m venv "$HOME/aqtenv"
    "$HOME/aqtenv/bin/pip" -q install aqtinstall
    "$HOME/aqtenv/bin/aqt" install-qt -O "$HOME/Qt" linux desktop "$QT_VERSION" linux_gcc_64 \
        -m qtcharts qtmultimedia qtwebsockets qtimageformats qtshadertools qt5compat
fi
mkdir -p "$HOME/Qt/removed-ffmpeg"
mv -f "$QT"/lib/libav*.so* "$QT"/lib/libsw*.so* "$QT/plugins/multimedia/libffmpegmediaplugin.so" \
    "$QT"/lib/cmake/Qt6Multimedia/Qt6QFFmpegMediaPlugin* "$HOME/Qt/removed-ffmpeg/" 2>/dev/null || true

if [ ! -d "$HOME/ffmpeg9" ]; then
    git clone --depth 1 --branch "$FFMPEG_BRANCH" https://github.com/FFmpeg/FFmpeg.git "$HOME/ffmpeg-src"
    (cd "$HOME/ffmpeg-src" &&
        ./configure --prefix="$HOME/ffmpeg9" --disable-static --enable-shared --disable-doc --disable-debug \
            --enable-gpl --enable-version3 --enable-runtime-cpudetect --enable-libx264 --enable-libx265 \
            --enable-libzimg --enable-libmp3lame --enable-libopus --enable-libdav1d --enable-libvpx --enable-libwebp &&
        make -j"$(nproc)" && make install)
fi

if [ ! -d "$HOME/mlt" ]; then
    git clone --depth 1 --branch "$MLT_VERSION" https://github.com/mltframework/mlt.git "$HOME/mlt-src"
    (cd "$HOME/mlt-src" &&
        PKG_CONFIG_PATH="$HOME/ffmpeg9/lib/pkgconfig" cmake -G Ninja -B build \
            -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/mlt" \
            -DCMAKE_PREFIX_PATH="$HOME/ffmpeg9;$QT" -DCMAKE_INSTALL_RPATH="$HOME/ffmpeg9/lib" \
            -DCMAKE_BUILD_RPATH="$HOME/ffmpeg9/lib" -DCLANG_FORMAT=OFF -DMOD_QT6=ON -DMOD_GLAXNIMATE_QT6=OFF \
            -DMOD_MOVIT=OFF -DMOD_OPENCV=OFF -DMOD_RTAUDIO=OFF -DMOD_RUBBERBAND=OFF -DMOD_SOX=OFF \
            -DMOD_VIDSTAB=OFF -DMOD_VORBIS=OFF -DMOD_XINE=OFF -DMOD_SPATIALAUDIO=OFF -DMOD_GDK=OFF \
            -DMOD_DECKLINK=OFF -DMOD_NDI=OFF -DMOD_JACKRACK=OFF -DUSE_LV2=OFF -DUSE_VST2=OFF -DUSE_LADSPA=OFF \
            -DMOD_PLACEBO=OFF -DMOD_OPENFX=OFF -DMOD_RNNOISE=OFF -DBUILD_TESTING=OFF &&
        cmake --build build && cmake --install build)
fi

cat >"$REPO/scripts/bugs/env.sh" <<EOF
export DISPLAY=\${DISPLAY:-:99}
export LD_LIBRARY_PATH=$HOME/bugs-install/lib:$HOME/ffmpeg9/lib:$HOME/mlt/lib:$QT/lib
export PATH=$HOME/ffmpeg9/bin:$HOME/mlt/bin:\$PATH
export PKG_CONFIG_PATH=$HOME/mlt/lib/pkgconfig
export LC_ALL=C.UTF-8
export QT_LOGGING_RULES="*.debug=false;qt.multimedia*=false"
export SDL_AUDIODRIVER=dummy
EOF

# Finish runs melt from the app's own folder, as in the Windows package.
mkdir -p "$HOME/bugs-install/bin"
ln -sf "$HOME/mlt/bin/melt-7" "$HOME/bugs-install/bin/melt-7"

# shellcheck disable=SC1091
source "$REPO/scripts/bugs/env.sh"
cmake -G Ninja -S "$REPO" -B "$REPO/build/dev" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_PREFIX_PATH="$QT" -DCMAKE_INSTALL_PREFIX="$HOME/bugs-install" \
    -DCLANG_FORMAT=OFF -DSHOTCUT_BUILD_TESTS=ON -DBUILD_MINIMAL_MEDIA_BACKEND=ON
echo
echo "Ready. Build with: source scripts/bugs/env.sh && cmake --build build/dev && cmake --install build/dev"
echo "Test with:  (Xvfb :99 &) ; ctest --test-dir build/dev -R simple --output-on-failure"
