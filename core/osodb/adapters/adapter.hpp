/**
 * @file adapter.hpp
 * @brief IAdapter — how a persistence/source-of-truth backend attaches to osodb.
 *
 * An adapter connects the in-memory cache (osodb) to something durable or remote
 * — MariaDB (the source of truth), Postgres/TSDB, an MCU simulator, a cloud
 * sync. It is the seam that makes osodb a *cache*: the hub stays fast and
 * standard-C++; adapters carry the writes out and the truth in.
 *
 * Lifecycle:
 *   attach()        load tag definitions into the hub (read-through at startup),
 *                   and subscribe for write-through of current values / set-points.
 *   poll_control()  pull externally-authored set-points (e.g. MariaDB
 *                   `required_value` written over SQL) into the hub.
 *   flush()         write-back any buffered current values to the store.
 *   detach()        unsubscribe and release resources.
 *
 * This header is dependency-free; concrete adapters include their client library
 * in their .cpp only.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include "../include/osodb.hpp"

namespace osodb {

/// Write-back policy for current values, shared by every persistence adapter.
enum class FlushPolicy {
  WriteThrough,  ///< flush on every change (simplest; more DB traffic)
  WriteBack      ///< buffer and flush on flush()/timer (fewer, batched writes)
};

class IAdapter {
 public:
  virtual ~IAdapter() = default;

  /// Load definitions into @p hub and subscribe for write-through. Returns true on success.
  virtual bool attach(IHub& hub) = 0;

  /// Pull externally-authored set-points from the store into the hub. Returns count applied.
  virtual size_t poll_control() = 0;

  /// Write back any buffered current values to the store. Returns count flushed.
  virtual size_t flush() = 0;

  /// Unsubscribe and release resources.
  virtual void detach() = 0;
};

}  // namespace osodb
