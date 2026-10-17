/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_per.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   PER family emitter (UPER/APER share one implementation via the
   aper flag): encode/decode body emission.
**
** Specification: Generated code layout follows ITU type rules; emission
**                 logic is toolchain-internal.
** Design Spec:   asn1-toolkit/docs/architecture.md
**                 asn1-toolkit/README.md
***************************************************************************/
#include "emit_internal.hpp"

namespace asn1 {
namespace codegen {

namespace {

/**
 *  Function    : integer_constraint_expr
 *  Description : Builds and returns a string for integer constraint expr.
 *  Parameters  : d — const ir::IntegerDesc& d
 *  Returns     : std::string
 */
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

/**
 *  Function    : size_constraint_expr
 *  Description : Builds and returns a string for size constraint expr.
 *  Parameters  : d — const ir::ConstraintDesc& d
 *  Returns     : std::string
 */
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

}  // namespace

  /**
   *  Function    : ns_codec
   *  Description : Computes ns codec from (aper).
   *  Parameters  : aper — bool aper
   *  Returns     : const char*
   */
const char* Emitter::ns_codec(bool aper) const { return aper ? "asn1::aper" : "asn1::uper"; }

  /**
   *  Function    : fn_suffix
   *  Description : Computes fn suffix from (aper).
   *  Parameters  : aper — bool aper
   *  Returns     : const char*
   */
const char* Emitter::fn_suffix(bool aper) const { return aper ? "aper" : "uper"; }

  /**
   *  Function    : emit_codec_fns
   *  Description : Performs emit codec fns (definition).
   *  Parameters  : out — std::ostream& out; id — ir::TypeId id; aper — bool aper
   *  Returns     : void
   */
void Emitter::emit_codec_fns(std::ostream& out, ir::TypeId id, bool aper) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    const std::string enc = std::string("encode_") + fn_suffix(aper);
    const std::string dec = std::string("decode_") + fn_suffix(aper);

    std::ostream* body =
        begin_codec_fn(out, "asn1::Result<void> " + enc +
                                "(asn1::BitWriter& w, const " + name + "& value)");
    emit_encode_body(*body, t, "value", aper, "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";

    // Out-parameter form so multiple named types can coexist (return-type-only
    // overloads are ill-formed in C++).
    body = begin_codec_fn(out, "asn1::Result<void> " + dec +
                                   "(asn1::BitReader& r, " + name + "& value)");
    emit_decode_body(*body, t, "value", aper, "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";
  }

  /**
   *  Function    : enum_root_sorted_values
   *  Description : Computes enum root sorted values from (e).
   *  Parameters  : e — const ir::EnumeratedDesc& e
   *  Returns     : static std::vector<std::int64_t>
   */
std::vector<std::int64_t> Emitter::enum_root_sorted_values(const ir::EnumeratedDesc& e) {
    std::vector<std::int64_t> vals;
    vals.reserve(e.root.size());
    for (const auto& nn : e.root) {
      vals.push_back(nn.value.as_i64 ? *nn.value.as_i64 : 0);
    }
    std::sort(vals.begin(), vals.end());
    return vals;
  }

  /**
   *  Function    : enum_extension_values
   *  Description : Computes enum extension values from (e).
   *  Parameters  : e — const ir::EnumeratedDesc& e
   *  Returns     : static std::vector<std::int64_t>
   */
std::vector<std::int64_t> Emitter::enum_extension_values(const ir::EnumeratedDesc& e) {
    std::vector<std::int64_t> vals;
    vals.reserve(e.extensions.size());
    for (const auto& nn : e.extensions) {
      vals.push_back(nn.value.as_i64 ? *nn.value.as_i64 : 0);
    }
    return vals;
  }

void Emitter::emit_extension_group_encode(std::ostream& out, const ir::ExtensionGroup& group,
                                   const std::string& expr, bool aper,
                                   const std::string& ind) {
    const char* C = ns_codec(aper);
    std::vector<const ir::Field*> optionals;
    if (group.version_bracket) {
      // X.691 19.9: a version-bracket group is encoded as a nested SEQUENCE
      // inside the open type, so its OPTIONAL/DEFAULT components carry a
      // preamble there. A plain extension addition encodes only the component
      // value — its presence is already signalled by the extension bitmap.
      for (const ir::Field& f : group.fields) {
        if (field_has_presence_bit(f)) {
          optionals.push_back(&f);
        }
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
    for (const ir::Field& f : group.fields) {
      const std::string fname = cpp_ident(f.name);
      const ir::Type& ft = resolve(model_, f.type);
      // Extension-group fields are force-optional in the struct (see
      // emit_struct_fields(..., force_optional=true)), so every member is
      // encoded under a presence check and unwrapped with .value(). Per
      // X.691 19.9 NOTE 1 the group preamble carries presence bits only for
      // OPTIONAL/DEFAULT members; a mandatory member has no bit and is
      // encoded whenever the group itself is present (the caller sets it).
      out << ind << "if (" << expr << "." << fname << ") {\n";
      emit_field_encode(out, ft, expr + "." + fname + ".value()", aper, ind + "  ");
      out << ind << "}\n";
    }
  }

void Emitter::emit_extension_group_decode(std::ostream& out, const ir::ExtensionGroup& group,
                                   const std::string& expr, bool aper,
                                   const std::string& ind) {
    const char* C = ns_codec(aper);
    std::vector<const ir::Field*> optionals;
    if (group.version_bracket) {
      // X.691 19.9: only version-bracket groups carry a nested-SEQUENCE
      // preamble inside the open type; plain extension additions encode just
      // the component value (presence comes from the extension bitmap).
      for (const ir::Field& f : group.fields) {
        if (field_has_presence_bit(f)) {
          optionals.push_back(&f);
        }
      }
    }
    if (!optionals.empty()) {
      out << ind << "{\n";
      out << ind << "  auto gbm = " << C << "::decode_sequence_preamble(r, false, "
          << optionals.size() << ");\n";
      out << ind << "  if (!gbm) return gbm.error();\n";
      std::size_t opt_i = 0;
      for (const ir::Field& f : group.fields) {
        const std::string fname = cpp_ident(f.name);
        const ir::Type& ft = resolve(model_, f.type);
        if (field_has_presence_bit(f)) {
          out << ind << "  if (gbm.value().optionals.size() > " << opt_i
              << " && gbm.value().optionals[" << opt_i << "]) {\n";
          const std::string tmp = "field" + std::to_string(temp_counter_++);
          out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
          emit_field_decode(out, ft, tmp, aper, ind + "    ");
          if (f.presence == ir::Presence::Optional) {
            out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n";
          } else {
            out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n";
          }
          if (f.presence == ir::Presence::Default && f.default_value) {
            const std::string def_expr = default_val_cpp(f);
            if (!def_expr.empty()) {
              out << ind << "  } else {\n"
                  << ind << "    " << expr << "." << fname << " = " << def_expr << ";\n";
            }
          }
          out << ind << "  }\n";
          ++opt_i;
        } else {
          out << ind << "  {\n";
          const std::string tmp = "field" + std::to_string(temp_counter_++);
          out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
          emit_field_decode(out, ft, tmp, aper, ind + "    ");
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
        emit_field_decode(out, ft, tmp, aper, ind + "  ");
        out << ind << "  " << expr << "." << fname << " = std::move(" << tmp << ");\n"
            << ind << "}\n";
      }
    }
  }

void Emitter::emit_root_fields_encode(std::ostream& out, const std::vector<ir::Field>& fields,
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
          std::string def_val = default_val_cpp(f);
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

void Emitter::emit_sequence_like_encode(std::ostream& out, const ir::SequenceDesc& seq,
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
    auto emit_tc_encode = [&](const std::vector<ir::Field>& fields) {
      for (const auto& f : fields) {
        if (!f.table_constraint || f.table_constraint->entries.empty()) {
          continue;
        }
        const std::string fname = cpp_ident(f.name);
        const std::string gov = cpp_ident(f.table_constraint->governor_field_name);
        const auto alt_names = compute_table_entry_alt_names(f.table_constraint->entries);
        out << ind << "  if (" << expr << "." << fname << ".empty() && "
            << expr << ".decoded_" << fname << ".has_value()) {\n";
        // Parameter name must not collide with `alt` emitted by a nested
        // CHOICE encode body inside the visit lambda.
        out << ind << "    auto tc_res = std::visit([&](const auto& tc_alt) -> asn1::Result<void> {\n";
        out << ind << "      using AltT = std::decay_t<decltype(tc_alt)>;\n";
        for (std::size_t ei = 0; ei < f.table_constraint->entries.size(); ++ei) {
          const auto& entry = f.table_constraint->entries[ei];
          out << ind << "      if constexpr (std::is_same_v<AltT, typename std::decay_t<decltype("
              << expr << ")>::Decoded_" << fname << "::" << alt_names[ei] << ">) {\n";
          out << ind << "        asn1::BitWriter sub_w;\n";
          out << ind << "        asn1::BitWriter& w = sub_w;\n";
          emit_encode_body(out, resolve(model_, entry.type), "tc_alt.value", aper, ind + "        ");
          out << ind << "        const_cast<std::decay_t<decltype(" << expr << ")>&>(" << expr << ")." << fname
              << " = sub_w.take();\n";
          if (entry.has_id_value) {
            out << ind << "        const_cast<std::decay_t<decltype(" << expr << ")>&>(" << expr << ")." << gov
                << " = static_cast<decltype(" << expr << "." << gov << ")>("
                << entry.id_value.as_i64.value_or(0) << ");\n";
          }
          out << ind << "        return asn1::Result<void>::success();\n";
          out << ind << "      }\n";
        }
        out << ind << "      return asn1::Result<void>::success();\n";
        out << ind << "    }, " << expr << ".decoded_" << fname << "->choice);\n";
        out << ind << "    if (!tc_res) return tc_res.error();\n";
        out << ind << "  }\n";
      }
    };
    emit_tc_encode(seq.root);
    emit_tc_encode(seq.trailing_root);
    out << ind << "  std::uint8_t opt[" << (optionals.empty() ? 1 : optionals.size())
        << "] = {};\n";
    for (std::size_t i = 0; i < optionals.size(); ++i) {
      const ir::Field& pf = *optionals[i];
      out << ind << "  opt[" << i << "] = ";
      if (pf.presence == ir::Presence::Default) {
        if (pf.default_value) {
          std::string def_val = default_val_cpp(pf);
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
        for (std::size_t i = 0; i < group.fields.size(); ++i) {
          if (i) {
            out << " || ";
          }
          emit_ext_group_field_present(out, group.fields[i], expr);
        }
        if (group.fields.empty()) {
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
        for (std::size_t i = 0; i < group.fields.size(); ++i) {
          if (i) {
            out << " || ";
          }
          emit_ext_group_field_present(out, group.fields[i], expr);
        }
        if (group.fields.empty()) {
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

void Emitter::emit_root_fields_decode(std::ostream& out, const std::vector<ir::Field>& fields,
                               const std::string& expr, bool aper, const std::string& ind,
                               std::size_t& opt_i) {
    for (const ir::Field& f : fields) {
      const std::string fname = cpp_ident(f.name);
      const ir::Type& ft = resolve(model_, f.type);
      if (f.presence == ir::Presence::Optional) {
        out << ind << "  if (pre.value().optionals.size() > " << opt_i
            << " && pre.value().optionals[" << opt_i << "]) {\n";
        const std::string tmp = "field" + std::to_string(temp_counter_++);
        out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
        emit_field_decode(out, ft, tmp, aper, ind + "    ");
        out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n"
            << ind << "  } else {\n"
            << ind << "    " << expr << "." << fname << " = std::nullopt;\n"
            << ind << "  }\n";
        ++opt_i;
      } else if (f.presence == ir::Presence::Default) {
        out << ind << "  if (pre.value().optionals.size() > " << opt_i
            << " && pre.value().optionals[" << opt_i << "]) {\n";
        emit_field_decode(out, ft, expr + "." + fname, aper, ind + "    ");
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
        emit_field_decode(out, ft, expr + "." + fname, aper, ind + "  ");
      }
    }
  }

void Emitter::emit_sequence_like_decode(std::ostream& out, const ir::SequenceDesc& seq,
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
    auto emit_tc_decode = [&](const std::vector<ir::Field>& fields) {
      for (const auto& f : fields) {
        if (!f.table_constraint || f.table_constraint->entries.empty()) {
          continue;
        }
        const std::string fname = cpp_ident(f.name);
        const std::string gov = cpp_ident(f.table_constraint->governor_field_name);
        const auto alt_names = compute_table_entry_alt_names(f.table_constraint->entries);
        out << ind << "  if (!" << expr << "." << fname << ".empty()) {\n";
        out << ind << "    switch (" << expr << "." << gov << ") {\n";
        for (std::size_t ei = 0; ei < f.table_constraint->entries.size(); ++ei) {
          const auto& entry = f.table_constraint->entries[ei];
          if (!entry.has_id_value) continue;
          out << ind << "      case " << entry.id_value.as_i64.value_or(0) << ": {\n";
          out << ind << "        " << type_cpp(entry.type) << " payload{};\n";
          out << ind << "        asn1::BitReader sub_r(" << expr << "." << fname << ");\n";
          out << ind << "        asn1::BitReader& r = sub_r;\n";
          out << ind << "        auto decode_res = ([&]() -> asn1::Result<void> {\n";
          emit_decode_body(out, resolve(model_, entry.type), "payload", aper, ind + "          ");
          out << ind << "          return asn1::Result<void>::success();\n";
          out << ind << "        })();\n";
          out << ind << "        if (decode_res) {\n";
          out << ind << "          typename std::decay_t<decltype(" << expr << ")>::Decoded_" << fname << " dec{};\n";
          out << ind << "          dec.choice = typename std::decay_t<decltype(" << expr << ")>::Decoded_" << fname
              << "::" << alt_names[ei] << "{std::move(payload)};\n";
          out << ind << "          " << expr << ".decoded_" << fname << " = std::move(dec);\n";
          out << ind << "        }\n";
          out << ind << "        break;\n";
          out << ind << "      }\n";
        }
        out << ind << "      default: break;\n";
        out << ind << "    }\n";
        out << ind << "  }\n";
      }
    };
    emit_tc_decode(seq.root);
    emit_tc_decode(seq.trailing_root);
    out << ind << "}\n";
  }

void Emitter::emit_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
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
        out << ind << per_string_encode_call(C, t.string.kind, expr,
                                             size_constraint_expr(t.string.size)) << ";\n";
        break;
      case ir::TypeKind::SequenceOf: {
        const ir::Type& elem = resolve(model_, t.sequence_of.element);
        // A named SEQUENCE OF lowers to a distinct alias struct with a
        // `.value` member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(t) ? expr + ".value" : expr;
        out << ind << "{\n";
        out << ind << "  std::size_t remaining = " << vec << ".size();\n";
        out << ind << "  std::size_t offset = 0;\n";
        out << ind << "  bool is_first = true;\n";
        out << ind << "  for (;;) {\n";
        out << ind << "    std::size_t chunk = " << C << "::encode_sequence_of_chunk(w, remaining, is_first, "
            << size_constraint_expr(t.sequence_of.size) << ");\n";
        out << ind << "    for (std::size_t i = 0; i < chunk; ++i) {\n";
        out << ind << "      const auto& elem = " << vec << "[offset + i];\n";
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
        // A named SET OF lowers to a distinct alias struct with a
        // `.value` member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(t) ? expr + ".value" : expr;
        out << ind << "{\n";
        out << ind << "  std::size_t remaining = " << vec << ".size();\n";
        out << ind << "  std::size_t offset = 0;\n";
        out << ind << "  bool is_first = true;\n";
        out << ind << "  for (;;) {\n";
        out << ind << "    std::size_t chunk = " << C << "::encode_sequence_of_chunk(w, remaining, is_first, "
            << size_constraint_expr(t.set_of.size) << ");\n";
        out << ind << "    for (std::size_t i = 0; i < chunk; ++i) {\n";
        out << ind << "      const auto& elem = " << vec << "[offset + i];\n";
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
          // Unique local: a nested CHOICE alternative re-enters this block
          // with expr = "<outer alt local>.value", and a duplicate `alt`
          // shadows the initializer it is deduced from.
          const std::string alt_local = "alt" + std::to_string(temp_counter_++);
          out << ind << "        const auto& " << alt_local << " = std::get<" << i << ">(" << expr
              << ".alt);\n";
          emit_field_encode(out, ft, alt_local + ".value", aper, ind + "        ");
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
          // Unique local: see the root-alternative case above.
          const std::string alt_local = "alt" + std::to_string(temp_counter_++);
          out << ind << "          const auto& " << alt_local << " = std::get<" << vi << ">("
              << expr << ".alt);\n";
          emit_field_encode(out, ft, alt_local + ".value", aper, ind + "          ");
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

void Emitter::emit_field_encode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                         bool aper, const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "if (auto r = encode_" << fn_suffix(aper) << "(w, " << expr
          << "); !r) return r;\n";
    } else {
      emit_encode_body(out, ft, expr, aper, ind);
    }
  }

void Emitter::emit_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
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
            << ind << "  auto tmp = "
            << per_string_decode_call(C, t.string.kind, expr,
                                      size_constraint_expr(t.string.size))
            << ";\n"
            << ind << "  if (!tmp) return tmp.error();\n"
            << ind << "  " << expr << " = std::move(tmp.value());\n"
            << ind << "}\n";
        break;
      case ir::TypeKind::SequenceOf: {
        const ir::Type& elem_t = resolve(model_, t.sequence_of.element);
        // A named SEQUENCE OF lowers to a distinct alias struct with a
        // `.value` member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(t) ? expr + ".value" : expr;
        out << ind << "{\n"
            << ind << "  " << vec << ".clear();\n"
            << ind << "  bool is_first = true;\n"
            << ind << "  for (;;) {\n"
            << ind << "    auto n = " << C << "::decode_sequence_of_chunk(r, is_first, "
            << size_constraint_expr(t.sequence_of.size) << ");\n"
            << ind << "    if (!n) return n.error();\n"
            << ind << "    if (" << vec << ".size() + n.value() > 1000000) return asn1::make_error(asn1::Error::Code::LengthOverflow, r.bit_offset(), \"sequence of count exceeds safe limit\");\n"
            << ind << "    " << vec << ".reserve(" << vec << ".size() + std::min<std::size_t>(n.value(), 1024));\n"
            << ind << "    for (std::size_t i = 0; i < n.value(); ++i) {\n"
            << ind << "      " << type_cpp(t.sequence_of.element) << " elem{};\n";
        if (is_named(elem_t)) {
          out << ind << "      if (auto dr = decode_" << fn_suffix(aper)
              << "(r, elem); !dr) return dr;\n";
        } else {
          emit_decode_body(out, elem_t, "elem", aper, ind + "      ");
        }
        out << ind << "      " << vec << ".push_back(std::move(elem));\n"
            << ind << "    }\n"
            << ind << "    is_first = false;\n"
            << ind << "    if (n.value() < 16384) break;\n"
            << ind << "  }\n"
            << ind << "}\n";
        break;
      }

      case ir::TypeKind::SetOf: {
        const ir::Type& elem_t = resolve(model_, t.set_of.element);
        // A named SET OF lowers to a distinct alias struct with a
        // `.value` member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(t) ? expr + ".value" : expr;
        out << ind << "{\n"
            << ind << "  " << vec << ".clear();\n"
            << ind << "  bool is_first = true;\n"
            << ind << "  for (;;) {\n"
            << ind << "    auto n = " << C << "::decode_sequence_of_chunk(r, is_first, "
            << size_constraint_expr(t.set_of.size) << ");\n"
            << ind << "    if (!n) return n.error();\n"
            << ind << "    if (" << vec << ".size() + n.value() > 1000000) return asn1::make_error(asn1::Error::Code::LengthOverflow, r.bit_offset(), \"set of count exceeds safe limit\");\n"
            << ind << "    " << vec << ".reserve(" << vec << ".size() + std::min<std::size_t>(n.value(), 1024));\n"
            << ind << "    for (std::size_t i = 0; i < n.value(); ++i) {\n"
            << ind << "      " << type_cpp(t.set_of.element) << " elem{};\n";
        if (is_named(elem_t)) {
          out << ind << "      if (auto dr = decode_" << fn_suffix(aper)
              << "(r, elem); !dr) return dr;\n";
        } else {
          emit_decode_body(out, elem_t, "elem", aper, ind + "      ");
        }
        out << ind << "      " << vec << ".push_back(std::move(elem));\n"
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
        const std::string ty = is_named(t) ? named_ref(t) : "std::int64_t";
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
              << ind << "        " << named_ref(t) << "::" << cpp_ident(f.name)
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
              << ind << "          " << named_ref(t) << "::" << cpp_ident(f.name)
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

void Emitter::emit_field_decode(std::ostream& out, const ir::Type& ft, const std::string& expr,
                         bool aper, const std::string& ind) {
    if (is_named(ft)) {
      out << ind << "if (auto dr = decode_" << fn_suffix(aper) << "(r, " << expr
          << "); !dr) return dr;\n";
    } else {
      emit_decode_body(out, ft, expr, aper, ind);
    }
  }

const char* Emitter::per_char_string_kind_expr(ir::StringKind kind) {
    switch (kind) {
      case ir::StringKind::NumericString:
        return "asn1::per::CharStringKind::NumericString";
      case ir::StringKind::PrintableString:
        return "asn1::per::CharStringKind::PrintableString";
      case ir::StringKind::VisibleString:
        return "asn1::per::CharStringKind::VisibleString";
      case ir::StringKind::IA5String:
        return "asn1::per::CharStringKind::IA5String";
      case ir::StringKind::BMPString:
        return "asn1::per::CharStringKind::BMPString";
      case ir::StringKind::UniversalString:
        return "asn1::per::CharStringKind::UniversalString";
      case ir::StringKind::UTF8String:
      case ir::StringKind::TeletexString:
      case ir::StringKind::VideotexString:
      case ir::StringKind::GraphicString:
      case ir::StringKind::GeneralString:
      case ir::StringKind::UTCTime:
      case ir::StringKind::GeneralizedTime:
        return nullptr;  // octet-based: handled by the caller
    }
    return nullptr;
  }

std::string Emitter::per_string_encode_call(const char* C, ir::StringKind kind,
                                     const std::string& expr,
                                     const std::string& size_expr) {
    if (const char* k = per_char_string_kind_expr(kind)) {
      return std::string(C) + "::encode_restricted_string(w, " + expr + ", " + k +
             ", " + size_expr + ")";
    }
    return std::string(C) + "::encode_utf8_string(w, " + expr + ", " + size_expr + ")";
  }

std::string Emitter::per_string_decode_call(const char* C, ir::StringKind kind,
                                     const std::string& expr,
                                     const std::string& size_expr) {
    if (const char* k = per_char_string_kind_expr(kind)) {
      return std::string(C) + "::decode_restricted_string(r, " + k + ", " + size_expr + ")";
    }
    return std::string(C) + "::decode_utf8_string(r, " + size_expr + ")";
  }

}  // namespace codegen
}  // namespace asn1
