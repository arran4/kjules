#!/usr/bin/env bash
set -euo pipefail

ROOTFS_DIR="${HOME}/.cache/kde-dev-rootfs"
MARKER_FILE="${ROOTFS_DIR}/.ready"
ARCHIVE_URL="https://github.com/arran4/kde-dev-rootfs/releases/latest/download/kde-dev-rootfs-forky-amd64.tar.zst"
SHA256_URL="https://github.com/arran4/kde-dev-rootfs/releases/latest/download/kde-dev-rootfs-forky-amd64.tar.zst.sha256"

if [ -f "${MARKER_FILE}" ]; then
    echo "Rootfs is already ready at ${ROOTFS_DIR}"
    exit 0
fi

echo "Installing zstd..."
sudo apt-get update && sudo apt-get install -y zstd

mkdir -p "${ROOTFS_DIR}"
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "${TEMP_DIR}"' EXIT

echo "Downloading archive and checksum..."
curl --fail --location --retry 3 -o "${TEMP_DIR}/archive.tar.zst" "${ARCHIVE_URL}"
curl --fail --location --retry 3 -o "${TEMP_DIR}/archive.tar.zst.sha256" "${SHA256_URL}"

echo "Verifying checksum..."
cd "${TEMP_DIR}"
# The downloaded sha256 file might just have the hash, or hash + filename.
# We'll just read the first word (the hash) and check.
EXPECTED_HASH=$(awk '{print $1}' archive.tar.zst.sha256)
ACTUAL_HASH=$(sha256sum archive.tar.zst | awk '{print $1}')

if [ "${EXPECTED_HASH}" != "${ACTUAL_HASH}" ]; then
    echo "Checksum mismatch! Expected: ${EXPECTED_HASH}, Actual: ${ACTUAL_HASH}"
    exit 1
fi

echo "Extracting archive to ${ROOTFS_DIR}..."
sudo tar --numeric-owner -I zstd -xf archive.tar.zst -C "${ROOTFS_DIR}"

sudo touch "${MARKER_FILE}"
sudo chown $(id -u):$(id -g) "${MARKER_FILE}"

echo "Rootfs bootstrap complete."
