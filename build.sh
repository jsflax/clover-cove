#!/usr/bin/env bash
# =============================================================================
# Clover Cove — Native 96 KiB Multi-Platform Build Pipeline
# Targets:
#   - macOS Native Metal App Bundle      (dist/CloverCoveMac.app)
#   - iOS App Clip Metal IPA (<= 96 KiB) (dist/CloverCoveClip.ipa)
#   - iOS Simulator App Clip Bundle      (build/CloverCoveClipSim.app)
#   - Zero-DEX Android Instant APK       (dist/CloverCoveInstant.apk, <= 96 KiB)
#
# Usage:
#   ./build.sh        # Build all available targets (macOS, iOS, and Android if SDK/NDK installed)
#   ./build.sh mac    # Build macOS desktop target only
# =============================================================================
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

MODE="${1:-all}"
MAX_BYTES=98304 # Hard 96 KiB (98,304-byte) package limit

mkdir -p build dist

ENG_SRCS=(
  src/engine96k/eng_math.c
  src/engine96k/eng_mesh.c
  src/engine96k/eng_ui.c
)

GAME_DIR="src/clover_cove"
GAME_SRCS=(
  src/clover_cove/clover_mesh.c
  src/clover_cove/clover_anim.c
  src/clover_cove/clover_audio.c
  src/clover_cove/clover_game.c
)

TITLE="Clover Cove"
PREFIX="CloverCove"
MAC_BUNDLE_ID="com.challenge96kb.clovercove.mac"
OPT_FLAGS=(-Oz -flto)

echo "=== Building [$TITLE] (macOS Metal) ==="
xcrun clang -ObjC -fobjc-arc -O2 -Wall -Wextra -Wno-deprecated-declarations \
  "${ENG_SRCS[@]}" "${GAME_SRCS[@]}" \
  src/platform_apple/rhi_metal.m \
  src/platform_apple/main_macos.m \
  -framework Cocoa -framework Metal -framework QuartzCore -framework AVFoundation \
  -framework GameController -framework IOKit -framework UniformTypeIdentifiers \
  -o "build/${PREFIX}Mac"

TMP_STAGE="/tmp/clovercove_stage_$$"
trap 'rm -rf "$TMP_STAGE"' EXIT
mkdir -p "$TMP_STAGE/${PREFIX}Mac.app/Contents/MacOS" "$TMP_STAGE/${PREFIX}Mac.app/Contents/Resources"
cp "build/${PREFIX}Mac" "$TMP_STAGE/${PREFIX}Mac.app/Contents/MacOS/${PREFIX}Mac"
if [ -f "assets/icons/${PREFIX}.icns" ]; then
  cp "assets/icons/${PREFIX}.icns" "$TMP_STAGE/${PREFIX}Mac.app/Contents/Resources/AppIcon.icns"
fi
cat > "$TMP_STAGE/${PREFIX}Mac.app/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDisplayName</key>
    <string>${TITLE}</string>
    <key>CFBundleExecutable</key>
    <string>${PREFIX}Mac</string>
    <key>CFBundleIconFile</key>
    <string>AppIcon</string>
    <key>CFBundleIdentifier</key>
    <string>${MAC_BUNDLE_ID}</string>
    <key>CFBundleName</key>
    <string>${TITLE}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>GCSupportsControllerUserInteraction</key>
    <true/>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF
xattr -cr "$TMP_STAGE/${PREFIX}Mac.app"
MAC_SIGN_ID="-"
FOUND_DEV_ID="$(security find-identity -v -p codesigning 2>/dev/null | awk -F'"' '/Apple Development:/{print $2; exit}' || true)"
if [ -n "$FOUND_DEV_ID" ]; then
  MAC_SIGN_ID="$FOUND_DEV_ID"
fi
codesign --force --sign "$MAC_SIGN_ID" "$TMP_STAGE/${PREFIX}Mac.app"
rm -rf "dist/${PREFIX}Mac.app"
cp -R "$TMP_STAGE/${PREFIX}Mac.app" "dist/${PREFIX}Mac.app"

if [ "$MODE" = "mac" ]; then
  echo "Built dist/${PREFIX}Mac.app"
  exit 0
fi

FAILED=0
check_pkg() {
  local f="$1"
  local cap="$2"
  local BYTES
  BYTES=$(wc -c < "$f" | tr -d ' ')
  local FREE=$((cap - BYTES))
  printf "  %-30s %8d B  (%6.2f KiB /  96.00 KiB)  [%+d B]\n" "$f" "$BYTES" "$(awk "BEGIN{print $BYTES/1024}")" "$FREE"
  if [ "$BYTES" -gt "$cap" ]; then
    echo "  FAIL: $f ($BYTES B) exceeds ceiling $cap B!" >&2
    FAILED=1
  fi
}

BUILT_IOS=0
if IOS_SDK="$(xcrun --sdk iphoneos --show-sdk-path 2>/dev/null)" && [ -n "$IOS_SDK" ]; then
  echo "=== Building [$TITLE] (iOS App Clip IPA + Simulator) ==="
  IPA_STAGE="$TMP_STAGE/ipa_${PREFIX}"
  mkdir -p "$IPA_STAGE/Payload/${PREFIX}Clip.app"

  xcrun --sdk iphoneos clang -ObjC -fobjc-arc "${OPT_FLAGS[@]}" \
    -target arm64-apple-ios17.0 -isysroot "$IOS_SDK" \
    -fvisibility=hidden -ffunction-sections -fdata-sections \
    -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -Wl,-dead_strip -Wall -Wextra -Wno-deprecated-declarations \
    "${ENG_SRCS[@]}" "${GAME_SRCS[@]}" \
    src/platform_apple/rhi_metal.m \
    src/platform_apple/main_ios.m \
    -framework UIKit -framework Metal -framework QuartzCore -framework AVFoundation \
    -framework GameController -framework Foundation \
    -o "$IPA_STAGE/Payload/${PREFIX}Clip.app/${PREFIX}Clip"

  xcrun strip -x "$IPA_STAGE/Payload/${PREFIX}Clip.app/${PREFIX}Clip"
  cp "${GAME_DIR}/Info_AppClip.plist" "$IPA_STAGE/Payload/${PREFIX}Clip.app/Info.plist"
  plutil -convert binary1 "$IPA_STAGE/Payload/${PREFIX}Clip.app/Info.plist"

  xattr -cr "$IPA_STAGE"
  codesign --force --sign - --entitlements src/platform_apple/AppClip.entitlements \
    "$IPA_STAGE/Payload/${PREFIX}Clip.app"
  xattr -cr "$IPA_STAGE"

  rm -f "dist/${PREFIX}Clip.ipa"
  (cd "$IPA_STAGE" && zip -9 -X -r -q "$ROOT_DIR/dist/${PREFIX}Clip.ipa" Payload)
  BUILT_IOS=1

  if SIM_SDK="$(xcrun --sdk iphonesimulator --show-sdk-path 2>/dev/null)" && [ -n "$SIM_SDK" ]; then
    SIM_STAGE="$TMP_STAGE/${PREFIX}ClipSim.app"
    mkdir -p "$SIM_STAGE"
    cp "${GAME_DIR}/Info_AppClip.plist" "$SIM_STAGE/Info.plist"
    xcrun --sdk iphonesimulator clang -ObjC -fobjc-arc "${OPT_FLAGS[@]}" \
      -target arm64-apple-ios17.0-simulator -isysroot "$SIM_SDK" \
      -Wall -Wextra -Wno-deprecated-declarations \
      "${ENG_SRCS[@]}" "${GAME_SRCS[@]}" \
      src/platform_apple/rhi_metal.m \
      src/platform_apple/main_ios.m \
      -framework UIKit -framework Metal -framework QuartzCore -framework AVFoundation \
      -framework GameController -framework Foundation \
      -o "$SIM_STAGE/${PREFIX}Clip"
    xattr -cr "$SIM_STAGE"
    codesign --force --sign - "$SIM_STAGE"
    rm -rf "build/${PREFIX}ClipSim.app"
    cp -R "$SIM_STAGE" "build/${PREFIX}ClipSim.app"
  fi
else
  echo "Skipping iOS App Clip build (iOS SDK not found; install full Xcode to build iOS targets)."
fi

BUILT_ANDROID=0
ANDROID_SDK="${ANDROID_HOME:-$HOME/Library/Android/sdk}"
if [ -d "$ANDROID_SDK/build-tools" ] && [ -d "$ANDROID_SDK/platforms" ] && [ -d "$ANDROID_SDK/ndk" ]; then
  echo "=== Building [$TITLE] (Zero-DEX Android Instant APK) ==="
  BUILD_TOOLS_DIR="$(ls -d "$ANDROID_SDK/build-tools/"* | sort -V | tail -n 1)"
  PLATFORM_DIR="$(ls -d "$ANDROID_SDK/platforms/android-"* | sort -V | tail -n 1)"
  NDK_DIR="$(ls -d "$ANDROID_SDK/ndk/"* | sort -V | tail -n 1)"

  NDK_CLANG="$(ls "$NDK_DIR"/toolchains/llvm/prebuilt/*/bin/aarch64-linux-android26-clang | head -n 1)"
  AAPT2="$BUILD_TOOLS_DIR/aapt2"
  ZIPALIGN="$BUILD_TOOLS_DIR/zipalign"
  APKSIGNER="$BUILD_TOOLS_DIR/apksigner"
  ANDROID_JAR="$PLATFORM_DIR/android.jar"

  if [ -z "${JAVA_HOME:-}" ] && [ -d "$HOME/.sdkman/candidates/java/current" ]; then
    export JAVA_HOME="$HOME/.sdkman/candidates/java/current"
  fi
  KEYTOOL="${JAVA_HOME:+$JAVA_HOME/bin/}keytool"

  mkdir -p build/android
  if [ ! -f build/android/debug.keystore ]; then
    "$KEYTOOL" -genkeypair -keystore build/android/debug.keystore -storepass android \
      -alias androiddebugkey -keypass android -keyalg EC -groupname secp256r1 \
      -validity 10000 -dname "CN=Android Debug,O=Android,C=US" >/dev/null 2>&1
  fi

  AND_DIR="build/android_${PREFIX}"
  rm -rf "$AND_DIR"
  mkdir -p "$AND_DIR/lib/arm64-v8a" "$AND_DIR/compiled_res"

  "$NDK_CLANG" -std=c17 -Wall -Wextra \
    -shared -fPIC "${OPT_FLAGS[@]}" -fvisibility=hidden \
    -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -Wl,--gc-sections -Wl,--strip-all -Wl,-z,max-page-size=16384 \
    "${ENG_SRCS[@]}" "${GAME_SRCS[@]}" \
    src/platform_android/rhi_gles.c \
    src/platform_android/main_android.c \
    -landroid -lEGL -lGLESv3 -laaudio -lm -llog \
    -o "$AND_DIR/lib/arm64-v8a/libmain.so"

  "$AAPT2" compile "${GAME_DIR}/res/values/strings.xml" -o "$AND_DIR/compiled_res/"
  "$AAPT2" link -o "$AND_DIR/base.apk" \
    -I "$ANDROID_JAR" \
    --manifest "${GAME_DIR}/AndroidManifest.xml" \
    "$AND_DIR/compiled_res/values_strings.arsc.flat"

  cp "$AND_DIR/base.apk" "$AND_DIR/unaligned.apk"
  (cd "$AND_DIR" && xattr -cr lib && zip -9 -X -q unaligned.apk lib/arm64-v8a/libmain.so)
  "$ZIPALIGN" -f 4 "$AND_DIR/unaligned.apk" "$AND_DIR/aligned.apk"

  "$APKSIGNER" sign --ks build/android/debug.keystore --ks-pass pass:android \
    --v1-signing-enabled false --v2-signing-enabled true --v3-signing-enabled true \
    --out "dist/${PREFIX}Instant.apk" "$AND_DIR/aligned.apk"
  "$APKSIGNER" verify "dist/${PREFIX}Instant.apk"
  BUILT_ANDROID=1
else
  echo "Skipping Android Instant APK build (set ANDROID_HOME with build-tools, platforms, and ndk)."
fi

echo ""
echo "=========================================================================="
echo "  CLOVER COVE — 96 KiB PACKAGE BUDGET REPORT"
echo "=========================================================================="
[ "$BUILT_IOS" -eq 1 ]     && check_pkg "dist/${PREFIX}Clip.ipa"    "$MAX_BYTES"
[ "$BUILT_ANDROID" -eq 1 ] && check_pkg "dist/${PREFIX}Instant.apk" "$MAX_BYTES"
echo "=========================================================================="
if [ "$FAILED" -ne 0 ]; then
  exit 1
fi
echo "PASS: All built mobile packages verified <= 96 KiB (98,304 bytes)!"
