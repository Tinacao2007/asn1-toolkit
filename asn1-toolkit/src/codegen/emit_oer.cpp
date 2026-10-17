/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_oer.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   OER family emitter (OER/COER share one implementation via the
   coer flag): octet encode/decode body emission.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "emit_internal.hpp"

namespace asn1 {
namespace codegen {

  /**
   *  Function    : oer_use_coer
   *  Description : Returns a boolean result from none.
   *  Parameters  : none
   *  Returns     : bool
   */
bool Emitter::oer_use_coer() const {
    return (options_.codec & CodecKind::Coer) != CodecKind::None;
  }

  /**
   *  Function    : oer_ns
   *  Description : Selects the OER runtime namespace (coer variant vs oer).
   *  Parameters  : none
   *  Returns     : const char*
   */
const char* Emitter::oer_ns() const { return oer_use_coer() ? "asn1::coer" : "asn1::oer"; }


  /**
   *  Function    : oer_fn_suffix
   *  Description : Selects the OER codec function suffix (coer vs oer).
   *  Parameters  : none
   *  Returns     : const char*
   */
const char* Emitter::oer_fn_suffix() const { return oer_use_coer() ? "coer" : "oer"; }


  /**
   *  Function    : oer_integer_constraint_expr
   *  Description : Builds and returns a string for oer integer constraint expr.
   *  Parameters  : d — const ir::IntegerDesc& d
   *  Returns     : std::string
   */
std::string Emitter::oer_integer_constraint_expr(const ir::IntegerDesc& d) {
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

  /**
   *  Function    : oer_size_constraint_expr
   *  Description : Builds and returns a string for oer size constraint expr.
   *  Parameters  : d — const ir::ConstraintDesc& d
   *  Returns     : std::string
   */
std::string Emitter::oer_size_constraint_expr(const ir::ConstraintDesc& d) {
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

  /**
   *  Function    : oer_real_form_expr
   *  Description : Builds and returns a string for oer real form expr.
   *  Parameters  : f — ir::RealIeeeForm f
   *  Returns     : std::string
   */
std::string Emitter::oer_real_form_expr(ir::RealIeeeForm f) {
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

  /**
   *  Function    : emit_oer_codec_fns
   *  Description : Performs emit oer codec fns (definition).
   *  Parameters  : out — std::ostream& out; id — ir::TypeId id
   *  Returns     : void
   */
void Emitter::emit_oer_codec_fns(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    const char* suf = oer_fn_suffix();
    std::ostream* body = begin_codec_fn(
        out, std::string("asn1::Result<void> encode_") + suf +
                "(asn1::ByteWriter& w, const " + name + "& value)");
    emit_oer_encode_body(*body, t, "value", "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";

    body = begin_codec_fn(out, std::string("asn1::Result<void> decode_") + suf +
                                   "(asn1::ByteReader& r, " + name + "& value)");
    emit_oer_decode_body(*body, t, "value", "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";
  }

void Emitter::emit_oer_field_encode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                             const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "if (auto r = encode_" << oer_fn_suffix() << "(w, " << expr << "); !r) return r;\n";
    } else {
      emit_oer_encode_body(out, ft, expr, ind);
    }
  }

void Emitter::emit_oer_field_decode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                             const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "if (auto dr = decode_" << oer_fn_suffix() << "(r, " << expr
          << "); !dr) return dr;\n";
    } else {
      emit_oer_decode_body(out, ft, expr, ind);
    }
  }

void Emitter::emit_oer_extension_group_encode(std::ostream& out, const ir::ExtensionGroup& group,
                                       const std::string& expr, const std::string& ind) {
    std::vector<const ir::Field*> optionals;
    if (group.version_bracket) {
      // X.690 16.4 mirrors X.691 19.9: only version-bracket groups are
      // encoded as a nested SEQUENCE (preamble included) inside the open
      // type; plain extension additions carry only the component value.
      for (const ir::Field& f : group.fields) {
        if (field_has_presence_bit(f)) {
          optionals.push_back(&f);
        }
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
    for (const ir::Field& f : group.fields) {
      const std::string fname = cpp_ident(f.name);
      const ir::Type& ft = resolve(model_, f.type);
      // Extension-group fields are force-optional in the struct; see the PER
      // encoder for the X.691 19.9 / X.690 16.4 rationale.
      out << ind << "if (" << expr << "." << fname << ") {\n";
      emit_oer_field_encode(out, ft, expr + "." + fname + ".value()", ind + "  ");
      out << ind << "}\n";
    }
  }

void Emitter::emit_oer_extension_group_decode(std::ostream& out, const ir::ExtensionGroup& group,
                                       const std::string& expr, const std::string& ind) {
    std::vector<const ir::Field*> optionals;
    if (group.version_bracket) {
      // X.690 16.4 mirrors X.691 19.9: only version-bracket groups carry a
      // nested-SEQUENCE preamble inside the open type.
      for (const ir::Field& f : group.fields) {
        if (field_has_presence_bit(f)) {
          optionals.push_back(&f);
        }
      }
    }
    if (!optionals.empty()) {
      out << ind << "{\n";
      out << ind << "  auto gbm = " << oer_ns() << "::decode_sequence_preamble(r, false, "
          << optionals.size() << ");\n";
      out << ind << "  if (!gbm) return gbm.error();\n";
      std::size_t opt_i = 0;
      for (const ir::Field& f : group.fields) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        if (field_has_presence_bit(f)) {
          out << ind << "  if (gbm.value().optionals.size() > " << opt_i
              << " && gbm.value().optionals[" << opt_i << "]) {\n";
          if (f.presence == ir::Presence::Optional) {
            const std::string tmp = "field" + std::to_string(temp_counter_++);
            out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
            emit_oer_field_decode(out, ft, tmp, ind + "    ");
            out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n";
          } else {
            emit_oer_field_decode(out, ft, expr + "." + fname, ind + "    ");
          }
          out << ind << "  }\n";
          ++opt_i;
        } else {
          out << ind << "  {\n";
          const std::string tmp = "field" + std::to_string(temp_counter_++);
          out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
          emit_oer_field_decode(out, ft, tmp, ind + "    ");
          out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n"
              << ind << "  }\n";
        }
      }
      out << ind << "}\n";
    } else {
      for (const ir::Field& f : group.fields) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        out << ind << "{\n";
        const std::string tmp = "field" + std::to_string(temp_counter_++);
        out << ind << "  " << type_cpp(f.type) << " " << tmp << "{};\n";
        emit_oer_field_decode(out, ft, tmp, ind + "  ");
        out << ind << "  " << expr << "." << fname << " = std::move(" << tmp << ");\n"
            << ind << "}\n";
      }
    }
  }

void Emitter::emit_oer_sequence_encode(std::ostream& out, const ir::SequenceDesc& seq,
                                const std::string& expr, const std::string& ind,
                                bool is_set) {
    // Root components (SET: in X.696 18.2 canonical order — the preamble
    // bits follow the same order per 18.1; extension additions are NOT
    // reordered, X.696 18.2 NOTE 1).
    std::vector<const ir::Field*> root_fields;
    for (const ir::Field& f : seq.root) {
      root_fields.push_back(&f);
    }
    for (const ir::Field& f : seq.trailing_root) {
      root_fields.push_back(&f);
    }
    if (is_set) {
      std::stable_sort(root_fields.begin(), root_fields.end(),
                       [this](const ir::Field* a, const ir::Field* b) {
                         return set_component_key(*a) < set_component_key(*b);
                       });
    }
    std::vector<const ir::Field*> optionals;
    for (const ir::Field* fp : root_fields) {
      if (field_has_presence_bit(*fp)) {
        optionals.push_back(fp);
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
          std::string def_val = default_val_cpp(pf);
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
        for (std::size_t i = 0; i < group.fields.size(); ++i) {
          if (i) out << " || ";
          emit_ext_group_field_present(out, group.fields[i], expr);
        }
        if (group.fields.empty()) out << "false";
        out << ");\n";
        out << ind << "  if (ep[" << g << "]) ext_present = true;\n";
      }
    }
    out << ind << "  " << oer_ns() << "::encode_sequence_preamble(w, "
        << (seq.extensible ? "true" : "false") << ", ext_present, "
        << "asn1::Span<const bool>(opt, " << optionals.size() << "));\n";
    out << ind << "}\n";
    auto emit_fields_encode = [&]() {
      for (const ir::Field* fp : root_fields) {
        const ir::Field& f = *fp;
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        if (f.presence == ir::Presence::Optional) {
          out << ind << "if (" << expr << "." << fname << ") {\n";
          emit_oer_field_encode(out, ft, expr + "." + fname + ".value()", ind + "  ");
          out << ind << "}\n";
        } else if (f.presence == ir::Presence::Default) {
          if (f.default_value) {
            std::string def_val = default_val_cpp(f);
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
    emit_fields_encode();
    if (seq.extensible && n_ext > 0) {
      out << ind << "{\n";
      out << ind << "  bool ep[" << n_ext << "] = {};\n";
      out << ind << "  bool ext_present = false;\n";
      for (std::size_t g = 0; g < n_ext; ++g) {
        out << ind << "  ep[" << g << "] = (";
        const auto& group = seq.extension_groups[g];
        for (std::size_t i = 0; i < group.fields.size(); ++i) {
          if (i) out << " || ";
          emit_ext_group_field_present(out, group.fields[i], expr);
        }
        if (group.fields.empty()) out << "false";
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

void Emitter::emit_oer_sequence_decode(std::ostream& out, const ir::SequenceDesc& seq,
                                const std::string& expr, const std::string& ind,
                                bool is_set) {
    // Mirror of the encode order: SET root components (and their preamble
    // bits) in X.696 18.2 canonical order; extension additions keep
    // definition order (18.2 NOTE 1).
    std::vector<const ir::Field*> root_fields;
    for (const ir::Field& f : seq.root) {
      root_fields.push_back(&f);
    }
    for (const ir::Field& f : seq.trailing_root) {
      root_fields.push_back(&f);
    }
    if (is_set) {
      std::stable_sort(root_fields.begin(), root_fields.end(),
                       [this](const ir::Field* a, const ir::Field* b) {
                         return set_component_key(*a) < set_component_key(*b);
                       });
    }
    std::size_t n_optionals = 0;
    for (const ir::Field* fp : root_fields) {
      if (field_has_presence_bit(*fp)) {
        ++n_optionals;
      }
    }
    const std::size_t n_ext = seq.extension_groups.size();
    out << ind << "{\n";
    out << ind << "  auto pre = " << oer_ns() << "::decode_sequence_preamble(r, "
        << (seq.extensible ? "true" : "false") << ", " << n_optionals << ");\n";
    out << ind << "  if (!pre) return pre.error();\n";
    std::size_t opt_i = 0;
    auto emit_fields = [&]() {
      for (const ir::Field* fp : root_fields) {
        const ir::Field& f = *fp;
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        if (f.presence == ir::Presence::Optional) {
          out << ind << "  if (pre.value().optionals.size() > " << opt_i
              << " && pre.value().optionals[" << opt_i << "]) {\n";
          const std::string tmp = "field" + std::to_string(temp_counter_++);
          out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
          emit_oer_field_decode(out, ft, tmp, ind + "    ");
          out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n"
              << ind << "  } else {\n"
              << ind << "    " << expr << "." << fname << " = std::nullopt;\n"
              << ind << "  }\n";
          ++opt_i;
        } else if (f.presence == ir::Presence::Default) {
          out << ind << "  if (pre.value().optionals.size() > " << opt_i
              << " && pre.value().optionals[" << opt_i << "]) {\n";
          emit_oer_field_decode(out, ft, expr + "." + fname, ind + "    ");
          if (f.default_value) {
            const std::string def_expr = default_val_cpp(f);
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
    emit_fields();
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

void Emitter::emit_oer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
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
        // A named SEQUENCE OF lowers to an alias struct with a `.value`
        // member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
        if (oer_use_coer() && rt.kind == ir::TypeKind::SetOf) {
          out << ind << "{\n";
          out << ind << "  std::vector<std::vector<std::uint8_t>> parts;\n";
          out << ind << "  parts.reserve(" << vec << ".size());\n";
          out << ind << "  for (const auto& elem : " << vec << ") {\n";
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
          out << ind << oer_ns() << "::encode_sequence_of_length(w, " << vec
              << ".size());\n";
          out << ind << "for (const auto& elem : " << vec << ") {\n";
          emit_oer_field_encode(out, elem, "elem", ind + "  ");
          out << ind << "}\n";
        }
        break;
      }
      case ir::TypeKind::Sequence:
        emit_oer_sequence_encode(out, rt.sequence, expr, ind, false);
        break;
      case ir::TypeKind::Set:
        emit_oer_sequence_encode(out, rt.set, expr, ind, true);
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
          // Unique local: a nested CHOICE alternative re-enters this block
          // with expr = "<outer alt local>.value", and a duplicate `alt`
          // shadows the initializer it is deduced from.
          const std::string alt_local = "alt" + std::to_string(temp_counter_++);
          out << ind << "      const auto& " << alt_local << " = std::get<" << i << ">(" << expr
              << ".alt);\n";
          // X.696 8.7.2: the OER tag octet carries class (bits 8-7) and tag
          // number (bits 6-1) only — there is no constructed bit, so the
          // tag is the same for primitive and constructed alternatives.
          out << ind << "      " << oer_ns() << "::encode_choice_tag(w, " << f.tag.number << "u, "
              << "false);\n";
          if (is_ext) {
            out << ind << "      asn1::ByteWriter ow;\n";
            out << ind << "      {\n";
            out << ind << "        asn1::ByteWriter& w = ow;\n";
            emit_oer_field_encode(out, ft, alt_local + ".value", ind + "        ");
            out << ind << "      }\n";
            out << ind << "      " << oer_ns() << "::encode_open_type(w, ow.take());\n";
          } else {
            emit_oer_field_encode(out, ft, alt_local + ".value", ind + "      ");
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

void Emitter::emit_oer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
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
        // A named SEQUENCE OF lowers to an alias struct with a `.value`
        // member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
        out << ind << "{\n"
            << ind << "  auto n = " << oer_ns() << "::decode_sequence_of_length(r);\n"
            << ind << "  if (!n) return n.error();\n"
            << ind << "  if (n.value() > 65536) return asn1::make_error(asn1::Error::Code::LengthOverflow, r.offset(), \"OER sequence of count exceeds safe limit\");\n"
            << ind << "  " << vec << ".clear();\n"
            << ind << "  " << vec << ".reserve(std::min<std::size_t>(n.value(), 1024));\n";
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
              << ind << "    " << vec << ".push_back(std::move(elem));\n"
              << ind << "  }\n"
              << ind << "  if (auto ord = " << oer_ns()
              << "::require_set_of_order(parts); !ord) return ord.error();\n";
        } else {
          out << ind << "  for (std::size_t i = 0; i < n.value(); ++i) {\n"
              << ind << "    " << type_cpp(elem_id) << " elem{};\n";
          emit_oer_field_decode(out, elem, "elem", ind + "    ");
          out << ind << "    " << vec << ".push_back(std::move(elem));\n"
              << ind << "  }\n";
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Sequence:
        emit_oer_sequence_decode(out, rt.sequence, expr, ind, false);
        break;
      case ir::TypeKind::Set:
        emit_oer_sequence_decode(out, rt.set, expr, ind, true);
        break;
      case ir::TypeKind::Choice: {
        const auto all = choice_all_fields(rt.choice);
        const std::size_t root_n = rt.choice.alternatives.size();
        const std::string choice_ty = is_named(rt) ? named_ref(rt) : std::string{};
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

}  // namespace codegen
}  // namespace asn1
