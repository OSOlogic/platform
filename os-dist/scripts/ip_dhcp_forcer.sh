#!/bin/bash

# 1. Check if we are root
if [ "$EUID" -ne 0 ]; then
  echo "❌ You need to run this script as root (e.g., sudo ./ip_dhcp_forcer.sh)"
  exit 1
fi

# 2. Check dependencies (we also need nmcli now, which comes by default in Ubuntu)
if ! command -v dnsmasq &> /dev/null || ! command -v nmcli &> /dev/null; then
    echo "⚠️ Missing 'dnsmasq' or 'network-manager'."
    echo "👉 Install them with: apt install dnsmasq network-manager"
    exit 1
fi

echo "========================================"
echo "🌐 SELECT THE NETWORK INTERFACE"
echo "========================================"
# Read interfaces (excluding 'lo' which is local) into an array
mapfile -t interfaces < <(ls /sys/class/net/ | grep -v lo)

# Create the numeric menu for interfaces
PS3="👉 Type the interface number and press Enter: "
select INTERFAZ in "${interfaces[@]}"; do
    if [ -n "$INTERFAZ" ]; then
        echo "✅ You have selected: $INTERFAZ"
        echo ""
        break
    else
        echo "❌ Invalid option. Try again."
    fi
done

echo "========================================"
echo "🗂️ SELECT THE NETWORK PROFILE (NetworkManager)"
echo "========================================"
# Read the names of saved profiles in your Linux
mapfile -t perfiles < <(nmcli -t -f NAME connection show)

# Create the numeric menu for profiles
PS3="👉 Type the profile number and press Enter: "
select PERFIL in "${perfiles[@]}"; do
    if [ -n "$PERFIL" ]; then
        echo "✅ You have selected the profile: $PERFIL"
        echo ""
        break
    else
        echo "❌ Invalid option. Try again."
    fi
done

# 3. Activate the chosen profile using the official Linux manager (NetworkManager)
echo "----------------------------------------"
echo "🔄 Applying the profile '$PERFIL' to interface '$INTERFAZ'..."
nmcli connection up id "$PERFIL" ifname "$INTERFAZ" >/dev/null

if [ $? -ne 0 ]; then
    echo "❌ Error activating the profile. Are you sure it's a valid Ethernet profile?"
    exit 1
fi

# Give the network card 2 seconds to assimilate the new configuration
sleep 2

# 4. Extract the IP your computer grabbed thanks to that profile
IP_PORTATIL=$(ip -4 addr show dev "$INTERFAZ" | awk '/inet / {print $2}' | cut -d/ -f1 | head -n 1)

if [ -z "$IP_PORTATIL" ]; then
    echo "❌ Could not detect any IP on $INTERFAZ."
    echo "Make sure the profile '$PERFIL' has a static IP configured in its settings."
    exit 1
fi

# Get the base of the network (e.g., from 192.168.100.1 it gets "192.168.100")
BASE_RED=$(echo "$IP_PORTATIL" | cut -d. -f1,2,3)

# 5. Ask for the last number of the IP for the board
echo "----------------------------------------"
echo "💻 Your PC now has the IP: $IP_PORTATIL"
echo "📡 The locked base network is: $BASE_RED.X"
echo "----------------------------------------"

while true; do
    # Here we do the visual trick so it looks like the IP is pre-typed
    read -p "👉 Type ONLY the last number for the PLC ($BASE_RED.____): " OCTETO
    
    # Validate that it is a number between 1 and 254
    if [[ "$OCTETO" =~ ^[0-9]+$ ]] && [ "$OCTETO" -ge 1 ] && [ "$OCTETO" -le 254 ]; then
        IP_PLACA="$BASE_RED.$OCTETO"
        
        # Prevent assigning the board the same IP your laptop already has
        if [ "$IP_PORTATIL" == "$IP_PLACA" ]; then
            echo "❌ Warning: The PLC cannot have the same IP as your laptop ($IP_PORTATIL)."
        else
            break
        fi
    else
        echo "❌ Invalid number. It must be between 1 and 254."
    fi
done

echo "----------------------------------------"
echo "🚀 Starting DHCP server (Delivering only the IP $IP_PLACA)..."
echo "⚠️ Connect your board and power it on."
echo "🛑 Press Ctrl+C when finished to close the DHCP server."
echo "----------------------------------------"

# Clean up leftovers and launch dnsmasq
killall dnsmasq 2>/dev/null

dnsmasq --no-daemon \
        --port=0 \
        --listen-address="$IP_PORTATIL" \
        --bind-interfaces \
        --dhcp-range="$IP_PLACA","$IP_PLACA",255.255.255.0,1h \
        --log-dhcp