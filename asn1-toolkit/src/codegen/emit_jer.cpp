/***************************************************************************
** Copyright (C)  2026-2031 PROCODEC All rights reserved.
** -------------------------------------------------------------------------
** This document contains proprietary information belonging to PROCODEC.
** Passing on and copying of this document, use and communication of its
** contents is not permitted without prior written authorisation.
** -------------------------------------------------------------------------
** Revision Information :
**   $Filename: asn1-toolkit/src/codegen/emit_jer.cpp
**   $Version: 0.1
**   $Date:   2026-10-14
**   $Author: tina.cao
***************************************************************************
**  File Description:
**
**   JER family emitter: JSON encode/decode body emission.
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
   *  Function    : jer_name_form_expr
   *  Description : Computes jer name form expr from (f).
   *  Parameters  : f — ir::Field::JerNameForm f
   *  Returns     : static const char*
   */
const char* Emitter::jer_name_form_expr(ir::Field::JerNameForm f) {
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

  /**
   *  Function    : jer_text_form_expr
   *  Description : Computes jer text form expr from (f).
   *  Parameters  : f — ir::JerEncoding::TextForm f
   *  Returns     : static const char*
   */
const char* Emitter::jer_text_form_expr(ir::JerEncoding::TextForm f) {
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

  /**
   *  Function    : emit_jer_codec_fns
   *  Description : Performs emit jer codec fns (definition).
   *  Parameters  : out — std::ostream& out; id — ir::TypeId id
   *  Returns     : void
   */
void Emitter::emit_jer_codec_fns(std::ostream& out, ir::TypeId id) {
    const ir::Type& t = model_.arena.get(id);
    const std::string name = named_type_cpp(t);
    std::ostream* body = begin_codec_fn(
        out, "asn1::Result<asn1::jer::Value> encode_jer(const " + name + "& value)");
    emit_jer_encode_body(*body, t, "value", "  ");
    *body << "}\n\n";

    body = begin_codec_fn(
        out, "asn1::Result<void> decode_jer(const asn1::jer::Value& v, " + name +
                "& value)");
    emit_jer_decode_body(*body, t, "value", "v", "  ");
    *body << "  return asn1::Result<void>::success();\n}\n\n";
  }

void Emitter::emit_jer_encode_call(std::ostream& out, const ir::Type& ft, const std::string& expr,
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

void Emitter::emit_jer_encode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
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
        // A named SEQUENCE OF lowers to an alias struct with a `.value`
        // member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
        out << ind << "std::vector<asn1::jer::Value> jer_elems;\n";
        out << ind << "jer_elems.reserve(" << vec << ".size());\n";
        out << ind << "for (const auto& jer_elem : " << vec << ") {\n";
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
        // A named SET OF lowers to an alias struct with a `.value` member;
        // an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
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
            out << ind << "for (const auto& jer_item : " << vec << ") {\n";
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
        out << ind << "for (const auto& jer_elem : " << vec << ") {\n";
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
              /**
               *  Function    : empty
               *  Description : Computes empty from (!).
               *  Parameters  : ! — f.jer_name_form !
               *  Returns     : if (f.jer_name_form != ir::Field::JerNameForm::AsIs || !f.jer_name_literal.
               */
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
                std::string def_val = default_val_cpp(f);
                if (!def_val.empty()) {
                  out << ind << "if (" << expr << "." << fname << " != " << def_val << ") {\n";
                  out << ind << "  auto jer_m = ";
                  emit_jer_field_encode_expr(out, ft, expr + "." + fname, ind + "  ");
                  out << ind << "  if (!jer_m) return jer_m.error();\n";
                  /**
                   *  Function    : empty
                   *  Description : Computes empty from (!).
                   *  Parameters  : ! — f.jer_name_form !
                   *  Returns     : if (f.jer_name_form != ir::Field::JerNameForm::AsIs || !f.jer_name_literal.
                   */
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
              /**
               *  Function    : empty
               *  Description : Computes empty from (!).
               *  Parameters  : ! — f.jer_name_form !
               *  Returns     : if (f.jer_name_form != ir::Field::JerNameForm::AsIs || !f.jer_name_literal.
               */
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
              /**
               *  Function    : empty
               *  Description : Computes empty from (!).
               *  Parameters  : ! — f.jer_name_form !
               *  Returns     : if (f.jer_name_form != ir::Field::JerNameForm::AsIs || !f.jer_name_literal.
               */
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
        if (rt.object_class_field.open_type) {
          if (rt.jer.base64) {
            out << ind << "return asn1::Result<asn1::jer::Value>::success("
                << "asn1::jeri::encode_octet_string_base64(asn1::Span<const std::uint8_t>("
                << expr << ".data(), " << expr << ".size())));\n";
          } else {
            out << ind << "return asn1::Result<asn1::jer::Value>::success("
                << "asn1::jeri::encode_octet_string(asn1::Span<const std::uint8_t>(" << expr
                << ".data(), " << expr << ".size())));\n";
          }
        } else {
          emit_jer_encode_body(out, resolve(model_, rt.object_class_field.fixed_type), expr, ind);
        }
        break;
    }
  }

void Emitter::emit_jer_field_encode_expr(std::ostream& out, const ir::Type& ft,
                                  const std::string& expr, const std::string& ind) {
    if (is_named(ft)) {
      out << "encode_jer(" << expr << ");\n";
    } else {
      out << "([&]() -> asn1::Result<asn1::jer::Value> {\n";
      emit_jer_encode_body(out, ft, expr, ind);
      out << ind << "})();\n";
    }
  }

void Emitter::emit_jer_decode_body(std::ostream& out, const ir::Type& t, const std::string& expr,
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
                const std::string def_expr = default_val_cpp(f);
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
              const std::string tmp = "jer_field" + std::to_string(temp_counter_++);
              out << ind << "    " << type_cpp(f.type) << " " << tmp << "{};\n";
              emit_jer_field_decode_stmt(out, ft, tmp, "jer_f", ind + "    ");
              out << ind << "    " << expr << "." << fname << " = std::move(" << tmp << ");\n";
            } else {
              emit_jer_field_decode_stmt(out, ft, expr + "." + fname, "jer_f", ind + "    ");
            }
            if (f.presence == ir::Presence::Default && f.default_value) {
              const std::string def_expr = default_val_cpp(f);
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
          // A named CHOICE nests its alternative structs; qualify the name so
          // nested choices (Nested_rec_q::m_) resolve from a free function.
          const std::string choice_ty = is_named(rt) ? named_ref(rt) : std::string{};
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
            if (!choice_ty.empty()) {
              out << ind << "    " << choice_ty << "::" << cpp_ident(f.name) << "_ alt{};\n";
            } else {
              out << ind << "    " << cpp_ident(f.name) << "_ alt{};\n";
            }
            emit_jer_field_decode_stmt(out, ft, "alt.value", "jer_payload", ind + "    ");
            out << ind << "    " << expr << ".alt = std::move(alt);\n";
            out << ind << "    return asn1::Result<void>::success();\n";
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
        // A named SEQUENCE OF lowers to an alias struct with a `.value`
        // member; an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
        out << ind << "{\n";
        out << ind << "  auto jer_arr = asn1::jeri::decode_sequence_of(" << v_expr << ");\n";
        out << ind << "  if (!jer_arr) return jer_arr.error();\n";
        out << ind << "  " << vec << ".clear();\n";
        out << ind << "  " << vec << ".reserve(jer_arr.value()->size());\n";
        out << ind << "  for (const auto& jer_item : *jer_arr.value()) {\n";
        out << ind << "    " << type_cpp(elem_id) << " jer_elem{};\n";
        emit_jer_field_decode_stmt(out, elem, "jer_elem", "jer_item", ind + "    ");
        out << ind << "    " << vec << ".push_back(std::move(jer_elem));\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::SetOf: {
        const ir::TypeId elem_id = rt.set_of.element;
        const ir::Type& elem = resolve(model_, elem_id);
        // A named SET OF lowers to an alias struct with a `.value` member;
        // an inline (anonymous) one is a bare vector.
        const std::string vec = is_named(rt) ? expr + ".value" : expr;
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
            out << ind << "  " << vec << ".clear();\n";
            out << ind << "  " << vec << ".reserve(jer_obj.value().size());\n";
            out << ind << "  for (const auto& jer_ent : jer_obj.value()) {\n";
            out << ind << "    " << type_cpp(elem_id) << " jer_elem{};\n";
            out << ind << "    jer_elem." << kname << " = jer_ent.first;\n";
            emit_jer_field_decode_stmt(out, vt, "jer_elem." + vname, "*jer_ent.second",
                                       ind + "    ");
            out << ind << "    " << vec << ".push_back(std::move(jer_elem));\n";
            out << ind << "  }\n";
            out << ind << "}\n";
            break;
          }
        }
        out << ind << "{\n";
        out << ind << "  auto jer_arr = asn1::jeri::decode_sequence_of(" << v_expr << ");\n";
        out << ind << "  if (!jer_arr) return jer_arr.error();\n";
        out << ind << "  " << vec << ".clear();\n";
        out << ind << "  " << vec << ".reserve(jer_arr.value()->size());\n";
        out << ind << "  for (const auto& jer_item : *jer_arr.value()) {\n";
        out << ind << "    " << type_cpp(elem_id) << " jer_elem{};\n";
        emit_jer_field_decode_stmt(out, elem, "jer_elem", "jer_item", ind + "    ");
        out << ind << "    " << vec << ".push_back(std::move(jer_elem));\n";
        out << ind << "  }\n";
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::Referenced:
        out << ind << "return asn1::make_error(asn1::Error::Code::Unsupported, 0, "
               "\"JER decode not fully generated for this type\");\n";
        break;
      case ir::TypeKind::ObjectClassField: {
        out << ind << "{\n";
        if (rt.object_class_field.open_type) {
          if (rt.jer.base64) {
            out << ind << "  auto tmp = asn1::jeri::decode_octet_string_base64(" << v_expr
                << ");\n";
          } else {
            out << ind << "  auto tmp = asn1::jeri::decode_octet_string(" << v_expr << ");\n";
          }
          out << ind << "  if (!tmp) return tmp.error();\n";
          out << ind << "  " << expr << " = std::move(tmp.value());\n";
        } else {
          emit_jer_decode_body(out, resolve(model_, rt.object_class_field.fixed_type), expr,
                               v_expr, ind + "  ");
        }
        out << ind << "}\n";
        break;
      }
      case ir::TypeKind::InstanceOf: {
        out << ind << "{\n";
        out << ind << "  auto tmp = asn1::jeri::decode_sequence_array(" << v_expr << ");\n";
        out << ind << "  if (!tmp) return tmp.error();\n";
        out << ind << "  const auto& parts = *tmp.value();\n";
        out << ind << "  if (parts.size() < 2u) return asn1::make_error("
               "asn1::Error::Code::InvalidArgument, 0, \"INSTANCE OF expects [type-id, value]\");\n";
        out << ind << "  auto tid = asn1::jeri::decode_object_identifier(parts[0]);\n";
        out << ind << "  if (!tid) return tid.error();\n";
        out << ind << "  " << expr << ".type_id = std::move(tid.value());\n";
        out << ind << "  auto val = asn1::jeri::decode_octet_string(parts[1]);\n";
        out << ind << "  if (!val) return val.error();\n";
        out << ind << "  " << expr << ".value = std::move(val.value());\n";
        out << ind << "}\n";
        break;
      }
    }
  }

void Emitter::emit_jer_field_decode_stmt(std::ostream& out, const ir::Type& ft,
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

}  // namespace codegen
}  // namespace asn1
