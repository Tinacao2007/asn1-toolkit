/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_cache.cpp
**   $Version: 0.1
**   $Date:   2026-10-16
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   P2-3 content-hash cache (implementation).
**
** Specification: asn1cxx CLI (--emit-cache / --no-emit-cache).
** Design Spec:   asn1-toolkit/docs/todo.md#p2-3
**                 asn1-toolkit/docs/architecture.md
***************************************************************************/
#include "emit_cache.hpp"

#include <asn1/codegen/sha256.hpp>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

namespace asn1 {
namespace codegen {

namespace {

// One cache file = one emit unit. Unit names may include alphanumerics and
// `_` only (C++ identifiers, so the flat / split module ns idents already
// satisfy this). Anything else is escaped to `_0xXX` to keep the file
// parseable.
std::string escape_unit_name(const std::string& name) {
  std::string out;
  out.reserve(name.size());
  for (unsigned char c : name) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        c == '_' || c == '-') {
      out.push_back(static_cast<char>(c));
    } else {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "_0x%02x", c);
      out.append(buf);
    }
  }
  return out;
}

std::string unit_cache_filename(const std::string& unit_name) {
  return escape_unit_name(unit_name) + ".cache";
}

}  // namespace

ContentCache::ContentCache(const std::filesystem::path& emit_dir)
    : cache_dir_(emit_dir / ".asn1cxx-cache") {
  std::error_code ec;
  std::filesystem::create_directories(cache_dir_, ec);
  std::filesystem::directory_iterator it(cache_dir_, ec);
  if (ec) {
    return;
  }
  std::filesystem::directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      break;
    }
    const auto& entry = *it;
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::string filename = entry.path().filename().string();
    if (filename.size() < 7 || filename.substr(filename.size() - 6) != ".cache") {
      continue;
    }
    load_one(entry.path());
  }
}

void ContentCache::load_one(const std::filesystem::path& cache_file) {
  std::ifstream in(cache_file);
  if (!in) {
    return;
  }
  std::string line;
  if (!std::getline(in, line)) {
    return;
  }
  // Trim trailing CR (LF vs CRLF).
  while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
    line.pop_back();
  }
  if (line.empty()) {
    return;
  }
  CacheEntry e;
  if (!parse(line, e)) {
    return;
  }
  entries_[cache_file.stem().string()] = std::move(e);
}

std::string ContentCache::serialise(const CacheEntry& e) {
  std::string out;
  out.append(e.key);
  out.push_back(' ');
  out.append(std::to_string(e.outputs.size()));
  // Deterministic ordering makes a flush reproducible across runs.
  std::vector<std::string> names;
  names.reserve(e.outputs.size());
  for (const auto& [n, _] : e.outputs) {
    names.push_back(n);
  }
  std::sort(names.begin(), names.end());
  for (const std::string& n : names) {
    out.push_back(' ');
    out.append(n);
    out.push_back(':');
    out.append(e.outputs.at(n));
  }
  return out;
}

bool ContentCache::parse(const std::string& line, CacheEntry& out) {
  // Format: "<key> <count> <rel>:<hash> <rel>:<hash> ..."
  std::istringstream iss(line);
  if (!(iss >> out.key)) {
    return false;
  }
  std::size_t count = 0;
  if (!(iss >> count)) {
    return false;
  }
  for (std::size_t i = 0; i < count; ++i) {
    std::string item;
    if (!(iss >> item)) {
      return false;
    }
    const auto colon = item.find(':');
    if (colon == std::string::npos || colon == 0 || colon == item.size() - 1) {
      return false;
    }
    out.outputs[item.substr(0, colon)] = item.substr(colon + 1);
  }
  return true;
}

std::string ContentCache::compute_key(const std::string& unit_name,
                                      const std::vector<std::filesystem::path>& source_paths,
                                      const std::vector<std::string>& extra_inputs,
                                      const std::unordered_map<std::string, std::string>& source_hashes) const {
  // Compose a stable, well-bounded string and hash it once.
  std::string concat;
  concat.reserve(unit_name.size() + 256);
  concat.append("unit:");
  concat.append(unit_name);
  concat.push_back('|');
  // Deterministic source order: sort by absolute path string.
  std::vector<std::filesystem::path> sorted_paths(source_paths.begin(), source_paths.end());
  std::sort(sorted_paths.begin(), sorted_paths.end(),
            [](const std::filesystem::path& a, const std::filesystem::path& b) {
              return a.string() < b.string();
            });
  for (const auto& p : sorted_paths) {
    concat.append("src:");
    concat.append(p.string());
    concat.push_back('=');
    // Reuse a precomputed source hash when the caller has one — the same
    // .asn file otherwise gets SHA-256'd on every per-unit call (once for
    // `cache_source_fingerprint`, again here), which is wasteful for the
    // LPP/S1AP fixtures that ship ~hundreds of KB of source.
    auto it = source_hashes.find(p.string());
    std::string h;
    if (it != source_hashes.end()) {
      h = it->second;
    } else {
      h = sha256_file_hex(p);
    }
    if (h.empty()) {
      // Treat unreadable source as a unique sentinel so we never accidentally
      // hit a cache built from a different filesystem layout.
      concat.append("unreadable");
    } else {
      concat.append(h);
    }
    concat.push_back('|');
  }
  for (const std::string& s : extra_inputs) {
    concat.append("opt:");
    concat.append(s);
    concat.push_back('|');
  }
  return sha256_hex(concat);
}

const CacheEntry* ContentCache::lookup(const std::string& unit_name) const {
  auto it = entries_.find(unit_name);
  return it == entries_.end() ? nullptr : &it->second;
}

void ContentCache::store(const std::string& unit_name, CacheEntry entry) {
  entries_[unit_name] = std::move(entry);
}

void ContentCache::flush() {
  std::error_code ec;
  std::filesystem::create_directories(cache_dir_, ec);
  for (const auto& [name, e] : entries_) {
    const std::filesystem::path target = cache_dir_ / unit_cache_filename(name);
    // Atomic write: tmp + rename. On Windows rename-over-existing fails
    // without explicit remove; the WSL hosted path still goes through the
    // Linux VFS so std::filesystem::rename works fine here.
    const std::filesystem::path tmp = target.string() + ".tmp";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      if (!out) {
        continue;
      }
      out << serialise(e) << '\n';
      if (!out) {
        continue;
      }
    }
    std::filesystem::rename(tmp, target, ec);
    if (ec) {
      // Last-resort fallback: remove target then rename. (Cross-FS copies
      // and read-only targets can refuse overwrite.)
      std::filesystem::remove(target, ec);
      ec.clear();
      std::filesystem::rename(tmp, target, ec);
    }
  }
}

std::string ContentCache::hash_output_file(const std::filesystem::path& emit_dir,
                                          const std::filesystem::path& rel_path) {
  return sha256_file_hex(emit_dir / rel_path);
}

}  // namespace codegen
}  // namespace asn1
