#!/bin/bash

# Script to create a compressed and auto-expandable image from an SD card on Linux Mint

set -e

# Colors
BLUE='\033[1;34m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
RED='\033[1;31m'
RESET='\033[0m'

echo -e "${BLUE}=== Armbian Image Creator with PiShrink ===${RESET}"

# Permissions check
if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}Error: This script must be run as root (sudo).${RESET}"
  exit 1
fi

# Dependencies check
for cmd in dd lsblk wget; do
  if ! command -v $cmd &> /dev/null; then
    echo -e "${RED}Error: '$cmd' is not installed. Please install it first.${RESET}"
    exit 1
  fi
done

# Detect external disks (excludes system disk)
echo -e "${BLUE}Detecting connected block devices...${RESET}"
lsblk -dpno NAME,SIZE,MODEL | grep -v "$(df / | tail -1 | cut -d' ' -f1)" | grep -E "/dev/sd|/dev/mmcblk"

read -rp $'\n'"Enter the SD device (e.g., /dev/sdb): " SD_DEVICE

# Confirm device
echo -e "${YELLOW}You have selected: $SD_DEVICE${RESET}"
read -rp "Is this correct? (y/n): " CONFIRM

if [[ ! "$CONFIRM" =~ ^[Yy]$ ]]; then
  echo -e "${RED}Operation cancelled.${RESET}"
  exit 1
fi

# Verify that the device exists
if [ ! -b "$SD_DEVICE" ]; then
  echo -e "${RED}Error: The device '$SD_DEVICE' does not exist.${RESET}"
  exit 1
fi

# Ask for image name
read -rp "Name for the image (without extension): " IMG_NAME
IMG_NAME=${IMG_NAME:-armbian_custom}
mkdir -p created_images
IMG_FILE="created_images/${IMG_NAME}.img"
IMG_GZ="${IMG_FILE}.gz"

echo -e "${BLUE}Creating image with dd...${RESET}"
echo -e "Source: $SD_DEVICE"
echo -e "Destination: $IMG_FILE"

# Create image with progress
dd if="$SD_DEVICE" of="$IMG_FILE" bs=4M status=progress conv=fsync

sync

# Check if PiShrink is present, otherwise download it
if [ ! -f ./pishrink.sh ]; then
  echo -e "${BLUE}Downloading PiShrink...${RESET}"
  wget -q https://raw.githubusercontent.com/Drewsif/PiShrink/master/pishrink.sh
  chmod +x pishrink.sh
fi

echo -e "${BLUE}Running PiShrink to reduce and compress the image...${RESET}"

# PiShrink has several parameters, here we use -z to compress
# Check https://github.com/Drewsif/PiShrink

./pishrink.sh -z "$IMG_FILE"

echo -e "${GREEN}Image compressed and ready: ${IMG_GZ}${RESET}"

echo -e "${GREEN}Process completed! You can use this image to flash other SD cards.${RESET}"
