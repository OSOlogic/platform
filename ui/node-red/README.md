# OsoLogic Node-RED Interface

This is the **Node-RED** based control and visualization interface for the **OsoLogic PLC** ecosystem. It acts as the primary bridge (HMI) between the PLC Core (C++) and the operator, allowing real-time monitoring and control of I/O points via a MariaDB/MySQL database.

## 🚀 Purpose

The `PLCBorrell-node-red-interface` subproject provides a flow architecture designed for:
1.  **Bi-directional Synchronization**: Reading input/output states and sending control commands.
2.  **Engineering Abstraction (NET values)**: Working directly with real units (e.g., Bar, kV, %) without worrying about raw hardware values.
3.  **Flexible Interface**: Serving as a foundation for custom Dashboards and high-level supervision logic.

## 🏗️ Communication Architecture

The flow integrates with the **OsoLogic Core** using the database as a **Real-Time Mirror**:

### 📤 Reading (Polling)
Node-RED periodically queries the `rtmirror_complete` view. This view combines:
*   Dynamic data from `rtmirror` (current processed values).
*   Configuration from `module_io_config` (user labels, units).
*   Definitions from `model_io_definition` (data types, access).

### 📥 Writing (Commands)
When a control is actuated in Node-RED, a dynamic SQL query is generated to update the target value in the `rtmirror` table. The PLC Core detects this change and propagates it to the physical hardware.

## 🛠️ Requirements and Installation

1.  **Node-RED**: Version 3.0 or higher.
2.  **MySQL Node**: You must install `node-red-node-mysql` via the Manage Palette.
3.  **Database**: The OsoLogic Core must have initialized the `PLC` database.

### Steps to import:
1.  Copy the contents of `flows.json`.
2.  In Node-RED, go to **Import** -> **Clipboard**.
3.  Configure the MySQL database node with the correct IP and credentials:
    *   **Host**: Server IP (e.g., `localhost` or the Raspberry Pi/Industrial PC IP).
    *   **Port**: `3306`.
    *   **Database**: `PLC`.

## ⚙️ Flow Components

*   **Polling Database (0.1s)**: Injector node that triggers state reading every 100ms.
*   **MySQL Result Processor**: Function node that normalizes data types (e.g., converting 1/0 to Boolean for bits).
*   **Command Generator**: Specialized function that builds the bulk `UPDATE` statement to optimize write performance.
*   **UI Dashboard Nodes**: (Optional) Integrated visual elements for quick control.

## ⚠️ Important Technical Notes

*   **Label Uniqueness**: This flow relies on `user_label` configurations being unique project-wide.
*   **Security**: The current flow updates `net_required_value`. Do not attempt to write directly to `net_value`, as it is read-only and will be overwritten by the Core in the next cycle.
*   **Hardcoded Paths**: Check the File/Log nodes to ensure that paths to `.txt` files match your current file system.

---
*Developed for the OsoLogic ecosystem - Advancing industrial automation.*
---

## Run in the OSOLogic sandbox

The [sandbox](../../sandbox/) brings up Node-RED alongside MariaDB + osodb:

```bash
cd sandbox && docker compose up --build     # Node-RED at http://localhost:1880
```

To wire these flows to the sandbox DB:

1. **Manage Palette → Install** `node-red-contrib-mysql-config`.
2. **Import** `/reference/flows.json` (mounted from this folder).
3. Point the **MySQL config node** at host `db`, database `osodb`, port `3306` (user `osoapp`/`osoapp`).

> **Schema.** These flows target Diego's **`rtmirror` / `rtmirror_complete`** schema. The sandbox DB
> ([`sandbox/db/init.sql`](../../sandbox/db/init.sql)) now ships compatibility **views** mapping that
> schema onto the `tags` table — `rtmirror_complete` (read: `io_definition_id`, `user_label`,
> `net_value`, `io_type`, `units`, `purpose`, `visibility`) and an updatable `rtmirror` (write:
> `net_required_value` → the tag's set-point). So the flows run unchanged against the sandbox DB;
> the read/write cadence and NET-value scaling are the tuning that remains.
