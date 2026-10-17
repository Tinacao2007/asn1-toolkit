/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/tools/asn1cxx/main.cpp
**   $Version: 0.1
**   $Date:   2026-10-03
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Command-line driver: lex/parse ASN.1 modules, run semantic analysis,
**   optional
**   AST/IR dumps, and emit generated C++ under --emit-dir.
**
** Specification: Invokes X.680/X.681 front end; optional codecs per
**                 X.690/X.691/X.696/X.693/X.697.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include <asn1/ast/print.hpp>
#include <asn1/codegen/emit.hpp>
#include <asn1/frontend/lexer.hpp>
#include <asn1/frontend/parser.hpp>
#include <asn1/frontend/token.hpp>
#include <asn1/ir/print.hpp>
#include <asn1/semantic/analyzer.hpp>
#include <asn1/support/diagnostics.hpp>
#include <asn1/support/source_file.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kVersion = "0.1.0";

/**
 *  Function    : parse_codec_item
 *  Description : Parses one codec name from a --codec list item.
 *  Parameters  : v — std::string_view v
 *  Returns     : std::optional<asn1::codegen::CodecKind>
 */
std::optional<asn1::codegen::CodecKind> parse_codec_item(std::string_view v) {
  if (v == "uper") {
    return asn1::codegen::CodecKind::Uper;
  }
  if (v == "aper") {
    return asn1::codegen::CodecKind::Aper;
  }
  if (v == "per") {
    return asn1::codegen::CodecKind::Per;
  }
  if (v == "jer") {
    return asn1::codegen::CodecKind::Jer;
  }
  if (v == "xer") {
    return asn1::codegen::CodecKind::Xer;
  }
  if (v == "exer") {
    return asn1::codegen::CodecKind::Exer;
  }
  if (v == "ber") {
    return asn1::codegen::CodecKind::Ber;
  }
  if (v == "der") {
    return asn1::codegen::CodecKind::Der;
  }
  if (v == "oer") {
    return asn1::codegen::CodecKind::Oer;
  }
  if (v == "coer") {
    return asn1::codegen::CodecKind::Coer;
  }
  if (v == "cxer") {
    return asn1::codegen::CodecKind::Cxer;
  }
  if (v == "cer") {
    return asn1::codegen::CodecKind::Cer;
  }
  return std::nullopt;
}

/**
 *  Function    : print_usage
 *  Description : Performs print usage (definition).
 *  Parameters  : out — std::ostream& out
 *  Returns     : void
 */
void print_usage(std::ostream& out) {
  out << "asn1cxx - ASN.1 compiler (asn1-toolkit " << kVersion << ")\n"
      << "\n"
      << "Usage:\n"
      << "  asn1cxx [options] <file.asn>...\n"
      << "\n"
      << "Options:\n"
      << "  -h, --help              Show this help and exit\n"
      << "  -V, --version           Show version and exit\n"
      << "  --dump-tokens           Lex each file and print the token stream\n"
      << "  --dump-ast              Parse each file and print the AST\n"
      << "  --dump-ir               Parse + analyze and print the type IR\n"
      << "  --emit-dir <dir>        Emit generated.hpp / generated.cpp into <dir>\n"
      << "  --codec <uper|aper|per|ber|der|cer|jer|xer|exer|oer|coer|cxer>\n"
      << "                         Comma-separated combination (e.g. uper,ber)\n"
      << "                         Codec for --emit-dir (default: uper)\n"
      << "  --namespace <id>        C++ namespace for generated types (default: asn1_gen)\n"
      << "  --emit-inline           Emit codec functions inline in the header\n"
      << "                         (default: definitions live in generated.cpp)\n"
      << "  --emit-split            Emit one <Module>.hpp/.cpp pair per ASN.1\n"
      << "                         module (types namespaced per module) plus\n"
      << "                         the aggregate <basename>.hpp\n"
      << "  --emit-cache / --no-emit-cache\n"
      << "                         Toggle the content-hash cache stored in\n"
      << "                         <dir>/.asn1cxx-cache/ (default: on; CI\n"
      << "                         golden diffs typically pass --no-emit-cache\n"
      << "                         to disable)\n"
      << "  --parallel <N>          Run declare + resolve passes in parallel\n"
      << "                         with N worker threads (0 = serial baseline,\n"
      << "                         default: 0). lower + tag remain serial.\n"
      << "\n"
      << "See docs/README.md.\n";
}

/**
 *  Function    : dump_tokens
 *  Description : Computes dump tokens from (path).
 *  Parameters  : path — const std::string& path
 *  Returns     : int
 */
int dump_tokens(const std::string& path) {
  asn1::Diagnostics diag;
  asn1::SourceFile file = asn1::SourceFile::from_path(path);
  asn1::Lexer lexer(file, diag);

  std::cout << "; tokens for " << path << '\n';
  for (;;) {
    asn1::Token tok = lexer.next();
    const auto& loc = tok.range.begin;
    std::cout << loc.line << ':' << loc.column << '\t' << asn1::to_string(tok.kind);
    if (!tok.text.empty()) {
      std::cout << '\t' << tok.text;
    }
    std::cout << '\n';
    if (tok.kind == asn1::TokenKind::EndOfFile) {
      break;
    }
  }
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/**
 *  Function    : dump_ast
 *  Description : Computes dump ast from (path).
 *  Parameters  : path — const std::string& path
 *  Returns     : int
 */
int dump_ast(const std::string& path) {
  asn1::Diagnostics diag;
  asn1::SourceFile file = asn1::SourceFile::from_path(path);
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto modules = parser.parse_modules();
  if (!modules.empty()) {
    std::cout << "; AST for " << path << '\n';
    for (const auto& module : modules) {
      asn1::ast::print(std::cout, *module);
    }
  }
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }
  if (modules.empty()) {
    std::cerr << "asn1cxx: failed to parse module\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

asn1::ir::Model analyze_files(const std::vector<std::string>& paths, asn1::Diagnostics& diag,
                              std::vector<asn1::SourceFile>& files, int parallelism) {
  std::vector<std::unique_ptr<asn1::ast::Module>> modules;
  files.reserve(paths.size());
  for (const std::string& path : paths) {
    files.push_back(asn1::SourceFile::from_path(path));
    asn1::Lexer lexer(files.back(), diag);
    asn1::Parser parser(lexer, diag);
    auto file_modules = parser.parse_modules();
    for (auto& module : file_modules) {
      modules.push_back(std::move(module));
    }
  }
  asn1::Analyzer analyzer(diag);
  return analyzer.analyze(std::move(modules), parallelism);
}

/**
 *  Function    : dump_ir
 *  Description : Computes dump ir from (paths).
 *  Parameters  : paths — const std::vector<std::string>& paths
 *  Returns     : int
 */
int dump_ir(const std::vector<std::string>& paths, int parallelism) {
  asn1::Diagnostics diag;
  std::vector<asn1::SourceFile> files;
  asn1::ir::Model model = analyze_files(paths, diag, files, parallelism);
  std::cout << "; IR\n";
  asn1::ir::print(std::cout, model);
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

// P2-3: walk the diagnostics for the most recent "emit cache:" note and
// extract its `<hits>/<total>` ratio. Returns {0,0} when no note exists
// (e.g. the CLI passed `--no-emit-cache` or no emit ran).
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

int emit_code(const std::vector<std::string>& paths, const std::string& dir,
              asn1::codegen::CodecKind codec, const std::string& namespace_name,
              bool emit_inline, bool emit_split, bool emit_cache, int parallelism) {
  asn1::Diagnostics diag;
  std::vector<asn1::SourceFile> files;
  asn1::ir::Model model = analyze_files(paths, diag, files, parallelism);
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }

  std::filesystem::create_directories(dir);
  asn1::codegen::EmitOptions opt;
  opt.codec = codec;
  opt.basename = "generated";
  opt.namespace_name = namespace_name;
  opt.emit_inline = emit_inline;
  opt.emit_split = emit_split;
  opt.emit_cache = emit_cache;
  // Absolute paths to the .asn inputs; the content-hash cache folds these
  // into the unit key so a source edit naturally invalidates the entry.
  opt.source_paths.reserve(paths.size());
  for (const std::string& p : paths) {
    opt.source_paths.push_back(std::filesystem::absolute(p));
  }

  if (emit_split) {
    asn1::codegen::CppGenerator gen;
    gen.emit_split(model, opt, diag, dir);
    if (!diag.ok()) {
      diag.print(std::cerr);
      return EXIT_FAILURE;
    }
    diag.print(std::cerr);
    // Tell apart a 100% cache hit (the aggregate header is always
    // rewritten because it just `#include`s every unit) from a partial hit
    // that actually opened some emit unit's streams.
    const HitMiss hm2 = latest_cache_stats(diag);
    if (hm2.total > 0 && hm2.hits == hm2.total) {
      std::cerr << "asn1cxx: reused (cached) split emit into " << dir
                << " (<Module>.hpp/.cpp per module + generated.hpp aggregate)\n";
    } else {
      std::cerr << "asn1cxx: wrote split emit into " << dir
                << " (<Module>.hpp/.cpp per module + generated.hpp aggregate)\n";
    }
    return EXIT_SUCCESS;
  }

  asn1::codegen::CppGenerator gen;
  gen.emit_to_dir(model, opt, diag, dir);
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }
  // Emit diagnostics (the cache note lives here) and a per-mode status line.
  // A 100% cache hit leaves the on-disk files untouched; the user wants to
  // know whether they got "wrote" or "reused". We tell apart by looking at
  // the most recent cache note: if every unit hit, nothing was written.
  diag.print(std::cerr);
  const HitMiss hm = latest_cache_stats(diag);
  if (hm.total > 0 && hm.hits == hm.total) {
    std::cerr << "asn1cxx: reused (cached) " << (std::filesystem::path(dir) / "generated.hpp")
              << " and " << (std::filesystem::path(dir) / "generated.cpp") << '\n';
  } else {
    std::cerr << "asn1cxx: wrote " << (std::filesystem::path(dir) / "generated.hpp") << " and "
              << (std::filesystem::path(dir) / "generated.cpp") << '\n';
  }
  return EXIT_SUCCESS;
}

}  // namespace

/**
 *  Function    : main
 *  Description : Computes main from (argc, argv).
 *  Parameters  : argc — int argc; argv — char** argv
 *  Returns     : int
 */
int main(int argc, char** argv) {
  if (argc <= 1) {
    print_usage(std::cerr);
    return EXIT_FAILURE;
  }

  bool dump_tok = false;
  bool dump_ast_flag = false;
  bool dump_ir_flag = false;
  std::string emit_dir;
  std::string namespace_name = "asn1_gen";
  // CLI default is out-of-line (declarations in the .hpp, definitions in the
  // companion .cpp); --emit-inline restores the all-inline header for small
  // single-TU modules where a zero-link setup is worth more.
  bool emit_inline = false;
  // --emit-split: per-module <Module>.hpp/.cpp pairs + aggregate header
  // (types namespaced per module; out-of-line regardless of --emit-inline).
  bool emit_split = false;
  // P2-3: --no-emit-cache disables the content-hash cache; the default
  // (`emit_cache = true`) skips emit when the unit's source + options are
  // unchanged since the last successful run.
  bool emit_cache = true;
  // P3-1: parallel frontend analysis (declare + resolve passes). 0 keeps the
  // original serial baseline (default — preserves byte-identical diagnostics).
  int parallelism = 0;
  // None until the first --codec flag; falls back to Uper afterwards so an
  // unset --codec keeps the historical default while --codec uper,ber-style
  // combinations accumulate by OR.
  asn1::codegen::CodecKind codec = asn1::codegen::CodecKind::None;
  std::vector<std::string> files;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage(std::cout);
      return EXIT_SUCCESS;
    }
    if (arg == "-V" || arg == "--version") {
      std::cout << "asn1cxx " << kVersion << '\n';
      return EXIT_SUCCESS;
    }
    if (arg == "--dump-tokens") {
      dump_tok = true;
      continue;
    }
    if (arg == "--dump-ast") {
      dump_ast_flag = true;
      continue;
    }
    if (arg == "--dump-ir") {
      dump_ir_flag = true;
      continue;
    }
    if (arg == "--emit-dir") {
      if (i + 1 >= argc) {
        std::cerr << "asn1cxx: --emit-dir requires a directory\n";
        return EXIT_FAILURE;
      }
      emit_dir = argv[++i];
      continue;
    }
    if (arg == "--namespace") {
      if (i + 1 >= argc) {
        std::cerr << "asn1cxx: --namespace requires an identifier\n";
        return EXIT_FAILURE;
      }
      namespace_name = argv[++i];
      continue;
    }
    if (arg == "--emit-inline") {
      emit_inline = true;
      continue;
    }
    if (arg == "--emit-split") {
      emit_split = true;
      continue;
    }
    if (arg == "--emit-cache") {
      emit_cache = true;
      continue;
    }
    if (arg == "--no-emit-cache") {
      emit_cache = false;
      continue;
    }
    if (arg == "--parallel") {
      if (i + 1 >= argc) {
        std::cerr << "asn1cxx: --parallel requires a worker count (0 = serial)\n";
        return EXIT_FAILURE;
      }
      const std::string v = argv[++i];
      try {
        const long n = std::stol(v);
        if (n < 0 || n > 1024) {
          std::cerr << "asn1cxx: --parallel must be 0..1024\n";
          return EXIT_FAILURE;
        }
        parallelism = static_cast<int>(n);
      } catch (const std::exception&) {
        std::cerr << "asn1cxx: --parallel requires an integer (got '" << v << "')\n";
        return EXIT_FAILURE;
      }
      continue;
    }
    if (arg == "--codec") {
      if (i + 1 >= argc) {
        std::cerr << "asn1cxx: --codec requires a comma-separated list of "
                     "uper|aper|per|ber|der|cer|jer|xer|exer|oer|coer|cxer\n";
        return EXIT_FAILURE;
      }
      const std::string_view v = argv[++i];
      // Comma-separated combination (e.g. --codec uper,ber) OR/OR repeated
      // --codec flags accumulate into one bitmask.
      std::size_t pos = 0;
      while (pos <= v.size()) {
        const std::size_t comma = v.find(',', pos);
        const std::string_view item =
            v.substr(pos, comma == std::string_view::npos
                               ? std::string_view::npos
                               : comma - pos);
        pos = (comma == std::string_view::npos) ? v.size() + 1 : comma + 1;
        if (item.empty()) {
          std::cerr << "asn1cxx: empty codec name in '--codec " << v << "'\n";
          return EXIT_FAILURE;
        }
        std::optional<asn1::codegen::CodecKind> one = parse_codec_item(item);
        if (!one) {
          std::cerr << "asn1cxx: unknown codec '" << item << "'\n";
          return EXIT_FAILURE;
        }
        codec = codec | *one;
      }
      continue;
    }
    if (!arg.empty() && arg[0] == '-') {
      std::cerr << "asn1cxx: unknown option '" << arg << "'\n";
      return EXIT_FAILURE;
    }
    files.emplace_back(arg);
  }

  if (files.empty()) {
    print_usage(std::cerr);
    return EXIT_FAILURE;
  }

  if (!dump_tok && !dump_ast_flag && !dump_ir_flag && emit_dir.empty()) {
    std::cerr << "asn1cxx: specify --dump-tokens, --dump-ast, --dump-ir, or --emit-dir\n";
    return EXIT_FAILURE;
  }

  int status = EXIT_SUCCESS;
  try {
    if (dump_ir_flag) {
      if (dump_ir(files, parallelism) != EXIT_SUCCESS) {
        status = EXIT_FAILURE;
      }
    }
    if (!emit_dir.empty()) {
      // No --codec given at all: keep the historical UPER default.
      if (codec == asn1::codegen::CodecKind::None) {
        codec = asn1::codegen::CodecKind::Uper;
      }
      if (emit_code(files, emit_dir, codec, namespace_name, emit_inline, emit_split, emit_cache, parallelism) != EXIT_SUCCESS) {
        status = EXIT_FAILURE;
      }
    }
    for (const std::string& path : files) {
      if (dump_tok) {
        if (dump_tokens(path) != EXIT_SUCCESS) {
          status = EXIT_FAILURE;
        }
      }
      if (dump_ast_flag) {
        if (dump_ast(path) != EXIT_SUCCESS) {
          status = EXIT_FAILURE;
        }
      }
    }
  } catch (const std::exception& ex) {
    std::cerr << "asn1cxx: " << ex.what() << '\n';
    status = EXIT_FAILURE;
  }
  return status;
}
