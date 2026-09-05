#!/bin/bash -e
source "$(dirname "$0")/../Source/ultra-shared/scripts/preamble.sh"

# --batch skips the keep-window-open prompts (for callers like tag.sh)
BATCH=0
if [ "${1:-}" == "--batch" ]; then
    BATCH=1
fi

handle_error() {
    echo "An error occurred on line $1"
    if [ "$BATCH" -eq 0 ]; then
        read -p "Press enter to continue"
    fi
    exit 1
}

finish() {
    if [ "$BATCH" -eq 0 ]; then
        read -p "Press enter to close"
    fi
    exit 0
}

# Pulls the latest sidid.cfg and sidid.nfo from the upstream sidid repo
# (github.com/cadaver/sidid) into Data. Only these two files, never the rest.

RAW="https://raw.githubusercontent.com/cadaver/sidid/master"

CHANGED=0
for file in sidid.cfg sidid.nfo; do
    tmp="Data/$file.download"
    curl -fsSL "$RAW/$file" -o "$tmp"

    if cmp -s "$tmp" "Data/$file"; then
        rm "$tmp"
        echo "$file: up to date"
    else
        mv "$tmp" "Data/$file"
        echo "$file: UPDATED"
        CHANGED=1
    fi
done

if [ "$CHANGED" -eq 1 ]; then
    echo "Remember: deploy via sync.sh, re-scan only affected tunes if signatures changed."
fi
finish
