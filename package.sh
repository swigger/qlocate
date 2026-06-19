#!/bin/bash
# package.sh - Build and package qlocate
# Usage: ./package.sh [--skip-build]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
VERSION="1.0.0"
APP_NAME="qlocate"

SKIP_BUILD=0
[[ "$1" == "--skip-build" ]] && SKIP_BUILD=1

# Build if needed
if [[ $SKIP_BUILD -eq 0 ]]; then
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="${QT_DIR:-/opt/qt/Qt5.15.14}"
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)
fi

cd "$BUILD_DIR"
DIST_DIR="$BUILD_DIR/dist"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

case "$(uname -s)" in
    Darwin)
        echo "=== Packaging macOS .dmg ==="

        # Create .app bundle
        APP_BUNDLE="$DIST_DIR/$APP_NAME.app"
        mkdir -p "$APP_BUNDLE/Contents/MacOS"
        mkdir -p "$APP_BUNDLE/Contents/Resources"

        cp "$BUILD_DIR/$APP_NAME" "$APP_BUNDLE/Contents/MacOS/"

        cat > "$APP_BUNDLE/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>com.qlocate.app</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundleVersion</key>
    <string>$VERSION</string>
    <key>CFBundleShortVersionString</key>
    <string>$VERSION</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

        # Deploy Qt frameworks
        if command -v macdeployqt &>/dev/null; then
            macdeployqt "$APP_BUNDLE"
        elif [[ -x "${QT_DIR:-/opt/qt/Qt5.15.14}/bin/macdeployqt" ]]; then
            "${QT_DIR:-/opt/qt/Qt5.15.14}/bin/macdeployqt" "$APP_BUNDLE"
        else
            echo "WARNING: macdeployqt not found, .app may not be self-contained"
        fi

        # Create .dmg
        DMG_NAME="${APP_NAME}-${VERSION}-macos.dmg"
        hdiutil create -volname "$APP_NAME" -srcfolder "$DIST_DIR" \
            -ov -format UDZO "$DIST_DIR/$DMG_NAME"

        # Clean up .app (dmg contains it)
        rm -rf "$APP_BUNDLE"
        echo "Created: $DIST_DIR/$DMG_NAME"
        ;;

    Linux)
        echo "=== Packaging Linux .zip ==="

        PKG_DIR="$DIST_DIR/${APP_NAME}-${VERSION}-linux"
        mkdir -p "$PKG_DIR"
        cp "$BUILD_DIR/$APP_NAME" "$PKG_DIR/"
        cp "$SCRIPT_DIR/src/qlocate.toml" "$PKG_DIR/qlocate.toml.example"

        cd "$DIST_DIR"
        zip -r "${APP_NAME}-${VERSION}-linux.zip" "$(basename "$PKG_DIR")"
        rm -rf "$PKG_DIR"
        echo "Created: $DIST_DIR/${APP_NAME}-${VERSION}-linux.zip"
        ;;

    MINGW*|MSYS*|CYGWIN*)
        echo "=== Packaging Windows .zip ==="

        PKG_DIR="$DIST_DIR/${APP_NAME}-${VERSION}-win"
        mkdir -p "$PKG_DIR"
        cp "$BUILD_DIR/${APP_NAME}.exe" "$PKG_DIR/"
        cp "$SCRIPT_DIR/src/qlocate.toml" "$PKG_DIR/qlocate.toml.example"

        # Deploy Qt DLLs
        if command -v windeployqt &>/dev/null; then
            windeployqt "$PKG_DIR/${APP_NAME}.exe"
        elif [[ -x "${QT_DIR}/bin/windeployqt.exe" ]]; then
            "${QT_DIR}/bin/windeployqt.exe" "$PKG_DIR/${APP_NAME}.exe"
        else
            echo "WARNING: windeployqt not found, zip may be incomplete"
        fi

        cd "$DIST_DIR"
        zip -r "${APP_NAME}-${VERSION}-win.zip" "$(basename "$PKG_DIR")"
        rm -rf "$PKG_DIR"
        echo "Created: $DIST_DIR/${APP_NAME}-${VERSION}-win.zip"
        ;;

    *)
        echo "Unknown platform: $(uname -s)"
        exit 1
        ;;
esac
