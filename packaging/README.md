# packaging/ — OSOlogic® Distribution Packaging

**(C) Roig Borrell S.L. · Ibercomp S.L.**
Part of [OSOlogic](https://github.com/OSOlogic/platform) — Open Industrial Automation Platform · AGPL-3.0

---

Distribution packaging definitions for installing OSOlogic on standard Linux distributions without flashing a full OS image.

## Directory Structure

```
packaging/
├── deb/    # Debian/Ubuntu .deb packages (dpkg/apt)
├── rpm/    # Red Hat/Fedora .rpm packages (dnf/yum)
└── ipk/    # OpenWrt/Entware .ipk packages (opkg)
```

### `deb/`
Debian package definitions for OSOlogic components. Enables installation on Debian, Ubuntu, and Raspberry Pi OS via `apt`. Includes service units (systemd), configuration file templates, and post-install scripts that configure the RT scheduler and `osodb`.

### `rpm/`
RPM package definitions for Red Hat Enterprise Linux, Fedora, and compatible distributions. Useful for industrial PCs running RHEL-based systems in environments where Debian is not the standard.

### `ipk/`
OpenWrt `.ipk` package definitions for deploying lightweight OSOlogic components on OpenWrt-based routers and industrial gateways. Targets low-resource embedded systems where only the gateway and MQTT stack are needed.

---

*OSOlogic® is developed and maintained by **Roig Borrell S.L.** and **Ibercomp S.L.***
