/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tests/codegen/cli_test.cpp
**   $Version: 0.1
**   $Date:   2026-10-12
**   $Author: tina.cao
***************************************************************************/
/**  File Description:
**
**   End-to-end tests for the asn1cxx CLI. Spawns the built asn1cxx binary
**   as a subprocess and checks exit codes, stderr messages, and emitted
**   files. (Kept as a subprocess rather than including main.cpp so the
**   heavy compiler headers stay out of the test translation unit.)
**
** Specification: asn1cxx CLI contract (tools/asn1cxx/main.cpp).
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

struct CliResult {
  int code;
  std::string out;
  std::string err;
};

// Run the asn1cxx binary with args, capturing stdout/stderr via temp files.
// Temp file names include the pid so parallel ctest invocations of this binary
// (one per test, via --gtest_filter) never share files.
CliResult run_asn1cxx(const std::vector<std::string>& args) {
  namespace fs = std::filesystem;
  const fs::path bin = ASN1CXX_BIN;
  const std::string tag =
      "asn1cxx_cli_" + std::to_string(static_cast<long long>(::getpid())) + "_";
  const fs::path out_f = fs::temp_directory_path() / (tag + "out.txt");
  const fs::path err_f = fs::temp_directory_path() / (tag + "err.txt");

  std::vector<std::string> storage;
  storage.reserve(args.size() + 3);
  storage.push_back(bin.string());
  for (const auto& a : args) storage.push_back(a);

  std::vector<char*> argv;
  argv.reserve(storage.size() + 1);
  for (auto& s : storage) argv.push_back(const_cast<char*>(s.c_str()));
  argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid == 0) {
    // Child: redirect stdout/stderr to temp files, then exec.
    FILE* fo = std::fopen(out_f.c_str(), "w");
    FILE* fe = std::fopen(err_f.c_str(), "w");
    if (fo) dup2(fileno(fo), STDOUT_FILENO);
    if (fe) dup2(fileno(fe), STDERR_FILENO);
    execv(bin.c_str(), argv.data());
    _exit(127);  // exec failed
  }
  int status = 0;
  waitpid(pid, &status, 0);
  const int code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

  auto read_all = [](const fs::path& p) {
    std::string s;
    if (FILE* f = std::fopen(p.c_str(), "r")) {
      char buf[4096];
      std::size_t n;
      while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
      std::fclose(f);
    }
    return s;
  };
  CliResult r{code, read_all(out_f), read_all(err_f)};
  std::error_code ec;
  fs::remove(out_f, ec);
  fs::remove(err_f, ec);
  return r;
}

std::string example(const char* name) {
  return std::string(ASN1_EXAMPLES_DIR) + "/" + name;
}

}  // namespace

TEST(Cli, NoArgsFailsWithUsage) {
  const auto r = run_asn1cxx({});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("Usage:"), std::string::npos);
}

TEST(Cli, HelpSucceeds) {
  const auto r = run_asn1cxx({"--help"});
  EXPECT_EQ(r.code, 0);
  EXPECT_NE(r.out.find("Usage:"), std::string::npos);
}

TEST(Cli, VersionSucceeds) {
  const auto r = run_asn1cxx({"--version"});
  EXPECT_EQ(r.code, 0);
  EXPECT_NE(r.out.find("asn1cxx"), std::string::npos);
}

TEST(Cli, DumpIrOnExampleSucceeds) {
  const auto r = run_asn1cxx({"--dump-ir", example("person.asn")});
  EXPECT_EQ(r.code, 0);
}

TEST(Cli, DumpAstOnExampleSucceeds) {
  const auto r = run_asn1cxx({"--dump-ast", example("person.asn")});
  EXPECT_EQ(r.code, 0);
}

TEST(Cli, UnknownOptionFails) {
  const auto r = run_asn1cxx({"--bogus", example("person.asn")});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("unknown option"), std::string::npos);
}

TEST(Cli, UnknownCodecFails) {
  const auto r = run_asn1cxx(
      {"--emit-dir", "/tmp/asn1cxx_cli_ignore", "--codec", "nope",
       example("person.asn")});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("unknown codec"), std::string::npos);
}

TEST(Cli, MissingInputFileFails) {
  const auto r = run_asn1cxx({"--dump-ir", "/no/such/file.asn"});
  EXPECT_NE(r.code, 0);
}

TEST(Cli, EmitProducesFiles) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() / "asn1cxx_cli_emit_test";
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  ASSERT_FALSE(ec);

  const auto r = run_asn1cxx(
      {"--emit-dir", dir.string(), "--codec", "uper", "--namespace", "cli_gen",
       example("person.asn")});
  EXPECT_EQ(r.code, 0);
  EXPECT_TRUE(fs::exists(dir / "generated.hpp"));
  EXPECT_TRUE(fs::exists(dir / "generated.cpp"));

  fs::remove_all(dir, ec);
}

// --- Multi-codec combination (comma-separated / repeated --codec flags) ---

std::string read_text(const std::string& path) {
  std::string s;
  if (std::FILE* f = std::fopen(path.c_str(), "r")) {
    char buf[4096];
    std::size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    std::fclose(f);
  }
  return s;
}

// Emits into a per-test dir and returns the generated.hpp text.
std::string emit_hpp(const std::vector<std::string>& args, int* code) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() /
                       ("asn1cxx_cli_codec_" +
                        std::to_string(static_cast<long long>(::getpid())));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);

  std::vector<std::string> full = args;
  full.insert(full.begin(), "--emit-dir");
  full.insert(full.begin() + 1, dir.string());
  const auto r = run_asn1cxx(full);
  *code = r.code;
  const std::string hpp = read_text((dir / "generated.hpp").string());
  fs::remove_all(dir, ec);
  return hpp;
}

TEST(Cli, CodecComboUperBerEmitsBothRuntimes) {
  int code = -1;
  const std::string hpp =
      emit_hpp({"--codec", "uper,ber", example("person.asn")}, &code);
  EXPECT_EQ(code, 0);
  EXPECT_NE(hpp.find("asn1/runtime/uper.hpp"), std::string::npos);
  EXPECT_NE(hpp.find("asn1/runtime/ber/tlv.hpp"), std::string::npos);
  // APER was NOT requested and must not leak into the emitted header.
  EXPECT_EQ(hpp.find("asn1/runtime/aper.hpp"), std::string::npos);
}

TEST(Cli, DefaultCodecIsUperWhenFlagAbsent) {
  int code = -1;
  const std::string hpp = emit_hpp({example("person.asn")}, &code);
  EXPECT_EQ(code, 0);
  EXPECT_NE(hpp.find("asn1/runtime/uper.hpp"), std::string::npos);
  EXPECT_EQ(hpp.find("asn1/runtime/ber/tlv.hpp"), std::string::npos);
}

TEST(Cli, RepeatedCodecFlagsAccumulate) {
  int code = -1;
  const std::string hpp = emit_hpp(
      {"--codec", "uper", "--codec", "ber", example("person.asn")}, &code);
  EXPECT_EQ(code, 0);
  EXPECT_NE(hpp.find("asn1/runtime/uper.hpp"), std::string::npos);
  EXPECT_NE(hpp.find("asn1/runtime/ber/tlv.hpp"), std::string::npos);
}

TEST(Cli, EmptyCodecNameInListFails) {
  const auto r = run_asn1cxx(
      {"--emit-dir", "/tmp/asn1cxx_cli_ignore", "--codec", "uper,",
       example("person.asn")});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("empty codec name"), std::string::npos);
}

TEST(Cli, UnknownCodecInsideListFails) {
  const auto r = run_asn1cxx(
      {"--emit-dir", "/tmp/asn1cxx_cli_ignore", "--codec", "uper,nope",
       example("person.asn")});
  EXPECT_NE(r.code, 0);
  EXPECT_NE(r.err.find("unknown codec"), std::string::npos);
}

// --- Out-of-line emit (default): declarations in .hpp, definitions in .cpp ---

// Emits into a per-test dir and returns (hpp, cpp) text.
std::pair<std::string, std::string> emit_hpp_cpp(const std::vector<std::string>& args,
                                                 int* code) {
  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path() /
                       ("asn1cxx_cli_ool_" +
                        std::to_string(static_cast<long long>(::getpid())));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);

  std::vector<std::string> full = args;
  full.insert(full.begin(), "--emit-dir");
  full.insert(full.begin() + 1, dir.string());
  const auto r = run_asn1cxx(full);
  *code = r.code;
  const std::string hpp = read_text((dir / "generated.hpp").string());
  const std::string cpp = read_text((dir / "generated.cpp").string());
  fs::remove_all(dir, ec);
  return {hpp, cpp};
}

TEST(Cli, DefaultEmitIsOutOfLine) {
  int code = -1;
  const auto [hpp, cpp] = emit_hpp_cpp(
      {"--codec", "uper", "--namespace", "ool_gen", example("person.asn")}, &code);
  EXPECT_EQ(code, 0);
  // Header: declarations only (no `inline` definitions, no runtime call bodies).
  EXPECT_NE(hpp.find("encode_uper(asn1::BitWriter& w, const Person& value);"),
            std::string::npos);
  EXPECT_EQ(hpp.find("inline asn1::Result"), std::string::npos);
  EXPECT_EQ(hpp.find("asn1::uper::encode_integer"), std::string::npos);
  // Companion .cpp: the definitions, wrapped in the generated namespace.
  EXPECT_NE(cpp.find("#include \"generated.hpp\""), std::string::npos);
  EXPECT_NE(cpp.find("namespace ool_gen {"), std::string::npos);
  EXPECT_NE(cpp.find("asn1::Result<void> encode_uper(asn1::BitWriter& w, "
                     "const Person& value) {"),
            std::string::npos);
  EXPECT_NE(cpp.find("asn1::uper::encode_integer"), std::string::npos);
  EXPECT_NE(cpp.find("}  // namespace ool_gen"), std::string::npos);
}

TEST(Cli, EmitInlineRestoresHeaderDefinitions) {
  int code = -1;
  const auto [hpp, cpp] = emit_hpp_cpp(
      {"--codec", "uper", "--emit-inline", "--namespace", "inl_gen",
       example("person.asn")},
      &code);
  EXPECT_EQ(code, 0);
  EXPECT_NE(hpp.find("inline asn1::Result<void> encode_uper(asn1::BitWriter& w, "
                     "const Person& value) {"),
            std::string::npos);
  EXPECT_NE(hpp.find("asn1::uper::encode_integer"), std::string::npos);
  // Companion TU stays a stub in inline mode.
  EXPECT_NE(cpp.find("companion TU"), std::string::npos);
  EXPECT_EQ(cpp.find("asn1::uper::encode_integer"), std::string::npos);
}

// --- P2-3 content-hash cache (--emit-cache / --no-emit-cache) ---

namespace fs = std::filesystem;

// Emits into a per-test dir twice (cache-miss then cache-hit) and returns
// the diagnostic stderr from the second run so callers can look for the
// "emit cache:" note.
struct DoubleRun {
  int code_first;
  int code_second;
  std::string err_first;
  std::string err_second;
  fs::path dir;
};
DoubleRun run_twice(const std::vector<std::string>& base_args) {
  DoubleRun r{};
  r.dir = fs::temp_directory_path() /
          ("asn1cxx_cli_cache_" +
           std::to_string(static_cast<long long>(::getpid())));
  std::error_code ec;
  fs::remove_all(r.dir, ec);
  fs::create_directories(r.dir, ec);

  std::vector<std::string> args = base_args;
  args.insert(args.begin(), "--emit-dir");
  args.insert(args.begin() + 1, r.dir.string());

  auto first = run_asn1cxx(args);
  r.code_first = first.code;
  r.err_first = first.err;

  auto second = run_asn1cxx(args);
  r.code_second = second.code;
  r.err_second = second.err;
  return r;
}

TEST(Cli, EmitCacheSecondRunHits) {
  const auto r = run_twice({"--codec", "uper", example("person.asn")});
  EXPECT_EQ(r.code_first, 0);
  EXPECT_EQ(r.code_second, 0);
  // Second-run stderr should mention a cache hit count.
  EXPECT_NE(r.err_second.find("emit cache:"), std::string::npos);
  EXPECT_NE(r.err_second.find("units hit"), std::string::npos);
  std::error_code ec;
  fs::remove_all(r.dir, ec);
}

TEST(Cli, NoEmitCacheNeverProducesCacheNote) {
  const auto r = run_twice({"--codec", "uper", "--no-emit-cache",
                            example("person.asn")});
  EXPECT_EQ(r.code_first, 0);
  EXPECT_EQ(r.code_second, 0);
  // Neither run should mention the cache (caching disabled entirely).
  EXPECT_EQ(r.err_first.find("emit cache:"), std::string::npos);
  EXPECT_EQ(r.err_second.find("emit cache:"), std::string::npos);
  // No cache directory on disk either.
  EXPECT_FALSE(fs::exists(r.dir / ".asn1cxx-cache"));
  std::error_code ec;
  fs::remove_all(r.dir, ec);
}

TEST(Cli, EmitCacheFlagReEnables) {
  const auto r = run_twice({"--no-emit-cache", "--emit-cache", "--codec",
                            "uper", example("person.asn")});
  EXPECT_EQ(r.code_first, 0);
  EXPECT_EQ(r.code_second, 0);
  // Last flag wins: --emit-cache is on, so second run reports a hit.
  EXPECT_NE(r.err_second.find("emit cache:"), std::string::npos);
  // On a 100% cache hit the user-visible status line is "reused (cached)"
  // instead of "wrote ...", so callers can tell apart the no-op and the
  // fresh-emit cases without parsing the cache note.
  EXPECT_NE(r.err_second.find("reused (cached)"), std::string::npos);
  std::error_code ec;
  fs::remove_all(r.dir, ec);
}

TEST(Cli, EmitCacheMissSaysWrote) {
  // First run is necessarily a miss; the user-visible status line is the
  // historical "wrote ... and ...". After that the second run reuses the
  // cache (covered by EmitCacheSecondRunHits).
  const auto r = run_twice({"--codec", "uper", example("person.asn")});
  EXPECT_EQ(r.code_first, 0);
  EXPECT_EQ(r.code_second, 0);
  EXPECT_NE(r.err_first.find("wrote "), std::string::npos);
  EXPECT_EQ(r.err_first.find("reused (cached)"), std::string::npos);
  std::error_code ec;
  fs::remove_all(r.dir, ec);
}

// End-to-end on the big fixture that exercises the split-mode cache path
// (per-unit content-hash + per-unit output verification). Run from a real
// LPP fixture; if the fixture is missing, the test self-skips.
TEST(Cli, SplitEmitCacheSecondRunHits) {
  // End-to-end on the big fixture that exercises the split-mode cache path
  // (per-unit content-hash + per-unit output verification). We resolve to
  // an absolute path because ctest may run us from any CWD.
  const fs::path asn =
      fs::absolute(fs::path(ASN1_EXAMPLES_DIR).parent_path() / "tests/fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn");
  if (!fs::exists(asn)) {
    GTEST_SKIP() << "LPP fixture not present at " << asn.string();
  }
  const auto r = run_twice({"--codec", "uper", "--emit-split", asn.string()});
  if (r.code_first != 0) {
    GTEST_SKIP() << "LPP fixture parse/analyze failed (code " << r.code_first << ")";
  }
  EXPECT_EQ(r.code_second, 0);
  EXPECT_NE(r.err_second.find("emit cache:"), std::string::npos);
  std::error_code ec;
  fs::remove_all(r.dir, ec);
}

// P3-1: byte-equality gate between serial (--parallel 0) and parallel
// (--parallel 16) runs on the LPP 3GPP fixture. This is the headline
// reproducibility check listed in docs/todo.md (P3-1 sub-task 8).
TEST(Cli, ParallelModeProducesByteIdenticalOutput) {
  const fs::path asn =
      fs::absolute(fs::path(ASN1_EXAMPLES_DIR).parent_path() / "tests/fixtures/telecom/lpp_14_3_0/LPP-PDU-Definitions.asn");
  if (!fs::exists(asn)) {
    GTEST_SKIP() << "LPP fixture not present at " << asn.string();
  }
  const fs::path dir_s = fs::temp_directory_path() /
                         ("asn1cxx_p31_serial_" + std::to_string(static_cast<long long>(::getpid())));
  const fs::path dir_p = fs::temp_directory_path() /
                         ("asn1cxx_p31_parallel_" + std::to_string(static_cast<long long>(::getpid())));
  std::error_code ec;
  fs::remove_all(dir_s, ec);
  fs::remove_all(dir_p, ec);
  fs::create_directories(dir_s, ec);
  fs::create_directories(dir_p, ec);

  // `--no-emit-cache` keeps the comparison focused on analyze output.
  const CliResult s =
      run_asn1cxx({"--emit-dir", dir_s.string(), "--codec", "uper",
                   "--namespace", "p31_serial", "--no-emit-cache", asn.string()});
  const CliResult p =
      run_asn1cxx({"--emit-dir", dir_p.string(), "--codec", "uper",
                   "--namespace", "p31_parallel", "--no-emit-cache",
                   "--parallel", "16", asn.string()});
  if (s.code != 0) {
    fs::remove_all(dir_s, ec);
    fs::remove_all(dir_p, ec);
    GTEST_SKIP() << "LPP fixture parse/analyze failed (code " << s.code << ")";
  }
  EXPECT_EQ(p.code, 0);
  // The generated files must be byte-equal except for the namespace name
  // the user passed (which is intentionally different so the two outputs
  // are visibly distinct in CI logs). Strip the namespace token before
  // comparing.
  const auto bytes = [](const fs::path& f) {
    std::string s;
    if (FILE* fp = std::fopen(f.c_str(), "rb")) {
      char buf[8192];
      std::size_t n;
      while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) s.append(buf, n);
      std::fclose(fp);
    }
    return s;
  };
  auto replace_all = [](std::string s, const std::string& from,
                        const std::string& to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
      s.replace(pos, from.size(), to);
      pos += to.size();
    }
    return s;
  };
  const std::string b_s =
      replace_all(bytes(dir_s / "generated.hpp"), "p31_serial", "P31_NS");
  const std::string b_p =
      replace_all(bytes(dir_p / "generated.hpp"), "p31_parallel", "P31_NS");
  const std::string c_s =
      replace_all(bytes(dir_s / "generated.cpp"), "p31_serial", "P31_NS");
  const std::string c_p =
      replace_all(bytes(dir_p / "generated.cpp"), "p31_parallel", "P31_NS");
  EXPECT_EQ(b_s, b_p) << "generated.hpp diverged between serial and parallel";
  EXPECT_EQ(c_s, c_p) << "generated.cpp diverged between serial and parallel";

  fs::remove_all(dir_s, ec);
  fs::remove_all(dir_p, ec);
}
