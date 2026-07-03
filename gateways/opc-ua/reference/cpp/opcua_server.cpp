/**
 * @file opcua_server.cpp
 * @brief Native C++ OPC-UA gateway for OSOLogic, built on open62541.
 *
 * Exposes the osodb data hub as an OPC-UA address space — the same contract as
 * the Python reference (reference/python/): one Object per module, one Variable
 * per I/O point, deterministic reversible NodeId `ns=2;s=<module>.<iodef>`,
 * DataType/AccessLevel derived from the tag meta, live reads from the hub and
 * guarded writes back as set-points.
 *
 * This is the Community Edition server (anonymous, read/write). OPC-UA security,
 * Historical Access, Alarms & Conditions and redundancy are Enterprise add-ons.
 *
 * Build: see CMakeLists.txt (needs open62541 and the osodb library).
 *
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <atomic>
#include <csignal>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "osodb.hpp"  // provided via target_include_directories in CMake

namespace {

std::atomic<bool> g_running{true};
void on_signal(int) { g_running = false; }

constexpr UA_UInt16 NS = 2;  // urn:osologic:platform

// Per-node context: which tag this variable maps to, plus its static meta.
struct NodeCtx {
  osodb::IHub* hub;
  osodb::TagKey key;
  osodb::TagMeta meta;
};

// Map osodb DataType -> open62541 type descriptor.
const UA_DataType& ua_type(osodb::DataType t) {
  switch (t) {
    case osodb::DataType::Boolean: return UA_TYPES[UA_TYPES_BOOLEAN];
    case osodb::DataType::Int16:   return UA_TYPES[UA_TYPES_INT16];
    case osodb::DataType::UInt16:  return UA_TYPES[UA_TYPES_UINT16];
    case osodb::DataType::Int32:   return UA_TYPES[UA_TYPES_INT32];
    case osodb::DataType::UInt32:  return UA_TYPES[UA_TYPES_UINT32];
    case osodb::DataType::Float:   return UA_TYPES[UA_TYPES_FLOAT];
    case osodb::DataType::Raw:
    default:                       return UA_TYPES[UA_TYPES_UINT64];
  }
}

// Build a UA_Variant holding the tag's current value in its natural type.
void raw_to_variant(const osodb::TagMeta& m, const osodb::Sample& s, UA_Variant* out) {
  switch (m.type) {
    case osodb::DataType::Boolean: { UA_Boolean v = s.raw != 0; UA_Variant_setScalarCopy(out, &v, &UA_TYPES[UA_TYPES_BOOLEAN]); break; }
    case osodb::DataType::Int16:   { UA_Int16  v = static_cast<UA_Int16>(s.raw);  UA_Variant_setScalarCopy(out, &v, &UA_TYPES[UA_TYPES_INT16]);  break; }
    case osodb::DataType::UInt16:  { UA_UInt16 v = static_cast<UA_UInt16>(s.raw); UA_Variant_setScalarCopy(out, &v, &UA_TYPES[UA_TYPES_UINT16]); break; }
    case osodb::DataType::Int32:   { UA_Int32  v = static_cast<UA_Int32>(s.raw);  UA_Variant_setScalarCopy(out, &v, &UA_TYPES[UA_TYPES_INT32]);  break; }
    case osodb::DataType::UInt32:  { UA_UInt32 v = static_cast<UA_UInt32>(s.raw); UA_Variant_setScalarCopy(out, &v, &UA_TYPES[UA_TYPES_UINT32]); break; }
    case osodb::DataType::Float:   { UA_UInt32 bits = static_cast<UA_UInt32>(s.raw); UA_Float v; std::memcpy(&v, &bits, sizeof(v)); UA_Variant_setScalarCopy(out, &v, &UA_TYPES[UA_TYPES_FLOAT]); break; }
    case osodb::DataType::Raw:
    default:                       { UA_UInt64 v = s.raw; UA_Variant_setScalarCopy(out, &v, &UA_TYPES[UA_TYPES_UINT64]); break; }
  }
}

// Extract a raw 64-bit set-point from an incoming UA_Variant.
bool variant_to_raw(const osodb::TagMeta& m, const UA_Variant* in, uint64_t* out) {
  if (!in->data) return false;
  switch (m.type) {
    case osodb::DataType::Boolean: *out = (*static_cast<UA_Boolean*>(in->data)) ? 1 : 0; return true;
    case osodb::DataType::Int16:   *out = static_cast<uint16_t>(*static_cast<UA_Int16*>(in->data));  return true;
    case osodb::DataType::UInt16:  *out = *static_cast<UA_UInt16*>(in->data); return true;
    case osodb::DataType::Int32:   *out = static_cast<uint32_t>(*static_cast<UA_Int32*>(in->data));  return true;
    case osodb::DataType::UInt32:  *out = *static_cast<UA_UInt32*>(in->data); return true;
    case osodb::DataType::Float:   { UA_Float v = *static_cast<UA_Float*>(in->data); uint32_t bits; std::memcpy(&bits, &v, sizeof(bits)); *out = bits; return true; }
    case osodb::DataType::Raw:
    default:                       *out = *static_cast<UA_UInt64*>(in->data); return true;
  }
}

UA_StatusCode read_tag(UA_Server*, const UA_NodeId*, void*, const UA_NodeId*, void* nodeContext,
                       UA_Boolean includeSourceTimeStamp, const UA_NumericRange*,
                       UA_DataValue* value) {
  auto* ctx = static_cast<NodeCtx*>(nodeContext);
  osodb::Sample s;
  if (!ctx->hub->read(ctx->key, s)) {
    value->hasStatus = true;
    value->status = UA_STATUSCODE_BADNODATA;
    return UA_STATUSCODE_GOOD;
  }
  raw_to_variant(ctx->meta, s, &value->value);
  value->hasValue = true;
  value->hasStatus = true;
  value->status = (s.quality == osodb::Quality::Good) ? UA_STATUSCODE_GOOD
                : (s.quality == osodb::Quality::BadNotConnected) ? UA_STATUSCODE_BADNOTCONNECTED
                                                                 : UA_STATUSCODE_UNCERTAIN;
  if (includeSourceTimeStamp) {
    value->hasSourceTimestamp = true;
    // osodb ts is ns since Unix epoch; UA_DateTime is 100ns ticks since 1601.
    value->sourceTimestamp = UA_DateTime_fromUnixTime(0) + static_cast<UA_DateTime>(s.ts_ns / 100);
  }
  return UA_STATUSCODE_GOOD;
}

UA_StatusCode write_tag(UA_Server*, const UA_NodeId*, void*, const UA_NodeId*, void* nodeContext,
                        const UA_NumericRange*, const UA_DataValue* value) {
  auto* ctx = static_cast<NodeCtx*>(nodeContext);
  if (ctx->meta.access != osodb::Access::ReadWrite) return UA_STATUSCODE_BADNOTWRITABLE;
  if (!value->hasValue) return UA_STATUSCODE_BADNODATA;
  uint64_t raw = 0;
  if (!variant_to_raw(ctx->meta, &value->value, &raw)) return UA_STATUSCODE_BADTYPEMISMATCH;
  ctx->hub->set_required(ctx->key, raw);  // guarded write -> set-point, applied by the scan
  return UA_STATUSCODE_GOOD;
}

void add_tag_node(UA_Server* server, std::vector<std::unique_ptr<NodeCtx>>& ctxs,
                  osodb::IHub& hub, const osodb::TagKey& key, const osodb::TagMeta& meta) {
  auto ctx = std::make_unique<NodeCtx>(NodeCtx{&hub, key, meta});
  std::string id = key.to_string();                 // "<module>.<iodef>"
  std::string label = meta.label.empty() ? id : meta.label;

  UA_DataSource src;
  src.read = read_tag;
  src.write = write_tag;

  UA_VariableAttributes attr = UA_VariableAttributes_default;
  attr.displayName = UA_LOCALIZEDTEXT_ALLOC("", label.c_str());
  attr.dataType = ua_type(meta.type).typeId;
  attr.accessLevel = UA_ACCESSLEVELMASK_READ |
                     (meta.access == osodb::Access::ReadWrite ? UA_ACCESSLEVELMASK_WRITE : 0);

  UA_NodeId node_id = UA_NODEID_STRING_ALLOC(NS, id.c_str());
  UA_QualifiedName bn = UA_QUALIFIEDNAME_ALLOC(NS, id.c_str());

  UA_Server_addDataSourceVariableNode(
      server, node_id, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES), bn,
      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, src, ctx.get(), nullptr);

  UA_NodeId_clear(&node_id);
  UA_QualifiedName_clear(&bn);
  UA_LocalizedText_clear(&attr.displayName);
  ctxs.push_back(std::move(ctx));
}

}  // namespace

int main() {
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);

  UA_Server* server = UA_Server_new();
  UA_ServerConfig_setMinimal(UA_Server_getConfig(server), 4840, nullptr);
  UA_Server_addNamespace(server, "urn:osologic:platform");  // expected to be index 2

  osodb::IHub& hub = osodb::MemoryHub::instance();

  // Publish every tag currently defined in the hub. (A production gateway would
  // also subscribe to hub definition changes and add/remove nodes live.)
  std::vector<std::unique_ptr<NodeCtx>> ctxs;
  for (const auto& key : hub.tags()) {
    osodb::TagMeta meta;
    if (hub.meta(key, meta)) add_tag_node(server, ctxs, hub, key, meta);
  }

  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
              "OSOLogic OPC-UA (C++/open62541) — %zu tags on opc.tcp://0.0.0.0:4840", ctxs.size());

  UA_StatusCode rc = UA_Server_run(server, reinterpret_cast<volatile const bool*>(&g_running));
  UA_Server_delete(server);
  return rc == UA_STATUSCODE_GOOD ? 0 : 1;
}
