#!/bin/sh
# ==============================================================================
# Node-RED first-boot provisioner (OSOLogic sandbox)
# ==============================================================================
# On an empty /data volume this imports the reference flows (Diego's PLCBorrell
# interface — its custom blocks like TON/TOF are subflows inside flows.json) and
# installs the palette nodes those flows need, so `docker compose up` gives a
# working Node-RED with no manual "Manage Palette / import" steps. Runs once,
# then hands off to the normal Node-RED start.
# ==============================================================================
set -e
DATA=/data
REF=/reference

if [ ! -f "$DATA/.oso-seeded" ]; then
    echo "[oso-seed] first boot — provisioning reference flows + palette nodes"

    [ -f "$REF/flows.json" ]      && cp "$REF/flows.json"      "$DATA/flows.json"
    [ -f "$REF/flows_cred.json" ] && cp "$REF/flows_cred.json" "$DATA/flows_cred.json"

    # Palette nodes: whatever the reference package.json declares, plus the
    # Dashboard 2.0 pack the flows' ui-* nodes require. package.json is the
    # single source of truth — add a node type there and it installs next boot.
    DEPS=$(node -e "try{var d=require('$REF/package.json').dependencies||{};process.stdout.write(Object.keys(d).map(function(k){return k+'@'+d[k]}).join(' '))}catch(e){}" 2>/dev/null || true)
    echo "[oso-seed] installing palette: $DEPS @flowfuse/node-red-dashboard"
    if ( cd "$DATA" && npm install --no-audit --no-fund $DEPS @flowfuse/node-red-dashboard ) 2>&1 | tail -6; then
        echo "[oso-seed] palette installed"
    else
        echo "[oso-seed] palette install had warnings — finish any missing node via Manage Palette"
    fi

    touch "$DATA/.oso-seeded"
    echo "[oso-seed] provisioning done"
fi

exec npm --no-update-notifier --no-fund start --cache /data/.npm -- --userDir /data
