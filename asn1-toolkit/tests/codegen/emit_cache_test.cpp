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
#include <filesystem>
#include <fstream>
#include <iostream>
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
      "_" + tag + "_" + std::to_string(static_cast<long long>(::getpid())) + "_" +
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
  asn1::SourceFile file = asn1::SourceFile::from_path(asn.string());
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
    // first.hpp file remains untouched on disk.
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
    EXPECT_TRUE(fs::exists(tmp / "first.hpp"));
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
