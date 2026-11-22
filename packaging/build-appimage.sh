#!/usr/bin/env bash
# Builds MPQ_Viewer-<version>-x86_64.AppImage. Needs: cmake, g++, qt6-base-dev, qt6-wayland (optional),
# network access for the linuxdeploy / appimagetool downloads.
# Build on the OLDEST distro you want to support: an AppImage needs at least the glibc it was built against.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${WORK:-$HERE/appbuild}"
TOOLS="$WORK/tools"
mkdir -p "$TOOLS"; cd "$WORK"

fetch() { [ -x "$TOOLS/$2" ] || { wget -q -O "$TOOLS/$2" "$1" && chmod +x "$TOOLS/$2"; }; }
fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage linuxdeploy-x86_64.AppImage
fetch https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage
fetch https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage appimagetool-x86_64.AppImage

cmake -S "$HERE" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$WORK/build" -j"$(nproc)"
rm -rf AppDir && DESTDIR="$WORK/AppDir" cmake --install "$WORK/build" --prefix /usr

export PATH="$TOOLS:$PATH" APPIMAGE_EXTRACT_AND_RUN=1 ARCH=x86_64 NO_STRIP=1 DEPLOY_PLATFORM_THEMES=1
export QMAKE="${QMAKE:-$(command -v qmake6 || echo /usr/lib/qt6/bin/qmake6)}"
export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"
linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt \
  -d AppDir/usr/share/applications/mpq-viewer.desktop \
  -i AppDir/usr/share/icons/hicolor/scalable/apps/mpq-viewer.svg

# GNOME never draws title bars for Wayland-native windows and the bundled Qt has no GNOME-style
# client-side decorations, so on GNOME/Wayland start through XWayland (the user can still override
# with QT_QPA_PLATFORM=wayland).
rm -f AppDir/AppRun
cat > AppDir/AppRun <<'RUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
if [ -z "$QT_QPA_PLATFORM" ] && [ "$XDG_SESSION_TYPE" = "wayland" ] && [ -n "$DISPLAY" ]; then
    case "$XDG_CURRENT_DESKTOP" in *GNOME*|*gnome*|*Unity*|*Budgie*) export QT_QPA_PLATFORM=xcb ;; esac
fi
exec "$HERE/usr/bin/mpq-viewer" "$@"
RUN
chmod +x AppDir/AppRun
appimagetool-x86_64.AppImage AppDir "MPQ_Viewer-$(grep -oP 'project\(mpq-viewer VERSION \K[0-9.]+' "$HERE/CMakeLists.txt")-x86_64.AppImage"
