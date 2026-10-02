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
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::string_view kVersion = "0.1.0";

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
      << "  --codec <uper|aper|both>  Codec for --emit-dir (default: uper)\n"
      << "\n"
      << "See docs/ARCHITECTURE.md.\n";
}

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

int dump_ast(const std::string& path) {
  asn1::Diagnostics diag;
  asn1::SourceFile file = asn1::SourceFile::from_path(path);
  asn1::Lexer lexer(file, diag);
  asn1::Parser parser(lexer, diag);
  auto module = parser.parse_module();
  if (module) {
    std::cout << "; AST for " << path << '\n';
    asn1::ast::print(std::cout, *module);
  }
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }
  if (!module) {
    std::cerr << "asn1cxx: failed to parse module\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

asn1::ir::Model analyze_files(const std::vector<std::string>& paths, asn1::Diagnostics& diag,
                              std::vector<asn1::SourceFile>& files) {
  std::vector<std::unique_ptr<asn1::ast::Module>> modules;
  files.reserve(paths.size());
  for (const std::string& path : paths) {
    files.push_back(asn1::SourceFile::from_path(path));
    asn1::Lexer lexer(files.back(), diag);
    asn1::Parser parser(lexer, diag);
    auto module = parser.parse_module();
    if (module) {
      modules.push_back(std::move(module));
    }
  }
  asn1::Analyzer analyzer(diag);
  return analyzer.analyze(std::move(modules));
}

int dump_ir(const std::vector<std::string>& paths) {
  asn1::Diagnostics diag;
  std::vector<asn1::SourceFile> files;
  asn1::ir::Model model = analyze_files(paths, diag, files);
  std::cout << "; IR\n";
  asn1::ir::print(std::cout, model);
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

int emit_code(const std::vector<std::string>& paths, const std::string& dir,
              asn1::codegen::CodecKind codec) {
  asn1::Diagnostics diag;
  std::vector<asn1::SourceFile> files;
  asn1::ir::Model model = analyze_files(paths, diag, files);
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }

  std::filesystem::create_directories(dir);
  const auto header_path = std::filesystem::path(dir) / "generated.hpp";
  const auto source_path = std::filesystem::path(dir) / "generated.cpp";

  std::ofstream header(header_path);
  std::ofstream source(source_path);
  if (!header || !source) {
    std::cerr << "asn1cxx: failed to open emit outputs in " << dir << '\n';
    return EXIT_FAILURE;
  }

  asn1::codegen::EmitOptions opt;
  opt.codec = codec;
  opt.basename = "generated";
  asn1::codegen::CppGenerator gen;
  gen.emit(model, opt, diag, header, &source);
  if (!diag.ok()) {
    diag.print(std::cerr);
    return EXIT_FAILURE;
  }
  std::cerr << "asn1cxx: wrote " << header_path.string() << " and " << source_path.string()
            << '\n';
  return EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc <= 1) {
    print_usage(std::cerr);
    return EXIT_FAILURE;
  }

  bool dump_tok = false;
  bool dump_ast_flag = false;
  bool dump_ir_flag = false;
  std::string emit_dir;
  asn1::codegen::CodecKind codec = asn1::codegen::CodecKind::Uper;
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
    if (arg == "--codec") {
      if (i + 1 >= argc) {
        std::cerr << "asn1cxx: --codec requires uper|aper|both\n";
        return EXIT_FAILURE;
      }
      const std::string_view v = argv[++i];
      if (v == "uper") {
        codec = asn1::codegen::CodecKind::Uper;
      } else if (v == "aper") {
        codec = asn1::codegen::CodecKind::Aper;
      } else if (v == "both") {
        codec = asn1::codegen::CodecKind::Both;
      } else {
        std::cerr << "asn1cxx: unknown codec '" << v << "'\n";
        return EXIT_FAILURE;
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
      if (dump_ir(files) != EXIT_SUCCESS) {
        status = EXIT_FAILURE;
      }
    }
    if (!emit_dir.empty()) {
      if (emit_code(files, emit_dir, codec) != EXIT_SUCCESS) {
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
