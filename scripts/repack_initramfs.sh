#!/bin/bash
set -e

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGING="${REPO_ROOT}/rootfs/staging"
CPIO="${REPO_ROOT}/rootfs/initramfs.cpio.gz"

# Extract existing initramfs if staging doesn't exist
if [ ! -d "${STAGING}" ]; then
  echo "Extracting initramfs..."
  mkdir -p "${STAGING}"
  cd "${STAGING}"
  gunzip -c "${CPIO}" | cpio -id --quiet
fi

# Overlay init script
cp "${REPO_ROOT}/scripts/init" "${STAGING}/init"
chmod +x "${STAGING}/init"

# Repack
echo "Repacking initramfs..."
cd "${STAGING}"
find . | cpio -H newc -o --quiet | gzip > "${CPIO}"

echo "Done: $(du -sh "${CPIO}" | cut -f1)"