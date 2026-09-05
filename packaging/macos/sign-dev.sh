#!/bin/bash
# Re-sign a local Flameshot.app with a stable identity so macOS TCC
# (ScreenCapture) grants survive rebuilds.
#
# Background: ad-hoc signatures (`codesign -s -`) pin the TCC grant to the
# binary cdhash. Every rebuild changes the cdhash, so System Settings keeps
# showing an old grant as enabled while CGPreflightScreenCaptureAccess()
# returns false and the app re-prompts on every launch. Signing with a
# stable identity (even a free self-signed "Flameshot Dev" cert) pins the
# designated requirement to the cert leaf instead, so the grant survives.
#
# Usage:
#   ./packaging/macos/sign-dev.sh [--reset-tcc] [path/to/Flameshot.app] [identity]
#
# Examples:
#   ./packaging/macos/sign-dev.sh ./build/src/Flameshot.app
#   ./packaging/macos/sign-dev.sh ./build/src/Flameshot.app "Flameshot Dev"
#   ./packaging/macos/sign-dev.sh --reset-tcc ./build/src/Flameshot.app "Flameshot Dev"
#
# --reset-tcc: additionally clears the stale ScreenCapture grant
#   (`tccutil reset ScreenCapture <bundle-id>`) after signing. Use this
#   once when switching to a new signing identity or when an old entry
#   lingers in System Settings but capture still re-prompts. Do NOT run
#   it on every rebuild - it wipes the grant and forces a re-grant.
#
# One-time cert setup (Keychain Access > Certificate Assistant >
# Create a Certificate...):
#   Name: Flameshot Dev, Identity Type: Self Signed Root,
#   Certificate Type: Code Signing, then in Get Info > Trust > Code Signing: Always Trust.
# Or via CLI (creates cert in login keychain):
#   See docs/dev/src/macos-dev-signing.md
#
# After re-signing with a new identity, reset the stale TCC entry once:
#   tccutil reset ScreenCapture org.flameshot.Flameshot
# Then launch via `open <app>`, grant once in
# System Settings > Privacy & Security > Screen & System Audio Recording.

set -euo pipefail

RESET_TCC=0
ARGS=()
for arg in "$@"; do
    if [ "$arg" = "--reset-tcc" ]; then
        RESET_TCC=1
    else
        ARGS+=("$arg")
    fi
done

APP_PATH="${ARGS[0]:-./build/src/Flameshot.app}"
# $2 may be a codesign identity name or a path to a file containing one
# (CMake writes the identity to a file to avoid shell-quoting issues with
# spaces/parens in identities like "Apple Development: x (TEAMID)").
IDENTITY_ARG="${ARGS[1]:-${CODE_SIGN_IDENTITY:-Flameshot Dev}}"
if [ -f "$IDENTITY_ARG" ]; then
    IDENTITY="$(<"$IDENTITY_ARG")"
else
    IDENTITY="$IDENTITY_ARG"
fi
BUNDLE_ID="${FLAMESHOT_BUNDLE_IDENTIFIER:-org.flameshot.Flameshot}"

if [ -z "$IDENTITY" ]; then
    IDENTITY="-"
fi

if [ ! -d "$APP_PATH" ]; then
    echo "error: app bundle not found: $APP_PATH" >&2
    echo "usage: $0 [--reset-tcc] [path/to/Flameshot.app] [codesign-identity]" >&2
    exit 1
fi

if ! security find-identity -v -p codesigning | grep -q "$IDENTITY"; then
    echo "warning: codesigning identity '$IDENTITY' not found in keychains." >&2
    echo "Falling back to ad hoc signing (grant will NOT survive rebuilds)." >&2
    echo "To fix: create a self-signed 'Flameshot Dev' code-signing cert." >&2
    echo "See docs/dev/src/macos-dev-signing.md" >&2
    IDENTITY="-"
fi

echo "Signing $APP_PATH as '$IDENTITY' (identifier: $BUNDLE_ID)..."
codesign --force --deep --sign "$IDENTITY" --identifier "$BUNDLE_ID" "$APP_PATH"

echo "Verifying..."
codesign --verify --deep --strict "$APP_PATH"
codesign -dv --verbose=4 "$APP_PATH" 2>&1 | head -20 || true
echo "Designated requirement:"
codesign -d -r- "$APP_PATH" 2>&1 | head -10 || true

if [ "$RESET_TCC" = "1" ]; then
    echo ""
    echo "Resetting stale ScreenCapture grant for $BUNDLE_ID..."
    tccutil reset ScreenCapture "$BUNDLE_ID"
    echo "Reset done. The next launch will re-prompt; grant once."
fi

if [ "$IDENTITY" = "-" ]; then
    echo ""
    echo "NOTE: ad-hoc DR is cdhash-pinned (designated => cdhash H\"...\")."
    echo "Expect re-prompts after every rebuild. Use a stable identity +"
    echo "FLAMESHOT_SKIP_MAC_PERMISSION_REQUEST=1 for iteration if needed."
else
    echo ""
    if [ "$RESET_TCC" = "1" ]; then
        echo "Done. Launch and grant once:"
    else
        echo "Done. If this is a new identity, reset the stale grant once:"
        echo "  $0 --reset-tcc \"$APP_PATH\" \"$IDENTITY\""
        echo "or manually:"
        echo "  tccutil reset ScreenCapture $BUNDLE_ID"
    fi
    echo "then: open \"$APP_PATH\" and grant once."
fi
