/**
 * @file mariadb_adapter.hpp
 * @brief MariaDbAdapter — bridges the osodb cache to MariaDB (the source of truth).
 *
 * This is the seam that turns osodb into a *cache in front of MariaDB*. It wraps
 * the PRO core's `PLC_Database` (the C++ MariaDB layer) and reconciles it with
 * the hub, without changing the on-disk schema:
 *
 *   attach()        loads tag definitions from the DB (devices / model_io_definition /
 *                   module_io_config) and `define()`s them in the hub — the read-through
 *                   at startup. Subscribes for write-through of current values.
 *   poll_control()  reads externally-authored set-points — `get_all_required_values()`,
 *                   i.e. rows written by ANY SQL client via `UPDATE … SET required_value`
 *                   — and pushes them into the hub as `set_required()`. This is what makes
 *                   SQL a first-class control surface.
 *   flush()         write-back: sends buffered current values to `rtmirror` via
 *                   `batch_update_rtmirror_values()` so SELECTs / views stay live.
 *
 * The mapping from IoDefinition -> TagMeta (bit->Boolean, register_count-> UInt16/UInt32/
 * Float, hardware_access-> ReadOnly/ReadWrite, scale/offset/units/label, purpose) is
 * documented in adapters/README.md and applied in the .cpp.
 *
 * The implementation (.cpp) includes MariaDB and the PRO core headers; this header is
 * dependency-free (pimpl) so it can be included anywhere.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include <memory>
#include <string>

#include "adapter.hpp"

namespace osodb {

// FlushPolicy is defined in adapter.hpp (shared by every persistence adapter).

class MariaDbAdapter final : public IAdapter {
 public:
  explicit MariaDbAdapter(FlushPolicy policy = FlushPolicy::WriteBack);
  ~MariaDbAdapter() override;

  bool attach(IHub& hub) override;    ///< load definitions, subscribe for write-through
  size_t poll_control() override;     ///< pull required_value set-points into the hub
  size_t flush() override;            ///< write-back current values to rtmirror
  void detach() override;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;  // holds PLC_Database handle, buffers, subscription id
};

}  // namespace osodb
