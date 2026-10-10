#!/usr/bin/env python3
"""
Downloads the similarity vectors from sidflow-data and converts them into
Data/Databases/similarity.bin, the file ultraSID's "Find similar" reads.

    python Tools/update_similarity.py [release-tag]

Source: https://github.com/chrisgleissner/sidflow-data by Chris Gleissner,
GPL-3.0. The lite bundle (sidcorr-lite-1) holds a 58-dimension vector per
subtune: 24 sound, 11 melody and 23 playroutine dimensions. The playroutine
dimensions are dropped (they match the composer's or editor's driver, not the
music), the remaining 35 keep the published weights.

The release must be built from the same HVSC version as Data/ultraSID.db, and
only tunes and subtunes present there are written.

similarity.bin layout, little endian, read by Source/Database/Similarity.cpp:
    "uSIM"
    u8 dimensions
    dimensions x 256 float32 codebook, sqrt(weight) folded in
    u32 tune count, per tune (sorted by path):
        u8 length + path as in ultraSID.db ("/MUSICIANS/..." without ".sid")
        u16 subtune count, per subtune: u16 subtune, dimensions x u8 code
"""

import hashlib
import json
import math
import struct
import sys
import urllib.request
from pathlib import Path

REPO = "chrisgleissner/sidflow-data"
BUNDLE = "sidcorr-hvsc-full-sidcorr-lite-1.sidcorr"
MANIFEST = "sidcorr-hvsc-full-sidcorr-lite-1.manifest.json"

SOURCE_DIMENSIONS = 58
KEPT_DIMENSIONS = 35        # 24 sound + 11 melody, the playroutine follows
CENTROIDS = 256

DATA = Path(__file__).resolve().parent.parent / "Data"


def fail(text):
    sys.exit("error: " + text)


def fetch(url):
    request = urllib.request.Request(url, headers={"User-Agent": "ultraSID-update-similarity"})
    with urllib.request.urlopen(request) as response:
        return response.read()


def download_release(tag):
    api = f"https://api.github.com/repos/{REPO}/releases/" + (f"tags/{tag}" if tag else "latest")
    release = json.loads(fetch(api))
    assets = {a["name"]: a["browser_download_url"] for a in release["assets"]}

    for name in (BUNDLE, MANIFEST, "SHA256SUMS"):
        if name not in assets:
            fail(f"release {release['tag_name']} has no {name}")

    bundle = fetch(assets[BUNDLE])
    manifest = fetch(assets[MANIFEST])
    sums = fetch(assets["SHA256SUMS"]).decode()

    expected = {line.split()[1].lstrip("*"): line.split()[0] for line in sums.splitlines() if line.strip()}
    for name, data in ((BUNDLE, bundle), (MANIFEST, manifest)):
        if hashlib.sha256(data).hexdigest() != expected.get(name):
            fail(f"{name} does not match SHA256SUMS")

    return release["tag_name"], bundle, json.loads(manifest)


def parse_bundle(b):
    """Returns (codebook[dim][centroid], [(sid_path, subtune, codes)])"""
    if b[:8] != b"SIDCORR\0":
        fail("the bundle is not a sidcorr file")

    dims, file_id_width, song_width, pq_subspaces, centroids = struct.unpack_from("<HBBHH", b, 12)
    codebook_offset = struct.unpack_from("<I", b, 24)[0]

    if dims != SOURCE_DIMENSIONS or pq_subspaces != dims or centroids != CENTROIDS:
        fail(f"unexpected bundle shape: {dims} dimensions, {pq_subspaces} subspaces, {centroids} centroids")

    floats = struct.unpack_from(f"<{dims * centroids}f", b, codebook_offset + 4)
    codebook = [floats[d * centroids:(d + 1) * centroids] for d in range(dims)]

    index_offset = struct.unpack_from("<Q", b, len(b) - 40)[0]
    epoch = struct.unpack_from("<Q", b, index_offset)[0]
    track_count, file_count, dict_bytes, table_bytes = struct.unpack_from("<4I", b, epoch)

    row_bytes = file_id_width + song_width + 2 + dims
    if table_bytes != track_count * row_bytes:
        fail("the bundle's track table size does not match its header")

    paths = []
    at = epoch + 40
    while at < epoch + 40 + dict_bytes:
        length = struct.unpack_from("<H", b, at)[0]
        paths.append(b[at + 2:at + 2 + length].decode("utf-8"))
        at += 2 + length

    if len(paths) != file_count:
        fail("the bundle's file dictionary does not match its header")

    tracks = []
    for _ in range(track_count):
        file_id = int.from_bytes(b[at:at + file_id_width], "little")
        at += file_id_width
        subtune = int.from_bytes(b[at:at + song_width], "little")
        at += song_width + 2        # the packed legacy ratings
        tracks.append((paths[file_id], subtune, b[at:at + dims]))
        at += dims

    return codebook, tracks


def read_tune_counts(db):
    """(HVSC version, ultraSID.db path -> subtune count), layout in Source/Database/uSIDFormat.h"""
    if db[:4] != b"uSID":
        fail("Data/ultraSID.db is not a uSID file")

    count = struct.unpack_from("<I", db, 9)[0]
    at = 13
    tunes = {}

    for _ in range(count):
        texts = []
        for _ in range(4):
            length = db[at]
            texts.append(db[at + 1:at + 1 + length])
            at += 1 + length

        num_tunes = struct.unpack_from("<H", db, at + 4)[0]
        at += 6 + num_tunes * 4
        tunes[texts[0].decode("latin-1")] = num_tunes

    return db[4], tunes


def main():
    tag, bundle, manifest = download_release(sys.argv[1] if len(sys.argv) > 1 else None)

    if manifest.get("schema_version") != "sidcorr-lite-1" or manifest.get("similarity_metric") != "weighted-cosine":
        fail("unexpected manifest: " + str(manifest.get("schema_version")) + ", " + str(manifest.get("similarity_metric")))

    weights = manifest["vector_weights"]
    if len(weights) != SOURCE_DIMENSIONS:
        fail(f"the manifest has {len(weights)} weights, expected {SOURCE_DIMENSIONS}")

    codebook, tracks = parse_bundle(bundle)
    hvsc_version, tunes = read_tune_counts((DATA / "ultraSID.db").read_bytes())

    # "HVSC 85 + Update 85"
    source_hvsc = manifest.get("hvsc_version", "")
    if source_hvsc.split()[1:2] != [str(hvsc_version)]:
        fail(f"release {tag} is built from '{source_hvsc}', ultraSID.db from HVSC {hvsc_version}")

    by_path = {}
    unknown_tunes = set()
    bad_subtunes = 0

    for sid_path, subtune, codes in tracks:
        path = "/" + sid_path.removesuffix(".sid")

        if path not in tunes:
            unknown_tunes.add(path)
            continue

        if not 1 <= subtune <= tunes[path]:
            bad_subtunes += 1
            continue

        by_path.setdefault(path, []).append((subtune, codes[:KEPT_DIMENSIONS]))

    out = bytearray(b"uSIM")
    out += bytes([KEPT_DIMENSIONS])

    for d in range(KEPT_DIMENSIONS):
        scale = math.sqrt(weights[d])
        out += struct.pack(f"<{CENTROIDS}f", *(c * scale for c in codebook[d]))

    out += struct.pack("<I", len(by_path))

    rows = 0
    for path in sorted(by_path):
        encoded = path.encode("latin-1")
        out += bytes([len(encoded)]) + encoded
        out += struct.pack("<H", len(by_path[path]))

        for subtune, codes in sorted(by_path[path]):
            out += struct.pack("<H", subtune) + codes
            rows += 1

    target = DATA / "Databases" / "similarity.bin"
    target.write_bytes(out)

    print(f"sidflow-data {tag}, {source_hvsc}: {rows} subtunes of {len(by_path)} tunes written to {target} ({len(out)} bytes)")
    print(f"skipped: {len(unknown_tunes)} tunes not in ultraSID.db, {bad_subtunes} subtunes out of range")

    for path in sorted(unknown_tunes)[:10]:
        print("  not in ultraSID.db:", path)


if __name__ == "__main__":
    main()
