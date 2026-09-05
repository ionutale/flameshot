# macOS dev signing & ScreenCapture permission

## The problem

On macOS, Flameshot prompts for ScreenCapture via
`CGRequestScreenCaptureAccess()` (`src/core/flameshot.cpp`).
The system dialog text (“record this computer’s screen and audio”) comes
from macOS, not from Flameshot.

If every launch re-prompts even though
System Settings > Privacy & Security > Screen & System Audio Recording
shows Flameshot as enabled, the grant is almost certainly **stale**:

* Ad-hoc signatures (`codesign -s -`, the CMake default) pin the TCC grant
  to the binary **cdhash**. Every rebuild changes the cdhash, so the stored
  grant no longer matches the new binary.
* `CGPreflightScreenCaptureAccess()` then returns `false` while Settings
  still shows the old row as enabled.
* On Sequoia/Tahoe this is stricter: ad-hoc grants may never stick at all.

## What Flameshot does about it

* Permission is requested **lazily** in `Flameshot::ensureScreenCaptureAccess()`,
  called from `gui()` / `screen()` / `full()` — not in the constructor.
  Daemon start, config window, and launcher no longer trigger the prompt.
* Flameshot activates itself, then calls `CGRequestScreenCaptureAccess()`
  and defers entirely to the macOS prompt / System Settings. It shows no
  dialogs of its own. The grant only takes effect after the app relaunches.
* `FLAMESHOT_SKIP_MAC_PERMISSION_REQUEST=1` skips the request entirely
  (capture will fail without a grant, but useful to silence the prompt
  during UI iteration).
* CMake signs with an explicit `--identifier org.flameshot.Flameshot`
  (`FLAMESHOT_BUNDLE_IDENTIFIER`) for both identity and ad-hoc signing.

## Recommended dev setup (A)

1. Create a self-signed code-signing cert once (free, no paid Apple
   Developer account needed):
   Keychain Access > Certificate Assistant > Create a Certificate…
   Name `Flameshot Dev`, Identity Type Self Signed Root,
   Certificate Type Code Signing. Then Get Info > Trust > Code Signing:
   Always Trust.
2. Configure the build:
   `cmake -B build -DCODE_SIGN_IDENTITY="Flameshot Dev" ...`
   or re-sign an existing build:
   `./packaging/macos/sign-dev.sh ./build/src/Flameshot.app "Flameshot Dev"`
3. Reset the stale grant **once** after switching identities:
   `./packaging/macos/sign-dev.sh --reset-tcc ./build/src/Flameshot.app "Flameshot Dev"`
   (or manually: `tccutil reset ScreenCapture org.flameshot.Flameshot`).
   Don't run the reset on every rebuild — it wipes the grant and forces a
   re-grant.
4. Launch the bundle (not the raw binary):
   `open ./build/src/Flameshot.app`
   Grant once in System Settings. The grant now pins to the cert leaf and
   survives rebuilds.
5. Verify:
   `codesign -d -r- ./build/src/Flameshot.app` should show
   `certificate leaf = H"..."`, **not** only `cdhash H"..."`.

## Quick iteration without signing

```bash
FLAMESHOT_SKIP_MAC_PERMISSION_REQUEST=1 open ./build/src/Flameshot.app
```

## Hygiene checklist

* Keep a single copy in `/Applications` for stable testing; delete duplicates.
* `xattr -dr com.apple.quarantine /Applications/Flameshot.app` if Gatekeeper
  translocates the app.
* Always launch the `.app` via `open`, never the raw `Contents/MacOS/flameshot`
  binary — TCC keys off the bundle identity.
* After changing identities, reset the stale grant once via
  `./packaging/macos/sign-dev.sh --reset-tcc ...` (or
  `tccutil reset ScreenCapture org.flameshot.Flameshot`) before
  re-granting, otherwise the stale row lingers.
