#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

docker build --platform linux/arm64 -t tinyvm-builder "${REPO_ROOT}/scripts"