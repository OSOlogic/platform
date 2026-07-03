/**
 * @file osodb.cpp
 * @brief Implementation of the MemoryHub backend and shared osodb helpers.
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include "../include/osodb.hpp"

#include <chrono>
#include <cstring>

namespace osodb {

// --- TagKey -----------------------------------------------------------------
std::string TagKey::to_string() const {
  return std::to_string(module_id) + "." + std::to_string(io_definition_id);
}

std::optional<TagKey> TagKey::parse(const std::string& text) {
  auto dot = text.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= text.size()) return std::nullopt;
  try {
    size_t p1 = 0, p2 = 0;
    unsigned long mod = std::stoul(text.substr(0, dot), &p1);
    unsigned long def = std::stoul(text.substr(dot + 1), &p2);
    if (p1 != dot || p2 != text.size() - dot - 1) return std::nullopt;
    TagKey key;
    key.module_id = static_cast<uint32_t>(mod);
    key.io_definition_id = static_cast<uint32_t>(def);
    return key;
  } catch (...) {
    return std::nullopt;
  }
}

// --- Sample -----------------------------------------------------------------
double Sample::engineering(const TagMeta& meta) const {
  double value;
  switch (meta.type) {
    case DataType::Boolean:
      value = (raw != 0) ? 1.0 : 0.0;
      break;
    case DataType::Int16:
      value = static_cast<double>(static_cast<int16_t>(raw & 0xFFFF));
      break;
    case DataType::UInt16:
      value = static_cast<double>(static_cast<uint16_t>(raw & 0xFFFF));
      break;
    case DataType::Int32:
      value = static_cast<double>(static_cast<int32_t>(raw & 0xFFFFFFFF));
      break;
    case DataType::UInt32:
      value = static_cast<double>(static_cast<uint32_t>(raw & 0xFFFFFFFF));
      break;
    case DataType::Float: {
      uint32_t bits = static_cast<uint32_t>(raw & 0xFFFFFFFF);
      float single;
      std::memcpy(&single, &bits, sizeof(single));
      value = static_cast<double>(single);
      break;
    }
    case DataType::Raw:
    default:
      value = static_cast<double>(raw);
      break;
  }
  return value * meta.scale + meta.offset;
}

// --- time -------------------------------------------------------------------
uint64_t now_ns() {
  using namespace std::chrono;
  return static_cast<uint64_t>(
      duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
}

// --- MemoryHub --------------------------------------------------------------
MemoryHub& MemoryHub::instance() {
  static MemoryHub hub;
  return hub;
}

void MemoryHub::define(const TagKey& key, const TagMeta& meta) {
  std::unique_lock<std::shared_mutex> lk(mutex_);
  tags_[key].meta = meta;  // creates the Entry if absent, preserving any value
}

bool MemoryHub::is_defined(const TagKey& key) const {
  std::shared_lock<std::shared_mutex> lk(mutex_);
  return tags_.find(key) != tags_.end();
}

bool MemoryHub::meta(const TagKey& key, TagMeta& out) const {
  std::shared_lock<std::shared_mutex> lk(mutex_);
  auto it = tags_.find(key);
  if (it == tags_.end()) return false;
  out = it->second.meta;
  return true;
}

std::vector<TagKey> MemoryHub::tags() const {
  std::shared_lock<std::shared_mutex> lk(mutex_);
  std::vector<TagKey> out;
  out.reserve(tags_.size());
  for (const auto& kv : tags_) out.push_back(kv.first);
  return out;
}

void MemoryHub::write_current(const TagKey& key, uint64_t raw, Quality q, uint64_t ts_ns) {
  {
    std::unique_lock<std::shared_mutex> lk(mutex_);
    Entry& e = tags_[key];
    e.sample.raw = raw;
    e.sample.quality = q;
    e.sample.ts_ns = ts_ns ? ts_ns : now_ns();
  }
  notify(Event::Current, key);
}

void MemoryHub::write_current_batch(const std::vector<std::pair<TagKey, uint64_t>>& values,
                                    Quality q) {
  uint64_t ts = now_ns();
  {
    std::unique_lock<std::shared_mutex> lk(mutex_);
    for (const auto& kv : values) {
      Entry& e = tags_[kv.first];
      e.sample.raw = kv.second;
      e.sample.quality = q;
      e.sample.ts_ns = ts;
    }
  }
  for (const auto& kv : values) notify(Event::Current, kv.first);
}

bool MemoryHub::read(const TagKey& key, Sample& out) const {
  std::shared_lock<std::shared_mutex> lk(mutex_);
  auto it = tags_.find(key);
  if (it == tags_.end()) return false;
  out = it->second.sample;
  return true;
}

void MemoryHub::set_required(const TagKey& key, uint64_t raw) {
  {
    std::unique_lock<std::shared_mutex> lk(mutex_);
    Entry& e = tags_[key];
    e.has_required = true;
    e.required_raw = raw;
  }
  notify(Event::Required, key);
}

bool MemoryHub::take_required(const TagKey& key, uint64_t& out) {
  std::unique_lock<std::shared_mutex> lk(mutex_);
  auto it = tags_.find(key);
  if (it == tags_.end() || !it->second.has_required) return false;
  out = it->second.required_raw;
  it->second.has_required = false;
  return true;
}

std::vector<std::pair<TagKey, uint64_t>> MemoryHub::drain_required() {
  std::unique_lock<std::shared_mutex> lk(mutex_);
  std::vector<std::pair<TagKey, uint64_t>> out;
  for (auto& kv : tags_) {
    if (kv.second.has_required) {
      out.emplace_back(kv.first, kv.second.required_raw);
      kv.second.has_required = false;
    }
  }
  return out;
}

bool MemoryHub::has_required(const TagKey& key) const {
  std::shared_lock<std::shared_mutex> lk(mutex_);
  auto it = tags_.find(key);
  return it != tags_.end() && it->second.has_required;
}

void MemoryHub::set_module_status(uint32_t module_id, bool connected, uint64_t ts_ns) {
  {
    std::unique_lock<std::shared_mutex> lk(mutex_);
    ModuleStatus& s = modules_[module_id];
    s.connected = connected;
    s.last_seen_ns = ts_ns ? ts_ns : now_ns();
  }
  notify(Event::Status, TagKey{module_id, 0});
}

ModuleStatus MemoryHub::module_status(uint32_t module_id) const {
  std::shared_lock<std::shared_mutex> lk(mutex_);
  auto it = modules_.find(module_id);
  return it == modules_.end() ? ModuleStatus{} : it->second;
}

int MemoryHub::subscribe(Listener l) {
  std::lock_guard<std::mutex> lk(listeners_mutex_);
  int id = next_listener_++;
  listeners_[id] = std::move(l);
  return id;
}

void MemoryHub::unsubscribe(int id) {
  std::lock_guard<std::mutex> lk(listeners_mutex_);
  listeners_.erase(id);
}

void MemoryHub::notify(Event ev, const TagKey& key) const {
  // Copy the listeners so callbacks may (un)subscribe without deadlocking and so
  // we never run user code while holding the data lock.
  std::vector<Listener> copy;
  {
    std::lock_guard<std::mutex> lk(listeners_mutex_);
    copy.reserve(listeners_.size());
    for (const auto& kv : listeners_) copy.push_back(kv.second);
  }
  for (const auto& l : copy) l(ev, key);
}

size_t MemoryHub::size() const {
  std::shared_lock<std::shared_mutex> lk(mutex_);
  return tags_.size();
}

}  // namespace osodb
