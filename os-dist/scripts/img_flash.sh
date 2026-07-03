#!/bin/bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' 

CARPETA="created_images"

echo -e "${BLUE}==================================================${NC}"
echo -e "${BLUE}   SECURE FLASHER (USB / SD CARDS ONLY)         ${NC}"
echo -e "${BLUE}==================================================${NC}"

# 1. Folder and files validation
if [ ! -d "$CARPETA" ]; then
    echo -e "${RED}[!] The folder '$CARPETA' does not exist.${NC}"
    exit 1
fi

# Enable nullglob so the array is empty if there are no matches
shopt -s nullglob
archivos=( "$CARPETA"/*.img "$CARPETA"/*.gz )
shopt -u nullglob

if [ ${#archivos[@]} -eq 0 ]; then
    echo -e "${RED}[!] No .img or .gz files found in '$CARPETA'.${NC}"
    exit 1
fi

echo -e "\n${GREEN}1. Select the image:${NC}"
select ruta_imagen in "${archivos[@]}"; do
    if [ -n "$ruta_imagen" ]; then
        imagen=$(basename "$ruta_imagen")
        break
    else
        echo "Invalid option."
    fi
done

# 2. List ONLY USB or SD devices (Filter by TRAN)
echo -e "\n${GREEN}2. External peripherals detected (USB/SD):${NC}"
echo "-------------------------------------------------------"
# Filter by transport type: usb or mmc (SD cards)
lista_discos=$(lsblk -d -n -o NAME,SIZE,MODEL,TRAN | grep -E "usb|mmc")

if [ -z "$lista_discos" ]; then
    echo "  (No USB or SD devices found)"
    echo "-------------------------------------------------------"
    echo -e "${RED}[!] Error: Connect the SD card and try again.${NC}"
    exit 1
fi

echo "$lista_discos" | awk '{print $1 " - " $2 " - " $3 " (via " $4 ")"}'
echo "-------------------------------------------------------"

echo -e "Type the device name (e.g., sdb or mmcblk0):"
read destino

# Clean input
destino_limpio=$(echo $destino | sed 's|^/dev/||')
DISCO_REAL="/dev/$destino_limpio"

# Validate that the chosen device is in the allowed list
if ! echo "$lista_discos" | grep -q "^$destino_limpio "; then
    echo -e "${RED}[!] ERROR: The device $destino_limpio is not an external peripheral.${NC}"
    exit 1
fi

# 3. Final confirmation
echo -e "\n${RED}⚠️  FINAL CONFIRMATION!${NC}"
echo -e "All data will be destroyed on: ${BLUE}$DISCO_REAL${NC}"
read -p "Are you absolutely sure? (type 'yes' to continue): " confirmacion

if [ "$confirmacion" != "yes" ]; then
    echo "Operation cancelled by the user."
    exit 0
fi

# 4. Flashing process
echo -e "\n${BLUE}[*] Unmounting and flashing...${NC}"
sudo umount ${DISCO_REAL}* 2>/dev/null
if [[ "$ruta_imagen" == *.gz ]]; then
    zcat "$ruta_imagen" | sudo dd of="$DISCO_REAL" bs=4M status=progress conv=fsync
else
    sudo dd if="$ruta_imagen" of="$DISCO_REAL" bs=4M status=progress conv=fsync
fi

echo -e "\n${GREEN}[✔] Flashing completed!${NC}"

# 5. Expand the last partition to occupy all free space
echo -e "\n${BLUE}[*] Expanding the last partition to the maximum available memory...${NC}"

# Refresh the partition table in the kernel
sudo partprobe "$DISCO_REAL" 2>/dev/null
sleep 2

# Identify the last partition number
PART_NUM=$(sudo parted -s "$DISCO_REAL" print 2>/dev/null | awk '/^[[:blank:]]*[0-9]+/ {print $1}' | tail -n 1)

if [ -n "$PART_NUM" ]; then
    echo "Resizing partition $PART_NUM on $DISCO_REAL..."
    
    # Try to fix the GPT table if the image is GPT (fails silently if MBR)
    sudo sgdisk -e "$DISCO_REAL" >/dev/null 2>&1 || true
    
    # Resize the partition using parted (to 100% free)
    sudo parted -s "$DISCO_REAL" resizepart "$PART_NUM" 100% >/dev/null 2>&1
    
    # Refresh again after changes
    sudo partprobe "$DISCO_REAL" 2>/dev/null
    sleep 2
    
    # Format the correct partition name depending on the device type
    if [[ "$DISCO_REAL" == *"mmcblk"* ]] || [[ "$DISCO_REAL" == *"nvme"* ]] || [[ "$DISCO_REAL" == *"loop"* ]]; then
        PART_DEV="${DISCO_REAL}p${PART_NUM}"
    else
        PART_DEV="${DISCO_REAL}${PART_NUM}"
    fi

    echo "Expanding the file system on $PART_DEV..."
    # Pre-check of the file system (ext2/3/4)
    sudo e2fsck -f -y "$PART_DEV" >/dev/null 2>&1 || true
    
    # Expand file system
    if sudo resize2fs "$PART_DEV" >/dev/null 2>&1; then
        echo -e "${GREEN}[✔] Partition and file system successfully expanded.${NC}"
    else
        echo -e "${RED}[!] Error expanding (normal if partition is not ext2/ext3/ext4).${NC}"
    fi
else
    echo -e "${RED}[!] Could not identify the partition to expand.${NC}"
fi

# 6. Eject the device so it's safe to remove
echo -e "\n${BLUE}[*] Finalizing...${NC}"
sudo eject "$DISCO_REAL" 2>/dev/null || true
echo -e "${GREEN}The drive has been logically ejected. You can now physically remove it from the PC.${NC}\n"
