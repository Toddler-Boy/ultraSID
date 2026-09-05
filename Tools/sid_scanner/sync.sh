#!/bin/bash -e
handle_error() {
    echo "An error occurred on line $1"
    read -p "Press enter to continue"
    exit 1
}

trap 'handle_error $LINENO' ERR
set -o pipefail

# Syncs the portable scanner with the server share, newest file wins.
# Every entry has a nominal direction (deploy: repo -> server, fetch:
# server -> repo); a pending copy against its direction means the two
# sides diverged, so nothing is copied and the conflicts are listed.
# Usage: sync.sh [-n]   (-n = show the plan without copying)

fatal() {
    echo "$1"
    read -p "Press enter to close"
    exit 1
}

finish() {
    read -p "Press enter to close"
    exit 0
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ROOTS_FILE="$SCRIPT_DIR/sync-roots.txt"

DRY=0
if [ "${1:-}" = "-n" ]; then
    DRY=1
fi

if [ ! -f "$ROOTS_FILE" ]; then
    fatal "Missing $ROOTS_FILE, create it with: \$SHARE\$ = Z:/Data/SID scanner"
fi

SHARE="$(sed -n 's/^\$SHARE\$ *= *//p' "$ROOTS_FILE" | tr -d '\r')"
if [ -z "$SHARE" ]; then
    fatal "No \$SHARE\$ entry in $ROOTS_FILE"
fi
if [ ! -d "$SHARE" ]; then
    fatal "Share not reachable: $SHARE"
fi

# Pairs as "kind|local|remote". deploy = repo masters, one way only, a newer
# server copy aborts. sync = scan results, newest side wins, but all pending
# result copies must agree on a direction (a mix means both sides rendered
# independently, no per-file merge is safe). Directories expand below.
PAIRS=(
    "deploy|$ROOT/Builds/vs/sid_scanner_artefacts/Release/sid_scanner.exe|$SHARE/sid_scanner.exe"
    "deploy|$ROOT/Data/sidid.cfg|$SHARE/Data/sidid.cfg"
    "deploy|$ROOT/Data/Databases/STIL-addendum.txt|$SHARE/Data/Databases/STIL-addendum.txt"
    "deploy|$ROOT/Data/Databases/Songlengths-addendum.md5|$SHARE/Data/Databases/Songlengths-addendum.md5"
    "deploy|$ROOT/Data/Databases/audio-profiles.csv|$SHARE/Data/Databases/audio-profiles.csv"
    "deploy|$ROOT/Data/Databases/chip-profiles.csv|$SHARE/Data/Databases/chip-profiles.csv"
    "deploy|$ROOT/Data/Databases/tune-overrides.csv|$SHARE/Data/Databases/tune-overrides.csv"
    "deploy|$ROOT/Data/Databases/tune-patches.txt|$SHARE/Data/Databases/tune-patches.txt"
    "deploy|$ROOT/Data/Databases/digi-players.csv|$SHARE/Data/Databases/digi-players.csv"
    "deploy|$ROOT/Data/Databases/digi-tunes.csv|$SHARE/Data/Databases/digi-tunes.csv"
    "sync|$ROOT/Data/Databases/Songdelays.md5|$SHARE/Data/Databases/Songdelays.md5"
    "sync|$ROOT/Data/ultraSID.db|$SHARE/Data/ultraSID.db"
    "sync|$ROOT/Tools/sid_scanner/Data/SID_LUFS.txt|$SHARE/Data/SID_LUFS.txt"
    "sync|$ROOT/Tools/sid_scanner/Data/SID_Filter.txt|$SHARE/Data/SID_Filter.txt"
    "sync|$ROOT/Tools/sid_scanner/Data/SID_Digi.txt|$SHARE/Data/SID_Digi.txt"
    "sync|$ROOT/Tools/sid_scanner/Data/SID_Loop.txt|$SHARE/Data/SID_Loop.txt"
    "sync|$ROOT/Tools/sid_scanner/Data/SID_Settings.txt|$SHARE/Data/SID_Settings.txt"
    "sync|$ROOT/Tools/sid_scanner/Data/SID_WriteRates.txt|$SHARE/Data/SID_WriteRates.txt"
    "sync|$ROOT/Tools/sid_scanner/Data/SID_DigiHints.txt|$SHARE/Data/SID_DigiHints.txt"
    "sync|$ROOT/Tools/sid_scanner/Data/bugs.txt|$SHARE/Data/bugs.txt"
)

DEPLOY_DIRS=( "Data/Roms" "Data/Exotic tunes" )
for dir in "${DEPLOY_DIRS[@]}"; do
    while IFS= read -r -d '' f; do
        rel="${f#"$ROOT/$dir/"}"
        PAIRS+=( "deploy|$f|$SHARE/$dir/$rel" )
    done < <(find "$ROOT/$dir" -type f -print0)
done

mtime () { stat -c %Y "$1" 2>/dev/null || echo 0; }

PLAN_SRC=(); PLAN_DST=(); PLAN_WHAT=()
CONFLICTS=()
SYNC_UP=(); SYNC_DOWN=()
UPTODATE=0

for pair in "${PAIRS[@]}"; do
    dir="${pair%%|*}"; rest="${pair#*|}"
    local_f="${rest%%|*}"; remote_f="${rest#*|}"

    lt=$(mtime "$local_f")
    rt=$(mtime "$remote_f")

    if [ "$lt" -eq 0 ] && [ "$rt" -eq 0 ]; then
        continue
    fi

    # A file existing on only one side is a gap, not a conflict: fill it from
    # the side that has it, whatever the entry's nominal direction
    if [ "$rt" -eq 0 ]; then
        PLAN_SRC+=( "$local_f" ); PLAN_DST+=( "$remote_f" ); PLAN_WHAT+=( "deploy -> server  ${local_f#"$ROOT/"}" )
        continue
    fi
    if [ "$lt" -eq 0 ]; then
        PLAN_SRC+=( "$remote_f" ); PLAN_DST+=( "$local_f" ); PLAN_WHAT+=( "fetch  <- server  ${local_f#"$ROOT/"}" )
        continue
    fi

    diff=$(( lt - rt ))
    if [ "$diff" -ge -2 ] && [ "$diff" -le 2 ]; then
        UPTODATE=$(( UPTODATE + 1 ))
        continue
    fi

    if [ "$diff" -gt 0 ]; then newer="local"; else newer="remote"; fi

    if [ "$dir" = "deploy" ]; then
        if [ "$newer" = "local" ]; then
            PLAN_SRC+=( "$local_f" ); PLAN_DST+=( "$remote_f" ); PLAN_WHAT+=( "deploy -> server  ${local_f#"$ROOT/"}" )
        else
            CONFLICTS+=( "repo master, but the server copy is newer: $local_f" )
        fi
    else
        if [ "$newer" = "local" ]; then
            PLAN_SRC+=( "$local_f" ); PLAN_DST+=( "$remote_f" ); PLAN_WHAT+=( "deploy -> server  ${local_f#"$ROOT/"}" )
            SYNC_UP+=( "${local_f#"$ROOT/"}" )
        else
            PLAN_SRC+=( "$remote_f" ); PLAN_DST+=( "$local_f" ); PLAN_WHAT+=( "fetch  <- server  ${local_f#"$ROOT/"}" )
            SYNC_DOWN+=( "${local_f#"$ROOT/"}" )
        fi
    fi
done

if [ "${#SYNC_UP[@]}" -gt 0 ] && [ "${#SYNC_DOWN[@]}" -gt 0 ]; then
    echo "Scan results moved on both sides since the last sync:"
    printf '  local is newer:  %s\n' "${SYNC_UP[@]}"
    printf '  server is newer: %s\n' "${SYNC_DOWN[@]}"
    CONFLICTS+=( "results criss-cross, both sides rendered" )
fi

if [ "${#CONFLICTS[@]}" -gt 0 ]; then
    echo "Criss-cross detected, nothing copied:"
    printf '  %s\n' "${CONFLICTS[@]}"
    fatal "Resolve by hand, then re-run."
fi

if [ "${#PLAN_SRC[@]}" -eq 0 ]; then
    echo "Everything in sync ($UPTODATE files)."
    finish
fi

printf '%s\n' "${PLAN_WHAT[@]}"

if [ "$DRY" -eq 1 ]; then
    echo "Dry run, nothing copied ($UPTODATE files already in sync)."
    finish
fi

for i in "${!PLAN_SRC[@]}"; do
    mkdir -p "$(dirname "${PLAN_DST[$i]}")"
    if ! cp -p "${PLAN_SRC[$i]}" "${PLAN_DST[$i]}"; then
        fatal "FAILED: ${PLAN_SRC[$i]}"
    fi
done

echo "Done, ${#PLAN_SRC[@]} copied, $UPTODATE already in sync."
finish
