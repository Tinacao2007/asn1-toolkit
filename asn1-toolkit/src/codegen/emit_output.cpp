/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_output.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Implementation source for codegen / emit_output.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "emit_output.hpp"
#include "emit_internal.hpp"
#include "generator_revision.hpp"
#include <asn1/codegen/c_types.hpp>
#include <asn1/codegen/sha256.hpp>
#include <fstream>
#include <map>
#include <set>
#include <iomanip>

namespace asn1 { namespace codegen {
namespace fs = std::filesystem;
std::string portable_output_key(std::string name) {
  for (char& c : name) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
  return name;
}
bool portable_output_name(const std::string& name) {
  if (name.empty() || name.size() > 120) return false;
  for (unsigned char c : name)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_')) return false;
  const auto key = portable_output_key(name);
  if (key == "con" || key == "prn" || key == "aux" || key == "nul") return false;
  if (key.size() == 4 && (key.substr(0,3) == "com" || key.substr(0,3) == "lpt") &&
      key[3] >= '0' && key[3] <= '9') return false;
  return true;
}
bool valid_c_prefix(const std::string& prefix) {
  const auto alpha=[](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z');};
  if(prefix.empty() || !alpha(prefix.front()) || prefix.back()=='_' || prefix.find("__")!=std::string::npos) return false;
  for(char c:prefix) if(!alpha(c) && !(c>='0'&&c<='9') && c!='_') return false;
  return true;
}
bool validate_emit_options(const EmitOptions& opt, Diagnostics& diag) {
  if (opt.language == TargetLanguage::C) {
    if (!valid_c_prefix(opt.c_prefix.empty()?"asn1_gen":opt.c_prefix)) {
      diag.error({},"C types: invalid non-reserved symbol prefix");return false;
    }
    const auto c_mask=static_cast<unsigned>(opt.codec);
    const auto per_mask=static_cast<unsigned>(CodecKind::Uper|CodecKind::Aper);
    if ((opt.c_types_only && opt.codec != CodecKind::None) ||
        (!opt.c_types_only && opt.codec == CodecKind::None) ||
        (c_mask & ~per_mask) != 0) {
      diag.error({}, "C output supports --types-only without a codec, or the currently implemented UPER/APER C codec subset");
      return false;
    }
    if (opt.emit_inline && (opt.emit_split || opt.split_max_types)) {
      diag.error({}, "C inline and module/source splitting conflict"); return false;
    }
    if (opt.namespace_name != "asn1_gen") {
      diag.error({}, "C output does not accept a C++ namespace"); return false;
    }
  } else if (!opt.c_prefix.empty() || opt.c_types_only || opt.split_max_types) {
    diag.error({}, "c-prefix, types-only and split-max-types require --lang c"); return false;
  }
  if (!portable_output_name(opt.basename)) {
    diag.error({}, "basename must be a portable filename stem (letters, digits, underscore; no Windows device names)");
    return false;
  }
  const auto& ns = opt.namespace_name;
  if (!portable_output_name(ns) || (ns[0] >= '0' && ns[0] <= '9') ||
      ns[0] == '_' || ns.find("__") != std::string::npos || is_cpp_keyword(ns)) {
    diag.error({}, "namespace must be one non-reserved C++17 identifier");
    return false;
  }
  for (const auto& path : opt.source_paths) {
    if (path.generic_string().find_first_of("\r\n") != std::string::npos) {
      diag.error({}, "source dependency path cannot contain a line break"); return false;
    }
  }
  return diag.ok();
}
void emit_generated_to_dir(const ir::Model& model, const EmitOptions& opt,
                           Diagnostics& diag, const fs::path& dir) try {
  if (!validate_emit_options(opt, diag)) return;
  if (opt.language == TargetLanguage::C) {
    emit_directory(model,opt,diag,dir,opt.emit_split); return;
  }
  CppGenerator generator;
  if (opt.emit_split) generator.emit_split(model, opt, diag, dir);
  else generator.emit_to_dir(model, opt, diag, dir);
} catch(const fs::filesystem_error& ex) {
  diag.error({},std::string("code generation filesystem failure: ")+ex.what());
} catch(const std::length_error& ex) {
  diag.error({},std::string("code generation capacity exceeded: ")+ex.what());
} catch(const std::ios_base::failure& ex) {
  diag.error({},std::string("code generation stream failure: ")+ex.what());
}
namespace {
const char* manifest_name = ".asn1cxx-manifest";
struct Workspace {
  fs::path lock;
  ~Workspace() { if (!lock.empty()) { std::error_code ec; fs::remove_all(lock, ec); } }
};
bool artifact_name(const fs::path& path) {
  if(path.filename()!=path) return false;
  if(path=="generated-sources.json" || path=="generated-sources.cmake") return true;
  const auto ext=path.extension();
  return (ext==".hpp" || ext==".cpp" || ext==".h" || ext==".c") && portable_output_name(path.stem().string());
}
bool read_owned(const fs::path& dir, std::map<std::string,std::string>& owned) {
  std::ifstream in(dir / manifest_name);
  if (!in) { std::error_code ec; return !fs::exists(dir / manifest_name,ec) && !ec; }
  std::string magic;
  if (!std::getline(in, magic) || magic != "asn1cxx-artifacts-v1") return false;
  std::string name, hash;
  while (in >> name) {
    if (name == "dependencies") { std::string line; std::getline(in,line); continue; }
    if (!(in >> hash)) return false;
    const fs::path path(name);
    if (!artifact_name(path) || (hash.size() != 64 || hash.find_first_not_of("0123456789abcdef") != std::string::npos) || !owned.emplace(name,hash).second)
      return false;
  }
  return in.eof();
}
bool generated_legacy(const fs::path& path) {
  constexpr char marker[] = "// Generated by asn1cxx";
  char prefix[sizeof(marker)-1];
  std::ifstream in(path, std::ios::binary);
  in.read(prefix, sizeof(prefix));
  return in && std::equal(std::begin(prefix),std::end(prefix),marker);
}
}
void emit_directory(const ir::Model& model, EmitOptions opt, Diagnostics& diag,
                    const fs::path& dir, bool split) {
  if (!validate_emit_options(opt, diag)) return;
  std::error_code ec;
  fs::create_directories(dir,ec);
  if (ec) { diag.error({}, "cannot create output directory: " + ec.message()); return; }
  const auto lock = dir / ".asn1cxx-emitting";
  if (!fs::create_directory(lock,ec)) {
    diag.error({}, "output directory busy or unavailable (.asn1cxx-emitting); concurrent generation is rejected"); return;
  }
  Workspace workspace{lock};
  const auto stage = lock / "stage";
  const auto backup = lock / "backup";
  fs::create_directory(stage,ec);
  if (!ec) fs::create_directory(backup,ec);
  if (ec) { diag.error({}, "cannot create generation staging area: " + ec.message()); return; }
  const auto manifest_status = fs::symlink_status(dir/manifest_name,ec);
  if (ec == std::errc::no_such_file_or_directory) ec.clear();
  if (ec || (fs::exists(manifest_status) && !fs::is_regular_file(manifest_status))) {
    diag.error({}, "artifact manifest must be a regular file"); return;
  }
  std::map<std::string,std::string> owned;
  if (!read_owned(dir,owned)) { diag.error({}, "invalid artifact manifest; refusing output changes"); return; }
  // Reuse only regular files. Never follow a symlink out of the output directory.
  fs::directory_iterator it(dir,ec), end;
  for (; !ec && it != end; it.increment(ec)) {
    const auto path = it->path();
    const auto status = it->symlink_status(ec);
    if (ec) break;
    if (fs::is_regular_file(status) && artifact_name(path.filename()) &&
        (owned.count(path.filename().string()) || generated_legacy(path)))
      fs::copy_file(path,stage/path.filename(),fs::copy_options::overwrite_existing,ec);
  }
  if (ec) { diag.error({}, "cannot stage previous output: " + ec.message()); return; }
  const auto cache = dir / ".asn1cxx-cache";
  if (!ec && fs::is_directory(fs::symlink_status(cache,ec))) {
    fs::create_directory(stage/".asn1cxx-cache",ec);
    fs::directory_iterator ci(cache,ec);
    for (; !ec && ci != end; ci.increment(ec)) {
      if (fs::is_regular_file(ci->symlink_status(ec)) && ci->path().extension() == ".cache")
        fs::copy_file(ci->path(),stage/".asn1cxx-cache"/ci->path().filename(),fs::copy_options::overwrite_existing,ec);
    }
  } else if (ec == std::errc::no_such_file_or_directory) ec.clear();
  ec.clear(); // Cache read/copy failure is best-effort; incomplete entries miss verification.
  // Explicit registration avoids treating unrelated staged headers as generated outputs.
  std::set<std::string> outputs;
  opt.emit_dir = stage;
  opt.emit_split = split;
  if (opt.language == TargetLanguage::C) {
    const auto files=render_c_type_files(model,opt,diag);
    if(!diag.ok()) return;
    bool cacheable=opt.emit_cache && !opt.source_paths.empty();
    std::unordered_map<std::string,std::string> source_hashes;
    std::string source_order;
    for(const auto& dependency:opt.source_paths){
      auto hash=ContentCache::hash_output_file(dependency.parent_path(),dependency.filename());
      if(hash.empty()) cacheable=false;
      source_hashes[dependency.string()]=hash;
      source_order+=std::to_string(dependency.string().size())+":"+dependency.string();
    }
    ContentCache c_cache(stage);
    const std::vector<std::string> extras={"language=c","types_only="+std::to_string(opt.c_types_only),"codec="+std::to_string(static_cast<unsigned>(opt.codec)),
      "prefix="+opt.c_prefix,"basename="+opt.basename,"inline="+std::to_string(opt.emit_inline),
      "split="+std::to_string(split),"max_types="+std::to_string(opt.split_max_types),
      "order="+sha256_hex(source_order),"format=c-artifacts-2","generator=" ASN1_GENERATOR_REVISION};
    std::size_t hits=0;
    for(const auto& file:files){
      if(!artifact_name(file.name)){diag.error({},"non-portable C output name: "+file.name);return;}
      outputs.insert(file.name);
      const auto key=c_cache.compute_key(file.name,opt.source_paths,extras,source_hashes);
      const auto expected=sha256_hex(file.contents);
      const auto unit="c_"+sha256_hex(file.name);
      const auto* entry=cacheable?c_cache.lookup(unit):nullptr;
      const bool hit=entry && entry->key==key && entry->outputs.size()==1 && entry->outputs.count(file.name) &&
        entry->outputs.at(file.name)==expected && ContentCache::hash_output_file(stage,file.name)==expected;
      if(hit) ++hits;
      else {
        std::ofstream out(stage/file.name,std::ios::binary);out << file.contents;out.close();
        if(!out){diag.error({},"cannot write C staged output: "+file.name);return;}
      }
      if(cacheable)c_cache.store(unit,CacheEntry{key,{{file.name,expected}}});
    }
    if(cacheable)c_cache.flush();
    diag.note({},"emit cache: "+std::to_string(hits)+"/"+std::to_string(files.size())+" units hit (C verified artifact reuse; rendering still planned)");
  } else {
    Emitter emitter(model,opt,diag);
    emitter.output_files_ = &outputs;
    if (split) emitter.run_split(stage); else emitter.run_to_dir(stage);
  }
  if (!diag.ok()) return;
  std::map<std::string,std::string> hashes;
  for (const auto& name : outputs) {
    const auto target = dir/name;
    const auto status = fs::symlink_status(target,ec);
    if (ec == std::errc::no_such_file_or_directory) ec.clear();
    if (ec || (fs::exists(status) && (!fs::is_regular_file(status) ||
        (!owned.count(name) && !generated_legacy(target))))) {
      diag.error({}, "refusing to replace unowned or non-regular output: " + name); return;
    }
    hashes[name] = ContentCache::hash_output_file(stage,name);
    if (hashes[name].empty()) { diag.error({}, "cannot hash staged output: " + name); return; }
  }
  std::ofstream manifest(stage/manifest_name,std::ios::binary);
  manifest << "asn1cxx-artifacts-v1\n";
  for (const auto& item : hashes) manifest << item.first << ' ' << item.second << '\n';
  for (const auto& dependency : opt.source_paths)
    manifest << "dependencies " << std::quoted(dependency.generic_string()) << ' '
             << ContentCache::hash_output_file(dependency.parent_path(),dependency.filename()) << '\n';
  manifest.close();
  if (!manifest) { diag.error({}, "cannot write artifact manifest"); return; }
  std::vector<std::string> changes;
  for (const auto& item : hashes)
    if (ContentCache::hash_output_file(dir,item.first) != item.second) changes.push_back(item.first);
  for (const auto& item : owned)
    if (!hashes.count(item.first) && ContentCache::hash_output_file(dir,item.first) == item.second &&
        fs::is_regular_file(fs::symlink_status(dir/item.first,ec))) changes.push_back(item.first);
  changes.push_back(manifest_name); // completion marker is always committed last
  std::vector<const std::string*> saved, installed;
  saved.reserve(changes.size()); installed.reserve(changes.size());
  auto rollback = [&]() {
    bool restored = true;
    for (auto i = installed.rbegin(); i != installed.rend(); ++i) { fs::remove(dir/ **i,ec); if(ec) restored=false; }
    for (auto i = saved.rbegin(); i != saved.rend(); ++i) { fs::rename(backup/ **i,dir/ **i,ec); if(ec) restored=false; }
    if (!restored) { workspace.lock.clear(); diag.error({}, "publish rollback failed; recover backups from " + backup.string()); }
  };
  try {
  for (const auto& name : changes) {
    const auto status = fs::symlink_status(dir/name,ec);
    if (ec == std::errc::no_such_file_or_directory) ec.clear();
    if (ec) break;
    if (fs::exists(status)) {
      if (!fs::is_regular_file(status)) { ec = std::make_error_code(std::errc::invalid_argument); break; }
      fs::rename(dir/name,backup/name,ec);
      if (ec) break;
      saved.push_back(&name);
    }
    if (hashes.count(name) || name == manifest_name) {
      fs::rename(stage/name,dir/name,ec);
      if (ec) break;
      installed.push_back(&name);
    }
  }
  } catch (...) {
    rollback();
    throw;
  }
  if (ec) {
    const auto failure = ec;
    rollback();
    diag.error({}, "cannot publish generated artifacts: " + failure.message()); return;
  }
  // Cache persistence is best-effort and cannot certify completion; manifest does that.
  const auto cache_status = fs::symlink_status(cache,ec);
  if (ec == std::errc::no_such_file_or_directory) ec.clear();
  if (opt.emit_cache && !ec && (!fs::exists(cache_status) || fs::is_directory(cache_status))) {
    fs::create_directories(cache,ec);
    fs::directory_iterator ci(stage/".asn1cxx-cache",ec);
    for (; !ec && ci != end; ci.increment(ec))
      if (fs::is_regular_file(ci->symlink_status(ec))) {
        const auto destination = cache/ci->path().filename();
        const auto status = fs::symlink_status(destination,ec);
        if (ec == std::errc::no_such_file_or_directory) ec.clear();
        if (!ec && (!fs::exists(status) || fs::is_regular_file(status)))
          fs::copy_file(ci->path(),destination,fs::copy_options::overwrite_existing,ec);
      }
  }
}
}}
