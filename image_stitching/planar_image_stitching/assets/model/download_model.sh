#!/bin/bash
# Download the SuperPoint + LightGlue ONNX models into this script's own
# directory (assets/model/), no matter where the script is invoked from.
#
# Each model is fetched into "<name>.tmp" and only moved to its final name
# after its size matches the server's Content-Length. An interrupted download
# therefore never leaves a truncated file under the real name, and a stale or
# oversized .tmp is discarded instead of being resumed onto.
set -euo pipefail

MODEL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_URL="https://archive.spacemit.com/spacemit-ai/model_zoo/vision"

MODELS=(
    "superpoint/superpoint_360x640.q.onnx"
    "lightglue/lightglue_for_superpoint_512_depth9.fp16.onnx"
)

# echo the remote size in bytes, or nothing if the server omits Content-Length
remote_size() {
    wget --spider --server-response "$1" 2>&1 \
        | awk '/^[[:space:]]*Content-Length:/ { size = $2 } END { print size }' \
        | tr -d '\r'
}

local_size() {
    [ -f "$1" ] && stat -c '%s' "$1" || echo 0
}

for model in "${MODELS[@]}"; do
    name="$(basename "$model")"
    url="${BASE_URL}/${model}"
    dest="${MODEL_DIR}/${name}"
    tmp="${dest}.tmp"

    expected="$(remote_size "$url")"
    if [ -z "$expected" ]; then
        echo "warning: ${name}: server did not report Content-Length, size check skipped" >&2
    fi

    if [ -n "$expected" ] && [ "$(local_size "$dest")" = "$expected" ]; then
        echo "up to date: ${name} (${expected} bytes)"
        continue
    fi

    # A leftover .tmp longer than the remote file is corrupt: wget -c would
    # append to it and never converge. Drop it and start over.
    if [ -n "$expected" ] && [ "$(local_size "$tmp")" -gt "$expected" ]; then
        echo "discarding oversized partial download: ${name}.tmp" >&2
        rm -f "$tmp"
    fi

    echo "Downloading ${name} -> ${dest}"
    wget -c -O "$tmp" "$url"

    if [ -n "$expected" ]; then
        actual="$(local_size "$tmp")"
        if [ "$actual" != "$expected" ]; then
            echo "error: ${name}: got ${actual} bytes, expected ${expected}; keeping ${tmp} for resume" >&2
            exit 1
        fi
    fi

    mv -f "$tmp" "$dest"
    echo "done: ${name} ($(local_size "$dest") bytes, sha256 $(sha256sum "$dest" | cut -d' ' -f1))"
done

echo "Done. Models are in ${MODEL_DIR}"
