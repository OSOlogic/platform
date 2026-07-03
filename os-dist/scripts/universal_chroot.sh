#!/bin/bash

# ==============================================================================
# UNIVERSAL IMAGE CHROOT UTILITY
# Mounts and enters ANY Linux .img file (x86, ARM, ARM64) using QEMU.
# ==============================================================================

set -e

# --- Colors ---
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'
BOLD='\033[1m'

info() { echo -e "${CYAN}[INFO]${NC} $1"; }
success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# --- Check Root ---
if [ "$EUID" -ne 0 ]; then
    error "This script must be run as root. Please use sudo."
fi

echo -e "${CYAN}${BOLD}================================================${NC}"
echo -e "${CYAN}${BOLD}           UNIVERSAL IMAGE CHROOT TOOL          ${NC}"
echo -e "${CYAN}${BOLD}================================================${NC}"

# --- 1. Dependencies ---
info "Checking dependencies..."
DEPS_NEEDED=false
for pkg in qemu-user-static kpartx; do
    if ! command -v $pkg &> /dev/null; then
        warning "$pkg is missing. Will install..."
        DEPS_NEEDED=true
    fi
done

if [ "$DEPS_NEEDED" = true ]; then
    apt-get update -y
    apt-get install -y qemu-user-static kpartx || error "Failed to install dependencies."
    success "Dependencies installed."
fi

# --- 2. Get Image ---
echo ""
read -e -p "Enter the full path to your .img file: " IMG_PATH

if [ ! -f "$IMG_PATH" ]; then
    error "File not found: $IMG_PATH"
fi

MOUNT_DIR="/mnt/universal_chroot"
LOOP_DEV=""

# --- Cleanup Function (Runs automatically on exit or error) ---
cleanup() {
    echo -e "\n${YELLOW}${BOLD}>>> INITIATING CLEANUP <<<${NC}"
    
    # Restore original resolv.conf if we backed it up
    if [ -f "$MOUNT_DIR/etc/resolv.conf.bak" ]; then
        mv "$MOUNT_DIR/etc/resolv.conf.bak" "$MOUNT_DIR/etc/resolv.conf" 2>/dev/null || true
    fi

    info "Unmounting filesystems..."
    umount -l "$MOUNT_DIR/dev/pts" 2>/dev/null || true
    umount -l "$MOUNT_DIR/dev" 2>/dev/null || true
    umount -l "$MOUNT_DIR/proc" 2>/dev/null || true
    umount -l "$MOUNT_DIR/sys" 2>/dev/null || true
    umount -l "$MOUNT_DIR/boot" 2>/dev/null || true
    umount -l "$MOUNT_DIR" 2>/dev/null || true

    if [ -n "$LOOP_DEV" ]; then
        info "Detaching loop device $LOOP_DEV..."
        losetup -d "$LOOP_DEV" 2>/dev/null || true
    fi
    
    rm -rf "$MOUNT_DIR" 2>/dev/null || true
    success "Image unmounted safely. Goodbye!"
}

# Bind the cleanup function to script exit or interruption
trap cleanup EXIT INT TERM

# --- 3. Setup Loop Device ---
info "Mounting image to a loop device..."
LOOP_DEV=$(losetup -fP --show "$IMG_PATH")
success "Image mapped to $LOOP_DEV"

# --- 4. Partition Selection ---
echo -e "\n${CYAN}Available Partitions in the image:${NC}"
# Show partitions with their sizes and filesystem types
lsblk -p -o NAME,SIZE,FSTYPE,LABEL "$LOOP_DEV" | grep -v "^${LOOP_DEV} "

echo ""
info "Usually, the ROOT partition is the largest one (e.g., ext4)."
read -p "Enter the exact name of the ROOT partition (e.g., ${LOOP_DEV}p2): " ROOT_PART

if [ ! -b "$ROOT_PART" ]; then
    error "Partition $ROOT_PART is not a valid block device."
fi

# Ask for boot partition (Optional but recommended for kernel updates)
read -p "Enter the BOOT partition (leave blank if none/unknown): " BOOT_PART

# --- 5. Mounting ---
mkdir -p "$MOUNT_DIR"
info "Mounting root partition..."
mount "$ROOT_PART" "$MOUNT_DIR"

if [ -n "$BOOT_PART" ] && [ -b "$BOOT_PART" ]; then
    info "Mounting boot partition..."
    mkdir -p "$MOUNT_DIR/boot"
    mount "$BOOT_PART" "$MOUNT_DIR/boot" || warning "Failed to mount boot. Continuing..."
fi

# --- 6. Virtual Filesystems & Networking ---
info "Mounting host virtual filesystems (dev, proc, sys)..."
mount --bind /dev "$MOUNT_DIR/dev"
mount --bind /dev/pts "$MOUNT_DIR/dev/pts"
mount -t proc proc "$MOUNT_DIR/proc"
mount -t sysfs sysfs "$MOUNT_DIR/sys"

info "Setting up network access inside the image..."
if [ ! -L "$MOUNT_DIR/etc/resolv.conf" ] && [ -f "$MOUNT_DIR/etc/resolv.conf" ]; then
    cp "$MOUNT_DIR/etc/resolv.conf" "$MOUNT_DIR/etc/resolv.conf.bak"
fi
cp /etc/resolv.conf "$MOUNT_DIR/etc/resolv.conf" 2>/dev/null || true

# --- 7. QEMU Emulation Binaries ---
info "Injecting QEMU static binaries for ARM cross-architecture translation..."
cp /usr/bin/qemu-aarch64-static "$MOUNT_DIR/usr/bin/" 2>/dev/null || true
cp /usr/bin/qemu-arm-static "$MOUNT_DIR/usr/bin/" 2>/dev/null || true

# --- 8. ENTER CHROOT ---
echo -e "\n${GREEN}${BOLD}>>> YOU ARE NOW INSIDE THE IMAGE <<<${NC}"
echo -e "Changes made here will be saved directly to $IMG_PATH."
echo -e "Type ${RED}exit${NC} or press ${RED}Ctrl+D${NC} when you are done.\n"

# Launch interactive bash inside the chroot
chroot "$MOUNT_DIR" /bin/bash

# Once the user exits the chroot, the script will naturally end,
# triggering the 'trap cleanup EXIT' defined above.
