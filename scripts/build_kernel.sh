#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

mkdir -p "${REPO_ROOT}/kernel"

docker run --rm --platform linux/arm64 \
  -v "${REPO_ROOT}/scripts:/scripts:ro" \
  -v "${REPO_ROOT}/kernel:/out" \
  ubuntu:24.04 \
  bash /scripts/docker_build_kernel.sh