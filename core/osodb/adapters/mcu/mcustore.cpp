/**
 * @file mcustore.cpp
 * @brief ISqlConn over the bare-metal McuStore — recognizes the osodb statement subset.
 * @copyright Copyright (c) 2026 Roig Borrell S.L. and Ibercomp S.L.
 * @license AGPL-3.0-or-later
 */
#include "mcustore.hpp"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace osodb {
namespace {

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

/// Value of `key = <number>` in @p low (lowercased SQL); false if absent.
bool find_num(const std::string& low, const char* key, long long& out) {
  size_t p = low.find(key);
  if (p == std::string::npos) return false;
  p += std::strlen(key);
  while (p < low.size() && (low[p] == ' ' || low[p] == '=')) ++p;
  size_t e = p;
  while (e < low.size() && (std::isdigit((unsigned char)low[e]) || low[e] == '-')) ++e;
  if (e == p) return false;
  out = std::strtoll(low.substr(p, e - p).c_str(), nullptr, 10);
  return true;
}

/// Split a comma list, honoring single-quoted strings (no nesting needed for osodb SQL).
std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  bool q = false;
  for (char c : s) {
    if (c == '\'') { q = !q; cur.push_back(c); }
    else if (c == ',' && !q) { out.push_back(cur); cur.clear(); }
    else cur.push_back(c);
  }
  out.push_back(cur);
  for (auto& t : out) {
    size_t a = t.find_first_not_of(" \t");
    size_t b = t.find_last_not_of(" \t");
    t = (a == std::string::npos) ? "" : t.substr(a, b - a + 1);
  }
  return out;
}

void assign(McuRow& r, const std::string& col, const std::string& val) {
  std::string v = val;
  if (col == "label") {
    if (v.size() >= 2 && v.front() == '\'') v = v.substr(1, v.size() - 2);
    std::strncpy(r.label, v.c_str(), sizeof(r.label) - 1);
  } else if (col == "module_id") r.module_id = (uint32_t)std::strtoul(v.c_str(), nullptr, 10);
  else if (col == "io_id") r.io_id = (uint32_t)std::strtoul(v.c_str(), nullptr, 10);
  else if (col == "data_type") r.data_type = (int32_t)std::strtol(v.c_str(), nullptr, 10);
  else if (col == "access") r.access = (int32_t)std::strtol(v.c_str(), nullptr, 10);
  else if (col == "scale") r.scale = std::strtod(v.c_str(), nullptr);
  else if (col == "offset") r.offset = std::strtod(v.c_str(), nullptr);
  else if (col == "value_raw") r.value_raw = std::strtoull(v.c_str(), nullptr, 10);
  else if (col == "required_raw") r.required_raw = std::strtoull(v.c_str(), nullptr, 10);
  else if (col == "has_required") r.has_required = (v != "0");
}

class McuBareConn final : public ISqlConn {
 public:
  bool open(const std::string&) override { return true; }
  void close() override {}
  std::string last_error() const override { return err_; }

  bool exec(const std::string& sql) override {
    const std::string low = lower(sql);
    if (low.find("create table") != std::string::npos) return true;  // schema is compiled-in
    if (low.find("insert into osodb_tags") != std::string::npos) return do_insert(sql);
    if (low.find("update osodb_tags") != std::string::npos) return do_update(low);
    err_ = "unsupported statement (MCU subset)";
    return false;
  }

  std::vector<SqlRow> query(const std::string& sql) override {
    const std::string low = lower(sql);
    std::vector<SqlRow> out;
    if (low.find("where has_required = 1") != std::string::npos) {
      for (auto& r : store_.rows)
        if (r.used && r.has_required)
          out.push_back({std::to_string(r.module_id), std::to_string(r.io_id),
                         std::to_string(r.required_raw)});
      return out;
    }
    long long m = 0, io = 0;
    const bool has_where = find_num(low, "module_id =", m) && find_num(low, "io_id =", io);
    if (low.find("select value_raw") != std::string::npos && has_where) {
      if (auto* r = store_.find((uint32_t)m, (uint32_t)io)) out.push_back({std::to_string(r->value_raw)});
      return out;
    }
    if (low.find("select has_required") != std::string::npos && has_where) {
      if (auto* r = store_.find((uint32_t)m, (uint32_t)io)) out.push_back({r->has_required ? "1" : "0"});
      return out;
    }
    // Definitions: module_id, io_id, label, data_type, access, scale, offset, units, purpose.
    for (auto& r : store_.rows)
      if (r.used)
        out.push_back({std::to_string(r.module_id), std::to_string(r.io_id), r.label,
                       std::to_string(r.data_type), std::to_string(r.access),
                       std::to_string(r.scale), std::to_string(r.offset), "", "1"});
    return out;
  }

 private:
  bool do_insert(const std::string& sql) {
    const size_t co = sql.find('('), cc = sql.find(')');
    const size_t vo = sql.find('(', cc), vc = sql.rfind(')');
    if (co == std::string::npos || vo == std::string::npos || vc <= vo) { err_ = "bad INSERT"; return false; }
    auto cols = split_csv(sql.substr(co + 1, cc - co - 1));
    auto vals = split_csv(sql.substr(vo + 1, vc - vo - 1));
    if (cols.size() != vals.size()) { err_ = "INSERT arity"; return false; }
    McuRow* r = store_.alloc();
    if (!r) { err_ = "store full"; return false; }
    r->used = true;
    for (size_t i = 0; i < cols.size(); ++i) assign(*r, lower(cols[i]), vals[i]);
    return true;
  }
  bool do_update(const std::string& low) {
    if (low.find("has_required = 0") != std::string::npos) {
      for (auto& r : store_.rows) r.has_required = false;
      return true;
    }
    long long val = 0, q = 0, ts = 0, m = 0, io = 0;
    if (find_num(low, "value_raw =", val) && find_num(low, "module_id =", m) &&
        find_num(low, "io_id =", io)) {
      if (auto* r = store_.find((uint32_t)m, (uint32_t)io)) {
        r->value_raw = (uint64_t)val;
        if (find_num(low, "quality =", q)) r->quality = (int32_t)q;
        if (find_num(low, "ts_ns =", ts)) r->ts_ns = (uint64_t)ts;
        return true;
      }
    }
    err_ = "unsupported UPDATE (MCU subset)";
    return false;
  }

  McuStore<256> store_;
  std::string err_;
};

}  // namespace

std::unique_ptr<ISqlConn> make_mcu_bare_conn(int /*capacity*/) {
  return std::make_unique<McuBareConn>();
}

}  // namespace osodb
