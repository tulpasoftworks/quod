#!/bin/sh
# SPDX-License-Identifier: MIT
# A release, built on this Mac and uploaded to GitHub as a draft: the FM-1 firmware (fm1/build.sh --release), the app
# carrying the Audio Unit, signed with a Developer ID and notarised by Apple, the plug-in for Windows (VST3, CLAP),
# cross-built here (tools/plugin-windows.sh), and the browser's Quod with this firmware (quod-site.zip). The
# credentials stay in this Mac's keychain; nothing of them goes to GitHub.
#   tools/release.sh X.Y        (one digit each: the FM-1's package identity holds two)
#
# Needs, once:
#   - a "Developer ID Application" certificate in the keychain (Xcode > Settings > Accounts > Manage Certificates;
#     a paid Apple Developer Program membership)
#   - apple/Local.xcconfig: DEVELOPMENT_TEAM = <your team ID>
#   - a notary profile in the keychain (an app-specific password from appleid.apple.com):
#       xcrun notarytool store-credentials quod-notary --apple-id <your Apple ID> --team-id <your team ID>
#     (QUOD_NOTARY_PROFILE names another)
#   - the FM-1's toolchain and SDK files (fm1/tools/get_toolchain.sh, fm1/tools/get_sdk.sh), and Docker running
#   - gh, logged in, with push access to origin
#   - for the plug-in for Windows: cargo install cargo-xwin, rustup target add x86_64-pc-windows-msvc, brew install llvm
#   - for the site: rustup target add wasm32-unknown-unknown (and the same llvm)
set -eu
cd "$(dirname "$0")/.."
V="${1:-}"
case "$V" in [0-9].[0-9]) ;; *) echo "usage: tools/release.sh X.Y   (e.g. 0.1)"; exit 2 ;; esac
TAG="v$V"
PROFILE="${QUOD_NOTARY_PROFILE:-quod-notary}"
OUT="apple/build/release/$TAG"
APP="apple/build/release/Build/Products/Release/Quod.app"
fail() { echo "release: $*" >&2; exit 1; }

# the tree: committed, and what origin has (a release is a commit anyone can build again)
[ -z "$(git status --porcelain)" ] || fail "commit or stash your changes first"
git fetch -q origin
[ "$(git rev-parse HEAD)" = "$(git rev-parse '@{u}')" ] || fail "push first: HEAD is not origin's"
! git rev-parse -q --verify "refs/tags/$TAG" >/dev/null || fail "$TAG exists already"
! git ls-remote --exit-code --tags origin "$TAG" >/dev/null 2>&1 || fail "$TAG exists on origin already"

# the credentials, all in the keychain
grep -q '^DEVELOPMENT_TEAM *= *[A-Z0-9]' apple/Local.xcconfig 2>/dev/null ||
    fail "no team: put DEVELOPMENT_TEAM = <your team ID> in apple/Local.xcconfig"
security find-identity -v -p codesigning | grep -q '"Developer ID Application' ||
    fail "no Developer ID Application certificate in the keychain (Xcode > Settings > Accounts > Manage Certificates)"
xcrun notarytool history --keychain-profile "$PROFILE" >/dev/null 2>&1 ||
    fail "no notary profile '$PROFILE': xcrun notarytool store-credentials $PROFILE --apple-id <you> --team-id <team>"
docker info >/dev/null 2>&1 || fail "Docker is not running (the FM-1's toolchain runs in it)"
gh auth status >/dev/null 2>&1 || fail "gh is not logged in"

rm -rf "$OUT" && mkdir -p "$OUT"

echo "== the FM-1 firmware, $V"
(cd fm1 && ./build.sh --release "$V")
cp "fm1/build/quod-$V.fwsc" "$OUT/"

echo "== the app and its Audio Unit: Developer ID, hardened runtime, a secure timestamp; Apple silicon and Intel"
(cd apple && xcodebuild -quiet -scheme Quod -configuration Release -derivedDataPath build/release \
    -destination 'generic/platform=macOS' clean build ONLY_ACTIVE_ARCH=NO \
    CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY="Developer ID Application" OTHER_CODE_SIGN_FLAGS=--timestamp \
    CODE_SIGN_INJECT_BASE_ENTITLEMENTS=NO MARKETING_VERSION="$V.0" CURRENT_PROJECT_VERSION="$V")
    # (CODE_SIGN_INJECT_BASE_ENTITLEMENTS=NO: no get-task-allow, a debugger's; notarisation refuses it)
codesign --verify --deep --strict --verbose=1 "$APP"
[ -f "$APP/Contents/Resources/Licenses/THIRD-PARTY-NOTICES.md" ] || fail "the app carries no licences"
lipo -archs "$APP/Contents/MacOS/Quod" | grep -q 'x86_64.*arm64\|arm64.*x86_64' || fail "not universal"
codesign -d --entitlements - "$APP" 2>/dev/null | grep -q get-task-allow && fail "get-task-allow is still in"

echo "== notarised by Apple (a few minutes), the ticket stapled to the app"
ditto -c -k --keepParent "$APP" "$OUT/notarise.zip"
xcrun notarytool submit "$OUT/notarise.zip" --keychain-profile "$PROFILE" --wait | tee "$OUT/notary.log"
grep -q 'status: Accepted' "$OUT/notary.log" ||
    fail "not accepted: see $OUT/notary.log, and xcrun notarytool log <id> --keychain-profile $PROFILE"
rm "$OUT/notarise.zip"
xcrun stapler staple "$APP"
spctl --assess --type execute --verbose=2 "$APP"
ditto -c -k --keepParent "$APP" "$OUT/Quod-$V-macOS.zip"

echo "== the site (the sim, [ FLASH ], [ LINK ]) with this firmware: quod-site.zip, for any static host"
web/build.sh "$OUT/site" "$OUT/quod-$V.fwsc" "$V"
(cd "$OUT/site" && zip -qr ../quod-site.zip .)

echo "== the plug-in for Windows (VST3, CLAP), cross-built here"
tools/plugin-windows.sh "$V"
cp "target/Quod-$V-Windows.zip" "$OUT/"

echo "== the licences, for the firmware and the app (the fonts' OFL, the GPL of the firmware's platform)"
mkdir -p "$OUT/Quod-$V-licenses"
cp LICENSE THIRD-PARTY-NOTICES.md art/fonts/Terminus-OFL.txt art/fonts/UnifrakturMaguntia-OFL.txt "$OUT/Quod-$V-licenses/"
cp fm1/LICENSE "$OUT/Quod-$V-licenses/fm1-LICENSE-GPL-3.0.txt"
cp fm1/LICENSING.md "$OUT/Quod-$V-licenses/fm1-LICENSING.md"
(cd "$OUT" && ditto -c -k --norsrc --noextattr --keepParent "Quod-$V-licenses" "Quod-$V-licenses.zip")   # (no ._ files)

echo "== $TAG on GitHub, a draft (publish it from its page once checked)"
git tag -a "$TAG" -m "Quod $V"
git push -q origin "$TAG"
cat > "$OUT/notes.md" <<EOF
**Mac** (macOS 13 or later): unzip \`Quod-$V-macOS.zip\`, move Quod.app to Applications and open it once; that
registers the Audio Unit (Tulpa SoftWorks: Quod) for Logic, GarageBand and other hosts.

**Windows**: unzip \`Quod-$V-Windows.zip\`; copy Quod.vst3 to C:\\Program Files\\Common Files\\VST3 (or Quod.clap
to C:\\Program Files\\Common Files\\CLAP). The builds aren't signed: Windows may ask before the first load.

**In a browser**: the same Quod, with [ FLASH ], which installs it on an FM-1 from Chrome or Edge, and [ LINK ],
which sends settings between the page and the FM-1. \`quod-site.zip\` is that site with this firmware, for any
static host; the latest is always at https://github.com/tulpasoftworks/quod/releases/latest/download/quod-site.zip.

**M-VAVE FM-1**: install from the browser's [ FLASH ], or \`quod-$V.fwsc\` over USB with \`python3 fm1/tools/fm1_install.py quod-$V.fwsc\` (from the
source of this tag; the installer needs \`pip3 install mido python-rtmidi\` first). To get back: hold OCT- while switching
on, then install any firmware.

The firmware and the VST3 are free software under the GPL-3.0 (the VST3 links nih-plug's GPL VST3 bindings); their
complete source is this tag. Quod itself is MIT. The licences
(MIT, the GPL, the fonts' Open Font License) and the third-party notices are in \`Quod-$V-licenses.zip\`, and inside
the app (Contents/Resources/Licenses).
EOF
gh release create "$TAG" --draft --verify-tag --title "Quod $V" --notes-file "$OUT/notes.md" \
    "$OUT/quod-$V.fwsc" "$OUT/Quod-$V-macOS.zip" "$OUT/Quod-$V-Windows.zip" "$OUT/Quod-$V-licenses.zip" \
    "$OUT/quod-site.zip"
echo "release: $TAG drafted: $(gh release view "$TAG" --json url --jq .url)"
