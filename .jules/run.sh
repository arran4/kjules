#!/usr/bin/env bash
set -euo pipefail

ROOTFS_DIR="${HOME}/.cache/kde-dev-rootfs"

if [ ! -d "${ROOTFS_DIR}" ]; then
    echo "Rootfs not found at ${ROOTFS_DIR}. Did you run .jules/bootstrap.sh?"
    exit 1
fi

if [ $# -eq 0 ]; then
    echo "Usage: $0 <command> [args...]"
    exit 1
fi

# Ensure workspace directory exists in rootfs
sudo mkdir -p "${ROOTFS_DIR}/workspace"

# Function to unmount bind mounts
cleanup() {
    sudo umount "${ROOTFS_DIR}/workspace" || true
    sudo umount "${ROOTFS_DIR}/dev/pts" || true
    sudo umount "${ROOTFS_DIR}/dev" || true
    sudo umount "${ROOTFS_DIR}/sys" || true
    sudo umount "${ROOTFS_DIR}/proc" || true
    sudo rm -f "${ROOTFS_DIR}/etc/resolv.conf" || true

    # Restore original resolv.conf if it existed
    if [ -f "${ROOTFS_DIR}/etc/resolv.conf.orig" ]; then
        sudo mv "${ROOTFS_DIR}/etc/resolv.conf.orig" "${ROOTFS_DIR}/etc/resolv.conf" || true
    fi
}

# Trap to ensure cleanup happens on exit
trap cleanup EXIT

# Bind mounts
sudo mount -t proc proc "${ROOTFS_DIR}/proc"
sudo mount -t sysfs sys "${ROOTFS_DIR}/sys"
sudo mount --bind /dev "${ROOTFS_DIR}/dev"
sudo mount --bind /dev/pts "${ROOTFS_DIR}/dev/pts"
sudo mount --bind "$(pwd)" "${ROOTFS_DIR}/workspace"

# Setup DNS
if [ -f "${ROOTFS_DIR}/etc/resolv.conf" ]; then
    sudo mv "${ROOTFS_DIR}/etc/resolv.conf" "${ROOTFS_DIR}/etc/resolv.conf.orig"
fi
sudo cp /etc/resolv.conf "${ROOTFS_DIR}/etc/resolv.conf"

# Execute command inside chroot
# Run as user with same uid/gid (defaults to root if not specified,
# but better to run as normal user if possible, or we can just run as root for build)
sudo chroot "${ROOTFS_DIR}" /bin/bash -c "
    export QT_QPA_PLATFORM=offscreen
    export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
    cd /workspace
    \"\$@\"
" -- "$@"
