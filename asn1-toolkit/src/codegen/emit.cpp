#include <asn1/codegen/emit.hpp>

#include <algorithm>
#include <functional>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace asn1 {
namespace codegen {
namespace {

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

const ir::Type& resolve(const ir::Model& model, ir::TypeId id) {
  const ir::Type* t = &model.arena.get(id);
  while (t->kind == ir::TypeKind::Referenced && t->referenced.resolved != ir::kInvalidType) {
    t = &model.arena.get(t->referenced.resolved);
  }
  return *t;
}

bool is_named(const ir::Type& t) { return !t.name.empty(); }

std::string named_type_cpp(const ir::Type& t) { return cpp_ident(t.name); }

bool integer_is_bigint(const ir::IntegerDesc& d) { return !d.host_bits.has_value(); }

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

std::string opt_i64_expr(const std::optional<ir::BigInt>& b) {
  if (b && b->as_i64) {
    return "std::int64_t{" + std::to_string(*b->as_i64) + "}";
  }
  return "std::nullopt";
}

std::string integer_constraint_expr(const ir::IntegerDesc& d) {
  std::ostringstream os;
  os << "asn1::per::IntegerConstraint{" << opt_i64_expr(d.constraint.lower) << ", "
     << opt_i64_expr(d.constraint.upper) << ", "
     << (d.constraint.extensible ? "true" : "false");
  if (d.constraint.root_ranges.size() > 1) {
    os << ", {";
    for (std::size_t i = 0; i < d.constraint.root_ranges.size(); ++i) {
      if (i > 0) os << ", ";
      os << "asn1::per::IntegerRange{" << opt_i64_expr(d.constraint.root_ranges[i].lower) << ", "
         << opt_i64_expr(d.constraint.root_ranges[i].upper) << "}";
    }
    os << "}";
  }
  os << "}";
  return os.str();
}

std::string opt_size_expr(const std::optional<ir::BigInt>& b) {
  if (b && b->as_i64 && *b->as_i64 >= 0) {
    return "std::size_t{" + std::to_string(static_cast<std::uint64_t>(*b->as_i64)) + "}";
  }
  return "std::nullopt";
}

std::string size_constraint_expr(const ir::ConstraintDesc& d) {
  std::ostringstream os;
  os << "asn1::per::SizeConstraint{" << opt_size_expr(d.lower) << ", "
     << opt_size_expr(d.upper) << ", " << (d.extensible ? "true" : "false");
  if (d.root_ranges.size() > 1) {
    os << ", {";
    for (std::size_t i = 0; i < d.root_ranges.size(); ++i) {
      if (i > 0) os << ", ";
      os << "asn1::per::SizeRange{" << opt_size_expr(d.root_ranges[i].lower) << ", "
         << opt_size_expr(d.root_ranges[i].upper) << "}";
    }
    os << "}";
  }
  os << "}";
  return os.str();
}

class Emitter {
 public:
  Emitter(const ir::Model& model, EmitOptions options, Diagnostics& diag)
      : model_(model), options_(std::move(options)), diag_(diag) {}

  void run(std::ostream& header, std::ostream* source) {
    collect_named();
    emit_header(header);
    if (source) {
      *source << "// Generated by asn1cxx - companion TU (types are header-only).\n"
              << "#include \"" << options_.basename << ".hpp\"\n";
    }
  }

 private:
  const ir::Model& model_;
  EmitOptions options_;
  Diagnostics& diag_;
  std::vector<ir::TypeId> named_order_;
  std::unordered_set<ir::TypeId> named_set_;

  void collect_named() {
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
        if (e.name == t.name && e.module == t.module) {
          named_set_.insert(id);
          return;
        }
      }
      visiting.insert(id);
      auto dep = [&](ir::TypeId d) {
        if (d == ir::kInvalidType) {
          return;
        }
        // Prefer the named target of aliases / resolved refs.
        const ir::Type& dt = model_.arena.get(d);
        if (dt.kind == ir::TypeKind::Referenced &&
            dt.referenced.resolved != ir::kInvalidType) {
          consider(dt.referenced.resolved);
        }
        consider(d);
      };
      if (t.kind == ir::TypeKind::Sequence) {
        for (const auto& f : t.sequence.root) {
          dep(f.type);
        }
        for (const auto& g : t.sequence.extension_groups) {
          for (const auto& f : g) {
            dep(f.type);
          }
        }
        for (const auto& f : t.sequence.trailing_root) {
          dep(f.type);
        }
      } else if (t.kind == ir::TypeKind::Set) {
        for (const auto& f : t.set.root) {
          dep(f.type);
        }
        for (const auto& g : t.set.extension_groups) {
          for (const auto& f : g) {
            dep(f.type);
          }
        }
        for (const auto& f : t.set.trailing_root) {
          dep(f.type);
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

  std::string type_cpp(ir::TypeId id, bool for_optional = false) {
    if (named_set_.count(id)) {
      return named_type_cpp(model_.arena.get(id));
    }
    const ir::Type& t = resolve(model_, id);
    if (is_named(t)) {
      for (ir::TypeId nid : named_order_) {
        const ir::Type& n = model_.arena.get(nid);
        if (n.name == t.name && n.module == t.module) {
          return named_type_cpp(n);
        }
      }
      return named_type_cpp(t);
    }
    return inline_type_cpp(t, for_optional);
  }

  std::string inline_type_cpp(const ir::Type& t, bool /*for_optional*/) {
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        return "bool";
      case ir::TypeKind::Null:
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

  void emit_header(std::ostream& out) {
    const std::string ns = cpp_ident(options_.namespace_name);
    const std::string guard =
        "ASN1_GEN_" + ns + "_" + cpp_ident(options_.basename) + "_HPP_";
    out << "// Generated by asn1cxx - do not edit.\n"
        << "#pragma once\n\n"
        << "#ifndef " << guard << "\n"
        << "#define " << guard << "\n\n";

    const bool emit_jer = options_.codec == CodecKind::Jer;
    const bool emit_xml = options_.codec == CodecKind::Xer ||
                          options_.codec == CodecKind::Exer ||
                          options_.codec == CodecKind::Cxer;
    const bool emit_per = options_.codec == CodecKind::Uper ||
                          options_.codec == CodecKind::Aper ||
                          options_.codec == CodecKind::Both;
    const bool emit_oer =
        options_.codec == CodecKind::Oer || options_.codec == CodecKind::Coer;
    const bool emit_tlv =
        options_.codec == CodecKind::Ber || options_.codec == CodecKind::Der;
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
    if (options_.codec == CodecKind::Uper || options_.codec == CodecKind::Both) {
      out << "#include <asn1/runtime/uper.hpp>\n";
    }
    if (options_.codec == CodecKind::Aper || options_.codec == CodecKind::Both) {
      out << "#include <asn1/runtime/aper.hpp>\n";
    }
    if (emit_jer) {
      out << "#include <asn1/runtime/jer/json.hpp>\n"
          << "#include <asn1/runtime/jer/codec.hpp>\n"
          << "#include <asn1/runtime/jeri/codec.hpp>\n";
    }
    if (emit_xml) {
      out << "#include <asn1/runtime/xer/codec.hpp>\n";
      if (options_.codec == CodecKind::Exer) {
        out << "#include <asn1/runtime/exer/codec.hpp>\n";
      }
      if (options_.codec == CodecKind::Cxer) {
        out << "#include <asn1/runtime/cxer/codec.hpp>\n";
      }
    }
    if (options_.codec == CodecKind::Oer) {
      out << "#include <asn1/runtime/oer/codec.hpp>\n";
    }
    if (options_.codec == CodecKind::Coer) {
      out << "#include <asn1/runtime/coer/codec.hpp>\n";
    }
    if (emit_tlv) {
      out << "#include <asn1/runtime/ber/tlv.hpp>\n"
          << "#include <asn1/runtime/ber/codec.hpp>\n";
      if (options_.codec == CodecKind::Der) {
        out << "#include <asn1/runtime/der/codec.hpp>\n";
      }
    }
    out << "\n"
        << "#include <cstdint>\n"
        << "#include <optional>\n"
        << "#include <string>\n"
        << "#include <type_traits>\n"
        << "#include <variant>\n"
        << "#include <vector>\n\n"
        << "namespace " << ns << "_detail {\n"
        << "struct Null {};\n"
        << "}  // namespace " << ns << "_detail\n\n"
        << "namespace " << ns << " {\n\n";

    // Forward declarations for named sequence/choice.
    for (ir::TypeId id : named_order_) {
      const ir::Type& t = model_.arena.get(id);
      if (t.kind == ir::TypeKind::Sequence || t.kind == ir::TypeKind::Set ||
          t.kind == ir::TypeKind::Choice) {
        out << "struct " << named_type_cpp(t) << ";\n";
      }
    }
    out << "\n";

    for (ir::TypeId id : named_order_) {
      emit_type_decl(out, id);
      out << "\n";
    }

    for (ir::TypeId id : named_order_) {
      // Type aliases (`using A = B`) share B's codecs; do not emit duplicates.
      if (model_.arena.get(id).kind == ir::TypeKind::Referenced) {
        continue;
      }
      if (options_.codec == CodecKind::Uper || options_.codec == CodecKind::Both) {
        emit_codec_fns(out, id, /*aper=*/false);
      }
      if (options_.codec == CodecKind::Aper || options_.codec == CodecKind::Both) {
        emit_codec_fns(out, id, /*aper=*/true);
      }
      if (options_.codec == CodecKind::Jer) {
        emit_jer_codec_fns(out, id);
      }
      if (emit_xml) {
        emit_xer_codec_fns(out, id);
      }
      if (emit_oer) {
        emit_oer_codec_fns(out, id);
      }
      if (options_.codec == CodecKind::Ber) {
        emit_tlv_codec_fns(out, id, /*der=*/false);
      }
      if (options_.codec == CodecKind::Der) {
        emit_tlv_codec_fns(out, id, /*der=*/true);
      }
    }

    out << "}  // namespace " << cpp_ident(options_.namespace_name) << "\n\n"
        << "#endif  // " << guard << "\n";
  }

  void emit_type_decl(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        out << "using " << name << " = bool;\n";
        break;
      case ir::TypeKind::Null:
        out << "using " << name << " = " << cpp_ident(options_.namespace_name)
            << "_detail::Null;\n";
        break;
      case ir::TypeKind::Integer:
        out << "using " << name << " = " << integer_host_type(t.integer) << ";\n";
        break;
      case ir::TypeKind::OctetString:
        out << "using " << name << " = std::vector<std::uint8_t>;\n";
        break;
      case ir::TypeKind::BitString:
        out << "using " << name << " = asn1::BitStringValue;\n";
        break;
      case ir::TypeKind::String:
        out << "using " << name << " = std::string;\n";
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
        out << "using " << name << " = std::vector<std::uint64_t>;\n";
        break;
      case ir::TypeKind::ObjectClassField:
        if (t.object_class_field.open_type) {
          out << "using " << name << " = std::vector<std::uint8_t>;  // open type\n";
        } else {
          out << "using " << name << " = " << type_cpp(t.object_class_field.fixed_type) << ";\n";
        }
        break;
      case ir::TypeKind::Real:
        out << "using " << name << " = double;\n";
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
          emit_struct_fields(out, group, true);
        }
        emit_struct_fields(out, t.set.trailing_root, false);
        out << "};\n";
        break;
      }
      case ir::TypeKind::Sequence: {
        out << "struct " << name << " {\n";
        emit_struct_fields(out, t.sequence.root, false);
        for (const auto& group : t.sequence.extension_groups) {
          emit_struct_fields(out, group, true);
        }
        emit_struct_fields(out, t.sequence.trailing_root, false);
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

  const char* ns_codec(bool aper) const { return aper ? "asn1::aper" : "asn1::uper"; }
  const char* fn_suffix(bool aper) const { return aper ? "aper" : "uper"; }

  void emit_codec_fns(std::ostream& out, ir::TypeId id, bool aper) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    const std::string enc = std::string("encode_") + fn_suffix(aper);
    const std::string dec = std::string("decode_") + fn_suffix(aper);

    out << "inline asn1::Result<void> " << enc << "(asn1::BitWriter& w, const " << name
        << "& value) {\n";
    emit_encode_body(out, t, "value", aper, "  ");
    out << "  return asn1::Result<void>::success();\n}\n\n";

    // Out-parameter form so multiple named types can coexist (return-type-only
    // overloads are ill-formed in C++).
    out << "inline asn1::Result<void> " << dec << "(asn1::BitReader& r, " << name
        << "& value) {\n";
    emit_decode_body(out, t, "value", aper, "  ");
    out << "  return asn1::Result<void>::success();\n}\n\n";
  }


  static std::vector<std::int64_t> enum_root_sorted_values(const ir::EnumeratedDesc& e) {
    std::vector<std::int64_t> vals;
    vals.reserve(e.root.size());
    for (const auto& nn : e.root) {
      vals.push_back(nn.value.as_i64 ? *nn.value.as_i64 : 0);
    }
    std::sort(vals.begin(), vals.end());
    return vals;
  }

  static std::vector<std::int64_t> enum_extension_values(const ir::EnumeratedDesc& e) {
    std::vector<std::int64_t> vals;
    vals.reserve(e.extensions.size());
    for (const auto& nn : e.extensions) {
      vals.push_back(nn.value.as_i64 ? *nn.value.as_i64 : 0);
    }
    return vals;
  }

  static std::vector<const ir::Field*> choice_all_fields(const ir::ChoiceDesc& c) {
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

  static bool field_has_presence_bit(const ir::Field& f) {
    return f.presence == ir::Presence::Optional || f.presence == ir::Presence::Default;
  }

  static bool field_wraps_optional(const ir::Field& f, bool force_optional) {
    if (f.presence == ir::Presence::Default) {
      return false;
    }
    return force_optional || f.presence == ir::Presence::Optional;
  }

  static void emit_ext_group_field_present(std::ostream& out, const ir::Field& f,
                                             const std::string& expr) {
    if (f.presence == ir::Presence::Default) {
      out << "true";
    } else if (f.presence == ir::Presence::Optional) {
      out << expr << "." << cpp_ident(f.name) << ".has_value()";
    } else {
      out << "true";
    }
  }

  std::string default_val_cpp(ir::ValueId vid) {
    if (vid >= model_.arena.value_count()) return "";
    const ir::Value& val = model_.arena.get_value(vid);
    switch (val.kind) {
      case ir::ValueKind::Boolean:
        return val.boolean ? "true" : "false";
      case ir::ValueKind::Integer:
        if (val.integer.as_i64) {
          return std::to_string(*val.integer.as_i64);
        }
        break;
      case ir::ValueKind::String:
        return "\"" + val.text + "\"";
      default:
        break;
    }
    return "";
  }

  void emit_struct_fields(std::ostream& out, const std::vector<ir::Field>& fields,
                          bool force_optional) {
    for (const ir::Field& f : fields) {
      const std::string field_ty = type_cpp(f.type);
      const std::string field_name = cpp_ident(f.name);
      if (field_wraps_optional(f, force_optional)) {
        out << "  std::optional<" << field_ty << "> " << field_name << ";\n";
      } else {
        std::string def_init;
        if (f.presence == ir::Presence::Default && f.default_value) {
          std::string v = default_val_cpp(*f.default_value);
          if (!v.empty()) {
            def_init = " = " + v;
          }
        }
        out << "  " << field_ty << " " << field_name << def_init << ";\n";
      }
    }
  }

  /// Encode one extension addition group (caller must shadow `w` to the open-type writer).
  void emit_extension_group_encode(std::ostream& out, const std::vector<ir::Field>& group,
                                   const std::string& expr, bool aper,
                                   const std::string& ind) {
    const char* C = ns_codec(aper);
    std::vector<const ir::Field*> optionals;
    for (const ir::Field& f : group) {
      if (field_has_presence_bit(f)) {
        optionals.push_back(&f);
      }
    }
    if (!optionals.empty()) {
      out << ind << "{\n";
      out << ind << "  std::uint8_t gopt[" << optionals.size() << "] = {};\n";
      for (std::size_t i = 0; i < optionals.size(); ++i) {
        const ir::Field& pf = *optionals[i];
        out << ind << "  gopt[" << i << "] = ";
        if (pf.presence == ir::Presence::Default) {
          out << "1;\n";
        } else {
          out << expr << "." << cpp_ident(pf.name) << ".has_value() ? 1 : 0;\n";
        }
      }
      out << ind << "  " << C
          << "::encode_sequence_preamble(w, false, false, asn1::Span<const std::uint8_t>(gopt, "
          << optionals.size() << "));\n";
      out << ind << "}\n";
    }
    for (const ir::Field& f : group) {
      const std::string fname = cpp_ident(f.name);
      const ir::Type& ft = resolve(model_, f.type);
      if (f.presence == ir::Presence::Optional) {
        out << ind << "if (" << expr << "." << fname << ") {\n";
        emit_field_encode(out, ft, expr + "." + fname + ".value()", aper, ind + "  ");
        out << ind << "}\n";
      } else {
        emit_field_encode(out, ft, expr + "." + fname, aper, ind);
      }
    }
  }

  void emit_extension_group_decode(std::ostream& out, const std::vector<ir::Field>& group,
                                   const std::string& expr, bool aper,
                                   const std::string& ind) {
    const char* C = ns_codec(aper);
    std::vector<const ir::Field*> optionals;
    for (const ir::Field& f : group) {
      if (field_has_presence_bit(f)) {
        optionals.push_back(&f);
      }
    }
    if (!optionals.empty()) {
      out << ind << "{\n";
      out << ind << "  auto gbm = " << C << "::decode_sequence_preamble(r, false, "
          << optionals.size() << ");\n";
      out << ind << "  if (!gbm) return gbm.error();\n";
      std::size_t opt_i = 0;
      for (const ir::Field& f : group) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        if (field_has_presence_bit(f)) {
          out << ind << "  if (gbm.value().optionals.size() > " << opt_i
              << " && gbm.value().optionals[" << opt_i << "]) {\n"
              << ind << "    " << type_cpp(f.type) << " field{};\n";
          emit_field_decode(out, ft, "field", aper, ind + "    ");
          if (f.presence == ir::Presence::Optional) {
            out << ind << "    " << expr << "." << fname << " = std::move(field);\n";
          } else {
            out << ind << "    " << expr << "." << fname << " = std::move(field);\n";
          }
          if (f.presence == ir::Presence::Default && f.default_value) {
            const std::string def_expr = default_val_cpp(*f.default_value);
            if (!def_expr.empty()) {
              out << ind << "  } else {\n"
                  << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
            }
          }
          out << ind << "  }\n";
          ++opt_i;
        } else {
          out << ind << "  {\n"
              << ind << "    " << type_cpp(f.type) << " field{};\n";
          emit_field_decode(out, ft, "field", aper, ind + "    ");
          out << ind << "    " << expr << "." << fname << " = std::move(field);\n"
              << ind << "  }\n";
        }
      }
      out << ind << "}\n";
    } else {
      for (const ir::Field& f : group) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        out << ind << "{\n"
            << ind << "  " << type_cpp(f.type) << " field{};\n";
        emit_field_decode(out, ft, "field", aper, ind + "  ");
        out << ind << "  " << expr << "." << fname << " = std::move(field);\n"
            << ind << "}\n";
      }
    }
  }

  void emit_root_fields_encode(std::ostream& out, const std::vector<ir::Field>& fields,
                               const std::string& expr, bool aper, const std::string& ind) {
    for (const ir::Field& f : fields) {
      const std::string fname = cpp_ident(f.name);
      const ir::Type& ft = resolve(model_, f.type);
      if (f.presence == ir::Presence::Optional) {
        out << ind << "if (" << expr << "." << fname << ") {\n";
        emit_field_encode(out, ft, expr + "." + fname + ".value()", aper, ind + "  ");
        out << ind << "}\n";
      } else if (f.presence == ir::Presence::Default) {
        if (f.default_value) {
          std::string def_val = default_val_cpp(*f.default_value);
          if (!def_val.empty()) {
            out << ind << "if (" << expr << "." << fname << " != " << def_val << ") {\n";
            emit_field_encode(out, ft, expr + "." + fname, aper, ind + "  ");
            out << ind << "}\n";
            continue;
          }
        }
        emit_field_encode(out, ft, expr + "." + fname, aper, ind);
      } else {
        emit_field_encode(out, ft, expr + "." + fname, aper, ind);
      }
    }
  }

  void emit_sequence_like_encode(std::ostream& out, const ir::SequenceDesc& seq,
                                 const std::string& expr, bool aper,
                                 const std::string& ind) {
    const char* C = ns_codec(aper);
    std::vector<const ir::Field*> optionals;
    for (const ir::Field& f : seq.root) {
      if (field_has_presence_bit(f)) {
        optionals.push_back(&f);
      }
    }
    for (const ir::Field& f : seq.trailing_root) {
      if (field_has_presence_bit(f)) {
        optionals.push_back(&f);
      }
    }
    const std::size_t n_ext = seq.extension_groups.size();
    out << ind << "{\n";
    out << ind << "  std::uint8_t opt[" << (optionals.empty() ? 1 : optionals.size())
        << "] = {};\n";
    for (std::size_t i = 0; i < optionals.size(); ++i) {
      const ir::Field& pf = *optionals[i];
      out << ind << "  opt[" << i << "] = ";
      if (pf.presence == ir::Presence::Default) {
        if (pf.default_value) {
          std::string def_val = default_val_cpp(*pf.default_value);
          if (!def_val.empty()) {
            out << "(" << expr << "." << cpp_ident(pf.name) << " != " << def_val << ") ? 1 : 0;\n";
          } else {
            out << "1;\n";
          }
        } else {
          out << "1;\n";
        }
      } else {
        out << expr << "." << cpp_ident(pf.name) << ".has_value() ? 1 : 0;\n";
      }
    }
    out << ind << "  bool ext_present = false;\n";
    if (seq.extensible && n_ext > 0) {
      out << ind << "  std::uint8_t ep[" << n_ext << "] = {};\n";
      for (std::size_t g = 0; g < n_ext; ++g) {
        out << ind << "  ep[" << g << "] = (";
        const auto& group = seq.extension_groups[g];
        for (std::size_t i = 0; i < group.size(); ++i) {
          if (i) {
            out << " || ";
          }
          emit_ext_group_field_present(out, group[i], expr);
        }
        if (group.empty()) {
          out << "false";
        }
        out << ") ? 1 : 0;\n";
        out << ind << "  if (ep[" << g << "]) ext_present = true;\n";
      }
    }
    out << ind << "  " << C << "::encode_sequence_preamble(w, "
        << (seq.extensible ? "true" : "false") << ", ext_present, "
        << "asn1::Span<const std::uint8_t>(opt, " << optionals.size() << "));\n";
    out << ind << "}\n";
    emit_root_fields_encode(out, seq.root, expr, aper, ind);
    emit_root_fields_encode(out, seq.trailing_root, expr, aper, ind);
    if (seq.extensible && n_ext > 0) {
      out << ind << "{\n";
      out << ind << "  std::uint8_t ep[" << n_ext << "] = {};\n";
      out << ind << "  bool ext_present = false;\n";
      for (std::size_t g = 0; g < n_ext; ++g) {
        out << ind << "  ep[" << g << "] = (";
        const auto& group = seq.extension_groups[g];
        for (std::size_t i = 0; i < group.size(); ++i) {
          if (i) {
            out << " || ";
          }
          emit_ext_group_field_present(out, group[i], expr);
        }
        if (group.empty()) {
          out << "false";
        }
        out << ") ? 1 : 0;\n";
        out << ind << "  if (ep[" << g << "]) ext_present = true;\n";
      }
      out << ind << "  if (ext_present) {\n";
      out << ind << "    std::vector<std::vector<std::uint8_t>> ots;\n";
      for (std::size_t g = 0; g < n_ext; ++g) {
        out << ind << "    if (ep[" << g << "]) {\n";
        out << ind << "      asn1::BitWriter ow;\n";
        out << ind << "      {\n";
        out << ind << "        asn1::BitWriter& w = ow;\n";
        emit_extension_group_encode(out, seq.extension_groups[g], expr, aper, ind + "        ");
        out << ind << "      }\n";
        out << ind << "      ow.align_to_octet();\n";
        out << ind << "      ots.push_back(ow.take());\n";
        out << ind << "    }\n";
      }
      out << ind << "    " << C
          << "::encode_extension_additions(w, asn1::Span<const std::uint8_t>(ep, " << n_ext
          << "), ots);\n";
      out << ind << "  }\n";
      out << ind << "}\n";
    }
  }

  void emit_root_fields_decode(std::ostream& out, const std::vector<ir::Field>& fields,
                               const std::string& expr, bool aper, const std::string& ind,
                               std::size_t& opt_i) {
    for (const ir::Field& f : fields) {
      const std::string fname = cpp_ident(f.name);
      const ir::Type& ft = resolve(model_, f.type);
      if (f.presence == ir::Presence::Optional) {
        out << ind << "  if (pre.value().optionals.size() > " << opt_i
            << " && pre.value().optionals[" << opt_i << "]) {\n"
            << ind << "    " << type_cpp(f.type) << " field{};\n";
        emit_field_decode(out, ft, "field", aper, ind + "    ");
        out << ind << "    " << expr << "." << fname << " = std::move(field);\n"
            << ind << "  } else {\n"
            << ind << "    " << expr << "." << fname << " = std::nullopt;\n"
            << ind << "  }\n";
        ++opt_i;
      } else if (f.presence == ir::Presence::Default) {
        out << ind << "  if (pre.value().optionals.size() > " << opt_i
            << " && pre.value().optionals[" << opt_i << "]) {\n";
        emit_field_decode(out, ft, expr + "." + fname, aper, ind + "    ");
        if (f.default_value) {
          const std::string def_expr = default_val_cpp(*f.default_value);
          if (!def_expr.empty()) {
            out << ind << "  } else {\n"
                << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
          }
        }
        out << ind << "  }\n";
        ++opt_i;
      } else {
        emit_field_decode(out, ft, expr + "." + fname, aper, ind + "  ");
      }
    }
  }

  void emit_sequence_like_decode(std::ostream& out, const ir::SequenceDesc& seq,
                                 const std::string& expr, bool aper,
                                 const std::string& ind) {
    const char* C = ns_codec(aper);
    std::size_t n_optionals = 0;
    for (const ir::Field& f : seq.root) {
      if (field_has_presence_bit(f)) {
        ++n_optionals;
      }
    }
    for (const ir::Field& f : seq.trailing_root) {
      if (field_has_presence_bit(f)) {
        ++n_optionals;
      }
    }
    const std::size_t n_ext = seq.extension_groups.size();
    out << ind << "{\n";
    out << ind << "  auto pre = " << C << "::decode_sequence_preamble(r, "
        << (seq.extensible ? "true" : "false") << ", " << n_optionals << ");\n";
    out << ind << "  if (!pre) return pre.error();\n";
    std::size_t opt_i = 0;
    emit_root_fields_decode(out, seq.root, expr, aper, ind, opt_i);
    emit_root_fields_decode(out, seq.trailing_root, expr, aper, ind, opt_i);
    if (seq.extensible) {
      out << ind << "  if (pre.value().extensions_present) {\n";
      out << ind << "    auto ext = " << C << "::decode_extension_additions(r);\n";
      out << ind << "    if (!ext) return ext.error();\n";
      if (n_ext > 0) {
        out << ind << "    std::size_t ot_i = 0;\n";
        out << ind << "    for (std::size_t gi = 0; gi < ext.value().presence.size(); ++gi) {\n";
        out << ind << "      if (!ext.value().presence[gi]) continue;\n";
        out << ind << "      if (ot_i >= ext.value().open_types.size()) break;\n";
        out << ind << "      const auto& ot = ext.value().open_types[ot_i++];\n";
        out << ind << "      if (gi >= " << n_ext << "u) continue;\n";
        out << ind << "      asn1::BitReader er(ot);\n";
        out << ind << "      {\n";
        out << ind << "        asn1::BitReader& r = er;\n";
        out << ind << "        switch (gi) {\n";
        for (std::size_t g = 0; g < n_ext; ++g) {
          out << ind << "          case " << g << ":\n";
          emit_extension_group_decode(out, seq.extension_groups[g], expr, aper,
                                      ind + "            ");
          out << ind << "            break;\n";
        }
        out << ind << "          default: break;\n";
        out << ind << "        }\n";
        out << ind << "      }\n";
        out << ind << "    }\n";
      }
      out << ind << "  }\n";
    }
    out << ind << "}\n";
  }

  void emit_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                        bool aper, const std::string& ind) {
    const char* C = ns_codec(aper);
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        out << ind << C << "::encode_boolean(w, " << expr << ");\n";
        break;
      case ir::TypeKind::Null:
        out << ind << C << "::encode_null(w);\n";
        break;
      case ir::TypeKind::Integer:
        if (integer_is_bigint(t.integer)) {
          out << ind << "if (auto er = " << C << "::encode_integer(w, " << expr << ", "
              << integer_constraint_expr(t.integer) << "); !er) return er.error();\n";
        } else {
          out << ind << "if (auto er = " << C << "::encode_integer(w, static_cast<std::int64_t>("
              << expr << "), " << integer_constraint_expr(t.integer)
              << "); !er) return er.error();\n";
        }
        break;
      case ir::TypeKind::OctetString:
        out << ind << C << "::encode_octet_string(w, " << expr << ", "
            << size_constraint_expr(t.octet_string.size) << ");\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << C << "::encode_bit_string(w, " << expr << ".bits, " << expr
            << ".bit_length, " << size_constraint_expr(t.bit_string.size) << ");\n";
        break;
      case ir::TypeKind::String:
        out << ind << C << "::encode_utf8_string(w, " << expr << ", "
            << size_constraint_expr(t.string.size) << ");\n";
        break;
      case ir::TypeKind::SequenceOf: {
        const ir::Type& elem = resolve(model_, t.sequence_of.element);
        out << ind << "{\n";
        out << ind << "  std::size_t remaining = " << expr << ".value.size();\n";
        out << ind << "  std::size_t offset = 0;\n";
        out << ind << "  bool is_first = true;\n";
        out << ind << "  for (;;) {\n";
        out << ind << "    std::size_t chunk = " << C << "::encode_sequence_of_chunk(w, remaining, is_first, "
            << size_constraint_expr(t.sequence_of.size) << ");\n";
        out << ind << "    for (std::size_t i = 0; i < chunk; ++i) {\n";
        out << ind << "      const auto& elem = " << expr << ".value[offset + i];\n";
        if (is_named(elem)) {
          out << ind << "      if (auto r = encode_" << fn_suffix(aper) << "(w, elem); !r) return r;\n";
        } else {
          emit_encode_body(out, elem, "elem", aper, ind + "      ");
        }
        out << ind << "    }\n";
        out << ind << "    offset += chunk;\n";
        out << ind << "    remaining -= chunk;\n";
        out << ind << "    is_first = false;\n";
        out << ind << "    if (chunk < 16384) break;\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }

      case ir::TypeKind::SetOf: {
        const ir::Type& elem = resolve(model_, t.set_of.element);
        out << ind << "{\n";
        out << ind << "  std::size_t remaining = " << expr << ".value.size();\n";
        out << ind << "  std::size_t offset = 0;\n";
        out << ind << "  bool is_first = true;\n";
        out << ind << "  for (;;) {\n";
        out << ind << "    std::size_t chunk = " << C << "::encode_sequence_of_chunk(w, remaining, is_first, "
            << size_constraint_expr(t.set_of.size) << ");\n";
        out << ind << "    for (std::size_t i = 0; i < chunk; ++i) {\n";
        out << ind << "      const auto& elem = " << expr << ".value[offset + i];\n";
        if (is_named(elem)) {
          out << ind << "      if (auto r = encode_" << fn_suffix(aper) << "(w, elem); !r) return r;\n";
        } else {
          emit_encode_body(out, elem, "elem", aper, ind + "      ");
        }
        out << ind << "    }\n";
        out << ind << "    offset += chunk;\n";
        out << ind << "    remaining -= chunk;\n";
        out << ind << "    is_first = false;\n";
        out << ind << "    if (chunk < 16384) break;\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Enumerated: {
        const auto vals = enum_root_sorted_values(t.enumerated);
        const auto ext_vals = enum_extension_values(t.enumerated);
        out << ind << "{\n";
        out << ind << "  bool is_ext = false;\n";
        out << ind << "  std::size_t idx = 0;\n";
        out << ind << "  switch (static_cast<std::int64_t>(" << expr << ")) {\n";
        for (std::size_t i = 0; i < vals.size(); ++i) {
          out << ind << "    case " << vals[i] << ": idx = " << i << "; break;\n";
        }
        for (std::size_t i = 0; i < ext_vals.size(); ++i) {
          out << ind << "    case " << ext_vals[i] << ": is_ext = true; idx = " << i
              << "; break;\n";
        }
        out << ind << "    default: return asn1::make_error(asn1::Error::Code::ConstraintViolation, 0, \"invalid ENUMERATED value\");\n";
        out << ind << "  }\n";
        out << ind << "  if (is_ext) {\n";
        out << ind << "    " << C << "::encode_enumerated_extension(w, idx);\n";
        out << ind << "  } else {\n";
        out << ind << "    " << C << "::encode_enumerated(w, idx, "
            << t.enumerated.root.size() << ", "
            << (t.enumerated.extensible ? "true" : "false") << ");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::ObjectIdentifier:
        out << ind << C << "::encode_object_identifier(w, " << expr << ", false);\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << C << "::encode_object_identifier(w, " << expr << ", true);\n";
        break;
      case ir::TypeKind::ObjectClassField:
        if (t.object_class_field.open_type) {
          out << ind << C << "::encode_open_type(w, " << expr << ");\n";
        } else {
          emit_encode_body(out, resolve(model_, t.object_class_field.fixed_type), expr, aper, ind);
        }
        break;
      case ir::TypeKind::InstanceOf:
        out << ind << C << "::encode_object_identifier(w, " << expr << ".type_id, false);\n";
        out << ind << C << "::encode_open_type(w, " << expr << ".value);\n";
        break;
      case ir::TypeKind::Real:
        out << ind << C << "::encode_real(w, " << expr << ");\n";
        break;
      case ir::TypeKind::Set:
        emit_sequence_like_encode(out, t.set, expr, aper, ind);
        break;
      case ir::TypeKind::Sequence:
        emit_sequence_like_encode(out, t.sequence, expr, aper, ind);
        break;
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(t.choice);
        const std::size_t root_n = t.choice.alternatives.size();
        out << ind << "{\n";
        out << ind << "  const std::size_t index = " << expr << ".alt.index();\n";
        out << ind << "  if (index < " << root_n << "u) {\n";
        out << ind << "    " << C << "::encode_choice_root(w, index, " << root_n << ", "
            << (t.choice.extensible ? "true" : "false") << ");\n";
        out << ind << "    switch (index) {\n";
        for (std::size_t i = 0; i < root_n; ++i) {
          const ir::Field& f = t.choice.alternatives[i];
          const ir::Type& ft = resolve(model_, f.type);
          out << ind << "      case " << i << ": {\n";
          out << ind << "        const auto& alt = std::get<" << i << ">(" << expr
              << ".alt);\n";
          emit_field_encode(out, ft, "alt.value", aper, ind + "        ");
          out << ind << "        break;\n";
          out << ind << "      }\n";
        }
        out << ind << "      default: break;\n";
        out << ind << "    }\n";
        out << ind << "  } else {\n";
        out << ind << "    const std::size_t ext_i = index - " << root_n << "u;\n";
        out << ind << "    asn1::BitWriter ow;\n";
        out << ind << "    {\n";
        out << ind << "      asn1::BitWriter& w = ow;\n";
        out << ind << "      switch (index) {\n";
        for (std::size_t i = 0; i < t.choice.extensions.size(); ++i) {
          const ir::Field& f = t.choice.extensions[i];
          const ir::Type& ft = resolve(model_, f.type);
          const std::size_t vi = root_n + i;
          out << ind << "        case " << vi << ": {\n";
          out << ind << "          const auto& alt = std::get<" << vi << ">(" << expr
              << ".alt);\n";
          emit_field_encode(out, ft, "alt.value", aper, ind + "          ");
          out << ind << "          break;\n";
          out << ind << "        }\n";
        }
        out << ind << "        default:\n";
        out << ind
            << "          return asn1::make_error(asn1::Error::Code::InvalidArgument, w.bit_size(), "
               "\"CHOICE index out of range\");\n";
        out << ind << "      }\n";
        out << ind << "    }\n";
        out << ind << "    ow.align_to_octet();\n";
        out << ind << "    " << C << "::encode_choice_extension(w, ext_i, ow.take());\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        (void)all;
        break;
      }
      case ir::TypeKind::Referenced:
        if (t.referenced.resolved != ir::kInvalidType) {
          emit_encode_body(out, resolve(model_, t.referenced.resolved), expr, aper, ind);
        }
        break;
    }
  }

  void emit_field_encode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                         bool aper, const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "if (auto r = encode_" << fn_suffix(aper) << "(w, " << expr
          << "); !r) return r;\n";
    } else {
      emit_encode_body(out, ft, expr, aper, ind);
    }
  }

  void emit_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                        bool aper, const std::string& ind) {
    const char* C = ns_codec(aper);
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "{\n"
            << ind << "  auto tmp = " << C << "::decode_boolean(r);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "{\n"
            << ind << "  auto tmp = " << C << "::decode_null(r);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Integer:
        out << ind << "{\n";
        if (integer_is_bigint(t.integer)) {
          out << ind << "  auto tmp = " << C << "::decode_big_integer(r, "
              << integer_constraint_expr(t.integer) << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          out << ind << "  auto tmp = " << C << "::decode_integer(r, "
              << integer_constraint_expr(t.integer) << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = static_cast<" << integer_host_type(t.integer)
              << ">(tmp.value());\n";
        }
        out << ind << "}\n";
        break;
      case ir::TypeKind::OctetString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << C << "::decode_octet_string(r, "
            << size_constraint_expr(t.octet_string.size) << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << C << "::decode_bit_string(r, "
            << size_constraint_expr(t.bit_string.size) << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << "{\n"
            << ind << "  auto tmp = " << C << "::decode_utf8_string(r, "
            << size_constraint_expr(t.string.size) << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::SequenceOf: {
        const ir::Type& elem_t = resolve(model_, t.sequence_of.element);
        out << ind << "{\n"
            << ind << "  " << expr << ".value.clear();\n"
            << ind << "  bool is_first = true;\n"
            << ind << "  for (;;) {\n"
            << ind << "    auto n = " << C << "::decode_sequence_of_chunk(r, is_first, "
            << size_constraint_expr(t.sequence_of.size) << ");\n"
            << ind << "    if (!n) return n.error();\n"
            << ind << "    if (" << expr << ".value.size() + n.value() > 1000000) return asn1::make_error(asn1::Error::Code::LengthOverflow, r.bit_offset(), \"sequence of count exceeds safe limit\");\n"
            << ind << "    " << expr << ".value.reserve(" << expr << ".value.size() + std::min<std::size_t>(n.value(), 1024));\n"
            << ind << "    for (std::size_t i = 0; i < n.value(); ++i) {\n"
            << ind << "      " << type_cpp(t.sequence_of.element) << " elem{};\n";
        if (is_named(elem_t)) {
          out << ind << "      if (auto dr = decode_" << fn_suffix(aper)
              << "(r, elem); !dr) return dr;\n";
        } else {
          emit_decode_body(out, elem_t, "elem", aper, ind + "      ");
        }
        out << ind << "      " << expr << ".value.push_back(std::move(elem));\n"
            << ind << "    }\n"
            << ind << "    is_first = false;\n"
            << ind << "    if (n.value() < 16384) break;\n"
            << ind << "  }\n"
            << ind << "}\n";
        break;
      }

      case ir::TypeKind::SetOf: {
        const ir::Type& elem_t = resolve(model_, t.set_of.element);
        out << ind << "{\n"
            << ind << "  " << expr << ".value.clear();\n"
            << ind << "  bool is_first = true;\n"
            << ind << "  for (;;) {\n"
            << ind << "    auto n = " << C << "::decode_sequence_of_chunk(r, is_first, "
            << size_constraint_expr(t.set_of.size) << ");\n"
            << ind << "    if (!n) return n.error();\n"
            << ind << "    if (" << expr << ".value.size() + n.value() > 1000000) return asn1::make_error(asn1::Error::Code::LengthOverflow, r.bit_offset(), \"set of count exceeds safe limit\");\n"
            << ind << "    " << expr << ".value.reserve(" << expr << ".value.size() + std::min<std::size_t>(n.value(), 1024));\n"
            << ind << "    for (std::size_t i = 0; i < n.value(); ++i) {\n"
            << ind << "      " << type_cpp(t.set_of.element) << " elem{};\n";
        if (is_named(elem_t)) {
          out << ind << "      if (auto dr = decode_" << fn_suffix(aper)
              << "(r, elem); !dr) return dr;\n";
        } else {
          emit_decode_body(out, elem_t, "elem", aper, ind + "      ");
        }
        out << ind << "      " << expr << ".value.push_back(std::move(elem));\n"
            << ind << "    }\n"
            << ind << "    is_first = false;\n"
            << ind << "    if (n.value() < 16384) break;\n"
            << ind << "  }\n"
            << ind << "}\n";
        break;
      }
      case ir::TypeKind::Enumerated: {
        const auto vals = enum_root_sorted_values(t.enumerated);
        const auto ext_vals = enum_extension_values(t.enumerated);
        const std::string ty = is_named(t) ? named_type_cpp(t) : "std::int64_t";
        out << ind << "{\n"
            << ind << "  auto idx = " << C << "::decode_enumerated(r, "
            << t.enumerated.root.size() << ", "
            << (t.enumerated.extensible ? "true" : "false") << ");\n"
            << ind << "  if (!idx) return idx.error();\n"
            << ind << "  if (idx.value().extension) {\n"
            << ind << "    switch (idx.value().index) {\n";
        for (std::size_t i = 0; i < ext_vals.size(); ++i) {
          out << ind << "      case " << i << ": " << expr << " = static_cast<" << ty << ">("
              << ext_vals[i] << "); break;\n";
        }
        out << ind << "      default: " << expr << " = static_cast<" << ty << ">(0); break;\n";
        out << ind << "    }\n"
            << ind << "  } else {\n"
            << ind << "    switch (idx.value().index) {\n";
        for (std::size_t i = 0; i < vals.size(); ++i) {
          out << ind << "      case " << i << ": " << expr << " = static_cast<" << ty << ">("
              << vals[i] << "); break;\n";
        }
        out << ind << "      default: " << expr << " = static_cast<" << ty << ">(0); break;\n";
        out << ind << "    }\n"
            << ind << "  }\n"
            << ind << "}\n";
        break;
      }
      case ir::TypeKind::ObjectIdentifier:
        out << ind << "{\n"
            << ind << "  auto tmp = " << C << "::decode_object_identifier(r, false);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << "{\n"
            << ind << "  auto tmp = " << C << "::decode_object_identifier(r, true);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::ObjectClassField:
        if (t.object_class_field.open_type) {
          out << ind << "{\n"
              << ind << "  auto tmp = " << C << "::decode_open_type(r);\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = std::move(tmp.value());\n"
              << ind << "}\n";
        } else {
          emit_decode_body(out, resolve(model_, t.object_class_field.fixed_type), expr, aper, ind);
        }
        break;
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n"
            << ind << "  auto oid = " << C << "::decode_object_identifier(r, false);\n"
            << ind << "  if (!oid) return oid.error();\n"
            << ind << "  " << expr << ".type_id = std::move(oid.value());\n"
            << ind << "  auto ot = " << C << "::decode_open_type(r);\n"
            << ind << "  if (!ot) return ot.error();\n"
            << ind << "  " << expr << ".value = std::move(ot.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "{\n"
            << ind << "  auto tmp = " << C << "::decode_real(r);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Set:
        emit_sequence_like_decode(out, t.set, expr, aper, ind);
        break;
      case ir::TypeKind::Sequence:
        emit_sequence_like_decode(out, t.sequence, expr, aper, ind);
        break;
      case ir::TypeKind::Choice: {
        const std::size_t root_n = t.choice.alternatives.size();
        out << ind << "{\n"
            << ind << "  auto idx = " << C << "::decode_choice(r, " << root_n << ", "
            << (t.choice.extensible ? "true" : "false") << ");\n"
            << ind << "  if (!idx) return idx.error();\n"
            << ind << "  if (!idx.value().extension) {\n"
            << ind << "    switch (idx.value().index) {\n";
        for (std::size_t i = 0; i < root_n; ++i) {
          const ir::Field& f = t.choice.alternatives[i];
          const ir::Type& ft = resolve(model_, f.type);
          out << ind << "      case " << i << ": {\n"
              << ind << "        " << named_type_cpp(t) << "::" << cpp_ident(f.name)
              << "_ alt{};\n";
          emit_field_decode(out, ft, "alt.value", aper, ind + "        ");
          out << ind << "        " << expr << ".alt = std::move(alt);\n"
              << ind << "        break;\n"
              << ind << "      }\n";
        }
        out << ind << "      default:\n"
            << ind
            << "        return asn1::make_error(asn1::Error::Code::InvalidArgument, r.bit_offset(), "
               "\"CHOICE index out of range\");\n"
            << ind << "    }\n"
            << ind << "  } else {\n"
            << ind << "    auto ot = " << C << "::decode_open_type(r);\n"
            << ind << "    if (!ot) return ot.error();\n"
            << ind << "    asn1::BitReader er(ot.value());\n"
            << ind << "    {\n"
            << ind << "      asn1::BitReader& r = er;\n"
            << ind << "      switch (idx.value().index) {\n";
        for (std::size_t i = 0; i < t.choice.extensions.size(); ++i) {
          const ir::Field& f = t.choice.extensions[i];
          const ir::Type& ft = resolve(model_, f.type);
          out << ind << "        case " << i << ": {\n"
              << ind << "          " << named_type_cpp(t) << "::" << cpp_ident(f.name)
              << "_ alt{};\n";
          emit_field_decode(out, ft, "alt.value", aper, ind + "          ");
          out << ind << "          " << expr << ".alt = std::move(alt);\n"
              << ind << "          break;\n"
              << ind << "        }\n";
        }
        out << ind << "        default:\n"
            << ind
            << "          return asn1::make_error(asn1::Error::Code::Unsupported, r.bit_offset(), "
               "\"unknown CHOICE extension alternative\");\n"
            << ind << "      }\n"
            << ind << "    }\n"
            << ind << "  }\n"
            << ind << "}\n";
        break;
      }
      case ir::TypeKind::Referenced:
        if (t.referenced.resolved != ir::kInvalidType) {
          emit_decode_body(out, resolve(model_, t.referenced.resolved), expr, aper, ind);
        }
        break;
    }
  }

  void emit_field_decode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                         bool aper, const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "if (auto dr = decode_" << fn_suffix(aper) << "(r, " << expr
          << "); !dr) return dr;\n";
    } else {
      emit_decode_body(out, ft, expr, aper, ind);
    }
  }

  static const char* jer_name_form_expr(ir::Field::JerNameForm f) {
    switch (f) {
      case ir::Field::JerNameForm::AsIs:
        return "asn1::jeri::NameForm::AsIs";
      case ir::Field::JerNameForm::Capitalized:
        return "asn1::jeri::NameForm::Capitalized";
      case ir::Field::JerNameForm::Uppercased:
        return "asn1::jeri::NameForm::Uppercased";
      case ir::Field::JerNameForm::Lowercased:
        return "asn1::jeri::NameForm::Lowercased";
      case ir::Field::JerNameForm::Literal:
        return "asn1::jeri::NameForm::Literal";
    }
    return "asn1::jeri::NameForm::AsIs";
  }

  static const char* jer_text_form_expr(ir::JerEncoding::TextForm f) {
    switch (f) {
      case ir::JerEncoding::TextForm::AsIs:
        return "asn1::jeri::NameForm::AsIs";
      case ir::JerEncoding::TextForm::Capitalized:
        return "asn1::jeri::NameForm::Capitalized";
      case ir::JerEncoding::TextForm::Uppercased:
        return "asn1::jeri::NameForm::Uppercased";
      case ir::JerEncoding::TextForm::Lowercased:
        return "asn1::jeri::NameForm::Lowercased";
      case ir::JerEncoding::TextForm::Literal:
        return "asn1::jeri::NameForm::Literal";
    }
    return "asn1::jeri::NameForm::AsIs";
  }

  void emit_jer_codec_fns(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    out << "inline asn1::Result<asn1::jer::Value> encode_jer(const " << name
        << "& value) {\n";
    emit_jer_encode_body(out, t, "value", "  ");
    out << "}\n\n";

    out << "inline asn1::Result<void> decode_jer(const asn1::jer::Value& v, " << name
        << "& value) {\n";
    emit_jer_decode_body(out, t, "value", "v", "  ");
    out << "  return asn1::Result<void>::success();\n}\n\n";
  }

  void emit_jer_encode_call(std::ostream& out, const ir::Type& ft, const std::string& expr,
                            const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "encode_jer(" << expr << ")";
    } else {
      out << ind << "([&]() -> asn1::Result<asn1::jer::Value> {\n";
      emit_jer_encode_body(out, resolve(model_, ft.kind == ir::TypeKind::Referenced
                                                     ? ft.referenced.resolved
                                                     : ir::kInvalidType),
                           expr, ind + "  ");
      out << ind << "})()";
    }
  }

  void emit_jer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "return asn1::Result<asn1::jer::Value>::success(asn1::jeri::encode_boolean("
            << expr << "));\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "return asn1::Result<asn1::jer::Value>::success(asn1::jeri::encode_null());\n";
        break;
      case ir::TypeKind::Integer:
        out << ind << "return asn1::Result<asn1::jer::Value>::success(asn1::jeri::encode_integer("
            << expr << "));\n";
        break;
      case ir::TypeKind::OctetString:
        if (rt.jer.base64) {
          out << ind << "return asn1::Result<asn1::jer::Value>::success("
              << "asn1::jeri::encode_octet_string_base64(asn1::Span<const std::uint8_t>("
              << expr << ".data(), " << expr << ".size())));\n";
        } else {
          out << ind << "return asn1::Result<asn1::jer::Value>::success("
              << "asn1::jeri::encode_octet_string(asn1::Span<const std::uint8_t>(" << expr
              << ".data(), " << expr << ".size())));\n";
        }
        break;
      case ir::TypeKind::BitString:
        out << ind << "return asn1::Result<asn1::jer::Value>::success(asn1::jeri::encode_bit_string("
            << expr << ".bits, " << expr << ".bit_length));\n";
        break;
      case ir::TypeKind::String:
        out << ind << "return asn1::Result<asn1::jer::Value>::success("
            << "asn1::jeri::encode_utf8_string(" << expr << "));\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        out << ind << "return asn1::Result<asn1::jer::Value>::success("
            << "asn1::jeri::encode_object_identifier(" << expr << "));\n";
        break;
      case ir::TypeKind::InstanceOf:
        out << ind << "return asn1::Result<asn1::jer::Value>::success("
            << "asn1::jeri::encode_sequence_of({asn1::jeri::encode_object_identifier(" << expr
            << ".type_id), asn1::jeri::encode_octet_string(asn1::Span<const std::uint8_t>("
            << expr << ".value.data(), " << expr << ".value.size()))}));\n";
        break;
      case ir::TypeKind::Enumerated: {
        out << ind << "std::string jer_enum_id;\n";
        out << ind << "switch (static_cast<std::int64_t>(" << expr << ")) {\n";
        for (const auto& nn : rt.enumerated.root) {
          out << ind << "  case " << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
              << ": jer_enum_id = \"" << nn.name << "\"; break;\n";
        }
        for (const auto& nn : rt.enumerated.extensions) {
          out << ind << "  case " << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
              << ": jer_enum_id = \"" << nn.name << "\"; break;\n";
        }
        out << ind << "  default: jer_enum_id = \"" << (rt.enumerated.root.empty()
                                                             ? "?"
                                                             : rt.enumerated.root.front().name)
            << "\"; break;\n";
        out << ind << "}\n";
        if (rt.jer.text_form != ir::JerEncoding::TextForm::AsIs) {
          out << ind << "return asn1::Result<asn1::jer::Value>::success("
              << "asn1::jeri::encode_enumerated_text(jer_enum_id, "
              << jer_text_form_expr(rt.jer.text_form) << ", \"" << rt.jer.text_literal
              << "\"));\n";
        } else {
          out << ind << "return asn1::Result<asn1::jer::Value>::success("
              << "asn1::jeri::encode_enumerated(jer_enum_id));\n";
        }
        break;
      }
      case ir::TypeKind::SequenceOf: {
        const ir::Type& elem = resolve(model_, rt.sequence_of.element);
        out << ind << "std::vector<asn1::jer::Value> jer_elems;\n";
        out << ind << "jer_elems.reserve(" << expr << ".value.size());\n";
        out << ind << "for (const auto& jer_elem : " << expr << ".value) {\n";
        out << ind << "  auto jer_enc = ";
        emit_jer_field_encode_expr(out, elem, "jer_elem", ind + "  ");
        out << ind << "  if (!jer_enc) return jer_enc.error();\n";
        out << ind << "  jer_elems.push_back(std::move(jer_enc.value()));\n";
        out << ind << "}\n";
        out << ind << "return asn1::Result<asn1::jer::Value>::success("
            << "asn1::jeri::encode_sequence_of(std::move(jer_elems)));\n";
        break;
      }
      case ir::TypeKind::SetOf: {
        const ir::Type& elem = resolve(model_, rt.set_of.element);
        if (rt.jer.object) {
          const ir::Type* seq = &elem;
          if (seq->kind == ir::TypeKind::Referenced && seq->referenced.resolved != ir::kInvalidType) {
            seq = &resolve(model_, seq->referenced.resolved);
          }
          if (seq->kind == ir::TypeKind::Sequence && seq->sequence.root.size() == 2u) {
            const ir::Field& kf = seq->sequence.root[0];
            const ir::Field& vf = seq->sequence.root[1];
            const std::string kname = cpp_ident(kf.name);
            const std::string vname = cpp_ident(vf.name);
            out << ind << "std::vector<std::pair<std::string, asn1::jer::Value>> jer_entries;\n";
            out << ind << "for (const auto& jer_item : " << expr << ".value) {\n";
            out << ind << "  auto jer_val = ";
            emit_jer_field_encode_expr(out, resolve(model_, vf.type), "jer_item." + vname,
                                       ind + "  ");
            out << ind << "  if (!jer_val) return jer_val.error();\n";
            out << ind << "  jer_entries.emplace_back(jer_item." << kname
                << ", std::move(jer_val.value()));\n";
            out << ind << "}\n";
            out << ind << "return asn1::Result<asn1::jer::Value>::success("
                << "asn1::jeri::encode_set_of_object(std::move(jer_entries)));\n";
            break;
          }
        }
        out << ind << "std::vector<asn1::jer::Value> jer_elems;\n";
        out << ind << "for (const auto& jer_elem : " << expr << ".value) {\n";
        out << ind << "  auto jer_enc = ";
        emit_jer_field_encode_expr(out, elem, "jer_elem", ind + "  ");
        out << ind << "  if (!jer_enc) return jer_enc.error();\n";
        out << ind << "  jer_elems.push_back(std::move(jer_enc.value()));\n";
        out << ind << "}\n";
        out << ind << "return asn1::Result<asn1::jer::Value>::success("
            << "asn1::jeri::encode_sequence_of(std::move(jer_elems)));\n";
        break;
      }
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        const auto& fields =
            rt.kind == ir::TypeKind::Sequence ? rt.sequence.root : rt.set.root;
        if (rt.jer.array) {
          out << ind << "std::vector<asn1::jer::Value> jer_parts;\n";
          for (const ir::Field& f : fields) {
            const std::string fname = cpp_ident(f.name);
            const ir::Type& ft = resolve(model_, f.type);
            const bool optional = f.presence == ir::Presence::Optional;
            if (optional) {
              out << ind << "if (" << expr << "." << fname << ") {\n";
              out << ind << "  auto jer_p = ";
              emit_jer_field_encode_expr(out, ft, expr + "." + fname + ".value()",
                                         ind + "  ");
              out << ind << "  if (!jer_p) return jer_p.error();\n";
              out << ind << "  jer_parts.push_back(std::move(jer_p.value()));\n";
              out << ind << "} else {\n";
              out << ind << "  jer_parts.push_back(asn1::jer::Value::null_value());\n";
              out << ind << "}\n";
            } else {
              out << ind << "{\n";
              out << ind << "  auto jer_p = ";
              emit_jer_field_encode_expr(out, ft, expr + "." + fname, ind + "  ");
              out << ind << "  if (!jer_p) return jer_p.error();\n";
              out << ind << "  jer_parts.push_back(std::move(jer_p.value()));\n";
              out << ind << "}\n";
            }
          }
          out << ind << "return asn1::Result<asn1::jer::Value>::success("
              << "asn1::jeri::encode_sequence_array(std::move(jer_parts)));\n";
        } else {
          out << ind << "std::vector<std::pair<std::string, asn1::jer::Value>> jer_members;\n";
          for (const ir::Field& f : fields) {
            const std::string fname = cpp_ident(f.name);
            const ir::Type& ft = resolve(model_, f.type);
            const bool optional = f.presence == ir::Presence::Optional;
            if (optional) {
              out << ind << "if (" << expr << "." << fname << ") {\n";
              out << ind << "  auto jer_m = ";
              emit_jer_field_encode_expr(out, ft, expr + "." + fname + ".value()",
                                         ind + "  ");
              out << ind << "  if (!jer_m) return jer_m.error();\n";
              if (f.jer_name_form != ir::Field::JerNameForm::AsIs ||
                  !f.jer_name_literal.empty()) {
                out << ind << "  jer_members.push_back(asn1::jeri::named_member(\"" << f.name
                    << "\", std::move(jer_m.value()), " << jer_name_form_expr(f.jer_name_form)
                    << ", \"" << f.jer_name_literal << "\"));\n";
              } else {
                out << ind << "  jer_members.emplace_back(\"" << f.name
                    << "\", std::move(jer_m.value()));\n";
              }
              out << ind << "}\n";
            } else if (f.presence == ir::Presence::Default) {
              if (f.default_value) {
                std::string def_val = default_val_cpp(*f.default_value);
                if (!def_val.empty()) {
                  out << ind << "if (" << expr << "." << fname << " != " << def_val << ") {\n";
                  out << ind << "  auto jer_m = ";
                  emit_jer_field_encode_expr(out, ft, expr + "." + fname, ind + "  ");
                  out << ind << "  if (!jer_m) return jer_m.error();\n";
                  if (f.jer_name_form != ir::Field::JerNameForm::AsIs ||
                      !f.jer_name_literal.empty()) {
                    out << ind << "  jer_members.push_back(asn1::jeri::named_member(\"" << f.name
                        << "\", std::move(jer_m.value()), " << jer_name_form_expr(f.jer_name_form)
                        << ", \"" << f.jer_name_literal << "\"));\n";
                  } else {
                    out << ind << "  jer_members.emplace_back(\"" << f.name
                        << "\", std::move(jer_m.value()));\n";
                  }
                  out << ind << "}\n";
                  continue;
                }
              }
              out << ind << "{\n";
              out << ind << "  auto jer_m = ";
              emit_jer_field_encode_expr(out, ft, expr + "." + fname, ind + "  ");
              out << ind << "  if (!jer_m) return jer_m.error();\n";
              if (f.jer_name_form != ir::Field::JerNameForm::AsIs ||
                  !f.jer_name_literal.empty()) {
                out << ind << "  jer_members.push_back(asn1::jeri::named_member(\"" << f.name
                    << "\", std::move(jer_m.value()), " << jer_name_form_expr(f.jer_name_form)
                    << ", \"" << f.jer_name_literal << "\"));\n";
              } else {
                out << ind << "  jer_members.emplace_back(\"" << f.name
                    << "\", std::move(jer_m.value()));\n";
              }
              out << ind << "}\n";
            } else {
              out << ind << "{\n";
              out << ind << "  auto jer_m = ";
              emit_jer_field_encode_expr(out, ft, expr + "." + fname, ind + "  ");
              out << ind << "  if (!jer_m) return jer_m.error();\n";
              if (f.jer_name_form != ir::Field::JerNameForm::AsIs ||
                  !f.jer_name_literal.empty()) {
                out << ind << "  jer_members.push_back(asn1::jeri::named_member(\"" << f.name
                    << "\", std::move(jer_m.value()), " << jer_name_form_expr(f.jer_name_form)
                    << ", \"" << f.jer_name_literal << "\"));\n";
              } else {
                out << ind << "  jer_members.emplace_back(\"" << f.name
                    << "\", std::move(jer_m.value()));\n";
              }
              out << ind << "}\n";
            }
          }
          out << ind << "return asn1::Result<asn1::jer::Value>::success("
              << "asn1::jeri::make_sequence(std::move(jer_members)));\n";
        }
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        out << ind << "switch (" << expr << ".alt.index()) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          out << ind << "  case " << i << ": {\n";
          out << ind << "    const auto& jer_alt = std::get<" << i << ">(" << expr
              << ".alt);\n";
          out << ind << "    auto jer_enc = ";
          emit_jer_field_encode_expr(out, ft, "jer_alt.value", ind + "    ");
          out << ind << "    if (!jer_enc) return jer_enc.error();\n";
          if (rt.jer.unwrapped) {
            out << ind << "    return asn1::Result<asn1::jer::Value>::success("
                << "asn1::jeri::encode_choice_unwrapped(std::move(jer_enc.value())));\n";
          } else {
            const char* alt_key = f.name.c_str();
            if (f.jer_name_form != ir::Field::JerNameForm::AsIs || !f.jer_name_literal.empty()) {
              out << ind << "    return asn1::Result<asn1::jer::Value>::success("
                  << "asn1::jeri::encode_choice(asn1::jeri::transform_name(\"" << f.name
                  << "\", " << jer_name_form_expr(f.jer_name_form) << ", \""
                  << f.jer_name_literal << "\"), std::move(jer_enc.value())));\n";
            } else {
              out << ind << "    return asn1::Result<asn1::jer::Value>::success("
                  << "asn1::jeri::encode_choice(\"" << alt_key
                  << "\", std::move(jer_enc.value())));\n";
            }
          }
          out << ind << "  }\n";
        }
        out << ind << "  default:\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"CHOICE index out of range\");\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_jer_encode_body(out, resolve(model_, rt.referenced.resolved), expr, ind);
        } else {
          out << ind << "return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
                 "\"unresolved type reference\");\n";
        }
        break;
      case ir::TypeKind::Real:
        out << ind << "return asn1::Result<asn1::jer::Value>::success(asn1::jeri::encode_real("
            << expr << "));\n";
        break;
      case ir::TypeKind::ObjectClassField:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
               "\"JER encoding not implemented for this type\");\n";
        break;
    }
  }

  void emit_jer_field_encode_expr(std::ostream& out, const ir::Type& ft,
                                  const std::string& expr, const std::string& ind) {
    if (is_named(ft)) {
      out << "encode_jer(" << expr << ");\n";
    } else {
      out << "([&]() -> asn1::Result<asn1::jer::Value> {\n";
      emit_jer_encode_body(out, ft, expr, ind);
      out << ind << "})();\n";
    }
  }

  void emit_jer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& v_expr, const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    switch (rt.kind) {
      case ir::TypeKind::Boolean: {
        out << ind << "{\n";
        out << ind << "  auto tmp = asn1::jeri::decode_boolean(" << v_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = tmp.value();\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Null:
        out << ind << "{\n";
        out << ind << "  auto tmp = asn1::jeri::decode_null(" << v_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Integer: {
        out << ind << "{\n";
        if (integer_is_bigint(rt.integer)) {
          out << ind << "  auto tmp = asn1::jeri::decode_big_integer(" << v_expr << ");\n";
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          out << ind << "  auto tmp = asn1::jeri::decode_integer(" << v_expr << ");\n";
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = static_cast<" << integer_host_type(rt.integer)
              << ">(tmp.value());\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::OctetString: {
        out << ind << "{\n";
        if (rt.jer.base64) {
          out << ind << "  auto tmp = asn1::jeri::decode_octet_string_base64(" << v_expr
              << ");\n";
        } else {
          out << ind << "  auto tmp = asn1::jeri::decode_octet_string(" << v_expr << ");\n";
        }
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::BitString: {
        out << ind << "{\n";
        out << ind << "  auto tmp = asn1::jeri::decode_bit_string(" << v_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::String: {
        out << ind << "{\n";
        out << ind << "  auto tmp = asn1::jeri::decode_utf8_string(" << v_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid: {
        out << ind << "{\n";
        out << ind << "  auto tmp = asn1::jeri::decode_object_identifier(" << v_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Enumerated: {
        out << ind << "{\n";
        out << ind << "  std::vector<std::string> jer_ids = {";
        for (std::size_t i = 0; i < rt.enumerated.root.size(); ++i) {
          if (i) {
            out << ", ";
          }
          out << "\"" << rt.enumerated.root[i].name << "\"";
        }
        for (const auto& nn : rt.enumerated.extensions) {
          out << ", \"" << nn.name << "\"";
        }
        out << "};\n";
        if (rt.jer.text_form != ir::JerEncoding::TextForm::AsIs) {
          out << ind << "  auto jer_id = asn1::jeri::decode_enumerated_text(" << v_expr
              << ", jer_ids, " << jer_text_form_expr(rt.jer.text_form) << ");\n";
        } else {
          out << ind << "  auto jer_id = asn1::jeri::decode_enumerated(" << v_expr << ");\n";
        }
        out << ind << "  if (!jer_id) return jer_id.error();\n";
        out << ind << "  const std::string& jer_name = jer_id.value();\n";
        out << ind << "  bool jer_enum_ok = false;\n";
        auto emit_enum_map = [&](const std::vector<ir::NamedNumber>& items) {
          for (const auto& nn : items) {
            out << ind << "  if (!jer_enum_ok && jer_name == \"" << nn.name << "\") { "
                << expr << " = static_cast<std::remove_cv_t<std::remove_reference_t<decltype(" << expr << ")>>>("
                << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
                << "); jer_enum_ok = true; }\n";
          }
        };
        emit_enum_map(rt.enumerated.root);
        emit_enum_map(rt.enumerated.extensions);
        out << ind << "  if (!jer_enum_ok) return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"unknown ENUMERATED value\");\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        const auto& fields =
            rt.kind == ir::TypeKind::Sequence ? rt.sequence.root : rt.set.root;
        if (rt.jer.array) {
          out << ind << "{\n";
          out << ind << "  auto jer_arr = asn1::jeri::decode_sequence_array(" << v_expr
              << ");\n";
          out << ind << "  if (!jer_arr) return jer_arr.error();\n";
          out << ind << "  const auto& jer_parts = *jer_arr.value();\n";
          out << ind << "  if (jer_parts.size() < " << fields.size() << "u) return "
                 "asn1::make_error(asn1::Error::Code::InvalidArgument, 0, \"SEQUENCE array too short\");\n";
          std::size_t idx = 0;
          for (const ir::Field& f : fields) {
            const std::string fname = cpp_ident(f.name);
            const ir::Type& ft = resolve(model_, f.type);
            const bool optional = f.presence == ir::Presence::Optional;
            if (optional) {
              out << ind << "  if (!jer_parts[" << idx << "].is_null()) {\n";
              emit_jer_field_decode_stmt(out, ft, expr + "." + fname,
                                         "jer_parts[" + std::to_string(idx) + "]",
                                         ind + "    ");
              out << ind << "  }\n";
            } else if (f.presence == ir::Presence::Default) {
              out << ind << "  if (!jer_parts[" << idx << "].is_null()) {\n";
              emit_jer_field_decode_stmt(out, ft, expr + "." + fname,
                                         "jer_parts[" + std::to_string(idx) + "]",
                                         ind + "    ");
              if (f.default_value) {
                const std::string def_expr = default_val_cpp(*f.default_value);
                if (!def_expr.empty()) {
                  out << ind << "  } else {\n"
                      << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
                }
              }
              out << ind << "  }\n";
            } else {
              emit_jer_field_decode_stmt(out, ft, expr + "." + fname,
                                         "jer_parts[" + std::to_string(idx) + "]", ind + "  ");
            }
            ++idx;
          }
          out << ind << "}\n";
        } else {
          out << ind << "{\n";
          for (const ir::Field& f : fields) {
            const std::string fname = cpp_ident(f.name);
            const ir::Type& ft = resolve(model_, f.type);
            const bool optional = f.presence == ir::Presence::Optional;
            std::string key_expr = "\"" + f.name + "\"";
            if (f.jer_name_form != ir::Field::JerNameForm::AsIs || !f.jer_name_literal.empty()) {
              key_expr = "asn1::jeri::transform_name(\"" + f.name + "\", " +
                         std::string(jer_name_form_expr(f.jer_name_form)) + ", \"" +
                         f.jer_name_literal + "\")";
            }
            out << ind << "  if (auto jer_fm = asn1::jeri::find_member(" << v_expr << ", "
                << key_expr << ")) {\n";
            out << ind << "    const asn1::jer::Value& jer_f = *jer_fm.value();\n";
            if (optional) {
              out << ind << "    " << type_cpp(f.type) << " jer_field{};\n";
              emit_jer_field_decode_stmt(out, ft, "jer_field", "jer_f", ind + "    ");
              out << ind << "    " << expr << "." << fname << " = std::move(jer_field);\n";
            } else {
              emit_jer_field_decode_stmt(out, ft, expr + "." + fname, "jer_f", ind + "    ");
            }
            if (f.presence == ir::Presence::Default && f.default_value) {
              const std::string def_expr = default_val_cpp(*f.default_value);
              if (!def_expr.empty()) {
                out << ind << "  } else {\n"
                    << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
              }
              out << ind << "  }\n";
            } else {
              out << ind << "  }\n";
            }
          }
          out << ind << "}\n";
        }
        break;
      }
      case ir::TypeKind::Choice: {
        if (rt.jer.unwrapped) {
          out << ind << "{\n";
          out << ind << "  auto jer_u = asn1::jeri::decode_choice_unwrapped(" << v_expr
              << ");\n";
          out << ind << "  if (!jer_u) return jer_u.error();\n";
          out << ind << "  const asn1::jer::Value& jer_alt = **jer_u.value();\n";
          for (std::size_t i = 0; i < rt.choice.alternatives.size(); ++i) {
            const ir::Field& f = rt.choice.alternatives[i];
            out << ind << "  if (i == " << i << ") { (void)0; }\n";
          }
          out << ind << "  return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
                 "\"UNWRAPPED CHOICE decode requires manual dispatch\");\n";
          out << ind << "}\n";
        } else {
          out << ind << "{\n";
          out << ind << "  auto jer_ch = asn1::jeri::decode_choice(" << v_expr << ");\n";
          out << ind << "  if (!jer_ch) return jer_ch.error();\n";
          out << ind << "  const std::string& jer_key = jer_ch.value().first;\n";
          out << ind << "  const asn1::jer::Value& jer_payload = *jer_ch.value().second;\n";
          for (const ir::Field* fp : choice_all_fields(rt.choice)) {
            const ir::Field& f = *fp;
            const ir::Type& ft = resolve(model_, f.type);
            std::string key_match = "jer_key == \"" + f.name + "\"";
            if (f.jer_name_form != ir::Field::JerNameForm::AsIs || !f.jer_name_literal.empty()) {
              key_match = "jer_key == asn1::jeri::transform_name(\"" + f.name + "\", " +
                          std::string(jer_name_form_expr(f.jer_name_form)) + ", \"" +
                          f.jer_name_literal + "\")";
            }
            out << ind << "  if (" << key_match << ") {\n";
            out << ind << "    " << cpp_ident(f.name) << "_ alt{};\n";
            emit_jer_field_decode_stmt(out, ft, "alt.value", "jer_payload", ind + "    ");
            out << ind << "    " << expr << ".alt = std::move(alt);\n";
            out << ind << "    return value;\n";
            out << ind << "  }\n";
          }
          out << ind << "  return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
                 "\"unknown CHOICE alternative\");\n";
          out << ind << "}\n";
        }
        break;
      }
      case ir::TypeKind::Real:
        out << ind << "{\n";
        out << ind << "  auto tmp = asn1::jeri::decode_real(" << v_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = tmp.value();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::SequenceOf: {
        const ir::TypeId elem_id = rt.sequence_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        out << ind << "{\n";
        out << ind << "  auto jer_arr = asn1::jeri::decode_sequence_of(" << v_expr << ");\n";
        out << ind << "  if (!jer_arr) return jer_arr.error();\n";
        out << ind << "  " << expr << ".value.clear();\n";
        out << ind << "  " << expr << ".value.reserve(jer_arr.value()->size());\n";
        out << ind << "  for (const auto& jer_item : *jer_arr.value()) {\n";
        out << ind << "    " << type_cpp(elem_id) << " jer_elem{};\n";
        emit_jer_field_decode_stmt(out, elem, "jer_elem", "jer_item", ind + "    ");
        out << ind << "    " << expr << ".value.push_back(std::move(jer_elem));\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id = rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        if (rt.jer.object) {
          const ir::Type* seq = &elem;
          if (seq->kind == ir::TypeKind::Referenced &&
              seq->referenced.resolved != ir::kInvalidType) {
            seq = &resolve(model_, seq->referenced.resolved);
          }
          if (seq->kind == ir::TypeKind::Sequence && seq->sequence.root.size() == 2u) {
            const ir::Field& kf = seq->sequence.root[0];
            const ir::Field& vf = seq->sequence.root[1];
            const std::string kname = cpp_ident(kf.name);
            const std::string vname = cpp_ident(vf.name);
            const ir::Type& vt = resolve(model_, vf.type);
            out << ind << "{\n";
            out << ind << "  auto jer_obj = asn1::jeri::decode_set_of_object(" << v_expr
                << ");\n";
            out << ind << "  if (!jer_obj) return jer_obj.error();\n";
            out << ind << "  " << expr << ".value.clear();\n";
            out << ind << "  " << expr << ".value.reserve(jer_obj.value().size());\n";
            out << ind << "  for (const auto& jer_ent : jer_obj.value()) {\n";
            out << ind << "    " << type_cpp(elem_id) << " jer_elem{};\n";
            out << ind << "    jer_elem." << kname << " = jer_ent.first;\n";
            emit_jer_field_decode_stmt(out, vt, "jer_elem." + vname, "*jer_ent.second",
                                       ind + "    ");
            out << ind << "    " << expr << ".value.push_back(std::move(jer_elem));\n";
            out << ind << "  }\n";
            out << ind << "}\n";
            break;
          }
        }
        out << ind << "{\n";
        out << ind << "  auto jer_arr = asn1::jeri::decode_sequence_of(" << v_expr << ");\n";
        out << ind << "  if (!jer_arr) return jer_arr.error();\n";
        out << ind << "  " << expr << ".value.clear();\n";
        out << ind << "  " << expr << ".value.reserve(jer_arr.value()->size());\n";
        out << ind << "  for (const auto& jer_item : *jer_arr.value()) {\n";
        out << ind << "    " << type_cpp(elem_id) << " jer_elem{};\n";
        emit_jer_field_decode_stmt(out, elem, "jer_elem", "jer_item", ind + "    ");
        out << ind << "    " << expr << ".value.push_back(std::move(jer_elem));\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Referenced:
      case ir::TypeKind::ObjectClassField:
      case ir::TypeKind::InstanceOf:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
               "\"JER decode not fully generated for this type\");\n";
        break;
    }
  }

  void emit_jer_field_decode_stmt(std::ostream& out, const ir::Type& ft,
                                  const std::string& lhs_expr, const std::string& v_expr,
                                  const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "{\n";
      out << ind << "  if (auto dr = decode_jer(" << v_expr << ", " << lhs_expr
          << "); !dr) return dr.error();\n";
      out << ind << "}\n";
      return;
    }
    emit_jer_decode_body(out, ft, lhs_expr, v_expr, ind);
  }

  bool xer_use_exer() const { return options_.codec == CodecKind::Exer; }
  bool xer_use_cxer() const { return options_.codec == CodecKind::Cxer; }
  const char* xml_ns() const { return xer_use_cxer() ? "asn1::cxer" : "asn1::xer"; }
  const char* xml_fn_suffix() const { return xer_use_cxer() ? "cxer" : "xer"; }

  static std::string transform_xml_name(const std::string& name, ir::Field::JerNameForm form,
                                        const std::string& literal) {
    switch (form) {
      case ir::Field::JerNameForm::Literal:
        return literal;
      case ir::Field::JerNameForm::Capitalized: {
        if (name.empty()) {
          return name;
        }
        std::string out = name;
        if (out[0] >= 'a' && out[0] <= 'z') {
          out[0] = static_cast<char>(out[0] - 'a' + 'A');
        }
        return out;
      }
      case ir::Field::JerNameForm::Uppercased: {
        std::string out = name;
        for (char& c : out) {
          if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
          }
        }
        return out;
      }
      case ir::Field::JerNameForm::Lowercased: {
        std::string out = name;
        for (char& c : out) {
          if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
          }
        }
        return out;
      }
      case ir::Field::JerNameForm::AsIs:
        break;
    }
    return name;
  }

  std::string xer_field_name(const ir::Field& f) const {
    return transform_xml_name(f.name, f.jer_name_form, f.jer_name_literal);
  }

  void emit_xer_enum_id_switch(std::ostream& out, const ir::Type& rt, const std::string& expr,
                               const std::string& ind) {
    out << ind << "std::string xer_enum_id;\n";
    out << ind << "switch (static_cast<std::int64_t>(" << expr << ")) {\n";
    for (const auto& nn : rt.enumerated.root) {
      out << ind << "  case " << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
          << ": xer_enum_id = \"" << nn.name << "\"; break;\n";
    }
    for (const auto& nn : rt.enumerated.extensions) {
      out << ind << "  case " << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
          << ": xer_enum_id = \"" << nn.name << "\"; break;\n";
    }
    out << ind << "  default: xer_enum_id = \""
        << (rt.enumerated.root.empty() ? "?" : rt.enumerated.root.front().name)
        << "\"; break;\n";
    out << ind << "}\n";
  }

  void emit_xer_field_encode_expr(std::ostream& out, const ir::Type& ft,
                                  const std::string& expr, const std::string& ind) {
    if (is_named(ft)) {
      out << "encode_" << xml_fn_suffix() << "(" << expr << ");\n";
    } else {
      out << "([&]() -> asn1::Result<asn1::exer::Element> {\n";
      emit_xer_encode_body(out, ft, expr, ind);
      out << ind << "})();\n";
    }
  }

  void emit_xer_field_decode_stmt(std::ostream& out, const ir::Type& ft,
                                 const std::string& lhs_expr, const std::string& el_expr,
                                 const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "{\n";
      out << ind << "  if (auto dr = decode_" << xml_fn_suffix() << "(" << el_expr << ", "
          << lhs_expr << "); !dr) return dr.error();\n";
      out << ind << "}\n";
      return;
    }
    emit_xer_decode_body(out, ft, lhs_expr, el_expr, ind);
  }

  void emit_xer_codec_fns(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    const char* suf = xml_fn_suffix();
    out << "inline asn1::Result<asn1::exer::Element> encode_" << suf << "(const " << name
        << "& value) {\n";
    emit_xer_encode_body(out, t, "value", "  ");
    out << "}\n\n";

    out << "inline asn1::Result<void> decode_" << suf << "(const asn1::exer::Element& el, "
        << name << "& value) {\n";
    emit_xer_decode_body(out, t, "value", "el", "  ");
    out << "  return asn1::Result<void>::success();\n}\n\n";
  }

  void emit_xer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const bool exer_mode = xer_use_exer();
    const std::string el_name = rt.name.empty() ? "Type" : rt.name;
    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        if (exer_mode && rt.exer.text) {
          out << ind << "return asn1::Result<asn1::exer::Element>::success("
              << "asn1::exer::encode_boolean_text(\"" << el_name << "\", " << expr << "));\n";
        } else {
          out << ind << "return asn1::Result<asn1::exer::Element>::success("
              << "" << xml_ns() << "::encode_boolean(\"" << el_name << "\", " << expr << "));\n";
        }
        break;
      case ir::TypeKind::Null:
        out << ind << "return asn1::Result<asn1::exer::Element>::success("
            << "" << xml_ns() << "::encode_null(\"" << el_name << "\"));\n";
        break;
      case ir::TypeKind::Integer:
        if (integer_is_bigint(rt.integer)) {
          out << ind << "return asn1::Result<asn1::exer::Element>::success("
              << "" << xml_ns() << "::encode_integer(\"" << el_name << "\", " << expr << "));\n";
        } else {
          out << ind << "return asn1::Result<asn1::exer::Element>::success("
              << "" << xml_ns() << "::encode_integer(\"" << el_name << "\", static_cast<std::int64_t>("
              << expr << ")));\n";
        }
        break;
      case ir::TypeKind::OctetString:
        if (exer_mode && rt.exer.base64) {
          out << ind << "return asn1::Result<asn1::exer::Element>::success("
              << "asn1::exer::encode_octet_string_base64(\"" << el_name
              << "\", asn1::Span<const std::uint8_t>(" << expr << ".data(), " << expr
              << ".size())));\n";
        } else {
          out << ind << "return asn1::Result<asn1::exer::Element>::success("
              << "" << xml_ns() << "::encode_octet_string(\"" << el_name
              << "\", asn1::Span<const std::uint8_t>(" << expr << ".data(), " << expr
              << ".size())));\n";
        }
        break;
      case ir::TypeKind::BitString:
        out << ind << "return asn1::Result<asn1::exer::Element>::success("
            << "" << xml_ns() << "::encode_bit_string(\"" << el_name
            << "\", asn1::Span<const std::uint8_t>(" << expr << ".bits.data(), " << expr
            << ".bits.size()), " << expr << ".bit_length));\n";
        break;
      case ir::TypeKind::String:
        out << ind << "return asn1::Result<asn1::exer::Element>::success("
            << "" << xml_ns() << "::encode_utf8_string(\"" << el_name << "\", " << expr << "));\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        out << ind << "return asn1::Result<asn1::exer::Element>::success("
            << "" << xml_ns() << "::encode_object_identifier(\"" << el_name
            << "\", asn1::Span<const std::uint64_t>(" << expr << ".data(), " << expr
            << ".size())));\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "return asn1::Result<asn1::exer::Element>::success("
            << "" << xml_ns() << "::encode_real(\"" << el_name << "\", " << expr << "));\n";
        break;
      case ir::TypeKind::Enumerated: {
        if (exer_mode && rt.exer.use_number) {
          out << ind << "return asn1::Result<asn1::exer::Element>::success("
              << "asn1::exer::encode_enumerated_number(\"" << el_name
              << "\", static_cast<std::int64_t>(" << expr << ")));\n";
        } else {
          emit_xer_enum_id_switch(out, rt, expr, ind);
          if (exer_mode && rt.exer.text) {
            out << ind << "return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_enumerated_text(\"" << el_name
                << "\", xer_enum_id));\n";
          } else {
            out << ind << "return asn1::Result<asn1::exer::Element>::success("
                << "" << xml_ns() << "::encode_enumerated(\"" << el_name << "\", xer_enum_id));\n";
          }
        }
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        if (exer_mode && rt.exer.list) {
          if (elem.kind == ir::TypeKind::String) {
            out << ind << "return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_strings(\"" << el_name << "\", " << expr
                << ".value));\n";
          } else if (elem.kind == ir::TypeKind::Integer && integer_is_bigint(elem.integer)) {
            out << ind << "return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_integers(\"" << el_name << "\", " << expr
                << ".value));\n";
          } else if (elem.kind == ir::TypeKind::Integer) {
            out << ind << "{\n";
            out << ind << "  std::vector<std::int64_t> xer_list;\n";
            out << ind << "  xer_list.reserve(" << expr << ".value.size());\n";
            out << ind << "  for (const auto& xer_item : " << expr << ".value) {\n";
            out << ind << "    xer_list.push_back(static_cast<std::int64_t>(xer_item));\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_integers(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else if (elem.kind == ir::TypeKind::Enumerated && elem.exer.use_number) {
            out << ind << "{\n";
            out << ind << "  std::vector<std::int64_t> xer_list;\n";
            out << ind << "  xer_list.reserve(" << expr << ".value.size());\n";
            out << ind << "  for (const auto& xer_item : " << expr << ".value) {\n";
            out << ind << "    xer_list.push_back(static_cast<std::int64_t>(xer_item));\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_integers(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else if (elem.kind == ir::TypeKind::Enumerated) {
            out << ind << "{\n";
            out << ind << "  std::vector<std::string> xer_list;\n";
            out << ind << "  xer_list.reserve(" << expr << ".value.size());\n";
            out << ind << "  for (const auto& xer_item : " << expr << ".value) {\n";
            out << ind << "    std::string xer_enum_id;\n";
            out << ind << "    switch (static_cast<std::int64_t>(xer_item)) {\n";
            for (const auto& nn : elem.enumerated.root) {
              out << ind << "      case "
                  << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
                  << ": xer_enum_id = \"" << nn.name << "\"; break;\n";
            }
            for (const auto& nn : elem.enumerated.extensions) {
              out << ind << "      case "
                  << (nn.value.as_i64 ? std::to_string(*nn.value.as_i64) : "0")
                  << ": xer_enum_id = \"" << nn.name << "\"; break;\n";
            }
            out << ind << "      default: xer_enum_id = \""
                << (elem.enumerated.root.empty() ? "?" : elem.enumerated.root.front().name)
                << "\"; break;\n";
            out << ind << "    }\n";
            out << ind << "    xer_list.push_back(std::move(xer_enum_id));\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_strings(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else if (elem.kind == ir::TypeKind::Boolean) {
            out << ind << "{\n";
            out << ind << "  std::vector<std::uint8_t> xer_list;\n";
            out << ind << "  xer_list.reserve(" << expr << ".value.size());\n";
            out << ind << "  for (const auto& xer_item : " << expr << ".value) {\n";
            out << ind << "    xer_list.push_back(xer_item ? 1 : 0);\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_booleans(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else if (elem.kind == ir::TypeKind::ObjectIdentifier ||
                     elem.kind == ir::TypeKind::RelativeOid) {
            out << ind << "return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_object_identifiers(\"" << el_name
                << "\", " << expr << ".value));\n";
          } else if (elem.kind == ir::TypeKind::Real) {
            out << ind << "return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_reals(\"" << el_name << "\", " << expr
                << ".value));\n";
          } else if (elem.kind == ir::TypeKind::OctetString) {
            out << ind << "return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_octet_strings(\"" << el_name << "\", "
                << expr << ".value));\n";
          } else if (elem.kind == ir::TypeKind::BitString) {
            out << ind << "{\n";
            out << ind << "  std::vector<asn1::xer::BitStringValue> xer_list;\n";
            out << ind << "  xer_list.reserve(" << expr << ".value.size());\n";
            out << ind << "  for (const auto& xer_item : " << expr << ".value) {\n";
            out << ind << "    asn1::xer::BitStringValue bs;\n";
            out << ind << "    bs.bits = xer_item.bits;\n";
            out << ind << "    bs.bit_length = xer_item.bit_length;\n";
            out << ind << "    xer_list.push_back(std::move(bs));\n";
            out << ind << "  }\n";
            out << ind << "  return asn1::Result<asn1::exer::Element>::success("
                << "asn1::exer::encode_list_of_bit_strings(\"" << el_name
                << "\", xer_list));\n";
            out << ind << "}\n";
          } else {
            out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
                   "\"LIST encoding requires a character-encodable element type\");\n";
          }
        } else {
          out << ind << "{\n";
          out << ind << "  std::vector<asn1::exer::Element> xer_items;\n";
          out << ind << "  xer_items.reserve(" << expr << ".value.size());\n";
          out << ind << "  for (const auto& xer_item : " << expr << ".value) {\n";
          out << ind << "    auto enc = ";
          emit_xer_field_encode_expr(out, elem, "xer_item", ind + "    ");
          out << ind << "    if (!enc) return enc.error();\n";
          out << ind << "    xer_items.push_back(std::move(enc.value()));\n";
          out << ind << "  }\n";
          out << ind << "  return asn1::Result<asn1::exer::Element>::success("
              << "" << xml_ns() << "::"
              << (xer_use_cxer() && rt.kind == ir::TypeKind::SetOf ? "encode_set_of"
                                                                  : "encode_sequence_of")
              << "(\"" << el_name << "\", std::move(xer_items)));\n";
          out << ind << "}\n";
        }
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  asn1::exer::Element root;\n";
        out << ind << "  root.name = \"" << el_name << "\";\n";
        out << ind << "  root.children.push_back(" << xml_ns() << "::encode_object_identifier(\"type-id\", "
            << "asn1::Span<const std::uint64_t>(" << expr
            << ".type_id.data(), " << expr << ".type_id.size())));\n";
        out << ind << "  root.children.push_back(" << xml_ns() << "::encode_octet_string(\"value\", "
            << "asn1::Span<const std::uint8_t>(" << expr << ".value.data(), " << expr
            << ".value.size())));\n";
        out << ind << "  return asn1::Result<asn1::exer::Element>::success(std::move(root));\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        out << ind << "{\n";
        out << ind << "  const std::size_t xer_idx = " << expr << ".alt.index();\n";
        out << ind << "  switch (xer_idx) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          const std::string xml_name = xer_field_name(f);
          out << ind << "    case " << i << ": {\n";
          out << ind << "      const auto& alt = std::get<" << i << ">(" << expr << ".alt);\n";
          out << ind << "      auto enc = ";
          emit_xer_field_encode_expr(out, ft, "alt.value", ind + "      ");
          out << ind << "      if (!enc) return enc.error();\n";
          out << ind << "      enc.value().name = \"" << xml_name << "\";\n";
          out << ind << "      return asn1::Result<asn1::exer::Element>::success("
              << "" << xml_ns() << "::encode_choice(\"" << el_name
              << "\", std::move(enc.value())));\n";
          out << ind << "    }\n";
        }
        out << ind << "    default:\n";
        out << ind << "      return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"CHOICE index out of range\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        const auto& fields =
            rt.kind == ir::TypeKind::Sequence ? rt.sequence.root : rt.set.root;
        out << ind << "{\n";
        out << ind << "  asn1::exer::Element seq;\n";
        out << ind << "  seq.name = \"" << el_name << "\";\n";
        for (const ir::Field& f : fields) {
          const std::string fname = cpp_ident(f.name);
          const std::string xml_name = xer_field_name(f);
          const ir::Type& ft = resolve(model_, f.type);
          const bool optional = f.presence == ir::Presence::Optional;
          if (exer_mode && f.exer.attribute) {
            const ir::Type& rft = ft.kind == ir::TypeKind::Referenced &&
                                          ft.referenced.resolved != ir::kInvalidType
                                      ? resolve(model_, ft.referenced.resolved)
                                      : ft;
            auto emit_attr = [&](const char* api, const std::string& val_expr) {
              if (optional) {
                out << ind << "  if (" << expr << "." << fname << ") {\n";
                out << ind << "    asn1::exer::" << api << "(seq, \"" << xml_name << "\", "
                    << val_expr << ");\n";
                out << ind << "  }\n";
              } else {
                out << ind << "  asn1::exer::" << api << "(seq, \"" << xml_name << "\", "
                    << val_expr << ");\n";
              }
            };
            if (rft.kind == ir::TypeKind::Boolean) {
              emit_attr("encode_attribute_boolean",
                        optional ? ("*" + expr + "." + fname) : (expr + "." + fname));
              continue;
            }
            if (rft.kind == ir::TypeKind::Integer || rft.kind == ir::TypeKind::Enumerated) {
              if (rft.kind == ir::TypeKind::Integer && integer_is_bigint(rft.integer)) {
                emit_attr("encode_attribute_integer",
                          optional ? ("*" + expr + "." + fname) : (expr + "." + fname));
              } else {
                emit_attr("encode_attribute_integer",
                          "static_cast<std::int64_t>(" +
                              (optional ? ("*" + expr + "." + fname) : (expr + "." + fname)) +
                              ")");
              }
              continue;
            }
            if (rft.kind == ir::TypeKind::String) {
              emit_attr("encode_attribute_string",
                        optional ? ("*" + expr + "." + fname) : (expr + "." + fname));
              continue;
            }
          }
          if (optional) {
            out << ind << "  if (" << expr << "." << fname << ") {\n";
            out << ind << "    auto enc = ";
            emit_xer_field_encode_expr(out, ft, "*" + expr + "." + fname, ind + "    ");
            out << ind << "    if (!enc) return enc.error();\n";
            if (exer_mode && f.exer.untagged) {
              out << ind << "    asn1::exer::append_untagged(seq, std::move(enc.value()));\n";
            } else {
              out << ind << "    enc.value().name = \"" << xml_name << "\";\n";
              out << ind << "    seq.children.push_back(std::move(enc.value()));\n";
            }
            out << ind << "  }";
            if (exer_mode && f.exer.use_nil) {
              out << " else {\n";
              out << ind << "    asn1::exer::Element nil;\n";
              out << ind << "    nil.name = \"" << xml_name << "\";\n";
              out << ind << "    asn1::exer::set_nil(nil, true);\n";
              out << ind << "    seq.children.push_back(std::move(nil));\n";
              out << ind << "  }\n";
            } else {
              out << "\n";
            }
          } else if (f.presence == ir::Presence::Default) {
            if (f.default_value) {
              std::string def_val = default_val_cpp(*f.default_value);
              if (!def_val.empty()) {
                out << ind << "  if (" << expr << "." << fname << " != " << def_val << ") {\n";
                out << ind << "    auto enc = ";
                emit_xer_field_encode_expr(out, ft, expr + "." + fname, ind + "    ");
                out << ind << "    if (!enc) return enc.error();\n";
                if (exer_mode && f.exer.untagged) {
                  out << ind << "    asn1::exer::append_untagged(seq, std::move(enc.value()));\n";
                } else {
                  out << ind << "    enc.value().name = \"" << xml_name << "\";\n";
                  out << ind << "    seq.children.push_back(std::move(enc.value()));\n";
                }
                out << ind << "  }\n";
                continue;
              }
            }
            out << ind << "  {\n";
            out << ind << "    auto enc = ";
            emit_xer_field_encode_expr(out, ft, expr + "." + fname, ind + "    ");
            out << ind << "    if (!enc) return enc.error();\n";
            if (exer_mode && f.exer.untagged) {
              out << ind << "    asn1::exer::append_untagged(seq, std::move(enc.value()));\n";
            } else {
              out << ind << "    enc.value().name = \"" << xml_name << "\";\n";
              out << ind << "    seq.children.push_back(std::move(enc.value()));\n";
            }
            out << ind << "  }\n";
          } else {
            out << ind << "  {\n";
            out << ind << "    auto enc = ";
            emit_xer_field_encode_expr(out, ft, expr + "." + fname, ind + "    ");
            out << ind << "    if (!enc) return enc.error();\n";
            if (exer_mode && f.exer.untagged) {
              out << ind << "    asn1::exer::append_untagged(seq, std::move(enc.value()));\n";
            } else {
              out << ind << "    enc.value().name = \"" << xml_name << "\";\n";
              out << ind << "    seq.children.push_back(std::move(enc.value()));\n";
            }
            out << ind << "  }\n";
          }
        }
        out << ind << "  return asn1::Result<asn1::exer::Element>::success(std::move(seq));\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_xer_encode_body(out, resolve(model_, rt.referenced.resolved), expr, ind);
        } else {
          out << ind << "return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
                 "\"unresolved type reference\");\n";
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
               "\"XER encoding not generated for this type\");\n";
        break;
    }
  }

  void emit_xer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& el_expr, const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const bool exer_mode = xer_use_exer();
    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "{\n";
        if (exer_mode && rt.exer.text) {
          out << ind << "  auto tmp = asn1::exer::decode_boolean_text(" << el_expr << ");\n";
        } else {
          out << ind << "  auto tmp = " << xml_ns() << "::decode_boolean(" << el_expr << ");\n";
        }
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = tmp.value();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_null(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Integer:
        out << ind << "{\n";
        if (integer_is_bigint(rt.integer)) {
          out << ind << "  auto tmp = " << xml_ns() << "::decode_big_integer(" << el_expr << ");\n";
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          out << ind << "  auto tmp = " << xml_ns() << "::decode_integer(" << el_expr << ");\n";
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = static_cast<" << integer_host_type(rt.integer)
              << ">(tmp.value());\n";
        }
        out << ind << "}\n";
        break;
      case ir::TypeKind::OctetString:
        out << ind << "{\n";
        if (exer_mode && rt.exer.base64) {
          out << ind << "  auto tmp = asn1::exer::decode_octet_string_base64(" << el_expr
              << ");\n";
        } else {
          out << ind << "  auto tmp = " << xml_ns() << "::decode_octet_string(" << el_expr << ");\n";
        }
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_bit_string(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << ".bits = std::move(tmp.value().bits);\n";
        out << ind << "  " << expr << ".bit_length = tmp.value().bit_length;\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_utf8_string(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_real(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = tmp.value();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
      case ir::TypeKind::RelativeOid:
        out << ind << "{\n";
        out << ind << "  auto tmp = " << xml_ns() << "::decode_object_identifier(" << el_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  " << expr << " = std::move(tmp.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Enumerated: {
        out << ind << "{\n";
        if (exer_mode && rt.exer.use_number) {
          out << ind << "  auto tmp = asn1::exer::decode_enumerated_number(" << el_expr << ");\n";
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = static_cast<"
              << (is_named(rt) ? named_type_cpp(rt) : "std::int64_t") << ">(tmp.value());\n";
        } else {
          if (exer_mode && rt.exer.text) {
            out << ind << "  auto tmp = asn1::exer::decode_enumerated_text(" << el_expr << ");\n";
          } else {
            out << ind << "  auto tmp = " << xml_ns() << "::decode_enumerated(" << el_expr << ");\n";
          }
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  bool xer_enum_ok = false;\n";
          auto emit_match = [&](const ir::NamedNumber& nn) {
            out << ind << "  if (!xer_enum_ok && tmp.value() == \"" << nn.name << "\") { "
                << expr << " = static_cast<" << (is_named(rt) ? named_type_cpp(rt) : "std::int64_t")
                << ">(" << (nn.value.as_i64 ? *nn.value.as_i64 : 0)
                << "); xer_enum_ok = true; }\n";
          };
          for (const auto& nn : rt.enumerated.root) {
            emit_match(nn);
          }
          for (const auto& nn : rt.enumerated.extensions) {
            emit_match(nn);
          }
          out << ind << "  if (!xer_enum_ok) return asn1::make_error("
                 "asn1::Error::Code::InvalidArgument, 0, \"unknown ENUMERATED identifier\");\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        out << ind << "{\n";
        if (exer_mode && rt.exer.list) {
          if (elem.kind == ir::TypeKind::String) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_strings(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::Integer && integer_is_bigint(elem.integer)) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_big_integers(" << el_expr
                << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::Integer) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_integers(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value.clear();\n";
            out << ind << "  " << expr << ".value.reserve(tmp.value().size());\n";
            out << ind << "  for (std::int64_t xer_n : tmp.value()) {\n";
            out << ind << "    " << expr << ".value.push_back(static_cast<"
                << type_cpp(elem_id) << ">(xer_n));\n";
            out << ind << "  }\n";
          } else if (elem.kind == ir::TypeKind::Enumerated && elem.exer.use_number) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_integers(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value.clear();\n";
            out << ind << "  " << expr << ".value.reserve(tmp.value().size());\n";
            out << ind << "  for (std::int64_t xer_n : tmp.value()) {\n";
            out << ind << "    " << expr << ".value.push_back(static_cast<"
                << type_cpp(elem_id) << ">(xer_n));\n";
            out << ind << "  }\n";
          } else if (elem.kind == ir::TypeKind::Enumerated) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_strings(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value.clear();\n";
            out << ind << "  " << expr << ".value.reserve(tmp.value().size());\n";
            out << ind << "  for (const auto& xer_tok : tmp.value()) {\n";
            out << ind << "    bool xer_enum_ok = false;\n";
            auto emit_match = [&](const ir::NamedNumber& nn) {
              out << ind << "    if (!xer_enum_ok && xer_tok == \"" << nn.name << "\") { "
                  << expr << ".value.push_back(static_cast<" << type_cpp(elem_id) << ">("
                  << (nn.value.as_i64 ? *nn.value.as_i64 : 0)
                  << ")); xer_enum_ok = true; }\n";
            };
            for (const auto& nn : elem.enumerated.root) {
              emit_match(nn);
            }
            for (const auto& nn : elem.enumerated.extensions) {
              emit_match(nn);
            }
            out << ind << "    if (!xer_enum_ok) return asn1::make_error("
                   "asn1::Error::Code::InvalidArgument, 0, "
                   "\"unknown ENUMERATED identifier in LIST\");\n";
            out << ind << "  }\n";
          } else if (elem.kind == ir::TypeKind::Boolean) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_booleans(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value.clear();\n";
            out << ind << "  " << expr << ".value.reserve(tmp.value().size());\n";
            out << ind << "  for (std::uint8_t xer_b : tmp.value()) {\n";
            out << ind << "    " << expr << ".value.push_back(xer_b != 0);\n";
            out << ind << "  }\n";
          } else if (elem.kind == ir::TypeKind::ObjectIdentifier ||
                     elem.kind == ir::TypeKind::RelativeOid) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_object_identifiers("
                << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::Real) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_reals(" << el_expr << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::OctetString) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_octet_strings(" << el_expr
                << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value = std::move(tmp.value());\n";
          } else if (elem.kind == ir::TypeKind::BitString) {
            out << ind << "  auto tmp = asn1::exer::decode_list_of_bit_strings(" << el_expr
                << ");\n";
            out << ind << "  if (!tmp) return tmp.error();\n";
            out << ind << "  " << expr << ".value.clear();\n";
            out << ind << "  " << expr << ".value.reserve(tmp.value().size());\n";
            out << ind << "  for (auto& xer_bs : tmp.value()) {\n";
            out << ind << "    " << type_cpp(elem_id) << " xer_item{};\n";
            out << ind << "    xer_item.bits = std::move(xer_bs.bits);\n";
            out << ind << "    xer_item.bit_length = xer_bs.bit_length;\n";
            out << ind << "    " << expr << ".value.push_back(std::move(xer_item));\n";
            out << ind << "  }\n";
          } else {
            out << ind << "  return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
                   "\"LIST decode requires a character-encodable element type\");\n";
          }
        } else {
          out << ind << "  " << expr << ".value.clear();\n";
          out << ind << "  for (const auto& xer_child : " << el_expr << ".children) {\n";
          out << ind << "    " << type_cpp(elem_id) << " xer_item{};\n";
          emit_xer_field_decode_stmt(out, elem, "xer_item", "xer_child", ind + "    ");
          out << ind << "    " << expr << ".value.push_back(std::move(xer_item));\n";
          out << ind << "  }\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  auto tid = " << xml_ns() << "::find_child(" << el_expr << ", \"type-id\");\n";
        out << ind << "  if (!tid) return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"missing type-id\");\n";
        out << ind << "  auto oid = " << xml_ns() << "::decode_object_identifier(*tid.value());\n";
        out << ind << "  if (!oid) return oid.error();\n";
        out << ind << "  " << expr << ".type_id = std::move(oid.value());\n";
        out << ind << "  auto val = " << xml_ns() << "::find_child(" << el_expr << ", \"value\");\n";
        out << ind << "  if (!val) return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"missing value\");\n";
        out << ind << "  auto oct = " << xml_ns() << "::decode_octet_string(*val.value());\n";
        out << ind << "  if (!oct) return oct.error();\n";
        out << ind << "  " << expr << ".value = std::move(oct.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Choice: {
        out << ind << "{\n";
        out << ind << "  if (" << el_expr << ".children.size() != 1) {\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"CHOICE expects one child\");\n";
        out << ind << "  }\n";
        out << ind << "  const auto& xer_alt = " << el_expr << ".children[0];\n";
        for (const ir::Field* fp : choice_all_fields(rt.choice)) {
          const ir::Field& f = *fp;
          const ir::Type& ft = resolve(model_, f.type);
          const std::string xml_name = xer_field_name(f);
          out << ind << "  if (xer_alt.name == \"" << xml_name << "\") {\n";
          out << ind << "    " << cpp_ident(f.name) << "_ alt{};\n";
          emit_xer_field_decode_stmt(out, ft, "alt.value", "xer_alt", ind + "    ");
          out << ind << "    " << expr << ".alt = std::move(alt);\n";
          out << ind << "  } else";
        }
        out << " {\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
               "\"unknown CHOICE alternative\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        const auto& fields =
            rt.kind == ir::TypeKind::Sequence ? rt.sequence.root : rt.set.root;
        out << ind << "{\n";
        for (const ir::Field& f : fields) {
          const std::string fname = cpp_ident(f.name);
          const std::string xml_name = xer_field_name(f);
          const ir::Type& ft = resolve(model_, f.type);
          const bool optional = f.presence == ir::Presence::Optional;
          if (exer_mode && f.exer.attribute) {
            const ir::Type& rft = ft.kind == ir::TypeKind::Referenced &&
                                          ft.referenced.resolved != ir::kInvalidType
                                      ? resolve(model_, ft.referenced.resolved)
                                      : ft;
            out << ind << "  {\n";
            if (rft.kind == ir::TypeKind::Boolean) {
              out << ind << "    auto tmp = asn1::exer::decode_attribute_boolean(" << el_expr
                  << ", \"" << xml_name << "\");\n";
              out << ind << "    if (tmp) " << expr << "." << fname << " = tmp.value();\n";
            } else if (rft.kind == ir::TypeKind::Integer && integer_is_bigint(rft.integer)) {
              out << ind << "    auto tmp = asn1::exer::get_attribute(" << el_expr << ", \""
                  << xml_name << "\");\n";
              out << ind << "    if (tmp) {\n";
              out << ind << "      auto bi = asn1::BigInteger::from_decimal(tmp.value());\n";
              out << ind << "      if (bi) " << expr << "." << fname
                  << " = std::move(bi.value());\n";
              out << ind << "    }\n";
            } else if (rft.kind == ir::TypeKind::Integer || rft.kind == ir::TypeKind::Enumerated) {
              out << ind << "    auto tmp = asn1::exer::get_attribute(" << el_expr << ", \""
                  << xml_name << "\");\n";
              out << ind << "    if (tmp) " << expr << "." << fname
                  << " = static_cast<" << type_cpp(f.type)
                  << ">(std::stoll(tmp.value()));\n";
            } else if (rft.kind == ir::TypeKind::String) {
              out << ind << "    auto tmp = asn1::exer::get_attribute(" << el_expr << ", \""
                  << xml_name << "\");\n";
              out << ind << "    if (tmp) " << expr << "." << fname << " = tmp.value();\n";
            }
            out << ind << "  }\n";
            continue;
          }
          if (exer_mode && f.exer.untagged) {
            // UNTAGGED: component content is merged into the parent element.
            out << ind << "  {\n";
            if (optional) {
              out << ind << "    " << type_cpp(f.type) << " xer_field{};\n";
              emit_xer_field_decode_stmt(out, ft, "xer_field", el_expr, ind + "    ");
              out << ind << "    " << expr << "." << fname << " = std::move(xer_field);\n";
            } else {
              emit_xer_field_decode_stmt(out, ft, expr + "." + fname, el_expr, ind + "    ");
            }
            out << ind << "  }\n";
            continue;
          }
          out << ind << "  {\n";
          out << ind << "    auto child = " << xml_ns() << "::find_child(" << el_expr << ", \"" << xml_name
              << "\");\n";
          out << ind << "    if (child) {\n";
          if (exer_mode && f.exer.use_nil) {
            out << ind << "      auto xer_nil = asn1::exer::is_nil(*child.value());\n";
            out << ind << "      if (xer_nil && xer_nil.value()) {\n";
            if (optional) {
              out << ind << "        " << expr << "." << fname << " = std::nullopt;\n";
            }
            out << ind << "      } else {\n";
            if (optional) {
              out << ind << "        " << type_cpp(f.type) << " xer_field{};\n";
              emit_xer_field_decode_stmt(out, ft, "xer_field", "*child.value()",
                                         ind + "        ");
              out << ind << "        " << expr << "." << fname << " = std::move(xer_field);\n";
            } else {
              emit_xer_field_decode_stmt(out, ft, expr + "." + fname, "*child.value()",
                                         ind + "        ");
            }
            out << ind << "      }\n";
          } else if (optional) {
            out << ind << "      " << type_cpp(f.type) << " xer_field{};\n";
            emit_xer_field_decode_stmt(out, ft, "xer_field", "*child.value()", ind + "      ");
            out << ind << "      " << expr << "." << fname << " = std::move(xer_field);\n";
          } else {
            emit_xer_field_decode_stmt(out, ft, expr + "." + fname, "*child.value()",
                                       ind + "      ");
          }
          out << ind << "    } else if (" << (f.presence == ir::Presence::Default ? "true" : "false") << ") {\n";
          if (f.presence == ir::Presence::Default && f.default_value) {
            const std::string def_expr = default_val_cpp(*f.default_value);
            if (!def_expr.empty()) {
              out << ind << "      " << expr << "." << fname << " = " << def_expr << ";\n";
            }
          }
          out << ind << "    }\n";
          out << ind << "  }\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_xer_decode_body(out, resolve(model_, rt.referenced.resolved), expr, el_expr, ind);
        } else {
          out << ind << "return asn1::make_error(asn1::Error::Code::InvalidArgument, 0, "
                 "\"unresolved type reference\");\n";
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
               "\"XER decode not generated for this type\");\n";
        break;
    }
  }
  // ---- OER ----

  bool oer_use_coer() const { return options_.codec == CodecKind::Coer; }
  const char* oer_ns() const { return oer_use_coer() ? "asn1::coer" : "asn1::oer"; }
  const char* oer_fn_suffix() const { return oer_use_coer() ? "coer" : "oer"; }


  std::string oer_integer_constraint_expr(const ir::IntegerDesc& d) {
    std::ostringstream os;
    os << "" << oer_ns() << "::IntegerConstraint{" << opt_i64_expr(d.constraint.lower) << ", "
       << opt_i64_expr(d.constraint.upper) << ", "
       << (d.constraint.extensible ? "true" : "false");
    if (d.constraint.root_ranges.size() > 1) {
      os << ", {";
      for (std::size_t i = 0; i < d.constraint.root_ranges.size(); ++i) {
        if (i > 0) os << ", ";
        os << "" << oer_ns() << "::IntegerRange{" << opt_i64_expr(d.constraint.root_ranges[i].lower) << ", "
           << opt_i64_expr(d.constraint.root_ranges[i].upper) << "}";
      }
      os << "}";
    }
    os << "}";
    return os.str();
  }

  std::string oer_size_constraint_expr(const ir::ConstraintDesc& d) {
    std::ostringstream os;
    os << "" << oer_ns() << "::SizeConstraint{" << opt_size_expr(d.lower) << ", "
       << opt_size_expr(d.upper) << ", " << (d.extensible ? "true" : "false");
    if (d.root_ranges.size() > 1) {
      os << ", {";
      for (std::size_t i = 0; i < d.root_ranges.size(); ++i) {
        if (i > 0) os << ", ";
        os << "" << oer_ns() << "::SizeRange{" << opt_size_expr(d.root_ranges[i].lower) << ", "
           << opt_size_expr(d.root_ranges[i].upper) << "}";
      }
      os << "}";
    }
    os << "}";
    return os.str();
  }

  std::string oer_real_form_expr(ir::RealIeeeForm f) {
    const std::string ns = oer_ns();
    switch (f) {
      case ir::RealIeeeForm::Binary32:
        return ns + "::RealIeeeForm::Binary32";
      case ir::RealIeeeForm::Binary64:
        return ns + "::RealIeeeForm::Binary64";
      case ir::RealIeeeForm::Unconstrained:
      default:
        return ns + "::RealIeeeForm::Unconstrained";
    }
  }

  static bool type_is_constructed(const ir::Type& t) {
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

  void emit_oer_codec_fns(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    out << "inline asn1::Result<void> encode_" << oer_fn_suffix() << "(asn1::ByteWriter& w, const " << name
        << "& value) {\n";
    emit_oer_encode_body(out, t, "value", "  ");
    out << "  return asn1::Result<void>::success();\n}\n\n";

    out << "inline asn1::Result<void> decode_" << oer_fn_suffix() << "(asn1::ByteReader& r, "
        << name << "& value) {\n";
    emit_oer_decode_body(out, t, "value", "  ");
    out << "  return asn1::Result<void>::success();\n}\n\n";
  }

  void emit_oer_field_encode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                             const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "if (auto r = encode_" << oer_fn_suffix() << "(w, " << expr << "); !r) return r;\n";
    } else {
      emit_oer_encode_body(out, ft, expr, ind);
    }
  }

  void emit_oer_field_decode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                             const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "if (auto dr = decode_" << oer_fn_suffix() << "(r, " << expr
          << "); !dr) return dr;\n";
    } else {
      emit_oer_decode_body(out, ft, expr, ind);
    }
  }

  void emit_oer_extension_group_encode(std::ostream& out, const std::vector<ir::Field>& group,
                                       const std::string& expr, const std::string& ind) {
    std::vector<const ir::Field*> optionals;
    for (const ir::Field& f : group) {
      if (field_has_presence_bit(f)) {
        optionals.push_back(&f);
      }
    }
    if (!optionals.empty()) {
      out << ind << "{\n";
      out << ind << "  bool gopt[" << optionals.size() << "] = {};\n";
      for (std::size_t i = 0; i < optionals.size(); ++i) {
        const ir::Field& pf = *optionals[i];
        out << ind << "  gopt[" << i << "] = ";
        if (pf.presence == ir::Presence::Default) {
          out << "true;\n";
        } else {
          out << expr << "." << cpp_ident(pf.name) << ".has_value();\n";
        }
      }
      out << ind << "  " << oer_ns() << "::encode_sequence_preamble(w, false, false, "
          << "asn1::Span<const bool>(gopt, " << optionals.size() << "));\n";
      out << ind << "}\n";
    }
    for (const ir::Field& f : group) {
      const std::string fname = cpp_ident(f.name);
      const ir::Type& ft = resolve(model_, f.type);
      if (f.presence == ir::Presence::Optional) {
        out << ind << "if (" << expr << "." << fname << ") {\n";
        emit_oer_field_encode(out, ft, expr + "." + fname + ".value()", ind + "  ");
        out << ind << "}\n";
      } else {
        emit_oer_field_encode(out, ft, expr + "." + fname, ind);
      }
    }
  }

  void emit_oer_extension_group_decode(std::ostream& out, const std::vector<ir::Field>& group,
                                       const std::string& expr, const std::string& ind) {
    std::vector<const ir::Field*> optionals;
    for (const ir::Field& f : group) {
      if (field_has_presence_bit(f)) {
        optionals.push_back(&f);
      }
    }
    if (!optionals.empty()) {
      out << ind << "{\n";
      out << ind << "  auto gbm = " << oer_ns() << "::decode_sequence_preamble(r, false, "
          << optionals.size() << ");\n";
      out << ind << "  if (!gbm) return gbm.error();\n";
      std::size_t opt_i = 0;
      for (const ir::Field& f : group) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        if (field_has_presence_bit(f)) {
          out << ind << "  if (gbm.value().optionals.size() > " << opt_i
              << " && gbm.value().optionals[" << opt_i << "]) {\n";
          if (f.presence == ir::Presence::Optional) {
            out << ind << "    " << type_cpp(f.type) << " field{};\n";
            emit_oer_field_decode(out, ft, "field", ind + "    ");
            out << ind << "    " << expr << "." << fname << " = std::move(field);\n";
          } else {
            emit_oer_field_decode(out, ft, expr + "." + fname, ind + "    ");
          }
          out << ind << "  }\n";
          ++opt_i;
        } else {
          out << ind << "  {\n"
              << ind << "    " << type_cpp(f.type) << " field{};\n";
          emit_oer_field_decode(out, ft, "field", ind + "    ");
          out << ind << "    " << expr << "." << fname << " = std::move(field);\n"
              << ind << "  }\n";
        }
      }
      out << ind << "}\n";
    } else {
      for (const ir::Field& f : group) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        out << ind << "{\n"
            << ind << "  " << type_cpp(f.type) << " field{};\n";
        emit_oer_field_decode(out, ft, "field", ind + "  ");
        out << ind << "  " << expr << "." << fname << " = std::move(field);\n"
            << ind << "}\n";
      }
    }
  }

  void emit_oer_sequence_encode(std::ostream& out, const ir::SequenceDesc& seq,
                                const std::string& expr, const std::string& ind) {
    std::vector<const ir::Field*> optionals;
    for (const ir::Field& f : seq.root) {
      if (field_has_presence_bit(f)) {
        optionals.push_back(&f);
      }
    }
    for (const ir::Field& f : seq.trailing_root) {
      if (field_has_presence_bit(f)) {
        optionals.push_back(&f);
      }
    }
    const std::size_t n_ext = seq.extension_groups.size();
    out << ind << "{\n";
    out << ind << "  bool opt[" << (optionals.empty() ? 1 : optionals.size()) << "] = {};\n";
    for (std::size_t i = 0; i < optionals.size(); ++i) {
      const ir::Field& pf = *optionals[i];
      out << ind << "  opt[" << i << "] = ";
      if (pf.presence == ir::Presence::Default) {
        if (pf.default_value) {
          std::string def_val = default_val_cpp(*pf.default_value);
          if (!def_val.empty()) {
            out << "(" << expr << "." << cpp_ident(pf.name) << " != " << def_val << ");\n";
          } else {
            out << "true;\n";
          }
        } else {
          out << "true;\n";
        }
      } else {
        out << expr << "." << cpp_ident(pf.name) << ".has_value();\n";
      }
    }
    out << ind << "  bool ext_present = false;\n";
    if (seq.extensible && n_ext > 0) {
      out << ind << "  bool ep[" << n_ext << "] = {};\n";
      for (std::size_t g = 0; g < n_ext; ++g) {
        out << ind << "  ep[" << g << "] = (";
        const auto& group = seq.extension_groups[g];
        for (std::size_t i = 0; i < group.size(); ++i) {
          if (i) out << " || ";
          emit_ext_group_field_present(out, group[i], expr);
        }
        if (group.empty()) out << "false";
        out << ");\n";
        out << ind << "  if (ep[" << g << "]) ext_present = true;\n";
      }
    }
    out << ind << "  " << oer_ns() << "::encode_sequence_preamble(w, "
        << (seq.extensible ? "true" : "false") << ", ext_present, "
        << "asn1::Span<const bool>(opt, " << optionals.size() << "));\n";
    out << ind << "}\n";
    auto emit_fields_encode = [&](const std::vector<ir::Field>& fields) {
      for (const ir::Field& f : fields) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        if (f.presence == ir::Presence::Optional) {
          out << ind << "if (" << expr << "." << fname << ") {\n";
          emit_oer_field_encode(out, ft, expr + "." + fname + ".value()", ind + "  ");
          out << ind << "}\n";
        } else if (f.presence == ir::Presence::Default) {
          if (f.default_value) {
            std::string def_val = default_val_cpp(*f.default_value);
            if (!def_val.empty()) {
              out << ind << "if (" << expr << "." << fname << " != " << def_val << ") {\n";
              emit_oer_field_encode(out, ft, expr + "." + fname, ind + "  ");
              out << ind << "}\n";
              continue;
            }
          }
          emit_oer_field_encode(out, ft, expr + "." + fname, ind);
        } else {
          emit_oer_field_encode(out, ft, expr + "." + fname, ind);
        }
      }
    };
    emit_fields_encode(seq.root);
    emit_fields_encode(seq.trailing_root);
    if (seq.extensible && n_ext > 0) {
      out << ind << "{\n";
      out << ind << "  bool ep[" << n_ext << "] = {};\n";
      out << ind << "  bool ext_present = false;\n";
      for (std::size_t g = 0; g < n_ext; ++g) {
        out << ind << "  ep[" << g << "] = (";
        const auto& group = seq.extension_groups[g];
        for (std::size_t i = 0; i < group.size(); ++i) {
          if (i) out << " || ";
          emit_ext_group_field_present(out, group[i], expr);
        }
        if (group.empty()) out << "false";
        out << ");\n";
        out << ind << "  if (ep[" << g << "]) ext_present = true;\n";
      }
      out << ind << "  if (ext_present) {\n";
      out << ind << "    std::vector<std::vector<std::uint8_t>> ots;\n";
      for (std::size_t g = 0; g < n_ext; ++g) {
        out << ind << "    if (ep[" << g << "]) {\n";
        out << ind << "      asn1::ByteWriter ow;\n";
        out << ind << "      {\n";
        out << ind << "        asn1::ByteWriter& w = ow;\n";
        emit_oer_extension_group_encode(out, seq.extension_groups[g], expr, ind + "        ");
        out << ind << "      }\n";
        out << ind << "      ots.push_back(ow.take());\n";
        out << ind << "    }\n";
      }
      out << ind << "    " << oer_ns() << "::encode_extension_additions(w, "
          << "asn1::Span<const bool>(ep, " << n_ext << "), ots);\n";
      out << ind << "  }\n";
      out << ind << "}\n";
    }
  }

  void emit_oer_sequence_decode(std::ostream& out, const ir::SequenceDesc& seq,
                                const std::string& expr, const std::string& ind) {
    std::size_t n_optionals = 0;
    for (const ir::Field& f : seq.root) {
      if (field_has_presence_bit(f)) {
        ++n_optionals;
      }
    }
    for (const ir::Field& f : seq.trailing_root) {
      if (field_has_presence_bit(f)) {
        ++n_optionals;
      }
    }
    const std::size_t n_ext = seq.extension_groups.size();
    out << ind << "{\n";
    out << ind << "  auto pre = " << oer_ns() << "::decode_sequence_preamble(r, "
        << (seq.extensible ? "true" : "false") << ", " << n_optionals << ");\n";
    out << ind << "  if (!pre) return pre.error();\n";
    std::size_t opt_i = 0;
    auto emit_fields = [&](const std::vector<ir::Field>& fields) {
      for (const ir::Field& f : fields) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        if (f.presence == ir::Presence::Optional) {
          out << ind << "  if (pre.value().optionals.size() > " << opt_i
              << " && pre.value().optionals[" << opt_i << "]) {\n"
              << ind << "    " << type_cpp(f.type) << " field{};\n";
          emit_oer_field_decode(out, ft, "field", ind + "    ");
          out << ind << "    " << expr << "." << fname << " = std::move(field);\n"
              << ind << "  } else {\n"
              << ind << "    " << expr << "." << fname << " = std::nullopt;\n"
              << ind << "  }\n";
          ++opt_i;
        } else if (f.presence == ir::Presence::Default) {
          out << ind << "  if (pre.value().optionals.size() > " << opt_i
              << " && pre.value().optionals[" << opt_i << "]) {\n";
          emit_oer_field_decode(out, ft, expr + "." + fname, ind + "    ");
          if (f.default_value) {
            const std::string def_expr = default_val_cpp(*f.default_value);
            if (!def_expr.empty()) {
              out << ind << "  } else {\n"
                  << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
            }
          }
          out << ind << "  }\n";
          ++opt_i;
        } else {
          emit_oer_field_decode(out, ft, expr + "." + fname, ind + "  ");
        }
      }
    };
    emit_fields(seq.root);
    emit_fields(seq.trailing_root);
    if (seq.extensible) {
      out << ind << "  if (pre.value().extensions_present) {\n";
      out << ind << "    auto ext = " << oer_ns() << "::decode_extension_additions(r);\n";
      out << ind << "    if (!ext) return ext.error();\n";
      if (n_ext > 0) {
        out << ind << "    std::size_t ot_i = 0;\n";
        out << ind << "    for (std::size_t gi = 0; gi < ext.value().presence.size(); ++gi) {\n";
        out << ind << "      if (!ext.value().presence[gi]) continue;\n";
        out << ind << "      if (ot_i >= ext.value().open_types.size()) break;\n";
        out << ind << "      const auto& ot = ext.value().open_types[ot_i++];\n";
        out << ind << "      if (gi >= " << n_ext << "u) continue;\n";
        out << ind << "      asn1::ByteReader er(ot);\n";
        out << ind << "      {\n";
        out << ind << "        asn1::ByteReader& r = er;\n";
        out << ind << "        switch (gi) {\n";
        for (std::size_t g = 0; g < n_ext; ++g) {
          out << ind << "          case " << g << ":\n";
          emit_oer_extension_group_decode(out, seq.extension_groups[g], expr,
                                          ind + "            ");
          out << ind << "            break;\n";
        }
        out << ind << "          default: break;\n";
        out << ind << "        }\n";
        out << ind << "      }\n";
        out << ind << "    }\n";
      }
      out << ind << "  }\n";
    }
    out << ind << "}\n";
  }

  void emit_oer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "" << oer_ns() << "::encode_boolean(w, " << expr << ");\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "" << oer_ns() << "::encode_null(w);\n";
        break;
      case ir::TypeKind::Integer:
        if (integer_is_bigint(rt.integer)) {
          out << ind << "" << oer_ns() << "::encode_integer(w, " << expr << ", "
              << oer_integer_constraint_expr(rt.integer) << ");\n";
        } else {
          out << ind << "" << oer_ns() << "::encode_integer(w, static_cast<std::int64_t>(" << expr
              << "), " << oer_integer_constraint_expr(rt.integer) << ");\n";
        }
        break;
      case ir::TypeKind::OctetString:
        out << ind << "" << oer_ns() << "::encode_octet_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".data(), " << expr << ".size()), "
            << oer_size_constraint_expr(rt.octet_string.size) << ");\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "" << oer_ns() << "::encode_bit_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".bits.data(), " << expr << ".bits.size()), " << expr
            << ".bit_length, " << oer_size_constraint_expr(rt.bit_string.size) << ");\n";
        break;
      case ir::TypeKind::String:
        out << ind << "" << oer_ns() << "::encode_utf8_string(w, " << expr << ", "
            << oer_size_constraint_expr(rt.string.size) << ");\n";
        break;
      case ir::TypeKind::Enumerated:
        out << ind << "" << oer_ns() << "::encode_enumerated(w, static_cast<std::int64_t>(" << expr
            << "));\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
        out << ind << "" << oer_ns() << "::encode_object_identifier(w, asn1::Span<const std::uint64_t>("
            << expr << ".data(), " << expr << ".size()), false);\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << "" << oer_ns() << "::encode_object_identifier(w, asn1::Span<const std::uint64_t>("
            << expr << ".data(), " << expr << ".size()), true);\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "{\n";
        out << ind << "  auto rr = " << oer_ns() << "::encode_real(w, " << expr << ", "
            << oer_real_form_expr(rt.real.ieee_form) << ");\n";
        out << ind << "  if (!rr) return rr.error();\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        if (oer_use_coer() && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "{\n";
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n";
          out << ind << "  parts.reserve(" << expr << ".value.size());\n";
          out << ind << "  for (const auto& elem : " << expr << ".value) {\n";
          out << ind << "    asn1::ByteWriter ew;\n";
          out << ind << "    {\n";
          out << ind << "      asn1::ByteWriter& w = ew;\n";
          emit_oer_field_encode(out, elem, "elem", ind + "      ");
          out << ind << "    }\n";
          out << ind << "    parts.push_back(ew.take());\n";
          out << ind << "  }\n";
          out << ind << "  " << oer_ns() << "::encode_set_of(w, std::move(parts));\n";
          out << ind << "}\n";
        } else {
          out << ind << oer_ns() << "::encode_sequence_of_length(w, " << expr
              << ".value.size());\n";
          out << ind << "for (const auto& elem : " << expr << ".value) {\n";
          emit_oer_field_encode(out, elem, "elem", ind + "  ");
          out << ind << "}\n";
        }
        break;
      }
      case ir::TypeKind::Sequence:
        emit_oer_sequence_encode(out, rt.sequence, expr, ind);
        break;
      case ir::TypeKind::Set:
        emit_oer_sequence_encode(out, rt.set, expr, ind);
        break;
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        const std::size_t root_n = rt.choice.alternatives.size();
        out << ind << "{\n";
        out << ind << "  const std::size_t index = " << expr << ".alt.index();\n";
        out << ind << "  switch (index) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          const bool is_ext = i >= root_n;
          out << ind << "    case " << i << ": {\n";
          out << ind << "      const auto& alt = std::get<" << i << ">(" << expr << ".alt);\n";
          out << ind << "      " << oer_ns() << "::encode_choice_tag(w, " << f.tag.number << "u, "
              << (type_is_constructed(ft) ? "true" : "false") << ");\n";
          if (is_ext) {
            out << ind << "      asn1::ByteWriter ow;\n";
            out << ind << "      {\n";
            out << ind << "        asn1::ByteWriter& w = ow;\n";
            emit_oer_field_encode(out, ft, "alt.value", ind + "        ");
            out << ind << "      }\n";
            out << ind << "      " << oer_ns() << "::encode_open_type(w, ow.take());\n";
          } else {
            emit_oer_field_encode(out, ft, "alt.value", ind + "      ");
          }
          out << ind << "      break;\n";
          out << ind << "    }\n";
        }
        out << ind << "    default:\n";
        out << ind << "      return asn1::make_error(asn1::Error::Code::InvalidArgument, "
               "w.size(), \"CHOICE index out of range\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "" << oer_ns() << "::encode_object_identifier(w, asn1::Span<const std::uint64_t>("
            << expr << ".type_id.data(), " << expr << ".type_id.size()), false);\n";
        out << ind << "" << oer_ns() << "::encode_octet_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".value.data(), " << expr << ".value.size()));\n";
        break;
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_oer_encode_body(out, resolve(model_, rt.referenced.resolved), expr, ind);
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, w.size(), "
               "\"OER encode not generated for this type\");\n";
        break;
    }
  }

  void emit_oer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_boolean(r);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_null(r);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Integer:
        out << ind << "{\n";
        if (integer_is_bigint(rt.integer)) {
          out << ind << "  auto tmp = " << oer_ns() << "::decode_big_integer(r, "
              << oer_integer_constraint_expr(rt.integer) << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          out << ind << "  auto tmp = " << oer_ns() << "::decode_integer(r, "
              << oer_integer_constraint_expr(rt.integer) << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = static_cast<" << integer_host_type(rt.integer)
              << ">(tmp.value());\n";
        }
        out << ind << "}\n";
        break;
      case ir::TypeKind::OctetString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_octet_string(r, "
            << oer_size_constraint_expr(rt.octet_string.size) << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_bit_string(r, "
            << oer_size_constraint_expr(rt.bit_string.size) << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << ".bits = std::move(tmp.value().bits);\n"
            << ind << "  " << expr << ".bit_length = tmp.value().bit_length;\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_utf8_string(r, "
            << oer_size_constraint_expr(rt.string.size) << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Enumerated:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_enumerated(r);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = static_cast<std::remove_cv_t<std::remove_reference_t<decltype(" << expr << ")>>>(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_object_identifier(r, false);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_object_identifier(r, true);\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "{\n"
            << ind << "  auto tmp = " << oer_ns() << "::decode_real(r, "
            << oer_real_form_expr(rt.real.ieee_form) << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        out << ind << "{\n"
            << ind << "  auto n = " << oer_ns() << "::decode_sequence_of_length(r);\n"
            << ind << "  if (!n) return n.error();\n"
            << ind << "  if (n.value() > 65536) return asn1::make_error(asn1::Error::Code::LengthOverflow, r.offset(), \"OER sequence of count exceeds safe limit\");\n"
            << ind << "  " << expr << ".value.clear();\n"
            << ind << "  " << expr << ".value.reserve(std::min<std::size_t>(n.value(), 1024));\n";
        if (oer_use_coer() && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n"
              << ind << "  parts.reserve(std::min<std::size_t>(n.value(), 1024));\n"
              << ind << "  for (std::size_t i = 0; i < n.value(); ++i) {\n"
              << ind << "    const auto rem = r.remaining_span();\n"
              << ind << "    asn1::ByteReader er(rem);\n"
              << ind << "    " << type_cpp(elem_id) << " elem{};\n"
              << ind << "    {\n"
              << ind << "      asn1::ByteReader& r = er;\n";
          emit_oer_field_decode(out, elem, "elem", ind + "      ");
          out << ind << "    }\n"
              << ind << "    const std::size_t used = rem.size() - er.remaining();\n"
              << ind << "    parts.emplace_back(rem.data(), rem.data() + used);\n"
              << ind << "    if (auto skip = r.read(used); !skip) return skip.error();\n"
              << ind << "    " << expr << ".value.push_back(std::move(elem));\n"
              << ind << "  }\n"
              << ind << "  if (auto ord = " << oer_ns()
              << "::require_set_of_order(parts); !ord) return ord.error();\n";
        } else {
          out << ind << "  for (std::size_t i = 0; i < n.value(); ++i) {\n"
              << ind << "    " << type_cpp(elem_id) << " elem{};\n";
          emit_oer_field_decode(out, elem, "elem", ind + "    ");
          out << ind << "    " << expr << ".value.push_back(std::move(elem));\n"
              << ind << "  }\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Sequence:
        emit_oer_sequence_decode(out, rt.sequence, expr, ind);
        break;
      case ir::TypeKind::Set:
        emit_oer_sequence_decode(out, rt.set, expr, ind);
        break;
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        const std::size_t root_n = rt.choice.alternatives.size();
        const std::string choice_ty = is_named(rt) ? named_type_cpp(rt) : std::string{};
        out << ind << "{\n";
        out << ind << "  auto tag = " << oer_ns() << "::decode_choice_tag(r);\n";
        out << ind << "  if (!tag) return tag.error();\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          const bool is_ext = i >= root_n;
          out << ind << "  if (tag.value() == " << f.tag.number << "u) {\n";
          if (!choice_ty.empty()) {
            out << ind << "    " << choice_ty << "::" << cpp_ident(f.name) << "_ alt{};\n";
          } else {
            out << ind << "    " << cpp_ident(f.name) << "_ alt{};\n";
          }
          if (is_ext) {
            out << ind << "    auto ot = " << oer_ns() << "::decode_open_type(r);\n";
            out << ind << "    if (!ot) return ot.error();\n";
            out << ind << "    asn1::ByteReader er(ot.value());\n";
            out << ind << "    {\n";
            out << ind << "      asn1::ByteReader& r = er;\n";
            emit_oer_field_decode(out, ft, "alt.value", ind + "      ");
            out << ind << "    }\n";
          } else {
            emit_oer_field_decode(out, ft, "alt.value", ind + "    ");
          }
          out << ind << "    " << expr << ".alt = std::move(alt);\n";
          out << ind << "  } else ";
        }
        out << "{\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, r.offset(), "
               "\"unknown CHOICE tag\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n"
            << ind << "  auto tid = " << oer_ns() << "::decode_object_identifier(r, false);\n"
            << ind << "  if (!tid) return tid.error();\n"
            << ind << "  " << expr << ".type_id = std::move(tid.value());\n"
            << ind << "  auto val = " << oer_ns() << "::decode_octet_string(r);\n"
            << ind << "  if (!val) return val.error();\n"
            << ind << "  " << expr << ".value = std::move(val.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_oer_decode_body(out, resolve(model_, rt.referenced.resolved), expr, ind);
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, r.offset(), "
               "\"OER decode not generated for this type\");\n";
        break;
    }
  }

  // ---- BER / DER ----

  const char* tlv_ns(bool der) const { return der ? "asn1::der" : "asn1::ber"; }
  const char* tlv_fn(bool der) const { return der ? "der" : "ber"; }

  static std::string ber_tag_expr(const ir::Tag& tag, bool constructed) {
    std::ostringstream os;
    const char* fn = "asn1::ber::universal";
    switch (tag.cls) {
      case ir::TagClass::Application:
        fn = "asn1::ber::application";
        break;
      case ir::TagClass::Context:
        fn = "asn1::ber::context";
        break;
      case ir::TagClass::Private:
        fn = "asn1::ber::private_tag";
        break;
      case ir::TagClass::Universal:
      default:
        fn = "asn1::ber::universal";
        break;
    }
    os << fn << "(" << tag.number << "u, " << (constructed ? "true" : "false") << ")";
    return os.str();
  }

  static std::uint64_t universal_number_for(const ir::Type& t) {
    switch (t.kind) {
      case ir::TypeKind::Boolean:
        return 1;
      case ir::TypeKind::Integer:
        return 2;
      case ir::TypeKind::BitString:
        return 3;
      case ir::TypeKind::OctetString:
        return 4;
      case ir::TypeKind::Null:
        return 5;
      case ir::TypeKind::ObjectIdentifier:
        return 6;
      case ir::TypeKind::InstanceOf:
        return 8;
      case ir::TypeKind::Real:
        return 9;
      case ir::TypeKind::Enumerated:
        return 10;
      case ir::TypeKind::RelativeOid:
        return 13;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::Choice:
        return 16;
      case ir::TypeKind::Set:
      case ir::TypeKind::SetOf:
        return 17;
      case ir::TypeKind::String:
        return 12;  // UTF8String
      default:
        return 16;
    }
  }

  static std::string natural_tag_expr(const ir::Type& t) {
    ir::Tag tag;
    tag.cls = ir::TagClass::Universal;
    tag.number = universal_number_for(t);
    tag.is_explicit = false;
    return ber_tag_expr(tag, type_is_constructed(t));
  }

  void emit_tlv_codec_fns(std::ostream& out, ir::TypeId id, bool der) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    const char* suf = tlv_fn(der);
    out << "inline asn1::Result<void> encode_" << suf << "(asn1::ByteWriter& w, const " << name
        << "& value) {\n";
    emit_tlv_encode_body(out, t, "value", der, /*override_tag=*/nullptr, "  ");
    out << "  return asn1::Result<void>::success();\n}\n\n";

    out << "inline asn1::Result<void> decode_" << suf << "(asn1::ByteReader& r, " << name
        << "& value) {\n";
    emit_tlv_decode_body(out, t, "value", der, /*override_tag=*/nullptr, "  ");
    out << "  return asn1::Result<void>::success();\n}\n\n";
  }

  void emit_tlv_field_encode(std::ostream& out, const ir::Type& ft, const ir::Field* field,
                             const std::string& expr, bool der, const std::string& ind) {
    if (field && (field->tag.cls != ir::TagClass::Universal || field->tag.number != 0 ||
                  field->tag.is_explicit)) {
      if (field->tag.is_explicit) {
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter inner;\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteWriter& w = inner;\n";
        if (is_named(ft)) {
          out << ind << "    if (auto r = encode_" << tlv_fn(der) << "(w, " << expr
              << "); !r) return r;\n";
        } else {
          emit_tlv_encode_body(out, ft, expr, der, nullptr, ind + "    ");
        }
        out << ind << "  }\n";
        out << ind << "  asn1::ber::encode_constructed(w, "
            << ber_tag_expr(field->tag, true) << ", asn1::Span<const std::uint8_t>(inner.buffer().data(), "
            << "inner.buffer().size()));\n";
        out << ind << "}\n";
        return;
      }
      // IMPLICIT: replace natural tag
      if (is_named(ft) && type_is_constructed(ft)) {
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter inner;\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteWriter& w = inner;\n";
        emit_tlv_encode_content(out, ft, expr, der, ind + "    ");
        out << ind << "  }\n";
        out << ind << "  asn1::ber::encode_constructed(w, "
            << ber_tag_expr(field->tag, true)
            << ", asn1::Span<const std::uint8_t>(inner.buffer().data(), inner.buffer().size()));\n";
        out << ind << "}\n";
        return;
      }
      emit_tlv_encode_body(out, ft, expr, der, &field->tag, ind);
      return;
    }
    if (is_named(ft)) {
      out << ind << "if (auto r = encode_" << tlv_fn(der) << "(w, " << expr
          << "); !r) return r;\n";
    } else {
      emit_tlv_encode_body(out, ft, expr, der, nullptr, ind);
    }
  }

  void emit_tlv_encode_content(std::ostream& out, const ir::Type& t, const std::string& expr,
                               bool der, const std::string& ind) {
    // Encode without outermost constructed wrapper (for IMPLICIT SEQUENCE/SET).
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    if (rt.kind == ir::TypeKind::Sequence) {
      emit_tlv_sequence_components(out, rt.sequence, expr, der, ind);
    } else if (rt.kind == ir::TypeKind::Set) {
      emit_tlv_sequence_components(out, rt.set, expr, der, ind);
    } else {
      emit_tlv_encode_body(out, rt, expr, der, nullptr, ind);
    }
  }

  void emit_tlv_sequence_field_encode(std::ostream& out, const ir::Field& f,
                                      const std::string& expr, bool der,
                                      const std::string& ind) {
    const std::string fname = cpp_ident(f.name);
    const ir::Type& ft = resolve(model_, f.type);
    if (f.presence == ir::Presence::Optional) {
      out << ind << "if (" << expr << "." << fname << ") {\n";
      emit_tlv_field_encode(out, ft, &f, expr + "." + fname + ".value()", der, ind + "  ");
      out << ind << "}\n";
    } else if (f.presence == ir::Presence::Default && f.default_value) {
      const std::string def_val = default_val_cpp(*f.default_value);
      if (!def_val.empty()) {
        out << ind << "if (" << expr << "." << fname << " != " << def_val << ") {\n";
        emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind + "  ");
        out << ind << "}\n";
      } else {
        emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind);
      }
    } else {
      emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind);
    }
  }

  void emit_tlv_sequence_components(std::ostream& out, const ir::SequenceDesc& seq,
                                    const std::string& expr, bool der,
                                    const std::string& ind) {
    for (const ir::Field& f : seq.root) {
      emit_tlv_sequence_field_encode(out, f, expr, der, ind);
    }
    for (const ir::Field& f : seq.trailing_root) {
      emit_tlv_sequence_field_encode(out, f, expr, der, ind);
    }
    for (const auto& group : seq.extension_groups) {
      for (const ir::Field& f : group) {
        emit_tlv_sequence_field_encode(out, f, expr, der, ind);
      }
    }
  }

  void emit_tlv_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            bool der, const ir::Tag* override_tag, const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const std::string NS = tlv_ns(der);
    const std::string tag =
        override_tag ? ber_tag_expr(*override_tag, type_is_constructed(rt))
                     : (rt.tag.cls != ir::TagClass::Universal || rt.tag.number != 0
                            ? ber_tag_expr(rt.tag, type_is_constructed(rt))
                            : natural_tag_expr(rt));

    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << NS << "::encode_boolean(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Null:
        out << ind << NS << "::encode_null(w, " << tag << ");\n";
        break;
      case ir::TypeKind::Integer:
        if (integer_is_bigint(rt.integer)) {
          out << ind << NS << "::encode_integer(w, " << expr << ", " << tag << ");\n";
        } else {
          out << ind << NS << "::encode_integer(w, static_cast<std::int64_t>(" << expr
              << "), " << tag << ");\n";
        }
        break;
      case ir::TypeKind::OctetString:
        out << ind << NS << "::encode_octet_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n";
        out << ind << "  const std::uint8_t unused = static_cast<std::uint8_t>(("
            << expr << ".bit_length % 8u == 0u) ? 0u : (8u - (" << expr
            << ".bit_length % 8u)));\n";
        out << ind << "  " << NS << "::encode_bit_string(w, asn1::Span<const std::uint8_t>("
            << expr << ".bits.data(), " << expr << ".bits.size()), unused, " << tag << ");\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << NS << "::encode_utf8_string(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Enumerated:
        out << ind << NS << "::encode_enumerated(w, static_cast<std::int64_t>(" << expr
            << "), " << tag << ");\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
        out << ind << NS << "::encode_object_identifier(w, asn1::Span<const std::uint64_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << NS << "::encode_relative_oid(w, asn1::Span<const std::uint64_t>("
            << expr << ".data(), " << expr << ".size()), " << tag << ");\n";
        break;
      case ir::TypeKind::Real:
        out << ind << NS << "::encode_real(w, " << expr << ", " << tag << ");\n";
        break;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::Set) {
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n";
          auto emit_set_parts = [&](const std::vector<ir::Field>& fields) {
            for (const ir::Field& f : fields) {
              const std::string fname = cpp_ident(f.name);
              const ir::Type& ft = resolve(model_, f.type);
              if (f.presence == ir::Presence::Optional) {
                out << ind << "  if (" << expr << "." << fname << ") {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname + ".value()", der,
                                      ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              } else if (f.presence == ir::Presence::Default) {
                if (f.default_value) {
                  std::string def_val = default_val_cpp(*f.default_value);
                  if (!def_val.empty()) {
                    out << ind << "  if (" << expr << "." << fname << " != " << def_val << ") {\n";
                    out << ind << "    asn1::ByteWriter ew;\n";
                    out << ind << "    {\n";
                    out << ind << "      asn1::ByteWriter& w = ew;\n";
                    emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind + "      ");
                    out << ind << "    }\n";
                    out << ind << "    parts.push_back(ew.take());\n";
                    out << ind << "  }\n";
                    continue;
                  }
                }
                out << ind << "  {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              } else {
                out << ind << "  {\n";
                out << ind << "    asn1::ByteWriter ew;\n";
                out << ind << "    {\n";
                out << ind << "      asn1::ByteWriter& w = ew;\n";
                emit_tlv_field_encode(out, ft, &f, expr + "." + fname, der, ind + "      ");
                out << ind << "    }\n";
                out << ind << "    parts.push_back(ew.take());\n";
                out << ind << "  }\n";
              }
            }
          };
          emit_set_parts(rt.set.root);
          emit_set_parts(rt.set.trailing_root);
          out << ind << "  asn1::der::encode_set(w, std::move(parts), " << tag << ");\n";
        } else {
          out << ind << "  asn1::ByteWriter comps;\n";
          out << ind << "  {\n";
          out << ind << "    asn1::ByteWriter& w = comps;\n";
          if (rt.kind == ir::TypeKind::Sequence) {
            emit_tlv_sequence_components(out, rt.sequence, expr, der, ind + "    ");
          } else {
            emit_tlv_sequence_components(out, rt.set, expr, der, ind + "    ");
          }
          out << ind << "  }\n";
          if (der) {
            out << ind << "  asn1::der::encode_sequence(w, asn1::Span<const std::uint8_t>("
                << "comps.buffer().data(), comps.buffer().size()), " << tag << ");\n";
          } else {
            out << ind << "  asn1::ber::encode_constructed(w, " << tag
                << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
          }
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n";
          out << ind << "  parts.reserve(" << expr << ".value.size());\n";
          out << ind << "  for (const auto& elem : " << expr << ".value) {\n";
          out << ind << "    asn1::ByteWriter ew;\n";
          out << ind << "    {\n";
          out << ind << "      asn1::ByteWriter& w = ew;\n";
          if (is_named(elem)) {
            out << ind << "      if (auto r = encode_" << tlv_fn(der)
                << "(w, elem); !r) return r;\n";
          } else {
            emit_tlv_encode_body(out, elem, "elem", der, nullptr, ind + "      ");
          }
          out << ind << "    }\n";
          out << ind << "    parts.push_back(ew.take());\n";
          out << ind << "  }\n";
          out << ind << "  asn1::der::encode_set(w, std::move(parts), " << tag << ");\n";
        } else {
          out << ind << "  asn1::ByteWriter comps;\n";
          out << ind << "  for (const auto& elem : " << expr << ".value) {\n";
          out << ind << "    asn1::ByteWriter& w = comps;\n";
          if (is_named(elem)) {
            out << ind << "    if (auto r = encode_" << tlv_fn(der)
                << "(w, elem); !r) return r;\n";
          } else {
            emit_tlv_encode_body(out, elem, "elem", der, nullptr, ind + "    ");
          }
          out << ind << "  }\n";
          if (der) {
            out << ind << "  asn1::der::encode_sequence(w, asn1::Span<const std::uint8_t>("
                << "comps.buffer().data(), comps.buffer().size()), " << tag << ");\n";
          } else {
            out << ind << "  asn1::ber::encode_constructed(w, " << tag
                << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
          }
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        out << ind << "{\n";
        out << ind << "  const std::size_t index = " << expr << ".alt.index();\n";
        out << ind << "  switch (index) {\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          out << ind << "    case " << i << ": {\n";
          out << ind << "      const auto& alt = std::get<" << i << ">(" << expr << ".alt);\n";
          emit_tlv_field_encode(out, ft, &f, "alt.value", der, ind + "      ");
          out << ind << "      break;\n";
          out << ind << "    }\n";
        }
        out << ind << "    default:\n";
        out << ind << "      return asn1::make_error(asn1::Error::Code::InvalidArgument, "
               "w.size(), \"CHOICE index out of range\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  asn1::ByteWriter comps;\n";
        out << ind << "  " << NS << "::encode_object_identifier(comps, asn1::Span<const std::uint64_t>("
            << expr << ".type_id.data(), " << expr << ".type_id.size()));\n";
        out << ind << "  " << NS << "::encode_octet_string(comps, asn1::Span<const std::uint8_t>("
            << expr << ".value.data(), " << expr << ".value.size()));\n";
        out << ind << "  asn1::ber::encode_constructed(w, " << tag
            << ", asn1::Span<const std::uint8_t>(comps.buffer().data(), comps.buffer().size()));\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_tlv_encode_body(out, resolve(model_, rt.referenced.resolved), expr, der,
                               override_tag, ind);
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, w.size(), "
               "\"TLV encode not generated for this type\");\n";
        break;
    }
  }

  void emit_tlv_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
                            bool der, const ir::Tag* override_tag, const std::string& ind) {
    const ir::Type& rt = t.kind == ir::TypeKind::Referenced && t.referenced.resolved != ir::kInvalidType
                             ? resolve(model_, t.referenced.resolved)
                             : t;
    const std::string NS = tlv_ns(der);
    const std::string tag =
        override_tag ? ber_tag_expr(*override_tag, type_is_constructed(rt))
                     : (rt.tag.cls != ir::TagClass::Universal || rt.tag.number != 0
                            ? ber_tag_expr(rt.tag, type_is_constructed(rt))
                            : natural_tag_expr(rt));

    switch (rt.kind) {
      case ir::TypeKind::Boolean:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_boolean(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Null:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_null(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Integer:
        out << ind << "{\n";
        if (integer_is_bigint(rt.integer)) {
          out << ind << "  auto tmp = " << NS << "::decode_big_integer(r, " << tag << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          out << ind << "  auto tmp = " << NS << "::decode_integer(r, " << tag << ");\n"
              << ind << "  if (!tmp) return tmp.error();\n"
              << ind << "  " << expr << " = static_cast<" << integer_host_type(rt.integer)
              << ">(tmp.value());\n";
        }
        out << ind << "}\n";
        break;
      case ir::TypeKind::OctetString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_octet_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::BitString:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_bit_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::String:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_utf8_string(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Enumerated:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_enumerated(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = static_cast<std::remove_cv_t<std::remove_reference_t<decltype(" << expr << ")>>>(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::ObjectIdentifier:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_object_identifier(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::RelativeOid:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_relative_oid(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Real:
        out << ind << "{\n"
            << ind << "  auto tmp = " << NS << "::decode_real(r, " << tag << ");\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = tmp.value();\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::Sequence:
      case ir::TypeKind::Set: {
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::Set) {
          out << ind << "  auto content = asn1::der::decode_set(r, " << tag << ");\n";
        } else if (der) {
          out << ind << "  auto content = asn1::der::decode_sequence(r, " << tag << ");\n";
        } else {
          out << ind << "  auto content = asn1::ber::decode_constructed(r, " << tag << ");\n";
        }
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  {\n";
        out << ind << "    asn1::ByteReader& r = cr;\n";
        const auto& fields = rt.kind == ir::TypeKind::Sequence ? rt.sequence.root : rt.set.root;
        const auto& trailing =
            rt.kind == ir::TypeKind::Sequence ? rt.sequence.trailing_root : rt.set.trailing_root;
        for (const ir::Field& f : fields) {
          emit_tlv_field_decode_stmt(out, f, expr, der, ind + "    ");
        }
        for (const ir::Field& f : trailing) {
          emit_tlv_field_decode_stmt(out, f, expr, der, ind + "    ");
        }
        if (rt.kind == ir::TypeKind::Sequence) {
          for (const auto& group : rt.sequence.extension_groups) {
            for (const ir::Field& f : group) {
              emit_tlv_field_decode_stmt(out, f, expr, der, ind + "    ");
            }
          }
        }
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SequenceOf:
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id =
            rt.kind == ir::TypeKind::SequenceOf ? rt.sequence_of.element : rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        out << ind << "{\n";
        if (der && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "  auto content = asn1::der::decode_set(r, " << tag << ");\n";
        } else if (der) {
          out << ind << "  auto content = asn1::der::decode_sequence(r, " << tag << ");\n";
        } else {
          out << ind << "  auto content = asn1::ber::decode_constructed(r, " << tag << ");\n";
        }
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  " << expr << ".value.clear();\n";
        out << ind << "  while (cr.remaining() > 0) {\n";
        out << ind << "    " << type_cpp(elem_id) << " elem{};\n";
        out << ind << "    {\n";
        out << ind << "      asn1::ByteReader& r = cr;\n";
        if (is_named(elem)) {
          out << ind << "      if (auto dr = decode_" << tlv_fn(der)
              << "(r, elem); !dr) return dr;\n";
        } else {
          emit_tlv_decode_body(out, elem, "elem", der, nullptr, ind + "      ");
        }
        out << ind << "    }\n";
        out << ind << "    " << expr << ".value.push_back(std::move(elem));\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        const std::string choice_ty = is_named(rt) ? named_type_cpp(rt) : std::string{};
        out << ind << "{\n";
        out << ind << "  auto hdr = asn1::ber::decode_tlv_header(r);\n";
        out << ind << "  if (!hdr) return hdr.error();\n";
        out << ind << "  if (hdr.value().length.indefinite) {\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::Unsupported, r.offset(), "
               "\"indefinite CHOICE not generated\");\n";
        out << ind << "  }\n";
        out << ind << "  auto content = r.read(hdr.value().length.value);\n";
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  const auto hdr_tag = hdr.value().tag;\n";
        for (std::size_t i = 0; i < all.size(); ++i) {
          const ir::Field& f = *all[i];
          const ir::Type& ft = resolve(model_, f.type);
          const std::string expect =
              ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
          out << ind << "  if (hdr_tag == " << expect << ") {\n";
          if (!choice_ty.empty()) {
            out << ind << "    " << choice_ty << "::" << cpp_ident(f.name) << "_ alt{};\n";
          } else {
            out << ind << "    " << cpp_ident(f.name) << "_ alt{};\n";
          }
          if (f.tag.is_explicit) {
            out << ind << "    asn1::ByteReader cr(content.value());\n";
            out << ind << "    {\n";
            out << ind << "      asn1::ByteReader& r = cr;\n";
            if (is_named(ft)) {
              out << ind << "      if (auto dr = decode_" << tlv_fn(der)
                  << "(r, alt.value); !dr) return dr;\n";
            } else {
              emit_tlv_decode_body(out, ft, "alt.value", der, nullptr, ind + "      ");
            }
            out << ind << "    }\n";
          } else {
            out << ind << "    asn1::ByteWriter tmpw;\n";
            out << ind << "    asn1::ber::encode_tlv(tmpw, " << natural_tag_expr(ft)
                << ", content.value());\n";
            out << ind << "    asn1::ByteReader rr(tmpw.buffer());\n";
            out << ind << "    {\n";
            out << ind << "      asn1::ByteReader& r = rr;\n";
            if (is_named(ft)) {
              out << ind << "      if (auto dr = decode_" << tlv_fn(der)
                  << "(r, alt.value); !dr) return dr;\n";
            } else {
              emit_tlv_decode_body(out, ft, "alt.value", der, nullptr, ind + "      ");
            }
            out << ind << "    }\n";
          }
          out << ind << "    " << expr << ".alt = std::move(alt);\n";
          out << ind << "  } else ";
        }
        out << "{\n";
        out << ind << "    return asn1::make_error(asn1::Error::Code::InvalidArgument, r.offset(), "
               "\"unknown CHOICE tag\");\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf:
        out << ind << "{\n";
        out << ind << "  auto content = asn1::ber::decode_constructed(r, " << tag << ");\n";
        out << ind << "  if (!content) return content.error();\n";
        out << ind << "  asn1::ByteReader cr(content.value());\n";
        out << ind << "  auto tid = " << NS << "::decode_object_identifier(cr);\n";
        out << ind << "  if (!tid) return tid.error();\n";
        out << ind << "  " << expr << ".type_id = std::move(tid.value());\n";
        out << ind << "  auto val = " << NS << "::decode_octet_string(cr);\n";
        out << ind << "  if (!val) return val.error();\n";
        out << ind << "  " << expr << ".value = std::move(val.value());\n";
        out << ind << "}\n";
        break;
      case ir::TypeKind::Referenced:
        if (rt.referenced.resolved != ir::kInvalidType) {
          emit_tlv_decode_body(out, resolve(model_, rt.referenced.resolved), expr, der,
                               override_tag, ind);
        }
        break;
      default:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, r.offset(), "
               "\"TLV decode not generated for this type\");\n";
        break;
    }
  }

  void emit_tlv_field_decode_stmt(std::ostream& out, const ir::Field& f, const std::string& expr,
                                  bool der, const std::string& ind) {
    const std::string fname = cpp_ident(f.name);
    const ir::Type& ft = resolve(model_, f.type);
    if (f.presence == ir::Presence::Default) {
      const std::string expect =
          ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
      out << ind << "if (r.remaining() > 0) {\n";
      out << ind << "  asn1::ByteReader peek_r(r.remaining_span());\n";
      out << ind << "  auto hdr = asn1::ber::decode_tlv_header(peek_r);\n";
      out << ind << "  if (hdr && hdr.value().tag == " << expect << ") {\n";
      out << ind << "    emit_field_present: ;\n";
      emit_tlv_field_decode_value(out, ft, &f, expr + "." + fname, der, ind + "    ");
      out << ind << "  } else {\n";
      if (f.default_value) {
        const std::string def_expr = default_val_cpp(*f.default_value);
        if (!def_expr.empty()) {
          out << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
        }
      }
      out << ind << "  }\n";
      out << ind << "} else {\n";
      if (f.default_value) {
        const std::string def_expr = default_val_cpp(*f.default_value);
        if (!def_expr.empty()) {
          out << ind << "  " << expr << "." << fname << " = " << def_expr << ";\n";
        }
      }
      out << ind << "}\n";
      return;
    }
    if (f.presence == ir::Presence::Optional) {
      const std::string expect =
          ber_tag_expr(f.tag, type_is_constructed(ft) || f.tag.is_explicit);
      out << ind << "if (r.remaining() == 0) {\n";
      out << ind << "  " << expr << "." << fname << " = std::nullopt;\n";
      out << ind << "} else {\n";
      out << ind << "  asn1::ByteReader peek_r(r.remaining_span());\n";
      out << ind << "  auto hdr = asn1::ber::decode_tlv_header(peek_r);\n";
      out << ind << "  if (!hdr) return hdr.error();\n";
      out << ind << "  if (hdr.value().tag == " << expect << ") {\n";
      out << ind << "    " << type_cpp(f.type) << " field{};\n";
      emit_tlv_field_decode_value(out, ft, &f, "field", der, ind + "    ");
      out << ind << "    " << expr << "." << fname << " = std::move(field);\n";
      out << ind << "  } else {\n";
      out << ind << "    " << expr << "." << fname << " = std::nullopt;\n";
      out << ind << "  }\n";
      out << ind << "}\n";
    } else {
      emit_tlv_field_decode_value(out, ft, &f, expr + "." + fname, der, ind);
    }
  }

  void emit_tlv_field_decode_value(std::ostream& out, const ir::Type& ft, const ir::Field* field,
                                   const std::string& expr, bool der, const std::string& ind) {
    if (field && field->tag.is_explicit) {
      out << ind << "{\n";
      out << ind << "  auto content = asn1::ber::decode_constructed(r, "
          << ber_tag_expr(field->tag, true) << ");\n";
      out << ind << "  if (!content) return content.error();\n";
      out << ind << "  asn1::ByteReader cr(content.value());\n";
      out << ind << "  {\n";
      out << ind << "    asn1::ByteReader& r = cr;\n";
      if (is_named(ft)) {
        out << ind << "    if (auto dr = decode_" << tlv_fn(der) << "(r, " << expr
            << "); !dr) return dr;\n";
      } else {
        emit_tlv_decode_body(out, ft, expr, der, nullptr, ind + "    ");
      }
      out << ind << "  }\n";
      out << ind << "}\n";
      return;
    }
    if (field && field->tag.cls != ir::TagClass::Universal) {
      emit_tlv_decode_body(out, ft, expr, der, &field->tag, ind);
      return;
    }
    if (is_named(ft)) {
      out << ind << "if (auto dr = decode_" << tlv_fn(der) << "(r, " << expr
          << "); !dr) return dr;\n";
    } else {
      emit_tlv_decode_body(out, ft, expr, der, nullptr, ind);
    }
  }

};

}  // namespace

void CppGenerator::emit(const ir::Model& model, const EmitOptions& options, Diagnostics& diag,
                        std::ostream& header, std::ostream* source) {
  Emitter emitter(model, options, diag);
  emitter.run(header, source);
}

std::string CppGenerator::emit_header_string(const ir::Model& model, const EmitOptions& options,
                                             Diagnostics& diag) {
  std::ostringstream os;
  emit(model, options, diag, os, nullptr);
  return os.str();
}

}  // namespace codegen
}  // namespace asn1
