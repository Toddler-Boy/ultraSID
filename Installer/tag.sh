#!/bin/bash -e

cd "$(dirname "$0")"
cd ..
ROOT=$(pwd)

cd $ROOT

git pull

# Releases ship the latest sidid signatures; a fresh fetch must be
# reviewed and committed before tagging
bash Tools/update_sidid.sh --batch
if ! git diff --quiet -- Data/sidid.cfg Data/sidid.nfo; then
    echo "sidid signatures changed: review and commit, then re-run tag.sh"
    exit 1
fi

git push

VER=$(tr -d ' \r\n' < VERSION)

echo "Tagging [v$VER]"
git tag "v$VER" && git push origin "v$VER"
