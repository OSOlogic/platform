/**
 * @file osodb.hpp
 * @brief osodb — the canonical real-time data hub of OSOLogic.
 *
 * OSOLogic is data-centric: hardware, SQL, OPC-UA, MQTT, REST and MCP all meet
 * at the data. MariaDB is the central hub and source of truth (and, at large
 * scale, the hub itself). osodb is the in-memory real-time *cache* in front of
 * it — think Redis in front of MySQL: the hot path (scan cycle, gateways) hits
 * memory in nanoseconds, while the database keeps the truth.
 *
 * osodb is an interface (`IHub`) so the runtime and gateways never depend on a
 * concrete store:
 *
 *   - MemoryHub: the in-process cache. Nanosecond access, dependency-free, and
 *     small enough to run on a baremetal MCU (RP2040/STM32/ESP32) where no
 *     database exists — it holds live state locally and syncs to MariaDB when
 *     connected. MariaDB sits behind it as the source-of-truth adapter
 *     (write-through / write-back / read-through); see adapters/.
 *   - At large scale MariaDB is the central hub directly (thousands of
 *     connections, auth, security, HA) with MemoryHub caches in front.
 *
 * Everything is keyed by the core's identity — (module_id, io_definition_id) —
 * so every interface (NodeId, MQTT topic, REST/MCP path) agrees on names.
 *
 * This header is dependency-free (standard library only). Anything that touches
 * an external system lives under adapters/.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

namespace osodb {

/// Identity of a single I/O point — matches the core's (module_id, io_definition_id).
struct TagKey {
  uint32_t module_id = 0;
  uint32_t io_definition_id = 0;

  bool operator==(const TagKey& other) const {
    return module_id == other.module_id && io_definition_id == other.io_definition_id;
  }
  bool operator<(const TagKey& other) const {
    return module_id != other.module_id ? module_id < other.module_id
                                        : io_definition_id < other.io_definition_id;
  }

  /// Deterministic, reversible string id — used verbatim as the OPC-UA NodeId
  /// (`ns=2;s=<module>.<iodef>`) and by MQTT/REST/MCP so every interface agrees.
  std::string to_string() const;
  static std::optional<TagKey> parse(const std::string& text);
};

/// Logical data type of a tag, derived once from its IoDefinition.
enum class DataType : uint8_t {
  Boolean,  ///< io_type=bit
  UInt16,   ///< register, register_count=1 (unsigned)
  Int16,    ///< register, register_count=1 (signed)
  UInt32,   ///< register, register_count=2 (unsigned)
  Int32,    ///< register, register_count=2 (signed)
  Float,    ///< register, register_count=2 (IEEE-754 single)
  Raw       ///< opaque 64-bit payload
};

/// Access level as seen by the outside world.
enum class Access : uint8_t { ReadOnly, ReadWrite };

/// Value quality, mirrored into OPC-UA StatusCode by the gateway.
enum class Quality : uint8_t { Good, BadNotConnected, BadStale, Uncertain };

/// Static description of a tag (from model_io_definition + module_io_config).
struct TagMeta {
  DataType type = DataType::Raw;
  Access access = Access::ReadOnly;
  std::string label;   ///< human-readable (user_label / default_io_label)
  std::string units;   ///< engineering units, if any
  double scale = 1.0;  ///< engineering = raw * scale + offset
  double offset = 0.0;
  uint8_t purpose = 1;  ///< 1 standard, 2 secure_state, 3 config
};

/// A point-in-time reading of a tag's current value.
struct Sample {
  uint64_t raw = 0;                    ///< raw bit pattern (as stored in hardware/rtmirror)
  Quality quality = Quality::BadStale;
  uint64_t ts_ns = 0;                  ///< source timestamp, ns since Unix epoch

  /// Engineering value using the tag's scale/offset and type.
  double engineering(const TagMeta& meta) const;
};

/// Connection state of a module, surfaced as StatusCode on its tags.
struct ModuleStatus {
  bool connected = false;
  uint64_t last_seen_ns = 0;
};

/// Change notification kind.
enum class Event : uint8_t { Current, Required, Status };
using Listener = std::function<void(Event, const TagKey&)>;

/// Wall-clock nanoseconds since the Unix epoch.
uint64_t now_ns();

/**
 * @brief The hub abstraction. Runtime and gateways depend on this, never on a
 *        concrete backend, so the store can be swapped by configuration.
 *        All methods are thread-safe.
 */
class IHub {
 public:
  virtual ~IHub() = default;

  // --- definition (configuration side) ---
  virtual void define(const TagKey& key, const TagMeta& meta) = 0;
  virtual bool is_defined(const TagKey& key) const = 0;
  virtual bool meta(const TagKey& key, TagMeta& out) const = 0;
  virtual std::vector<TagKey> tags() const = 0;

  // --- current value (scan / hardware -> hub) ---
  virtual void write_current(const TagKey& key, uint64_t raw, Quality q = Quality::Good,
                             uint64_t ts_ns = 0) = 0;
  virtual void write_current_batch(const std::vector<std::pair<TagKey, uint64_t>>& values,
                                   Quality q = Quality::Good) = 0;
  virtual bool read(const TagKey& key, Sample& out) const = 0;

  // --- required value / set-point (gateway/UI -> hub -> scan) ---
  virtual void set_required(const TagKey& key, uint64_t raw) = 0;
  virtual bool take_required(const TagKey& key, uint64_t& out) = 0;
  virtual std::vector<std::pair<TagKey, uint64_t>> drain_required() = 0;
  virtual bool has_required(const TagKey& key) const = 0;

  // --- module status ---
  virtual void set_module_status(uint32_t module_id, bool connected, uint64_t ts_ns = 0) = 0;
  virtual ModuleStatus module_status(uint32_t module_id) const = 0;

  // --- change notifications (gateways subscribe) ---
  virtual int subscribe(Listener l) = 0;
  virtual void unsubscribe(int id) = 0;

  virtual size_t size() const = 0;
};

/**
 * @brief In-process, dependency-free backend. Canonical at the edge and on MCUs.
 *        A single shared instance is the norm; independent instances are allowed.
 */
class MemoryHub final : public IHub {
 public:
  /// Process-wide shared instance.
  static MemoryHub& instance();

  MemoryHub() = default;
  MemoryHub(const MemoryHub&) = delete;
  MemoryHub& operator=(const MemoryHub&) = delete;

  void define(const TagKey& key, const TagMeta& meta) override;
  bool is_defined(const TagKey& key) const override;
  bool meta(const TagKey& key, TagMeta& out) const override;
  std::vector<TagKey> tags() const override;

  void write_current(const TagKey& key, uint64_t raw, Quality q = Quality::Good,
                     uint64_t ts_ns = 0) override;
  void write_current_batch(const std::vector<std::pair<TagKey, uint64_t>>& values,
                           Quality q = Quality::Good) override;
  bool read(const TagKey& key, Sample& out) const override;

  void set_required(const TagKey& key, uint64_t raw) override;
  bool take_required(const TagKey& key, uint64_t& out) override;
  std::vector<std::pair<TagKey, uint64_t>> drain_required() override;
  bool has_required(const TagKey& key) const override;

  void set_module_status(uint32_t module_id, bool connected, uint64_t ts_ns = 0) override;
  ModuleStatus module_status(uint32_t module_id) const override;

  int subscribe(Listener l) override;
  void unsubscribe(int id) override;

  size_t size() const override;

 private:
  struct Entry {
    TagMeta meta;
    Sample sample;
    bool has_required = false;
    uint64_t required_raw = 0;
  };

  void notify(Event ev, const TagKey& key) const;  // caller must NOT hold the data lock

  mutable std::shared_mutex mutex_;
  std::map<TagKey, Entry> tags_;
  std::map<uint32_t, ModuleStatus> modules_;

  mutable std::mutex listeners_mutex_;
  std::map<int, Listener> listeners_;
  int next_listener_ = 1;
};

}  // namespace osodb
