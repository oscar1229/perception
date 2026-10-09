#!/bin/bash
# Download the SuperPoint + LightGlue ONNX models into this script's own
# directory (assets/model/), no matter where the script is invoked from.
set -e

MODEL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_URL="https://archive.spacemit.com/spacemit-ai/model_zoo/vision"

MODELS=(
    "superpoint/superpoint_360x640.q.onnx"
    "lightglue/lightglue_for_superpoint_512_depth9.fp16.onnx"
)

for model in "${MODELS[@]}"; do
    name="$(basename "$model")"
    echo "Downloading ${name} -> ${MODEL_DIR}/${name}"
    wget -c -O "${MODEL_DIR}/${name}" "${BASE_URL}/${model}"
done

echo "Done. Models are in ${MODEL_DIR}"
