/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_cache.hpp
**   $Version: 0.1
**   $Date:   2026-10-16
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   P2-3 content-hash cache (incremental emit). The cache lives in
**   `<emit_dir>/.asn1cxx-cache/`; one entry per emit unit. An entry
**   records (1) a content key derived from the unit's inputs (ASN.1
**   source files, codec mask, basename, namespace, emit-split flag),
**   and (2) the per-output sha256 of the files last written so the next
**   run can verify they were not hand-edited before trusting the cache.
**
** Specification: asn1cxx CLI (--emit-cache / --no-emit-cache).
** Design Spec:   asn1-toolkit/docs/todo.md#p2-3
**                 asn1-toolkit/docs/architecture.md
***************************************************************************/
#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace asn1 {
namespace codegen {

// Per-output-file fingerprint recorded alongside the cache key so the next
// run can verify the on-disk outputs are still exactly what we wrote.
struct CacheEntry {
  // sha256(content_key) — the unit's input fingerprint.
  std::string key;
  // Map of output filename (relative to the emit dir) -> sha256(bytes) of
  // the file that was on disk after the last successful emit. Missing
  // entries or stale hashes mean a downstream consumer rewrote the file
  // (e.g. manual edit) and the cache must be invalidated.
  std::unordered_map<std::string, std::string> outputs;
};

class ContentCache {
 public:
  // Construct an in-memory cache rooted at `<emit_dir>/.asn1cxx-cache`.
  // Existing entries are loaded eagerly; malformed or unreadable cache
  // files are silently treated as empty (cache is best-effort).
  explicit ContentCache(const std::filesystem::path& emit_dir);

  /// Build the canonical cache key for one unit. `unit_name` is a stable
  /// identifier (e.g. flat: "default"; split: the namespace ident of the
  /// module/unit, e.g. "LPP_PDU_Definitions" or "S1AP_IEs"). `source_hashes`
  /// maps each source path string -> sha256(content); pass an empty map to
  /// fall back to disk reads (slower). `extra_inputs` is a free-form hook
  /// (e.g. codec_mask, basename, namespace_name, emit_split flag) so the
  /// same cache directory can distinguish per-codec emits without
  /// collisions.
  std::string compute_key(const std::string& unit_name,
                         const std::vector<std::filesystem::path>& source_paths,
                         const std::vector<std::string>& extra_inputs,
                         const std::unordered_map<std::string, std::string>& source_hashes)
                         const;

  // Returns the cached entry for `unit_name`, or nullptr if absent.
  const CacheEntry* lookup(const std::string& unit_name) const;

  // Replace (or insert) the entry for `unit_name`. Not persisted until
  // flush() — that lets a batch of emit units finish before any disk I/O.
  void store(const std::string& unit_name, CacheEntry entry);

  // Atomically write the in-memory entries to disk: a tmp file is written
  // first, then renamed over the existing `<unit>.cache`. Missing entries
  // on disk are silently ignored (no error).
  void flush();

  // Read the sha256 of an output file relative to emit_dir.
  // Returns empty string if the file does not exist or cannot be read.
  static std::string hash_output_file(const std::filesystem::path& emit_dir,
                                      const std::filesystem::path& rel_path);

  const std::filesystem::path& cache_dir() const { return cache_dir_; }

 private:
  // Parse one <unit>.cache file. Bad/unreadable -> no entry.
  void load_one(const std::filesystem::path& cache_file);
  // Serialise one entry to a single line: "<key> <count> <rel>:<hash>...".
  static std::string serialise(const CacheEntry& e);
  // Parse a single line written by `serialise()`. Returns false on bad
  // format so the caller can skip it.
  static bool parse(const std::string& line, CacheEntry& out);

  std::filesystem::path cache_dir_;
  std::unordered_map<std::string, CacheEntry> entries_;
};

}  // namespace codegen
}  // namespace asn1
