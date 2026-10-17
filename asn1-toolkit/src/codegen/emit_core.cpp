/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_core.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   Core emitter: driver (run/emit_header), naming and type
   declarations, plus the shared helpers used by every codec
   family file.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "emit_internal.hpp"

#include <asn1/codegen/sha256.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace asn1 {
namespace codegen {

/**
 *  Function    : is_cpp_keyword
 *  Description : Returns whether cpp keyword holds for the given inputs.
 *  Parameters  : id — std::string_view id
 *  Returns     : bool
 */
bool is_cpp_keyword(std::string_view id) {
  // Identifiers that would be illegal as C++ enum/member/type names.
  static constexpr const char* kWords[] = {
      "alignas",      "alignof",    "and",         "and_eq",     "asm",
      "auto",         "bitand",     "bitor",       "bool",       "break",
      "case",         "catch",      "char",        "char8_t",    "char16_t",
      "char32_t",     "class",      "compl",       "concept",    "const",
      "consteval",    "constexpr",  "constinit",   "const_cast", "continue",
      "co_await",     "co_return",  "co_yield",    "decltype",   "default",
      "delete",       "do",         "double",      "dynamic_cast","else",
      "enum",         "explicit",   "export",      "extern",     "false",
      "float",        "for",        "friend",      "goto",       "if",
      "inline",       "int",        "long",        "mutable",    "namespace",
      "new",          "noexcept",   "not",         "not_eq",     "nullptr",
      "operator",     "or",         "or_eq",       "private",    "protected",
      "public",       "register",   "reinterpret_cast","requires","return",
      "short",        "signed",     "sizeof",      "static",     "static_assert",
      "static_cast",  "struct",     "switch",      "template",   "this",
      "thread_local", "throw",      "true",        "try",        "typedef",
      "typeid",       "typename",   "union",       "unsigned",   "using",
      "virtual",      "void",       "volatile",    "wchar_t",    "while",
      "xor",          "xor_eq",
  };
  for (const char* w : kWords) {
    if (id == w) {
      return true;
    }
  }
  return false;
}

/**
 *  Function    : cpp_ident
 *  Description : Builds and returns a string for cpp ident.
 *  Parameters  : name — std::string_view name
 *  Returns     : std::string
 */
std::string cpp_ident(std::string_view name) {
  std::string out;
  out.reserve(name.size() + 1);
  for (char c : name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '_') {
      out.push_back(c);
    } else if (c == '-' || c == ' ') {
      out.push_back('_');
    }
  }
  if (out.empty() || (out[0] >= '0' && out[0] <= '9')) {
    out.insert(out.begin(), '_');
  }
  if (is_cpp_keyword(out)) {
    out.push_back('_');
  }
  return out;
}

/**
 *  Function    : resolve
 *  Description : Computes resolve from (model, id).
 *  Parameters  : model — const ir::Model& model; id — ir::TypeId id
 *  Returns     : const ir::Type&
 */
const ir::Type& resolve(const ir::Model& model, ir::TypeId id) {
  const ir::Type* t = &model.arena.get(id);
  while (t->kind == ir::TypeKind::Referenced && t->referenced.resolved != ir::kInvalidType) {
    t = &model.arena.get(t->referenced.resolved);
  }
  return *t;
}

/**
 *  Function    : is_named
 *  Description : Returns whether the given type carries an ASN.1 name.
 *  Parameters  : t — const ir::Type& t
 *  Returns     : bool
 */
bool is_named(const ir::Type& t) { return !t.name.empty(); }


/**
 *  Function    : named_type_cpp
 *  Description : Builds the C++ name of a named type.
 *  Parameters  : t — const ir::Type& t
 *  Returns     : std::string
 */
std::string named_type_cpp(const ir::Type& t) { return cpp_ident(t.name); }


/**
 *  Function    : integer_is_bigint
 *  Description : Returns whether the integer needs the BigInt host type.
 *  Parameters  : d — const ir::IntegerDesc& d
 *  Returns     : bool
 */
bool integer_is_bigint(const ir::IntegerDesc& d) { return !d.host_bits.has_value(); }


/**
 *  Function    : integer_host_type
 *  Description : Builds and returns a string for integer host type.
 *  Parameters  : d — const ir::IntegerDesc& d
 *  Returns     : std::string
 */
std::string integer_host_type(const ir::IntegerDesc& d) {
  if (d.host_bits) {
    if (!d.is_signed) {
      switch (*d.host_bits) {
        case 8:
          return "std::uint8_t";
        case 16:
          return "std::uint16_t";
        case 32:
          return "std::uint32_t";
        default:
          return "std::uint64_t";
      }
    }
    switch (*d.host_bits) {
      case 8:
        return "std::int8_t";
      case 16:
        return "std::int16_t";
      case 32:
        return "std::int32_t";
      default:
        return "std::int64_t";
    }
  }
  return "asn1::BigInteger";
}

/**
 *  Function    : opt_i64_expr
 *  Description : Builds and returns a string for opt i64 expr.
 *  Parameters  : b — const std::optional<ir::BigInt>& b
 *  Returns     : std::string
 */
std::string opt_i64_expr(const std::optional<ir::BigInt>& b) {
  if (b && b->as_i64) {
    return "std::int64_t{" + std::to_string(*b->as_i64) + "}";
  }
  return "std::nullopt";
}

/**
 *  Function    : opt_size_expr
 *  Description : Builds and returns a string for opt size expr.
 *  Parameters  : b — const std::optional<ir::BigInt>& b
 *  Returns     : std::string
 */
std::string opt_size_expr(const std::optional<ir::BigInt>& b) {
  if (b && b->as_i64 && *b->as_i64 >= 0) {
    return "std::size_t{" + std::to_string(static_cast<std::uint64_t>(*b->as_i64)) + "}";
  }
  return "std::nullopt";
}

  /**
   *  Function    : run
   *  Description : Performs run (definition).
   *  Parameters  : header — std::ostream& header; source — std::ostream* source
   *  Returns     : void
   */
void Emitter::run(std::ostream& header, std::ostream* source) {
    collect_named();
    const std::string ns = cpp_ident(options_.namespace_name);
    def_out_ = (source && !options_.emit_inline) ? source : nullptr;
    if (def_out_) {
      // Out-of-line mode: preamble first, namespace opened before the
      // definitions that emit_header streams in, closed afterwards.
      *source << "// Generated by asn1cxx - definitions TU (see "
              << options_.basename << ".hpp for declarations).\n"
              << "#include \"" << options_.basename << ".hpp\"\n\n"
              << "namespace " << ns << " {\n\n";
    } else if (source) {
      *source << "// Generated by asn1cxx - companion TU (types are header-only).\n"
              << "#include \"" << options_.basename << ".hpp\"\n";
    }
    emit_header(header);
    if (def_out_) {
      *source << "}  // namespace " << ns << "\n";
      def_out_ = nullptr;
    }
  }

void Emitter::run_to_dir(const std::filesystem::path& dir) {
    cache_reset_stats();
    std::filesystem::create_directories(dir);
    const std::filesystem::path hpp_path = dir / (options_.basename + ".hpp");
    const std::filesystem::path cpp_path = dir / (options_.basename + ".cpp");
    const std::vector<std::filesystem::path> outputs{hpp_path.filename(),
                                                     cpp_path.filename()};
    // Cache check: if the unit is in cache and outputs are byte-identical,
    // skip emit entirely. Note `expected_outputs` carry paths relative to
    // `dir` so hash_output_file can resolve them under emit_dir.
    if (cache_check_unit("default", outputs)) {
      cache_finalise();
      return;
    }
    {
      // Bracket the streams so they close (flush + sync) before
      // cache_record_unit hashes the files on disk — otherwise the
      // post-write hash is the empty-file hash and the cache is poisoned.
      std::ofstream header(hpp_path, std::ios::binary | std::ios::trunc);
      std::ofstream source(cpp_path, std::ios::binary | std::ios::trunc);
      if (!header || !source) {
        diag_.error({}, "failed to open emit outputs in " + dir.string());
        return;
      }
      auto t0 = std::chrono::steady_clock::now();
      run(header, &source);
      auto t1 = std::chrono::steady_clock::now();
      cache_miss_wall_ += std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0);
    }
    cache_record_unit("default", outputs);
    cache_finalise();
  }

  /**
   *  Function    : begin_codec_fn
   *  Description : Emits a codec function header. Inline mode writes
   *                "inline <sig> {" to the declaration stream; out-of-line
   *                mode writes "<sig>;" to the header and "<sig> {" to the
   *                definition stream.
   *  Parameters  : decl_out — header stream; sig — full signature text
   *  Returns     : std::ostream* — stream the body must be written to
   */
std::ostream* Emitter::begin_codec_fn(std::ostream& decl_out, const std::string& sig) {
    if (def_out_) {
      decl_out << sig << ";\n";
      *def_out_ << sig << " {\n";
      return def_out_;
    }
    decl_out << "inline " << sig << " {\n";
    return &decl_out;
  }

  /**
   *  Function    : collect_named
   *  Description : Performs collect named (definition).
   *  Parameters  : none
   *  Returns     : void
   */
void Emitter::collect_named() {
    std::unordered_set<ir::TypeId> visiting;
    std::function<void(ir::TypeId)> consider = [&](ir::TypeId id) {
      if (id == ir::kInvalidType || named_set_.count(id) || visiting.count(id)) {
        return;
      }
      const ir::Type& t = model_.arena.get(id);
      if (!is_named(t)) {
        return;
      }
      for (ir::TypeId existing : named_order_) {
        const ir::Type& e = model_.arena.get(existing);
        // C++ names carry no module qualifier, so two arena types with the
        // same name collide in the output even from different modules (the
        // same parameterized instantiation reached via two paths). Dedup on
        // the emitted name. In split mode each module has its own
        // namespace, so only same-module names collide.
        const bool same_name =
            options_.emit_split
                ? (cpp_ident(e.name) == cpp_ident(t.name) && e.module == t.module)
                : (cpp_ident(e.name) == cpp_ident(t.name));
        if (same_name) {
          named_set_.insert(id);
          return;
        }
      }
      visiting.insert(id);
      std::function<void(ir::TypeId)> dep = [&](ir::TypeId d) {
        if (d == ir::kInvalidType) {
          return;
        }
        const ir::Type& dt = model_.arena.get(d);
        if (dt.kind == ir::TypeKind::Referenced &&
            dt.referenced.resolved != ir::kInvalidType) {
          consider(dt.referenced.resolved);
        }
        // An ObjectClassField type is unnamed, so consider() bails on it; its
        // fixed-type governor (e.g. &id -> ProtocolExtensionID) is otherwise
        // never traversed and the member declaration references a missing type.
        if (dt.kind == ir::TypeKind::ObjectClassField) {
          dep(dt.object_class_field.fixed_type);
        }
        // An inline (anonymous) SEQUENCE OF / SET OF is unnamed too; its
        // element must still be emitted before the containing type so the
        // generated codec call resolves at its definition point.
        if (dt.kind == ir::TypeKind::SequenceOf) {
          dep(dt.sequence_of.element);
        }
        if (dt.kind == ir::TypeKind::SetOf) {
          dep(dt.set_of.element);
        }
        consider(d);
      };
      if (t.kind == ir::TypeKind::Sequence) {
        for (const auto& f : t.sequence.root) {
          dep(f.type);
          if (f.table_constraint) {
            for (const auto& e : f.table_constraint->entries) {
              dep(e.type);
            }
          }
        }
        for (const auto& g : t.sequence.extension_groups) {
          for (const auto& f : g.fields) {
            dep(f.type);
            if (f.table_constraint) {
              for (const auto& e : f.table_constraint->entries) {
                dep(e.type);
              }
            }
          }
        }
        for (const auto& f : t.sequence.trailing_root) {
          dep(f.type);
          if (f.table_constraint) {
            for (const auto& e : f.table_constraint->entries) {
              dep(e.type);
            }
          }
        }
      } else if (t.kind == ir::TypeKind::Set) {
        for (const auto& f : t.set.root) {
          dep(f.type);
          if (f.table_constraint) {
            for (const auto& e : f.table_constraint->entries) {
              dep(e.type);
            }
          }
        }
        for (const auto& g : t.set.extension_groups) {
          for (const auto& f : g.fields) {
            dep(f.type);
            if (f.table_constraint) {
              for (const auto& e : f.table_constraint->entries) {
                dep(e.type);
              }
            }
          }
        }
        for (const auto& f : t.set.trailing_root) {
          dep(f.type);
          if (f.table_constraint) {
            for (const auto& e : f.table_constraint->entries) {
              dep(e.type);
            }
          }
        }
      } else if (t.kind == ir::TypeKind::Choice) {
        for (const auto& f : t.choice.alternatives) {
          dep(f.type);
        }
        for (const auto& f : t.choice.extensions) {
          dep(f.type);
        }
      } else if (t.kind == ir::TypeKind::SequenceOf) {
        dep(t.sequence_of.element);
      } else if (t.kind == ir::TypeKind::SetOf) {
        dep(t.set_of.element);
      } else if (t.kind == ir::TypeKind::Referenced) {
        dep(t.referenced.resolved);
      }
      visiting.erase(id);
      named_set_.insert(id);
      named_order_.push_back(id);
    };
    for (ir::TypeId id : model_.exported_types) {
      consider(id);
    }
    for (std::size_t i = 0; i < model_.arena.type_count(); ++i) {
      consider(static_cast<ir::TypeId>(i));
    }
  }

  /**
   *  Function    : type_cpp
   *  Description : Builds and returns a string for type cpp.
   *  Parameters  : id — ir::TypeId id; for_optional — bool for_optional
   *  Returns     : std::string
   */
std::string Emitter::type_cpp(ir::TypeId id, bool for_optional) {
    if (named_set_.count(id)) {
      return named_ref(model_.arena.get(id));
    }
    const ir::Type& t = resolve(model_, id);
    if (is_named(t)) {
      for (ir::TypeId nid : named_order_) {
        const ir::Type& n = model_.arena.get(nid);
        const bool same_name =
            options_.emit_split
                ? (cpp_ident(n.name) == cpp_ident(t.name) && n.module == t.module)
                : (cpp_ident(n.name) == cpp_ident(t.name));
        if (same_name) {
          return named_ref(n);
        }
      }
      return named_ref(t);
    }
    return inline_type_cpp(t, for_optional);
  }

  std::string Emitter::named_ref(const ir::Type& t) const {
    // Flat single-namespace mode: every named type lives in the generated
    // namespace, so no qualification is needed (byte-identical to the
    // pre-split output).
    if (!options_.emit_split || t.module.empty() || t.module == current_module_) {
      return named_type_cpp(t);
    }
    const auto it = module_ns_.find(t.module);
    if (it == module_ns_.end()) {
      return named_type_cpp(t);
    }
    return it->second + "::" + named_type_cpp(t);
  }

  /**
   *  Function    : inline_type_cpp
   *  Description : Builds and returns a string for inline type cpp.
   *  Parameters  : t — const ir::Type& t; for_optional — bool for_optional
   *  Returns     : std::string
   */
  std::string Emitter::inline_type_cpp(const ir::Type& t, bool /*for_optional*/) {
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        return "bool";
      case ir::TypeKind::Null:
        // Split mode: each module header defines its own `detail::Null`
        // inside the module namespace (a shared namespace-level struct would
        // be multiply defined once the aggregate header includes every
        // module header).
        if (options_.emit_split) {
          return "detail::Null";
        }
        return cpp_ident(options_.namespace_name) + "_detail::Null";
      case ir::TypeKind::Integer:
        return integer_host_type(t.integer);
      case ir::TypeKind::OctetString:
        return "std::vector<std::uint8_t>";
      case ir::TypeKind::BitString:
        return "asn1::BitStringValue";
      case ir::TypeKind::String:
        return "std::string";
      case ir::TypeKind::SequenceOf: {
        return "std::vector<" + type_cpp(t.sequence_of.element) + ">";
      }
      case ir::TypeKind::SetOf: {
        return "std::vector<" + type_cpp(t.set_of.element) + ">";
      }
      case ir::TypeKind::Enumerated:
        return "std::int64_t";
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        return "std::vector<std::uint64_t>";
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set:
      case ir::TypeKind::Choice:
        diag_.error({}, "anonymous SEQUENCE/SET/CHOICE nested types are not emitted as "
                         "inline structs; assign a type name");
        return "/*unsupported-anonymous*/void";
      case ir::TypeKind::ObjectClassField:
        if (t.object_class_field.open_type) {
          return "std::vector<std::uint8_t>";
        }
        return type_cpp(t.object_class_field.fixed_type);
      case ir::TypeKind::Real:
        return "double";
      case ir::TypeKind::InstanceOf:
        return "struct { std::vector<std::uint64_t> type_id; std::vector<std::uint8_t> value; }";
      case ir::TypeKind::Referenced:
        if (t.referenced.resolved != ir::kInvalidType) {
          return type_cpp(t.referenced.resolved);
        }
        diag_.error({}, "unresolved type reference '" + t.referenced.name + "'");
        return "void";
    }
    return "void";
  }

  /**
   *  Function    : emit_header
   *  Description : Performs emit header (definition).
   *  Parameters  : out — std::ostream& out
   *  Returns     : void
   */
void Emitter::emit_includes(std::ostream& out) {
    const bool emit_jer = (options_.codec & CodecKind::Jer) != CodecKind::None;
    const bool emit_xml = (options_.codec &
                           (CodecKind::Xer | CodecKind::Exer | CodecKind::Cxer)) !=
                          CodecKind::None;
    const bool emit_per = (options_.codec & (CodecKind::Uper | CodecKind::Aper)) !=
                          CodecKind::None;
    const bool emit_oer =
        (options_.codec & (CodecKind::Oer | CodecKind::Coer)) != CodecKind::None;
    const bool emit_tlv = (options_.codec & (CodecKind::Ber | CodecKind::Der |
                                             CodecKind::Cer)) != CodecKind::None;
    out << "#include <algorithm>\n"
        << "#include <asn1/runtime/bigint.hpp>\n"
        << "#include <asn1/runtime/bit_string.hpp>\n";
    if (emit_per) {
      out << "#include <asn1/runtime/bit_io.hpp>\n"
          << "#include <asn1/runtime/byte_io.hpp>\n"
          << "#include <asn1/runtime/per/codec.hpp>\n";
    }
    if (emit_oer || emit_tlv) {
      out << "#include <asn1/runtime/byte_io.hpp>\n";
    }
    if ((options_.codec & CodecKind::Uper) != CodecKind::None) {
      out << "#include <asn1/runtime/uper.hpp>\n";
    }
    if ((options_.codec & CodecKind::Aper) != CodecKind::None) {
      out << "#include <asn1/runtime/aper.hpp>\n";
    }
    if (emit_jer) {
      out << "#include <asn1/runtime/byte_io.hpp>\n"
          << "#include <asn1/runtime/jer/json.hpp>\n"
          << "#include <asn1/runtime/jer/codec.hpp>\n"
          << "#include <asn1/runtime/jeri/codec.hpp>\n";
    }
    if (emit_xml) {
      // byte_io.hpp provides asn1::make_error / asn1::Error::Code, which the
      // generated XER/EXER/CXER encode/decode bodies call on malformed input
      // (unknown ENUMERATED identifier, CHOICE index out of range, etc.).
      // PER / OER / TLV families already pull it in via their own branches;
      // XML is the only family that would otherwise leave it out, so we add
      // it here for any XML selection. Harmless when other families are
      // included too (header guards).
      out << "#include <asn1/runtime/byte_io.hpp>\n";
      out << "#include <asn1/runtime/xer/codec.hpp>\n";
      if ((options_.codec & CodecKind::Exer) != CodecKind::None) {
        out << "#include <asn1/runtime/exer/codec.hpp>\n";
      }
      if ((options_.codec & CodecKind::Cxer) != CodecKind::None) {
        out << "#include <asn1/runtime/cxer/codec.hpp>\n";
      }
    }
    if ((options_.codec & CodecKind::Oer) != CodecKind::None) {
      out << "#include <asn1/runtime/oer/codec.hpp>\n";
    }
    if ((options_.codec & CodecKind::Coer) != CodecKind::None) {
      out << "#include <asn1/runtime/coer/codec.hpp>\n";
    }
    if (emit_tlv) {
      out << "#include <asn1/runtime/ber/tlv.hpp>\n"
          << "#include <asn1/runtime/ber/codec.hpp>\n";
      if ((options_.codec & CodecKind::Der) != CodecKind::None) {
        out << "#include <asn1/runtime/der/codec.hpp>\n";
      }
      if ((options_.codec & CodecKind::Cer) != CodecKind::None) {
        out << "#include <asn1/runtime/cer/codec.hpp>\n";
      }
    }
  }

  /**
   *  Function    : emit_types_and_codecs
   *  Description : Emits forward declarations, type declarations and codec
   *                functions for the given named types.
   *  Parameters  : out — std::ostream& out; ids — const std::vector<ir::TypeId>& ids
   *  Returns     : void
   */
void Emitter::emit_types_and_codecs(std::ostream& out, const std::vector<ir::TypeId>& ids) {
    // Forward declarations for named sequence/choice.
    for (ir::TypeId id : ids) {
      const ir::Type& t = model_.arena.get(id);
      if (t.kind == ir::TypeKind::Sequence || t.kind == ir::TypeKind::Set ||
          t.kind == ir::TypeKind::Choice) {
        out << "struct " << named_type_cpp(t) << ";\n";
      }
    }
    out << "\n";

    for (ir::TypeId id : ids) {
      emit_type_decl(out, id);
      out << "\n";
    }

    for (ir::TypeId id : ids) {
      emit_type_codecs(out, id);
    }
  }

  /**
   *  Function    : emit_type_codecs
   *  Description : Performs emit type codecs (definition). The per-type
   *                slice of emit_types_and_codecs.
   *  Parameters  : out — std::ostream& out; id — ir::TypeId id
   *  Returns     : void
   */
void Emitter::emit_type_codecs(std::ostream& out, ir::TypeId id) {
    const bool emit_jer = (options_.codec & CodecKind::Jer) != CodecKind::None;
    const bool emit_xml = (options_.codec &
                           (CodecKind::Xer | CodecKind::Exer | CodecKind::Cxer)) !=
                          CodecKind::None;
    const bool emit_oer =
        (options_.codec & (CodecKind::Oer | CodecKind::Coer)) != CodecKind::None;

    // Type aliases (`using A = B`) share B's codecs; do not emit duplicates.
    if (model_.arena.get(id).kind == ir::TypeKind::Referenced) {
      return;
    }
    if ((options_.codec & CodecKind::Uper) != CodecKind::None) {
      emit_codec_fns(out, id, /*aper=*/false);
    }
    if ((options_.codec & CodecKind::Aper) != CodecKind::None) {
      emit_codec_fns(out, id, /*aper=*/true);
    }
    if (emit_jer) {
      emit_jer_codec_fns(out, id);
    }
    if (emit_xml) {
      emit_xer_codec_fns(out, id);
    }
    if (emit_oer) {
      emit_oer_codec_fns(out, id);
    }
    if ((options_.codec & CodecKind::Ber) != CodecKind::None) {
      emit_tlv_codec_fns(out, id, TlvKind::Ber);
    }
    if ((options_.codec & CodecKind::Der) != CodecKind::None) {
      emit_tlv_codec_fns(out, id, TlvKind::Der);
    }
    if ((options_.codec & CodecKind::Cer) != CodecKind::None) {
      emit_tlv_codec_fns(out, id, TlvKind::Cer);
    }
  }

  /**
   *  Function    : emit_header
   *  Description : Performs emit header (definition). Flat single-namespace
   *                mode: one header carrying every module's types.
   *  Parameters  : out — std::ostream& out
   *  Returns     : void
   */
void Emitter::emit_header(std::ostream& out) {
    const std::string ns = cpp_ident(options_.namespace_name);
    const std::string guard =
        "ASN1_GEN_" + ns + "_" + cpp_ident(options_.basename) + "_HPP_";
    out << "// Generated by asn1cxx - do not edit.\n"
        << "#pragma once\n\n"
        << "#ifndef " << guard << "\n"
        << "#define " << guard << "\n\n";
    emit_includes(out);
    out << "\n"
        << "#include <cstdint>\n"
        << "#include <optional>\n"
        << "#include <string>\n"
        << "#include <type_traits>\n"
        << "#include <utility>\n"
        << "#include <variant>\n"
        << "#include <vector>\n\n"
        << "namespace " << ns << "_detail {\n"
        << "struct Null {};\n"
        << "}  // namespace " << ns << "_detail\n\n"
        << "namespace " << ns << " {\n\n";
    emit_types_and_codecs(out, named_order_);
    out << "}  // namespace " << cpp_ident(options_.namespace_name) << "\n\n"
        << "#endif  // " << guard << "\n";
  }

std::string Emitter::table_entry_alt_name(const ir::TableConstraintEntry& entry, std::size_t idx) {
    std::string base;
    if (!entry.id_symbol.empty()) {
      base = cpp_ident(entry.id_symbol);
    } else if (!entry.type_name.empty()) {
      base = cpp_ident(entry.type_name);
    } else if (entry.has_id_value) {
      base = "id_" + std::to_string(entry.id_value.as_i64.value_or(static_cast<std::int64_t>(idx)));
    } else {
      base = "Alt_" + std::to_string(idx);
    }
    return base + "_";
  }

std::vector<std::string> Emitter::compute_table_entry_alt_names(
      const std::vector<ir::TableConstraintEntry>& entries) {
    std::vector<std::string> names;
    std::unordered_map<std::string, int> seen;
    for (std::size_t i = 0; i < entries.size(); ++i) {
      std::string base = table_entry_alt_name(entries[i], i);
      int count = ++seen[base];
      if (count > 1) {
        if (!base.empty() && base.back() == '_') base.pop_back();
        base += "_" + std::to_string(count) + "_";
      }
      names.push_back(std::move(base));
    }
    return names;
  }

void Emitter::emit_table_constraint_structs(std::ostream& out, const std::vector<ir::Field>& fields) {
    for (const auto& f : fields) {
      if (!f.table_constraint || f.table_constraint->entries.empty()) {
        continue;
      }
      const std::string fname = cpp_ident(f.name);
      const auto alt_names = compute_table_entry_alt_names(f.table_constraint->entries);
      out << "\n  // Table constraint specialization for " << fname << "\n";
      out << "  struct Decoded_" << fname << " {\n";
      for (std::size_t ei = 0; ei < f.table_constraint->entries.size(); ++ei) {
        const auto& entry = f.table_constraint->entries[ei];
        out << "    struct " << alt_names[ei] << " { " << type_cpp(entry.type) << " value; };\n";
      }
      out << "    std::variant<\n";
      out << "      std::monostate";
      for (std::size_t ei = 0; ei < f.table_constraint->entries.size(); ++ei) {
        out << ",\n      " << alt_names[ei];
      }
      out << "\n    > choice;\n";
      out << "  };\n";
      out << "  std::optional<Decoded_" << fname << "> decoded_" << fname << ";\n";
    }
  }

Emitter::AliasShape Emitter::alias_shape(const ir::Type& t) const {
    const ir::Type& r =
        (t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType)
            ? resolve(model_, t.referenced.resolved)
            : t;
    switch (r.kind) {
      case ir::TypeKind::Boolean:
      case ir::TypeKind::Real:
      case ir::TypeKind::Enumerated:
        return AliasShape::Scalar;
      case ir::TypeKind::Integer:
        return integer_is_bigint(r.integer) ? AliasShape::Class : AliasShape::Scalar;
      case ir::TypeKind::ObjectClassField:
        if (r.object_class_field.open_type) {
          return AliasShape::Class;
        }
        return alias_shape(resolve(model_, r.object_class_field.fixed_type));
      default:
        return AliasShape::Class;
    }
  }

void Emitter::emit_distinct_alias(std::ostream& out, const std::string& name, const std::string& host,
                           AliasShape shape) {
    if (shape == AliasShape::Scalar) {
      out << "struct " << name << " {\n"
          << "  " << host << " value{};\n"
          << "  " << name << "() = default;\n"
          << "  " << name << "(" << host << " v) : value(v) {}\n"
          << "  operator " << host << "() const { return value; }\n"
          << "};\n";
      return;
    }
    out << "struct " << name << " : " << host << " {\n"
        << "  " << name << "() = default;\n"
        << "  " << name << "(const " << host << "& v) : " << host << "(v) {}\n"
        << "  " << name << "(" << host << "&& v) : " << host << "(std::move(v)) {}\n"
        << "  " << name << "& operator=(const " << host << "& v) {\n"
        << "    static_cast<" << host << "&>(*this) = v;\n"
        << "    return *this;\n"
        << "  }\n"
        << "  " << name << "& operator=(" << host << "&& v) {\n"
        << "    static_cast<" << host << "&>(*this) = std::move(v);\n"
        << "    return *this;\n"
        << "  }\n"
        << "};\n";
  }

/**
 *  Function    : emit_type_decl
 *  Description : Performs emit type decl (definition).
 *  Parameters  : out — std::ostream& out; id — ir::TypeId id
 *  Returns     : void
 */
void Emitter::emit_type_decl(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        emit_distinct_alias(out, name, "bool", AliasShape::Scalar);
        break;
      case ir::TypeKind::Null:
        emit_distinct_alias(out, name,
                            options_.emit_split
                                ? "detail::Null"
                                : cpp_ident(options_.namespace_name) + "_detail::Null",
                            AliasShape::Class);
        break;
      case ir::TypeKind::Integer:
        emit_distinct_alias(out, name, integer_host_type(t.integer), alias_shape(t));
        break;
      case ir::TypeKind::OctetString:
        emit_distinct_alias(out, name, "std::vector<std::uint8_t>", AliasShape::Class);
        break;
      case ir::TypeKind::BitString:
        emit_distinct_alias(out, name, "asn1::BitStringValue", AliasShape::Class);
        break;
      case ir::TypeKind::String:
        emit_distinct_alias(out, name, "std::string", AliasShape::Class);
        break;
      case ir::TypeKind::SequenceOf:
        out << "struct " << name << " {\n"
            << "  std::vector<" << type_cpp(t.sequence_of.element) << "> value;\n"
            << "};\n";
        break;
      case ir::TypeKind::SetOf:
        out << "struct " << name << " {\n"
            << "  std::vector<" << type_cpp(t.set_of.element) << "> value;\n"
            << "};\n";
        break;
      case ir::TypeKind::Enumerated: {
        out << "enum class " << name << " : std::int64_t {\n";
        for (const auto& nn : t.enumerated.root) {
          out << "  " << cpp_ident(nn.name);
          if (nn.value.as_i64) {
            out << " = " << *nn.value.as_i64;
          }
          out << ",\n";
        }
        for (const auto& nn : t.enumerated.extensions) {
          out << "  " << cpp_ident(nn.name);
          if (nn.value.as_i64) {
            out << " = " << *nn.value.as_i64;
          }
          out << ",\n";
        }
        out << "};\n";
        break;
      }
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        emit_distinct_alias(out, name, "std::vector<std::uint64_t>", AliasShape::Class);
        break;
      case ir::TypeKind::ObjectClassField:
        if (t.object_class_field.open_type) {
          emit_distinct_alias(out, name, "std::vector<std::uint8_t>", AliasShape::Class);
        } else {
          const ir::Type& fixed = resolve(model_, t.object_class_field.fixed_type);
          const std::string host = type_cpp(t.object_class_field.fixed_type);
          // Inherit a named class so this name stays distinct from the governor
          // type. Enumerations cannot be base classes; hold them by value.
          AliasShape shape = alias_shape(fixed);
          if (is_named(fixed) && fixed.kind != ir::TypeKind::Enumerated) {
            shape = AliasShape::Class;
          }
          emit_distinct_alias(out, name, host, shape);
        }
        break;
      case ir::TypeKind::Real:
        emit_distinct_alias(out, name, "double", AliasShape::Scalar);
        break;
      case ir::TypeKind::InstanceOf:
        out << "struct " << name << " {\n"
            << "  std::vector<std::uint64_t> type_id;\n"
            << "  std::vector<std::uint8_t> value;\n"
            << "};\n";
        break;
      case ir::TypeKind::Set: {
        out << "struct " << name << " {\n";
        emit_struct_fields(out, t.set.root, false);
        for (const auto& group : t.set.extension_groups) {
          emit_struct_fields(out, group.fields, true);
        }
        emit_struct_fields(out, t.set.trailing_root, false);
        emit_table_constraint_structs(out, t.set.root);
        for (const auto& group : t.set.extension_groups) {
          emit_table_constraint_structs(out, group.fields);
        }
        emit_table_constraint_structs(out, t.set.trailing_root);
        out << "};\n";
        break;
      }
      case ir::TypeKind::Sequence: {
        out << "struct " << name << " {\n";
        emit_struct_fields(out, t.sequence.root, false);
        for (const auto& group : t.sequence.extension_groups) {
          emit_struct_fields(out, group.fields, true);
        }
        emit_struct_fields(out, t.sequence.trailing_root, false);
        emit_table_constraint_structs(out, t.sequence.root);
        for (const auto& group : t.sequence.extension_groups) {
          emit_table_constraint_structs(out, group.fields);
        }
        emit_table_constraint_structs(out, t.sequence.trailing_root);
        out << "};\n";
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(t.choice);
        out << "struct " << name << " {\n";
        for (const ir::Field* f : all) {
          out << "  struct " << cpp_ident(f->name) << "_ {\n"
              << "    " << type_cpp(f->type) << " value;\n"
              << "  };\n";
        }
        out << "  std::variant<";
        for (std::size_t i = 0; i < all.size(); ++i) {
          if (i) {
            out << ", ";
          }
          out << cpp_ident(all[i]->name) << "_";
        }
        out << "> alt;\n};\n";
        break;
      }
      case ir::TypeKind::Referenced:
        if (t.referenced.resolved != ir::kInvalidType) {
          out << "using " << name << " = " << type_cpp(t.referenced.resolved) << ";\n";
        } else {
          diag_.error({}, "unresolved type alias '" + name + "'");
          out << "using " << name << " = void;\n";
        }
        break;
    }
  }

  /**
   *  Function    : choice_all_fields
   *  Description : Computes choice all fields from (c).
   *  Parameters  : c — const ir::ChoiceDesc& c
   *  Returns     : static std::vector<const ir::Field*>
   */
std::vector<const ir::Field*> Emitter::choice_all_fields(const ir::ChoiceDesc& c) {
    std::vector<const ir::Field*> all;
    all.reserve(c.alternatives.size() + c.extensions.size());
    for (const ir::Field& f : c.alternatives) {
      all.push_back(&f);
    }
    for (const ir::Field& f : c.extensions) {
      all.push_back(&f);
    }
    return all;
  }

  /**
   *  Function    : field_has_presence_bit
   *  Description : Returns a boolean result from f.
   *  Parameters  : f — const ir::Field& f
   *  Returns     : static bool
   */
bool Emitter::field_has_presence_bit(const ir::Field& f) {
    return f.presence == ir::Presence::Optional || f.presence == ir::Presence::Default;
  }

  /**
   *  Function    : field_wraps_optional
   *  Description : Returns a boolean result from f, force_optional.
   *  Parameters  : f — const ir::Field& f; force_optional — bool force_optional
   *  Returns     : static bool
   */
bool Emitter::field_wraps_optional(const ir::Field& f, bool force_optional) {
    if (f.presence == ir::Presence::Default) {
      return false;
    }
    return force_optional || f.presence == ir::Presence::Optional;
  }

void Emitter::emit_ext_group_field_present(std::ostream& out, const ir::Field& f,
                                             const std::string& expr) {
    if (f.presence == ir::Presence::Default) {
      out << "true";
    } else if (f.presence == ir::Presence::Optional) {
      out << expr << "." << cpp_ident(f.name) << ".has_value()";
    } else {
      out << "true";
    }
  }

  /**
   *  Function    : default_val_cpp
   *  Description : Builds and returns a string for default val cpp.
   *  Parameters  : vid — ir::ValueId vid
   *  Returns     : std::string
   */
std::string Emitter::default_val_cpp(const ir::Field& f) {
    if (!f.default_value) return "";
    const ir::Value& val = model_.arena.get_value(*f.default_value);
    std::string base;
    switch (val.kind) {
      case ir::ValueKind::Boolean:
        base = val.boolean ? "true" : "false";
        break;
      case ir::ValueKind::Integer:
        if (val.integer.as_i64) {
          base = std::to_string(*val.integer.as_i64);
        }
        break;
      case ir::ValueKind::String: {
        // The IR may carry character-string literals with their quotes
        // still attached (lexer capture); normalise before wrapping.
        std::string s = val.text;
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
          s = s.substr(1, s.size() - 2);
        }
        base = "\"" + s + "\"";
        break;
      }
      default:
        break;
    }
    if (base.empty()) return "";
    // An unconstrained INTEGER member is BigInteger: a bare int literal
    // neither converts to it nor compares with it.
    const ir::Type& ft = resolve(model_, f.type);
    if (ft.kind == ir::TypeKind::Integer && integer_is_bigint(ft.integer)) {
      return "asn1::BigInteger::from_i64(" + base + ")";
    }
    return base;
  }

void Emitter::emit_struct_fields(std::ostream& out, const std::vector<ir::Field>& fields,
                          bool force_optional) {
    for (const ir::Field& f : fields) {
      const std::string field_ty = type_cpp(f.type);
      const std::string field_name = cpp_ident(f.name);
      if (field_wraps_optional(f, force_optional)) {
        out << "  std::optional<" << field_ty << "> " << field_name << ";\n";
      } else {
        std::string def_init;
        if (f.presence == ir::Presence::Default) {
          std::string v = default_val_cpp(f);
          if (!v.empty()) {
            def_init = " = " + v;
          }
        }
        out << "  " << field_ty << " " << field_name << def_init << ";\n";
      }
    }
  }

  /**
   *  Function    : type_is_constructed
   *  Description : Returns a boolean result from t.
   *  Parameters  : t — const ir::Type& t
   *  Returns     : static bool
   */
bool Emitter::type_is_constructed(const ir::Type& t) {
    switch (t.kind) {
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set:
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf:
      case ir::TypeKind::Choice:
      case ir::TypeKind::InstanceOf:
        return true;
      default:
        return false;
    }
  }

  /**
   *  Function    : structural_children
   *  Description : Direct child type ids of a type: field/element/
   *                alternative/table-entry/resolved-alias/fixed-governor
   *                types — the places where a cross-module named reference
   *                can hide.
   *  Parameters  : t — const ir::Type& t
   *  Returns     : std::vector<ir::TypeId>
   */
std::vector<ir::TypeId> structural_children(const ir::Type& t) {
    std::vector<ir::TypeId> out;
    auto add_field = [&out](const ir::Field& f) {
      out.push_back(f.type);
      if (f.table_constraint) {
        for (const auto& e : f.table_constraint->entries) {
          out.push_back(e.type);
        }
      }
    };
    switch (t.kind) {
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        const ir::SequenceDesc& d = (t.kind == ir::TypeKind::Sequence) ? t.sequence : t.set;
        for (const auto& f : d.root) add_field(f);
        for (const auto& g : d.extension_groups) {
          for (const auto& f : g.fields) add_field(f);
        }
        for (const auto& f : d.trailing_root) add_field(f);
        break;
      }
      case ir::TypeKind::Choice:
        for (const auto& f : t.choice.alternatives) out.push_back(f.type);
        for (const auto& f : t.choice.extensions) out.push_back(f.type);
        break;
      case ir::TypeKind::SequenceOf:
        out.push_back(t.sequence_of.element);
        break;
      case ir::TypeKind::SetOf:
        out.push_back(t.set_of.element);
        break;
      case ir::TypeKind::Referenced:
        out.push_back(t.referenced.resolved);
        break;
      case ir::TypeKind::ObjectClassField:
        out.push_back(t.object_class_field.fixed_type);
        break;
      default:
        break;
    }
    return out;
  }

  /**
   *  Function    : collect_module_groups
   *  Description : Performs collect module groups (definition). Split mode:
   *                groups named types by ASN.1 module, assigns a unique C++
   *                namespace ident per module, records cross-module include
   *                dependencies, and forms emit units: one module per unit,
   *                except mutually recursive (strongly connected) modules
   *                which must share one header/.cpp pair — a by-value
   *                cross-module member needs the referenced type complete,
   *                which separate headers cannot provide for a cycle.
   *  Parameters  : none
   *  Returns     : void
   */
void Emitter::collect_module_groups() {
    module_ns_.clear();
    module_types_.clear();
    module_deps_.clear();
    unit_modules_.clear();
    unit_files_.clear();
    unit_deps_.clear();
    std::unordered_set<std::string> used_ns;
    std::vector<std::string> first_seen;
    for (ir::TypeId id : named_order_) {
      const ir::Type& t = model_.arena.get(id);
      auto [it, inserted] = module_types_.try_emplace(t.module);
      if (inserted) {
        first_seen.push_back(t.module);
        // Module names are distinct ASN.1 identifiers but can map to the
        // same C++ ident (`A-B` vs `A_B`); keep both namespace and file
        // name unique.
        const std::string base_ns = cpp_ident(t.module);
        std::string candidate = base_ns;
        for (int n = 2; used_ns.count(candidate) != 0; ++n) {
          candidate = base_ns + "_" + std::to_string(n);
        }
        used_ns.insert(candidate);
        module_ns_[t.module] = candidate;
      }
      it->second.push_back(id);
    }

    // Cross-module include dependencies: walk each named type's structural
    // children (recursing through unnamed/inline types only) and record the
    // module of every named type referenced. Stopping at named types is
    // correct: a referenced named type carries its own dependencies, and
    // its module header is included transitively.
    for (const auto& [mod, ids] : module_types_) {
      std::unordered_set<std::string> deps;
      for (ir::TypeId id : ids) {
        std::unordered_set<ir::TypeId> visited;
        std::function<void(ir::TypeId)> walk = [&](ir::TypeId c) {
          if (c == ir::kInvalidType || visited.count(c) != 0) {
            return;
          }
          visited.insert(c);
          const ir::Type& ct = model_.arena.get(c);
          if (named_set_.count(c) != 0) {
            if (!ct.module.empty() && ct.module != mod) {
              deps.insert(ct.module);
            }
            return;  // named type: its own header carries its dependencies
          }
          for (ir::TypeId child : structural_children(ct)) {
            walk(child);
          }
        };
        for (ir::TypeId child : structural_children(model_.arena.get(id))) {
          walk(child);
        }
      }
      std::vector<std::string>& dep_list = module_deps_[mod];
      dep_list.assign(deps.begin(), deps.end());
      std::sort(dep_list.begin(), dep_list.end());
    }

    // Strongly connected components over the module dependency graph.
    // reach1[m] = modules reachable from m through at least one edge.
    std::unordered_map<std::string, std::unordered_set<std::string>> reach1;
    for (const std::string& m : first_seen) {
      std::unordered_set<std::string> seen;
      std::vector<std::string> stack(module_deps_[m].begin(), module_deps_[m].end());
      while (!stack.empty()) {
        const std::string x = stack.back();
        stack.pop_back();
        if (seen.insert(x).second == false) {
          continue;
        }
        for (const std::string& d : module_deps_[x]) {
          stack.push_back(d);
        }
      }
      reach1[m] = std::move(seen);
    }
    std::unordered_map<std::string, std::size_t> unit_of;
    for (const std::string& m : first_seen) {
      if (unit_of.count(m) != 0) {
        continue;
      }
      std::vector<std::string> members;
      for (const std::string& n : first_seen) {
        const bool same = (n == m) || (reach1[m].count(n) != 0 && reach1[n].count(m) != 0);
        if (same) {
          members.push_back(n);
          unit_of[n] = unit_modules_.size();
        }
      }
      unit_modules_.push_back(std::move(members));
      unit_files_.push_back(module_ns_[m]);
      // External dependencies: union of member deps minus the unit's own
      // members (a module of an earlier unit is still external here).
      const std::unordered_set<std::string> member_set(unit_modules_.back().begin(),
                                                       unit_modules_.back().end());
      std::unordered_set<std::string> ext;
      for (const std::string& member : member_set) {
        for (const std::string& d : module_deps_[member]) {
          if (member_set.count(d) == 0) {
            ext.insert(d);
          }
        }
      }
      std::vector<std::string> ext_list(ext.begin(), ext.end());
      std::sort(ext_list.begin(), ext_list.end());
      unit_deps_.push_back(std::move(ext_list));
    }

    // Dependency-first unit order (DFS; cycles are already merged into
    // single units so this graph is acyclic).
    std::vector<std::size_t> order;
    std::unordered_set<std::size_t> done;
    std::function<void(std::size_t)> visit = [&](std::size_t u) {
      if (done.insert(u).second == false) {
        return;
      }
      for (const std::string& d : unit_deps_[u]) {
        visit(unit_of[d]);
      }
      order.push_back(u);
    };
    for (std::size_t u = 0; u < unit_modules_.size(); ++u) {
      visit(u);
    }
    std::vector<std::vector<std::string>> modules_by_order;
    std::vector<std::string> files_by_order;
    std::vector<std::vector<std::string>> deps_by_order;
    for (std::size_t u : order) {
      modules_by_order.push_back(std::move(unit_modules_[u]));
      files_by_order.push_back(std::move(unit_files_[u]));
      deps_by_order.push_back(std::move(unit_deps_[u]));
    }
    unit_modules_ = std::move(modules_by_order);
    unit_files_ = std::move(files_by_order);
    unit_deps_ = std::move(deps_by_order);
  }

  /**
   *  Function    : run_split
   *  Description : Performs run split (definition). Writes one
   *                `<Unit>.hpp/.cpp` pair per emit unit plus the aggregate
   *                header into dir. Single-module units keep one module
   *                namespace; merged (mutually recursive) units emit their
   *                types in dependency-first order, reopening each type's
   *                own module namespace as it changes.
   *  Parameters  : dir — const std::filesystem::path& dir
   *  Returns     : void
   */
void Emitter::run_split(const std::filesystem::path& dir) {
    cache_reset_stats();
    collect_named();
    collect_module_groups();
    const std::string ns = cpp_ident(options_.namespace_name);
    const std::string base = cpp_ident(options_.basename);

    for (std::size_t u = 0; u < unit_modules_.size(); ++u) {
      const std::vector<std::string>& members = unit_modules_[u];
      const std::string& file_base = unit_files_[u];
      const std::vector<std::string>& deps = unit_deps_[u];
      const std::string guard = "ASN1_GEN_" + ns + "_" + file_base + "_HPP_";

      // Types of the unit in named_order_ order (dependency-first).
      std::vector<ir::TypeId> types;
      for (ir::TypeId id : named_order_) {
        const std::string& mod = model_.arena.get(id).module;
        for (const std::string& member : members) {
          if (mod == member) {
            types.push_back(id);
            break;
          }
        }
      }

      // P2-3: cache check per unit. The unit name is the file basename
      // (stable across runs and matches what `unit_files_[u]` already
      // stores). When the key + every output's on-disk hash match, skip
      // the entire emit for this unit — the previous files are reused
      // verbatim and the unit's mtime / filesystem entries stay untouched.
      const std::filesystem::path hpp_path = dir / (file_base + ".hpp");
      const std::filesystem::path cpp_path = dir / (file_base + ".cpp");
      const std::vector<std::filesystem::path> unit_outputs{hpp_path.filename(),
                                                            cpp_path.filename()};
      if (cache_check_unit(file_base, unit_outputs)) {
        continue;
      }
      std::ofstream header(hpp_path);
      std::ofstream source(cpp_path);
      if (!header || !source) {
        diag_.error({}, "failed to open split emit outputs in " + dir.string());
        return;
      }

      const bool merged = members.size() > 1;
      source << "// Generated by asn1cxx - definitions TU for "
             << (merged ? "modules " : "module ") << members.front()
             << (merged ? " (+mutually recursive modules)" : "")
             << " (see " << file_base << ".hpp for declarations).\n"
             << "#include \"" << file_base << ".hpp\"\n\n"
             << "namespace " << ns << " {\n\n";

      header << "// Generated by asn1cxx - "
             << (merged ? "modules " : "module ") << members.front()
             << (merged ? " (+mutually recursive modules)" : "")
             << " (do not edit).\n"
             << "#pragma once\n\n"
             << "#ifndef " << guard << "\n"
             << "#define " << guard << "\n\n";
      emit_includes(header);
      for (const std::string& dep : deps) {
        header << "#include \"" << module_ns_[dep] << ".hpp\"\n";
      }
      header << "\n"
             << "#include <cstdint>\n"
             << "#include <optional>\n"
             << "#include <string>\n"
             << "#include <type_traits>\n"
             << "#include <utility>\n"
             << "#include <variant>\n"
             << "#include <vector>\n\n"
             << "namespace " << ns << " {\n";

      def_out_ = &source;
      if (!merged) {
        // Single-module unit: one module namespace, three-pass emission
        // (forward declarations, declarations, codecs).
        const std::string& mns = module_ns_[members.front()];
        header << "namespace " << mns << " {\n\n"
               << "namespace detail {\n"
               << "struct Null {};\n"
               << "}  // namespace detail\n\n";
        source << "namespace " << mns << " {\n\n";
        current_module_ = members.front();
        emit_types_and_codecs(header, types);
        current_module_.clear();
        header << "}  // namespace " << mns << "\n";
        source << "}  // namespace " << mns << "\n";
      } else {
        // Merged unit: forward declarations grouped by module, then
        // declarations + codecs in dependency-first order with the module
        // namespace reopened whenever it changes. Each module's
        // `detail::Null` is defined once, at its first namespace opening.
        header << "\n";
        for (const std::string& member : members) {
          const std::string& mns = module_ns_[member];
          header << "namespace " << mns << " {\n";
          for (ir::TypeId id : types) {
            const ir::Type& t = model_.arena.get(id);
            if (t.module != member) {
              continue;
            }
            if (t.kind == ir::TypeKind::Sequence || t.kind == ir::TypeKind::Set ||
                t.kind == ir::TypeKind::Choice) {
              header << "struct " << named_type_cpp(t) << ";\n";
            }
          }
          header << "}  // namespace " << mns << "\n";
        }
        header << "\n";

        std::string open_module;
        std::unordered_set<std::string> detail_done;
        auto close_ns = [&]() {
          if (open_module.empty()) {
            return;
          }
          const std::string& mns = module_ns_[open_module];
          header << "}  // namespace " << mns << "\n";
          source << "}  // namespace " << mns << "\n";
          open_module.clear();
        };
        auto open_ns = [&](const std::string& mod) {
          if (mod == open_module) {
            return;
          }
          close_ns();
          open_module = mod;
          const std::string& mns = module_ns_[mod];
          header << "namespace " << mns << " {\n\n";
          source << "namespace " << mns << " {\n\n";
          if (detail_done.insert(mod).second) {
            header << "namespace detail {\n"
                   << "struct Null {};\n"
                   << "}  // namespace detail\n\n";
          }
        };
        for (ir::TypeId id : types) {
          const std::string& mod = model_.arena.get(id).module;
          open_ns(mod);
          current_module_ = mod;
          emit_type_decl(header, id);
          header << "\n";
          emit_type_codecs(header, id);
        }
        current_module_.clear();
        close_ns();
      }
      def_out_ = nullptr;

      header << "}  // namespace " << ns << "\n\n"
             << "#endif  // " << guard << "\n";
      source << "}  // namespace " << ns << "\n";
      // Flush + sync before hashing the on-disk bytes; an unfilled
      // std::ofstream buffer reads back as a zero-byte file and poisons
      // the cache entry (P2-3 first iteration regression).
      header.flush();
      source.flush();
      cache_record_unit(file_base, unit_outputs);
    }

    // Aggregate header: one include pulls in every unit.
    const std::filesystem::path agg_path = dir / (base + ".hpp");
    std::ofstream agg(agg_path);
    if (!agg) {
      diag_.error({}, "failed to open aggregate header " + agg_path.string());
      return;
    }
    const std::string aguard = "ASN1_GEN_" + ns + "_" + base + "_HPP_";
    agg << "// Generated by asn1cxx - aggregate header (do not edit).\n"
        << "#pragma once\n\n"
        << "#ifndef " << aguard << "\n"
        << "#define " << aguard << "\n\n";
    for (const std::string& f : unit_files_) {
      agg << "#include \"" << f << ".hpp\"\n";
    }
    agg << "\n#endif  // " << aguard << "\n";
    cache_finalise();
  }

void CppGenerator::emit(const ir::Model& model, const EmitOptions& options, Diagnostics& diag,
                        std::ostream& header, std::ostream* source) {
  Emitter emitter(model, options, diag);
  emitter.run(header, source);
}

void CppGenerator::emit_split(const ir::Model& model, const EmitOptions& options,
                              Diagnostics& diag, const std::filesystem::path& dir) {
  // Copy options so we can attach the dir without mutating the caller's
  // struct (caller might pass a temporary, const-ref, or reused instance).
  EmitOptions opts = options;
  opts.emit_dir = dir;
  Emitter emitter(model, opts, diag);
  emitter.run_split(dir);
}

void CppGenerator::emit_to_dir(const ir::Model& model, const EmitOptions& options,
                                Diagnostics& diag, const std::filesystem::path& dir) {
  EmitOptions opts = options;
  opts.emit_dir = dir;
  Emitter emitter(model, opts, diag);
  emitter.run_to_dir(dir);
}

std::string CppGenerator::emit_header_string(const ir::Model& model, const EmitOptions& options,
                                             Diagnostics& diag) {
  std::ostringstream os;
  emit(model, options, diag, os, nullptr);
  return os.str();
}

// --- P2-3 content-hash cache helpers ---

namespace {

// Format a wall-time nanoseconds count as "X.XXs" (seconds, 2 decimals).
std::string format_seconds(std::chrono::nanoseconds ns) {
  const double seconds = static_cast<double>(ns.count()) / 1e9;
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.2fs", seconds);
  return buf;
}

}  // namespace

std::vector<std::string> Emitter::cache_extra_inputs() const {
  if (cache_extras_valid_) {
    return cache_extras_cached_;
  }
  std::vector<std::string>& out = cache_extras_cached_;
  // Codec bitmask + basename + namespace_name + emit_inline + emit_split
  // distinguish every distinct emit. cli_version is a forward-compat hook:
  // if the cache format changes, bump this and old entries naturally miss.
  out.push_back("codec=" + std::to_string(static_cast<unsigned>(options_.codec)));
  out.push_back("basename=" + options_.basename);
  out.push_back("namespace=" + options_.namespace_name);
  out.push_back("emit_inline=" + std::to_string(options_.emit_inline ? 1 : 0));
  out.push_back("emit_split=" + std::to_string(options_.emit_split ? 1 : 0));
  out.push_back("cli_version=asn1cxx-0.1.0");
  cache_extras_valid_ = true;
  return out;
}

std::string Emitter::cache_source_fingerprint() const {
  if (cache_source_fp_valid_) {
    return cache_source_fp_cached_;
  }
  // Force the per-source sha256 map to populate alongside this call so we
  // share one I/O pass over the source files between fingerprinting and
  // the subsequent per-unit compute_key() calls.
  (void)cache_source_hashes();
  if (options_.source_paths.empty()) {
    cache_source_fp_cached_ = "src:empty";
  } else {
    std::vector<std::filesystem::path> sorted(options_.source_paths.begin(),
                                              options_.source_paths.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const std::filesystem::path& a, const std::filesystem::path& b) {
                return a.string() < b.string();
              });
    std::string joined;
    joined.reserve(64 * sorted.size());
    for (const auto& p : sorted) {
      const std::string& h = cache_source_hashes_cached_.at(p.string());
      joined.append(p.string());
      joined.push_back('=');
      joined.append(h.empty() ? std::string("unreadable") : h);
      joined.push_back('|');
    }
    cache_source_fp_cached_ = "srcs=" + sha256_hex(joined);
  }
  cache_source_fp_valid_ = true;
  return cache_source_fp_cached_;
}

const std::unordered_map<std::string, std::string>& Emitter::cache_source_hashes() const {
  if (cache_source_hashes_valid_) {
    return cache_source_hashes_cached_;
  }
  for (const auto& p : options_.source_paths) {
    cache_source_hashes_cached_[p.string()] = sha256_file_hex(p);
  }
  cache_source_hashes_valid_ = true;
  return cache_source_hashes_cached_;
}

ContentCache* Emitter::cache_init() {
  if (cache_) {
    return cache_.get();
  }
  if (!options_.emit_cache) {
    return nullptr;
  }
  if (options_.emit_dir.empty()) {
    return nullptr;
  }
  cache_ = std::make_unique<ContentCache>(options_.emit_dir);
  return cache_.get();
}

void Emitter::cache_reset_stats() {
  cache_units_total_ = 0;
  cache_units_hit_ = 0;
  cache_miss_wall_ = std::chrono::nanoseconds{0};
  cache_extras_valid_ = false;
  cache_extras_cached_.clear();
  cache_source_fp_valid_ = false;
  cache_source_fp_cached_.clear();
  cache_source_hashes_valid_ = false;
  cache_source_hashes_cached_.clear();
}

bool Emitter::cache_check_unit(const std::string& unit_name,
                               const std::vector<std::filesystem::path>& expected_outputs) {
  ContentCache* cache = cache_init();
  if (!cache) {
    return false;
  }
  cache_units_total_++;
  const CacheEntry* entry = cache->lookup(unit_name);
  if (!entry) {
    return false;
  }
  // Re-derive the key from the current sources + emit options and compare.
  // `cache_source_hashes()` is memoised so the same .asn file is read+SHA-256'd
  // at most once across every per-unit call in this run.
  const std::vector<std::string> extras_buf = cache_extra_inputs();
  std::vector<std::string> extras = extras_buf;
  extras.push_back(cache_source_fingerprint());
  const std::string key =
      cache->compute_key(unit_name, options_.source_paths, extras, cache_source_hashes());
  if (entry->key != key) {
    return false;
  }
  // Verify every expected output is on disk and its hash matches what we
  // recorded last time. A manual edit or partial write changes the hash
  // and we miss (forcing a clean re-emit).
  if (entry->outputs.size() != expected_outputs.size()) {
    return false;
  }
  for (const auto& rel : expected_outputs) {
    const std::string rel_str = rel.string();
    auto it = entry->outputs.find(rel_str);
    if (it == entry->outputs.end()) {
      return false;
    }
    const std::string actual = ContentCache::hash_output_file(options_.emit_dir, rel);
    if (actual.empty() || actual != it->second) {
      return false;
    }
  }
  cache_units_hit_++;
  return true;
}

void Emitter::cache_record_unit(const std::string& unit_name,
                                const std::vector<std::filesystem::path>& outputs) {
  ContentCache* cache = cache_init();
  if (!cache) {
    return;
  }
  const std::vector<std::string> extras_buf = cache_extra_inputs();
  std::vector<std::string> extras = extras_buf;
  extras.push_back(cache_source_fingerprint());
  const std::string key =
      cache->compute_key(unit_name, options_.source_paths, extras, cache_source_hashes());
  CacheEntry e;
  e.key = key;
  for (const auto& rel : outputs) {
    const std::string rel_str = rel.string();
    const std::string h = ContentCache::hash_output_file(options_.emit_dir, rel);
    if (!h.empty()) {
      e.outputs[rel_str] = h;
    }
  }
  cache->store(unit_name, std::move(e));
}

void Emitter::cache_finalise() {
  if (!cache_) {
    return;
  }
  cache_->flush();
  // Diagnostic note: counts + wall-time split. Severity::Note so it does
  // not affect Diagnostics::ok() (caller still succeeds on a 100% hit).
  std::ostringstream msg;
  msg << "emit cache: " << cache_units_hit_ << "/" << cache_units_total_ << " units hit ("
      << format_seconds(std::chrono::nanoseconds{0}) << "), "
      << (cache_units_total_ - cache_units_hit_) << " missed ("
      << format_seconds(cache_miss_wall_) << ")";
  diag_.note({}, msg.str());
}

}  // namespace codegen
}  // namespace asn1
