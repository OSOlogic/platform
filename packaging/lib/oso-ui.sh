# shellcheck shell=bash
# ==============================================================================
# oso-ui.sh — terminal UI abstraction for the OSOLogic setup wizard
# ==============================================================================
# Auto-selects a frontend and exposes one set of helpers so the wizard renders
# the same on any terminal:
#
#   dialog    → richest full-screen ncurses UI (progress boxes, colour)
#   whiptail  → ncurses fallback (present by default on Debian/Armbian/Orange Pi)
#   plain     → last-resort text prompts (serial console / dumb $TERM)
#
# Every input helper leaves its result in the global UI_VALUE and returns 0 on
# OK / 1 on Cancel. Source this file; do not execute it.
#
# (C) Roig Borrell S.L. · Ibercomp S.L. — part of OSOLogic — AGPL-3.0
# ==============================================================================

UI_BACKEND=""          # dialog | whiptail | plain
UI_VALUE=""            # last collected value
UI_TITLE="OSOLogic Setup"
UI_BACKTITLE="OSOLogic® — The Modern and Open Automation Platform"

# --- backend selection --------------------------------------------------------
ui_init() {
    # Honour an explicit override: OSO_UI=plain|whiptail|dialog
    case "${OSO_UI:-}" in
        dialog|whiptail|plain) UI_BACKEND="$OSO_UI" ;;
        *)
            if [ ! -t 0 ] || [ ! -t 1 ]; then
                UI_BACKEND="plain"                      # not a real terminal
            elif [ "${TERM:-dumb}" = "dumb" ]; then
                UI_BACKEND="plain"
            elif command -v dialog >/dev/null 2>&1; then
                UI_BACKEND="dialog"
            elif command -v whiptail >/dev/null 2>&1; then
                UI_BACKEND="whiptail"
            else
                UI_BACKEND="plain"
            fi
            ;;
    esac
    export UI_BACKEND
}

ui_backend() { printf '%s\n' "$UI_BACKEND"; }

# Common flags for dialog/whiptail. Both write the chosen value to fd 2, so we
# swap fds 1<->2 to capture it on stdout.
_ui_ncurses() { [ "$UI_BACKEND" = "dialog" ] || [ "$UI_BACKEND" = "whiptail" ]; }

# --- informational ------------------------------------------------------------
# ui_msg TEXT [HEIGHT] [WIDTH]
ui_msg() {
    local text="$1" h="${2:-12}" w="${3:-70}"
    if _ui_ncurses; then
        "$UI_BACKEND" --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" \
            --msgbox "$text" "$h" "$w"
    else
        printf '\n%s\n\n[ press Enter to continue ] ' "$text"; read -r _
    fi
}

# ui_info TEXT — transient, non-blocking notice (falls back to a plain line)
ui_info() {
    local text="$1"
    if [ "$UI_BACKEND" = "dialog" ]; then
        dialog --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" --infobox "$text" 6 70
    elif [ "$UI_BACKEND" = "whiptail" ]; then
        whiptail --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" --infobox "$text" 6 70
    else
        printf '  ... %s\n' "$text"
    fi
}

# --- yes / no -----------------------------------------------------------------
# ui_yesno TEXT [default:yes|no] → 0 = yes, 1 = no
ui_yesno() {
    local text="$1" def="${2:-yes}"
    if _ui_ncurses; then
        local extra=""
        [ "$def" = "no" ] && extra="--defaultno"
        # shellcheck disable=SC2086
        "$UI_BACKEND" --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" $extra \
            --yesno "$text" 10 70
    else
        local ans hint="[Y/n]"; [ "$def" = "no" ] && hint="[y/N]"
        printf '\n%s %s ' "$text" "$hint"; read -r ans
        ans="${ans:-$def}"
        [[ "$ans" =~ ^([Yy]|yes)$ ]]
    fi
}

# --- text input ---------------------------------------------------------------
# ui_input TEXT [default] → UI_VALUE
ui_input() {
    local text="$1" def="${2:-}"
    if _ui_ncurses; then
        UI_VALUE=$("$UI_BACKEND" --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" \
            --inputbox "$text" 10 70 "$def" 3>&1 1>&2 2>&3) || return 1
    else
        local ans; printf '\n%s [%s]: ' "$text" "$def"; read -r ans
        UI_VALUE="${ans:-$def}"
    fi
    return 0
}

# --- password input (with confirmation + non-empty enforcement) ---------------
# ui_password TEXT → UI_VALUE  (loops until two entries match and are non-empty)
ui_password() {
    local text="$1" p1 p2
    while true; do
        if _ui_ncurses; then
            p1=$("$UI_BACKEND" --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" \
                --insecure --passwordbox "$text" 10 70 3>&1 1>&2 2>&3) || return 1
            p2=$("$UI_BACKEND" --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" \
                --insecure --passwordbox "Confirm: $text" 10 70 3>&1 1>&2 2>&3) || return 1
        else
            printf '\n%s: ' "$text"; read -rs p1; echo
            printf 'Confirm: '; read -rs p2; echo
        fi
        if [ -z "$p1" ]; then
            ui_msg "Password cannot be empty. Please try again."
            continue
        fi
        if [ "$p1" != "$p2" ]; then
            ui_msg "Passwords do not match. Please try again."
            continue
        fi
        UI_VALUE="$p1"
        return 0
    done
}

# --- single-choice menu -------------------------------------------------------
# ui_menu TEXT tag1 "label 1" tag2 "label 2" ... → UI_VALUE = chosen tag
ui_menu() {
    local text="$1"; shift
    if _ui_ncurses; then
        local n=$(( $# / 2 ))
        UI_VALUE=$("$UI_BACKEND" --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" \
            --menu "$text" 16 74 "$n" "$@" 3>&1 1>&2 2>&3) || return 1
    else
        printf '\n%s\n' "$text"
        local i=1 tag label; local -a tags=()
        while [ $# -gt 0 ]; do
            tag="$1"; label="$2"; shift 2
            tags+=("$tag"); printf '  %d) %s — %s\n' "$i" "$tag" "$label"; i=$((i+1))
        done
        local sel; printf 'Select [1]: '; read -r sel; sel="${sel:-1}"
        [[ "$sel" =~ ^[0-9]+$ ]] || sel=1
        UI_VALUE="${tags[$((sel-1))]:-${tags[0]}}"
    fi
    return 0
}

# --- multi-choice checklist ---------------------------------------------------
# ui_checklist TEXT tag1 "label 1" on tag2 "label 2" off ... → UI_VALUE = "tag1 tag3"
ui_checklist() {
    local text="$1"; shift
    if _ui_ncurses; then
        local n=$(( $# / 3 ))
        # dialog quotes items with spaces; strip surrounding quotes for uniformity
        UI_VALUE=$("$UI_BACKEND" --backtitle "$UI_BACKTITLE" --title "$UI_TITLE" \
            --separate-output --checklist "$text" 18 74 "$n" "$@" 3>&1 1>&2 2>&3) || return 1
        UI_VALUE=$(printf '%s' "$UI_VALUE" | tr '\n' ' ' | sed 's/  */ /g; s/ *$//')
    else
        printf '\n%s\n' "$text"
        local -a tags=() defs=()
        local tag label state
        while [ $# -gt 0 ]; do
            tag="$1"; label="$2"; state="$3"; shift 3
            tags+=("$tag"); defs+=("$state")
            printf '  - %-14s %s [default: %s]\n' "$tag" "$label" "$state"
        done
        local ans; printf 'Enter space-separated tags to ENABLE (blank = defaults): '; read -r ans
        if [ -z "$ans" ]; then
            UI_VALUE=""
            local i=0
            for tag in "${tags[@]}"; do
                [ "${defs[$i]}" = "on" ] && UI_VALUE="$UI_VALUE $tag"; i=$((i+1))
            done
            UI_VALUE="${UI_VALUE# }"
        else
            UI_VALUE="$ans"
        fi
    fi
    return 0
}

# --- run a command with live output -------------------------------------------
# ui_run TEXT command [args...] → streams stdout/stderr; returns command's status
ui_run() {
    local text="$1"; shift
    if [ "$UI_BACKEND" = "dialog" ]; then
        # programbox shows a scrolling live view of the command output
        "$@" 2>&1 | dialog --backtitle "$UI_BACKTITLE" --title "$text" \
            --programbox 24 92
        return "${PIPESTATUS[0]}"
    else
        # whiptail has no programbox; run in the clear with a header
        clear
        printf '======== %s ========\n\n' "$text"
        "$@"
    fi
}

# --- final summary + confirm --------------------------------------------------
# ui_confirm TEXT → 0 = proceed, 1 = abort
ui_confirm() {
    local text="$1"
    if _ui_ncurses; then
        "$UI_BACKEND" --backtitle "$UI_BACKTITLE" --title "Review — apply this setup?" \
            --yes-button "Install" --no-button "Cancel" --yesno "$text" 22 78
    else
        printf '\n%s\n\nProceed with installation? [y/N] ' "$text"
        local ans; read -r ans; [[ "$ans" =~ ^[Yy]$ ]]
    fi
}
