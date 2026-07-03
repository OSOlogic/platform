#!/bin/bash

# ==============================================================================
# OSOLOGIC - PROFESSIONAL SYSTEM ACCESS POINT & INSTALLER
# ==============================================================================
# This script handles the complete installation of the PLC OsoLogic project,
# from Git cloning to configuration and service deployment.
# ==============================================================================

# --- Configuration & Colors ---
set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Default Project URL
DEF_REPO_URL="https://github.com/BORRELL-AUTOMATION/OSOlogic-PRO-PLCBorrell-RealTime.git"

# --- Unattended / preseed support ---------------------------------------------
# Allows the guided wizard (oso-setup) or CI to drive this installer without
# prompts:  install_OsoLogic.sh --config /path/to/oso.conf   (or --unattended
# with the variables already exported). Interactive use is unchanged.
OSO_UNATTENDED="${OSO_UNATTENDED:-0}"
OSO_CONFIG=""
while [ $# -gt 0 ]; do
    case "$1" in
        --config) shift; OSO_CONFIG="${1:-}";;
        --config=*) OSO_CONFIG="${1#*=}";;
        --unattended) OSO_UNATTENDED=1;;
        *) : ;;   # ignore unknown args for backward compatibility
    esac
    shift || true
done
if [ -n "$OSO_CONFIG" ]; then
    [ -f "$OSO_CONFIG" ] || { echo -e "${RED}[ERROR]${NC} Config file not found: $OSO_CONFIG" >&2; exit 1; }
    # shellcheck disable=SC1090
    . "$OSO_CONFIG"
    OSO_UNATTENDED=1
fi

# --- Fancy Bear ASCII Logo ---
function print_logo() {
    clear
    echo -e "${RED}${BOLD}"
    echo -e "                            -------                  -------                                              
                          ------------------------------------                                            
                         --------------------------------------                                           
                         -------------------------------------                                            
                         -------------------------------------                                            
                         --------------------------------------                                           
                          --------------------------------------                                          
                          -------------------------------- -----                                          
                        -------------------   -----------  ------                                         
                       --------------------   -----------------------                                     
                       ------------------------------------------------                                   
                      --------------------------------------------------                                  
                     --------------------------------------------   ----                                  
                     --------------------------  -----------------------                                  
                     ---------------------------   --------------------                                   
                    ------------------------------   -----------------                                    
                    ---------------------------------          ------          ---                        
                    ------------------------------------------------        --------                      
                    ---------------------------------------------          ----  ----                     
                    -----------------------------------------             -----  ----                     
                    --------------------------------------             -------------                      
                     ------------------------------------       ----------    ----                        
                      ------------------------------------     ---------      ---                         
                       -----------------------------------    ---   ----      ---                         
                         ---------------------------------     ----------     ---                         
                            ------------------------------      ------------  ----                        
                               ---------------------------              ------------                      
                                  -----------------------                  ----  ----                     
                                      ------------------                   ----------                     
                                          -------------                     -------                       
                                               ------ "                                                 
                                                             
    echo -e "${NC}"
    echo -e "${BOLD}                        ================================================${NC}"
    echo -e "${BOLD}                        OSOLOGIC - SYSTEM DEPLOYER                       ${NC}"
    echo -e "${BOLD}                        ================================================${NC}"
    echo ""
}

# --- Helper Functions ---
function info() { echo -e "${CYAN}[INFO]${NC} $1"; }
function success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
function warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
function error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

function check_root() {
    if [ "$EUID" -ne 0 ]; then
        error "This script must be run as root (sudo)."
    fi
}

# --- 1. Repository Download & Setup ---
function git_setup_unattended() {
    echo -e "${MAGENTA}${BOLD}# STEP 2: Repository Setup (unattended)${NC}"
    : "${SETUP_MODE:=1}"
    : "${TARGET_DIR:=/home/oso/PLC_OsoLogic}"
    : "${GIT_BRANCH:=master}"

    if [ "$SETUP_MODE" = "2" ]; then
        : "${REPO_URL:=$DEF_REPO_URL}"
        if [ -d "$TARGET_DIR/.git" ]; then
            info "Repository already present at $TARGET_DIR; using it."
        elif [ -d "$TARGET_DIR" ] && [ "$(ls -A "$TARGET_DIR" 2>/dev/null)" ]; then
            info "Directory $TARGET_DIR exists and is not empty; using existing content."
        else
            info "Cloning $REPO_URL into $TARGET_DIR ..."
            mkdir -p "$TARGET_DIR" || error "Failed to create $TARGET_DIR"
            git clone "$REPO_URL" "$TARGET_DIR" || error "Clone of $REPO_URL failed."
        fi
        cd "$TARGET_DIR" || error "Failed to access $TARGET_DIR"
        git checkout "$GIT_BRANCH" 2>/dev/null || warning "Branch '$GIT_BRANCH' not found; staying on default branch."
    else
        [[ "$TARGET_DIR" =~ ^/home/oso/ ]] || error "Project path must be located within '/home/oso/'."
        [ -d "$TARGET_DIR" ] || error "Directory '$TARGET_DIR' does not exist."
        if [ -d "$TARGET_DIR/.git" ] && [[ "${PULL_UPDATES:-n}" =~ ^[Yy]$ ]]; then
            info "Updating repository and discarding local changes..."
            cd "$TARGET_DIR" || error "Failed to access $TARGET_DIR"
            git fetch --all
            git checkout "$GIT_BRANCH" 2>/dev/null || true
            git reset --hard "origin/$GIT_BRANCH" || error "Sync with origin/$GIT_BRANCH failed."
        fi
    fi

    REPO_DIR="$TARGET_DIR"
    cd "$REPO_DIR" || error "Failed to access project directory $REPO_DIR"
    if [ -d "$REPO_DIR/source" ]; then
        SRC_BASE="$REPO_DIR/source"
    else
        error "Could not find 'source' directory in $REPO_DIR. Repository structure is unexpected."
    fi
    success "Project root finalized at: $REPO_DIR"
    success "Source base detected at: $SRC_BASE"
    chown -R oso:oso "$REPO_DIR" || warning "Failed to set ownership to user 'oso'"
    echo ""
}

function git_setup() {
    if [ "${OSO_UNATTENDED:-0}" = "1" ]; then git_setup_unattended; return; fi
    echo -e "${MAGENTA}${BOLD}# STEP 2: Repository Setup${NC}"

    echo -e "${CYAN}Choose how to proceed with the project source code:${NC}"
    echo -e "1) ${BOLD}Existing path:${NC} Specify an existing project directory already on this system."
    echo -e "2) ${BOLD}Download:${NC} Clone the repository from a Git URL."
    read -p "Select option [1/2]: " SETUP_MODE

    if [ "$SETUP_MODE" == "1" ]; then
        # Mode 1: Specify existing path (with retry loop)
        while true; do
            read -p "Enter the absolute path to the existing project directory: " TARGET_DIR
            
            # Validation 1: Security restriction to /home/oso
            if [[ ! "$TARGET_DIR" =~ ^/home/oso/ ]]; then
                warning "Security Restriction: Project path must be located within '/home/oso/'."
                echo -e "Please try again."
                continue
            fi

            # Validation 2: Folder must exist
            if [ ! -d "$TARGET_DIR" ]; then
                warning "Directory '$TARGET_DIR' does not exist."
                echo -e "Please try again."
                continue
            fi
            
            # If we reach here, the path is correct
            break
        done
        info "Using existing project at: $TARGET_DIR"
        
        # If it's a git repo, offer an update
        if [ -d "$TARGET_DIR/.git" ]; then
            read -p "Git repository detected. Do you want to pull updates from origin? (y/n) [n]: " PULL_UPDATES
            if [[ "${PULL_UPDATES:-n}" =~ ^[Yy]$ ]]; then
                info "Updating repository and discarding local changes..."
                cd "$TARGET_DIR"
                git fetch --all
                read -p "Select branch to sync with [master]: " GIT_BRANCH
                GIT_BRANCH=${GIT_BRANCH:-master}
                git checkout "$GIT_BRANCH" 2>/dev/null || true
                git reset --hard origin/"$GIT_BRANCH" || error "Sync with origin/$GIT_BRANCH failed."
            fi
        fi
    else
        # Mode 2: Download repository
        TARGET_DIR="/home/oso/PLC_OsoLogic"
        info "Preparing to download a fresh copy of the repository into $TARGET_DIR."

        if [ -d "$TARGET_DIR" ] && [ "$(ls -A "$TARGET_DIR")" ]; then
            warning "Directory $TARGET_DIR already exists and is not empty."
            read -p "Do you want to use the existing content instead of downloading? (y/n) [y]: " USE_EXISTING
            USE_EXISTING=${USE_EXISTING:-y}
            
            if [[ "$USE_EXISTING" =~ ^[Yy]$ ]]; then
                info "Proceeding with existing directory content."
                if [ -d "$TARGET_DIR/.git" ]; then
                    read -p "Do you want to update it from origin? (y/n) [n]: " UPDATE_EXISTING
                    if [[ "${UPDATE_EXISTING:-n}" =~ ^[Yy]$ ]]; then
                        info "Updating repository..."
                        cd "$TARGET_DIR"
                        git fetch --all
                        read -p "Select branch to sync with [master]: " GIT_BRANCH
                        GIT_BRANCH=${GIT_BRANCH:-master}
                        git checkout "$GIT_BRANCH" 2>/dev/null || true
                        git reset --hard origin/"$GIT_BRANCH" || error "Failed to sync with origin/$GIT_BRANCH."
                    fi
                fi
            else
                error "Target directory is not empty. Please clear it or use Option 1 if the project is elsewhere."
            fi
        else
            # Proceed with git clone logic
            read -p "Enter Git Repository URL [$DEF_REPO_URL]: " REPO_URL
            REPO_URL=${REPO_URL:-$DEF_REPO_URL}
            
            while true; do
                echo -n "Enter Username (leave blank for public/SSH): "
                read GIT_USER
                if [ ! -z "$GIT_USER" ]; then
                    echo -n "Enter Token/Password: "
                    read -s GIT_PASS; echo ""
                    REPO_URL_CRED=$(echo "$REPO_URL" | sed "s|https://|https://$GIT_USER:$GIT_PASS@|")
                else
                    REPO_URL_CRED=$REPO_URL
                fi
                
                info "Downloading project into $TARGET_DIR..."
                mkdir -p "$TARGET_DIR" || error "Failed to create directory $TARGET_DIR"
                
                if git clone "$REPO_URL_CRED" "$TARGET_DIR"; then
                    cd "$TARGET_DIR"
                    git remote set-url origin "$REPO_URL" # Remove credentials from local config
                    
                    read -p "Select branch to use [master]: " GIT_BRANCH
                    GIT_BRANCH=${GIT_BRANCH:-master}
                    git checkout "$GIT_BRANCH" 2>/dev/null || warning "Branch '$GIT_BRANCH' not found. Staying on default branch."
                    break
                else
                    warning "Clone failed. Check your credentials and URL."
                    read -p "Do you want to try again? (y/n) [y]: " RETRY_CLONE
                    if [[ ! "${RETRY_CLONE:-y}" =~ ^[Yy]$ ]]; then
                        error "Installation aborted after failed clone."
                    fi
                    rm -rf "$TARGET_DIR"
                fi
            done
        fi
    fi

    REPO_DIR="$TARGET_DIR"
    cd "$REPO_DIR" || error "Failed to access project directory $REPO_DIR"
    
    # --- Finalize internal paths once repository is ready ---
    if [ -d "$REPO_DIR/source" ]; then
        SRC_BASE="$REPO_DIR/source"
    else
        error "Could not find 'source' directory in $REPO_DIR. Repository structure is unexpected."
    fi
    
    success "Project root finalized at: $REPO_DIR"
    success "Source base detected at: $SRC_BASE"
    chown -R oso:oso "$REPO_DIR" || warning "Failed to set ownership to user 'oso'"
    echo ""
}

# --- 2. Interactive JSON Configuration & DB Initialization ---
function collect_config() {
    echo -e "${MAGENTA}${BOLD}# STEP 3: Configuration Gathering${NC}"

    # --- Hardcoded Project Standards ---
    DB_HOST="localhost"
    DB_USER="oso"
    DB_NAME="PLC"
    MQTT_HOST="localhost"
    MQTT_ID="oso"

    if [ "${OSO_UNATTENDED:-0}" = "1" ]; then
        for _v in DB_ROOT_PASS DB_OSO_PASS MQTT_PASS; do
            [ -n "${!_v:-}" ] || error "Unattended install: required value '$_v' is missing from the config."
        done
        MQTT_PORT="${MQTT_PORT:-1883}"
        MGR_PORT="${MGR_PORT:-8080}"
        GUI_PORT="${GUI_PORT:-8082}"
        NR_PORT="${NR_PORT:-1880}"
        IP_ADDR=$(hostname -I | awk '{print $1}')
        EXT_URL="${EXT_URL:-$IP_ADDR}"
        info "Using unattended configuration (ports: MQTT $MQTT_PORT, Mgr $MGR_PORT, GUI $GUI_PORT, Node-RED $NR_PORT)."
        return
    fi

    echo -e "${CYAN}--- Database Security ---${NC}"
    while true; do
        read -s -p "Enter password for MariaDB 'root' user: " DB_ROOT_PASS; echo ""
        read -s -p "Confirm MariaDB 'root' password: " DB_ROOT_PASS_CONF; echo ""
        if [ "$DB_ROOT_PASS" == "$DB_ROOT_PASS_CONF" ] && [ ! -z "$DB_ROOT_PASS" ]; then
            break
        fi
        warning "Passwords do not match or are empty. Try again."
    done
    echo ""
    while true; do
        read -s -p "Enter password for Application DB user 'oso': " DB_OSO_PASS; echo ""
        read -s -p "Confirm Application DB user 'oso' password: " DB_OSO_PASS_CONF; echo ""
        if [ "$DB_OSO_PASS" == "$DB_OSO_PASS_CONF" ] && [ ! -z "$DB_OSO_PASS" ]; then
            break
        fi
        warning "Passwords do not match or are empty. Try again."
    done

    echo -e "\n${CYAN}--- MQTT Security ---${NC}"
    while true; do
        read -s -p "Enter password for MQTT user '$MQTT_ID': " MQTT_PASS; echo ""
        read -s -p "Confirm MQTT password: " MQTT_PASS_CONF; echo ""
        if [ "$MQTT_PASS" == "$MQTT_PASS_CONF" ] && [ ! -z "$MQTT_PASS" ]; then
            break
        fi
        warning "Passwords do not match or are empty. Try again."
    done

    echo -e "\n${CYAN}--- Service Ports ---${NC}"
    read -p "MQTT Port [1883]: " MQTT_PORT; MQTT_PORT=${MQTT_PORT:-1883}
    read -p "Services Manager Port [8080]: " MGR_PORT; MGR_PORT=${MGR_PORT:-8080}
    read -p "GUI Port [8082]: " GUI_PORT; GUI_PORT=${GUI_PORT:-8082}
    read -p "Node-RED Port [1880]: " NR_PORT; NR_PORT=${NR_PORT:-1880}
    
    IP_ADDR=$(hostname -I | awk '{print $1}')
    read -p "External IP/URL for web access [$IP_ADDR]: " EXT_URL; EXT_URL=${EXT_URL:-$IP_ADDR}
}

function config_setup() {
    echo -e "${MAGENTA}${BOLD}# STEP 4: Configuration Deployment${NC}"
    
    cd "$REPO_DIR" || error "Project directory not found"
    CONFIG_PATH="$REPO_DIR/config/config.json"
    mkdir -p "$REPO_DIR/config"

    # --- SSL Certificate Generation (On-the-fly) ---
    info "Generating unique SSL certificates for secure HTTPS access..."
    CERT_DIR="$SRC_BASE/addons/services/certs"
    mkdir -p "$CERT_DIR"
    
    # We generate a new one every time to ensure maximum privacy per PLC
    openssl req -x509 -newkey rsa:4096 -keyout "$CERT_DIR/key.pem" -out "$CERT_DIR/cert.pem" -days 3650 -nodes -subj "/C=ES/ST=Alicante/L=Borrell/O=OsoLogic/OU=PLC/CN=$EXT_URL" 2>/dev/null
    chmod 644 "$CERT_DIR/cert.pem" "$CERT_DIR/key.pem"
    success "Local SSL certificates generated successfully."

    # --- Auto-generate unique secret_keys ---
    GUI_SECRET_KEY=$(openssl rand -hex 32)
    NR_SECRET_KEY=$(openssl rand -hex 32)

    # --- Granular JSON update (field-by-field, preserving template) ---
    info "Updating config.json (granular, preserving all other fields)..."
    
    if [ ! -f "$CONFIG_PATH" ]; then
        error "Template config.json not found at $CONFIG_PATH. Ensure the repository was cloned correctly."
    fi

    jq \
        --arg dbh "$DB_HOST"        \
        --arg dbu "$DB_USER"        \
        --arg dbp "$DB_OSO_PASS"    \
        --arg dbn "$DB_NAME"        \
        --arg mqh "$MQTT_HOST"      \
        --argjson mqp "$MQTT_PORT"  \
        --arg mqi "$MQTT_ID"        \
        --arg mqpw "$MQTT_PASS"     \
        --argjson gup "$GUI_PORT"   \
        --arg eurl "https://$EXT_URL:$GUI_PORT/" \
        --arg gsk "$GUI_SECRET_KEY" \
        --arg nrh "$MQTT_HOST"      \
        --argjson nrp "$NR_PORT"    \
        --arg nreurl "https://$EXT_URL:$NR_PORT/" \
        --arg nrsk "$NR_SECRET_KEY" \
        '
        # Database
        .database.host     = $dbh |
        .database.user     = $dbu |
        .database.password = $dbp |
        .database.db_name  = $dbn |

        # MQTT
        .services.mqtt.broker_address = $mqh |
        .services.mqtt.port           = $mqp |
        .services.mqtt.client_id      = $mqi |
        .services.mqtt.password       = $mqpw |

        # GUI
        .services.gui.port         = $gup |
        .services.gui.external_url = $eurl |
        .services.gui.secret_key   = $gsk |

        # Node-RED
        .services.nodered.host              = $nrh |
        .services.nodered.port              = $nrp |
        .services.nodered.external_url      = $nreurl |
        .services.nodered.credential_secret = $nrsk
        ' "$CONFIG_PATH" > "$CONFIG_PATH.tmp" && mv "$CONFIG_PATH.tmp" "$CONFIG_PATH"

    # Ensure 'oso' user has permissions to write to config for the web manager
    chown oso:oso "$REPO_DIR/config/config.json"
    chmod 660 "$REPO_DIR/config/config.json"
    chmod 771 "$REPO_DIR/config"

    success "config.json updated successfully (all template keys preserved)."

# --- Node-RED HTTPS & Project Features Customization ---
    OSO_HOME=$(getent passwd oso | cut -d: -f6 || echo "/home/oso")
    NR_SETTINGS="$OSO_HOME/.node-red/settings.js"
    
    # 1. Ensure directory exists
    mkdir -p "$OSO_HOME/.node-red"

    info "Generating secure and minimalist Node-RED configuration..."
    
    # 2. Generate clean settings.js using Here-Doc
    cat <<EOF > "$NR_SETTINGS"
/**
 * Node-RED configuration for PLC OsoLogic
 * Automatically generated by the system installer.
 */
const fs = require("fs");

module.exports = {
    // --- Interface & Ports ---
    uiPort: process.env.PORT || $NR_PORT,
    flowFile: 'flows.json',

    // --- Security & Cryptography ---
    credentialSecret: "$NR_SECRET_KEY",

    // --- HTTPS Configuration ---
    https: {
        key: fs.readFileSync("$CERT_DIR/key.pem"),
        cert: fs.readFileSync("$CERT_DIR/cert.pem")
    },

    // --- Project Features (Git) ---
    editorTheme: {
        projects: {
            enabled: true
        }
    },

    // --- Environment ---
    userDir: "$OSO_HOME/.node-red",
    
    // Basic logging level
    logging: {
        console: {
            level: "info",
            metrics: false,
            audit: false
        }
    }
};
EOF

    # 3. Apply permissions and restart
    chown -R oso:oso "$OSO_HOME/.node-red" 2>/dev/null || true
    systemctl restart nodered 2>/dev/null || true
    success "Node-RED secure settings applied successfully."

    # --- MariaDB Configuration ---
    info "Mastering MariaDB configuration..."
    
    # 1. Enable 0.0.0.0 binding for remote access
    CONF_FILE=""
    FOR_FILES=("/etc/mysql/mariadb.conf.d/50-server.cnf" "/etc/mysql/my.cnf.d/server.cnf" "/etc/mysql/my.cnf")
    for f in "${FOR_FILES[@]}"; do [ -f "$f" ] && CONF_FILE="$f" && break; done
    
    if [ -n "$CONF_FILE" ]; then
        sed -i 's/^[[:space:]#]*bind-address[[:space:]]*=.*/bind-address = 0.0.0.0/' "$CONF_FILE"
        systemctl restart mariadb 2>/dev/null || true
    fi

    # --- MQTT Broker Security ---
    info "Verifying Mosquitto broker security for user '$MQTT_ID'..."
    touch /etc/mosquitto/passwd
    mosquitto_passwd -b /etc/mosquitto/passwd "$MQTT_ID" "$MQTT_PASS" 2>/dev/null
    
    cat <<EOF > /etc/mosquitto/conf.d/default.conf
allow_anonymous false
password_file /etc/mosquitto/passwd
listener $MQTT_PORT 0.0.0.0
EOF
    systemctl restart mosquitto 2>/dev/null || true
    success "Mosquitto configured: user '$MQTT_ID' created, anonymous access disabled, listening on port $MQTT_PORT."
}

# --- 2.1.1 MariaDB Security & User Setup ---
function database_security_setup() {
    info "Configuring database users and permissions..."
    
    # Escape single quotes to prevent SQL injection/errors
    SAFE_DB_OSO_PASS=$(echo "$DB_OSO_PASS" | sed "s/'/\\\\'/g")
    SAFE_DB_ROOT_PASS=$(echo "$DB_ROOT_PASS" | sed "s/'/\\\\'/g")

    # This is executed AFTER phpMyAdmin installation to ensure the password is set correctly
    MYSQL_PWD="" mysql -u root <<EOF || warning "Database setup had issues, but continuing..."
    CREATE DATABASE IF NOT EXISTS $DB_NAME;
    
    -- Configure Application user 'oso'
    CREATE OR REPLACE USER '$DB_USER'@'localhost' IDENTIFIED BY '$SAFE_DB_OSO_PASS';
    CREATE OR REPLACE USER '$DB_USER'@'%' IDENTIFIED BY '$SAFE_DB_OSO_PASS';
    GRANT ALL PRIVILEGES ON $DB_NAME.* TO '$DB_USER'@'localhost';
    GRANT ALL PRIVILEGES ON $DB_NAME.* TO '$DB_USER'@'%';
    
    -- Secure MariaDB root user
    ALTER USER 'root'@'localhost' IDENTIFIED BY '$SAFE_DB_ROOT_PASS';
    
    FLUSH PRIVILEGES;
EOF

    success "Database configured: 'root' and '$DB_USER' accounts updated."
}

# --- 2.2 Node-RED Custom Interface Setup ---
function nodered_flows_setup() {
    echo -e "${MAGENTA}${BOLD}# STEP 4.2: Node-RED Custom Interface Setup${NC}"
    
    # 1. Use the local interface bundled within the OSOlogic repository
    NR_INTERFACE_DIR="$SRC_BASE/addons/services/node-red-interface"
    
    if [ ! -d "$NR_INTERFACE_DIR" ]; then
        error "Node-RED interface directory not found at $NR_INTERFACE_DIR. Repository structure is unexpected."
    fi

    if [ -f "$NR_INTERFACE_DIR/flows.json" ]; then
        OSO_HOME=$(getent passwd oso | cut -d: -f6 || echo "/home/oso")
        NR_DIR="$OSO_HOME/.node-red"
        mkdir -p "$NR_DIR"
        
        # We target flows.json as the standard
        FLOW_FILE="$NR_DIR/flows.json"
        
        # Clear any existing credentials file to force Node-RED to re-import from flows.json
        rm -f "$NR_DIR/flows_cred.json"
        
        info "Configuring custom flows from local repository..."
        
        # Copy and fix paths/credentials using jq
        # We use standard field names for Node-RED MySQL nodes (host, user, password, database)
        jq \
            --arg dbh "$DB_HOST" \
            --arg dbn "$DB_NAME" \
            'map(
                if .type == "MySQLdatabase" then
                    .host = $dbh |
                    .port = "3306" |
                    .db   = $dbn
                else . end
            )' "$NR_INTERFACE_DIR/flows.json" > "$FLOW_FILE"
            
        chown oso:oso "$FLOW_FILE"
        success "Custom flows imported and configured at $FLOW_FILE."

        # 1.5 Install custom nodes from the interface if it contains a package.json
        if [ -f "$NR_INTERFACE_DIR/package.json" ]; then
            info "Custom package.json detected. Installing custom nodes..."
            su - oso -c "cd ~/.node-red && npm install $NR_INTERFACE_DIR --save" || warning "Failed to install custom nodes from local interface."
        fi
    else
        error "flows.json not found at $NR_INTERFACE_DIR. Cannot configure Node-RED interface."
    fi

    # 2. Install required npm packages
    info "Installing Node-RED dependencies (Flowfuse Dashboard, MySQL, Telegram, Email)..."
    su - oso -c "cd ~/.node-red && npm install @flowfuse/node-red-dashboard node-red-node-mysql node-red-contrib-mysql2 node-red-contrib-telegrambot node-red-node-email --save" || error "Some Node-RED packages failed to install. Check for internet connectivity or npm errors."

    # 3. Restart Node-RED
    info "Restarting Node-RED to apply changes..."
    systemctl restart nodered
    success "Node-RED interface setup completed."
    echo ""
}

# --- 2.1 Extra: phpMyAdmin Installation ---
function install_phpmyadmin() {
    echo -e "${MAGENTA}${BOLD}# EXTRA: Database Management (phpMyAdmin)${NC}"
    if [ "${OSO_UNATTENDED:-0}" = "1" ]; then
        INSTALL_PMA="${INSTALL_PMA:-n}"
        info "phpMyAdmin install: ${INSTALL_PMA}"
    else
        read -p "Do you want to install phpMyAdmin for easy database access? (y/n) [n]: " INSTALL_PMA
        INSTALL_PMA=${INSTALL_PMA:-n}
    fi
    
    if [[ "$INSTALL_PMA" =~ ^[Yy]$ ]]; then
        info "Installing phpMyAdmin (this may take a minute)..."
        
        # Pre-configure for unattended install
        export DEBIAN_FRONTEND=noninteractive

        # Pre-seed debconf with an empty root DB password.
        # This is to avoid the error during the installation of phpmyadmin of that it doesn't have access to the database
        # because we haven't applied the root password yet (it will be applied in database_security_setup).
        debconf-set-selections <<EOF
phpmyadmin phpmyadmin/dbconfig-install boolean true
phpmyadmin phpmyadmin/mysql/admin-user string root
phpmyadmin phpmyadmin/mysql/app-pass password $DB_OSO_PASS
phpmyadmin phpmyadmin/app-password-confirm password $DB_OSO_PASS
phpmyadmin phpmyadmin/reconfigure-webserver multiselect apache2
EOF

        # Install Apache, PHP, required modules, and phpMyAdmin
        apt-get install -y apache2 php libapache2-mod-php php-mysql phpmyadmin 2>/dev/null || warning "phpMyAdmin install had some issues. Check manually."
        
        # Force compatible Apache MPM module for PHP processing
        a2dismod mpm_event mpm_worker 2>/dev/null || true
        a2enmod mpm_prefork 2>/dev/null || true
        
        # Detect exact PHP version and enable its specific Apache module
        PHP_VER=$(php -r 'echo PHP_MAJOR_VERSION.".".PHP_MINOR_VERSION;' 2>/dev/null)
        if [ -n "$PHP_VER" ]; then
            a2enmod "php$PHP_VER" 2>/dev/null || true
        fi
        
        # Configure Apache for phpMyAdmin
        if [ -f /etc/phpmyadmin/apache.conf ] && [ -d /etc/apache2/conf-available ]; then
            ln -sf /etc/phpmyadmin/apache.conf /etc/apache2/conf-available/phpmyadmin.conf
            
            # Enable configuration (must be called without .conf extension)
            a2enconf phpmyadmin 2>/dev/null || true
            
            # Restart Apache to apply all module and configuration changes
            systemctl restart apache2 2>/dev/null || true
        fi
        
        success "phpMyAdmin setup completed."
        info "Access it at: http://$EXT_URL/phpmyadmin"
    else
        info "Skipping phpMyAdmin installation."
    fi
    echo ""
}

# --- 3. System & Dependency Installation ---
function install_dependencies() {
    # Helper function to check if a package is installed
    # Uses dpkg-query with strict status check to avoid false positives on
    # packages with status 'un' (unknown/not-installed) that dpkg -l returns 0 for
    is_package_installed() {
        dpkg-query -W -f='${Status}' "$1" 2>/dev/null | grep -q "install ok installed"
    }

    info "Updating system package list..."
    apt-get update -y
    
    # 1. Essential Tools Check & Install
    PYTHON_VER=$(python3 -c 'import sys; print(f"{sys.version_info.major}.{sys.version_info.minor}")')
    ESSENTIAL_TOOLS=(git python3 python3-pip python3-venv "python${PYTHON_VER}-venv" jq curl wget build-essential libmariadb-dev cmake)
    MISSING_TOOLS=()
    for tool in "${ESSENTIAL_TOOLS[@]}"; do
        if ! is_package_installed "$tool"; then
            MISSING_TOOLS+=("$tool")
        fi
    done

    if [ ${#MISSING_TOOLS[@]} -gt 0 ]; then
        info "Installing missing essential tools: ${MISSING_TOOLS[*]}"
        apt-get install -y "${MISSING_TOOLS[@]}" || error "Failed to install some essential tools."
    else
        success "All essential tools are already installed."
    fi

    # 2. MariaDB and Mosquitto Check & Install
    CORE_SERVICES=(mariadb-server mosquitto mosquitto-clients)
    MISSING_SERVICES=()
    for service in "${CORE_SERVICES[@]}"; do
        if ! is_package_installed "$service"; then
            MISSING_SERVICES+=("$service")
        fi
    done

    if [ ${#MISSING_SERVICES[@]} -gt 0 ]; then
        info "Installing missing core services: ${MISSING_SERVICES[*]}"
        apt-get install -y "${MISSING_SERVICES[@]}" || error "Failed to install MariaDB or Mosquitto"
    else
        success "MariaDB and Mosquitto are already installed. Proceeding to configuration..."
    fi
    
    systemctl enable mosquitto >/dev/null 2>&1
    systemctl start mosquitto >/dev/null 2>&1



    # --- 3.1 Setup 'oso' user ---
    SKIP_USER_SETUP=false
    if id "oso" &>/dev/null; then
        # Check if the user is already well configured (membership in essential groups)
        MISSING_GROUPS=()
        for grp in sudo dialout gpio i2c spi; do
            if ! groups oso | grep -q "\b$grp\b"; then
                MISSING_GROUPS+=("$grp")
            fi
        done

        if [ ${#MISSING_GROUPS[@]} -eq 0 ]; then
            success "User 'oso' is already well configured (exists and has all hardware permissions)."
            read -p "Do you want to reconfigure it anyway (change password)? (y/n) [n]: " RECONF_OSO
            RECONF_OSO=${RECONF_OSO:-n}
        else
            warning "User 'oso' exists but is missing some permissions: ${MISSING_GROUPS[*]}"
            read -p "Do you want to fix and reconfigure user 'oso'? (y/n) [y]: " RECONF_OSO
            RECONF_OSO=${RECONF_OSO:-y}
        fi

        if [[ ! "$RECONF_OSO" =~ ^[Yy]$ ]]; then
            info "Keeping existing 'oso' user configuration. Continuing..."
            SKIP_USER_SETUP=true
            CREATE_NEW=false
        else
            info "Reconfiguring existing user 'oso'..."
            CREATE_NEW=false
        fi
    else
        info "Creating system user 'oso' for Node-RED and services..."
        CREATE_NEW=true
    fi

    # Proceed with configuration only if needed
    if [ "$SKIP_USER_SETUP" = false ]; then
        while true; do
            read -rs -p "Enter password for user 'oso': " OSO_PASS; echo ""
            read -rs -p "Confirm password for user 'oso': " OSO_PASS_CONF; echo ""
            if [ "$OSO_PASS" == "$OSO_PASS_CONF" ] && [ ! -z "$OSO_PASS" ]; then
                break
            else
                warning "Passwords do not match or are empty. Try again."
            fi
        done

        if [ "$CREATE_NEW" = true ]; then
            useradd -m -s /bin/bash oso || error "Failed to create user 'oso'"
        fi
        
        echo "oso:$OSO_PASS" | chpasswd
        
        # Add to hardware groups for PLC functions
        for grp in sudo dialout gpio i2c spi; do
            groupadd -f $grp || true
            usermod -aG $grp oso
        done
    fi

    # --- 3.1.2 Allow user 'oso' to manage project services without password ---
    info "Configuring service permissions (polkit) for user 'oso'..."
    POLKIT_FILE="/etc/polkit-1/rules.d/10-oso-services.rules"
    mkdir -p /etc/polkit-1/rules.d/
    cat <<EOF > "$POLKIT_FILE"
polkit.addRule(function(action, subject) {
    if (subject.user == "oso") {
        if (action.id == "org.freedesktop.systemd1.manage-units") {
            var unit = action.lookup("unit");
            if (unit && (unit.substring(0, 11) == "plc_osologic-" || unit == "nodered.service")) {
                return polkit.Result.YES;
            }
        } else if (action.id == "org.freedesktop.systemd1.manage-unit-files" || action.id == "org.freedesktop.systemd1.reload-daemon") {
            return polkit.Result.YES;
        }
    }
});
EOF
    chmod 644 "$POLKIT_FILE"
    success "User 'oso' configured with service and hardware permissions."

    # --- 3.2 Install Node-RED (Non-Interactive) ---
    if command -v node-red &> /dev/null; then
        success "Node-RED already installed."
    else
        info "Running Node-RED installer for user 'oso' (unattended)..."
        
        # 1. Grant temporary passwordless sudo to 'oso'
        # This prevents the Node-RED installer from hanging while asking for an apt-get password
        echo "oso ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/oso_temp_nodered
        chmod 0440 /etc/sudoers.d/oso_temp_nodered

        # 2. Execute the official installer (Handling errors to ensure cleanup)
        if ! su - oso -c "bash <(curl -sL https://raw.githubusercontent.com/node-red/linux-installers/master/deb/update-nodejs-and-nodered) --confirm-install --skip-pi"; then
            rm -f /etc/sudoers.d/oso_temp_nodered
            error "Node-RED base installation failed."
        fi
        
        # 3. Revoke the temporary permissions for security (ESTO AHORA SIEMPRE SE EJECUTA)
        rm -f /etc/sudoers.d/oso_temp_nodered
            
        # 4. Enable the service (the installer now correctly configures it for the 'oso' user)
        if [ -f "/lib/systemd/system/nodered.service" ] || [ -f "/etc/systemd/system/nodered.service" ]; then
            systemctl enable nodered.service
            success "Node-RED service enabled for user 'oso'."
        else
            warning "Node-RED service file not found. You might need to enable it manually."
        fi
    fi
}

# --- 4. Service Environment Setup ---
function service_env_setup() {
    echo -e "${MAGENTA}${BOLD}# STEP 4: Setting up Python Environments${NC}"
    
    cd "$REPO_DIR" || error "Failed to access project directory $REPO_DIR"
    
    # SRC_BASE is already detected and global
    info "Using source base: $SRC_BASE"

    info "Setting up centralized Python Virtual Environment..."
    VENV_DIR="$REPO_DIR/venv"
    python3 -m venv "$VENV_DIR"
    
    info "Installing Python dependencies globally for all services..."
    "$VENV_DIR/bin/python3" -m pip install --upgrade pip
    if [ -f "$REPO_DIR/config/requirements.txt" ]; then
        info "Installing verified dependencies from requirements.txt..."
        "$VENV_DIR/bin/python3" -m pip install -r "$REPO_DIR/config/requirements.txt"
    else
        warning "requirements.txt not found! Falling back to latest versions..."
        "$VENV_DIR/bin/python3" -m pip install flask mysql-connector-python paho-mqtt pyModbusTCP pymysql pymodbus cryptography requests numpy
    fi
    chmod +x "$VENV_DIR/bin/python" 2>/dev/null || true
    chmod +x "$VENV_DIR/bin/python3" 2>/dev/null || true
    chown -R oso:oso "$VENV_DIR" 2>/dev/null || true
    
    echo ""
}

# --- 5. Service Deployment (systemd) ---
function deploy_services() {
    echo -e "${MAGENTA}${BOLD}# STEP 5: Deploying System Services${NC}"
    
    # 1. Path Definitions (Ensure these variables are set earlier in your script)
    ABS_MGR="$SRC_BASE/addons/services/services_manager"
    ABS_GUI="$SRC_BASE/addons/services/GUI"
    ABS_MQTT="$SRC_BASE/addons/services/mqtt"
    ABS_MODBUS="$SRC_BASE/addons/services/modbustcp"
    ABS_CORE="$SRC_BASE/core/build/core_plc_osologic"
    ABS_ROOT="$REPO_DIR"

    # 1.1 Compile PLC CORE
    echo -e "${MAGENTA}${BOLD}# STEP 5.1: Compiling PLC Core (C++)${NC}"
    if [ -d "$SRC_BASE/core" ]; then
        info "Core source directory found. Compiling..."
        cd "$SRC_BASE/core"
        make clean || true
        make -j$(nproc) || error "Failed to compile PLC Core. Check build-essential and libraries."
        success "PLC Core compiled successfully."
        cd "$ABS_ROOT"
    else
        error "Core source directory not found at $SRC_BASE/core."
    fi

    # 2. MASTER FUNCTION: Generates the .service file with efficiency and security
    # Parameters: Name, Description, WorkDir, ExecCmd, ExtraOptions
    build_service() {
        local NAME=$1; local DESC=$2; local WDIR=$3; local CMD=$4; local OPTS=$5
        
        cat <<EOF > "/etc/systemd/system/$NAME.service"
[Unit]
Description=$DESC
After=network.target

[Service]
Type=simple
User=oso
Group=oso
WorkingDirectory=$WDIR
ExecStart=$CMD
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal
Environment=PYTHONUNBUFFERED=1
# Security Hardening
ProtectSystem=strict
PrivateTmp=true
ReadWritePaths=$WDIR
$OPTS

[Install]
WantedBy=multi-user.target
EOF
        info "Service $NAME configured."
    }

    # 3. EXECUTION: Specific configuration for each service

    # PLC CORE: Requires hardware access (GPIO) and physical memory mapping for Orange Pi
    # Apply capabilities to the binary for manual execution/terminal testing
    setcap cap_sys_rawio,cap_dac_override+ep "$ABS_CORE" 2>/dev/null
    
    # Service options: Bypass root check for WiringPi and set hardware capabilities
    CORE_OPTS="Environment=WIRINGPI_GPIOMEM=1
AmbientCapabilities=CAP_SYS_RAWIO CAP_DAC_OVERRIDE
CapabilityBoundingSet=CAP_NET_BIND_SERVICE CAP_SYS_RAWIO CAP_DAC_OVERRIDE
ReadWritePaths=$ABS_ROOT"
    
    build_service "plc_osologic-core" "PLC OSOlogic Core Application" "$ABS_ROOT" "$ABS_CORE" "$CORE_OPTS"

    VENV_PYTHON="$REPO_DIR/venv/bin/python"

    # GUI: Database management interface
    build_service "plc_osologic-gui" "PLC OsoLogic Database Management GUI" "$ABS_GUI" "$VENV_PYTHON $ABS_GUI/app.py" ""

    # MANAGER: Service management interface
    build_service "plc_osologic-manager" "PLC OSOlogic Service Manager Web Interface" "$ABS_MGR" "$VENV_PYTHON $ABS_MGR/app.py" ""

    # MODBUS TCP: Requires permission to bind to port 502 (CAP_NET_BIND_SERVICE)
    MODBUS_OPTS="AmbientCapabilities=CAP_NET_BIND_SERVICE
CapabilityBoundingSet=CAP_NET_BIND_SERVICE"
    build_service "plc_osologic-modbustcp" "PLC OSOlogic Modbus TCP Gateway" "$ABS_MODBUS" "$VENV_PYTHON $ABS_MODBUS/server.py" "$MODBUS_OPTS"

    # MQTT: Data gateway integration
    build_service "plc_osologic-mqtt" "PLC OSOlogic MQTT Gateway" "$ABS_MQTT" "$VENV_PYTHON $ABS_MQTT/main.py" ""

    # 4. FINAL RELOAD
    systemctl daemon-reload
    success "All services deployed successfully under user 'oso' with specific hardware capabilities."
}

# --- 6. System Performance Optimization ---
function optimize_performance() {
    echo -e "${MAGENTA}${BOLD}# STEP 6: Optimizing System Performance${NC}"
    
    info "Setting CPU Scaling Governor to 'performance'..."
    
    # Install cpufrequtils if not present
    if ! command -v cpufreq-set &> /dev/null; then
        apt-get install -y cpufrequtils || warning "Could not install cpufrequtils. Skipping CPU optimization."
    fi
    
    if command -v cpufreq-set &> /dev/null; then
        # Configure for persistence
        if grep -q "GOVERNOR=" /etc/default/cpufrequtils 2>/dev/null; then
            sed -i 's/^GOVERNOR=.*/GOVERNOR="performance"/' /etc/default/cpufrequtils
        else
            echo 'GOVERNOR="performance"' >> /etc/default/cpufrequtils
        fi
        
        # Apply immediately to all cores
        for i in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
            [ -f "$i" ] && echo "performance" > "$i" 2>/dev/null || true
        done
        
        systemctl restart cpufrequtils 2>/dev/null || true
        success "System optimized for maximum performance."
    fi
    echo ""
}

# --- Core Database Execution ---
function init_core_database() {
    echo -e "${MAGENTA}${BOLD}# STEP 7: Initializing Core Database${NC}"
    info "Executing core database SQL files..."
    
    # Check variables needed
    DB_NAME=${DB_NAME:-"PLC"}
    DB_ROOT_PASS=${DB_ROOT_PASS:-""}
    
    if [ -f "$SRC_BASE/core/database/plc_osologic_database.sql" ]; then
        info "Importing plc_osologic_database.sql..."
        MYSQL_PWD="$DB_ROOT_PASS" mysql -u root "$DB_NAME" < "$SRC_BASE/core/database/plc_osologic_database.sql" || warning "Failed to execute plc_osologic_database.sql"
    else
        warning "Core database SQL file plc_osologic_database.sql not found."
    fi

    if [ -f "$SRC_BASE/core/database/PLC_config.sql" ]; then
        info "Importing PLC_config.sql..."
        MYSQL_PWD="$DB_ROOT_PASS" mysql -u root "$DB_NAME" < "$SRC_BASE/core/database/PLC_config.sql" || warning "Failed to execute PLC_config.sql"
    else
        warning "Core database SQL file PLC_config.sql not found."
    fi
    success "Core database initializaton completed."
    echo ""
}
# --- Easter Egg / Fun Command ---
function create_oso_command() {
    echo -e "${MAGENTA}${BOLD}# Creating 'oso' CLI command...${NC}"
    cat << 'EOF' > /usr/local/bin/oso
#!/bin/bash

# Checking the first argument provided by the human
case "$1" in
    -h|--help)
        echo "🐻 OSO COMMAND - Factory Owner Management System 🐻"
        echo "Usage: oso [option]"
        echo ""
        echo "Available options (and don't ask for more):"
        echo "  -h, --help      Show this damn help menu."
        echo "  -s, --status    Ask me how MY plant is doing."
        echo "  -w, --who       Find out who really runs this place."
        echo "  -j, --joke      If you want to waste time with jokes."
        ;;
        
    -s|--status)
        echo "🐻 The plant is perfect, obviously. The contactors are humming, Modbus is communicating, and your Node-RED flows... well, they are doing their best. Don't touch anything! ⚡"
        ;;
        
    -w|--who)
        echo "🐻 I am Oso. I own this PLC, the RAM, the CPU, and your patience. You are just the biped that swaps my power supply when it burns out. 👑🐾"
        ;;
        
    -j|--joke)
        echo "🐻 A joke? Your last firmware update. That one was a real classic. 🥁"
        ;;
        
    "")
        echo "🐻 Are you waking me from hibernation and not giving the command any arguments? Use 'oso --help' before I cut your SSH access. 🔌"
        ;;
        
    *)
        echo "🐻 Grrr... what does '$1' mean? Speak in binary or use 'oso --help'. I don't have time for human syntax errors. ❓"
        ;;
esac
EOF
    chmod +x /usr/local/bin/oso
    success "'oso' command created successfully."
    echo ""
}

# --- Main Execution ---
print_logo
check_root
install_dependencies
git_setup
collect_config
config_setup
nodered_flows_setup
# Install phpMyAdmin BEFORE setting the DB password to avoid access issues during setup
install_phpmyadmin
database_security_setup
service_env_setup
deploy_services
optimize_performance
init_core_database
create_oso_command

# Start and enable only plc_osologic-manager
info "Starting and enabling plc_osologic-manager service..."
systemctl enable plc_osologic-manager >/dev/null 2>&1
systemctl start plc_osologic-manager >/dev/null 2>&1
success "plc_osologic-manager service is now running."

echo -e "${YELLOW}${BOLD}================================================${NC}"
echo -e "${GREEN}${BOLD}      INSTALLATION COMPLETED SUCCESSFULLY!      ${NC}"
echo -e "${YELLOW}${BOLD}================================================${NC}"
echo ""
echo -e "Access Point Connectivity:"
echo -e "Manager URL: ${CYAN}https://$EXT_URL:$MGR_PORT${NC}"
echo -e "Database GUI: ${CYAN}http://$EXT_URL/phpmyadmin${NC} (if installed)"
echo ""
echo -e "Note: Only plc_osologic-manager has been started and enabled automatically."
echo -e "To manually start other services:"
echo -e "  ${BOLD}systemctl start plc_osologic-gui plc_osologic-mqtt plc_osologic-modbustcp${NC}"
echo ""
echo -e "${BLUE}OsoLogic Systems - Built for Industry.${NC}"

# Open Manager Web Interface
info "Opening Service Manager in web browser..."
if command -v xdg-open &> /dev/null; then
    if [ -n "$SUDO_USER" ]; then
        su - "$SUDO_USER" -c "xdg-open https://$EXT_URL:$MGR_PORT >/dev/null 2>&1" &
    else
        xdg-open "https://$EXT_URL:$MGR_PORT" >/dev/null 2>&1 &
    fi
elif command -v sensible-browser &> /dev/null; then
    if [ -n "$SUDO_USER" ]; then
        su - "$SUDO_USER" -c "sensible-browser https://$EXT_URL:$MGR_PORT >/dev/null 2>&1" &
    else
        sensible-browser "https://$EXT_URL:$MGR_PORT" >/dev/null 2>&1 &
    fi
else
    # Fallback to python webbrowser module
    if [ -n "$SUDO_USER" ]; then
        su - "$SUDO_USER" -c "python3 -m webbrowser -t https://$EXT_URL:$MGR_PORT >/dev/null 2>&1" &
    else
        python3 -m webbrowser -t "https://$EXT_URL:$MGR_PORT" >/dev/null 2>&1 &
    fi
fi