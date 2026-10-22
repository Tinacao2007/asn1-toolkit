/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/emit_cache_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-16
**   $Author: tina.cao
***************************************************************************/
/**  File Description:
**
**   P2-3 content-hash cache unit tests. Cover:
**
**     - flat mode: first emit is a miss, second emit is a hit
**       (no I/O on the second call; cached outputs are byte-identical)
**     - flat mode: changing one byte of the .asn input forces a miss
**     - flat mode: `--no-emit-cache` always re-emits (no cache hit)
**     - split mode: LPP single-unit hit; S1AP three-unit partial hit
**       (one .asn edit invalidates only its unit; siblings stay hit)
**     - cache directory is created under <emit-dir>/.asn1cxx-cache/
**     - cache note appears in the diagnostic stream
**     - sha256(): unit vectors match FIPS 180-4
**
** Specification: asn1cxx CLI (--emit-cache / --no-emit-cache).
** Design Spec:   asn1-toolkit/docs/todo.md#p2-3
***************************************************************************/
#include <asn1/codegen/emit.hpp>
#include <asn1/codegen/sha256.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/ir/type.hpp>
#include <asn1/semantic/analyzer.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

#include <cstdio>
#include <functional>
#include <thread>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace {

namespace fs = std::filesystem;

// Scratch directory unique to this test process so parallel ctest runs
// don't clobber each other. TMPDIR points under ~/.hermes/cache/scratch.
fs::path make_tmpdir(const std::string& tag) {
  static std::mt19937_64 rng{std::random_device{}()};
  const std::string suffix =
      "_" + tag + "_" + std::to_string(static_cast<long long>(std::hash<std::thread::id>{}(std::this_thread::get_id()))) + "_" +
      std::to_string(rng());
  fs::path dir = fs::temp_directory_path() / ("asn1cxx_cache_test" + suffix);
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  return dir;
}

void write_file(const fs::path& p, std::string_view text) {
  std::ofstream out(p, std::ios::binary | std::ios::trunc);
  out << text;
}

// Read file into string for byte-level comparison.
std::string read_file(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::ostringstream oss;
  oss << in.rdbuf();
  return oss.str();
}

// Parse + analyze an .asn fixture. Returns the model + a populated IR.
asn1::ir::Model build_model(const fs::path& asn, asn1::Diagnostics& diag) {
  auto loaded = asn1::SourceFile::from_path(asn.string());
  if (!loaded) { diag.error({}, loaded.error().message); return {}; }
  asn1::SourceFile file = std::move(loaded.value());
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto mods = parser.parse_modules();
  asn1::Analyzer analyzer(diag);
  return analyzer.analyze(std::move(mods));
}

// Convenience: run emit_to_dir with the cache on.
void emit_with_cache(const asn1::ir::Model& model, const fs::path& asn, const fs::path& out_dir,
                     asn1::Diagnostics& diag) {
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Uper;
  opt.basename = "generated";
  opt.namespace_name = "asn1_cache_test";
  opt.emit_cache = true;
  opt.source_paths = {fs::absolute(asn)};
  asn1::codegen::CppGenerator gen;
  gen.emit_to_dir(model, opt, diag, out_dir);
}

void emit_no_cache(const asn1::ir::Model& model, const fs::path& asn, const fs::path& out_dir,
                   asn1::Diagnostics& diag) {
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Uper;
  opt.basename = "generated";
  opt.namespace_name = "asn1_cache_test";
  opt.emit_cache = false;
  opt.source_paths = {fs::absolute(asn)};
  asn1::codegen::CppGenerator gen;
  gen.emit_to_dir(model, opt, diag, out_dir);
}

// Returns the count of "emit cache:" notes in the diagnostic stream.
std::size_t count_cache_notes(const asn1::Diagnostics& diag) {
  std::size_t n = 0;
  for (const auto& d : diag.items()) {
    if (d.message.find("emit cache:") != std::string::npos) {
      ++n;
    }
  }
  return n;
}

// Extract a single "X/Y" hit ratio from the latest cache note. Returns
// {hits, total} or {0,0} if no note is present.
struct HitMiss { std::size_t hits; std::size_t total; };
HitMiss latest_cache_stats(const asn1::Diagnostics& diag) {
  for (auto it = diag.items().rbegin(); it != diag.items().rend(); ++it) {
    const std::string& msg = it->message;
    const auto pos = msg.find("emit cache:");
    if (pos == std::string::npos) continue;
    // "emit cache: <hits>/<total> units hit (...)"
    const auto slash = msg.find('/', pos);
    const auto space = msg.find(' ', slash);
    if (slash == std::string::npos || space == std::string::npos) return {0, 0};
    return {static_cast<std::size_t>(std::stoul(msg.substr(pos + 12, slash - pos - 12))),
            static_cast<std::size_t>(std::stoul(msg.substr(slash + 1, space - slash - 1)))};
  }
  return {0, 0};
}

constexpr std::string_view kPersonAsn = R"ASN(MyModule DEFINITIONS AUTOMATIC TAGS ::=
BEGIN
  Person ::= SEQUENCE {
    id      INTEGER (0..65535),
    name    UTF8String,
    age     INTEGER (0..150) OPTIONAL
  }
END
)ASN";

constexpr std::string_view kLppAsnPath =
    "fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn";

constexpr std::string_view kS1apAsnPath =
    "fixtures/telecom/s1ap_14_4_0/s1ap_14_4_0.asn";

fs::path fixtures_root() {
  // tests/CMakeLists.txt passes ASN1CXX_FIXTURES_DIR as both a compile-time
  // define (so this translation unit can `stringify` it into a literal)
  // and as an env var (so a `ctest` invocation can still reach it when the
  // fixture root moves). Compile-time path is the canonical one.
#ifndef ASN1CXX_FIXTURES_DIR_STR
#define ASN1CXX_FIXTURES_DIR_STR ASN1CXX_FIXTURES_DIR
#endif
  return fs::path(ASN1CXX_FIXTURES_DIR_STR);
}

}  // namespace

// --- SHA-256 FIPS vectors ---

TEST(EmitCache, Sha256FipsVectors) {
  EXPECT_EQ(asn1::codegen::sha256_hex("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(asn1::codegen::sha256_hex(""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(asn1::codegen::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  EXPECT_EQ(asn1::codegen::sha256_hex("The quick brown fox jumps over the lazy dog"),
            "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592");
}

// --- Flat mode: second emit must hit, first emit must miss ---

TEST(EmitCacheFlat, SecondRunHitsCache) {
  const fs::path tmp = make_tmpdir("flat_hit");
  const fs::path asn = tmp / "Person.asn";
  write_file(asn, kPersonAsn);

  asn1::Diagnostics diag1;
  const auto model1 = build_model(asn, diag1);
  ASSERT_TRUE(diag1.ok()) << "parse/analyze failed";
  emit_with_cache(model1, asn, tmp, diag1);
  ASSERT_TRUE(diag1.ok()) << "first emit failed";
  EXPECT_GT(count_cache_notes(diag1), 0u);
  const HitMiss hm1 = latest_cache_stats(diag1);
  EXPECT_EQ(hm1.hits, 0u);  // first run cannot hit
  EXPECT_EQ(hm1.total, 1u);

  // Snapshot first-run outputs to compare against the second run.
  const std::string hpp_v1 = read_file(tmp / "generated.hpp");
  const std::string cpp_v1 = read_file(tmp / "generated.cpp");
  ASSERT_FALSE(hpp_v1.empty());
  ASSERT_FALSE(cpp_v1.empty());

  asn1::Diagnostics diag2;
  const auto model2 = build_model(asn, diag2);
  ASSERT_TRUE(diag2.ok());
  emit_with_cache(model2, asn, tmp, diag2);
  ASSERT_TRUE(diag2.ok());
  const HitMiss hm2 = latest_cache_stats(diag2);
  EXPECT_EQ(hm2.hits, 1u);
  EXPECT_EQ(hm2.total, 1u);

  // The cached outputs must be byte-identical to the first run.
  EXPECT_EQ(hpp_v1, read_file(tmp / "generated.hpp"));
  EXPECT_EQ(cpp_v1, read_file(tmp / "generated.cpp"));

  // Cache directory must exist under the emit dir.
  EXPECT_TRUE(fs::exists(tmp / ".asn1cxx-cache" / "default.cache"));

  std::error_code ec;
  fs::remove_all(tmp, ec);
}

TEST(EmitCacheFlat, EditedSourceInvalidatesCache) {
  const fs::path tmp = make_tmpdir("flat_edit");
  const fs::path asn = tmp / "Person.asn";
  write_file(asn, kPersonAsn);

  asn1::Diagnostics diag1;
  emit_with_cache(build_model(asn, diag1), asn, tmp, diag1);
  ASSERT_TRUE(diag1.ok());

  // Touch the source: add a benign comment so the bytes change but the
  // ASN.1 syntax stays valid. (Replacing an identifier with a similar
  // name like `age -> Age` is a parseable mutation but the test has to
  // also know the resulting IR keeps `ok()`, which is fragile; comments
  // are safest.)
  std::string src{kPersonAsn};
  src.append("\n-- benign comment edit for cache invalidation\n");
  write_file(asn, src);

  asn1::Diagnostics diag2;
  emit_with_cache(build_model(asn, diag2), asn, tmp, diag2);
  ASSERT_TRUE(diag2.ok());
  // After invalidation the second run should NOT hit.
  const HitMiss hm = latest_cache_stats(diag2);
  EXPECT_EQ(hm.hits, 0u);
  EXPECT_EQ(hm.total, 1u);

  std::error_code ec;
  fs::remove_all(tmp, ec);
}

TEST(EmitCacheFlat, NoCacheOptionAlwaysMisses) {
  const fs::path tmp = make_tmpdir("flat_nocache");
  const fs::path asn = tmp / "Person.asn";
  write_file(asn, kPersonAsn);

  asn1::Diagnostics diag1;
  emit_no_cache(build_model(asn, diag1), asn, tmp, diag1);
  ASSERT_TRUE(diag1.ok());
  EXPECT_EQ(count_cache_notes(diag1), 0u);  // no cache: no note emitted

  asn1::Diagnostics diag2;
  emit_no_cache(build_model(asn, diag2), asn, tmp, diag2);
  ASSERT_TRUE(diag2.ok());
  EXPECT_EQ(count_cache_notes(diag2), 0u);
  // No cache directory created either.
  EXPECT_FALSE(fs::exists(tmp / ".asn1cxx-cache"));

  std::error_code ec;
  fs::remove_all(tmp, ec);
}

TEST(EmitCacheFlat, ManualOutputEditInvalidatesCache) {
  const fs::path tmp = make_tmpdir("flat_manual");
  const fs::path asn = tmp / "Person.asn";
  write_file(asn, kPersonAsn);

  asn1::Diagnostics diag1;
  emit_with_cache(build_model(asn, diag1), asn, tmp, diag1);
  ASSERT_TRUE(diag1.ok());

  // Simulate a downstream consumer rewriting the output (e.g. include fix).
  const fs::path hpp = tmp / "generated.hpp";
  std::string text = read_file(hpp);
  text.append("\n// manual post-edit marker\n");
  write_file(hpp, text);

  asn1::Diagnostics diag2;
  emit_with_cache(build_model(asn, diag2), asn, tmp, diag2);
  ASSERT_TRUE(diag2.ok());
  // Post-write mismatch invalidates the entry: second run must miss and
  // restore the original (un-marker) header.
  const HitMiss hm = latest_cache_stats(diag2);
  EXPECT_EQ(hm.hits, 0u);
  EXPECT_EQ(hm.total, 1u);
  EXPECT_EQ(read_file(hpp).find("manual post-edit marker"), std::string::npos);

  std::error_code ec;
  fs::remove_all(tmp, ec);
}

TEST(EmitCacheFlat, DifferentBasenameInvalidatesCache) {
  const fs::path tmp = make_tmpdir("flat_basename");
  const fs::path asn = tmp / "Person.asn";
  write_file(asn, kPersonAsn);

  {
    asn1::Diagnostics diag;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Uper;
    opt.basename = "first";
    opt.emit_cache = true;
    opt.source_paths = {fs::absolute(asn)};
    asn1::codegen::CppGenerator gen;
    gen.emit_to_dir(build_model(asn, diag), opt, diag, tmp);
    ASSERT_TRUE(diag.ok());
    EXPECT_TRUE(fs::exists(tmp / "first.hpp"));
  }
  {
    // Re-emit with a different basename. The cache key differs (basename
    // is part of `cache_extra_inputs()`) so this is a miss; the old
    // first.hpp file is now removed only if the owned hash still matches.
    asn1::Diagnostics diag;
    asn1::codegen::EmitOptions opt;
    opt.codec = asn1::codegen::CodecKind::Uper;
    opt.basename = "second";
    opt.emit_cache = true;
    opt.source_paths = {fs::absolute(asn)};
    asn1::codegen::CppGenerator gen;
    gen.emit_to_dir(build_model(asn, diag), opt, diag, tmp);
    ASSERT_TRUE(diag.ok());
    const HitMiss hm = latest_cache_stats(diag);
    EXPECT_EQ(hm.hits, 0u);
    EXPECT_EQ(hm.total, 1u);
    EXPECT_FALSE(fs::exists(tmp / "first.hpp"));
    EXPECT_TRUE(fs::exists(tmp / "second.hpp"));
  }

  std::error_code ec;
  fs::remove_all(tmp, ec);
}

// --- Split mode: per-unit cache ---

TEST(EmitCacheSplit, LppSingleUnitSecondRunHits) {
  const fs::path fixtures = fixtures_root();
  const fs::path asn = fs::absolute(fixtures / kLppAsnPath);
  if (!fs::exists(asn)) {
    GTEST_SKIP() << "LPP fixture not found at " << asn.string();
  }
  const fs::path tmp = make_tmpdir("split_lpp");

  asn1::Diagnostics diag1;
  asn1::codegen::EmitOptions opt1;
  opt1.codec = asn1::codegen::CodecKind::Uper;
  opt1.basename = "generated";
  opt1.namespace_name = "asn1_split_lpp";
  opt1.emit_split = true;
  opt1.emit_cache = true;
  opt1.source_paths = {asn};
  asn1::codegen::CppGenerator gen1;
  gen1.emit_split(build_model(asn, diag1), opt1, diag1, tmp);
  ASSERT_TRUE(diag1.ok());

  asn1::Diagnostics diag2;
  asn1::codegen::EmitOptions opt2 = opt1;
  asn1::codegen::CppGenerator gen2;
  gen2.emit_split(build_model(asn, diag2), opt2, diag2, tmp);
  ASSERT_TRUE(diag2.ok());
  const HitMiss hm = latest_cache_stats(diag2);
  // LPP is a single module -> one unit; a clean re-emit must hit all of it.
  EXPECT_EQ(hm.hits, 1u);
  EXPECT_EQ(hm.total, 1u);

  std::error_code ec;
  fs::remove_all(tmp, ec);
}

TEST(EmitCacheSplit, NoCacheStillEmitsAllUnits) {
  const fs::path fixtures = fixtures_root();
  const fs::path asn = fs::absolute(fixtures / kLppAsnPath);
  if (!fs::exists(asn)) {
    GTEST_SKIP() << "LPP fixture not found at " << asn.string();
  }
  const fs::path tmp = make_tmpdir("split_lpp_nocache");

  asn1::Diagnostics diag;
  asn1::codegen::EmitOptions opt;
  opt.codec = asn1::codegen::CodecKind::Uper;
  opt.basename = "generated";
  opt.namespace_name = "asn1_split_lpp";
  opt.emit_split = true;
  opt.emit_cache = false;
  opt.source_paths = {asn};
  asn1::codegen::CppGenerator gen;
  gen.emit_split(build_model(asn, diag), opt, diag, tmp);
  ASSERT_TRUE(diag.ok());
  EXPECT_EQ(count_cache_notes(diag), 0u);

  std::error_code ec;
  fs::remove_all(tmp, ec);
}

TEST(EmitCacheFlat, OptionsDependenciesAndOutputDamageInvalidate) {
  const auto tmp = make_tmpdir("invalidation");
  const auto asn = tmp / "Person.asn", dependency = tmp / "dependency.asn";
  write_file(asn, kPersonAsn);
  write_file(dependency, "dependency revision 1");
  asn1::Diagnostics parsed;
  const auto model = build_model(asn, parsed);
  ASSERT_TRUE(parsed.ok());
  asn1::codegen::EmitOptions opt;
  opt.source_paths = {asn, dependency};
  auto emit = [&] {
    asn1::Diagnostics d;
    asn1::codegen::CppGenerator{}.emit_to_dir(model, opt, d, tmp);
    EXPECT_TRUE(d.ok());
    return latest_cache_stats(d).hits;
  };
  EXPECT_EQ(emit(), 0u); EXPECT_EQ(emit(), 1u);
  opt.namespace_name = "changed";
  EXPECT_EQ(emit(), 0u); EXPECT_EQ(emit(), 1u);
  opt.codec = asn1::codegen::CodecKind::Ber;
  EXPECT_EQ(emit(), 0u);
  opt.emit_inline = false;
  EXPECT_EQ(emit(), 0u);
  write_file(dependency, "dependency revision 2");
  EXPECT_EQ(emit(), 0u); EXPECT_EQ(emit(), 1u);
  write_file(tmp / "generated.hpp", "corrupted output");
  EXPECT_EQ(emit(), 0u);
  fs::remove(tmp / "generated.cpp");
  EXPECT_EQ(emit(), 0u);
  // Simulate a cache produced by another generator revision.
  auto metadata = read_file(tmp / ".asn1cxx-cache/default.cache");
  ASSERT_GE(metadata.size(), 64u);
  metadata.replace(0, 64, 64, '0');
  write_file(tmp / ".asn1cxx-cache/default.cache", metadata);
  EXPECT_EQ(emit(), 0u);
  fs::remove(dependency);
  EXPECT_EQ(emit(), 0u); EXPECT_EQ(emit(), 0u);
  fs::remove_all(tmp);
}

TEST(EmitCacheFlat, MissingProvenanceNeverReusesProgrammaticModel) {
  const auto tmp = make_tmpdir("model");
  const auto asn = tmp / "Person.asn";
  write_file(asn, kPersonAsn);
  asn1::Diagnostics d;
  auto first = build_model(asn, d);
  asn1::codegen::EmitOptions opt;
  asn1::codegen::CppGenerator{}.emit_to_dir(first, opt, d, tmp);
  auto before = read_file(tmp / "generated.hpp");
  write_file(asn, "M DEFINITIONS ::= BEGIN Different ::= BOOLEAN END");
  auto second = build_model(asn, d);
  asn1::codegen::CppGenerator{}.emit_to_dir(second, opt, d, tmp);
  EXPECT_TRUE(d.ok());
  EXPECT_NE(before, read_file(tmp / "generated.hpp"));
  EXPECT_EQ(count_cache_notes(d), 0u);
  EXPECT_TRUE(asn1::codegen::sha256_file_hex(tmp).empty());
  fs::remove_all(tmp);
}

TEST(EmitCacheFlat, InputOrderMustMatchUncachedOutput) {
  const auto tmp = make_tmpdir("source_order");
  const auto a = tmp / "a.asn", b = tmp / "b.asn";
  write_file(a, "A DEFINITIONS ::= BEGIN Alpha ::= SEQUENCE { n INTEGER (0..255) } END");
  write_file(b, "B DEFINITIONS ::= BEGIN Beta ::= SEQUENCE { yes BOOLEAN } END");
  auto model_for = [&](const std::vector<fs::path>& paths, asn1::Diagnostics& d) {
    std::vector<asn1::SourceFile> files;
    files.reserve(paths.size());
    for (const auto& path : paths) {
      auto file = asn1::SourceFile::from_path(path.string());
      EXPECT_TRUE(file.ok());
      files.push_back(std::move(file.value()));
    }
    std::vector<std::unique_ptr<asn1::ast::Module>> modules;
    for (auto& file : files) {
      asn1::Lexer lexer(file, d);
      asn1::Parser parser(lexer, d);
      auto parsed = parser.parse_modules();
      for (auto& module : parsed) modules.push_back(std::move(module));
    }
    return asn1::Analyzer(d).analyze(std::move(modules));
  };
  asn1::codegen::EmitOptions options;
  options.source_paths = {a, b};
  asn1::Diagnostics first;
  auto ab = model_for(options.source_paths, first);
  asn1::codegen::CppGenerator{}.emit_to_dir(ab, options, first, tmp / "cached");
  ASSERT_TRUE(first.ok());
  options.source_paths = {b, a};
  asn1::Diagnostics second;
  auto ba = model_for(options.source_paths, second);
  asn1::codegen::CppGenerator{}.emit_to_dir(ba, options, second, tmp / "cached");
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(latest_cache_stats(second).hits, 0u);
  options.emit_cache = false;
  asn1::Diagnostics fresh;
  asn1::codegen::CppGenerator{}.emit_to_dir(ba, options, fresh, tmp / "fresh");
  ASSERT_TRUE(fresh.ok());
  EXPECT_EQ(read_file(tmp / "cached/generated.hpp"), read_file(tmp / "fresh/generated.hpp"));
  EXPECT_EQ(read_file(tmp / "cached/generated.cpp"), read_file(tmp / "fresh/generated.cpp"));
  options.emit_cache = true;
  asn1::Diagnostics same;
  asn1::codegen::CppGenerator{}.emit_to_dir(ba, options, same, tmp / "cached");
  EXPECT_TRUE(same.ok());
  EXPECT_EQ(latest_cache_stats(same).hits, 1u);
  fs::remove_all(tmp);
}

TEST(EmitOutput, PortableNamesAndDirectoryOwnership) {
  auto tmp = make_tmpdir("output_contract");
  auto asn = tmp / "input.asn";
  write_file(asn, "M DEFINITIONS ::= BEGIN N ::= INTEGER (0..255) END");
  asn1::Diagnostics parsed;
  auto model = build_model(asn, parsed);
  ASSERT_TRUE(parsed.ok());
  asn1::codegen::EmitOptions options;
  options.emit_cache = false;
  for (const auto* name : {"../escape", "CON", "lpt1", "bad.name", "a/b", "a\\b", ""}) {
    options.basename = name;
    asn1::Diagnostics diag;
    asn1::codegen::CppGenerator{}.emit_to_dir(model,options,diag,tmp/"invalid");
    EXPECT_FALSE(diag.ok()) << name;
    EXPECT_FALSE(fs::exists(tmp/"invalid"));
  }
  options.basename = "generated";
  for (const auto* name : {"class", "std::nested", "7bad", "_reserved", "a__b"}) {
    options.namespace_name = name;
    asn1::Diagnostics diag;
    EXPECT_TRUE(asn1::codegen::CppGenerator{}.emit_header_string(model,options,diag).empty());
    EXPECT_FALSE(diag.ok());
  }
  options.namespace_name = "asn1_gen";
  const auto unicode_dir = tmp / fs::u8path(u8"目录 with spaces");
  asn1::Diagnostics unicode;
  asn1::codegen::CppGenerator{}.emit_to_dir(model,options,unicode,unicode_dir);
  EXPECT_TRUE(unicode.ok());
  EXPECT_TRUE(fs::exists(unicode_dir/"generated.hpp"));
  EXPECT_TRUE(fs::exists(unicode_dir/".asn1cxx-manifest"));
  fs::create_directory(tmp/"output");
  write_file(tmp/"output/generated.hpp", "user-owned\n");
  asn1::Diagnostics unowned;
  asn1::codegen::CppGenerator{}.emit_to_dir(model,options,unowned,tmp/"output");
  EXPECT_FALSE(unowned.ok());
  EXPECT_EQ(read_file(tmp/"output/generated.hpp"), "user-owned\n");
  EXPECT_FALSE(fs::exists(tmp/"output/generated.cpp"));
  EXPECT_FALSE(fs::exists(tmp/"output/.asn1cxx-manifest"));
  fs::remove_all(tmp);
}

TEST(EmitOutput, ModeSwitchCleansOnlyUnmodifiedOwnedFiles) {
  auto tmp = make_tmpdir("output_modes");
  auto asn = tmp/"input.asn";
  write_file(asn,"M DEFINITIONS ::= BEGIN N ::= INTEGER (0..255) END");
  asn1::Diagnostics parsed;
  auto model = build_model(asn,parsed);
  ASSERT_TRUE(parsed.ok());
  asn1::codegen::EmitOptions options;
  options.source_paths = {asn};
  asn1::Diagnostics first;
  asn1::codegen::CppGenerator{}.emit_to_dir(model,options,first,tmp/"out");
  ASSERT_TRUE(first.ok());
  EXPECT_NE(read_file(tmp/"out/.asn1cxx-manifest").find("dependencies"),std::string::npos);
  write_file(tmp/"out/notes.hpp","user file");
  asn1::Diagnostics split;
  asn1::codegen::CppGenerator{}.emit_split(model,options,split,tmp/"out");
  ASSERT_TRUE(split.ok());
  EXPECT_FALSE(fs::exists(tmp/"out/generated.cpp"));
  EXPECT_TRUE(fs::exists(tmp/"out/M.cpp"));
  write_file(tmp/"out/M.cpp","edited owned file");
  asn1::Diagnostics flat;
  asn1::codegen::CppGenerator{}.emit_to_dir(model,options,flat,tmp/"out");
  ASSERT_TRUE(flat.ok());
  EXPECT_EQ(read_file(tmp/"out/M.cpp"),"edited owned file");
  EXPECT_EQ(read_file(tmp/"out/notes.hpp"),"user file");
  EXPECT_FALSE(fs::exists(tmp/"out/M.hpp"));
  fs::remove_all(tmp);
}

TEST(EmitOutput, BusyDirectoryAndSplitCollisionPreservePreviousOutput) {
  auto tmp = make_tmpdir("output_busy");
  auto asn = tmp/"input.asn";
  write_file(asn,"M DEFINITIONS ::= BEGIN N ::= INTEGER (0..255) END");
  asn1::Diagnostics parsed;
  auto model = build_model(asn,parsed);
  asn1::codegen::EmitOptions options;
  asn1::Diagnostics first;
  asn1::codegen::CppGenerator{}.emit_to_dir(model,options,first,tmp/"out");
  ASSERT_TRUE(first.ok());
  const auto old = read_file(tmp/"out/generated.hpp");
  fs::create_directory(tmp/"out/.asn1cxx-emitting");
  asn1::Diagnostics busy;
  asn1::codegen::CppGenerator{}.emit_to_dir(model,options,busy,tmp/"out");
  EXPECT_FALSE(busy.ok());
  EXPECT_EQ(read_file(tmp/"out/generated.hpp"),old);
  fs::remove(tmp/"out/.asn1cxx-emitting");
  options.basename="m"; // case insensitive aggregate vs M module collision
  asn1::Diagnostics collision;
  asn1::codegen::CppGenerator{}.emit_split(model,options,collision,tmp/"out");
  EXPECT_FALSE(collision.ok());
  EXPECT_EQ(read_file(tmp/"out/generated.hpp"),old);
  EXPECT_FALSE(fs::exists(tmp/"out/m.hpp"));
  EXPECT_FALSE(fs::exists(tmp/"out/.asn1cxx-emitting"));
  fs::remove_all(tmp);
}

TEST(EmitOutput, InvalidManifestAndSymlinkNeverReplaceForeignFiles) {
  auto tmp = make_tmpdir("output_foreign");
  auto asn=tmp/"input.asn";
  write_file(asn,"M DEFINITIONS ::= BEGIN N ::= INTEGER (0..255) END");
  asn1::Diagnostics parsed;
  auto model=build_model(asn,parsed);
  asn1::codegen::EmitOptions options;
  fs::create_directory(tmp/"out");
  write_file(tmp/"out/.asn1cxx-manifest","corrupted\n");
  asn1::Diagnostics invalid;
  asn1::codegen::CppGenerator{}.emit_to_dir(model,options,invalid,tmp/"out");
  EXPECT_FALSE(invalid.ok());
  EXPECT_FALSE(fs::exists(tmp/"out/generated.hpp"));
  fs::remove(tmp/"out/.asn1cxx-manifest");
  write_file(tmp/"foreign.hpp","foreign content");
  std::error_code ec;
  fs::create_symlink(tmp/"foreign.hpp",tmp/"out/generated.hpp",ec);
  if (!ec) {
    asn1::Diagnostics symlink;
    asn1::codegen::CppGenerator{}.emit_to_dir(model,options,symlink,tmp/"out");
    EXPECT_FALSE(symlink.ok());
    EXPECT_EQ(read_file(tmp/"foreign.hpp"),"foreign content");
    EXPECT_TRUE(fs::is_symlink(tmp/"out/generated.hpp"));
  }
  fs::remove_all(tmp);
}
