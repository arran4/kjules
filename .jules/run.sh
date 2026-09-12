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

# Check if bind mounts are permitted, otherwise fallback to proot
if sudo mount --bind /dev "${ROOTFS_DIR}/dev" 2>/dev/null; then
    sudo umount "${ROOTFS_DIR}/dev" || true

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
    sudo chroot "${ROOTFS_DIR}" /bin/bash -c "
        export QT_QPA_PLATFORM=offscreen
        export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
        cd /workspace
        \"\$@\"
    " -- "$@"
else
    # Fallback to proot when container restricts mount syscall
    exec proot -0 -r "${ROOTFS_DIR}" -b "$(pwd)":/workspace -b "${HOME}":"${HOME}" -b /tmp:/tmp -b /dev -b /proc -b /sys -b /etc/resolv.conf:/etc/resolv.conf /bin/bash -c "
        export QT_QPA_PLATFORM=offscreen
        export PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
        cd /workspace
        \"\$@\"
    " -- "$@"
fi
