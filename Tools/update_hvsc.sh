#!/bin/bash -e
source "$(dirname "$0")/../Source/ultra-shared/scripts/preamble.sh"

# Brings the HVSC.zip the app and the scanner share to the latest official
# release: refreshes the sidid signatures, downloads the update archives,
# applies them with hvsc_update, re-measures what changed (sid_scanner
# --batch), which rebuilds ultraSID.db, bumps VERSION to <release>.0.0 and
# mirrors the new update archives plus the full archive to the CDN
# (BUNNY_ACCESS_KEY). Windows only (Release exes, built incrementally on
# every run; a target new to CMakeLists needs one `cmake --preset vs` first).
#
# Usage: update_hvsc.sh [--auto] [<HVSC.zip>]   (default zip: paths/hvsc from settings.yml)
#          --auto: needs a clean tree; commits "Bump to <release>.0.0" and runs
#                  Installer/tag.sh, unless the scan reported more than 10 problems
#        update_hvsc.sh --cdn <release>   mirror only: that release's update
#                                         archive and full archive to the CDN

finish() {
    read -p "Press enter to close"
    exit 0
}

fatal() {
    echo "$1"
    read -p "Press enter to close"
    exit 1
}

UPDATE_URL="https://hvsc.brona.dk/HVSC/HVSC_Update_{}.7z"
FULL_URL="https://hvsc.brona.dk/HVSC/HVSC_{}-all-of-them.7z"
STORAGE_URL="https://la.storage.bunnycdn.com/ultrasid/HVSC"
CDN_URL="https://cdn.ultrasid.com/HVSC"

TOOL="Builds/$TOOLCHAIN/hvsc_update_artefacts/Release/hvsc_update.exe"
SCANNER="Builds/$TOOLCHAIN/sid_scanner_artefacts/Release/sid_scanner.exe"
DL="Builds/hvsc"

mkdir -p "$DL" Builds/logs

# Downloads a URL to a file: 0 = fetched, 1 = not found, anything else fatal
fetch() {
    local code
    code=$(curl -sS -L -o "$2.part" -w '%{http_code}' "$1") || fatal "Download failed: $1"
    case "$code" in
        200) mv "$2.part" "$2"; return 0 ;;
        404) rm -f "$2.part"; return 1 ;;
        *)   rm -f "$2.part"; fatal "HTTP $code for $1" ;;
    esac
}

# Mirrors the files in CDN_FILES (sources in CDN_URLS) to the storage zone.
# The storage listing decides what is there already, never the CDN edge: it
# caches for a year, so a file deleted from storage still answers there
mirror_to_cdn() {
    local i name file sum code listing

    echo
    if [ -z "${BUNNY_ACCESS_KEY:-}" ]; then
        echo "BUNNY_ACCESS_KEY not set, the CDN still needs these under $STORAGE_URL/:"
        for i in "${!CDN_FILES[@]}"; do
            echo "  ${CDN_FILES[$i]}  <-  ${CDN_URLS[$i]}"
        done
        return
    fi

    listing=$(curl -sS -f -H "AccessKey: $BUNNY_ACCESS_KEY" "$STORAGE_URL/") || fatal "Can't list $STORAGE_URL/ (key rejected?)"

    for i in "${!CDN_FILES[@]}"; do
        name="${CDN_FILES[$i]}"
        file="$DL/$name"

        if grep -q "\"ObjectName\": *\"$name\"" <<< "$listing"; then
            echo "$name: already on the CDN"
            continue
        fi

        if [ ! -f "$file" ]; then
            echo "Downloading $name..."
            fetch "${CDN_URLS[$i]}" "$file" || fatal "$name not found at ${CDN_URLS[$i]}"
        fi

        sum=$(sha256sum "$file" | cut -d' ' -f1 | tr a-z A-Z)

        echo "Uploading $name..."
        code=$(curl -sS -o /dev/null -w '%{http_code}' -X PUT \
                    -H "AccessKey: $BUNNY_ACCESS_KEY" -H "Checksum: $sum" \
                    --data-binary "@$file" "$STORAGE_URL/$name") || fatal "Upload failed: $name"
        [ "$code" = "201" ] || fatal "Upload of $name failed, HTTP $code"
        echo "$name: uploaded, serving at $CDN_URL/$name"
    done
}

#
# Mirror-only mode
#
if [ "${1:-}" = "--cdn" ]; then
    [[ "${2:-}" =~ ^[0-9]+$ ]] || fatal "Usage: update_hvsc.sh --cdn <release>"

    CDN_FILES=( "HVSC_Update_$2.7z" "HVSC_$2-all-of-them.7z" )
    CDN_URLS=( "${UPDATE_URL/'{}'/$2}" "${FULL_URL/'{}'/$2}" )

    mirror_to_cdn
    finish
fi

AUTO=0
if [ "${1:-}" = "--auto" ]; then
    AUTO=1
    shift
fi

#
# --auto commits and tags, so the tree must hold nothing else
#
if [ "$AUTO" -eq 1 ]; then
    [ -z "$(git status --porcelain)" ] || fatal "--auto needs a clean working tree (git status)"
    git pull --ff-only > /dev/null || fatal "git pull failed"
fi

#
# The collection
#
if [ -n "${1:-}" ]; then
    ZIP="$1"
else
    ZIP="$(sed -n '/^paths:/,/^[^ ]/{s/^  hvsc: *//p}' "$APPDATA/ultraSID/settings.yml" | tr -d '\r')"
fi
[ -n "$ZIP" ] || fatal "No HVSC path: pass it as argument or set paths/hvsc in settings.yml"

ZIP="$(cygpath -u "$ZIP")"
[ -f "$ZIP" ] || fatal "Not a file: $ZIP"
[[ "${ZIP,,}" == *.zip ]] || fatal "Only the zip collection is supported: $ZIP"

#
# Latest player signatures first, so the scan identifies the new tunes'
# players with the final set (tag.sh checks the same files before a release)
#
bash Tools/update_sidid.sh --batch

#
# Tools, always brought up to date (incremental, seconds when nothing changed)
#
build_target() {
    echo "Building $1..."
    cmake --build --preset $TOOLCHAIN --config Release --target "$1" --parallel > "Builds/logs/build_$1.log" 2>&1 \
        || fatal "Build of $1 failed, see Builds/logs/build_$1.log"
}

build_target hvsc_update
build_target psiddrv
build_target sid_scanner

#
# Installed release vs the app version
#
RELEASE="$("$TOOL" "$(cygpath -w "$ZIP")" | sed -n 's/^Release //p')"
[ -n "$RELEASE" ] || fatal "Can't read the release of $ZIP"

VER="$(tr -d ' \r\n' < VERSION)"
MAJOR="${VER%%.*}"

echo "$ZIP: HVSC $RELEASE, VERSION $VER"

if [ "$RELEASE" -lt "$MAJOR" ]; then
    fatal "The collection is older than the app version, install HVSC $MAJOR first"
fi

#
# Update chain: every release past the installed one, until the first 404
#
if [ "$RELEASE" -eq "$MAJOR" ]; then
    UPDATES=()
    NEXT=$(( RELEASE + 1 ))
    while :; do
        FILE="$DL/HVSC_Update_$NEXT.7z"
        if [ -f "$FILE" ]; then
            echo "$(basename "$FILE"): already downloaded"
        elif fetch "${UPDATE_URL/'{}'/$NEXT}" "$FILE"; then
            echo "$(basename "$FILE"): downloaded"
        else
            break
        fi
        UPDATES+=( "$FILE" )
        NEXT=$(( NEXT + 1 ))
    done

    if [ ${#UPDATES[@]} -eq 0 ]; then
        echo "HVSC $RELEASE is the latest release, nothing to do"
        finish
    fi

    TARGET=$(( NEXT - 1 ))

    ARGS=( "$(cygpath -w "$ZIP")" )
    for f in "${UPDATES[@]}"; do
        ARGS+=( "$(cygpath -w "$f")" )
    done

    echo "Updating to HVSC $TARGET..."
    "$TOOL" "${ARGS[@]}" || fatal "hvsc_update failed, the collection is unchanged"
else
    # A previous run stopped after the update: pick up with the scan
    TARGET=$RELEASE
    echo "The collection is already at HVSC $TARGET, resuming with the scan"
fi

#
# Re-measure and rebuild the database (the fingerprint cache renders only
# new and changed subtunes)
#
BUGS="Tools/sid_scanner/Data/bugs.txt"
rm -f "$BUGS"

echo "Scanning the collection..."
"$SCANNER" --batch "MUSICIANS|DEMOS|GAMES" || fatal "sid_scanner failed (exit $?), see its log (Shift+F11 in the scanner)"

# Only written by a scan that hit failed or crashing tunes
if [ -f "$BUGS" ]; then
    echo
    echo "$(wc -l < "$BUGS") problem(s) during the scan, see $BUGS:"
    head -20 "$BUGS"
    echo
fi

DBVER=$(od -An -tu1 -j4 -N1 Data/ultraSID.db | tr -d ' ')
[ "$DBVER" = "$TARGET" ] || fatal "ultraSID.db carries HVSC $DBVER, expected $TARGET"

#
# App version follows the collection
#
NEWVER="$TARGET.0.0"
printf '%s' "$NEWVER" > VERSION
echo "VERSION $VER -> $NEWVER"

cmake --preset $TOOLCHAIN > Builds/logs/configure_hvsc.log 2>&1 || fatal "cmake configure failed, see Builds/logs/configure_hvsc.log"

#
# CDN mirror: the update archives the app's update chain will fetch, and the
# full archive for fresh installs
#
CDN_FILES=()
CDN_URLS=()
for (( v = MAJOR + 1; v <= TARGET; v++ )); do
    CDN_FILES+=( "HVSC_Update_$v.7z" )
    CDN_URLS+=( "${UPDATE_URL/'{}'/$v}" )
done
CDN_FILES+=( "HVSC_${TARGET}-all-of-them.7z" )
CDN_URLS+=( "${FULL_URL/'{}'/$TARGET}" )

mirror_to_cdn

echo
echo "Done: HVSC $TARGET, ultraSID.db $DBVER, VERSION $NEWVER"

#
# Release: the bump commit, then tag.sh (pull, signature check, push, tag)
#
if [ "$AUTO" -eq 1 ]; then
    # A handful of entries is the known crowd (rips that HLT instead of
    # looping, measured as one-shot); more means the new release broke something
    if [ -f "$BUGS" ] && [ "$(wc -l < "$BUGS")" -gt 10 ]; then
        echo "Not committing: the scan reported $(wc -l < "$BUGS") problems, review $BUGS, then commit \"Bump to $NEWVER\" and run Installer/tag.sh"
        finish
    fi

    git add VERSION Data/ultraSID.db Data/sidid.cfg Data/sidid.nfo Data/Databases/Songdelays.md5 Tools/sid_scanner/Data
    git commit -q -m "Bump to $NEWVER" || fatal "git commit failed"
    echo "Committed \"Bump to $NEWVER\""

    bash Installer/tag.sh || fatal "tag.sh failed"
    finish
fi

echo "Next: review git diff, commit \"Bump to $NEWVER\", then Installer/tag.sh"
echo "      Tools/sid_scanner/sync.sh -n shows what the server still needs"
finish
